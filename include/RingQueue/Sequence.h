
#ifndef _JIMI_SEQUENCE_H_
#define _JIMI_SEQUENCE_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include "test.h"
#include "port.h"

#include "vs_stdint.h"
#include "sleep.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(USE_64BIT_SEQUENCE) && (USE_64BIT_SEQUENCE != 0)
typedef uint64_t sequence_t;
#else
typedef uint32_t sequence_t;
#endif

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(push)
#pragma pack(1)
#endif

struct CACHE_ALIGN_PREFIX seqence_c32
{
    volatile uint32_t   value;
    char                padding1[JIMI_CACHE_LINE_SIZE - sizeof(uint32_t) * 1];
} CACHE_ALIGN_SUFFIX;

struct CACHE_ALIGN_PREFIX seqence_c64
{
    volatile uint64_t   value;
    char                padding1[JIMI_CACHE_LINE_SIZE - sizeof(uint32_t) * 1];
} CACHE_ALIGN_SUFFIX;

#if defined(USE_64BIT_SEQUENCE) && (USE_64BIT_SEQUENCE != 0)
typedef struct seqence_c64 seqence_c;
#else
typedef struct seqence_c32 seqence_c;
#endif

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif

// Class Sequence() use in C++ only
#ifdef __cplusplus

// Use a spinlock for multi-word accesses
class seq_spinlock
{
    static volatile uint32_t lock;

public:
    seq_spinlock()
    {
        while (jimi_val_compare_and_swap32(&lock, 0, 1) != 0) {
            // Thread.Yield()
            jimi_wsleep(0);
        }
    }

    ~seq_spinlock()
    {
        Jimi_ReadWriteBarrier();
        lock = 0;
    }
};
  
// This is a single lock that is used for all synchronized accesses if
// the compiler can't generate inline compare-and-swap operations.  In
// most cases it'll never be used, but the i386 needs it for 64-bit
// locked accesses and so does PPC32.  It's worth building libgcj with
// target=i486 (or above) to get the inlines.
volatile uint32_t seq_spinlock::lock = 0U;

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(push)
#pragma pack(1)
#endif

template <typename T>
class CACHE_ALIGN_PREFIX SequenceBase
{
public:
    static const uint32_t kSizeOfInt32 = sizeof(uint32_t);
    static const uint32_t kSizeOfInt64 = sizeof(uint64_t);
    static const uint32_t kSizeOfValue = sizeof(T);

    static const T INITIAL_VALUE        = static_cast<T>(0);
    static const T INITIAL_CURSOR_VALUE = static_cast<T>(0);

public:
    SequenceBase() : value(INITIAL_CURSOR_VALUE) {
        init(INITIAL_CURSOR_VALUE);
    }
    SequenceBase(T initial_val) : value(initial_val) {
        init(initial_val);
    }
    ~SequenceBase() {}

public:
    void init(T initial_val) {
#if 0
        if (sizeof(padding) > 0) {
            memset(&padding[0], 0, sizeof(padding));
        }
#elif 0
        const uint32_t kAlignTo4Bytes = (sizeof(T) + (kSizeOfInt32 - 1)) & (~(kSizeOfInt32 - 1));
        const int32_t fill_size = (sizeof(padding) > kAlignTo4Bytes)
                    ? (kAlignTo4Bytes - sizeof(T)) : (sizeof(padding) - sizeof(T));
        if (fill_size > 0) {
            memset(&padding[0], 0, fill_size);
        }
#else
        if (sizeof(T) > sizeof(uint32_t)) {
            *(uint64_t *)(&this->value) = (uint64_t)initial_val;
        }
        else {
            *(uint32_t *)(&this->value) = (uint32_t)initial_val;
        }
#endif
    }

    T get() const {
        T val = value;
        Jimi_ReadBarrier();
        //Jimi_MemoryBarrier();
        return val;
    }

    void set(T newValue) {
#if 0
        T oldValue;
        int cnt = 0;
        Jimi_WriteBarrier();
        Jimi_MemoryBarrier();
        Jimi_ReadWriteBarrier();
        oldValue = this->value;
        do {
            // Loop until the update is successful.
            T nowValue = this->value;
            if ((int)(nowValue - newValue) > 0) {
#ifdef _DEBUG
                cnt++;
                if (cnt >= 1)
                    break;
#else
                break;
#endif
            }
        } while (jimi_val_compare_and_swap32(&(this->value), oldValue, newValue) != oldValue);
#else
        Jimi_WriteBarrier();
        seq_spinlock spinlock;
        this->value = newValue;
#endif
    }

    T getVolatile() const {
        T val = value;
        Jimi_ReadBarrier();
        return val;
    }

    void setVolatile(T newValue) {
        Jimi_WriteBarrier();
        //seq_spinlock spinlock;
        this->value = newValue;
    }

    T compareAndSwap(T oldValue, T newValue) {
        Jimi_WriteBarrier();
        //seq_spinlock spinlock;
        return jimi_val_compare_and_swap32(&(this->value), oldValue, newValue);
    }

    bool compareAndSwapBool(T oldValue, T newValue) {
        Jimi_WriteBarrier();
        seq_spinlock spinlock;
        return (jimi_val_compare_and_swap32(&(this->value), oldValue, newValue) == oldValue);
    }

protected:
    volatile T  value;
    char        padding[(JIMI_CACHE_LINE_SIZE >= sizeof(T))
                      ? (JIMI_CACHE_LINE_SIZE - sizeof(T))
                      : (JIMI_CACHE_LINE_SIZE)];
} CACHE_ALIGN_SUFFIX;

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(pop)
#endif

template <>
void SequenceBase<int64_t>::set(int64_t newValue) {
#if 1
    Jimi_WriteBarrier();
    int64_t oldValue;
    // Loop until the update is successful.
    do {
        oldValue = this->value;
    } while (jimi_val_compare_and_swap64(&(this->value), oldValue, newValue) != oldValue);
#else
    Jimi_WriteBarrier();
    this->value = newValue;
#endif
}

template <>
void SequenceBase<uint64_t>::set(uint64_t newValue) {
#if 1
    Jimi_WriteBarrier();
    uint64_t oldValue;
    // Loop until the update is successful.
    do {
        oldValue = this->value;
    } while (jimi_val_compare_and_swap64(&(this->value), oldValue, newValue) != oldValue);
#else
    Jimi_WriteBarrier();
    this->value = newValue;
#endif
}

template <>
uint64_t SequenceBase<uint64_t>::compareAndSwap(uint64_t oldValue, uint64_t newValue) {
    Jimi_WriteBarrier();
    return jimi_val_compare_and_swap64(&(this->value), oldValue, newValue);
}

template <>
bool SequenceBase<uint64_t>::compareAndSwapBool(uint64_t oldValue, uint64_t newValue) {
    Jimi_WriteBarrier();
    return (jimi_val_compare_and_swap64(&(this->value), oldValue, newValue) == oldValue);
}

typedef SequenceBase<uint64_t> Sequence64;
typedef SequenceBase<uint32_t> Sequence32;
typedef SequenceBase<uint16_t> Sequence16;
typedef SequenceBase<uint8_t>  Sequence8;

#if defined(USE_64BIT_SEQUENCE) && (USE_64BIT_SEQUENCE != 0)
typedef SequenceBase<uint64_t> Sequence;
#else
typedef SequenceBase<uint32_t> Sequence;
#endif

#endif  /* __cplusplus */

#endif  /* _JIMI_SEQUENCE_H_ */
