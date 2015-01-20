
#ifndef _JIMI_DISRUPTOR_RINGQUEUE_H_
#define _JIMI_DISRUPTOR_RINGQUEUE_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include "vs_stdint.h"
#include "port.h"
#include "sleep.h"

//#include <atomic>

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
#include <assert.h>

#include "dump_mem.h"

namespace jimi {

#if 0
struct DisruptorRingQueueInfo
{
    volatile uint32_t head;
    volatile uint32_t tail;
};
#else
struct DisruptorRingQueueInfo
{
    volatile uint32_t head;
    char padding1[JIMI_CACHE_LINE_SIZE - sizeof(uint32_t)];

    volatile uint32_t tail;
    char padding2[JIMI_CACHE_LINE_SIZE - sizeof(uint32_t)];
};
#endif

typedef struct DisruptorRingQueueInfo DisruptorRingQueueInfo;

///////////////////////////////////////////////////////////////////
// class SmallDisruptorRingQueueCore<Capacity>
///////////////////////////////////////////////////////////////////

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers>
class SmallDisruptorRingQueueCore
{
public:
    typedef T           item_type;
    typedef uint32_t    flag_type;
#if 0
    typedef size_t      size_type;    
#else
    typedef uint32_t    size_type;
#endif

    typedef SequenceBase<SequenceType>  Sequence;

public:
    static const size_type  kCapacityCore   = (size_type)JIMI_MAX(JIMI_ROUND_TO_POW2(Capacity), 2);
    static const size_type  kProducers      = Producers;
    static const size_type  kConsumers      = Consumers;
    static const size_type  kProducersAlloc = (Producers <= 1) ? 1 : ((Producers + 1) & ((size_type)(~1U)));
    static const size_type  kConsumersAlloc = (Consumers <= 1) ? 1 : ((Consumers + 1) & ((size_type)(~1U)));
    static const bool       kIsAllocOnHeap  = false;

public:
    DisruptorRingQueueInfo  info;

    Sequence                cursor, workSequence;
    Sequence                gatingSequenceCache;
    Sequence                gatingSequences[kConsumersAlloc];

    item_type               entries[kCapacityCore];
    flag_type               availableBuffer[kCapacityCore];
};

///////////////////////////////////////////////////////////////////
// class DisruptorRingQueueCore<Capacity>
///////////////////////////////////////////////////////////////////

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers>
class DisruptorRingQueueCore
{
public:
    typedef T           item_type;
    typedef uint32_t    flag_type;
#if 0
    typedef size_t      size_type;    
#else
    typedef uint32_t    size_type;
#endif

    typedef SequenceBase<SequenceType>  Sequence;

public:
    static const size_type  kCapacityCore   = (size_type)JIMI_MAX(JIMI_ROUND_TO_POW2(Capacity), 2);
    static const size_type  kProducers      = Producers;
    static const size_type  kConsumers      = Consumers;
    static const size_type  kProducersAlloc = (Producers <= 1) ? 1 : ((Producers + 1) & ((size_type)(~1U)));
    static const size_type  kConsumersAlloc = (Consumers <= 1) ? 1 : ((Consumers + 1) & ((size_type)(~1U)));
    static const bool       kIsAllocOnHeap  = true;

public:
    DisruptorRingQueueInfo  info;

    Sequence                cursor, workSequence;
    Sequence                gatingSequenceCache;
    Sequence                gatingSequences[kConsumersAlloc];

    item_type *             entries;
    flag_type *             availableBuffer;
};

///////////////////////////////////////////////////////////////////
// class DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>
///////////////////////////////////////////////////////////////////

template <typename T, typename SequenceType = int64_t, uint32_t Capacity = 1024U,
          uint32_t Producers = 0, uint32_t Consumers = 0,
          typename CoreTy = DisruptorRingQueueCore<T, SequenceType, Capacity, Producers, Consumers> >
class DisruptorRingQueueBase
{
public:
    typedef CoreTy                      core_type;
    typedef typename CoreTy::item_type  item_type;
    typedef typename CoreTy::size_type  size_type;
    typedef typename CoreTy::flag_type  flag_type;
    typedef uint32_t                    index_type;
    typedef SequenceType                sequence_type;
    typedef typename CoreTy::Sequence   Sequence;

    typedef item_type                   value_type;
    typedef item_type *                 pointer;
    typedef const item_type *           const_pointer;
    typedef item_type &                 reference;
    typedef const item_type &           const_reference;

public:
    static const size_type  kCapacity       = CoreTy::kCapacityCore;
    static const index_type kIndexMask      = (index_type)(kCapacity - 1);
    static const uint32_t   kIndexShift     = JIMI_POPCONUT32(kIndexMask);

    static const size_type  kProducers      = CoreTy::kProducers;
    static const size_type  kConsumers      = CoreTy::kConsumers;
    static const size_type  kProducersAlloc = CoreTy::kConsumersAlloc;
    static const size_type  kConsumersAlloc = CoreTy::kConsumersAlloc;
    static const bool       kIsAllocOnHeap  = CoreTy::kIsAllocOnHeap;

    struct PopThreadStackData
    {
        Sequence       *tailSequence;
        sequence_type   current;
        sequence_type   cachedAvailableSequence;
        bool            processedSequence;
    };

    typedef struct PopThreadStackData PopThreadStackData;

public:
    DisruptorRingQueueBase(bool bInitHead = false);
    ~DisruptorRingQueueBase();

public:
    static sequence_type getMinimumSequence(const Sequence *sequences, const Sequence &workSequence,
                                            sequence_type mininum);

    void dump();
    void dump_core();
    void dump_info();
    void dump_detail();

    index_type mask() const     { return kIndexMask; };
    size_type  capacity() const { return kCapacity;  };
    size_type  length() const   { return sizes();    };
    size_type  sizes() const;

    void init(bool bInitHead = false);

    void start();
    void shutdown(int32_t timeOut = -1);

    Sequence *getGatingSequences(int index);

    void publish(sequence_type sequence);
    void setAvailable(sequence_type sequence)
        ;
    bool isAvailable(sequence_type sequence);
    sequence_type getHighestPublishedSequence(sequence_type lowerBound,
                                              sequence_type availableSequence);

    int push(const T & entry);
    int pop (T & entry, PopThreadStackData &stackData);
    int pop (T & entry, Sequence &tailSequence, sequence_type &nextSequence,
             sequence_type &cachedAvailableSequence, bool &processedSequence);

    sequence_type waitFor(sequence_type sequence);

    int q3_push(const T & entry);
    int q3_pop (T & entry);

    int spin_push(const T & entry);
    int spin_pop (T & entry);

    int mutex_push(const T & entry);
    int mutex_pop (T & entry);

protected:
    core_type       core;

    spin_mutex_t    spin_mutex;
    pthread_mutex_t queue_mutex;
};

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::DisruptorRingQueueBase(bool bInitHead /* = false */)
{
    init(bInitHead);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::~DisruptorRingQueueBase()
{
    // Do nothing!
    Jimi_ReadWriteBarrier();

    spin_mutex.locked = 0;

    pthread_mutex_destroy(&queue_mutex);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
void DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::init(bool bInitHead /* = false */)
{
    if (!bInitHead) {
        core.info.head = 0;
        core.info.tail = 0;
    }
    else {
        memset((void *)&core.info, 0, sizeof(core.info));
    }

    core.cursor.set(Sequence::INITIAL_CURSOR_VALUE);
    core.workSequence.set(Sequence::INITIAL_CURSOR_VALUE);
    //core.cursor.set(0x1234);
    //core.workSequence.set(0x5678);

#if defined(_DEBUG) || !defined(NDEBUG)
#if 0
    printf("CoreTy::kProducers      = %d\n",    kProducers);
    printf("CoreTy::kConsumers      = %d\n",    kConsumers);
    printf("CoreTy::kConsumersAlloc = %d\n",    kConsumersAlloc);
    printf("CoreTy::kCapacity       = %d\n",    kCapacity);
    printf("CoreTy::kIndexMask      = %d\n",    kIndexMask);
    printf("CoreTy::kIndexShift     = %d\n",    kIndexShift);
    printf("\n");
#endif
#endif

    for (int i = 0; i < kConsumersAlloc; ++i) {
        //core.gatingSequences[i].set(0x00111111U * (i + 1));
        core.gatingSequences[i].set(Sequence::INITIAL_CURSOR_VALUE);
    }

    Jimi_ReadWriteBarrier();

    // Initilized spin mutex
    spin_mutex.locked = 0;
    spin_mutex.spin_counter = MUTEX_MAX_SPIN_COUNT;
    spin_mutex.recurse_counter = 0;
    spin_mutex.thread_id = 0;
    spin_mutex.reserve = 0;

    // Initilized mutex
    pthread_mutex_init(&queue_mutex, NULL);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
void DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::dump()
{
    //ReleaseUtils::dump(&core, sizeof(core));
    memory_dump(this, sizeof(*this), false, 16, 0, 0);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
void DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::dump_core()
{
    //ReleaseUtils::dump(&core, sizeof(core));
    memory_dump(&core, sizeof(core), false, 16, 0, 0);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
void DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::dump_info()
{
    //ReleaseUtils::dump(&core.info, sizeof(core.info));
    memory_dump(&core.info, sizeof(core.info), false, 16, 0, 0);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
void DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::dump_detail()
{
#if 0
    printf("---------------------------------------------------------\n");
    printf("DisruptorRingQueueBase.p.head = %u\nDisruptorRingQueueBase.p.tail = %u\n\n", core.info.p.head, core.info.p.tail);
    printf("DisruptorRingQueueBase.c.head = %u\nDisruptorRingQueueBase.c.tail = %u\n",   core.info.c.head, core.info.c.tail);
    printf("---------------------------------------------------------\n\n");
#else
    printf("---------------------------------------------------------\n");
    printf("DisruptorRingQueueBase: (head = %u, tail = %u)\n",
           core.info.head, core.info.tail);
    printf("DisruptorRingQueueBase: (cursor = %u, workSequence = %u)\n",
           core.cursor.get(), core.workSequence.get());
    printf("---------------------------------------------------------\n");
    printf("\n");
#endif
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
typename DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::size_type
DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::sizes() const
{
    sequence_type head, tail;

    Jimi_ReadWriteBarrier();

    head = core.info.head;

    tail = core.info.tail;

    return (size_type)((head - tail) <= kIndexMask) ? (head - tail) : (size_type)(-1);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
void DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::start()
{
    sequence_type cursor = core.cursor.get();
    core.workSequence.set(cursor);
    core.gatingSequenceCache.set(cursor);

    int i;
    /*
    for (i = 0; i < kProducersAlloc; ++i) {
        core.gatingSequenceCaches[i].set(cursor);
    }
    //*/

    for (i = 0; i < kConsumersAlloc; ++i) {
        core.gatingSequences[i].set(cursor);
    }
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
void DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::shutdown(int32_t timeOut /* = -1 */)
{
    //
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
/* static */
inline
typename DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::sequence_type
DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::
    getMinimumSequence(const Sequence *sequences, const Sequence &workSequence, sequence_type mininum)
{
    assert(sequences != NULL);

#if 0
    sequence_type minSequence = sequences->get();
    for (int i = 1; i < kConsumers; ++i) {
        ++sequences;
        sequence_type seq = sequences->get();
#if 1
        minSequence = (seq < minSequence) ? seq : minSequence;
#else
        if (seq < minSequence)
            minSequence = seq;
#endif
    }

    sequence_type cachedWorkSequence;
    cachedWorkSequence = workSequence.get();
    if (cachedWorkSequence < minSequence)
        minSequence = cachedWorkSequence;

    if (mininum < minSequence)
        minSequence = mininum;
#else
    sequence_type minSequence = mininum;
    for (int i = 0; i < kConsumers; ++i) {
        sequence_type seq = sequences->get();
#if 1
        minSequence = (seq < minSequence) ? seq : minSequence;
#else
        if (seq < minSequence)
            minSequence = seq;
#endif
        ++sequences;
    }

    sequence_type cachedWorkSequence;
    cachedWorkSequence = workSequence.get();
    if (cachedWorkSequence < minSequence)
        minSequence = cachedWorkSequence;
#endif

    return minSequence;
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
void DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::publish(sequence_type sequence)
{
    Jimi_WriteBarrier();

    setAvailable(sequence);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
void DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::setAvailable(sequence_type sequence)
{
    index_type index = (index_type)((index_type)sequence &  kIndexMask);
    flag_type  flag  = (flag_type) (            sequence >> kIndexShift);

    if (core_type::kIsAllocOnHeap) {
        assert(core.availableBuffer != NULL);
    }
    Jimi_WriteBarrier();
    core.availableBuffer[index] = flag;
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
bool DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::isAvailable(sequence_type sequence)
{
    index_type index = (index_type)((index_type)sequence &  kIndexMask);
    flag_type  flag  = (flag_type) (            sequence >> kIndexShift);

    flag_type  flagValue = core.availableBuffer[index];
    Jimi_ReadBarrier();
    return (flagValue == flag);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
typename DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::sequence_type
DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::
        getHighestPublishedSequence(sequence_type lowerBound, sequence_type availableSequence)
{
    for (sequence_type sequence = lowerBound; sequence <= availableSequence; ++sequence) {
        if (!isAvailable(sequence)) {
            return (sequence - 1);
        }
    }

    return availableSequence;
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
typename DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::Sequence *
DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::getGatingSequences(int index)
{
    if (index >= 0 && index < kCapacity) {
        return &core.gatingSequences[index];
    }
    return NULL;
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
int DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::push(const T & entry)
{
    sequence_type current, nextSequence;
    do {
        current = core.cursor.get();
        nextSequence = current + 1;

        //sequence_type wrapPoint = nextSequence - kCapacity;
        sequence_type wrapPoint = current - kIndexMask;
        sequence_type cachedGatingSequence = core.gatingSequenceCache.get();

        if (wrapPoint > cachedGatingSequence || cachedGatingSequence > current) {
        //if ((current - cachedGatingSequence) >= kIndexMask) {
            sequence_type gatingSequence = DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>
                                            ::getMinimumSequence(core.gatingSequences, core.workSequence, current);
            //current = core.cursor.get();
            if (wrapPoint > gatingSequence) {
            //if ((current - gatingSequence) >= kIndexMask) {
                // Push() failed, maybe queue is full.
                //core.gatingSequenceCaches[id].set(gatingSequence);
#if 0
                jimi_sleep(0);
                continue;
#else
                return -1;
#endif
            }

            core.gatingSequenceCache.set(gatingSequence);
        }
        else if (core.cursor.compareAndSwap(current, nextSequence) != current) {
            // Need yiled() or sleep() a while.
            //jimi_wsleep(0);
        }
        else {
            // Claim a sequence succeeds.
            break;
        }
    } while (true);

    core.entries[nextSequence & kIndexMask] = entry;
    //core.entries[nextSequence & kIndexMask].copy(entry);

    Jimi_WriteBarrier();

    publish(nextSequence);

    Jimi_WriteBarrier();
    return 0;
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
int DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::pop(T & entry, PopThreadStackData &stackData)
{
    assert(stackData.tailSequence != NULL);
    return pop(entry, *stackData.tailSequence, stackData.current,
               stackData.cachedAvailableSequence, stackData.processedSequence);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
int DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::pop(T & entry, Sequence &tailSequence,
                                                                           sequence_type &nextSequence,
                                                                           sequence_type &cachedAvailableSequence,
                                                                           bool &processedSequence)
{
    sequence_type cursor, current, limit;
    while (true) {
        if (processedSequence) {
            processedSequence = false;
            do {
                cursor  = core.cursor.get();
                current = core.workSequence.get();
                nextSequence = current + 1;
                limit = cursor - 1;
                tailSequence.set(current);
#if 0
                if ((current == limit) || (current > limit && (limit - current) > kIndexMask)) {
#if 0
                    Jimi_ReadBarrier();
                    //processedSequence = true;
                    return -1;
#else
                    //jimi_wsleep(0);
#endif
                }
#endif
            } while (core.workSequence.compareAndSwap(current, nextSequence) != current);
        }

        if (cachedAvailableSequence >= nextSequence) {
        //if ((cachedAvailableSequence - current) <= kIndexMask * 2) {
        //if ((cachedAvailableSequence - nextSequence) <= (kIndexMask + 1)) {
            // Read the message data
            entry = core.entries[nextSequence & kIndexMask];

            Jimi_ReadBarrier();
            //tailSequence.set(nextSequence);
            processedSequence = true;

            Jimi_ReadBarrier();
            return 0;
        }
        else {
            // Maybe queue is empty now.
            //cachedAvailableSequence = waitFor(current + 1);
            cachedAvailableSequence = waitFor(nextSequence);
            //tailSequence.set(cachedAvailableSequence);
            if (cachedAvailableSequence < nextSequence)
                return -1;
        }
    }
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
typename DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::sequence_type
DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::waitFor(sequence_type sequence)
{
    sequence_type availableSequence;

#if defined(USE_SEQUENCE_SPIN_LOCK) && (USE_SEQUENCE_SPIN_LOCK != 0)
    static const uint32_t YIELD_THRESHOLD = 20;
#else
    static const uint32_t YIELD_THRESHOLD = 8;
#endif
    int32_t  pause_cnt;
    uint32_t loop_cnt, yeild_cnt, spin_cnt;

    loop_cnt = 0;
    spin_cnt = 1;
    while ((availableSequence = core.cursor.get()) < sequence) {
        // Need yiled() or sleep() a while.
        if (loop_cnt >= YIELD_THRESHOLD) {
            yeild_cnt = loop_cnt - YIELD_THRESHOLD;
            if ((yeild_cnt & 63) == 63) {
                jimi_sleep(1);
            }
            else if ((yeild_cnt & 3) == 3) {
                jimi_sleep(0);
            }
            else {
                if (!jimi_yield()) {
                    jimi_sleep(0);
                    //jimi_mm_pause();
                }
            }
        }
        else {
            for (pause_cnt = spin_cnt; pause_cnt > 0; --pause_cnt) {
                jimi_mm_pause();
            }
            spin_cnt = spin_cnt + 1;
        }
        loop_cnt++;
    }

    if (availableSequence < sequence)
        return availableSequence;

    return getHighestPublishedSequence(sequence, availableSequence);
}

#if 0
void reserve_code()
{
    sequence_type head, tail, next;
    sequence_type wrapPoint;
    bool maybeIsFull = false;
    do {
        head = core.cursor.get();
        tail = core.gatingSequenceCache.get();

        next = head + 1;
        //wrapPoint = next - kCapacity;
        wrapPoint = head - kIndexMask;

        if ((head - tail) > kIndexMask) {
            // Push() failed, maybe queue is full.
            maybeIsFull = true;
            //return -1;
        }

        if (maybeIsFull || tail < wrapPoint || tail > head) {
            sequence_type gatingSequence = DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>
                                            ::getMinimumSequence(core.gatingSequences, core.workSequence, head);
            if (maybeIsFull || wrapPoint > gatingSequence) {
                // Push() failed, maybe queue is full.
                return -1;
            }

            core.gatingSequenceCache.set(gatingSequence);
        }

        if (core.cursor.compareAndSwap(head, next) != head) {
            // Need yiled() or sleep() a while.
            jimi_wsleep(1);
        }
        else {
            // Claim a sequence succeeds.
            break;
        }
    } while (true);

    Jimi_WriteBarrier();

    core.entries[head & kIndexMask] = entry;
    //core.entries[head & kIndexMask].copy(entry);

    publish(head);

    Jimi_WriteBarrier();
}
#endif

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
int DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::q3_push(const T & entry)
{
    sequence_type head, tail, next;
    bool ok = false;

    Jimi_ReadWriteBarrier();

    do {
        head = core.info.head;
        tail = core.info.tail;
        if ((head - tail) > kIndexMask)
            return -1;
        next = head + 1;
        ok = jimi_bool_compare_and_swap32(&core.info.head, head, next);
    } while (!ok);

    Jimi_WriteBarrier();

    core.entries[head & kIndexMask] = entry;
    //core.entries[head & kIndexMask].copy(entry);

    return 0;
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
int DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::q3_pop(T & entry)
{
    sequence_type head, tail, next;
    bool ok = false;

    Jimi_ReadWriteBarrier();

    do {
        head = core.info.head;
        tail = core.info.tail;
        if ((tail == head) || (tail > head && (head - tail) > kIndexMask))
            return -1;
        next = tail + 1;
        ok = jimi_bool_compare_and_swap32(&core.info.tail, tail, next);
    } while (!ok);

    entry = core.entries[tail & kIndexMask];

    Jimi_ReadBarrier();

    return 0;
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
int DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::spin_push(const T & entry)
{
    sequence_type head, tail, next;
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
                        //jimi_wsleep(0);
                        jimi_mm_pause();
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
                        //jimi_wsleep(0);
                        jimi_mm_pause();
                    }
                }
#endif
            }
            loop_count++;
        } while (jimi_val_compare_and_swap32(&spin_mutex.locked, 0U, 1U) != 0U);
    }

    head = core.info.head;
    tail = core.info.tail;
    if ((head - tail) > kIndexMask) {
        Jimi_ReadWriteBarrier();
        spin_mutex.locked = 0;
        return -1;
    }
    next = head + 1;
    core.info.head = next;

    core.entries[head & kIndexMask] = entry;

    Jimi_ReadWriteBarrier();

    spin_mutex.locked = 0;

    return 0;
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
int DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::spin_pop(T & entry)
{
    sequence_type head, tail, next;
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
                        //jimi_wsleep(0);
                        jimi_mm_pause();
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
                        //jimi_wsleep(0);
                        jimi_mm_pause();
                    }
                }
#endif
            }
            loop_count++;
        } while (jimi_val_compare_and_swap32(&spin_mutex.locked, 0U, 1U) != 0U);
    }

    head = core.info.head;
    tail = core.info.tail;
    if ((tail == head) || (tail > head && (head - tail) > kIndexMask)) {
        Jimi_ReadWriteBarrier();
        spin_mutex.locked = 0;
        return -1;
    }
    next = tail + 1;
    core.info.tail = next;

    entry = core.entries[tail & kIndexMask];

    Jimi_ReadWriteBarrier();

    spin_mutex.locked = 0;

    return 0;
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
int DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::mutex_push(const T & entry)
{
    sequence_type head, tail, next;

    Jimi_ReadWriteBarrier();

    pthread_mutex_lock(&queue_mutex);

    head = core.info.head;
    tail = core.info.tail;
    if ((head - tail) > kIndexMask) {
        pthread_mutex_unlock(&queue_mutex);
        return -1;
    }
    next = head + 1;
    core.info.head = next;

    core.entries[head & kIndexMask] = entry;

    Jimi_ReadWriteBarrier();

    pthread_mutex_unlock(&queue_mutex);

    return 0;
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, typename CoreTy>
inline
int DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, CoreTy>::mutex_pop(T & entry)
{
    sequence_type head, tail, next;

    Jimi_ReadWriteBarrier();

    pthread_mutex_lock(&queue_mutex);

    head = core.info.head;
    tail = core.info.tail;
    //if (tail >= head && (head - tail) <= kIndexMask)
    if ((tail == head) || (tail > head && (head - tail) > kIndexMask)) {
        pthread_mutex_unlock(&queue_mutex);
        return -1;
    }
    next = tail + 1;
    core.info.tail = next;

    entry = core.entries[tail & kIndexMask];

    Jimi_ReadWriteBarrier();

    pthread_mutex_unlock(&queue_mutex);

    return 0;
}

///////////////////////////////////////////////////////////////////
// class SmallRingQueue<T, Capacity>
///////////////////////////////////////////////////////////////////

template <typename T, typename SequenceType = int64_t, uint32_t Capacity = 1024U,
          uint32_t Producers = 0, uint32_t Consumers = 0>
class SmallDisruptorRingQueue : public DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers,
                                         SmallDisruptorRingQueueCore<T, SequenceType, Capacity, Producers, Consumers> >
{
public:
    typedef SmallDisruptorRingQueueCore<T, SequenceType,Capacity, Producers, Consumers> core_type;
    typedef DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, core_type> parent_type;
    

    typedef typename parent_type::size_type     size_type;
    typedef typename parent_type::index_type    index_type;
    typedef typename parent_type::flag_type     flag_type;

    typedef T                           item_type;
    typedef T *                         value_type;
    typedef T *                         pointer;
    typedef const T *                   const_pointer;
    typedef T &                         reference;
    typedef const T &                   const_reference;

    static const size_type kCapacity = parent_type::kCapacity;

public:
    SmallDisruptorRingQueue(bool bFillQueue = true, bool bInitHead = false);
    ~SmallDisruptorRingQueue();

public:
    void dump();
    void dump_detail();

protected:
    void init_queue(bool bFillQueue = true);
};

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers>
SmallDisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers>::
    SmallDisruptorRingQueue(bool bFillQueue /* = true */,
                            bool bInitHead  /* = false */)
: parent_type(bInitHead)
{
    init_queue(bFillQueue);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers>
SmallDisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers>::~SmallDisruptorRingQueue()
{
    // Do nothing!
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers>
inline
void SmallDisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers>::init_queue(bool bFillQueue /* = true */)
{
    if (bFillQueue) {
        memset((void *)this->core.entries, 0, sizeof(item_type) * kCapacity);
    }
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers>
void SmallDisruptorRingQueue<T,SequenceType,  Capacity, Producers, Consumers>::dump()
{
    memory_dump(&this->core, sizeof(this->core), false, 16, 0, 0);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers>
void SmallDisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers>::dump_detail()
{
    printf("SmallRingQueue: (head = %u, tail = %u)\n",
           this->core.info.head, this->core.info.tail);

    printf("\n");
}

///////////////////////////////////////////////////////////////////
// class DisruptorRingQueue<T, Capacity>
///////////////////////////////////////////////////////////////////

template <typename T, typename SequenceType = int64_t, uint32_t Capacity = 1024U,
          uint32_t Producers = 0, uint32_t Consumers = 0>
class DisruptorRingQueue : public DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers,
                                    DisruptorRingQueueCore<T, SequenceType, Capacity, Producers, Consumers> >
{
public:
    typedef DisruptorRingQueueCore<T, SequenceType, Capacity, Producers, Consumers> core_type;
    typedef DisruptorRingQueueBase<T, SequenceType, Capacity, Producers, Consumers, core_type> parent_type;    

    typedef typename parent_type::size_type     size_type;
    typedef typename parent_type::index_type    index_type;
    typedef typename parent_type::flag_type     flag_type;

    typedef T                           item_type;
    typedef T *                         value_type;
    typedef T *                         pointer;
    typedef const T *                   const_pointer;
    typedef T &                         reference;
    typedef const T &                   const_reference;

    static const size_type kCapacity = parent_type::kCapacity;

public:
    DisruptorRingQueue(bool bFillQueue = true, bool bInitHead = false);
    ~DisruptorRingQueue();

public:
    void dump_detail();

protected:
    void init_queue(bool bFillQueue = true);
};

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers>
DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers>::DisruptorRingQueue(bool bFillQueue /* = true */,
                                                                                        bool bInitHead  /* = false */)
: parent_type(bInitHead)
{
    init_queue(bFillQueue);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers>
DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers>::~DisruptorRingQueue()
{
    // If the queue is allocated on system heap, release them.
    if (core_type::kIsAllocOnHeap) {
        if (this->core.availableBuffer) {
            delete [] this->core.availableBuffer;
            this->core.availableBuffer = NULL;
        }

        if (this->core.entries != NULL) {
            delete [] this->core.entries;
            this->core.entries = NULL;
        }
    }
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers>
inline
void DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers>::init_queue(bool bFillQueue /* = true */)
{
    item_type *newData = new T[kCapacity];
    if (newData != NULL) {
        if (bFillQueue) {
            memset((void *)newData, 0, sizeof(item_type) * kCapacity);
        }
        this->core.entries = newData;
    }

    flag_type *newBufferData = new flag_type[kCapacity];
    if (newBufferData != NULL) {
        if (bFillQueue) {
            //memset((void *)newBufferData, 0, sizeof(flag_type) * kCapacity);
            for (int i = 0; i < kCapacity; ++i) {
                newBufferData[i] = (flag_type)-1;
            }
        }
        this->core.availableBuffer = newBufferData;
    }
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers>
void DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers>::dump_detail()
{
    printf("---------------------------------------------------------\n");
    printf("DisruptorRingQueue: (head = %u, tail = %u)\n",
           this->core.info.head, this->core.info.tail);
    printf("DisruptorRingQueue: (cursor = %u, workSequence = %u)\n",
           this->core.cursor.get(), this->core.workSequence.get());
    printf("---------------------------------------------------------\n");

    printf("\n");
}

}  /* namespace jimi */

#endif  /* _JIMI_DISRUPTOR_RINGQUEUE_H_ */
