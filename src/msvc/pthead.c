
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)

#include "msvc/pthread.h"

int PTW32_CDECL pthread_attr_init(pthread_attr_t * attr)
{
    return 0;
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
                               void *arg)
{
    return 0;
}

int PTW32_CDECL pthread_detach(pthread_t tid)
{
    return 0;
}

int PTW32_CDECL pthread_join(pthread_t thread, void **value_ptr)
{
    return 0;
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
    return 0;
}

int PTW32_CDECL pthread_mutex_destroy(pthread_mutex_t * mutex)
{
    return 0;
}

int PTW32_CDECL pthread_mutex_lock(pthread_mutex_t * mutex)
{
    return 0;
}

int PTW32_CDECL pthread_mutex_timedlock(pthread_mutex_t * mutex,
                                        const struct timespec *abstime)
{
    return 0;
}

int PTW32_CDECL pthread_mutex_trylock(pthread_mutex_t * mutex)
{
    return 0;
}

int PTW32_CDECL pthread_mutex_unlock(pthread_mutex_t * mutex)
{
    return 0;
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
