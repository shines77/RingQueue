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

import static com.lmax.disruptor.RingBuffer.createSingleProducer;
import static com.lmax.disruptor.RingBuffer.createMultiProducer;

import com.lmax.disruptor.RingBuffer;
import com.lmax.disruptor.WorkerPool;
import com.lmax.disruptor.EventFactory;
import com.lmax.disruptor.EventHandler;
import com.lmax.disruptor.WorkHandler;
import com.lmax.disruptor.Sequencer;
import com.lmax.disruptor.SequenceBarrier;
import com.lmax.disruptor.FatalExceptionHandler;
import com.lmax.disruptor.BusySpinWaitStrategy;
import com.lmax.disruptor.YieldingWaitStrategy;
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
///
public final class RingBufferPerfTest
{
    private static final int BUFFER_SIZE = 1024;
    private static final int THREAD_NUMS = 2;

    private static final int PUSH_CNT = THREAD_NUMS;
    private static final int POP_CNT  = THREAD_NUMS;

    private static final int MSG_TOTAL_LENGTH = 8000000;
    private static final int PUSH_MSG_CNT = MSG_TOTAL_LENGTH / PUSH_CNT;
    private static final int POP_MSG_CNT  = MSG_TOTAL_LENGTH / POP_CNT;

    private static final long PAUSE_NANOS = 1000L;

    private static final ExecutorService executor = Executors.newCachedThreadPool(DaemonThreadFactory.INSTANCE);
    private static Disruptor<ValueEvent> disruptor = null;
    private static RingBuffer<ValueEvent> ringBuffer = null;

    ///////////////////////////////////////////////////////////////////////////////////////////////

    private static final Producer[] producers = new Producer[PUSH_CNT];
    {
        for (int i = 0; i < PUSH_CNT; i++)
        {
            producers[i] = new Producer(i, PUSH_MSG_CNT, PAUSE_NANOS);
        }
    }

    private static final ValueEvent[] events = new ValueEvent[MSG_TOTAL_LENGTH];
    {
        for (int i = 0; i < MSG_TOTAL_LENGTH; i++)
        {
            events[i] = new ValueEvent();
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

    private static final CountingEventHandler[] event_handlers = new CountingEventHandler[POP_CNT];
    {
        for (int i = 0; i < POP_CNT; i++)
        {
            event_handlers[i] = new CountingEventHandler(i, counters);
        }
    }

    // 构造拥有 POP_CNT 个WorkProcessor的WorkerPool
    private static final EventCountingWorkHandler[] handlers = new EventCountingWorkHandler[POP_CNT];
    {
        for (int i = 0; i < POP_CNT; i++)
        {
            handlers[i] = new EventCountingWorkHandler(i, counters);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    public void testImplementation() throws Exception
    {
        System.out.println("testImplementation() ... Enter()");

        long start = System.currentTimeMillis();

        runRingQueuePass();

        long usedtime = System.currentTimeMillis() - start;
        long opsPerSecond = (MSG_TOTAL_LENGTH * 1000L) / (System.currentTimeMillis() - start);

        System.out.format("Result: Run %d, Time = %d ms, Disruptor = %d ops/sec.%n", THREAD_NUMS, usedtime, opsPerSecond);
        System.out.println("testImplementation() ... Over().");
    }

    private void runRingQueuePass() throws Exception
    {
        final CountDownLatch latch = new CountDownLatch(1);
        final CyclicBarrier barrier = new CyclicBarrier(3);
        int i;

        resetCounters();

        for (i = 0; i < PUSH_CNT; i++)
        {
            producers[i].reset(barrier, latch);
        }
        for (i = 0; i < POP_CNT; i++)
        {
            event_handlers[i].reset(latch, MSG_TOTAL_LENGTH);
        }

        final Future<?> productorFutures[] = new Future<?>[PUSH_CNT];
        for (i = 0; i < PUSH_CNT; i++)
        {
            productorFutures[i] = executor.submit(producers[i]);
        }

        //workerPool.start(executor);

        //System.out.println("runRingQueuePass() --- barrier.await();");

        //barrier.await();
        latch.await();

        //workerPool.drainAndHalt();

        for (i = 0; i < PUSH_CNT; i++)
        {
            productorFutures[i].cancel(true);
        }
    }

    public static void main(final String[] args) throws Exception
    {
        System.out.println("This is RingBufferPerfTest ...");

        final RingBufferPerfTest test = new RingBufferPerfTest();

        ExecutorService executors = Executors.newCachedThreadPool();
        disruptor = new Disruptor<ValueEvent>(ValueEvent.EVENT_FACTORY,
            BUFFER_SIZE, executors, ProducerType.MULTI,
            //new BusySpinWaitStrategy()
            new YieldingWaitStrategy()
            );

        //disruptor.handleEventsWith(event_handlers);
        disruptor.handleEventsWithWorkerPool(handlers);

        ringBuffer = disruptor.start();

        test.testImplementation();

        disruptor.shutdown();
        executors.shutdown();
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

        public Producer(final int id, final long totalEvents, final long pauseTimeNs)
        {
            this.id = id;
            this.totalEvents = totalEvents;
            this.pauseTimeNs = pauseTimeNs;
        }

        @Override
        public void run()
        {
            try
            {
                //barrier.await();

                ValueEvent valueEvent;
                ValueEvent event;
                final long base = id * totalEvents;
                long index = base;

                while (index < (base + totalEvents))
                {
                    event = events[(int)index];

                    //System.out.println("Producer::Run() index = " + index + ",\tevent.getValue() = " + event.getValue());

                    // 获得 sequence
                    long sequence = ringBuffer.next();
                    // 获取可用位置
                    valueEvent = ringBuffer.get(sequence);
                    // 填充可用位置
                    valueEvent.setValue(event.getValue());
                    // 通知消费者
                    ringBuffer.publish(sequence);

                    index++;
                }

                latch.countDown();
            }
            catch (final Exception ex)
            {
                ex.printStackTrace();
                return;
            }
        }

        public void reset(final CyclicBarrier barrier, final CountDownLatch latch)
        {
            this.barrier = barrier;
            this.latch = latch;

            counter = 0;
        }
    }

    public static final class EventCountingWorkHandler implements WorkHandler<ValueEvent>
    {
        private int id;
        private final PaddedLong[] counters;

        public EventCountingWorkHandler(int id, final PaddedLong[] counters)
        {
            this.id = id;
            this.counters = counters;
        }

        @Override
        public void onEvent(final ValueEvent event) throws Exception
        {
            //System.out.println("EventCountingWorkHandler::onEvent() ... Enter()");

            counters[id].set(counters[id].get() + 1L);

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

    public static final class CountingEventHandler implements EventHandler<ValueEvent>
    {
        private int id;
        private final PaddedLong[] counters2;
        private final PaddedLong value = new PaddedLong();
        private long count;
        private CountDownLatch latch;

        public CountingEventHandler(int id, final PaddedLong[] counters)
        {
            this.id = id;
            this.counters2 = counters;
        }

        public long getValue() {
            return value.get();
        }

        public void reset(final CountDownLatch latch, final long expectedCount)
        {
            value.set(0L);
            this.latch = latch;
            count = expectedCount;
        }

        @Override
        public void onEvent(final ValueEvent event, final long sequence, final boolean endOfBatch) throws Exception
        {
            counters[id].set(counters[id].get() + 1L);

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

            if (count == sequence) {
                //latch.countDown();
            }
        }
    }

    public static final class ValueEvent
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

        public static final EventFactory<ValueEvent> EVENT_FACTORY = new EventFactory<ValueEvent>()
        {
            public ValueEvent newInstance()
            {
                return new ValueEvent();
            }
        };
    }
}
