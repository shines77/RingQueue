
#ifndef _JIMI_UTIL_SPINMUTEX_H_
#define _JIMI_UTIL_SPINMUTEX_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include <stdio.h>
#include <string.h>

#include "vs_stdint.h"

#ifndef _MSC_VER
#include <pthread.h>
#include "msvc/pthread.h"
#else
#include "msvc/pthread.h"
#endif

#ifdef _MSC_VER
#include <intrin.h>     // For _ReadWriteBarrier(), InterlockedCompareExchange()
#endif
#include <emmintrin.h>

#include "test.h"
#include "port.h"
#include "sleep.h"
#include "dump_mem.h"

namespace jimi {

#ifndef JIMI_CACHE_LINE_SIZE
#define JIMI_CACHE_LINE_SIZE    64
#endif

///////////////////////////////////////////////////////////////////
// struct SpinMutexCore
///////////////////////////////////////////////////////////////////

typedef struct SpinMutexCore SpinMutexCore;

struct SpinMutexCore
{
    volatile char paddding1[JIMI_CACHE_LINE_SIZE];
    volatile uint32_t Status;
    volatile char paddding2[JIMI_CACHE_LINE_SIZE - sizeof(uint32_t)];
};

///////////////////////////////////////////////////////////////////
// struct SpinMutexHelper<>
///////////////////////////////////////////////////////////////////

template <uint32_t _YieldThreshold = 4,
          uint32_t _SpinCount = 4000,
          uint32_t _A = 2, uint32_t _B = 1, uint32_t _C = 0,
          bool _NeedReset = 0>
struct SpinMutexHelper
{
public:
    static const uint32_t YieldThreshold    = _YieldThreshold;
    static const uint32_t SpinCount         = _SpinCount;
    static const uint32_t A                 = _A;
    static const uint32_t B                 = _B;
    static const uint32_t C                 = _C;
    static const uint32_t NeedReset         = _NeedReset;
};

typedef SpinMutexHelper<>  DefaultSMHelper;

/*******************************************************************************

  class SpinMutex<SpinHelper>

  Example:

    SpinMutex<DefaultSMHelper> spinMutex2;

    typedef SpinMutexHelper<
        5,      // _YieldThreshold, The spin loop times
        4000,   // _SpinCount, The initial value of spin counter
        2,      // _A
        1,      // _B
        0,      // _C, Next loop: spin_count = spin_count * _A / _B + _C;
        0       // _NeedReset? After run Sleep(1), reset the loop_count if need.
    > MySpinMutexHelper;

    SpinMutex<MySpinMutexHelper> spinMutex;

********************************************************************************/

template <typename SpinHelper>
class SpinMutex
{
public:
    static const uint32_t kLocked   = 1U;
    static const uint32_t kUnlocked = 0U;
    static const uint32_t kUnLocked = kUnlocked;

    static const uint32_t kYieldThreshold   = SpinHelper::YieldThreshold;
    static const uint32_t kSpinCount        = SpinHelper::SpinCount;
    static const uint32_t kA                = SpinHelper::A;
    static const uint32_t kB                = SpinHelper::B;
    static const uint32_t kC                = SpinHelper::C;
    static const uint32_t kNeedReset        = SpinHelper::NeedReset;

    static const uint32_t YIELD_THRESHOLD  = kYieldThreshold;   // When to switch over to a true yield.
    static const uint32_t SLEEP_0_INTERVAL = 4;                 // After how many yields should we Sleep(0)?
    static const uint32_t SLEEP_1_INTERVAL = 16;                // After how many yields should we Sleep(1)?

public:
    SpinMutex()  { core.Status = kUnlocked; };
    ~SpinMutex() { /* Do nothing! */        };

public:
    void lock();
    bool tryLock(unsigned int nSpinCount = 4000);
    void unlock();

    void spinWait(unsigned int nSpinCount = 4000);

private:
    SpinMutexCore core;
};

template <typename Helper>
void SpinMutex<Helper>::spinWait(unsigned int nSpinCount /* = 4000 */)
{
    for (; nSpinCount > 0; --nSpinCount) {
        jimi_mm_pause();
    }
}

template <typename Helper>
void SpinMutex<Helper>::lock()
{
    uint32_t loop_count, spin_count, pause_cnt, yield_cnt;

    printf("SpinMutex<T>::lock(): Enter().\n");

    Jimi_ReadWriteBarrier();

    /* atomic_exchange usually takes less instructions than
       atomic_compare_and_exchange.  On the other hand,
       atomic_compare_and_exchange potentially generates less bus traffic
       when the lock is locked.
       We assume that the first try mostly will be successful, and we use
       atomic_exchange.  For the subsequent tries we use
       atomic_compare_and_exchange.  */
    if (jimi_lock_test_and_set32(&core.Status, kLocked) != kUnLocked) {
        loop_count = 1;
        spin_count = kSpinCount;
        do {
            if (loop_count <= YIELD_THRESHOLD) {
                for (pause_cnt = spin_count; pause_cnt > 0; --pause_cnt) {
                    jimi_mm_pause();
                    //jimi_mm_pause();
                }
                if (kB == 0)
                    spin_count = spin_count + kC;
                else
                    spin_count = spin_count * kA / kB + kC;
            }
            else {
                // Yield Count is base on YIELD_THRESHOLD
                yield_cnt = loop_count - YIELD_THRESHOLD;

#if defined(__MINGW32__) || defined(__CYGWIN__)
                // Sleep(1) is too slowly in MinGW or cygwin, so we do not use it.
                if ((yield_cnt % SLEEP_1_INTERVAL) == (SLEEP_1_INTERVAL - 1)) {
                    // If enter Sleep(1) one time, reset the loop_count if need.
                    if (kNeedReset != 0)
                        loop_count = 1;
                }
                else if ((yield_cnt % SLEEP_0_INTERVAL) == (SLEEP_0_INTERVAL - 1)) {
                    jimi_wsleep(0);
                }
                else {
                    jimi_yield();
                }
#else
                if ((yield_cnt % SLEEP_1_INTERVAL) == (SLEEP_1_INTERVAL - 1)) {
                    jimi_wsleep(1);
                    // If enter Sleep(1) one time, reset the loop_count if need.
                    if (kNeedReset != 0)
                        loop_count = 1;
                }
                else if ((yield_cnt % SLEEP_0_INTERVAL) == (SLEEP_0_INTERVAL - 1)) {
                    jimi_wsleep(0);
                }
                else {
                    jimi_yield();
                }
#endif  /* defined(__MINGW32__) || defined(__CYGWIN__) */
            }
            // Just let the code look well
            loop_count++;
        } while (jimi_val_compare_and_swap32(&core.Status, kUnLocked, kLocked) != kUnLocked);
    }

    printf("SpinMutex<T>::lock(): Leave().\n");
}

template <typename Helper>
bool SpinMutex<Helper>::tryLock(unsigned int nSpinCount /* = 4000 */)
{
    printf("SpinMutex<T>::tryLock(): Enter().\n");

    Jimi_ReadWriteBarrier();

    /* atomic_exchange usually takes less instructions than
       atomic_compare_and_exchange.  On the other hand,
       atomic_compare_and_exchange potentially generates less bus traffic
       when the lock is locked.
       We assume that the first try mostly will be successful, and we use
       atomic_exchange.  For the subsequent tries we use
       atomic_compare_and_exchange.  */
    if (jimi_lock_test_and_set32(&core.Status, kLocked) != kUnLocked) {
        for (; nSpinCount > 0; --nSpinCount) {
            jimi_mm_pause();
        }
        bool isLocked = (jimi_val_compare_and_swap32(&core.Status, kUnLocked, kLocked) != kUnLocked);
        printf("SpinMutex<T>::tryLock(): Leave().\n");
        return isLocked;
    }

    printf("SpinMutex<T>::ltryLockock(): Leave().\n");
    return kLocked;
}

template <typename Helper>
void SpinMutex<Helper>::unlock()
{
    printf("SpinMutex<T>::unlock(): Enter().\n");

    Jimi_ReadWriteBarrier();

    core.Status = kUnLocked;

    printf("SpinMutex<T>::unlock(): Leave().\n");
}

}  /* namespace jimi */

#endif  /* _JIMI_UTIL_SPINMUTEX_H_ */
