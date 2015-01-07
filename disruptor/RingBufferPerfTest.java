/*
 * Copyright 2011 LMAX Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.lmax.disruptor.queue;

import java.io.PrintStream;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.CyclicBarrier;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.ThreadPoolExecutor;

import com.lmax.disruptor.RingBuffer;
import com.lmax.disruptor.WorkerPool;
import com.lmax.disruptor.EventFactory;
import com.lmax.disruptor.EventHandler;
import com.lmax.disruptor.WorkHandler;
import com.lmax.disruptor.Sequence;
import com.lmax.disruptor.Sequencer;
import com.lmax.disruptor.SequenceBarrier;
import com.lmax.disruptor.FatalExceptionHandler;
import com.lmax.disruptor.BusySpinWaitStrategy;
import com.lmax.disruptor.YieldingWaitStrategy;
import com.lmax.disruptor.SleepingWaitStrategy;
import com.lmax.disruptor.util.DaemonThreadFactory;
import com.lmax.disruptor.util.PaddedLong;
import com.lmax.disruptor.dsl.ProducerType;
import com.lmax.disruptor.dsl.Disruptor;

/**
 * <pre>
 *
 * Ping pongs between 2 event handlers and measures the latency of
 * a round trip.
 *
 * Queue Based:
 * ============
 *               +---take---+
 *               |          |
 *               |          V
 *            +====+      +====+
 *    +------>| Q1 |      | P2 |-------+
 *    |       +====+      +====+       |
 *   put                              put
 *    |       +====+      +====+       |
 *    +-------| P1 |      | Q2 |<------+
 *            +====+      +====+
 *               ^          |
 *               |          |
 *               +---take---+
 *
 * P1 - QueuePinger
 * P2 - QueuePonger
 * Q1 - PingQueue
 * Q2 - PongQueue
 *
 * </pre>
 *
 * Note: <b>This test is only useful on a system using an invariant TSC in user space from the System.nanoTime() call.</b>
 */


///
/// See: http://blog.csdn.net/xiaohulunb/article/details/38762845
/// See: http://ziyue1987.github.io/pages/2013/09/22/disruptor-use-manual.html
///
public final class RingBufferPerfTest
{
    private static final int BUFFER_SIZE = 1024;
    private static final int THREAD_NUMS = 2;

    private static final int PUSH_CNT = 2;
    private static final int POP_CNT  = 2;

    private static final int MAX_MSG_COUNT = 8000000 * 1;
    private static final int PUSH_MSG_CNT = MAX_MSG_COUNT / PUSH_CNT;
    private static final int POP_MSG_CNT  = MAX_MSG_COUNT / POP_CNT;

    private static final long PAUSE_NANOS = 1000L;

    private static final ExecutorService executor = Executors.newCachedThreadPool(DaemonThreadFactory.INSTANCE);
    private static Disruptor<MessageEvent> disruptor = null;
    private static RingBuffer<MessageEvent> ringBuffer = null;

    ///////////////////////////////////////////////////////////////////////////////////////////////

    private static final Producer[] producers = new Producer[PUSH_CNT];
    {
        for (int i = 0; i < PUSH_CNT; i++)
        {
            producers[i] = new Producer(i, PUSH_MSG_CNT, PAUSE_NANOS);
        }
    }

    private static final MessageEvent[] events = new MessageEvent[MAX_MSG_COUNT];
    {
        for (int i = 0; i < MAX_MSG_COUNT; i++)
        {
            events[i] = new MessageEvent();
            events[i].setValue(i + 1);
        }
    }

    private static final PaddedLong[] counters = new PaddedLong[POP_CNT];
    {
        for (int i = 0; i < POP_CNT; i++)
        {
            counters[i] = new PaddedLong();
        }
    }

    private static final Sequence[] shared_counters = new Sequence[POP_CNT];
    {
        for (int i = 0; i < POP_CNT; i++)
        {
            shared_counters[i] = new Sequence(0);
        }
    }

    // Construct a EventHandler array that have POP_CNT EventHandlers.
    private static final CountingEventHandler[] event_handlers = new CountingEventHandler[POP_CNT];
    {
        for (int i = 0; i < POP_CNT; i++)
        {
            event_handlers[i] = new CountingEventHandler(i, MAX_MSG_COUNT, counters, shared_counters);
        }
    }

    // Construct a WorkerPool that have POP_CNT WorkProcessors.
    private static final EventCountingWorkHandler[] work_handlers = new EventCountingWorkHandler[POP_CNT];
    {
        for (int i = 0; i < POP_CNT; i++)
        {
            work_handlers[i] = new EventCountingWorkHandler(i, POP_MSG_CNT, counters, shared_counters);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    public void testImplementation() throws Exception
    {
        System.out.println("testImplementation() ... Enter()");

        long start = System.currentTimeMillis();

        runRingQueuePass();

        long usedtime = System.currentTimeMillis() - start;
        long opsPerSecond = (MAX_MSG_COUNT * 1000L) / usedtime;

        System.out.println("");
        System.out.format("Time = %d ms, Disruptor = %d ops/sec.%n%n", usedtime, opsPerSecond);

        long totals = 0;
        for (int i = 0; i < POP_CNT; i++)
        {
            System.out.format("counters[%d] = %d%n", i, counters[i].get());
            totals += counters[i].get();
        }
        System.out.println("");
        System.out.format("totals = %d msgs%n", totals);
        System.out.println("");

        /*
        totals = 0;
        for (int i = 0; i < POP_CNT; i++)
        {
            System.out.format("shared_counters[%d] = %d%n", i, shared_counters[i].get());
            totals += shared_counters[i].get();
        }
        System.out.println("");
        System.out.format("totals = %d%n", totals);
        System.out.println("");
        //*/

        System.out.println("testImplementation() ... Over().");
    }

    private void runRingQueuePass() throws Exception
    {
        final CountDownLatch latch = new CountDownLatch(PUSH_CNT);
        final CyclicBarrier barrier = new CyclicBarrier(PUSH_CNT);
        int i;

        resetCounters();

        for (i = 0; i < PUSH_CNT; i++)
        {
            producers[i].reset(barrier, latch);
        }
        for (i = 0; i < POP_CNT; i++)
        {
            event_handlers[i].reset(latch, MAX_MSG_COUNT);
        }

        final Future<?> producerFutures[] = new Future<?>[PUSH_CNT];
        for (i = 0; i < PUSH_CNT; i++)
        {
            //producerFutures[i] = executor.submit(producers[i]);
        	executor.execute(producers[i]);
        }

        //workerPool.start(executor);

        //System.out.println("runRingQueuePass() --- barrier.await();");

        //barrier.await();
        latch.await();

        //workerPool.drainAndHalt();

        for (i = 0; i < PUSH_CNT; i++)
        {
            //producerFutures[i].cancel(true);
        }
    }

    public static void main(final String[] args) throws Exception
    {
    	System.out.println("");
        System.out.println("This is RingBufferPerfTest ...");
        System.out.println("");
        
        System.out.format("MAX_MSG_COUNT = %d%n", 	MAX_MSG_COUNT);
        System.out.format("BUFFER_SIZE   = %d%n", 	BUFFER_SIZE);
        System.out.format("producers     = %d%n", 	PUSH_CNT);
        System.out.format("consumers     = %d%n%n", POP_CNT);

        final RingBufferPerfTest test = new RingBufferPerfTest();

        ExecutorService executors = Executors.newCachedThreadPool();
        disruptor = new Disruptor<MessageEvent>(MessageEvent.EVENT_FACTORY,
            BUFFER_SIZE, executors,
            //ProducerType.SINGLE,
            ProducerType.MULTI,
            //new BusySpinWaitStrategy()
            new YieldingWaitStrategy()
            //new SleepingWaitStrategy()
            );

        //disruptor.handleEventsWith(event_handlers);
        disruptor.handleEventsWithWorkerPool(work_handlers);

        ringBuffer = disruptor.start();

        test.testImplementation();

        disruptor.shutdown();
        executors.shutdown();

        /*
        for (Producer producer : producers)
        {
            System.out.format("producer.failed = %b%n", producer.failed);
        }
        System.out.println("");
        //*/
    }

    private void resetCounters()
    {
        for (int i = 0; i < POP_CNT; i++)
        {
            counters[i].set(0L);
        }
    }

    private static class Producer implements Runnable
    {
        private CyclicBarrier barrier;
        private CountDownLatch latch;
        private long counter;
        private final int id;
        private final long totalEvents;
        private final long pauseTimeNs;

        public boolean failed = false;

        public Producer(final int id, final long totalEvents, final long pauseTimeNs)
        {
            this.id = id;
            this.totalEvents = totalEvents;
            this.pauseTimeNs = pauseTimeNs;
        }

        public void reset(final CyclicBarrier barrier, final CountDownLatch latch)
        {
            this.barrier = barrier;
            this.latch = latch;

            counter = 0;
        }

        @Override
        public void run()
        {
            try
            {
                barrier.await();

                MessageEvent event = null;
                final long base = id * totalEvents;
                long index = base;

                while (index < (base + totalEvents))
                {
                    event = events[(int)index];

                    //System.out.println("Producer::Run() index = " + index + ",\tevent.getValue() = " + event.getValue());

                    // Get the sequence
                    long sequence = ringBuffer.next();
                    // Get the event value by sequence
                    MessageEvent valueEvent = ringBuffer.get(sequence);
                    // Write the event value
                    valueEvent.setValue(event.getValue());
                    // Publish to comsumers
                    ringBuffer.publish(sequence);
                    
                    index++;
                }
            }
            catch (final Exception ex)
            {
                ex.printStackTrace();
                failed = true;
            }
            finally
            {
                latch.countDown();
            }
        }
    }

    public final class CountingEventHandler implements EventHandler<MessageEvent>
    {
        private final int id;
        private final int messageCount;
        private final PaddedLong[] counters;
        private final PaddedLong value = new PaddedLong();
        private final Sequence[] shared_counters;
        private int readCount = 0;
        private CountDownLatch latch;

        public CountingEventHandler(final int id, final int messageCount,
                                    final PaddedLong[] counters,
                                    final Sequence[] shared_counters)
        {
            this.id = id;
            this.messageCount = messageCount;
            this.counters = counters;
            this.shared_counters = shared_counters;
        }

        public long getValue() {
            return value.get();
        }

        public void reset(final CountDownLatch latch, final int expectedCount)
        {
            value.set(0L);
            this.latch = latch;
            readCount = expectedCount;
        }

        @Override
        public void onEvent(final MessageEvent event, final long sequence, final boolean endOfBatch) throws Exception
        {
            this.counters[id].set(this.counters[id].get() + 1L);
            //this.shared_counters[id].incrementAndGet();

            /*
            Thread.sleep(1);
            if (event != null)
            {
                System.out.println("CountingEventHandler::onEvent() -- id: " + id
                        + ", event: " + event.getValue()
                        + ", sequence: " + sequence
                        + ", counter: " + counters[id].get());
            }
            //*/
        }
    }

    public final class EventCountingWorkHandler implements WorkHandler<MessageEvent>
    {
        private final int id;
        private final int messageCount;
        private final PaddedLong[] counters;
        private final Sequence[] shared_counters;
        public int readCount = 0;

        public EventCountingWorkHandler(final int id, final int messageCount,
                                        final PaddedLong[] counters,
                                        final Sequence[] shared_counters)
        {
            this.id = id;
            this.messageCount = messageCount;
            this.counters = counters;
            this.shared_counters = shared_counters;
        }

        @Override
        public void onEvent(final MessageEvent event) throws Exception
        {
            //System.out.println("EventCountingWorkHandler::onEvent() ... Enter()");

            this.counters[id].set(this.counters[id].get() + 1L);
            //this.shared_counters[id].incrementAndGet();
            /*
            if (readCount < messageCount)
            {
                readCount++;
            }
            else
            {
                this.counters[id].set(messageCount);
            }
            //*/

            /*
            long sequence = 1;
            Thread.sleep(1);
            System.out.println(this + ", id: " + id
                    + ", event: " + event.getValue()
                    + ", sequence: " + sequence
                    + ", counter: " + counters[id].get());
            //*/

            //System.out.println("EventCountingWorkHandler::onEvent() ... Over()");
        }
    }

    public static final class MessageEvent
    {
        private long value;

        public long getValue()
        {
            return value;
        }

        public void setValue(final long value)
        {
            this.value = value;
        }

        public static final EventFactory<MessageEvent> EVENT_FACTORY = new EventFactory<MessageEvent>()
        {
            public MessageEvent newInstance()
            {
                return new MessageEvent();
            }
        };
    }
}
