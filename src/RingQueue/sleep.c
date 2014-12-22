
#include "sleep.h"

#if defined(__MINGW32__)
#include <unistd.h>     // For usleep()
#include <windows.h>    // For Sleep(), SwitchToThread()
#elif defined(__linux__)
#include <unistd.h>     // For usleep()
#include <sched.h>      // For sched_yield()
#endif  /* __MINGW32__ */

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#include <windows.h>    // For Sleep(), SwitchToThread()
#endif  /* _MSC_VER */

#if defined(__MINGW32__)

void jimi_sleep(unsigned int millisec)
{
    usleep(millisec * 1000);
}

void jimi_wsleep(unsigned int millisec)
{
    Sleep(millisec);
}

void jimi_yield()
{
    SwitchToThread();
}

#elif defined(__linux__)

void jimi_sleep(unsigned int millisec)
{
    usleep(millisec * 1000);
}

void jimi_wsleep(unsigned int millisec)
{
#if 0
    if (millisec != 0)
        jimi_sleep(millisec);
    else
        usleep(1);
#else
    sched_yield();
#endif
}

void jimi_yield()
{
    sched_yield();
}

#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)

void jimi_sleep(unsigned int millisec)
{
    Sleep(millisec);
}

void jimi_wsleep(unsigned int millisec)
{
    jimi_sleep(millisec);
}

void jimi_yield()
{
    SwitchToThread();
}

#else  /* other unknown os */

void jimi_sleep(unsigned int millisec)
{
    // Do nothing !!
    volatile unsigned int sum = 0;
    unsigned int i, j;
    for (i = 0; i < millisec; ++i) {
        sum += i;
        for (j = 50000; j >= 0; --j) {
            sum -= j;
        }
    }
}

void jimi_wsleep(unsigned int millisec)
{
    jimi_sleep(millisec);
}

void jimi_yield()
{
    // Not implemented
}

#endif  /* __linux__ */
