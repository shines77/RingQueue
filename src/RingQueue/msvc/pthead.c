
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)

#include "msvc/pthread.h"

#include <windows.h>
#include <process.h>    /* _beginthreadex(), _endthreadex() */

#include "port.h"

#include <stdio.h>
#include <assert.h>

int PTW32_CDECL pthread_attr_init(pthread_attr_t * attr)
{
    if (attr != NULL) {
        *attr = NULL;
        return (int)0;
    }
    else
        return (int)-1;
}

int PTW32_CDECL pthread_attr_destroy(pthread_attr_t * attr)
{
    return 0;
}

pthread_t PTW32_CDECL pthread_self(void)
{
    ///
    /// 关于GetCurrentThread()的返回
    ///
    /// See: http://www.cnblogs.com/freebird92/articles/846520.html
    ///

    HANDLE hCurrentThread = NULL;

    DuplicateHandle(
        GetCurrentProcess(),        // Handle of process that thread
                                    // pseudo-handle is relative to
        GetCurrentThread(),         // Current thread's pseudo-handle
        GetCurrentProcess(),        // Handle of process that the new, real,
                                    // thread handle is relative to
        &hCurrentThread,            // Will receive the new, real, handle
                                    // identifying the parent thread
        0,                          // Ignored due to DUPLICATE_SAME_ACCESS
        FALSE,                      // New thread handle is not inheritable
        DUPLICATE_SAME_ACCESS       // New thread handle has same
    );

    return (pthread_t)hCurrentThread;
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
    return (int)(!bResult);
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
    return (int)(dwResponse != WAIT_OBJECT_0);
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
    return (int)(!bResult);
#else
    InitializeCriticalSection(mutex);
    return 0;
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
    return (int)(mutex == NULL);
}

int PTW32_CDECL pthread_mutex_timedlock(pthread_mutex_t * mutex,
                                        const struct timespec *abstime)
{
    EnterCriticalSection(mutex);
    return (int)(mutex == NULL);
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
    return (int)(mutex == NULL);
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

#if defined(_MSC_VER) || defined(__INTEL_COMPILER) || defined(__MINGW32__) || defined(__CYGWIN__)

#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <pthread.h>
#include "msvc/pthread.h"
#include "port.h"
#endif

pthread_t PTW32_CDECL pthread_process_self(void)
{
    ///
    /// 关于GetCurrentThread()的返回
    ///
    /// See: http://www.cnblogs.com/freebird92/articles/846520.html
    ///
#if defined(__MINGW32__) || defined(__CYGWIN__)
    pthread_t ptw32_process = { 0, 0 };
#endif
    HANDLE hCurrentProcess = NULL;

    DuplicateHandle(
        GetCurrentProcess(),        // Handle of process that process
                                    // pseudo-handle is relative to
        GetCurrentProcess(),        // Process's pseudo-handle
        GetCurrentProcess(),        // Handle of process that the new, real,
                                    // thread handle is relative to
        &hCurrentProcess,           // Will receive the new, real, handle
                                    // identifying the process
        0,                          // Ignored due to DUPLICATE_SAME_ACCESS
        FALSE,                      // New thread handle is not inheritable
        DUPLICATE_SAME_ACCESS       // New thread handle has same
    );

#if defined(__MINGW32__) || defined(__CYGWIN__)
    ptw32_process.p = hCurrentProcess;
    ptw32_process.x = 0;
    return ptw32_process;
#else

    return (pthread_t)hCurrentProcess;
#endif
}

/*
 * About the thread affinity
 */
int PTW32_CDECL pthread_getaffinity_np(pthread_t thread, size_t cpuset_size,
                                       cpu_set_t * cpuset)
{
    return 0;
}

int PTW32_CDECL pthread_setaffinity_np(pthread_t thread_in, size_t cpuset_size,
                                        const cpu_set_t * cpuset)
{
    static const int echo = 1;
    HANDLE hCurrentProcess;
    HANDLE hTargetThread, thread;
    DWORD dwProcessAffinity, dwSystemAffinity;
    DWORD dwAffinityMask, dwAffinityMaskNew;
#if defined(__MINGW32__) || defined(__CYGWIN__)
    pthread_t threadTmp;
#endif
    BOOL bAffResult;
    int numCore;
    unsigned int loop_cnt;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    thread = thread_in.p;
#else
    thread = thread_in;
#endif

    //assert(thread != NULL && thread != INVALID_HANDLE_VALUE);
    //assert(cpuset != NULL);
    //assert(cpuset_size != 0);

    if (thread == NULL || thread == INVALID_HANDLE_VALUE) {
        return -1;
    }

    numCore = jimi_get_processor_num();
    if (numCore <= 1) {
        return 0;
    }

    if (echo) {
        printf("\n");
    }

    dwAffinityMask = cpuset->mask;
    if (dwAffinityMask == 0) {
        dwAffinityMask = 0xFFFFFFFFU;
    }

    // Check and set dwProcessAffinity to dwSystemAffinity
    loop_cnt = 0;
#if defined(__MINGW32__) || defined(__CYGWIN__)
    threadTmp = pthread_process_self();
    hCurrentProcess = (HANDLE)threadTmp.p;
#else
    hCurrentProcess = pthread_process_self();
#endif
    dwProcessAffinity = 0; dwSystemAffinity = 0;
    bAffResult = GetProcessAffinityMask(hCurrentProcess, &dwProcessAffinity, &dwSystemAffinity);
    if (bAffResult) {
        while (dwProcessAffinity != dwSystemAffinity) {
            if (echo) {
                printf("This process can not utilize all processors.\n");
            }
            bAffResult = SetProcessAffinityMask(hCurrentProcess, dwSystemAffinity);
            loop_cnt++;
            if (loop_cnt > 10) {
                printf("SetProcessAffinityMask(): bAffResult = %u, loop_cnt = %u.\n", bAffResult, loop_cnt);
                break;
            }
            if (bAffResult) {
                // Wait for the process affinity effected
                Sleep(1);
                // Update dwProcessAffinity
                bAffResult = GetProcessAffinityMask(hCurrentProcess, &dwProcessAffinity, &dwSystemAffinity);
            }
        }
    }
    else {
        return -1;
    }

    // Adjust dwAffinityMask
    dwAffinityMask = dwAffinityMask & dwProcessAffinity;

    // Set the affinity mask
#if defined(__MINGW32__) || defined(__CYGWIN__)
    hTargetThread = thread_in.p;
#else
    hTargetThread = thread;
#endif
    dwAffinityMaskNew = SetThreadAffinityMask(hTargetThread, dwAffinityMask);

    if (echo) {
        printf("\n");
    }

    if (dwAffinityMaskNew == 0)
        return -1;

    return 0;
}

#if 0

int PTW32_CDECL pthread_setaffinity_np2(pthread_t thread_in, size_t cpuset_size,
                                        const cpu_set_t * cpuset)
{
    static const int echo = 1;
    HANDLE hCurrentProcess = pthread_process_self();
    HANDLE hCurrentThread = pthread_self(), thread;
    DWORD dwProcessAffinity = 0, dwSystemAffinity = 0;
    DWORD dwAffinityMask;
    BOOL bAffResult;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    thread = thread_in.p;
#else
    thread = thread_in;
#endif

    assert(thread != NULL);
    assert(cpuset != NULL);
    assert(cpuset_size != 0);

    if (echo) {
        printf("\n");
    }

    dwAffinityMask = cpuset->mask;
    bAffResult = GetProcessAffinityMask(hCurrentProcess, &dwProcessAffinity, &dwSystemAffinity);
    if (bAffResult) {
        if (dwProcessAffinity != dwSystemAffinity) {
            if (echo) {
                printf("This process can not utilize all processors.\n");
            }
        }

        while ((dwAffinityMask != 0) && (dwAffinityMask <= dwProcessAffinity)) {
            // Check to make sure we can utilize this processsor first.
            if ((dwAffinityMask & dwProcessAffinity) != 0) {
                bAffResult = SetProcessAffinityMask(hCurrentProcess, dwAffinityMask);
                if (bAffResult) {
                    // Wait for the process affinity effected
                    Sleep(1);
                    //
                    if (echo) {
                        printf("SetProcessAffinityMask(): dwAffinityMask = 0x%08X\n", dwAffinityMask);
                    }
                    DWORD dwProcessAffinityNew = 0;
                    bAffResult = GetProcessAffinityMask(hCurrentProcess, &dwProcessAffinityNew, &dwSystemAffinity);
                    if (dwProcessAffinityNew == dwAffinityMask) {
                        if (echo) {
                            printf("SetProcessAffinityMask(): Success.\n");
                        }
                        bAffResult = SetThreadAffinityMask(hCurrentThread, dwAffinityMask);
                        Sleep(1);
                        break;
                    }
                }
            }
        }
    }

    if (echo) {
        printf("\n");
    }
    return 0;
}

#endif

#endif  // defined(_MSC_VER) || defined(__INTERL_COMPILER) || defined(__MINGW32__) || defined(__CYGWIN__)
