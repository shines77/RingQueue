
#ifndef _JIMI_SEQUENCE_H_
#define _JIMI_SEQUENCE_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#define __STDC_LIMIT_MACROS     // for INT64_MAX

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
    char                padding1[JIMI_CACHELINE_SIZE - sizeof(uint32_t) * 1];
} CACHE_ALIGN_SUFFIX;

struct CACHE_ALIGN_PREFIX seqence_c64
{
    volatile uint64_t   value;
    char                padding1[JIMI_CACHELINE_SIZE - sizeof(uint32_t) * 1];
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

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(push)
#pragma pack(1)
#endif

#if defined(USE_SEQUENCE_SPIN_LOCK) && (USE_SEQUENCE_SPIN_LOCK != 0)

#define SEQUENCE_YILED_THRESHOLD        1

template <typename T>
class CACHE_ALIGN_PREFIX seq_spinlock_core
{
public:
    static volatile CACHE_ALIGN_PREFIX T lock;
    static char padding[(JIMI_CACHELINE_SIZE >= sizeof(T))
                      ? (JIMI_CACHELINE_SIZE - sizeof(T))
                      : ((sizeof(T) - JIMI_CACHELINE_SIZE) & (JIMI_CACHELINE_SIZE - 1))];
} CACHE_ALIGN_SUFFIX;

// This is a single lock that is used for all synchronized accesses if
// the compiler can't generate inline compare-and-swap operations.  In
// most cases it'll never be used, but the i386 needs it for 64-bit
// locked accesses and so does PPC32.  It's worth building libgcj with
// target=i486 (or above) to get the inlines.

template <typename T>
volatile CACHE_ALIGN_PREFIX T seq_spinlock_core<T>::lock = static_cast<T>(0);

template <typename T>
char seq_spinlock_core<T>::padding[(JIMI_CACHELINE_SIZE >= sizeof(T))
                                 ? (JIMI_CACHELINE_SIZE - sizeof(T))
                                 : (((sizeof(T) - JIMI_CACHELINE_SIZE) & (JIMI_CACHELINE_SIZE - 1)))] = { 0 };

// Use a spinlock for multi-word accesses
template <typename T>
class seq_spinlock
{
public:
    static const uintptr_t kYieldThreshold = SEQUENCE_YILED_THRESHOLD;

    typedef seq_spinlock_core<T> core_type;

public:
    seq_spinlock()
    {
        Jimi_ReadCompilerBarrier();
        spin_wait();
    }

    ~seq_spinlock()
    {
        Jimi_WriteCompilerBarrier();
        core_type::lock = 0;
    }

private:
    void spin_wait()
    {
        intptr_t pause_cnt;
        uintptr_t loop_cnt, yeild_cnt, spin_cnt;
        loop_cnt = 0;
        spin_cnt = 1;
        while (jimi_val_compare_and_swap32(&core_type::lock, 0, 1) != 0) {
            // Thread.Yield()
            if (loop_cnt >= kYieldThreshold) {
                yeild_cnt = loop_cnt - kYieldThreshold;
                if ((yeild_cnt & 63) == 63) {
                    jimi_wsleep(1);
                }
                else if ((yeild_cnt & 3) == 3) {
                    jimi_wsleep(0);
                }
                else {
                    if (!jimi_yield()) {
                        jimi_wsleep(0);
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
    }
};

template <>
void seq_spinlock<int64_t>::spin_wait()
{
    intptr_t pause_cnt;
    uintptr_t loop_cnt, yeild_cnt, spin_cnt;
    loop_cnt = 0;
    spin_cnt = 1;
    while (jimi_val_compare_and_swap64(&core_type::lock, 0, 1) != 0) {
        // Thread.Yield()
        if (loop_cnt >= kYieldThreshold) {
            yeild_cnt = loop_cnt - kYieldThreshold;
            if ((yeild_cnt & 63) == 63) {
                jimi_wsleep(1);
            }
            else if ((yeild_cnt & 3) == 3) {
                jimi_wsleep(0);
            }
            else {
                if (!jimi_yield()) {
                    jimi_wsleep(0);
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
}

template <>
void seq_spinlock<uint64_t >::spin_wait()
{
    intptr_t pause_cnt;
    uintptr_t loop_cnt, yeild_cnt, spin_cnt;
    loop_cnt = 0;
    spin_cnt = 1;
    while (jimi_val_compare_and_swap64u(&core_type::lock, 0, 1) != 0) {
        // Thread.Yield()
        if (loop_cnt >= kYieldThreshold) {
            yeild_cnt = loop_cnt - kYieldThreshold;
            if ((yeild_cnt & 63) == 63) {
                jimi_wsleep(1);
            }
            else if ((yeild_cnt & 3) == 3) {
                jimi_wsleep(0);
            }
            else {
                if (!jimi_yield()) {
                    jimi_wsleep(0);
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
}

#else  /* !USE_SEQUENCE_SPIN_LOCK */

// This is a empty class
template <typename T>
class seq_spinlock {
public:
    seq_spinlock()  {}
    ~seq_spinlock() {}
};

#endif  /* USE_SEQUENCE_SPIN_LOCK */

#if defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) || defined(_M_IA64) || defined(__amd64__) || defined(__x86_64__)
typedef seq_spinlock<uint64_t> seq_spinlock_t;
#else
typedef seq_spinlock<uint32_t> seq_spinlock_t;
#endif

template <typename T>
class CACHE_ALIGN_PREFIX SequenceBase
{
public:
    static const uint32_t kSizeOfInt32 = sizeof(uint32_t);
    static const uint32_t kSizeOfInt64 = sizeof(uint64_t);
    static const uint32_t kSizeOfValue = sizeof(T);

    static const T INITIAL_VALUE        = static_cast<T>(0);
    static const T INITIAL_CURSOR_VALUE = static_cast<T>(-1);

    static const T MIN_VALUE;
    static const T MAX_VALUE;

    static const T kMinSequenceValue;
    static const T kMaxSequenceValue;

protected:
    volatile T  value;
    char        padding[(JIMI_CACHELINE_SIZE >= sizeof(T))
                      ? (JIMI_CACHELINE_SIZE - sizeof(T))
                      : ((sizeof(T) - JIMI_CACHELINE_SIZE) & (JIMI_CACHELINE_SIZE - 1))];

public:
    SequenceBase() : value(INITIAL_CURSOR_VALUE) {
        init(INITIAL_CURSOR_VALUE);
    }
    SequenceBase(T initial_val) : value(initial_val) {
        init(initial_val);
    }
    ~SequenceBase() {}

public:
    inline void init(T initial_val) {
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

    void setMinValue() {
        setOrder(kMinSequenceValue);
    }

    void setMaxValue() {
        setOrder(kMaxSequenceValue);
    }

    inline T get() const {
        T val = value;
        Jimi_ReadCompilerBarrier();
        return val;
    }

    inline void set(T newValue) {
#if 0
        T oldValue;
        int cnt = 0;
        Jimi_WriteCompilerBarrier();
        Jimi_MemoryBarrier();
        Jimi_CompilerBarrier();
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
        Jimi_WriteCompilerBarrier();
        this->value = newValue;
#endif
    }

    inline T getOrder() const {
        T val = value;
        Jimi_ReadCompilerBarrier();
        return val;
    }

    inline void setOrder(T newValue) {
        Jimi_WriteCompilerBarrier();
        this->value = newValue;
    }

    inline T getVolatile() const {
        T val = value;
        Jimi_ReadCompilerBarrier();
        return val;
    }

    inline void setVolatile(T newValue) {
        Jimi_WriteCompilerBarrier();
        seq_spinlock_t spinlock;
        this->value = newValue;
    }

    inline T compareAndSwap(T oldValue, T newValue) {
        Jimi_WriteCompilerBarrier();
#if defined(USE_SEQUENCE_SPIN_LOCK) && (USE_SEQUENCE_SPIN_LOCK != 0)
        seq_spinlock_t spinlock;
        return __internal_val_compare_and_swap32(&(this->value), oldValue, newValue);
#else
        return jimi_val_compare_and_swap32(&(this->value), oldValue, newValue);
#endif
    }

    inline bool compareAndSwapBool(T oldValue, T newValue) {
        Jimi_WriteCompilerBarrier();
#if defined(USE_SEQUENCE_SPIN_LOCK) && (USE_SEQUENCE_SPIN_LOCK != 0)
        seq_spinlock_t spinlock;
        return (__internal_val_compare_and_swap32(&(this->value), oldValue, newValue) == oldValue);
#else
        return jimi_val_compare_and_swap32(&(this->value), oldValue, newValue);
#endif
    }

} CACHE_ALIGN_SUFFIX;

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(pop)
#endif

/* Special define for MIN_SEQUENCE_VALUE and MAX_SEQUENCE_VALUE. */

template <typename T>
const T SequenceBase<T>::MIN_VALUE                  = static_cast<T>(0);
template <typename T>
const T SequenceBase<T>::MAX_VALUE                  = static_cast<T>(UINT32_MAX);

template <>
const int32_t SequenceBase<int32_t>::MIN_VALUE      = INT32_MIN;
template <>
const int32_t SequenceBase<int32_t>::MAX_VALUE      = INT32_MAX;

template <>
const uint32_t SequenceBase<uint32_t>::MIN_VALUE    = 0U;
template <>
const uint32_t SequenceBase<uint32_t>::MAX_VALUE    = UINT32_MAX;

template <>
const int64_t SequenceBase<int64_t>::MIN_VALUE      = INT64_MIN;
template <>
const int64_t SequenceBase<int64_t>::MAX_VALUE      = INT64_MAX;

template <>
const uint64_t SequenceBase<uint64_t>::MIN_VALUE    = 0ULL;
template <>
const uint64_t SequenceBase<uint64_t>::MAX_VALUE    = UINT64_MAX;

template <typename T>
const T SequenceBase<T>::kMinSequenceValue          = static_cast<T>(SequenceBase<T>::MIN_VALUE);
template <typename T>
const T SequenceBase<T>::kMaxSequenceValue          = static_cast<T>(SequenceBase<T>::MAX_VALUE);

/* For getOrder(), int64_t and uint64_t */

template <>
inline
int64_t SequenceBase<int64_t>::getOrder() const
{
    int64_t val;
#if defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) || defined(_M_IA64) || defined(__amd64__) || defined(__x86_64__)
    Jimi_ReadCompilerBarrier();
    val = this->value;
#else
  #if defined(USE_SEQUENCE_SPIN_LOCK) && (USE_SEQUENCE_SPIN_LOCK != 0)
    seq_spinlock_t spinlock;
    Jimi_ReadCompilerBarrier();
    val = this->value;
  #else
    int64_t oldValue = this->value;
    Jimi_ReadCompilerBarrier();
    val = jimi_val_compare_and_swap64(&(this->value), oldValue, oldValue);
  #endif  /* USE_SEQUENCE_SPIN_LOCK */
#endif  /* _M_X64 */
    return val;
}

template <>
inline
uint64_t SequenceBase<uint64_t>::getOrder() const
{
    uint64_t val;
#if defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) || defined(_M_IA64) || defined(__amd64__) || defined(__x86_64__)
    Jimi_ReadCompilerBarrier();
    val = this->value;
#else
  #if defined(USE_SEQUENCE_SPIN_LOCK) && (USE_SEQUENCE_SPIN_LOCK != 0)
    seq_spinlock_t spinlock;
    Jimi_ReadCompilerBarrier();
    val = this->value;
  #else
    uint64_t oldValue = this->value;
    Jimi_ReadCompilerBarrier();
    val = jimi_val_compare_and_swap64u(&(this->value), oldValue, oldValue);
  #endif  /* USE_SEQUENCE_SPIN_LOCK */
#endif  /* _M_X64 */
    return val;
}

/* For setOrder(), int64_t and uint64_t */

template <>
inline
void SequenceBase<int64_t>::setOrder(int64_t newValue)
{
    Jimi_WriteCompilerBarrier();
#if defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) || defined(_M_IA64) || defined(__amd64__) || defined(__x86_64__)
    this->value = newValue;
#else
  #if defined(USE_SEQUENCE_SPIN_LOCK) && (USE_SEQUENCE_SPIN_LOCK != 0)
    seq_spinlock_t spinlock;
    this->value = newValue;
  #else
    int64_t oldValue;
    intptr_t loop_cnt, spin_cnt;
    loop_cnt = 1;
    spin_cnt = 1;
    do {
        oldValue = this->value;
        if (jimi_lock_test_and_set64(&(this->value), newValue) == oldValue)
            break;
        if (loop_cnt < 0) {
            if (!jimi_yield())
                jimi_wsleep(0);
        }
        else {
            for (int i = spin_cnt; i > 0; --i)
                jimi_mm_pause();
            spin_cnt++;
        }
        --loop_cnt;
    } while (1);
  #endif  /* USE_SEQUENCE_SPIN_LOCK */
#endif  /* _M_X64 */
}

template <>
inline
void SequenceBase<uint64_t>::setOrder(uint64_t newValue)
{
    Jimi_WriteCompilerBarrier();
#if defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) || defined(_M_IA64) || defined(__amd64__) || defined(__x86_64__)
    this->value = newValue;
#else
  #if defined(USE_SEQUENCE_SPIN_LOCK) && (USE_SEQUENCE_SPIN_LOCK != 0)
    seq_spinlock_t spinlock;
    this->value = newValue;
  #else
    uint64_t oldValue;
    intptr_t loop_cnt, spin_cnt;
    loop_cnt = 1;
    spin_cnt = 1;
    do {
        oldValue = this->value;
        if (jimi_lock_test_and_set64u(&(this->value), newValue) == oldValue)
            break;
        if (loop_cnt < 0) {
            if (!jimi_yield())
                jimi_wsleep(0);
        }
        else {
            for (int i = spin_cnt; i > 0; --i)
                jimi_mm_pause();
            spin_cnt++;
        }
        --loop_cnt;
    } while (1);
  #endif  /* USE_SEQUENCE_SPIN_LOCK */
#endif  /* _M_X64 */
}

template <>
inline
uint32_t SequenceBase<uint32_t>::compareAndSwap(uint32_t oldValue, uint32_t newValue) 
{
    Jimi_WriteCompilerBarrier();
#if defined(USE_SEQUENCE_SPIN_LOCK) && (USE_SEQUENCE_SPIN_LOCK != 0)
    seq_spinlock_t spinlock;
    return (uint32_t)__internal_val_compare_and_swap32u(&(this->value), oldValue, newValue);
#else
    return jimi_val_compare_and_swap32u(&(this->value), oldValue, newValue);
#endif
}

/* For compareAndSwap() and compareAndSwapBool, int64_t */

template <>
inline
int64_t SequenceBase<int64_t>::compareAndSwap(int64_t oldValue, int64_t newValue)
{
    Jimi_WriteCompilerBarrier();
#if defined(USE_SEQUENCE_SPIN_LOCK) && (USE_SEQUENCE_SPIN_LOCK != 0)
    seq_spinlock_t spinlock;
    return (int64_t)__internal_val_compare_and_swap64(&(this->value), oldValue, newValue);
#else
    return jimi_val_compare_and_swap64(&(this->value), oldValue, newValue);
#endif
}

template <>
inline
bool SequenceBase<int64_t>::compareAndSwapBool(int64_t oldValue, int64_t newValue)
{
    Jimi_WriteCompilerBarrier();
    seq_spinlock_t spinlock;
    return (jimi_val_compare_and_swap64(&(this->value), oldValue, newValue) == oldValue);
}

/* For compareAndSwap() and compareAndSwapBool, uint64_t */

template <>
inline
uint64_t SequenceBase<uint64_t>::compareAndSwap(uint64_t oldValue, uint64_t newValue)
{
    Jimi_WriteCompilerBarrier();
#if defined(USE_SEQUENCE_SPIN_LOCK) && (USE_SEQUENCE_SPIN_LOCK != 0)
    seq_spinlock_t spinlock;
    return __internal_val_compare_and_swap64u(&(this->value), oldValue, newValue);
#else
    return jimi_val_compare_and_swap64u(&(this->value), oldValue, newValue);
#endif
}

template <>
inline
bool SequenceBase<uint64_t>::compareAndSwapBool(uint64_t oldValue, uint64_t newValue)
{
    Jimi_WriteCompilerBarrier();
    seq_spinlock_t spinlock;
    return (jimi_val_compare_and_swap64u(&(this->value), oldValue, newValue) == oldValue);
}

typedef SequenceBase<uint64_t>  SequenceU64;
typedef SequenceBase<uint32_t>  SequenceU32;
typedef SequenceBase<uint16_t>  SequenceU16;
typedef SequenceBase<uint8_t>   SequenceU8;

typedef SequenceBase<int64_t>   Sequence64;
typedef SequenceBase<int32_t>   Sequence32;
typedef SequenceBase<int16_t>   Sequence16;
typedef SequenceBase<int8_t>    Sequence8;

#if defined(USE_64BIT_SEQUENCE) && (USE_64BIT_SEQUENCE != 0)
typedef SequenceBase<uint64_t> SequenceStd;
#else
typedef SequenceBase<uint32_t> SequenceStd;
#endif

typedef SequenceBase<int64_t> Sequence;

#endif  /* __cplusplus */

#endif  /* _JIMI_SEQUENCE_H_ */
