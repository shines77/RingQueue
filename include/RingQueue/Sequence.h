
#ifndef _JIMI_SEQUENCE_H_
#define _JIMI_SEQUENCE_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include "vs_stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_M_X64) || defined(_M_AMD64) || defined(_M_IA64)
typedef uint64_t sequence_t;
#else
typedef uint32_t sequence_t;
#endif

#ifdef __cplusplus
}
#endif

template <typename T>
class SequenceBase
{
public:
    static const T  INITIAL_VALUE        = static_cast<T>(-1);
    static const T  INITIAL_ZERO_VALUE   = static_cast<T>(0);
    static const T  INITIAL_CURSOR_VALUE = static_cast<T>(-1);

public:
    SequenceBase() : value(INITIAL_VALUE) {}
    SequenceBase(T initize_val) : value(initize_val) {}
    ~SequenceBase() {}

public:
    T           get()        { return value;        }
    void        set(T value) { this->value = value; };

    volatile T  getVolatile()                 { return value;        }
    void        setVolatile(volatile T value) { this->value = value; };

protected:
    char        padding1[JIMI_CACHE_LINE_SIZE];
    volatile T  value;
    char        padding2[JIMI_CACHE_LINE_SIZE - sizeof(T) * 1];
};

typedef SequenceBase<uint32_t> Sequence;

typedef SequenceBase<uint32_t> Sequence32;
typedef SequenceBase<uint64_t> Sequence64;

#endif  /* _JIMI_SEQUENCE_H_ */
