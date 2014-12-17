
#include "console.h"
#include "sys_timer.h"
#include "get_char.h"

#include <stdio.h>

/* 预热时间至少要大于500毫秒, 如果还不够, 可以自行增加最小预热时间 */

void jimi_cpu_warmup(int delayTime)
{
#if defined(NDEBUG) || !defined(_DEBUG)
    jmc_timestamp startTime, stopTime;
    int i, j;
    volatile int sum = 0;
    jmc_timefloat elapsedTime = 0.0;
    jmc_timefloat delayTimeLimit = (jmc_timefloat)delayTime;
    printf("CPU warm up start ...\n");
    fflush(stdout);
    startTime = jmc_get_timestamp();
    do {
        // 这个循环的总和sum其实是一个固定值, 但应该没有编译器能够发现或优化掉(发现了也不会优化)
        for (i = 0; i < 10000; ++i) {
            sum += i;
            // 循环顺序故意颠倒过来的
            for (j = 5000; j >= 0; --j) {
                sum -= j;
            }
        }
        stopTime = jmc_get_timestamp();
        elapsedTime += jmc_get_interval_millisecf(stopTime - startTime);
    } while (elapsedTime < delayTimeLimit);

    // 输出sum的值只是为了防止编译器把循环优化掉
    printf("sum = %u, time: %0.3f ms\n", sum, elapsedTime);
    printf("CPU warm up done  ... \n\n");
    fflush(stdout);
#endif  /* !_DEBUG */
}

int jimi_console_readkey(bool enabledCpuWarmup, bool displayTips,
                         bool echoInput)
{
    int keyCode;
    if (displayTips) {
#if 1
        printf("Press any key to continue ...");
#else
        printf("请按任意键继续 ...");
#endif
        keyCode = jimi_getch();
        printf("\n");
    }
    else {
        keyCode = jimi_getch();
        if (echoInput) {
            if (keyCode != EOF)
                printf("%c", (char)keyCode);
            else
                printf("EOF: (%d)", keyCode);
        }
    }

    // 暂停后, 预热/唤醒一下CPU, 至少500毫秒
    if (enabledCpuWarmup) {
        jimi_cpu_warmup(500);
        //printf("\n");
    }
    return keyCode;
}

int jimi_console_readkey_newline(bool enabledCpuWarmup, bool displayTips,
                                 bool echoInput)
{
    int keyCode;
    keyCode = jimi_console_readkey(false, displayTips, echoInput);
    printf("\n");

    // 暂停后, 预热/唤醒一下CPU, 至少500毫秒
    if (enabledCpuWarmup) {
        jimi_cpu_warmup(500);
        //printf("\n");
    }
    return keyCode;
}
