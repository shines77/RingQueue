
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)

#include "msvc/pthread.h"

#include <windows.h>
#include <process.h>    /* _beginthreadex(), _endthreadex() */

#include <stdio.h>

int PTW32_CDECL pthread_attr_init(pthread_attr_t * attr)
{
    if (attr != NULL) {
        *attr = NULL;
        return 0;
    }
    else
        return 1;
}

int PTW32_CDECL pthread_attr_destroy(pthread_attr_t * attr)
{
    return 0;
}

/*
 * PThread Functions
 */
int PTW32_CDECL pthread_create(pthread_t * tid,
                               const pthread_attr_t * attr,
                               void *(PTW32_API *start)(void *),
                               void * arg)
{
    pthread_t hThread;
    unsigned int nThreadId;
    DWORD dwErrorCode;
    hThread = (pthread_t)_beginthreadex((void *)NULL, 0, (pthread_proc_t)start,
                                        arg, 0, (unsigned int *)&nThreadId);
    if (hThread == INVALID_HANDLE_VALUE || hThread == NULL) {
        dwErrorCode = GetLastError();
        printf("pthread_create(): dwErrorCode = 0x%08X\n", dwErrorCode);
        return (int)-1;
    }
    if (tid != NULL)
        *tid = hThread;
    return (int)0;
}

int PTW32_CDECL pthread_detach(pthread_t tid)
{
    BOOL bResult;
    bResult = TerminateThread((HANDLE)tid, -1);
    if (bResult && tid != NULL)
        bResult = CloseHandle((HANDLE)tid);
    return bResult;
}

int PTW32_CDECL pthread_join(pthread_t thread, void **value_ptr)
{
    DWORD dwMillisecs;
    DWORD dwResponse;
    if (value_ptr == NULL)
        dwMillisecs = INFINITE;
    else
        dwMillisecs = (DWORD)(*value_ptr);
    dwResponse = WaitForSingleObject(thread, dwMillisecs);
    return (int)dwResponse;
}

int PTW32_CDECL pthread_setaffinity_np(pthread_t thread, unsigned int size,
                                       void * data)
{
    return 0;
}

/*
 * Mutex Functions
 */
int PTW32_CDECL pthread_mutex_init(pthread_mutex_t * mutex,
                                   const pthread_mutexattr_t * attr)
{
#if 0
    DWORD dwSpinCounter;
    BOOL bResult;
    if (attr == NULL)
        dwSpinCounter = 4000;
    else
        dwSpinCounter = (DWORD)*attr;
    bResult = InitializeCriticalSectionAndSpinCount(mutex, dwSpinCounter);
    return bResult;
#else
    BOOL bResult;
    bResult = InitializeCriticalSection(mutex);
    return bResult;
#endif
}

int PTW32_CDECL pthread_mutex_destroy(pthread_mutex_t * mutex)
{
    if (mutex != NULL)
        DeleteCriticalSection(mutex);
    return (int)(mutex != NULL);
}

int PTW32_CDECL pthread_mutex_lock(pthread_mutex_t * mutex)
{
    EnterCriticalSection(mutex);
    return (int)(mutex != NULL);
}

int PTW32_CDECL pthread_mutex_timedlock(pthread_mutex_t * mutex,
                                        const struct timespec *abstime)
{
    EnterCriticalSection(mutex);
    return (int)(mutex != NULL);
}

int PTW32_CDECL pthread_mutex_trylock(pthread_mutex_t * mutex)
{
    BOOL bResult = FALSE;
    if (mutex != NULL)
        bResult = TryEnterCriticalSection(mutex);
    return (int)bResult;
}

int PTW32_CDECL pthread_mutex_unlock(pthread_mutex_t * mutex)
{
    LeaveCriticalSection(mutex);
    return (int)(mutex != NULL);
}

int PTW32_CDECL pthread_mutex_consistent(pthread_mutex_t * mutex)
{
    return 0;
}

/*
 * Spinlock Functions
 */
int PTW32_CDECL pthread_spin_init(pthread_spinlock_t * lock, int pshared)
{
    return 0;
}

int PTW32_CDECL pthread_spin_destroy(pthread_spinlock_t * lock)
{
    return 0;
}

int PTW32_CDECL pthread_spin_lock(pthread_spinlock_t * lock)
{
    return 0;
}

int PTW32_CDECL pthread_spin_trylock(pthread_spinlock_t * lock)
{
    return 0;
}

int PTW32_CDECL pthread_spin_unlock(pthread_spinlock_t * lock)
{
    return 0;
}

#endif  // defined(_MSC_VER) || defined(__INTERL_COMPILER)
