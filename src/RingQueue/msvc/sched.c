
#if defined(_MSC_VER) || defined(__INTEL_COMPILER) || defined(__MINGW32__) || defined(__CYGWIN__)

#include "msvc/sched.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "msvc/targetver.h"
#include <windows.h>
#include <process.h>    /* _beginthreadex(), _endthreadex() */

#include <string.h>
#include <assert.h>

void SCHW32_CDECL schw32_cpu_zero(cpu_set_t *cpuset)
{
    assert(cpuset != NULL);

    memset((void *)cpuset, 0, sizeof(*cpuset));
}

void SCHW32_CDECL schw32_cpu_set(int cpuid, cpu_set_t *cpuset)
{
    assert(cpuset != NULL);
    assert(cpuid >= 0 && cpuid <= 255);

    cpuset->mask |= 1U << (unsigned int)cpuid;
}

void SCHW32_CDECL schw32_cpu_clear(int cpuid, cpu_set_t *cpuset)
{
    assert(cpuset != NULL);
    assert(cpuid >= 0 && cpuid <= 255);

    cpuset->mask &= ~(1U << (unsigned int)cpuid);
}

int SCHW32_CDECL schw32_cpu_isset(int cpuid, const cpu_set_t *cpuset)
{
    assert(cpuset != NULL);
    assert(cpuid >= 0 && cpuid <= 255);

    return ((cpuset->mask & (1U << (unsigned int)cpuid)) != 0);
}

#endif  // defined(_MSC_VER) || defined(__INTERL_COMPILER) || defined(__MINGW32__) || defined(__CYGWIN__)
