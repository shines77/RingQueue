
#ifndef _JIMI_UTIL_RINGQUEUE_H_
#define _JIMI_UTIL_RINGQUEUE_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include <stdint.h>
#include <stdio.h>

#ifdef _MSC_VER
#include <intrin.h>     // For _ReadWriteBarrier(), InterlockedCompareExchange()
#endif
#include <emmintrin.h>

#ifndef JIMI_CACHELINE_SIZE
#define JIMI_CACHELINE_SIZE     64
#endif

#ifndef jimi_mm_pause
#define jimi_mm_pause       _mm_pause
#endif

#ifndef JIMI_MIN
#define JIMI_MIN(a, b)          ((a) < (b) ? (a) : (b))
#endif

#ifndef JIMI_MAX
#define JIMI_MAX(a, b)          ((a) > (b) ? (a) : (b))
#endif

#if defined(_WIN64) || defined(_M_X64)
#define JIMI_SIZE_T_SIZEOF      8
#else
#define JIMI_SIZE_T_SIZEOF      4
#endif

/**
 * macro for round to power of 2
 */
#define jimi_b2(x)              (        (x) | (        (x) >>  1))
#define jimi_b4(x)              ( jimi_b2(x) | ( jimi_b2(x) >>  2))
#define jimi_b8(x)              ( jimi_b4(x) | ( jimi_b4(x) >>  4))
#define jimi_b16(x)             ( jimi_b8(x) | ( jimi_b8(x) >>  8))
#define jimi_b32(x)             (jimi_b16(x) | (jimi_b16(x) >> 16))
#define jimi_b64(x)             (jimi_b32(x) | (jimi_b32(x) >> 32))

#define jimi_next_power_of_2(x)     (jimi_b32((x) - 1) + 1)
#define jimi_next_power_of_2_64(x)  (jimi_b64((x) - 1) + 1)

#if defined(JIMI_SIZE_T_SIZEOF) && (JIMI_SIZE_T_SIZEOF == 8)
#define JIMI_ROUND_TO_POW2(N)   jimi_next_power_of_2_64(N)
#else
#define JIMI_ROUND_TO_POW2(N)   jimi_next_power_of_2(N)
#endif

#if defined(_MSC_VER)

#ifndef jimi_likely
#define jimi_likely(x)      (x)
#endif

#ifndef jimi_unlikely
#define jimi_unlikely(x)    (x)
#endif

///
/// _ReadWriteBarrier
///
/// See: http://msdn.microsoft.com/en-us/library/f20w0x5e%28VS.80%29.aspx
///
/// See: http://en.wikipedia.org/wiki/Memory_ordering
///
#define Jimi_ReadWriteBarrier()  _ReadWriteBarrier();

#else  /* !_MSC_VER */

#ifndef jimi_likely
#define jimi_likely(x)      __builtin_expect((x), 1)
#endif

#ifndef jimi_unlikely
#define jimi_unlikely(x)    __builtin_expect((x), 0)
#endif

///
/// See: http://en.wikipedia.org/wiki/Memory_ordering
///
/// See: http://bbs.csdn.net/topics/310025520
///

#define Jimi_ReadWriteBarrier()     asm volatile ("":::"memory");
//#define Jimi_ReadWriteBarrier()     __asm__ __volatile__ ("":::"memory");

#endif  /* _MSC_VER */

#if defined(_MSC_VER)
#define jimi_compare_and_swap32(destPtr, oldValue, newValue)    \
    InterlockedCompareExchange((volatile LONG *)destPtr,    \
                            (uint32_t)(newValue), (uint32_t)(oldValue))
#define jimi_bool_compare_and_swap32(destPtr, oldValue, newValue)       \
    (InterlockedCompareExchange((volatile LONG *)destPtr,           \
                            (uint32_t)(newValue), (uint32_t)(oldValue)) \
                                == (uint32_t)(oldValue))
#elif defined(__linux__) || defined(__CYGWIN__) || defined(__MINGW__) || defined(__MINGW32__)
#define jimi_compare_and_swap32(destPtr, oldValue, newValue)    \
    __sync_compare_and_swap((volatile uint32_t *)destPtr,       \
                            (uint32_t)(oldValue), (uint32_t)(newValue))
#define jimi_bool_compare_and_swap32(destPtr, oldValue, newValue)   \
    __sync_bool_compare_and_swap((volatile uint32_t *)destPtr,      \
                            (uint32_t)(oldValue), (uint32_t)(newValue))
#else
#define jimi_compare_and_swap32(destPtr, oldValue, newValue)        \
    __internal_compare_and_swap32((volatile uint32_t *)(destPtr),   \
                                (uint32_t)(oldValue), (uint32_t)(newValue))
#define jimi_bool_compare_and_swap32(destPtr, oldValue, newValue)       \
    __internal_bool_compare_and_swap32((volatile uint32_t *)(destPtr),  \
                                (uint32_t)(oldValue), (uint32_t)(newValue))
#endif  /* _MSC_VER */

static inline
uint32_t __internal_compare_and_swap32(volatile uint32_t *destPtr,
                                       uint32_t oldValue,
                                       uint32_t newValue)
{
    uint32_t origValue = *destPtr;
    if (*destPtr == oldValue) {
        *destPtr = newValue;
    }
    return origValue;
}

static inline
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

namespace jimi {

struct RingQueueHead
{
    volatile uint32_t head;
    char padding1[JIMI_CACHELINE_SIZE - sizeof(uint32_t)];

    volatile uint32_t tail;
    char padding2[JIMI_CACHELINE_SIZE - sizeof(uint32_t)];
};

typedef struct RingQueueHead RingQueueHead;

///////////////////////////////////////////////////////////////////
// class SmallRingQueueCore<Capcity>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capcity>
class SmallRingQueueCore
{
public:
    typedef uint32_t    size_type;
    typedef T *         item_type;

public:
    static const size_type  kCapcityCore = (size_type)JIMI_MAX(JIMI_ROUND_TO_POW2(Capcity), 2);
    static const bool kIsAllocOnHeap     = false;

public:
    RingQueueHead   info;
    item_type       queue[kCapcityCore];
};

///////////////////////////////////////////////////////////////////
// class RingQueueCore<Capcity>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capcity>
class RingQueueCore
{
public:
    typedef T *     item_type;

public:
    static const bool kIsAllocOnHeap = true;

public:
    RingQueueHead   info;
    item_type *     queue;
};

///////////////////////////////////////////////////////////////////
// class RingQueueBase<T, Capcity, CoreTy>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capcity = 16U,
          typename CoreTy = RingQueueCore<T, Capcity> >
class RingQueueBase
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
    static const size_type  kCapcity = (size_type)JIMI_MAX(JIMI_ROUND_TO_POW2(Capcity), 2);
    static const index_type kMask    = (index_type)(kCapcity - 1);

public:
    RingQueueBase(bool bInitHead = false);
    ~RingQueueBase();

public:
    void dump_info();
    void dump_detail();

    index_type mask() const      { return kMask;    };
    size_type capcity() const    { return kCapcity; };
    size_type length() const     { return sizes();  };
    size_type sizes() const;

    void init(bool bInitHead = false);

    int push(T * item);
    T * pop();

public:
    core_type core;
};

template <typename T, uint32_t Capcity, typename CoreTy>
RingQueueBase<T, Capcity, CoreTy>::RingQueueBase(bool bInitHead  /* = false */)
{
    //printf("RingQueueBase::RingQueueBase();\n\n");

    init(bInitHead);
}

template <typename T, uint32_t Capcity, typename CoreTy>
RingQueueBase<T, Capcity, CoreTy>::~RingQueueBase()
{
    // Do nothing!
}

template <typename T, uint32_t Capcity, typename CoreTy>
inline
void RingQueueBase<T, Capcity, CoreTy>::init(bool bInitHead /* = false */)
{
    //printf("RingQueueBase::init();\n\n");

    if (!bInitHead) {
        core.info.head = 0;
        core.info.tail = 0;
    }
    else {
        memset((void *)&core.info, 0, sizeof(core.info));
    }
}

template <typename T, uint32_t Capcity, typename CoreTy>
void RingQueueBase<T, Capcity, CoreTy>::dump_info()
{
    //ReleaseUtils::dump(&core.info, sizeof(core.info));
}

template <typename T, uint32_t Capcity, typename CoreTy>
void RingQueueBase<T, Capcity, CoreTy>::dump_detail()
{
#if 0
    printf("---------------------------------------------------------\n");
    printf("RingQueueBase.p.head = %u\nRingQueueBase.p.tail = %u\n\n", core.info.p.head, core.info.p.tail);
    printf("RingQueueBase.c.head = %u\nRingQueueBase.c.tail = %u\n",   core.info.c.head, core.info.c.tail);
    printf("---------------------------------------------------------\n\n");
#else
    printf("RingQueueBase: (head = %u, tail = %u)\n",
           core.info.head, core.info.tail);
#endif
}

template <typename T, uint32_t Capcity, typename CoreTy>
inline
typename RingQueueBase<T, Capcity, CoreTy>::size_type
RingQueueBase<T, Capcity, CoreTy>::sizes() const
{
    index_type head, tail;

    head = core.info.head;
    Jimi_ReadWriteBarrier();

    tail = core.info.tail;
    Jimi_ReadWriteBarrier();

    return (size_type)(head - tail);
}

template <typename T, uint32_t Capcity, typename CoreTy>
inline
int RingQueueBase<T, Capcity, CoreTy>::push(T * item)
{
    index_type head, tail, next;
    bool ok = false;

#if 1
    do {
        head = core.info.head;
        tail = core.info.tail;
        if ((head - tail) > kMask)
            return -1;
        next = head + 1;
        ok = jimi_bool_compare_and_swap32(&core.info.head, head, next);
    } while (!ok);
#else
    do {
        head = core.info.head;
        tail = core.info.tail;
        if ((head - tail) > kMask)
            return -1;
        next = head + 1;
    } while (jimi_compare_and_swap32(&core.info.head, head, next) != head);
#endif

    core.queue[head & kMask] = item;

    Jimi_ReadWriteBarrier();

    return 0;
}

template <typename T, uint32_t Capcity, typename CoreTy>
inline
T * RingQueueBase<T, Capcity, CoreTy>::pop()
{
    index_type head, tail, next;
    value_type item;
    bool ok = false;

#if 1
    do {
        head = core.info.head;
        tail = core.info.tail;
        if ((tail == head) || (tail > head && (head - tail) > kMask))
            return (value_type)NULL;
        next = tail + 1;
        ok = jimi_bool_compare_and_swap32(&core.info.tail, tail, next);
    } while (!ok);
#else
    do {
        head = core.info.head;
        tail = core.info.tail;
        if (tail >= head && (head - tail) <= kMask)
            return (value_type)NULL;
        next = tail + 1;
    } while (jimi_compare_and_swap32(&core.info.tail, tail, next) != tail);
#endif

    item = core.queue[head & kMask];

    Jimi_ReadWriteBarrier();

    return item;
}

///////////////////////////////////////////////////////////////////
// class SmallRingQueue2<T, Capcity>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capcity = 16U>
class SmallRingQueue2 : public RingQueueBase<T, Capcity, SmallRingQueueCore<T, Capcity> >
{
public:
    typedef uint32_t                    size_type;
    typedef uint32_t                    index_type;
    typedef T *                         value_type;
    typedef T *                         pointer;
    typedef const T *                   const_pointer;
    typedef T &                         reference;
    typedef const T &                   const_reference;

    static const size_type kCapcity = RingQueueBase<T, Capcity, SmallRingQueueCore<T, Capcity> >::kCapcity;

public:
    SmallRingQueue2(bool bFillQueue = false, bool bInitHead = false);
    ~SmallRingQueue2();

public:
    void dump_detail();

protected:
    void init_queue(bool bFillQueue = false);
};

template <typename T, uint32_t Capcity>
SmallRingQueue2<T, Capcity>::SmallRingQueue2(bool bFillQueue /* = false */,
                                             bool bInitHead  /* = false */)
: RingQueueBase<T, Capcity, SmallRingQueueCore<T, Capcity> >(bInitHead)
{
    //printf("SmallRingQueue2::SmallRingQueue2();\n\n");

    init_queue(bFillQueue);
}

template <typename T, uint32_t Capcity>
SmallRingQueue2<T, Capcity>::~SmallRingQueue2()
{
    // Do nothing!
}

template <typename T, uint32_t Capcity>
inline
void SmallRingQueue2<T, Capcity>::init_queue(bool bFillQueue /* = false */)
{
    //printf("SmallRingQueue2::init_queue();\n\n");

    if (bFillQueue) {
        memset((void *)this->core.queue, 0, sizeof(value_type) * kCapcity);
    }
}

template <typename T, uint32_t Capcity>
void SmallRingQueue2<T, Capcity>::dump_detail()
{
    printf("SmallRingQueue2: (head = %u, tail = %u)\n",
           this->core.info.head, this->core.info.tail);
}

///////////////////////////////////////////////////////////////////
// class RingQueue2<T, Capcity>
///////////////////////////////////////////////////////////////////

template <typename T, uint32_t Capcity = 16U>
class RingQueue2 : public RingQueueBase<T, Capcity, RingQueueCore<T, Capcity> >
{
public:
    typedef uint32_t                    size_type;
    typedef uint32_t                    index_type;
    typedef T *                         value_type;
    typedef T *                         pointer;
    typedef const T *                   const_pointer;
    typedef T &                         reference;
    typedef const T &                   const_reference;

    typedef RingQueueCore<T, Capcity>   core_type;

    static const size_type kCapcity = RingQueueBase<T, Capcity, RingQueueCore<T, Capcity> >::kCapcity;

public:
    RingQueue2(bool bFillQueue = false, bool bInitHead = false);
    ~RingQueue2();

public:
    void dump_detail();

protected:
    void init_queue(bool bFillQueue = false);
};

template <typename T, uint32_t Capcity>
RingQueue2<T, Capcity>::RingQueue2(bool bFillQueue /* = false */,
                                   bool bInitHead  /* = false */)
: RingQueueBase<T, Capcity, RingQueueCore<T, Capcity> >(bInitHead)
{
    //printf("RingQueue2::RingQueue2();\n\n");

    init_queue(bFillQueue);
}

template <typename T, uint32_t Capcity>
RingQueue2<T, Capcity>::~RingQueue2()
{
    // If the queue is allocated on system heap, release them.
    if (RingQueueCore<T, Capcity>::kIsAllocOnHeap) {
        delete [] this->core.queue;
        this->core.queue = NULL;
    }
}

template <typename T, uint32_t Capcity>
inline
void RingQueue2<T, Capcity>::init_queue(bool bFillQueue /* = false */)
{
    //printf("RingQueue2::init_queue();\n\n");

    value_type *newData = new T *[kCapcity];
    if (newData != NULL) {
        this->core.queue = newData;
        if (bFillQueue) {
            memset((void *)this->core.queue, 0, sizeof(value_type) * kCapcity);
        }
    }
}

template <typename T, uint32_t Capcity>
void RingQueue2<T, Capcity>::dump_detail()
{
    printf("RingQueue2: (head = %u, tail = %u)\n",
           this->core.info.head, this->core.info.tail);
}

}  /* namespace jimi */

#undef JIMI_CACHELINE_SIZE

#endif  /* _JIMI_UTIL_RINGQUEUE_H_ */
