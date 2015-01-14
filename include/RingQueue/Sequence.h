
#ifndef _JIMI_SEQUENCE_H_
#define _JIMI_SEQUENCE_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include "vs_stdint.h"
#include "test.h"

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

// Class Sequence() use in c++ only
#ifdef __cplusplus

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(push)
#pragma pack(1)
#endif

template <typename T>
class CACHE_ALIGN_PREFIX SequenceBase
{
public:
    static const T  INITIAL_VALUE        = static_cast<T>(-1);
    static const T  INITIAL_ZERO_VALUE   = static_cast<T>(0);
    static const T  INITIAL_CURSOR_VALUE = static_cast<T>(-1);

public:
    SequenceBase() : value(INITIAL_VALUE) { memset(&padding1[0], 0, sizeof(padding1)); }
    SequenceBase(T initize_val) : value(initize_val) {}
    ~SequenceBase() {}

public:
    T           get()        { return value;        }
    void        set(T value) { this->value = value; };

    volatile T  getVolatile()                 { return value;        }
    void        setVolatile(volatile T value) { this->value = value; };

protected:
    volatile T  value;
    char        padding1[JIMI_CACHE_LINE_SIZE - sizeof(T) * 1];
} CACHE_ALIGN_SUFFIX;

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(pop)
#endif

typedef SequenceBase<uint32_t> Sequence32;
typedef SequenceBase<uint64_t> Sequence64;

#if defined(USE_64BIT_SEQUENCE) && (USE_64BIT_SEQUENCE != 0)
typedef SequenceBase<uint64_t> Sequence;
#else
typedef SequenceBase<uint32_t> Sequence;
#endif

#endif  /* __cplusplus */

#endif  /* _JIMI_SEQUENCE_H_ */
