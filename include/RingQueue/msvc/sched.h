
#undef  SCHW32_API_HAVE_DEFINED
#define SCHW32_API_HAVE_DEFINED     0

//#if !(defined(SCHED_H) && defined(_SCHED_H))

#if defined(_MSC_VER) || defined(__INTEL_COMPILER) || defined(__MINGW32__) || defined(__CYGWIN__)

#ifndef _JIMIC_SCHED_H_
#define _JIMIC_SCHED_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "msvc/targetver.h"
#include <windows.h>
#define SCHW32_INCLUDE_WINDOWS_H

/*
 * The Open Watcom C/C++ compiler uses a non-standard calling convention
 * that passes function args in registers unless __cdecl is explicitly specified
 * in exposed function prototypes.
 *
 * We force all calls to cdecl even though this could slow Watcom code down
 * slightly. If you know that the Watcom compiler will be used to build both
 * the DLL and application, then you can probably define this as a null string.
 * Remember that pthread.h (this file) is used for both the DLL and application builds.
 */
#ifndef SCHW32_CDECL
#define SCHW32_CDECL    __cdecl
#endif  // SCHW32_CDECL

#undef  SCHW32_API

#ifndef SCHW32_API
#define SCHW32_API      __stdcall
#endif  // SCHW32_API

#undef  SCHW32_API_HAVE_DEFINED
#define SCHW32_API_HAVE_DEFINED     1

/*
 * To avoid including windows.h we define only those things that we
 * actually need from it.
 */
#if !defined(SCHW32_INCLUDE_WINDOWS_H)
#if !defined(HANDLE)
# define PTW32__HANDLE_DEF
# define HANDLE     void *
#endif
#if !defined(DWORD)
# define PTW32__DWORD_DEF
# define DWORD      unsigned long
#endif
#endif

#include "vs_stdint.h"

/// <commit>
///
/// See: http://www.cnblogs.com/lovevivi/archive/2012/11/16/2773325.html
///
/// </commit>

#ifndef CPU_ZERO
#define CPU_ZERO(cpuset)            schw32_cpu_zero((cpuset))
#endif  /* CPU_ZERO */

#ifndef CPU_SET
#define CPU_SET(cpuid, cpuset)      schw32_cpu_set((cpuid), (cpuset))
#endif  /* CPU_SET */

#ifndef CPU_CLR
#define CPU_CLR(cpuid, cpuset)      schw32_cpu_clear((cpuid), (cpuset))
#endif  /* CPU_CLR */

#ifndef CPU_ISSET
#define CPU_ISSET(cpuid, cpuset)    schw32_cpu_isset((cpuid), (cpuset))
#endif  /* CPU_ISSET */

#ifdef __cplusplus
extern "C" {
#endif

#if 1
struct cpu_set_s {
    uint32_t mask;
};
#else
struct cpu_set_s {
    uint64_t mask;
};
#endif

typedef struct cpu_set_s    cpu_set_t;

/*
 * Set the CPU masks
 */
void SCHW32_CDECL schw32_cpu_zero(cpu_set_t *cpuset);

void SCHW32_CDECL schw32_cpu_set(int cpuid, cpu_set_t *cpuset);

void SCHW32_CDECL schw32_cpu_clear(int cpuid, cpu_set_t *cpuset);

int  SCHW32_CDECL schw32_cpu_isset(int cpuid, const cpu_set_t *cpuset);

#ifdef __cplusplus
}
#endif

#endif  /* !_JIMIC_SCHED_H_ */

#endif  /* (defined(_MSC_VER) || defined(__INTEL_COMPILER) || defined(__MINGW32__) || defined(__CYGWIN__)) */

//#endif  /* !(SCHED_H && _SCHED_H) */

#if (!defined(SCHW32_API_HAVE_DEFINED)) || (SCHW32_API_HAVE_DEFINED == 0)

#ifndef SCHW32_API
#define SCHW32_API
#endif

#endif  /* PTW32_API_HAVE_DEFINED */
