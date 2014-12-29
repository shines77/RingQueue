
#ifndef _JIMI_UTIL_SPINMUTEX_H_
#define _JIMI_UTIL_SPINMUTEX_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include "vs_stdint.h"

#include "test.h"
#include "port.h"
#include "sleep.h"

#ifndef _MSC_VER
#include <pthread.h>
#include "msvc/pthread.h"
#else
#include "msvc/pthread.h"
#endif  // !_MSC_VER

#ifdef _MSC_VER
#include <intrin.h>     // For _ReadWriteBarrier(), InterlockedCompareExchange()
#endif  // _MSC_VER
#include <emmintrin.h>

#include <stdio.h>
#include <string.h>

#include "dump_mem.h"

#ifndef JIMI_CACHE_LINE_SIZE
#define JIMI_CACHE_LINE_SIZE    64
#endif

#define SPINMUTEX_DEFAULT_SPIN_COUNT    4000

namespace jimi {

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

typedef struct SpinMutexYieldInfo SpinMutexYieldInfo;

struct SpinMutexYieldInfo
{
    uint32_t loop_count;
    uint32_t spin_count;
};

///////////////////////////////////////////////////////////////////
// struct SpinMutexHelper<>
///////////////////////////////////////////////////////////////////

template < uint32_t _YieldThreshold = 1U,
           uint32_t _SpinCountInitial = 2U,
           uint32_t _CoeffA = 2U,
           uint32_t _CoeffB = 1U,
           uint32_t _CoeffC = 0U,
           uint32_t _Sleep_0_Interval = 4U,
           uint32_t _Sleep_1_Interval = 32U,
           bool     _UseYieldProcessor = true,
           bool     _NeedReset = false >
class SpinMutexHelper
{
public:
    static const uint32_t YieldThreshold        = _YieldThreshold;
    static const uint32_t SpinCountInitial      = _SpinCountInitial;
    static const uint32_t CoeffA                = _CoeffA;
    static const uint32_t CoeffB                = _CoeffB;
    static const uint32_t CoeffC                = _CoeffC;
    static const uint32_t Sleep_0_Interval      = _Sleep_0_Interval;
    static const uint32_t Sleep_1_Interval      = _Sleep_1_Interval;
    static const bool     UseYieldProcessor     = _UseYieldProcessor;
    static const bool     NeedReset             = _NeedReset;
};

typedef SpinMutexHelper<>  DefaultSMHelper;

/*******************************************************************************

  class SpinMutex<SpinHelper>

  Example:

    SpinMutex<DefaultSMHelper> spinMutex2;

    typedef SpinMutexHelper<
        1,      // _YieldThreshold, The threshold of enter yield(), the spin loop times.
        2,      // _SpinCountInitial, The initial value of spin counter.
        2,      // _A
        1,      // _B
        0,      // _C, Next loop: spin_count = spin_count * _A / _B + _C;
        4,      // _Sleep_0_Interval, After how many yields should we Sleep(0)?
        32,     // _Sleep_1_Interval, After how many yields should we Sleep(1)?
        true,   // _UseYieldProcessor? Whether use jimi_yield() function in loop.
        false   // _NeedReset? After run Sleep(1), reset the loop_count if need.
    > MySpinMutexHelper;

    SpinMutex<MySpinMutexHelper> spinMutex;

********************************************************************************/

template < typename SpinHelper = SpinMutexHelper<> >
class SpinMutex
{
public:
    typedef SpinHelper      helper_type;

public:
    static const uint32_t kLocked   = 1U;
    static const uint32_t kUnlocked = 0U;

    static const uint32_t kYieldThreshold       = helper_type::YieldThreshold;
    static const uint32_t kSpinCountInitial     = helper_type::SpinCountInitial;
    static const uint32_t kA                    = helper_type::CoeffA;
    static const uint32_t kB                    = helper_type::CoeffB;
    static const uint32_t kC                    = helper_type::CoeffC;
    static const uint32_t kSleep_1_Interval     = helper_type::Sleep_1_Interval;
    static const uint32_t kSleep_0_Interval     = helper_type::Sleep_0_Interval;
    static const bool     kUseYieldProcessor    = helper_type::UseYieldProcessor;
    static const bool     kNeedReset            = helper_type::NeedReset;

    /* SPINMUTEX_DEFAULT_SPIN_COUNT = 4000 */
    static const int32_t  kDefaultSpinCount = SPINMUTEX_DEFAULT_SPIN_COUNT;

    /* kYieldThreshold default value is 1. */
    static const uint32_t YIELD_THRESHOLD  = kYieldThreshold;   // When to switch over to a true yield.
    /* kSleep_0_Interval default value is 4. */
    static const uint32_t SLEEP_0_INTERVAL = kSleep_0_Interval; // After how many yields should we Sleep(0)?
    /* kSleep_1_Interval default value is 32. */
#if defined(_M_X64) || defined(_WIN64) || defined(_M_AMD64)
    // Because Sleep(1) is too slowly in x64, Win64 or AMD64 mode, so we let Sleep(1) interval is double.
    static const uint32_t SLEEP_1_INTERVAL = kSleep_1_Interval * 2;
                                                                // After how many yields should we Sleep(1)?
#else
    static const uint32_t SLEEP_1_INTERVAL = kSleep_1_Interval; // After how many yields should we Sleep(1)?
#endif  /* _M_X64 || _WIN64 || _M_AMD64 */

public:
    SpinMutex()  { core.Status = kUnlocked; };
    ~SpinMutex() { /* Do nothing! */        };

public:
    void lock();
    bool tryLock(int nSpinCount = kDefaultSpinCount);
    void unlock();

    void yield_reset(SpinMutexYieldInfo &yieldInfo);
    void yield(SpinMutexYieldInfo &yieldInfo);

    static void spinWait(int nSpinCount = kDefaultSpinCount);

private:
    SpinMutexCore core;
};

template <typename SpinHelper>
inline
void SpinMutex<SpinHelper>::spinWait(int nSpinCount /* = kDefaultSpinCount(4000) */)
{
    for (; nSpinCount > 0; --nSpinCount) {
        jimi_mm_pause();
    }
}

template <typename SpinHelper>
void SpinMutex<SpinHelper>::lock()
{
    uint32_t loop_count, spin_count, yield_cnt;
    int32_t pause_cnt;

    Jimi_ReadWriteBarrier();

    /* atomic_exchange usually takes less instructions than
       atomic_compare_and_exchange.  On the other hand,
       atomic_compare_and_exchange potentially generates less bus traffic
       when the lock is locked.
       We assume that the first try mostly will be successful, and we use
       atomic_exchange.  For the subsequent tries we use
       atomic_compare_and_exchange.  */
    if (jimi_lock_test_and_set32(&core.Status, kLocked) != kUnlocked) {
        loop_count = 0;
        spin_count = kSpinCountInitial;
        do {
            if (loop_count < YIELD_THRESHOLD) {
                for (pause_cnt = spin_count; pause_cnt > 0; --pause_cnt) {
                    jimi_mm_pause();
                }
                if (kB == 0)
                    spin_count = spin_count + kC;
                else
                    spin_count = spin_count * kA / kB + kC;
            }
            else {
                // Yield count is base on YIELD_THRESHOLD
                yield_cnt = loop_count - YIELD_THRESHOLD;

#if (defined(__linux__) || defined(__GNUC__) \
    || defined(__clang__) || defined(__APPLE__) || defined(__FreeBSD__)) \
    && !(defined(__MINGW32__) || defined(__CYGWIN__) || defined(__MSYS__))
                if ((SLEEP_1_INTERVAL != 0) &&
                    (yield_cnt % SLEEP_1_INTERVAL) == (SLEEP_1_INTERVAL - 1)) {
                    // On Windows: 休眠一个时间片, 可以切换到任何物理Core上的任何等待中的线程/进程.
                    // On Linux: 等价于usleep(1).
                    jimi_wsleep(1);
                    // If enter Sleep(1) one time, reset the loop_count if need.
                    if (kNeedReset)
                        loop_count = 0; 
                }
                else {
                    // 在Linux下面, 因为jimi_yield()和jimi_wsleep(0)是等价的, 所以可以省略掉jimi_wsleep(0)部分的代码
                    if (kUseYieldProcessor || (SLEEP_1_INTERVAL != 0)) {
                        jimi_yield();
                    }
                }
#else
                if ((SLEEP_1_INTERVAL != 0) &&
                    (yield_cnt % SLEEP_1_INTERVAL) == (SLEEP_1_INTERVAL - 1)) {
#if defined(__MINGW32__) || defined(__CYGWIN__) || defined(__MSYS__)
                    // Because Sleep(1) is too slowly in MinGW or cygwin, so we do not use it.
                    // Do nothing!!!
#else
                    // jimi_wsleep(1) 休眠一个时间片, 可以切换到任何物理Core上的任何等待中的线程/进程 on Windows.
                    // jimi_wsleep(1) is equivalent to usleep(1) on Linux.
                    jimi_wsleep(1);
#endif
                    // If enter Sleep(1) one time, reset the loop_count if need.
                    if (kNeedReset)
                        loop_count = 0;
                }
                else if ((SLEEP_0_INTERVAL != 0) &&
                         (yield_cnt % SLEEP_0_INTERVAL) == (SLEEP_0_INTERVAL - 1)) {
                    //jimi_wsleep(0) on Windows: 只切换到跟当前线程优先级别相同的其他线程, 允许切换到其他物理Core的线程.
                    //jimi_wsleep(0) on Linux: 因为Linux上的usleep(0)无法实现Windows上的Sleep(0)的效果, 所以这里等价于sched_yield().
                    jimi_wsleep(0);
                }
                else {
                    // jimi_yield() on Windows: 只切换到当前线程所在的物理Core中其他线程, 即使别的Core中有合适的等待线程.
                    // jimi_yield() on Linux: sched_yield(), 把当前线程/进程放到等待线程/进程的末尾, 然后切换到等待线程/进程列表中的首个线程/进程.
                    if (kUseYieldProcessor) {
                        // if YieldProcessor() failed, then directly enter Sleep(0).
                        if (!jimi_yield()) {
                            jimi_wsleep(0);
                        }
                    }
                }
#endif  /* defined(__linux__) || defined(__GNUC__) || defined(__clang__) */
            }
            // Just let the code look well
            loop_count++;
        } while (jimi_val_compare_and_swap32(&core.Status, kUnlocked, kLocked) != kUnlocked);
    }
}

template <typename SpinHelper>
bool SpinMutex<SpinHelper>::tryLock(int nSpinCount /* = kDefaultSpinCount(4000) */)
{
    Jimi_ReadWriteBarrier();

    /* atomic_exchange usually takes less instructions than
       atomic_compare_and_exchange.  On the other hand,
       atomic_compare_and_exchange potentially generates less bus traffic
       when the lock is locked.
       We assume that the first try mostly will be successful, and we use
       atomic_exchange.  For the subsequent tries we use
       atomic_compare_and_exchange.  */
    if (jimi_lock_test_and_set32(&core.Status, kLocked) != kUnlocked) {
        for (; nSpinCount > 0; --nSpinCount) {
            jimi_mm_pause();
        }
        bool isLocked =
            (jimi_val_compare_and_swap32(&core.Status, kUnlocked, kLocked)
                                        != kUnlocked);
        return isLocked;
    }

    return kLocked;
}

template <typename Helper>
void SpinMutex<Helper>::unlock()
{
    Jimi_ReadWriteBarrier();

    core.Status = kUnlocked;
}

template <typename SpinHelper>
inline
void SpinMutex<SpinHelper>::yield_reset(SpinMutexYieldInfo &yieldInfo)
{
    yieldInfo.loop_count = 0;
    yieldInfo.spin_count = kSpinCountInitial;
}

template <typename SpinHelper>
inline
void SpinMutex<SpinHelper>::yield(SpinMutexYieldInfo &yieldInfo)
{
    uint32_t loop_count, spin_count, yield_cnt;
    int32_t pause_cnt;

    loop_count = yieldInfo.loop_count;
    spin_count = yieldInfo.spin_count;

    if (loop_count < YIELD_THRESHOLD) {
        for (pause_cnt = spin_count; pause_cnt > 0; --pause_cnt) {
            jimi_mm_pause();
        }
        if (kB == 0)
            yieldInfo.spin_count = spin_count + kC;
        else
            yieldInfo.spin_count = spin_count * kA / kB + kC;
    }
    else {
        // Yield count is base on YIELD_THRESHOLD
        yield_cnt = loop_count - YIELD_THRESHOLD;

#if (defined(__linux__) || defined(__GNUC__) \
|| defined(__clang__) || defined(__APPLE__) || defined(__FreeBSD__)) \
&& !(defined(__MINGW32__) || defined(__CYGWIN__) || defined(__MSYS__))
        if ((SLEEP_1_INTERVAL != 0) &&
            (yield_cnt % SLEEP_1_INTERVAL) == (SLEEP_1_INTERVAL - 1)) {
            // On Windows: 休眠一个时间片, 可以切换到任何物理Core上的任何等待中的线程/进程.
            // On Linux: 等价于usleep(1).
            jimi_wsleep(1);
            // If enter Sleep(1) one time, reset the loop_count if need.
            if (kNeedReset) {
                yield_reset(yieldInfo);
            }
        }
        else {
            // 在Linux下面, 因为jimi_yield()和jimi_wsleep(0)是等价的, 所以可以省略掉jimi_wsleep(0)部分的代码
            if (kUseYieldProcessor || (SLEEP_1_INTERVAL != 0)) {
                jimi_yield();
            }
        }
#else
        if ((SLEEP_1_INTERVAL != 0) &&
            (yield_cnt % SLEEP_1_INTERVAL) == (SLEEP_1_INTERVAL - 1)) {
#if defined(__MINGW32__) || defined(__CYGWIN__) || defined(__MSYS__)
            // Because Sleep(1) is too slowly in MinGW or cygwin, so we do not use it.
            // Do nothing!!!
#else
            // jimi_wsleep(1) 休眠一个时间片, 可以切换到任何物理Core上的任何等待中的线程/进程 on Windows.
            // jimi_wsleep(1) is equivalent to usleep(1) on Linux.
            jimi_wsleep(1);
#endif
            // If enter Sleep(1) one time, reset the loop_count if need.
            if (kNeedReset) {
                yield_reset(yieldInfo);
            }
        }
        else if ((SLEEP_0_INTERVAL != 0) &&
                    (yield_cnt % SLEEP_0_INTERVAL) == (SLEEP_0_INTERVAL - 1)) {
            //jimi_wsleep(0) on Windows: 只切换到跟当前线程优先级别相同的其他线程, 允许切换到其他物理Core的线程.
            //jimi_wsleep(0) on Linux: 因为Linux上的usleep(0)无法实现Windows上的Sleep(0)的效果, 所以这里等价于sched_yield().
            jimi_wsleep(0);
        }
        else {
            // jimi_yield() on Windows: 只切换到当前线程所在的物理Core中其他线程, 即使别的Core中有合适的等待线程.
            // jimi_yield() on Linux: sched_yield(), 把当前线程/进程放到等待线程/进程的末尾, 然后切换到等待线程/进程列表中的首个线程/进程.
            if (kUseYieldProcessor) {
                // if YieldProcessor() failed, then directly enter Sleep(0).
                if (!jimi_yield()) {
                    jimi_wsleep(0);
                }
            }
        }
#endif  /* defined(__linux__) || defined(__GNUC__) || defined(__clang__) */
    }
    // Just let the code look well.
    loop_count++;
    // Update loop_count to yieldInfo.
    yieldInfo.loop_count = loop_count;
}

}  /* namespace jimi */

#endif  /* _JIMI_UTIL_SPINMUTEX_H_ */
