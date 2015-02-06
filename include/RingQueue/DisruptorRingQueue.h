
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

///////////////////////////////////////////////////////////////////
// class DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>
///////////////////////////////////////////////////////////////////

template <typename T, typename SequenceType = int64_t, uint32_t Capacity = 1024U,
          uint32_t Producers = 0, uint32_t Consumers = 0, uint32_t NumThreads = 0>
class DisruptorRingQueue
{
public:
    typedef T                           item_type;
    typedef item_type                   value_type;
#if 0
    typedef size_t                      size_type;    
#else
    typedef uint32_t                    size_type;
#endif
    typedef uint32_t                    flag_type;
    typedef uint32_t                    index_type;
    typedef SequenceType                sequence_type;
    typedef SequenceBase<SequenceType>  Sequence;

    
    typedef item_type *                 pointer;
    typedef const item_type *           const_pointer;
    typedef item_type &                 reference;
    typedef const item_type &           const_reference;

public:
    static const size_type  kCapacity       = (size_type)JIMI_MAX(JIMI_ROUND_TO_POW2(Capacity), 2);
    static const index_type kIndexMask      = (index_type)(kCapacity - 1);
    static const uint32_t   kIndexShift     = JIMI_POPCONUT32(kIndexMask);

    static const size_type  kProducers      = Producers;
    static const size_type  kConsumers      = Consumers;
    static const size_type  kProducersAlloc = (Producers <= 1) ? 1 : ((Producers + 1) & ((size_type)(~1U)));
    static const size_type  kConsumersAlloc = (Consumers <= 1) ? 1 : ((Consumers + 1) & ((size_type)(~1U)));
    static const bool       kIsAllocOnHeap  = true;

    struct PopThreadStackData
    {
        Sequence *      tailSequence;
        sequence_type   nextSequence;
        sequence_type   cachedAvailableSequence;
        bool            processedSequence;
    };

    typedef struct PopThreadStackData PopThreadStackData;

public:
    DisruptorRingQueue(bool bFillQueue = true);
    ~DisruptorRingQueue();

public:
    static sequence_type getMinimumSequence(const Sequence *sequences, const Sequence &workSequence,
                                            sequence_type mininum);

    void dump();
    void dump_detail();

    index_type mask() const     { return kIndexMask; };
    size_type  capacity() const { return kCapacity;  };
    size_type  length() const   { return sizes();    };
    size_type  sizes() const;

    void init(bool bFillQueue = true);
    void init_queue(bool bFillQueue = true);

    void start();
    void shutdown(int32_t timeOut = -1);

    Sequence *getGatingSequences(int index);

    void publish(sequence_type sequence);
    void setAvailable(sequence_type sequence);
    bool isAvailable(sequence_type sequence);
    sequence_type getHighestPublishedSequence(sequence_type lowerBound,
                                              sequence_type availableSequence);

    int push(const T & entry);
    int pop (T & entry, PopThreadStackData & data);

    sequence_type waitFor(sequence_type sequence);

protected:
    Sequence        cursor, workSequence;
    Sequence        gatingSequences[kConsumersAlloc];
    Sequence        gatingSequenceCache;
    Sequence        gatingSequenceCaches[kProducersAlloc];

    item_type *     entries;
    flag_type *     availableBuffer;
};

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::DisruptorRingQueue(bool bFillQueue /* = true */)
{
    init(bFillQueue);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::~DisruptorRingQueue()
{
    // If the queue is allocated on system heap, release them.
    if (kIsAllocOnHeap) {
        if (this->availableBuffer) {
            delete [] this->availableBuffer;
            this->availableBuffer = NULL;
        }

        if (this->entries != NULL) {
            delete [] this->entries;
            this->entries = NULL;
        }
    }
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
void DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::init(bool bFillQueue /* = true */)
{
    this->cursor.set(Sequence::INITIAL_CURSOR_VALUE);
    this->workSequence.set(Sequence::INITIAL_CURSOR_VALUE);

    for (int i = 0; i < kConsumersAlloc; ++i) {
        this->gatingSequences[i].set(Sequence::INITIAL_CURSOR_VALUE);
    }

    init_queue(bFillQueue);

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
#endif  /* _DEBUG */
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
void DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::init_queue(bool bFillQueue /* = true */)
{
    item_type *newData = new T[kCapacity];
    if (newData != NULL) {
        if (bFillQueue) {
            memset((void *)newData, 0, sizeof(item_type) * kCapacity);
        }
        Jimi_MemoryBarrier();
        this->entries = newData;
    }

    flag_type *newBufferData = new flag_type[kCapacity];
    if (newBufferData != NULL) {
        if (bFillQueue) {
            //memset((void *)newBufferData, 0, sizeof(flag_type) * kCapacity);
            for (int i = 0; i < kCapacity; ++i) {
                newBufferData[i] = (flag_type)(-1);
            }
        }
        Jimi_MemoryBarrier();
        this->availableBuffer = newBufferData;
    }
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
void DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::dump()
{
    //ReleaseUtils::dump(&core, sizeof(core));
    dump_memory(this, sizeof(*this), false, 16, 0, 0);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
void DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::dump_detail()
{
    printf("---------------------------------------------------------\n");
    printf("DisruptorRingQueue: (head = %u, tail = %u)\n",
           this->cursor.get(), this->workSequence.get());
    printf("---------------------------------------------------------\n");

    printf("\n");
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
typename DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::size_type
DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::sizes() const
{
    sequence_type head, tail;

    Jimi_ReadBarrier();

    head = this->cursor.get();
    tail = this->workSequence.get();

    return (size_type)((head - tail) <= kIndexMask) ? (head - tail) : (size_type)(-1);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
void DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::start()
{
    sequence_type cursor = this->cursor.get();
    this->workSequence.set(cursor);
    this->gatingSequenceCache.set(cursor);

    int i;
    for (i = 0; i < kConsumersAlloc; ++i) {
        this->gatingSequences[i].set(cursor);
    }
    /*
    for (i = 0; i < kProducersAlloc; ++i) {
        this->gatingSequenceCaches[i].set(cursor);
    }
    //*/
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
void DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::shutdown(int32_t timeOut /* = -1 */)
{
    // TODO: do shutdown procedure
}

/* static */
template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
typename DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::sequence_type
DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::
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

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
void DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::publish(sequence_type sequence)
{
    Jimi_WriteBarrier();

    setAvailable(sequence);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
void DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::setAvailable(sequence_type sequence)
{
    index_type index = (index_type)((index_type)sequence &  kIndexMask);
    flag_type  flag  = (flag_type) (            sequence >> kIndexShift);

    if (kIsAllocOnHeap) {
        assert(this->availableBuffer != NULL);
    }
    Jimi_WriteBarrier();
    this->availableBuffer[index] = flag;
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
bool DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::isAvailable(sequence_type sequence)
{
    index_type index = (index_type)((index_type)sequence &  kIndexMask);
    flag_type  flag  = (flag_type) (            sequence >> kIndexShift);

    flag_type  flagValue = this->availableBuffer[index];
    Jimi_ReadBarrier();
    return (flagValue == flag);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
typename DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::sequence_type
DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::
        getHighestPublishedSequence(sequence_type lowerBound, sequence_type availableSequence)
{
    for (sequence_type sequence = lowerBound; sequence <= availableSequence; ++sequence) {
        if (!isAvailable(sequence)) {
            return (sequence - 1);
        }
    }

    return availableSequence;
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
typename DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::Sequence *
DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::getGatingSequences(int index)
{
    if (index >= 0 && index < kCapacity) {
        return &this->gatingSequences[index];
    }
    return NULL;
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
int DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::push(const T & entry)
{
    sequence_type current, nextSequence;
    do {
        current = this->cursor.get();
        nextSequence = current + 1;

        //sequence_type wrapPoint = nextSequence - kCapacity;
        sequence_type wrapPoint = current - kIndexMask;
        sequence_type cachedGatingSequence = this->gatingSequenceCache.get();

        if (wrapPoint > cachedGatingSequence || cachedGatingSequence > current) {
        //if ((current - cachedGatingSequence) >= kIndexMask) {
            sequence_type gatingSequence = DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>
                                            ::getMinimumSequence(this->gatingSequences, this->workSequence, current);
            //current = this->cursor.get();
            if (wrapPoint > gatingSequence) {
            //if ((current - gatingSequence) >= kIndexMask) {
                // Push() failed, maybe queue is full.
                //this->gatingSequenceCaches[id].set(gatingSequence);
#if 0
                for (int i = 2; i > 0; --i)
                    jimi_mm_pause();
                continue;
#else
                return -1;
#endif
            }

            this->gatingSequenceCache.setOrder(gatingSequence);
        }
        else if (this->cursor.compareAndSwap(current, nextSequence) != current) {
            // Need yiled() or sleep() a while.
            //jimi_wsleep(0);
        }
        else {
            // Claim a sequence succeeds.
            break;
        }
    } while (true);

    this->entries[nextSequence & kIndexMask] = entry;
    //this->entries[nextSequence & kIndexMask].copy(entry);

    Jimi_WriteBarrier();

    publish(nextSequence);

    Jimi_WriteBarrier();
    return 0;
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
int DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::pop(T & entry, PopThreadStackData & data)
{
    assert(data.tailSequence != NULL);

    sequence_type current, cursor, limit;
    while (true) {
        if (data.processedSequence) {
            data.processedSequence = false;
            do {
                cursor  = this->cursor.get();
                limit = cursor - 1;
                current = this->workSequence.get();
                data.nextSequence = current + 1;
                data.tailSequence->set(current);
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
            } while (this->workSequence.compareAndSwap(current, data.nextSequence) != current);
        }

        if (data.cachedAvailableSequence >= data.nextSequence) {
        //if ((cachedAvailableSequence - current) <= kIndexMask * 2) {
        //if ((cachedAvailableSequence - nextSequence) <= (kIndexMask + 1)) {
            // Read the message data
            entry = this->entries[data.nextSequence & kIndexMask];

            Jimi_ReadBarrier();
            //data.tailSequence->set(data.nextSequence);
            data.processedSequence = true;

            Jimi_ReadBarrier();
            return 0;
        }
        else {
            // Maybe queue is empty now.
            data.cachedAvailableSequence = waitFor(data.nextSequence);
            //data.tailSequence->set(cachedAvailableSequence);
            if (data.cachedAvailableSequence < data.nextSequence)
                return -1;
        }
    }
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
typename DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::sequence_type
DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::waitFor(sequence_type sequence)
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
    while ((availableSequence = this->cursor.get()) < sequence) {
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
        head = this->cursor.get();
        tail = this->gatingSequenceCache.get();

        next = head + 1;
        //wrapPoint = next - kCapacity;
        wrapPoint = head - kIndexMask;

        if ((head - tail) > kIndexMask) {
            // Push() failed, maybe queue is full.
            maybeIsFull = true;
            //return -1;
        }

        if (maybeIsFull || tail < wrapPoint || tail > head) {
            sequence_type gatingSequence = DisruptorRingQueue<T, SequenceType, Capacity, Producers, Consumers, NumThreads>
                                            ::getMinimumSequence(this->gatingSequences, this->workSequence, head);
            if (maybeIsFull || wrapPoint > gatingSequence) {
                // Push() failed, maybe queue is full.
                return -1;
            }

            this->gatingSequenceCache.setOrder(gatingSequence);
        }

        if (this->cursor.compareAndSwap(head, next) != head) {
            // Need yiled() or sleep() a while.
            jimi_wsleep(1);
        }
        else {
            // Claim a sequence succeeds.
            break;
        }
    } while (true);

    Jimi_WriteBarrier();

    this->entries[head & kIndexMask] = entry;
    //this->entries[head & kIndexMask].copy(entry);

    publish(head);

    Jimi_WriteBarrier();
}
#endif

}  /* namespace jimi */

#endif  /* _JIMI_DISRUPTOR_RINGQUEUE_H_ */
