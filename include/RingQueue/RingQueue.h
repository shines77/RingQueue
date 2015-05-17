
#ifndef _JIMI_UTIL_RINGQUEUE_H_
#define _JIMI_UTIL_RINGQUEUE_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include "vs_stdint.h"
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

namespace jimi {

#if 0
struct RingQueueHead
{
    volatile uint32_t head;
    volatile uint32_t tail;
};
#else
struct RingQueueHead
{
    volatile uint32_t head;
    char padding1[JIMI_CACHE_LINE_SIZE - sizeof(uint32_t)];

    volatile uint32_t tail;
    char padding2[JIMI_CACHE_LINE_SIZE - sizeof(uint32_t)];
};
#endif

typedef struct RingQueueHead RingQueueHead;

///////////////////////////////////////////////////////////////////
// class SmallRingQueueCore<Capacity>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capacity>
class SmallRingQueueCore
{
public:
    typedef uint32_t    size_type;
    typedef T *         item_type;

public:
    static const size_type  kCapacityCore   = (size_type)JIMI_MAX(JIMI_ROUND_TO_POW2(Capacity), 2);
    static const bool       kIsAllocOnHeap  = false;

public:
    RingQueueHead       info;
#if 0
    volatile item_type  queue[kCapacityCore];
#else
    item_type           queue[kCapacityCore];
#endif
};

///////////////////////////////////////////////////////////////////
// class RingQueueCore<Capacity>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capacity>
class RingQueueCore
{
public:
    typedef T *         item_type;

    RingQueueCore()  {}
    ~RingQueueCore() {}

public:
    static const bool kIsAllocOnHeap = true;

public:
    RingQueueHead       info;
#if 0
    volatile item_type *queue;
#else
    item_type          *queue;
#endif
};

///////////////////////////////////////////////////////////////////
// class RingQueueBase<T, Capacity, CoreTy>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capacity = 16U,
          typename CoreTy = RingQueueCore<T, Capacity> >
class RingQueueBase
{
public:
    typedef uint32_t                    size_type;
    typedef uint32_t                    index_type;
    typedef T *                         value_type;
    typedef typename CoreTy::item_type  item_type;
    typedef CoreTy                      core_type;
    typedef T *                         pointer;
    typedef const T *                   const_pointer;
    typedef T &                         reference;
    typedef const T &                   const_reference;

public:
    static const size_type  kCapacity = (size_type)JIMI_MAX(JIMI_ROUND_TO_POW2(Capacity), 2);
    static const index_type kMask     = (index_type)(kCapacity - 1);

public:
    RingQueueBase(bool bInitHead = false);
    ~RingQueueBase();

public:
    void dump_info();
    void dump_detail();

    index_type mask() const      { return kMask;     };
    size_type capacity() const   { return kCapacity; };
    size_type length() const     { return sizes();   };
    size_type sizes() const;

    void init(bool bInitHead = false);

    int push(T * item);
    T * pop();

    int push2(T * item);
    T * pop2();

    int spin_push(T * item);
    T * spin_pop();

    int spin1_push(T * item);
    T * spin1_pop();

    int spin2_push(T * item);
    T * spin2_pop();

    int spin2_push_(T * item);

    int spin3_push(T * item);
    T * spin3_pop();

    int spin8_push(T * item);
    T * spin8_pop();

    int spin9_push(T * item);
    T * spin9_pop();

    int mutex_push(T * item);
    T * mutex_pop();

protected:
    core_type       core;
    spin_mutex_t    spin_mutex;
    pthread_mutex_t queue_mutex;
};

template <typename T, uint32_t Capacity, typename CoreTy>
RingQueueBase<T, Capacity, CoreTy>::RingQueueBase(bool bInitHead  /* = false */)
{
    //printf("RingQueueBase::RingQueueBase();\n\n");

    init(bInitHead);
}

template <typename T, uint32_t Capacity, typename CoreTy>
RingQueueBase<T, Capacity, CoreTy>::~RingQueueBase()
{
    // Do nothing!
    Jimi_WriteCompilerBarrier();

    spin_mutex.locked = 0;

    pthread_mutex_destroy(&queue_mutex);
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
void RingQueueBase<T, Capacity, CoreTy>::init(bool bInitHead /* = false */)
{
    //printf("RingQueueBase::init();\n\n");

    if (!bInitHead) {
        core.info.head = 0;
        core.info.tail = 0;
    }
    else {
        memset((void *)&core.info, 0, sizeof(core.info));
    }

    Jimi_CompilerBarrier();

    // Initilized spin mutex
    spin_mutex.locked = 0;
    spin_mutex.spin_counter = MUTEX_MAX_SPIN_COUNT;
    spin_mutex.recurse_counter = 0;
    spin_mutex.thread_id = 0;
    spin_mutex.reserve = 0;

    // Initilized mutex
    pthread_mutex_init(&queue_mutex, NULL);
}

template <typename T, uint32_t Capacity, typename CoreTy>
void RingQueueBase<T, Capacity, CoreTy>::dump_info()
{
    //ReleaseUtils::dump(&core.info, sizeof(core.info));
    dump_memory(&core.info, sizeof(core.info), false, 16, 0, 0);
}

template <typename T, uint32_t Capacity, typename CoreTy>
void RingQueueBase<T, Capacity, CoreTy>::dump_detail()
{
#if 0
    printf("---------------------------------------------------------\n");
    printf("RingQueueBase.p.head = %u\nRingQueueBase.p.tail = %u\n\n", core.info.p.head, core.info.p.tail);
    printf("RingQueueBase.c.head = %u\nRingQueueBase.c.tail = %u\n",   core.info.c.head, core.info.c.tail);
    printf("---------------------------------------------------------\n\n");
#else
    printf("RingQueueBase: (head = %u, tail = %u)\n",
           core.info.head, core.info.tail);
#endif
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
typename RingQueueBase<T, Capacity, CoreTy>::size_type
RingQueueBase<T, Capacity, CoreTy>::sizes() const
{
    index_type head, tail;

    Jimi_CompilerBarrier();

    head = core.info.head;

    tail = core.info.tail;

    return (size_type)((head - tail) <= kMask) ? (head - tail) : (size_type)-1;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
int RingQueueBase<T, Capacity, CoreTy>::push(T * item)
{
    index_type head, tail, next;
    bool ok = false;

    Jimi_CompilerBarrier();

    do {
        head = core.info.head;
        tail = core.info.tail;
        if ((head - tail) > kMask)
            return -1;
        next = head + 1;
        ok = jimi_bool_compare_and_swap32(&core.info.head, head, next);
    } while (!ok);

    core.queue[head & kMask] = item;

    Jimi_CompilerBarrier();

    return 0;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
T * RingQueueBase<T, Capacity, CoreTy>::pop()
{
    index_type head, tail, next;
    value_type item;
    bool ok = false;

    Jimi_CompilerBarrier();

    do {
        head = core.info.head;
        tail = core.info.tail;
        if ((tail == head) || (tail > head && (head - tail) > kMask))
            return (value_type)NULL;
        next = tail + 1;
        ok = jimi_bool_compare_and_swap32(&core.info.tail, tail, next);
    } while (!ok);

    item = core.queue[tail & kMask];

    Jimi_CompilerBarrier();

    return item;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
int RingQueueBase<T, Capacity, CoreTy>::push2(T * item)
{
    index_type head, tail, next;
    bool ok = false;

    Jimi_CompilerBarrier();

#if 1
    do {
        head = core.info.head;
        tail = core.info.tail;
        if ((head - tail) > kMask)
            return -1;
        next = head + 1;
        ok = jimi_bool_compare_and_swap32(&core.info.head, head, next);
    } while (!ok);
#else
    do {
        head = core.info.head;
        tail = core.info.tail;
        if ((head - tail) > kMask)
            return -1;
        next = head + 1;
    } while (jimi_compare_and_swap32(&core.info.head, head, next) != head);
#endif

    Jimi_CompilerBarrier();

    core.queue[head & kMask] = item;    

    return 0;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
T * RingQueueBase<T, Capacity, CoreTy>::pop2()
{
    index_type head, tail, next;
    value_type item;
    bool ok = false;

    Jimi_CompilerBarrier();

#if 1
    do {
        head = core.info.head;
        tail = core.info.tail;
        //if (tail >= head && (head - tail) <= kMask)
        if ((tail == head) || (tail > head && (head - tail) > kMask))
            return (value_type)NULL;
        next = tail + 1;
        ok = jimi_bool_compare_and_swap32(&core.info.tail, tail, next);
    } while (!ok);
#else
    do {
        head = core.info.head;
        tail = core.info.tail;
        //if (tail >= head && (head - tail) <= kMask)
        if ((tail == head) || (tail > head && (head - tail) > kMask))
            return (value_type)NULL;
        next = tail + 1;
    } while (jimi_compare_and_swap32(&core.info.tail, tail, next) != tail);
#endif

    item = core.queue[tail & kMask];

    Jimi_CompilerBarrier();

    return item;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
int RingQueueBase<T, Capacity, CoreTy>::spin_push(T * item)
{
    index_type head, tail, next;
#if defined(USE_SPIN_MUTEX_COUNTER) && (USE_SPIN_MUTEX_COUNTER != 0)
    uint32_t pause_cnt, spin_count, max_spin_cnt;
#endif

#if defined(USE_SPIN_MUTEX_COUNTER) && (USE_SPIN_MUTEX_COUNTER != 0)
    max_spin_cnt = MUTEX_MAX_SPIN_COUNT;
    spin_count = 1;

    while (jimi_val_compare_and_swap32(&spin_mutex.locked, 0U, 1U) != 0U) {
        if (spin_count <= max_spin_cnt) {
            for (pause_cnt = spin_count; pause_cnt > 0; --pause_cnt) {
                jimi_mm_pause();
                //jimi_mm_pause();
            }
            spin_count *= 2;
        }
        else {
            //jimi_yield();
            jimi_wsleep(0);
            //spin_counter = 1;
        }
    }
#else   /* !USE_SPIN_MUTEX_COUNTER */
    while (jimi_val_compare_and_swap32(&spin_mutex.locked, 0U, 1U) != 0U) {
        //jimi_yield();
        jimi_wsleep(0);
    }
#endif   /* USE_SPIN_MUTEX_COUNTER */

    head = core.info.head;
    tail = core.info.tail;
    if ((head - tail) > kMask) {
        Jimi_CompilerBarrier();
        //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
        spin_mutex.locked = 0;
        return -1;
    }
    next = head + 1;
    core.info.head = next;

    core.queue[head & kMask] = item;

    Jimi_CompilerBarrier();

    //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
    spin_mutex.locked = 0;

    return 0;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
T * RingQueueBase<T, Capacity, CoreTy>::spin_pop()
{
    index_type head, tail, next;
    value_type item;
#if defined(USE_SPIN_MUTEX_COUNTER) && (USE_SPIN_MUTEX_COUNTER != 0)
    uint32_t pause_cnt, spin_count, max_spin_cnt;
#endif

#if defined(USE_SPIN_MUTEX_COUNTER) && (USE_SPIN_MUTEX_COUNTER != 0)
    max_spin_cnt = MUTEX_MAX_SPIN_COUNT;
    spin_count = 1;

    while (jimi_val_compare_and_swap32(&spin_mutex.locked, 0U, 1U) != 0U) {
        if (spin_count <= max_spin_cnt) {
            for (pause_cnt = spin_count; pause_cnt > 0; --pause_cnt) {
                jimi_mm_pause();
            }
            spin_count *= 2;
        }
        else {
            //jimi_yield();
            jimi_wsleep(0);
            //spin_counter = 1;
        }
    }
#else   /* !USE_SPIN_MUTEX_COUNTER */
    while (jimi_val_compare_and_swap32(&spin_mutex.locked, 0U, 1U) != 0U) {
        //jimi_yield();
        jimi_wsleep(0);
    }
#endif   /* USE_SPIN_MUTEX_COUNTER */

    head = core.info.head;
    tail = core.info.tail;
    if ((tail == head) || (tail > head && (head - tail) > kMask)) {
        Jimi_CompilerBarrier();
        spin_mutex.locked = 0;
        return (value_type)NULL;
    }
    next = tail + 1;
    core.info.tail = next;

    item = core.queue[tail & kMask];

    Jimi_CompilerBarrier();
    spin_mutex.locked = 0;

    return item;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
int RingQueueBase<T, Capacity, CoreTy>::spin1_push(T * item)
{
    index_type head, tail, next;
    uint32_t pause_cnt, spin_counter;
    static const uint32_t max_spin_cnt = MUTEX_MAX_SPIN_COUNT;

    Jimi_CompilerBarrier();

    /* atomic_exchange usually takes less instructions than
       atomic_compare_and_exchange.  On the other hand,
       atomic_compare_and_exchange potentially generates less bus traffic
       when the lock is locked.
       We assume that the first try mostly will be successful, and we use
       atomic_exchange.  For the subsequent tries we use
       atomic_compare_and_exchange.  */
    if (jimi_lock_test_and_set32(&spin_mutex.locked, 1U) != 0U) {
        spin_counter = 1;
        do {
            if (spin_counter <= max_spin_cnt) {
                for (pause_cnt = spin_counter; pause_cnt > 0; --pause_cnt) {
                    jimi_mm_pause();
                }
                spin_counter *= 2;
            }
            else {
                //jimi_yield();
                jimi_wsleep(0);
                //spin_counter = 1;
            }
        } while (jimi_val_compare_and_swap32(&spin_mutex.locked, 0U, 1U) != 0U);
    }

    head = core.info.head;
    tail = core.info.tail;
    if ((head - tail) > kMask) {
        Jimi_CompilerBarrier();
        spin_mutex.locked = 0;
        return -1;
    }
    next = head + 1;
    core.info.head = next;

    core.queue[head & kMask] = item;

    Jimi_CompilerBarrier();
    spin_mutex.locked = 0;

    return 0;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
T * RingQueueBase<T, Capacity, CoreTy>::spin1_pop()
{
    index_type head, tail, next;
    value_type item;
    uint32_t pause_cnt, spin_counter;
    static const uint32_t max_spin_cnt = MUTEX_MAX_SPIN_COUNT;

    Jimi_CompilerBarrier();

    /* atomic_exchange usually takes less instructions than
       atomic_compare_and_exchange.  On the other hand,
       atomic_compare_and_exchange potentially generates less bus traffic
       when the lock is locked.
       We assume that the first try mostly will be successful, and we use
       atomic_exchange.  For the subsequent tries we use
       atomic_compare_and_exchange.  */
    if (jimi_lock_test_and_set32(&spin_mutex.locked, 1U) != 0U) {
        spin_counter = 1;
        do {
            if (spin_counter <= max_spin_cnt) {
                for (pause_cnt = spin_counter; pause_cnt > 0; --pause_cnt) {
                    jimi_mm_pause();
                }
                spin_counter *= 2;
            }
            else {
                //jimi_yield();
                jimi_wsleep(0);
                //spin_counter = 1;
            }
        } while (jimi_val_compare_and_swap32(&spin_mutex.locked, 0U, 1U) != 0U);
    }

    head = core.info.head;
    tail = core.info.tail;
    if ((tail == head) || (tail > head && (head - tail) > kMask)) {
        Jimi_CompilerBarrier();
        //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
        spin_mutex.locked = 0;
        return (value_type)NULL;
    }
    next = tail + 1;
    core.info.tail = next;

    item = core.queue[tail & kMask];

    Jimi_CompilerBarrier();

    //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
    spin_mutex.locked = 0;

    return item;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
int RingQueueBase<T, Capacity, CoreTy>::spin2_push_(T * item)
{
    index_type head, tail, next;
    int32_t pause_cnt;
    uint32_t loop_count, yield_cnt, spin_count;
    static const uint32_t YIELD_THRESHOLD = 1;  // 自旋次数阀值

    Jimi_CompilerBarrier();    // 编译器读写屏障

    // 下面这一句是一个小技巧, 参考自 pthread_spin_lock(), 自旋开始.
    if (jimi_lock_test_and_set32(&spin_mutex.locked, 1U) != 0U) {
        loop_count = 0;
        spin_count = 1;
        do {
            if (loop_count < YIELD_THRESHOLD) {
                for (pause_cnt = spin_count; pause_cnt > 0; --pause_cnt) {
                    jimi_mm_pause();        // 这是为支持超线程的 CPU 准备的切换提示
                }
                spin_count *= 2;
            }
            else {
                yield_cnt = loop_count - YIELD_THRESHOLD;
                if ((yield_cnt & 63) == 63) {
                    jimi_sleep(1);          // 真正的休眠, 转入内核态
                }
                else if ((yield_cnt & 3) == 3) {
                    jimi_sleep(0);          // 切换到优先级跟自己一样或更高的线程, 可以换到别的CPU核心上
                }
                else {
                    if (!jimi_yield()) {    // 让步给该线程所在的CPU核心上的别的线程,
                                            // 不能切换到别的CPU核心上等待的线程
                        jimi_sleep(0);      // 如果同核心上没有可切换的线程,
                                            // 则切到别的核心试试(只能切优先级跟自己相同或更好的)
                    }
                }
            }
            loop_count++;
        } while (jimi_val_compare_and_swap32(&spin_mutex.locked, 0U, 1U) != 0U);
    }

    // 进入锁区域
    head = core.info.head;
    tail = core.info.tail;
    if ((head - tail) > kMask) {
        Jimi_CompilerBarrier();
        // 队列已满, 释放锁
        spin_mutex.locked = 0;
        return -1;
    }
    next = head + 1;
    core.info.head = next;

    core.queue[head & kMask] = item;    // 把数据写入队列

    Jimi_CompilerBarrier();        // 编译器读写屏障

    spin_mutex.locked = 0;          // 释放锁

    return 0;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
int RingQueueBase<T, Capacity, CoreTy>::spin2_push(T * item)
{
    index_type head, tail, next;
    int32_t pause_cnt;
    uint32_t loop_count, yield_cnt, spin_count;
    static const uint32_t YIELD_THRESHOLD = SPIN_YIELD_THRESHOLD;

    Jimi_CompilerBarrier();

    /* atomic_exchange usually takes less instructions than
       atomic_compare_and_exchange.  On the other hand,
       atomic_compare_and_exchange potentially generates less bus traffic
       when the lock is locked.
       We assume that the first try mostly will be successful, and we use
       atomic_exchange.  For the subsequent tries we use
       atomic_compare_and_exchange.  */
    if (jimi_lock_test_and_set32(&spin_mutex.locked, 1U) != 0U) {
        loop_count = 0;
        spin_count = 1;
        do {
            if (loop_count < YIELD_THRESHOLD) {
                for (pause_cnt = spin_count; pause_cnt > 0; --pause_cnt) {
                    jimi_mm_pause();
                }
                spin_count *= 2;
            }
            else {
                yield_cnt = loop_count - YIELD_THRESHOLD;
#if defined(__MINGW32__) || defined(__CYGWIN__)
                if ((yield_cnt & 3) == 3) {
                    jimi_wsleep(0);
                }
                else {
                    if (!jimi_yield()) {
                        jimi_wsleep(0);
                        //jimi_mm_pause();
                    }
                }
#else
                if ((yield_cnt & 63) == 63) {
                    jimi_wsleep(1);
                }
                else if ((yield_cnt & 3) == 3) {
                    jimi_wsleep(0);
                }
                else {
                    if (!jimi_yield()) {
                        jimi_wsleep(0);
                        //jimi_mm_pause();
                    }
                }
#endif
            }
            loop_count++;
            //jimi_mm_pause();
        } while (jimi_val_compare_and_swap32(&spin_mutex.locked, 0U, 1U) != 0U);
    }

    head = core.info.head;
    tail = core.info.tail;
    if ((head - tail) > kMask) {
        Jimi_CompilerBarrier();
        spin_mutex.locked = 0;
        return -1;
    }
    next = head + 1;
    core.info.head = next;

    core.queue[head & kMask] = item;

    Jimi_CompilerBarrier();
    spin_mutex.locked = 0;

    return 0;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
T * RingQueueBase<T, Capacity, CoreTy>::spin2_pop()
{
    index_type head, tail, next;
    value_type item;
    int32_t pause_cnt;
    uint32_t loop_count, yield_cnt, spin_count;
    static const uint32_t YIELD_THRESHOLD = SPIN_YIELD_THRESHOLD;

    Jimi_CompilerBarrier();

    /* atomic_exchange usually takes less instructions than
       atomic_compare_and_exchange.  On the other hand,
       atomic_compare_and_exchange potentially generates less bus traffic
       when the lock is locked.
       We assume that the first try mostly will be successful, and we use
       atomic_exchange.  For the subsequent tries we use
       atomic_compare_and_exchange.  */
    if (jimi_lock_test_and_set32(&spin_mutex.locked, 1U) != 0U) {
        loop_count = 0;
        spin_count = 1;
        do {
            if (loop_count < YIELD_THRESHOLD) {
                for (pause_cnt = spin_count; pause_cnt > 0; --pause_cnt) {
                    jimi_mm_pause();
                }
                spin_count *= 2;
            }
            else {
                yield_cnt = loop_count - YIELD_THRESHOLD;
#if defined(__MINGW32__) || defined(__CYGWIN__)
                if ((yield_cnt & 3) == 3) {
                    jimi_wsleep(0);
                }
                else {
                    if (!jimi_yield()) {
                        jimi_wsleep(0);
                        //jimi_mm_pause();
                    }
                }
#else
                if ((yield_cnt & 63) == 63) {
                    jimi_wsleep(1);
                }
                else if ((yield_cnt & 3) == 3) {
                    jimi_wsleep(0);
                }
                else {
                    if (!jimi_yield()) {
                        jimi_wsleep(0);
                        //jimi_mm_pause();
                    }
                }
#endif
            }
            loop_count++;
            //jimi_mm_pause();
        } while (jimi_val_compare_and_swap32(&spin_mutex.locked, 0U, 1U) != 0U);
    }

    head = core.info.head;
    tail = core.info.tail;
    if ((tail == head) || (tail > head && (head - tail) > kMask)) {
        Jimi_CompilerBarrier();
        //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
        spin_mutex.locked = 0;
        return (value_type)NULL;
    }
    next = tail + 1;
    core.info.tail = next;

    item = core.queue[tail & kMask];

    Jimi_CompilerBarrier();
    spin_mutex.locked = 0;

    return item;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
int RingQueueBase<T, Capacity, CoreTy>::spin3_push(T * item)
{
    index_type head, tail, next;
    int32_t pause_cnt;
    uint32_t loop_count, yield_cnt, spin_count;
    static const uint32_t YIELD_THRESHOLD = SPIN_YIELD_THRESHOLD;

    Jimi_CompilerBarrier();

    /* atomic_exchange usually takes less instructions than
       atomic_compare_and_exchange.  On the other hand,
       atomic_compare_and_exchange potentially generates less bus traffic
       when the lock is locked.
       We assume that the first try mostly will be successful, and we use
       atomic_exchange.  For the subsequent tries we use
       atomic_compare_and_exchange.  */
    if (jimi_lock_test_and_set32(&spin_mutex.locked, 1U) != 0U) {
        loop_count = 0;
        spin_count = 1;
        do {
            do {
                if (loop_count < YIELD_THRESHOLD) {
                    for (pause_cnt = spin_count; pause_cnt > 0; --pause_cnt) {
                        jimi_mm_pause();
                    }
                    spin_count *= 2;
                }
                else {
                    yield_cnt = loop_count - YIELD_THRESHOLD;
#if defined(__MINGW32__) || defined(__CYGWIN__)
                    if ((yield_cnt & 3) == 3) {
                        jimi_wsleep(0);
                    }
                    else {
                        if (!jimi_yield()) {
                            jimi_wsleep(0);
                            //jimi_mm_pause();
                        }
                    }
#else
                    if ((yield_cnt & 63) == 63) {
  #if !(defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) || defined(_M_IA64) || defined(__amd64__) || defined(__x86_64__))
                        jimi_wsleep(1);
  #else
                        jimi_wsleep(1);
  #endif  /* !(_M_X64 || _WIN64) */
                    }
                    else if ((yield_cnt & 3) == 3) {
                        jimi_wsleep(0);
                    }
                    else {
                        if (!jimi_yield()) {
                            jimi_wsleep(0);
                            //jimi_mm_pause();
                        }
                    }
#endif
                }
                loop_count++;
                //jimi_mm_pause();
            } while (spin_mutex.locked != 0U);
        } while (jimi_val_compare_and_swap32(&spin_mutex.locked, 0U, 1U) != 0U);
    }

    head = core.info.head;
    tail = core.info.tail;
    if ((head - tail) > kMask) {
        Jimi_CompilerBarrier();
        spin_mutex.locked = 0;
        return -1;
    }
    next = head + 1;
    core.info.head = next;

    core.queue[head & kMask] = item;

    Jimi_CompilerBarrier();
    spin_mutex.locked = 0;

    return 0;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
T * RingQueueBase<T, Capacity, CoreTy>::spin3_pop()
{
    index_type head, tail, next;
    value_type item;
    int32_t pause_cnt;
    uint32_t loop_count, yield_cnt, spin_count;
    static const uint32_t YIELD_THRESHOLD = SPIN_YIELD_THRESHOLD;

    Jimi_CompilerBarrier();

    /* atomic_exchange usually takes less instructions than
       atomic_compare_and_exchange.  On the other hand,
       atomic_compare_and_exchange potentially generates less bus traffic
       when the lock is locked.
       We assume that the first try mostly will be successful, and we use
       atomic_exchange.  For the subsequent tries we use
       atomic_compare_and_exchange.  */
    if (jimi_lock_test_and_set32(&spin_mutex.locked, 1U) != 0U) {
        loop_count = 0;
        spin_count = 1;
        do {
            do {
                if (loop_count < YIELD_THRESHOLD) {
                    for (pause_cnt = spin_count; pause_cnt > 0; --pause_cnt) {
                        jimi_mm_pause();
                    }
                    spin_count *= 2;
                }
                else {
                    yield_cnt = loop_count - YIELD_THRESHOLD;
#if defined(__MINGW32__) || defined(__CYGWIN__)
                    if ((yield_cnt & 3) == 3) {
                        jimi_wsleep(0);
                    }
                    else {
                        if (!jimi_yield()) {
                            jimi_wsleep(0);
                            //jimi_mm_pause();
                        }
                    }
#else
                    if ((yield_cnt & 63) == 63) {
  #if !(defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) || defined(_M_IA64) || defined(__amd64__) || defined(__x86_64__))
                        jimi_wsleep(1);
  #else
                        jimi_wsleep(1);
  #endif  /* !(_M_X64 || _WIN64) */
                    }
                    else if ((yield_cnt & 3) == 3) {
                        jimi_wsleep(0);
                    }
                    else {
                        if (!jimi_yield()) {
                            jimi_wsleep(0);
                            //jimi_mm_pause();
                        }
                    }
#endif
                }
                loop_count++;
                //jimi_mm_pause();
            } while (spin_mutex.locked != 0U);
        } while (jimi_val_compare_and_swap32(&spin_mutex.locked, 0U, 1U) != 0U);
    }

    head = core.info.head;
    tail = core.info.tail;
    if ((tail == head) || (tail > head && (head - tail) > kMask)) {
        Jimi_CompilerBarrier();
        spin_mutex.locked = 0;
        return (value_type)NULL;
    }
    next = tail + 1;
    core.info.tail = next;

    item = core.queue[tail & kMask];

    Jimi_CompilerBarrier();
    spin_mutex.locked = 0;

    return item;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
int RingQueueBase<T, Capacity, CoreTy>::spin8_push(T * item)
{
    index_type head, tail, next;

    Jimi_CompilerBarrier();

    while (spin_mutex.locked != 0) {
        jimi_mm_pause();
    }
    jimi_lock_test_and_set32(&spin_mutex.locked, 1U);

    head = core.info.head;
    tail = core.info.tail;
    if ((head - tail) > kMask) {
        Jimi_CompilerBarrier();
        spin_mutex.locked = 0;
        return -1;
    }
    next = head + 1;
    core.info.head = next;

    core.queue[head & kMask] = item;

    Jimi_CompilerBarrier();
    spin_mutex.locked = 0;

    return 0;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
T * RingQueueBase<T, Capacity, CoreTy>::spin8_pop()
{
    index_type head, tail, next;
    value_type item;
    int cnt;

    cnt = 0;
    Jimi_CompilerBarrier();

    while (spin_mutex.locked != 0) {
        jimi_mm_pause();
    }
    jimi_lock_test_and_set32(&spin_mutex.locked, 1U);

    head = core.info.head;
    tail = core.info.tail;
    if ((tail == head) || (tail > head && (head - tail) > kMask)) {
        Jimi_CompilerBarrier();
        spin_mutex.locked = 0;
        return (value_type)NULL;
    }
    next = tail + 1;
    core.info.tail = next;

    item = core.queue[tail & kMask];

    Jimi_CompilerBarrier();
    spin_mutex.locked = 0;

    return item;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
int RingQueueBase<T, Capacity, CoreTy>::spin9_push(T * item)
{
    index_type head, tail, next;
    int cnt;

    cnt = 0;
    Jimi_CompilerBarrier();

    while (spin_mutex.locked != 0) {
        jimi_mm_pause();
        cnt++;
        if (cnt > 8000) {
            cnt = 0;
            jimi_wsleep(1);
            //printf("push(): shared_lock = %d\n", spin_mutex.locked);
        }
    }

    //printf("push(): shared_lock = %d\n", spin_mutex.locked);
    //printf("push(): start: cnt = %d\n", cnt);
    //printf("push(): head = %u, tail = %u\n", core.info.head, core.info.tail);

    ///
    /// GCC 提供的原子操作 (From GCC 4.1.2)
    /// See: http://www.cnblogs.com/FrankTan/archive/2010/12/11/1903377.html
    ///
    jimi_lock_test_and_set32(&spin_mutex.locked, 1U);

    head = core.info.head;
    tail = core.info.tail;
    if ((head - tail) > kMask) {
        Jimi_CompilerBarrier();
        //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
        spin_mutex.locked = 0;
        return -1;
    }
    next = head + 1;
    core.info.head = next;

    core.queue[head & kMask] = item;

    Jimi_CompilerBarrier();

    //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
    spin_mutex.locked = 0;

    return 0;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
T * RingQueueBase<T, Capacity, CoreTy>::spin9_pop()
{
    index_type head, tail, next;
    value_type item;
    int cnt;

    cnt = 0;
    Jimi_CompilerBarrier();

    while (spin_mutex.locked != 0) {
        jimi_mm_pause();
        cnt++;
        if (cnt > 8000) {
            cnt = 0;
            jimi_wsleep(1);
            //printf("pop() : shared_lock = %d\n", spin_mutex.locked);
        }
    }

    //printf("pop() : shared_lock = %d\n", spin_mutex.locked);
    //printf("pop() : start: cnt = %d\n", cnt);
    //printf("pop() : head = %u, tail = %u\n", core.info.head, core.info.tail);

    ///
    /// GCC 提供的原子操作 (From GCC 4.1.2)
    /// See: http://www.cnblogs.com/FrankTan/archive/2010/12/11/1903377.html
    ///
    jimi_lock_test_and_set32(&spin_mutex.locked, 1U);

    head = core.info.head;
    tail = core.info.tail;
    if ((tail == head) || (tail > head && (head - tail) > kMask)) {
        Jimi_CompilerBarrier();
        //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
        spin_mutex.locked = 0;
        return (value_type)NULL;
    }
    next = tail + 1;
    core.info.tail = next;

    item = core.queue[tail & kMask];

    Jimi_CompilerBarrier();

    //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
    spin_mutex.locked = 0;

    return item;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
int RingQueueBase<T, Capacity, CoreTy>::mutex_push(T * item)
{
    index_type head, tail, next;

    pthread_mutex_lock(&queue_mutex);

    head = core.info.head;
    tail = core.info.tail;
    if ((head - tail) > kMask) {
        pthread_mutex_unlock(&queue_mutex);
        return -1;
    }
    next = head + 1;
    core.info.head = next;

    core.queue[head & kMask] = item;

    pthread_mutex_unlock(&queue_mutex);

    return 0;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
T * RingQueueBase<T, Capacity, CoreTy>::mutex_pop()
{
    index_type head, tail, next;
    value_type item;

    pthread_mutex_lock(&queue_mutex);

    head = core.info.head;
    tail = core.info.tail;
    //if (tail >= head && (head - tail) <= kMask)
    if ((tail == head) || (tail > head && (head - tail) > kMask)) {
        pthread_mutex_unlock(&queue_mutex);
        return (value_type)NULL;
    }
    next = tail + 1;
    core.info.tail = next;

    item = core.queue[tail & kMask];

    pthread_mutex_unlock(&queue_mutex);

    return item;
}

///////////////////////////////////////////////////////////////////
// class SmallRingQueue<T, Capacity>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capacity = 1024U>
class SmallRingQueue : public RingQueueBase<T, Capacity, SmallRingQueueCore<T, Capacity> >
{
public:
    typedef uint32_t                    size_type;
    typedef uint32_t                    index_type;
    typedef T *                         value_type;
    typedef T *                         pointer;
    typedef const T *                   const_pointer;
    typedef T &                         reference;
    typedef const T &                   const_reference;

    static const size_type kCapacity = RingQueueBase<T, Capacity, SmallRingQueueCore<T, Capacity> >::kCapacity;

public:
    SmallRingQueue(bool bFillQueue = true, bool bInitHead = false);
    ~SmallRingQueue();

public:
    void dump_detail();

protected:
    void init_queue(bool bFillQueue = true);
};

template <typename T, uint32_t Capacity>
SmallRingQueue<T, Capacity>::SmallRingQueue(bool bFillQueue /* = true */,
                                             bool bInitHead  /* = false */)
: RingQueueBase<T, Capacity, SmallRingQueueCore<T, Capacity> >(bInitHead)
{
    //printf("SmallRingQueue::SmallRingQueue();\n\n");

    init_queue(bFillQueue);
}

template <typename T, uint32_t Capacity>
SmallRingQueue<T, Capacity>::~SmallRingQueue()
{
    // Do nothing!
}

template <typename T, uint32_t Capacity>
inline
void SmallRingQueue<T, Capacity>::init_queue(bool bFillQueue /* = true */)
{
    //printf("SmallRingQueue::init_queue();\n\n");

    if (bFillQueue) {
        memset((void *)this->core.queue, 0, sizeof(value_type) * kCapacity);
    }
}

template <typename T, uint32_t Capacity>
void SmallRingQueue<T, Capacity>::dump_detail()
{
    printf("SmallRingQueue: (head = %u, tail = %u)\n",
           this->core.info.head, this->core.info.tail);
}

///////////////////////////////////////////////////////////////////
// class RingQueue<T, Capacity>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capacity = 1024U>
class RingQueue : public RingQueueBase<T, Capacity, RingQueueCore<T, Capacity> >
{
public:
    typedef uint32_t                    size_type;
    typedef uint32_t                    index_type;
    typedef T *                         value_type;
    typedef T *                         pointer;
    typedef const T *                   const_pointer;
    typedef T &                         reference;
    typedef const T &                   const_reference;

    typedef RingQueueCore<T, Capacity>   core_type;

    static const size_type kCapacity = RingQueueBase<T, Capacity, RingQueueCore<T, Capacity> >::kCapacity;

public:
    RingQueue(bool bFillQueue = true, bool bInitHead = false);
    ~RingQueue();

public:
    void dump_detail();

protected:
    void init_queue(bool bFillQueue = true);
};

template <typename T, uint32_t Capacity>
RingQueue<T, Capacity>::RingQueue(bool bFillQueue /* = true */,
                                   bool bInitHead  /* = false */)
: RingQueueBase<T, Capacity, RingQueueCore<T, Capacity> >(bInitHead)
{
    //printf("RingQueue::RingQueue();\n\n");

    init_queue(bFillQueue);
}

template <typename T, uint32_t Capacity>
RingQueue<T, Capacity>::~RingQueue()
{
    // If the queue is allocated on system heap, release them.
    if (RingQueueCore<T, Capacity>::kIsAllocOnHeap) {
        if (this->core.queue != NULL) {
            delete [] this->core.queue;
            this->core.queue = NULL;
        }
    }
}

template <typename T, uint32_t Capacity>
inline
void RingQueue<T, Capacity>::init_queue(bool bFillQueue /* = true */)
{
    //printf("RingQueue::init_queue();\n\n");

    value_type *newData = new T *[kCapacity];
    if (newData != NULL) {
        if (bFillQueue) {
            memset((void *)newData, 0, sizeof(value_type) * kCapacity);
        }
        this->core.queue = newData;
    }
}

template <typename T, uint32_t Capacity>
void RingQueue<T, Capacity>::dump_detail()
{
    printf("RingQueue: (head = %u, tail = %u)\n",
           this->core.info.head, this->core.info.tail);
}

}  /* namespace jimi */

#endif  /* _JIMI_UTIL_RINGQUEUE_H_ */
