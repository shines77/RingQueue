
#undef  PTW32_API_HAVE_DEFINED
#define PTW32_API_HAVE_DEFINED      0

#if !(defined(PTHREAD_H) && defined(_PTHREAD_H))

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)

#ifndef _JIMIC_PTHREAD_H_
#define _JIMIC_PTHREAD_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include "msvc/sched.h"
#include "vs_stdint.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "msvc/targetver.h"
#include <windows.h>

#define PTW32_INCLUDE_WINDOWS_H

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
#ifndef PTW32_CDECL
#define PTW32_CDECL     __cdecl
#endif  // PTW32_CDECL

#undef  PTW32_API

#ifndef PTW32_API
#define PTW32_API       __stdcall
#endif  // PTW32_API

#undef  PTW32_API_HAVE_DEFINED
#define PTW32_API_HAVE_DEFINED      1

/*
 * To avoid including windows.h we define only those things that we
 * actually need from it.
 */
#if !defined(PTW32_INCLUDE_WINDOWS_H)
#if !defined(HANDLE)
# define PTW32__HANDLE_DEF
# define HANDLE     void *
#endif
#if !defined(DWORD)
# define PTW32__DWORD_DEF
# define DWORD      unsigned long
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(HAVE_STRUCT_TIMESPEC)
#define HAVE_STRUCT_TIMESPEC
#if !defined(_TIMESPEC_DEFINED)
#include <time.h>
#define _TIMESPEC_DEFINED
#if defined(_CRT_NO_TIME_T)
struct timespec {
    time_t tv_sec;
    long tv_nsec;
};
typedef struct timespec timespec_t;
#endif  /* !_CRT_NO_TIME_T for VC2015's time.h */
#endif  /* !_TIMESPEC_DEFINED */
#endif  /* !HAVE_STRUCT_TIMESPEC */

typedef HANDLE   handle_t;

typedef handle_t pthread_t;
typedef handle_t pthread_attr_t;

typedef CRITICAL_SECTION pthread_mutex_t;
typedef handle_t pthread_mutexattr_t;

typedef handle_t pthread_spinlock_t;
typedef handle_t pthread_spinlock_attr_t;

typedef void *(PTW32_API *lpthread_proc_t)(void *);
typedef unsigned int (PTW32_API *pthread_proc_t)(void *);

int PTW32_CDECL pthread_attr_init(pthread_attr_t * attr);
int PTW32_CDECL pthread_attr_destroy(pthread_attr_t * attr);

HANDLE PTW32_CDECL pthread_process_self(void);
pthread_t PTW32_CDECL pthread_self(void);

/*
 * PThread Functions
 */
int PTW32_CDECL pthread_create(pthread_t * tid,
                               const pthread_attr_t * attr,
                               void *(PTW32_API *start)(void *),
                               void * arg);

int PTW32_CDECL pthread_detach(pthread_t tid);

int PTW32_CDECL pthread_join(pthread_t thread, void **value_ptr);

/*
 * About the thread affinity
 */
int PTW32_CDECL pthread_getaffinity_np(pthread_t thread, size_t cpuset_size,
                                       cpu_set_t * cpuset);

int PTW32_CDECL pthread_setaffinity_np(pthread_t thread, size_t cpuset_size,
                                       const cpu_set_t * cpuset);

/*
 * Mutex Functions
 */
int PTW32_CDECL pthread_mutex_init(pthread_mutex_t * mutex,
                                   const pthread_mutexattr_t * attr);

int PTW32_CDECL pthread_mutex_destroy(pthread_mutex_t * mutex);

int PTW32_CDECL pthread_mutex_lock(pthread_mutex_t * mutex);

int PTW32_CDECL pthread_mutex_timedlock(pthread_mutex_t * mutex,
                                        const struct timespec *abstime);

int PTW32_CDECL pthread_mutex_trylock(pthread_mutex_t * mutex);

int PTW32_CDECL pthread_mutex_unlock(pthread_mutex_t * mutex);

int PTW32_CDECL pthread_mutex_consistent(pthread_mutex_t * mutex);

/*
 * Spinlock Functions
 */
int PTW32_CDECL pthread_spin_init(pthread_spinlock_t * lock, int pshared);

int PTW32_CDECL pthread_spin_destroy(pthread_spinlock_t * lock);

int PTW32_CDECL pthread_spin_lock(pthread_spinlock_t * lock);

int PTW32_CDECL pthread_spin_trylock(pthread_spinlock_t * lock);

int PTW32_CDECL pthread_spin_unlock(pthread_spinlock_t * lock);

#ifdef __cplusplus
}
#endif

#endif  /* !_JIMIC_PTHREAD_H_ */

#endif  /* (defined(_MSC_VER) || defined(__INTEL_COMPILER)) */

#endif  /* !(PTHREAD_H && _PTHREAD_H) */

#if (!defined(PTW32_API_HAVE_DEFINED)) || (PTW32_API_HAVE_DEFINED == 0)

#ifndef PTW32_API
#define PTW32_API
#endif

#if defined(__MINGW32__) || defined(__CYGWIN__)

#ifndef PTW32_CDECL
#define PTW32_CDECL     __cdecl
#endif  // PTW32_CDECL

#include "vs_stdint.h"
#include "msvc/sched.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "msvc/targetver.h"
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * About the thread affinity
 */
int PTW32_CDECL pthread_getaffinity_np(pthread_t thread, size_t cpuset_size,
                                       cpu_set_t * cpuset);

int PTW32_CDECL pthread_setaffinity_np(pthread_t thread, size_t cpuset_size,
                                       const cpu_set_t * cpuset);

#ifdef __cplusplus
}
#endif

#endif  /* defined(__MINGW32__) || defined(__CYGWIN__) */

#endif  /* PTW32_API_HAVE_DEFINED */
