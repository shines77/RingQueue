using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

using Disruptor.RingQueueTest;

namespace Disruptor.RingQueueTest
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("");
            Console.WriteLine("This is RingBufferPerfTest ...");
            Console.WriteLine("");

            nPnCRingQueueTest ringQueueTest = new nPnCRingQueueTest();
            ringQueueTest.RunTest();

            Thread.MemoryBarrier();

            //Thread.Sleep(500);
            Console.Write("Press any key to continue ...");
            Console.ReadKey(false);
        }
    }
}
