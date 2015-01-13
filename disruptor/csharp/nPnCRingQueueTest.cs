#define RELEASE

using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;
using System.Runtime.InteropServices;
using System.Runtime.Serialization.Formatters.Binary;
using NUnit.Framework;

using Disruptor.Dsl;
using Disruptor.PerfTests;
using Disruptor.PerfTests.Support;

namespace Disruptor.RingQueueTest
{
    class nPnCRingQueueTest
    {
        private const int BUFFER_SIZE = 1024;

        private const int PUSH_CNT = 2;
        private const int POP_CNT  = 2;

#if DEBUG
        private const int MAX_MSG_COUNT = 8000 * 1;
#else
        private const int MAX_MSG_COUNT = 8000000 * 1;
#endif
        private const int PUSH_MSG_CNT  = MAX_MSG_COUNT / PUSH_CNT;
        private const int POP_MSG_CNT   = MAX_MSG_COUNT / POP_CNT;

        private const int MAX_RUN_PASS  = 3;

        private const int Million = 1000 * 1000;

        private long[] _results = null;

        private Dsl.Disruptor<MessageEvent>     _disruptor = null;
        private RingBuffer<MessageEvent>        _ringBuffer = null;
        private CountingWorkHandler[]           _workHandlers;
        private CountingWorkProcessor[]         _workProcessors;
        private ValueMutationEventHandler[]     _eventHandlers;
        private WorkerPool<MessageEvent>        _workerPool = null;
        private Thread[]                        _producerThreads;
        private CountdownEvent                  _latchs = null;

        public static MessageEvent[] _events = new MessageEvent[MAX_MSG_COUNT];
        private static Volatile.PaddedLong[] _counters = new Volatile.PaddedLong[POP_CNT];

        public static Volatile.PaddedLong _remainCount = new Volatile.PaddedLong(MAX_MSG_COUNT);

        public nPnCRingQueueTest()
        {
            InitEvents();
            InitRingQueue();
        }
        private int GetObjectSize(object TestObject)
        {
            BinaryFormatter bf = new BinaryFormatter();
            MemoryStream ms = new MemoryStream();
            byte[] Array;
            bf.Serialize(ms, TestObject);
            Array = ms.ToArray();
            return Array.Length;
        }

        public void RunTest()
        {
            MessageEvent _event = new MessageEvent();
            Console.WriteLine("MAX_MSG_COUNT         = {0}", MAX_MSG_COUNT);
            Console.WriteLine("BUFFER_SIZE           = {0}", BUFFER_SIZE);
            Console.WriteLine("producers             = {0}", PUSH_CNT);
            Console.WriteLine("consumers             = {0}", POP_CNT);
            //Console.WriteLine("MessageEvent  = {0}", GetObjectSize(_event));
            Console.WriteLine("MessageEvent.SizeOf() = {0}", Marshal.SizeOf(typeof(MessageEvent)));
            Console.WriteLine("");

            for (int n = 1; n <= MAX_RUN_PASS; ++n)
            {
                RunPass(n);
#if DEBUG
                Thread.Sleep(500);
                Console.ReadKey(false);
#endif
            }
        }

        private void InitEvents()
        {
            for (int i = 0; i < MAX_MSG_COUNT; i++)
            {
                _events[i] = new MessageEvent();
                _events[i].Value = i + 1;
            }
        }

        private void ResetEvents()
        {
            for (int i = 0; i < MAX_MSG_COUNT; i++)
            {
                _events[i].Value = i + 1;
            }
        }

        private void ResetCounters()
        {
            _remainCount.AtomicExchange(MAX_MSG_COUNT);

            for (int i = 0; i < POP_CNT; i++)
            {
                _counters[i] = new Volatile.PaddedLong(0);
                _eventHandlers[i].Reset();
                _workHandlers[i].Reset();
            }
        }

        public void InitRingQueue()
        {
            if (_disruptor == null)
            {
                if (PUSH_CNT <= 1)
                {
                    _disruptor = new Dsl.Disruptor<MessageEvent>(
                        () => new MessageEvent(),
                        new SingleThreadedClaimStrategy(BUFFER_SIZE),
                        //new MultiThreadedClaimStrategy(BUFFER_SIZE),
                        //new BusySpinWaitStrategy(),
                        new YieldingWaitStrategy(),
                        //new SleepingWaitStrategy(),
                        TaskScheduler.Default);
                }
                else
                {
                    _disruptor = new Dsl.Disruptor<MessageEvent>(
                        () => new MessageEvent(),
                        //new SingleThreadedClaimStrategy(BUFFER_SIZE),
                        new MultiThreadedClaimStrategy(BUFFER_SIZE),
                        //new BusySpinWaitStrategy(),
                        new YieldingWaitStrategy(),
                        //new BetterYieldingWaitStrategy(),
                        //new SleepingWaitStrategy(),
                        TaskScheduler.Default);
                }

                _latchs = new CountdownEvent(POP_CNT);

                _eventHandlers = new ValueMutationEventHandler[POP_CNT];

                for (int i = 0; i < POP_CNT; i++)
                {
                    if (i == 0)
                        _eventHandlers[0] = new ValueMutationEventHandler(0, Operation.Addition, MAX_MSG_COUNT, _latchs);
                    else if (i == 1)
                        _eventHandlers[1] = new ValueMutationEventHandler(1, Operation.Substraction, MAX_MSG_COUNT, _latchs);
                    else if (i == 2)
                        _eventHandlers[2] = new ValueMutationEventHandler(2, Operation.And, MAX_MSG_COUNT, _latchs);
                    else
                        _eventHandlers[i] = new ValueMutationEventHandler(i, Operation.And, MAX_MSG_COUNT, _latchs);
                }

                _workHandlers = new CountingWorkHandler[POP_CNT];
                for (int i = 0; i < POP_CNT; i++)
                {
                    _workHandlers[i] = new CountingWorkHandler(i, POP_MSG_CNT, _counters, _latchs);
                }

                _workProcessors = new CountingWorkProcessor[POP_CNT];
                for (int i = 0; i < POP_CNT; i++)
                {
                    _workProcessors[i] = new CountingWorkProcessor(i, POP_MSG_CNT, _counters, _latchs);
                }

                //_disruptor.HandleEventsWith(_eventHandlers);
                //_disruptor.HandleEventsWith(_workProcessors);

                _ringBuffer = _disruptor.RingBuffer;

                ///*
                _workerPool = new WorkerPool<MessageEvent>(_ringBuffer,
                                        _ringBuffer.NewBarrier(),
                                        new FatalExceptionHandler(),
                                        _workHandlers);
                //*/

                ///*
                _ringBuffer.SetGatingSequences(_workerPool.WorkerSequences);

                for (int i = 0; i < POP_CNT; i++)
                {
                    _workHandlers[i].SetRingBuffer(_ringBuffer);
                    _workProcessors[i].SetRingBuffer(_ringBuffer);
                }
                //*/

                ResetEvents();
                ResetCounters();

                _producerThreads = new Thread[PUSH_CNT];
                for (int i = 0; i < PUSH_CNT; i++)
                {
                    Producer producer = new Producer(i, PUSH_MSG_CNT, 0, _ringBuffer);
                    producer.Reset();
                    _producerThreads[i] = new Thread(new ThreadStart(producer.Run));
                }

                Thread.MemoryBarrier();

                for (int i = 0; i < PUSH_CNT; i++)
                {
                    _producerThreads[i].Start();
                }

                _remainCount.AtomicExchange(MAX_MSG_COUNT);

                Thread.MemoryBarrier();
                _workerPool.Start(TaskScheduler.Default);

                //_disruptor.Start();
            }
        }

        public long RunPass(int PassNumber)
        {
            InitRingQueue();

            var sw = Stopwatch.StartNew();

            Thread.MemoryBarrier();

            /*
            for (int i = 0; i < MAX_MSG_COUNT; i++)
            {
                var sequence = _ringBuffer.Next();
                _ringBuffer[sequence].Value = i;
                _ringBuffer.Publish(sequence);
                //Console.WriteLine("RunPass(): sequence = {0}, {1}, {2}.", MAX_MSG_COUNT, i, sequence);
            }
            //*/

            for (int i = 0; i < PUSH_CNT; i++)
            {
                _producerThreads[i].Join();
            }

            //Thread.MemoryBarrier();
            //Thread.Sleep(100);

            Thread.MemoryBarrier();

            /*
            for (int i = 0; i < POP_CNT; i++)
            {
                for (int j = 0; j < POP_CNT; j++)
                {
                    try
                    {
                        _latchs[i].Signal();
                    }
                    catch (Exception ex) { }
                }
            }
            //*/

            Thread.MemoryBarrier();

            for (int i = 0; i < POP_CNT; i++)
            {
                //_latchs.Wait();
            }

            Thread.MemoryBarrier();

            _workerPool.DrainAndHalt();

            Thread.MemoryBarrier();

            for (int i = 0; i < POP_CNT; i++)
            {
                var lifecycleAware = _workHandlers[i] as ILifecycleAware;
                if (lifecycleAware != null)
                    lifecycleAware.OnShutdown();
            }

            /*
            while (!_remainCount.AtomicCompareExchange(-1, 0))
            {
                // Do nothing!
                Thread.Yield();
            }
            //*/

            Thread.MemoryBarrier();

            //Thread.Sleep(200);

            long opsPerSecond;
            if (sw.ElapsedMilliseconds != 0)
                opsPerSecond = (MAX_MSG_COUNT * 1000L) / sw.ElapsedMilliseconds;
            else
                opsPerSecond = 0L;

            Console.WriteLine("Run = {0}, Time = {1} ms, Disruptor = {2:###,###,###,###} ops/sec.\n",
                PassNumber, sw.ElapsedMilliseconds, (long)opsPerSecond);

            /*
            try
            {
                for (int i = 0; i < POP_CNT; i++)
                {
                    if (i == 0)
                        Assert.AreEqual(ExpectedResults[0], _eventHandlers[0].Value, "Addition");
                    else if (i == 1)
                        Assert.AreEqual(ExpectedResults[1], _eventHandlers[1].Value, "Sub");
                    else if (i == 2)
                        Assert.AreEqual(ExpectedResults[2], _eventHandlers[2].Value, "And");
                    else
                        Assert.AreEqual(ExpectedResults[i], _eventHandlers[i].Value, "And");
                }
            }
            catch (Exception ex)
            {
                //Console.WriteLine(ex.Message.ToString());
                Console.WriteLine(ex.ToString());
                //Console.ReadKey();
            }
            finally
            {
                //
            }
            //*/

            long totals = 0;
            for (int i = 0; i < POP_CNT; i++)
            {
                Console.WriteLine("counters({0}) = {1}", i, _counters[i].ReadUnfenced());
                totals += _counters[i].ReadUnfenced();
            }
            Console.WriteLine("\ntotals = {0} messages.\n", totals);

            _workerPool.Halt();
            for (int i = 0; i < POP_CNT; i++)
            {
                _workHandlers[i] = null;
            }
            _workerPool = null;

            if (_disruptor != null)
            {
                _disruptor.Shutdown();
                _disruptor = null;
            }

            _ringBuffer = null;

            System.GC.Collect();

            return opsPerSecond;
        }

        protected long[] ExpectedResults
        {
            get
            {
                if (_results == null)
                {
                    _results = new long[POP_CNT];
                    for (int i = 0; i < POP_CNT; i++)
                    {
                        _results[i] = 0;
                    }
                    for (int i = 0; i < MAX_MSG_COUNT; i++)
                    {
                        for (int j = 0; j < POP_CNT; j++)
                        {
                            if (j == 0)
                                _results[0] = Operation.Addition.Op(_results[0], i);
                            else if (j == 1)
                                _results[1] = Operation.Substraction.Op(_results[1], i);
                            else if (j == 2)
                                _results[2] = Operation.And.Op(_results[2], i);
                            else
                                _results[j] = Operation.And.Op(_results[j], i);
                        }

                    }
                }
                return _results;
            }
        }
    }

    ///
    /// See: http://stackoverflow.com/questions/2331889/how-to-find-the-size-of-a-class-in-c-sharp
    /// See: http://stackoverflow.com/questions/605621/how-to-get-object-size-in-memory
    ///
    //sealed
    [StructLayout(LayoutKind.Sequential)]
    sealed class MessageEvent
    //struct MessageEvent
    {
        public long Value { get; set; }
        /*
        private Volatile.PaddedLong valueLong;
        public long Value {
            get { return valueLong.ReadUnfenced(); }
            set { valueLong.WriteUnfenced(value);  }
        }
        //*/
    }

    class Producer
    {
        private readonly int _id;
        private readonly long _totalEvents;
        private readonly long _pauseTimeNs;
        private long _counter = 0;
        private RingBuffer<MessageEvent> _ringBuffer = null;

        public bool _failed = false;

        public Producer(int id, long totalEvents, long pauseTimeNs,
                        RingBuffer<MessageEvent> ringBuffer)
        {
            this._id = id;
            this._totalEvents = totalEvents;
            this._pauseTimeNs = pauseTimeNs;
            this._ringBuffer = ringBuffer;
        }

        public void Reset()
        {
            this._counter = 0;
        }

        public void Run()
        {
            try
            {
                MessageEvent _event = null;
                long base_index = _id * _totalEvents;
                long max_index = base_index + _totalEvents;
                long index = base_index;

                Thread.MemoryBarrier();

                while (index < max_index)
                {
                    Thread.MemoryBarrier();
                    _event = nPnCRingQueueTest._events[(int)index];

                    //Console.WriteLine("Producer::Run() index = " + index + ",\tevent.Value() = " + _event.Value());

                    Thread.MemoryBarrier();

                    // Get the sequence
                    var sequence = this._ringBuffer.Next();
                    // Get the event value by sequence
                    MessageEvent valueEvent = this._ringBuffer[sequence];
                    // Write the event value
                    valueEvent.Value = _event.Value;
                    // Publish to comsumers
                    this._ringBuffer.Publish(sequence);

                    //Console.WriteLine("Producer()::Run() -- sequence = {0}, {1}.", index, sequence);
                    Thread.MemoryBarrier();

                    this._counter++;
                    index++;
                }
                //Console.WriteLine("Producer::Run() -- Thread: {0} done. counter = {1}", _id, this._counter);
            }
            catch (Exception ex)
            {
                //Console.WriteLine(ex.StackTrace.ToString());
                _failed = true;
            }
        }
    }

    class ValueMutationEventHandler : IEventHandler<MessageEvent>
    {
        private readonly int _id;
        private readonly Operation      _operation;
        private Volatile.PaddedLong     _value = new Volatile.PaddedLong(0);
        private readonly long           _max_sequence;
        private readonly CountdownEvent _latch;
        private RingBuffer<MessageEvent> _ringBuffer = null;

        public ValueMutationEventHandler(int id, Operation operation, long max_sequence, CountdownEvent latch)
        {
            _id = id;
            _operation = operation;
            _max_sequence = max_sequence;
            _latch = latch;
        }

        public void SetRingBuffer(RingBuffer<MessageEvent> ringBuffer)
        {
            _ringBuffer = ringBuffer;
        }

        public long Value
        {
            get { return _value.ReadUnfenced(); }
        }

        public void Reset()
        {
            _value.WriteUnfenced(0);
        }

        public void OnNext(MessageEvent _event, long sequence, bool endOfBatch)
        {
            _value.WriteUnfenced(_operation.Op(_value.ReadUnfenced(), _event.Value));

            //Console.WriteLine("ValueMutationEventHandler::Run() -- Event.Value: {0}", _event.Value);

            if (sequence >= _max_sequence - 1)
            {
                Console.WriteLine("ValueMutationEventHandler::Run() -- Thread: {0} done.", _id);
                this._latch.Signal();
            }
        }
    }

    class CountingWorkHandler : IWorkHandler<MessageEvent>
    {
        private readonly int _id;
        private readonly long _maxMessageCount;
        private bool _done = false;
        private int _counter = 0;
        private Volatile.PaddedLong _value = new Volatile.PaddedLong(0);
        private Volatile.PaddedLong[] _counters = null;
        private readonly CountdownEvent _latch = null;
        private RingBuffer<MessageEvent> _ringBuffer = null;

        public CountingWorkHandler(int id, long maxMessageCount,
                                   Volatile.PaddedLong[] counters,
                                   CountdownEvent latch)
        {
            _id = id;
            _maxMessageCount = maxMessageCount;
            _done = false;
            _counter = 0;
            _counters = counters;
            _latch = latch;
        }

        public void Reset()
        {
            _value.WriteUnfenced(0);
            _counter = 0;
        }

        public void SetRingBuffer(RingBuffer<MessageEvent> ringBuffer)
        {
            _ringBuffer = ringBuffer;
        }

        public void OnEvent(MessageEvent _event)
        {
            this._counters[_id].WriteUnfenced(this._counters[_id].ReadUnfenced() + 1L);
#if DEBUG
            Console.WriteLine("CountingWorkHandler::OnEvent() -- id = {0}, counter = {1}, event.Value = {2}",
                _id, _counter, _event.Value);
#endif
            Thread.MemoryBarrier();

            ++_counter;
            if (_counter >= _maxMessageCount)
            {
                if (!_done)
                {
#if DEBUG
                    Console.WriteLine("CountingWorkHandler::OnEvent() -- Thread: {0} done. counter = {1}", _id, _counter);
#endif
                }

                Thread.MemoryBarrier();
                try
                {
                    //if (!_done)
                    //    this._latch.Signal();
                    Thread.MemoryBarrier();

                    //Thread.Sleep(100);

                    Thread.MemoryBarrier();
                    _done = true;
                }
                catch (Exception ex)
                {
                    //Console.WriteLine(ex.StackTrace.ToString());
                }
            }

            /*
            if (nPnCRingQueueTest._remainCount.AtomicDecrementAndGet() <= 0)
            {
                try
                {
                    Console.WriteLine("CountingWorkHandler::OnEvent() -- Thread: {0} done. counter = {1}, remainCount = {2}",
                        _id, _counter, nPnCRingQueueTest._remainCount.ReadUnfenced());
                    Thread.MemoryBarrier();
                    _done = true;
                    this._latch.Signal();
                }
                catch (Exception ex)
                { }
            }
            //*/
        }
    }

    class CountingWorkProcessor : IEventProcessor
    {
        private readonly int _id;
        private readonly long _maxMessageCount;
        private volatile bool _done;
        private volatile bool _running;
        private Volatile.PaddedLong[] _counters = null;
        private readonly CountdownEvent _latch = null;
        private RingBuffer<MessageEvent> _ringBuffer = null;

        public CountingWorkProcessor(int id, long maxMessageCount,
                                     Volatile.PaddedLong[] counters,
                                     CountdownEvent latch)
        {
            _id = id;
            _maxMessageCount = maxMessageCount;
            _counters = counters;
            _latch = latch;

            _done = false;
            _running = false;
        }

        public Sequence Sequence { get; set; }

        public void SetRingBuffer(RingBuffer<MessageEvent> ringBuffer)
        {
            _ringBuffer = ringBuffer;
        }

        public bool Done
        {
            get { return _done; }
        }

        public bool Running
        {
            get { return _running; }
        }

        public void Reset()
        {
            _done = false;
        }

        public void Halt()
        {
            _running = false;
            this._latch.Signal();
        }

        public void Run()
        {
            _running = true;
            for (var i = 0; i < _maxMessageCount; i++)
            {
                Console.WriteLine("CountingWorkProcessor::Run() -- {0}", i);
                try
                {
                    var sequence = this._ringBuffer.Next();
                    this._ringBuffer[sequence].Value = i;
                    this._ringBuffer.Publish(sequence);
                }
                catch (Exception)
                {
                    break;
                }
            }
            _done = true;
            Console.WriteLine("CountingWorkProcessor::Run() -- Thread: {0} done.", _id);
            this._latch.Signal();
        }
    }
}
