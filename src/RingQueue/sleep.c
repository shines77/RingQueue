
#include "sleep.h"

#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <unistd.h>     // For usleep()
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "msvc/targetver.h"
#include <windows.h>    // For Sleep(), SwitchToThread()
#elif defined(__linux__) || defined(__GNUC__)
#include <unistd.h>     // For usleep()
#include <sched.h>      // For sched_yield()
#endif  /* __MINGW32__ */

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "msvc/targetver.h"
#include <windows.h>    // For Sleep(), SwitchToThread()
#endif  /* _MSC_VER */

#if defined(__MINGW32__) || defined(__CYGWIN__)

void jimi_sleep(unsigned int millisec)
{
    usleep(millisec * 1000);
}

void jimi_usleep(unsigned int usec)
{
    usleep(usec);
}

void jimi_wsleep(unsigned int millisec)
{
    Sleep(millisec);
}

int jimi_yield()
{
    return (int)SwitchToThread();
}

#elif defined(__linux__) || defined(__GNUC__)

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
    if (millisec == 0)
        sched_yield();
    else
        usleep(1);
#endif
}

int jimi_yield()
{
    return sched_yield();
}

#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)

void jimi_sleep(unsigned int millisec)
{
    Sleep(millisec);
}

void jimi_usleep(unsigned int usec)
{
    Sleep((usec + 999) / 1000);
}

void jimi_wsleep(unsigned int millisec)
{
    Sleep(millisec);
}

int jimi_yield()
{
    return (int)SwitchToThread();
}

#else  /* other unknown os */

void jimi_sleep(unsigned int millisec)
{
    // Do nothing !!
    volatile unsigned int sum = 0;
    unsigned int i, j;
    for (i = 0; i < millisec; ++i) {
        sum += i;
        for (j = 5; j >= 0; --j) {
            sum -= j;
        }
    }
}

void jimi_usleep(unsigned int usec)
{
    // Not implemented
    volatile unsigned int sum = 0;
    unsigned int i, j;
    for (i = 0; i < millisec; ++i) {
        sum += i;
        for (j = 1; j >= 0; --j) {
            sum -= j;
        }
    }
}

void jimi_wsleep(unsigned int millisec)
{
    jimi_sleep(millisec);
}

int jimi_yield()
{
    // Not implemented
    return 1;
}

#endif  /* __linux__ */
