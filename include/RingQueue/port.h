
#ifndef _JIMIC_PORT_H_
#define _JIMIC_PORT_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include "vs_stdint.h"
#include "vs_stdbool.h"
#include <emmintrin.h>

#ifndef jimi_mm_pause
#define jimi_mm_pause       _mm_pause
#endif

#if defined(_MSC_VER)

#ifndef jimi_likely
#define jimi_likely(x)      (x)
#endif

#ifndef jimi_unlikely
#define jimi_unlikely(x)    (x)
#endif

#ifndef JIMIC_INLINE
#define JIMIC_INLINE        __inline
#endif

///
/// _ReadWriteBarrier
///
/// See: http://msdn.microsoft.com/en-us/library/f20w0x5e%28VS.80%29.aspx
///
/// See: http://en.wikipedia.org/wiki/Memory_ordering
///
#define Jimi_ReadWriteBarrier()  _ReadWriteBarrier()

#else  /* !_MSC_VER */

#ifndef jimi_likely
#define jimi_likely(x)      __builtin_expect((x), 1)
#endif

#ifndef jimi_unlikely
#define jimi_unlikely(x)    __builtin_expect((x), 0)
#endif

#ifndef JIMIC_INLINE
#define JIMIC_INLINE        inline
#endif

///
/// See: http://en.wikipedia.org/wiki/Memory_ordering
///
/// See: http://bbs.csdn.net/topics/310025520
///

//#define Jimi_ReadWriteBarrier()     asm volatile ("":::"memory");
#define Jimi_ReadWriteBarrier()     __asm__ __volatile__ ("":::"memory");

#endif  /* _MSC_VER */

#if defined(_MSC_VER)

#include <windows.h>
#include <intrin.h>

#define jimi_val_compare_and_swap32(destPtr, oldValue, newValue)        \
    InterlockedCompareExchange((volatile LONG *)(destPtr),              \
                            (uint32_t)(newValue), (uint32_t)(oldValue))

#define jimi_bool_compare_and_swap32(destPtr, oldValue, newValue)       \
    (InterlockedCompareExchange((volatile LONG *)(destPtr),             \
                            (uint32_t)(newValue), (uint32_t)(oldValue)) \
                                == (uint32_t)(oldValue))
#define jimi_lock_test_and_set32(destPtr, newValue)                     \
    InterlockedExchange((volatile LONG *)(destPtr), (uint32_t)(newValue))

#elif defined(__linux__) || defined(__CYGWIN__) || defined(__MINGW__) || defined(__MINGW32__)
#define jimi_val_compare_and_swap32(destPtr, oldValue, newValue)        \
    __sync_val_compare_and_swap((volatile uint32_t *)(destPtr),         \
                            (uint32_t)(oldValue), (uint32_t)(newValue))

#define jimi_bool_compare_and_swap32(destPtr, oldValue, newValue)       \
    __sync_bool_compare_and_swap((volatile uint32_t *)(destPtr),        \
                            (uint32_t)(oldValue), (uint32_t)(newValue))

#define jimi_lock_test_and_set32(destPtr, newValue)                     \
    __sync_lock_test_and_set((volatile uint32_t *)(destPtr), (uint32_t)(newValue))

#else
#define jimi_val_compare_and_swap32(destPtr, oldValue, newValue)        \
    __internal_val_compare_and_swap32((volatile uint32_t *)(destPtr),   \
                                (uint32_t)(oldValue), (uint32_t)(newValue))

#define jimi_bool_compare_and_swap32(destPtr, oldValue, newValue)       \
    __internal_bool_compare_and_swap32((volatile uint32_t *)(destPtr),  \
                                (uint32_t)(oldValue), (uint32_t)(newValue))

#define jimi_lock_test_and_set32(destPtr, newValue)                     \
    __internal_lock_test_and_set32((volatile uint32_t *)(destPtr),      \
                                (uint32_t)(newValue))
#endif  /* _MSC_VER */

#ifdef __cplusplus
extern "C" {
#endif

static JIMIC_INLINE
uint32_t __internal_val_compare_and_swap32(volatile uint32_t *destPtr,
                                           uint32_t oldValue,
                                           uint32_t newValue)
{
    uint32_t origValue = *destPtr;
    if (*destPtr == oldValue) {
        *destPtr = newValue;
    }
    return origValue;
}

static JIMIC_INLINE
bool __internal_bool_compare_and_swap32(volatile uint32_t *destPtr,
                                        uint32_t oldValue,
                                        uint32_t newValue)
{
    if (*destPtr == oldValue) {
        *destPtr = newValue;
        return 1;
    }
    else
        return 0;
}

static JIMIC_INLINE
uint32_t __internal_lock_test_and_set32(volatile uint32_t *destPtr,
                                        uint32_t newValue)
{
    uint32_t origValue = *destPtr;
    *destPtr = newValue;
    return origValue;
}

#ifdef __cplusplus
}
#endif

#endif  /* _JIMIC_PORT_H_ */
