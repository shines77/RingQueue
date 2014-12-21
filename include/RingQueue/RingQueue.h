
#ifndef _JIMI_UTIL_RINGQUEUE_H_
#define _JIMI_UTIL_RINGQUEUE_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include <stdio.h>

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

#include "port.h"
#include "sleep.h"
#include "dump_mem.h"

#ifndef JIMI_CACHELINE_SIZE
#define JIMI_CACHELINE_SIZE     64
#endif

#ifndef JIMI_MIN
#define JIMI_MIN(a, b)          ((a) < (b) ? (a) : (b))
#endif

#ifndef JIMI_MAX
#define JIMI_MAX(a, b)          ((a) > (b) ? (a) : (b))
#endif

#if defined(_WIN64) || defined(_M_X64)
#define JIMI_SIZE_T_SIZEOF      8
#else
#define JIMI_SIZE_T_SIZEOF      4
#endif

/**
 * macro for round to power of 2
 */
#define jimi_b2(x)              (        (x) | (        (x) >>  1))
#define jimi_b4(x)              ( jimi_b2(x) | ( jimi_b2(x) >>  2))
#define jimi_b8(x)              ( jimi_b4(x) | ( jimi_b4(x) >>  4))
#define jimi_b16(x)             ( jimi_b8(x) | ( jimi_b8(x) >>  8))
#define jimi_b32(x)             (jimi_b16(x) | (jimi_b16(x) >> 16))
#define jimi_b64(x)             (jimi_b32(x) | (jimi_b32(x) >> 32))

#define jimi_next_power_of_2(x)     (jimi_b32((x) - 1) + 1)
#define jimi_next_power_of_2_64(x)  (jimi_b64((x) - 1) + 1)

#if defined(JIMI_SIZE_T_SIZEOF) && (JIMI_SIZE_T_SIZEOF == 8)
#define JIMI_ROUND_TO_POW2(N)   jimi_next_power_of_2_64(N)
#else
#define JIMI_ROUND_TO_POW2(N)   jimi_next_power_of_2(N)
#endif

namespace jimi {

#if 1
struct RingQueueHead
{
    volatile uint32_t head;
    volatile uint32_t tail;
};
#else
struct RingQueueHead
{
    volatile uint32_t head;
    char padding1[JIMI_CACHELINE_SIZE - sizeof(uint32_t)];

    volatile uint32_t tail;
    char padding2[JIMI_CACHELINE_SIZE - sizeof(uint32_t)];
};
#endif

typedef struct RingQueueHead RingQueueHead;

///////////////////////////////////////////////////////////////////
// class SmallRingQueueCore<Capcity>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capcity>
class SmallRingQueueCore
{
public:
    typedef uint32_t    size_type;
    typedef T *         item_type;

public:
    static const size_type  kCapcityCore = (size_type)JIMI_MAX(JIMI_ROUND_TO_POW2(Capcity), 2);
    static const bool kIsAllocOnHeap     = false;

public:
    RingQueueHead       info;
    volatile item_type  queue[kCapcityCore];
};

///////////////////////////////////////////////////////////////////
// class RingQueueCore<Capcity>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capcity>
class RingQueueCore
{
public:
    typedef T *         item_type;

public:
    static const bool kIsAllocOnHeap = true;

public:
    RingQueueHead       info;
    volatile item_type *queue;
};

///////////////////////////////////////////////////////////////////
// class RingQueueBase<T, Capcity, CoreTy>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capcity = 16U,
          typename CoreTy = RingQueueCore<T, Capcity> >
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
    static const size_type  kCapcity = (size_type)JIMI_MAX(JIMI_ROUND_TO_POW2(Capcity), 2);
    static const index_type kMask    = (index_type)(kCapcity - 1);

public:
    RingQueueBase(bool bInitHead = false);
    ~RingQueueBase();

public:
    void dump_info();
    void dump_detail();

    index_type mask() const      { return kMask;    };
    size_type capcity() const    { return kCapcity; };
    size_type length() const     { return sizes();  };
    size_type sizes() const;

    void init(bool bInitHead = false);

    int push(T * item);
    T * pop();

    int spin_push(T * item);
    T * spin_pop();

    int spin2_push(T * item);
    T * spin2_pop();

    int locked_push(T * item);
    T * locked_pop();

protected:
    core_type       core;
    spin_mutex_t    spin_mutex;
    pthread_mutex_t queue_mutex;
};

template <typename T, uint32_t Capcity, typename CoreTy>
RingQueueBase<T, Capcity, CoreTy>::RingQueueBase(bool bInitHead  /* = false */)
{
    //printf("RingQueueBase::RingQueueBase();\n\n");

    init(bInitHead);
}

template <typename T, uint32_t Capcity, typename CoreTy>
RingQueueBase<T, Capcity, CoreTy>::~RingQueueBase()
{
    // Do nothing!
    Jimi_ReadWriteBarrier();

    spin_mutex.locked = 0;

    pthread_mutex_destroy(&queue_mutex);
}

template <typename T, uint32_t Capcity, typename CoreTy>
inline
void RingQueueBase<T, Capcity, CoreTy>::init(bool bInitHead /* = false */)
{
    //printf("RingQueueBase::init();\n\n");

    if (!bInitHead) {
        core.info.head = 0;
        core.info.tail = 0;
    }
    else {
        memset((void *)&core.info, 0, sizeof(core.info));
    }

    Jimi_ReadWriteBarrier();

    // Initilized spin mutex
    spin_mutex.locked = 0;
    spin_mutex.spin_counter = MUTEX_MAX_SPIN_COUNTER;
    spin_mutex.recurse_counter = 0;
    spin_mutex.thread_id = 0;

    // Initilized mutex
    pthread_mutex_init(&queue_mutex, NULL);
}

template <typename T, uint32_t Capcity, typename CoreTy>
void RingQueueBase<T, Capcity, CoreTy>::dump_info()
{
    //ReleaseUtils::dump(&core.info, sizeof(core.info));
    dump_mem(&core.info, sizeof(core.info), false, 16, 0, 0);
}

template <typename T, uint32_t Capcity, typename CoreTy>
void RingQueueBase<T, Capcity, CoreTy>::dump_detail()
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

template <typename T, uint32_t Capcity, typename CoreTy>
inline
typename RingQueueBase<T, Capcity, CoreTy>::size_type
RingQueueBase<T, Capcity, CoreTy>::sizes() const
{
    index_type head, tail;

    Jimi_ReadWriteBarrier();

    head = core.info.head;

    tail = core.info.tail;

    return (size_type)((head - tail) <= kMask) ? (head - tail) : (size_type)-1;
}

template <typename T, uint32_t Capcity, typename CoreTy>
inline
int RingQueueBase<T, Capcity, CoreTy>::push(T * item)
{
    index_type head, tail, next;
    bool ok = false;

    Jimi_ReadWriteBarrier();

#if 1
    do {
        head = core.info.head;
        tail = core.info.tail;
        if ((head - tail) > kMask) {
            Jimi_ReadWriteBarrier();
            return -1;
        }
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

    core.queue[head & kMask] = item;

    Jimi_ReadWriteBarrier();

    return 0;
}

template <typename T, uint32_t Capcity, typename CoreTy>
inline
T * RingQueueBase<T, Capcity, CoreTy>::pop()
{
    index_type head, tail, next;
    value_type item;
    bool ok = false;

    Jimi_ReadWriteBarrier();

#if 1
    do {
        head = core.info.head;
        tail = core.info.tail;
        //if (tail >= head && (head - tail) <= kMask)
        if ((tail == head) || (tail > head && (head - tail) > kMask)) {
            Jimi_ReadWriteBarrier();
            return (value_type)NULL;
        }
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

    Jimi_ReadWriteBarrier();

    //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
    spin_mutex.locked = 0;

    return item;
}

template <typename T, uint32_t Capcity, typename CoreTy>
inline
int RingQueueBase<T, Capcity, CoreTy>::spin_push(T * item)
{
    index_type head, tail, next;
#if defined(USE_SPIN_MUTEX_COUNTER) && (USE_SPIN_MUTEX_COUNTER != 0)
    uint32_t pause_cnt, spin_counter, max_spin_cnt;
#endif

#if defined(USE_SPIN_MUTEX_COUNTER) && (USE_SPIN_MUTEX_COUNTER != 0)
    max_spin_cnt = spin_mutex.spin_counter;
    spin_counter = 1;

    while (__sync_val_compare_and_swap(&spin_mutex.locked, 0U, 1U) != 0U) {
        if (spin_counter <= max_spin_cnt) {
            for (pause_cnt = spin_counter; pause_cnt > 0; --pause_cnt) {
                jimi_mm_pause();
            }
            spin_counter *= 2;
        }
        else {
            //jimi_yield();
            jimi_wsleep(0);
            spin_counter = 1;
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
        Jimi_ReadWriteBarrier();
        //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
        spin_mutex.locked = 0;
        return -1;
    }
    next = head + 1;
    core.info.head = next;

    core.queue[head & kMask] = item;

    Jimi_ReadWriteBarrier();

    //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
    spin_mutex.locked = 0;

    return 0;
}

template <typename T, uint32_t Capcity, typename CoreTy>
inline
T * RingQueueBase<T, Capcity, CoreTy>::spin_pop()
{
    index_type head, tail, next;
    value_type item;
#if defined(USE_SPIN_MUTEX_COUNTER) && (USE_SPIN_MUTEX_COUNTER != 0)
    uint32_t pause_cnt, spin_counter, max_spin_cnt;
#endif

#if defined(USE_SPIN_MUTEX_COUNTER) && (USE_SPIN_MUTEX_COUNTER != 0)
    max_spin_cnt = spin_mutex.spin_counter;
    spin_counter = 1;

    while (__sync_val_compare_and_swap(&spin_mutex.locked, 0U, 1U) != 0U) {
        if (spin_counter <= max_spin_cnt) {
            for (pause_cnt = spin_counter; pause_cnt > 0; --pause_cnt) {
                jimi_mm_pause();
            }
            spin_counter *= 2;
        }
        else {
            //jimi_yield();
            jimi_wsleep(0);
            spin_counter = 1;
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
        Jimi_ReadWriteBarrier();
        //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
        spin_mutex.locked = 0;
        return (value_type)NULL;
    }
    next = tail + 1;
    core.info.tail = next;

    item = core.queue[tail & kMask];

    Jimi_ReadWriteBarrier();

    //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
    spin_mutex.locked = 0;

    return item;
}

template <typename T, uint32_t Capcity, typename CoreTy>
inline
int RingQueueBase<T, Capcity, CoreTy>::spin2_push(T * item)
{
    index_type head, tail, next;
    bool ok = false;
    int cnt;

    cnt = 0;
    Jimi_ReadWriteBarrier();

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
        Jimi_ReadWriteBarrier();
        //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
        spin_mutex.locked = 0;
        return -1;
    }
    next = head + 1;
    core.info.head = next;

    core.queue[head & kMask] = item;

    Jimi_ReadWriteBarrier();

    //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
    spin_mutex.locked = 0;

    return 0;
}

template <typename T, uint32_t Capcity, typename CoreTy>
inline
T * RingQueueBase<T, Capcity, CoreTy>::spin2_pop()
{
    index_type head, tail, next;
    value_type item;
    bool ok = false;
    int cnt;

    cnt = 0;
    Jimi_ReadWriteBarrier();

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
        Jimi_ReadWriteBarrier();
        //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
        spin_mutex.locked = 0;
        return (value_type)NULL;
    }
    next = tail + 1;
    core.info.tail = next;

    item = core.queue[tail & kMask];

    Jimi_ReadWriteBarrier();

    //jimi_lock_test_and_set32(&spin_mutex.locked, 0U);
    spin_mutex.locked = 0;

    return item;
}

template <typename T, uint32_t Capcity, typename CoreTy>
inline
int RingQueueBase<T, Capcity, CoreTy>::locked_push(T * item)
{
    index_type head, tail, next;

    if (queue_mutex == NULL) return -1;

    pthread_mutex_lock(&queue_mutex);

    head = core.info.head;
    tail = core.info.tail;
    if ((head - tail) > kMask) {
        pthread_mutex_unlock(&queue_mutex);
        return -1;
    }
    next = head + 1;
    core.info.head = next;

    Jimi_ReadWriteBarrier();

    core.queue[head & kMask] = item;

    Jimi_ReadWriteBarrier();

    pthread_mutex_unlock(&queue_mutex);

    return 0;
}

template <typename T, uint32_t Capcity, typename CoreTy>
inline
T * RingQueueBase<T, Capcity, CoreTy>::locked_pop()
{
    index_type head, tail, next;
    value_type item;

    if (queue_mutex == NULL) return NULL;

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

    Jimi_ReadWriteBarrier();

    item = core.queue[tail & kMask];

    Jimi_ReadWriteBarrier();

    pthread_mutex_unlock(&queue_mutex);

    return item;
}

///////////////////////////////////////////////////////////////////
// class SmallRingQueue<T, Capcity>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capcity = 16U>
class SmallRingQueue : public RingQueueBase<T, Capcity, SmallRingQueueCore<T, Capcity> >
{
public:
    typedef uint32_t                    size_type;
    typedef uint32_t                    index_type;
    typedef T *                         value_type;
    typedef T *                         pointer;
    typedef const T *                   const_pointer;
    typedef T &                         reference;
    typedef const T &                   const_reference;

    static const size_type kCapcity = RingQueueBase<T, Capcity, SmallRingQueueCore<T, Capcity> >::kCapcity;

public:
    SmallRingQueue(bool bFillQueue = true, bool bInitHead = false);
    ~SmallRingQueue();

public:
    void dump_detail();

protected:
    void init_queue(bool bFillQueue = true);
};

template <typename T, uint32_t Capcity>
SmallRingQueue<T, Capcity>::SmallRingQueue(bool bFillQueue /* = true */,
                                             bool bInitHead  /* = false */)
: RingQueueBase<T, Capcity, SmallRingQueueCore<T, Capcity> >(bInitHead)
{
    //printf("SmallRingQueue::SmallRingQueue();\n\n");

    init_queue(bFillQueue);
}

template <typename T, uint32_t Capcity>
SmallRingQueue<T, Capcity>::~SmallRingQueue()
{
    // Do nothing!
}

template <typename T, uint32_t Capcity>
inline
void SmallRingQueue<T, Capcity>::init_queue(bool bFillQueue /* = true */)
{
    //printf("SmallRingQueue::init_queue();\n\n");

    if (bFillQueue) {
        memset((void *)this->core.queue, 0, sizeof(value_type) * kCapcity);
    }
}

template <typename T, uint32_t Capcity>
void SmallRingQueue<T, Capcity>::dump_detail()
{
    printf("SmallRingQueue: (head = %u, tail = %u)\n",
           this->core.info.head, this->core.info.tail);
}

///////////////////////////////////////////////////////////////////
// class RingQueue<T, Capcity>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capcity = 16U>
class RingQueue : public RingQueueBase<T, Capcity, RingQueueCore<T, Capcity> >
{
public:
    typedef uint32_t                    size_type;
    typedef uint32_t                    index_type;
    typedef T *                         value_type;
    typedef T *                         pointer;
    typedef const T *                   const_pointer;
    typedef T &                         reference;
    typedef const T &                   const_reference;

    typedef RingQueueCore<T, Capcity>   core_type;

    static const size_type kCapcity = RingQueueBase<T, Capcity, RingQueueCore<T, Capcity> >::kCapcity;

public:
    RingQueue(bool bFillQueue = true, bool bInitHead = false);
    ~RingQueue();

public:
    void dump_detail();

protected:
    void init_queue(bool bFillQueue = true);
};

template <typename T, uint32_t Capcity>
RingQueue<T, Capcity>::RingQueue(bool bFillQueue /* = true */,
                                   bool bInitHead  /* = false */)
: RingQueueBase<T, Capcity, RingQueueCore<T, Capcity> >(bInitHead)
{
    //printf("RingQueue::RingQueue();\n\n");

    init_queue(bFillQueue);
}

template <typename T, uint32_t Capcity>
RingQueue<T, Capcity>::~RingQueue()
{
    // If the queue is allocated on system heap, release them.
    if (RingQueueCore<T, Capcity>::kIsAllocOnHeap) {
        delete [] this->core.queue;
        this->core.queue = NULL;
    }
}

template <typename T, uint32_t Capcity>
inline
void RingQueue<T, Capcity>::init_queue(bool bFillQueue /* = true */)
{
    //printf("RingQueue::init_queue();\n\n");

    value_type *newData = new T *[kCapcity];
    if (newData != NULL) {
        this->core.queue = newData;
        if (bFillQueue) {
            memset((void *)this->core.queue, 0, sizeof(value_type) * kCapcity);
        }
    }
}

template <typename T, uint32_t Capcity>
void RingQueue<T, Capcity>::dump_detail()
{
    printf("RingQueue: (head = %u, tail = %u)\n",
           this->core.info.head, this->core.info.tail);
}

}  /* namespace jimi */

#undef JIMI_CACHELINE_SIZE

#endif  /* _JIMI_UTIL_RINGQUEUE_H_ */
