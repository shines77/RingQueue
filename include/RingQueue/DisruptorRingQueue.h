
#ifndef _JIMI_DISRUPTOR_RINGQUEUE_H_
#define _JIMI_DISRUPTOR_RINGQUEUE_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include "vs_stdint.h"
#include "port.h"
#include "sleep.h"

#include <atomic>

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

#include "Sequence.h"

#include <stdio.h>
#include <string.h>

#include "dump_mem.h"

namespace jimi {

#if 0
struct DisruptorRingQueueHead
{
    volatile uint32_t head;
    volatile uint32_t tail;
};
#else
struct DisruptorRingQueueHead
{
    volatile uint32_t head;
    char padding1[JIMI_CACHE_LINE_SIZE - sizeof(uint32_t)];

    volatile uint32_t tail;
    char padding2[JIMI_CACHE_LINE_SIZE - sizeof(uint32_t)];

    Sequence cursor;
    Sequence next;
};
#endif

typedef struct DisruptorRingQueueHead DisruptorRingQueueHead;

///////////////////////////////////////////////////////////////////
// class SmallDisruptorRingQueueCore<Capacity>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capacity>
class SmallDisruptorRingQueueCore
{
public:
    typedef uint32_t    size_type;
    typedef T           item_type;

public:
    static const size_type  kCapacityCore   = (size_type)JIMI_MAX(JIMI_ROUND_TO_POW2(Capacity), 2);
    static const bool       kIsAllocOnHeap  = false;

public:
    DisruptorRingQueueHead  info;
    volatile item_type      entries[kCapacityCore];
};

///////////////////////////////////////////////////////////////////
// class DisruptorRingQueueCore<Capacity>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capacity>
class DisruptorRingQueueCore
{
public:
    typedef T   item_type;

public:
    static const bool kIsAllocOnHeap = true;

public:
    DisruptorRingQueueHead  info;
    volatile item_type     *entries;
};

///////////////////////////////////////////////////////////////////
// class DisruptorRingQueueBase<T, Capacity, CoreTy>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capacity = 1024U,
          typename CoreTy = DisruptorRingQueueCore<T, Capacity> >
class DisruptorRingQueueBase
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
    DisruptorRingQueueBase(bool bInitHead = false);
    ~DisruptorRingQueueBase();

public:
    void dump_info();
    void dump_detail();

    index_type mask() const       { return kMask;     };
    size_type  capacity() const   { return kCapacity; };
    size_type  length() const     { return sizes();   };
    size_type  sizes() const;

    void init(bool bInitHead = false);

    int push(T & entry);
    int pop (T & entry);

    int spin_push(T & entry);
    int spin_pop (T & entry);

    int mutex_push(T & entry);
    int mutex_pop (T & entry);


protected:
    core_type       core;
    spin_mutex_t    spin_mutex;
    pthread_mutex_t queue_mutex;
};

template <typename T, uint32_t Capacity, typename CoreTy>
DisruptorRingQueueBase<T, Capacity, CoreTy>::DisruptorRingQueueBase(bool bInitHead  /* = false */)
{
    init(bInitHead);
}

template <typename T, uint32_t Capacity, typename CoreTy>
DisruptorRingQueueBase<T, Capacity, CoreTy>::~DisruptorRingQueueBase()
{
    // Do nothing!
    Jimi_ReadWriteBarrier();

    spin_mutex.locked = 0;

    pthread_mutex_destroy(&queue_mutex);
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
void DisruptorRingQueueBase<T, Capacity, CoreTy>::init(bool bInitHead /* = false */)
{
    //printf("DisruptorRingQueueBase::init();\n\n");

    if (!bInitHead) {
        core.info.head = 0;
        core.info.tail = 0;
    }
    else {
        memset((void *)&core.info, 0, sizeof(core.info));
    }

    Jimi_ReadWriteBarrier();

    core.info.cursor.set(0x1234);
    core.info.next.set(0x5678);

    std::atomic<uint32_t> IsPublished;

    IsPublished.store(1, std::memory_order_release);

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
void DisruptorRingQueueBase<T, Capacity, CoreTy>::dump_info()
{
    //ReleaseUtils::dump(&core.info, sizeof(core.info));
    dump_mem(&core.info, sizeof(core.info), false, 16, 0, 0);
}

template <typename T, uint32_t Capacity, typename CoreTy>
void DisruptorRingQueueBase<T, Capacity, CoreTy>::dump_detail()
{
#if 0
    printf("---------------------------------------------------------\n");
    printf("DisruptorRingQueueBase.p.head = %u\nDisruptorRingQueueBase.p.tail = %u\n\n", core.info.p.head, core.info.p.tail);
    printf("DisruptorRingQueueBase.c.head = %u\nDisruptorRingQueueBase.c.tail = %u\n",   core.info.c.head, core.info.c.tail);
    printf("---------------------------------------------------------\n\n");
#else
    printf("DisruptorRingQueueBase: (head = %u, tail = %u)\n",
           core.info.head, core.info.tail);
#endif
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
typename DisruptorRingQueueBase<T, Capacity, CoreTy>::size_type
DisruptorRingQueueBase<T, Capacity, CoreTy>::sizes() const
{
    index_type head, tail;

    Jimi_ReadWriteBarrier();

    head = core.info.head;

    tail = core.info.tail;

    return (size_type)((head - tail) <= kMask) ? (head - tail) : (size_type)-1;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
int DisruptorRingQueueBase<T, Capacity, CoreTy>::push(T & entry)
{
    index_type head, tail, next;
    bool ok = false;

    Jimi_ReadWriteBarrier();

    do {
        head = core.info.head;
        tail = core.info.tail;
        if ((head - tail) > kMask)
            return -1;
        next = head + 1;
        ok = jimi_bool_compare_and_swap32(&core.info.head, head, next);
    } while (!ok);

    core.entries[head & kMask].value = entry.value;

    Jimi_ReadWriteBarrier();

    return 0;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
int DisruptorRingQueueBase<T, Capacity, CoreTy>::pop(T & entry)
{
    index_type head, tail, next;
    bool ok = false;

    Jimi_ReadWriteBarrier();

    do {
        head = core.info.head;
        tail = core.info.tail;
        if ((tail == head) || (tail > head && (head - tail) > kMask))
            return -1;
        next = tail + 1;
        ok = jimi_bool_compare_and_swap32(&core.info.tail, tail, next);
    } while (!ok);

    entry.value = core.entries[tail & kMask].value;

    Jimi_ReadWriteBarrier();

    return 0;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
int DisruptorRingQueueBase<T, Capacity, CoreTy>::spin_push(T & entry)
{
    index_type head, tail, next;
    int32_t pause_cnt;
    uint32_t loop_count, yield_cnt, spin_count;
    static const uint32_t YIELD_THRESHOLD = SPIN_YIELD_THRESHOLD;

    Jimi_ReadWriteBarrier();

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
        Jimi_ReadWriteBarrier();
        spin_mutex.locked = 0;
        return -1;
    }
    next = head + 1;
    core.info.head = next;

    core.entries[head & kMask].value = entry.value;

    Jimi_ReadWriteBarrier();

    spin_mutex.locked = 0;

    return 0;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
int DisruptorRingQueueBase<T, Capacity, CoreTy>::spin_pop(T & entry)
{
    index_type head, tail, next;
    value_type item;
    int32_t pause_cnt;
    uint32_t loop_count, yield_cnt, spin_count;
    static const uint32_t YIELD_THRESHOLD = SPIN_YIELD_THRESHOLD;

    Jimi_ReadWriteBarrier();

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
        Jimi_ReadWriteBarrier();
        spin_mutex.locked = 0;
        return -1;
    }
    next = tail + 1;
    core.info.tail = next;

    entry.value = core.entries[tail & kMask].value;

    Jimi_ReadWriteBarrier();

    spin_mutex.locked = 0;

    return 0;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
int DisruptorRingQueueBase<T, Capacity, CoreTy>::mutex_push(T & entry)
{
    index_type head, tail, next;

    Jimi_ReadWriteBarrier();

    pthread_mutex_lock(&queue_mutex);

    head = core.info.head;
    tail = core.info.tail;
    if ((head - tail) > kMask) {
        pthread_mutex_unlock(&queue_mutex);
        return -1;
    }
    next = head + 1;
    core.info.head = next;

    core.entries[head & kMask].value = entry.value;

    Jimi_ReadWriteBarrier();

    pthread_mutex_unlock(&queue_mutex);

    return 0;
}

template <typename T, uint32_t Capacity, typename CoreTy>
inline
int DisruptorRingQueueBase<T, Capacity, CoreTy>::mutex_pop(T & entry)
{
    index_type head, tail, next;

    Jimi_ReadWriteBarrier();

    pthread_mutex_lock(&queue_mutex);

    head = core.info.head;
    tail = core.info.tail;
    //if (tail >= head && (head - tail) <= kMask)
    if ((tail == head) || (tail > head && (head - tail) > kMask)) {
        pthread_mutex_unlock(&queue_mutex);
        return -1;
    }
    next = tail + 1;
    core.info.tail = next;

    entry.value = core.entries[tail & kMask].value;

    Jimi_ReadWriteBarrier();

    pthread_mutex_unlock(&queue_mutex);

    return 0;
}

///////////////////////////////////////////////////////////////////
// class SmallRingQueue<T, Capacity>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capacity = 1024U>
class SmallDisruptorRingQueue : public DisruptorRingQueueBase<T, Capacity, SmallDisruptorRingQueueCore<T, Capacity> >
{
public:
    typedef uint32_t                    size_type;
    typedef uint32_t                    index_type;
    typedef T                           item_type;
    typedef T *                         value_type;
    typedef T *                         pointer;
    typedef const T *                   const_pointer;
    typedef T &                         reference;
    typedef const T &                   const_reference;

    static const size_type kCapacity = DisruptorRingQueueBase<T, Capacity, SmallDisruptorRingQueueCore<T, Capacity> >::kCapacity;

public:
    SmallDisruptorRingQueue(bool bFillQueue = true, bool bInitHead = false);
    ~SmallDisruptorRingQueue();

public:
    void dump_detail();

protected:
    void init_queue(bool bFillQueue = true);
};

template <typename T, uint32_t Capacity>
SmallDisruptorRingQueue<T, Capacity>::SmallDisruptorRingQueue(bool bFillQueue /* = true */,
                                             bool bInitHead  /* = false */)
: DisruptorRingQueueBase<T, Capacity, SmallDisruptorRingQueueCore<T, Capacity> >(bInitHead)
{
    init_queue(bFillQueue);
}

template <typename T, uint32_t Capacity>
SmallDisruptorRingQueue<T, Capacity>::~SmallDisruptorRingQueue()
{
    // Do nothing!
}

template <typename T, uint32_t Capacity>
inline
void SmallDisruptorRingQueue<T, Capacity>::init_queue(bool bFillQueue /* = true */)
{
    if (bFillQueue) {
        memset((void *)this->core.entries, 0, sizeof(item_type) * kCapacity);
    }
}

template <typename T, uint32_t Capacity>
void SmallDisruptorRingQueue<T, Capacity>::dump_detail()
{
    printf("SmallRingQueue: (head = %u, tail = %u)\n",
           this->core.info.head, this->core.info.tail);
}

///////////////////////////////////////////////////////////////////
// class DisruptorRingQueue<T, Capacity>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capacity = 1024U>
class DisruptorRingQueue : public DisruptorRingQueueBase<T, Capacity, DisruptorRingQueueCore<T, Capacity> >
{
public:
    typedef uint32_t                    size_type;
    typedef uint32_t                    index_type;
    typedef T                           item_type;
    typedef T *                         value_type;
    typedef T *                         pointer;
    typedef const T *                   const_pointer;
    typedef T &                         reference;
    typedef const T &                   const_reference;

    typedef DisruptorRingQueueCore<T, Capacity>   core_type;

    static const size_type kCapacity = DisruptorRingQueueBase<T, Capacity, DisruptorRingQueueCore<T, Capacity> >::kCapacity;

public:
    DisruptorRingQueue(bool bFillQueue = true, bool bInitHead = false);
    ~DisruptorRingQueue();

public:
    void dump_detail();

protected:
    void init_queue(bool bFillQueue = true);
};

template <typename T, uint32_t Capacity>
DisruptorRingQueue<T, Capacity>::DisruptorRingQueue(bool bFillQueue /* = true */,
                                                    bool bInitHead  /* = false */)
: DisruptorRingQueueBase<T, Capacity, DisruptorRingQueueCore<T, Capacity> >(bInitHead)
{
    init_queue(bFillQueue);
}

template <typename T, uint32_t Capacity>
DisruptorRingQueue<T, Capacity>::~DisruptorRingQueue()
{
    // If the queue is allocated on system heap, release them.
    if (DisruptorRingQueueCore<T, Capacity>::kIsAllocOnHeap) {
        delete [] this->core.entries;
        this->core.entries = NULL;
    }
}

template <typename T, uint32_t Capacity>
inline
void DisruptorRingQueue<T, Capacity>::init_queue(bool bFillQueue /* = true */)
{
    item_type *newData = new T[kCapacity];
    if (newData != NULL) {
        this->core.entries = newData;
        if (bFillQueue) {
            memset((void *)this->core.entries, 0, sizeof(item_type) * kCapacity);
        }
    }
}

template <typename T, uint32_t Capacity>
void DisruptorRingQueue<T, Capacity>::dump_detail()
{
    printf("DisruptorRingQueue: (head = %u, tail = %u)\n",
           this->core.info.head, this->core.info.tail);
}

}  /* namespace jimi */

#endif  /* _JIMI_DISRUPTOR_RINGQUEUE_H_ */
