
#include <stdlib.h>

#include <xmmintrin.h>
#include <emmintrin.h>

#include "test.h"
#include "port.h"
#include "vs_stdint.h"
#include "get_char.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef likely
#define likely(x)   __builtin_expect((x), 1)
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect((x), 0)
#endif

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#pragma warning(push)
#pragma warning(disable: 4200)
#endif  /* _MSC_VER */

struct queue {
    struct {
        uint32_t mask;
        uint32_t size;
        volatile uint32_t head;
        volatile uint32_t tail;
    } p;
    char pad1[CACHE_LINE_SIZE - 4 * sizeof(uint32_t)];

    struct {
        uint32_t mask;
        uint32_t size;
        volatile uint32_t head;
        volatile uint32_t tail;
    } c;
    char pad2[CACHE_LINE_SIZE - 4 * sizeof(uint32_t)];

    void *msgs[0];
};

static inline struct queue *
qinit(void)
{
    struct queue *q = (struct queue *)calloc(1, sizeof(*q) + QSIZE * sizeof(void *));
    q->p.size = q->c.size = QSIZE;
    q->p.mask = q->c.mask = QMASK;

    return q;
}

static inline void
qfree(struct queue *q)
{
    if (q != NULL)
        free(q);
}

static inline int
push(struct queue *q, void *m)
{
    uint32_t head, tail, mask, next;
    int ok;

    mask = q->p.mask;

    do {
        head = q->p.head;
        tail = q->c.tail;
        if ((mask + tail - head) < 1U)      // 这样用表示队列最大长度为mask, 而不是(mask + 1)
        //if ((head - tail) > mask)
            return -1;
        next = head + 1;
        ok = jimi_bool_compare_and_swap32(&q->p.head, head, next);
    } while (!ok);

    q->msgs[head & mask] = m;

#ifdef _MSC_VER
    Jimi_CompilerBarrier();
#else
    asm volatile ("":::"memory");
#endif

    while (jimi_unlikely((q->p.tail != head))) {
        jimi_mm_pause();
    }

    q->p.tail = next;

    return 0;
}

static inline void *
pop(struct queue *q)
{
    uint32_t head, tail, mask, next;
    int ok;
    volatile void *ret;

    mask = q->c.mask;

    do {
        head = q->c.head;
        tail = q->p.tail;
        if ((tail - head) < 1U)
        //if (head == tail || (head > tail && (tail - head) > mask))
            return NULL;
        next = head + 1;
        ok = jimi_bool_compare_and_swap32(&q->c.head, head, next);
    } while (!ok);

    ret = q->msgs[head & mask];

#ifdef _MSC_VER
    Jimi_CompilerBarrier();
#else
    asm volatile ("":::"memory");
#endif

    while (jimi_unlikely((q->c.tail != head))) {
        jimi_mm_pause();
    }

    q->c.tail = next;

    return (void *)ret;
}

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#pragma warning(pop)
#endif  /* _MSC_VER */
