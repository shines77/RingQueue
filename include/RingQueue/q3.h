
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

#define USE_SLEEP_AND_LOG   0

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

static inline int
push(struct queue *q, void *m)
{
    uint32_t head, tail, mask, next;
#if defined(USE_SLEEP_AND_LOG) && (USE_SLEEP_AND_LOG != 0)
    uint32_t loop_cnt;
#endif
    int ok;

#if defined(USE_SLEEP_AND_LOG) && (USE_SLEEP_AND_LOG != 0)
    loop_cnt = 0;
#endif
    mask = q->p.mask;

    do {
        head = q->p.head;
        tail = q->c.tail;
        //if ((mask + tail - head) < 1U)      // 这样用表示队列最大长度为mask, 而不是(mask + 1)
        if ((head - tail) > mask)
            return -1;
#if defined(USE_SLEEP_AND_LOG) && (USE_SLEEP_AND_LOG != 0)
        loop_cnt++;
        if (loop_cnt > 1000) {
            loop_cnt = 0;
            printf("push(): cas() sleep(1)\n");
            printf("q->p.head = %u, head = %u\n", q->p.head, head);
            jimi_wsleep(1);
        }
#endif
        next = head + 1;
        ok = jimi_bool_compare_and_swap32(&q->p.head, head, next);
    } while (!ok);

    q->msgs[head & mask] = m;
#ifdef _MSC_VER
    Jimi_ReadWriteBarrier();
#else
    asm volatile ("":::"memory");
#endif

#if defined(USE_SLEEP_AND_LOG) && (USE_SLEEP_AND_LOG != 0)
    loop_cnt = 0;
#endif

    while (jimi_unlikely((q->p.tail != head))) {
#if defined(USE_SLEEP_AND_LOG) && (USE_SLEEP_AND_LOG != 0)
        loop_cnt++;
        if (loop_cnt > 1000) {
            loop_cnt = 0;
            //printf("push() sleep(1)\n");
            //printf("q->p.tail = %u, head = %u\n", q->p.tail, head);
            jimi_wsleep(1);
        }
#endif
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
#if defined(USE_SLEEP_AND_LOG) && (USE_SLEEP_AND_LOG != 0)
    uint32_t loop_cnt, loop_cnt2;
#endif
    volatile void *ret;

#if defined(USE_SLEEP_AND_LOG) && (USE_SLEEP_AND_LOG != 0)
    loop_cnt = 0;
    loop_cnt2 = 0;
#endif
    mask = q->c.mask;

    do {
        head = q->c.head;
        tail = q->p.tail;
        //if ((tail - head) < 1U)
        if (head == tail || (head > tail && (tail - head) > mask))
            return NULL;
#if defined(USE_SLEEP_AND_LOG) && (USE_SLEEP_AND_LOG != 0)
        loop_cnt++;
        if (loop_cnt > 1000) {
            loop_cnt = 0;
            printf("pop(): cas() sleep(1)\n");
            printf("q->c.head = %u, head = %u\n", q->c.head, head);
            jimi_wsleep(1);
        }
#endif
        next = head + 1;
        ok = jimi_bool_compare_and_swap32(&q->c.head, head, next);
    } while (!ok);

    ret = q->msgs[head & mask];
#ifdef _MSC_VER
    Jimi_ReadWriteBarrier();
#else
    asm volatile ("":::"memory");
#endif

#if defined(USE_SLEEP_AND_LOG) && (USE_SLEEP_AND_LOG != 0)
    loop_cnt = 0;
    loop_cnt2 = 0;
#endif

    while (jimi_unlikely((q->c.tail != head))) {
#if defined(USE_SLEEP_AND_LOG) && (USE_SLEEP_AND_LOG != 0)
        loop_cnt++;
        if (loop_cnt > 1000) {
            loop_cnt = 0;
            loop_cnt2++;
            if (loop_cnt2 > 10000) {
                loop_cnt = 0;
                loop_cnt2 = 0;
                printf("pop() sleep(1)\n");
                printf("q->c.tail = %u, head = %u\n", q->c.tail, head);
            }
            jimi_wsleep(1);
        }
#endif
        jimi_mm_pause();
    }

    q->c.tail = next;

    return (void *)ret;
}

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#pragma warning(pop)
#endif  /* _MSC_VER */
