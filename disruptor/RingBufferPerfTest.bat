
@echo off
@echo on
java -classpath .\build\classes\main;.\build\classes\perf;.\build\classes\test;.\lib\test\HdrHistogram.jar com.lmax.disruptor.queue.RingBufferPerfTest
pause
