
#ifndef _JIMI_DISRUPTOR_RINGQUEUEEX_H_
#define _JIMI_DISRUPTOR_RINGQUEUEEX_H_

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

#define JIMI_ALIGNED_TO(n, alignment)   \
    (((n) + ((alignment) - 1)) & ~(size_t)((alignment) - 1))

namespace jimi {

///////////////////////////////////////////////////////////////////
// class DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>
///////////////////////////////////////////////////////////////////

template <typename T, typename SequenceType = int64_t, uint32_t Capacity = 1024U,
          uint32_t Producers = 0, uint32_t Consumers = 0, uint32_t NumThreads = 0>
class DisruptorRingQueueEx
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
    static const size_type  kCacheLineSize  = JIMI_ROUND_TO_POW2(JIMI_CACHELINE_SIZE);

    static const size_type  kCapacity       = (size_type)JIMI_MAX(JIMI_ROUND_TO_POW2(Capacity), 2);
    static const index_type kIndexMask      = (index_type)(kCapacity - 1);
    static const uint32_t   kIndexShift     = JIMI_POPCONUT32(kIndexMask);

    static const index_type kIndexLineSize  = (index_type)JIMI_MAX(JIMI_ROUND_TO_POW2(kCacheLineSize / sizeof(flag_type)), 1);
    static const index_type kIndexBoxSize   = (index_type)JIMI_MAX(JIMI_ROUND_TO_POW2((kCapacity + kIndexLineSize - 1) / kIndexLineSize), 1);

    static const size_type  kEntryBlockSize     = JIMI_ALIGNED_TO(sizeof(T), kCacheLineSize);
    static const size_type  kEntryAlignment     = kCacheLineSize;
    static const size_t     kEntryAlignMask     = (~((size_t)kEntryAlignment - 1));
    static const size_type  kEntryAlignPadding  = kEntryAlignment - 1;

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

    struct EntryBlock_t
    {
        item_type   data;
        char        padding[kEntryBlockSize - sizeof(item_type)];
    };
    typedef struct EntryBlock_t block_type;

public:
    DisruptorRingQueueEx(bool bFillQueue = true);
    ~DisruptorRingQueueEx();

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

    int push(T const & entry);
    int pop (T & entry, PopThreadStackData & data);

    sequence_type waitFor(sequence_type sequence);

protected:
    Sequence        cursor, workSequence;
    Sequence        gatingSequences[kConsumersAlloc];
    Sequence        gatingSequenceCache;
    Sequence        gatingSequenceCaches[kProducersAlloc];

    block_type *    entries;
    flag_type *     availableBuffer;
    item_type *     entriesAlloc;
    flag_type *     availableBufferAlloc;
};

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::DisruptorRingQueueEx(bool bFillQueue /* = true */)
    : entries(NULL), availableBuffer(NULL), entriesAlloc(NULL), availableBufferAlloc(NULL),
      cursor(Sequence::INITIAL_CURSOR_VALUE), workSequence(Sequence::INITIAL_CURSOR_VALUE),
      gatingSequenceCache(Sequence::INITIAL_CURSOR_VALUE)
{
    init(bFillQueue);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::~DisruptorRingQueueEx()
{
    // If the queue is allocated on system heap, release them.
    if (kIsAllocOnHeap) {
        if (this->availableBufferAlloc) {
            delete [] this->availableBufferAlloc;
            this->availableBuffer = NULL;
            this->availableBufferAlloc = NULL;
        }

        if (this->entriesAlloc != NULL) {
            delete [] this->entriesAlloc;
            this->entries = NULL;
            this->entriesAlloc = NULL;
        }
    }
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
void DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::init(bool bFillQueue /* = true */)
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
void DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::init_queue(bool bFillQueue /* = true */)
{
    assert(kEntryBlockSize >= sizeof(item_type));
    item_type * newEntriesAlloc = (item_type *)::malloc(kEntryBlockSize * kCapacity + kEntryAlignPadding);
    if (newEntriesAlloc != NULL) {
        block_type * newEntries = reinterpret_cast<block_type *>(reinterpret_cast<uintptr_t>(
            reinterpret_cast<char *>(newEntriesAlloc) + kEntryAlignPadding) & (uintptr_t)kEntryAlignMask);
        if (bFillQueue) {
            ::memset((void *)newEntries, 0, kEntryBlockSize * kCapacity);
        }
        Jimi_MemoryBarrier();
        //Jimi_WriteCompilerBarrier();
        this->entries = newEntries;
        this->entriesAlloc = newEntriesAlloc;
    }

    flag_type * newBufferAlloc = (flag_type *)::malloc(kIndexBoxSize * kIndexLineSize * sizeof(flag_type) + kEntryAlignPadding);
    if (newBufferAlloc != NULL) {
         flag_type * newBufferData = reinterpret_cast<flag_type *>(reinterpret_cast<uintptr_t>(
                reinterpret_cast<char *>(newBufferAlloc) + kEntryAlignPadding) & (uintptr_t)kEntryAlignMask);
        if (bFillQueue) {
            for (unsigned i = 0; i < (kIndexBoxSize * kIndexLineSize); ++i) {
                newBufferData[i] = (flag_type)(-1);
            }
        }
        Jimi_MemoryBarrier();
        //Jimi_WriteCompilerBarrier();
        this->availableBuffer = newBufferData;
        this->availableBufferAlloc = newBufferAlloc;
    }
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
void DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::dump()
{
    //ReleaseUtils::dump(&core, sizeof(core));
    dump_memory(this, sizeof(*this), false, 16, 0, 0);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
void DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::dump_detail()
{
    printf("---------------------------------------------------------\n");
    printf("DisruptorRingQueueEx: (head = %llu, tail = %llu)\n",
           this->cursor.get(), this->workSequence.get());
    printf("---------------------------------------------------------\n");

    printf("\n");
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
typename DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::size_type
DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::sizes() const
{
    sequence_type head, tail;

    Jimi_ReadCompilerBarrier();

    head = this->cursor.get();
    tail = this->workSequence.get();

    return (size_type)((head - tail) <= kIndexMask) ? (head - tail) : (size_type)(-1);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
void DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::start()
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
void DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::shutdown(int32_t timeOut /* = -1 */)
{
    // TODO: do shutdown procedure
}

/* static */
template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
typename DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::sequence_type
DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::
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
void DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::publish(sequence_type sequence)
{
    Jimi_WriteCompilerBarrier();

    setAvailable(sequence);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
void DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::setAvailable(sequence_type sequence)
{
    index_type index = (index_type)((index_type)sequence &  kIndexMask);
    flag_type  flag  = (flag_type) (            sequence >> kIndexShift);

    if (kIsAllocOnHeap) {
        assert(this->availableBuffer != NULL);
    }
    index_type newIndex = (index & (kIndexBoxSize - 1)) * kIndexLineSize + (index / kIndexBoxSize);
    Jimi_WriteCompilerBarrier();
    this->availableBuffer[newIndex] = flag;
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
bool DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::isAvailable(sequence_type sequence)
{
    index_type index = (index_type)((index_type)sequence &  kIndexMask);
    flag_type  flag  = (flag_type) (            sequence >> kIndexShift);

    index_type newIndex = (index & (kIndexBoxSize - 1)) * kIndexLineSize + (index / kIndexBoxSize);
    flag_type  flagValue = this->availableBuffer[newIndex];
    Jimi_ReadCompilerBarrier();
    return (flagValue == flag);
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
typename DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::sequence_type
DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::
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
typename DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::Sequence *
DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::getGatingSequences(int index)
{
    if (index >= 0 && index < kCapacity) {
        return &this->gatingSequences[index];
    }
    return NULL;
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
int DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::push(T const & entry)
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
            sequence_type gatingSequence = DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>
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

    this->entries[nextSequence & kIndexMask].data = entry;
    //this->entries[nextSequence & kIndexMask].copy(entry);

    Jimi_WriteCompilerBarrier();

    publish(nextSequence);

    Jimi_WriteCompilerBarrier();
    return 0;
}

template <typename T, typename SequenceType, uint32_t Capacity, uint32_t Producers, uint32_t Consumers, uint32_t NumThreads>
inline
int DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::pop(T & entry, PopThreadStackData & data)
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
                    Jimi_ReadCompilerBarrier();
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
            entry = this->entries[data.nextSequence & kIndexMask].data;

            Jimi_ReadCompilerBarrier();
            //data.tailSequence->set(data.nextSequence);
            data.processedSequence = true;

            Jimi_ReadCompilerBarrier();
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
typename DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::sequence_type
DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>::waitFor(sequence_type sequence)
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
            sequence_type gatingSequence = DisruptorRingQueueEx<T, SequenceType, Capacity, Producers, Consumers, NumThreads>
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

    Jimi_WriteCompilerBarrier();

    this->entries[head & kIndexMask] = entry;
    //this->entries[head & kIndexMask].copy(entry);

    publish(head);

    Jimi_WriteCompilerBarrier();
}
#endif

}  /* namespace jimi */

#endif  /* _JIMI_DISRUPTOR_RINGQUEUEEX_H_ */
