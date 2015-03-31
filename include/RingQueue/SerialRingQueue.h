
#ifndef _JIMI_UTIL_SERIALRINGQUEUE_H_
#define _JIMI_UTIL_SERIALRINGQUEUE_H_

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

template <typename T, uint32_t Capacity = 1024U>
class SerialRingQueue
{
public:
    typedef T                   item_type;
    typedef item_type           value_type;
    typedef uint32_t            size_type;
    typedef uint32_t            sequence_type;
    typedef uint32_t            index_type;
    typedef T *                 pointer;
    typedef const T *           const_pointer;
    typedef T &                 reference;
    typedef const T &           const_reference;

public:
    static const bool       kIsAllocOnHeap  = true;
    static const size_type  kCapacity       = (size_type)JIMI_MAX(JIMI_ROUND_TO_POW2(Capacity), 2);
    static const index_type kMask           = (index_type)(kCapacity - 1);

public:
    SerialRingQueue();
    ~SerialRingQueue();

public:
    index_type mask() const      { return kMask;     };
    size_type capacity() const   { return kCapacity; };
    size_type length() const     { return sizes();   };
    size_type sizes() const;

    void init();

    int push(T const & entry);
    int pop(T & entry);

protected:
    sequence_type   headSequence;
    sequence_type   tailSequence;
    item_type *     entries;
};

template <typename T, uint32_t Capacity>
SerialRingQueue<T, Capacity>::SerialRingQueue()
: headSequence(0)
, tailSequence(0)
, entries(NULL)
{
    init();
}

template <typename T, uint32_t Capacity>
SerialRingQueue<T, Capacity>::~SerialRingQueue()
{
    Jimi_WriteBarrier();

    // If the queue is allocated on system heap, release them.
    if (SerialRingQueue<T, Capacity>::kIsAllocOnHeap) {
        if (this->entries != NULL) {
            delete [] this->entries;
            this->entries = NULL;
        }
    }
}

template <typename T, uint32_t Capacity>
inline
void SerialRingQueue<T, Capacity>::init()
{
    value_type * newData = new T[kCapacity];
    if (newData != NULL) {
        memset((void *)newData, 0, sizeof(value_type) * kCapacity);
        this->entries = newData;
    }
}

template <typename T, uint32_t Capacity>
inline
typename SerialRingQueue<T, Capacity>::size_type
SerialRingQueue<T, Capacity>::sizes() const
{
    sequence_type head, tail;

    Jimi_ReadBarrier();

    head = this->headSequence;
    tail = this->tailSequence;

    return (size_type)((head - tail) <= kMask) ? (head - tail) : (size_type)(-1);
}

template <typename T, uint32_t Capacity>
int SerialRingQueue<T, Capacity>::push(T const & entry)
{
    sequence_type head, tail, next;

    Jimi_ReadBarrier();
    head = this->headSequence;
    tail = this->tailSequence;
    if ((head - tail) > kMask) {
        return -1;
    }

    Jimi_WriteBarrier();
#if 0
    this->entries[((index_type)head) & kMask] = entry;
#else
    this->entries[head & (sequence_type)kMask] = entry;
#endif

    next = head + 1;

    Jimi_WriteBarrier();
    this->headSequence = next;

    return 0;
}

template <typename T, uint32_t Capacity>
int SerialRingQueue<T, Capacity>::pop(T & entry)
{
    sequence_type head, tail, next;

    Jimi_ReadBarrier();
    head = this->headSequence;
    tail = this->tailSequence;
    if ((tail == head) || (tail > head && (head - tail) > kMask)) {
        return -1;
    }

    Jimi_ReadBarrier();
#if 0
    entry = this->entries[((index_type)tail) & kMask)];
#else
    entry = this->entries[tail & (sequence_type)kMask];
#endif

    next = tail + 1;

    Jimi_ReadWriteBarrier();
    this->tailSequence = next;

    return 0;
}

}  /* namespace jimi */

#endif  /* _JIMI_UTIL_SERIALRINGQUEUE_H_ */
