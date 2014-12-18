
#include <stdint.h>
#include <stdlib.h>

#include <xmmintrin.h>
#include <emmintrin.h>

#include "get_char.h"

#include <windows.h>

#define jimi_wsleep     Sleep

#ifndef likely
#define likely(x)   __builtin_expect((x), 1)
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect((x), 0)
#endif

#define USE_SLEEP_AND_LOG   0

#define QSZ         (1024 * 1)
#define QMSK        (QSZ - 1)

typedef
struct msg_t {
    uint64_t dummy;
} msg_t;

#define CACHE_LINE_SIZE     64

struct queue {
    struct {
        uint32_t mask;
        uint32_t size;
        volatile uint32_t head;
        volatile uint32_t tail;
    } p;
    char pad[CACHE_LINE_SIZE - 4 * sizeof(uint32_t)];

    struct {
        uint32_t mask;
        uint32_t size;
        volatile uint32_t head;
        volatile uint32_t tail;
    } c;
    char pad2[CACHE_LINE_SIZE - 4 * sizeof(uint32_t)];

    volatile void *msgs[0];
};

static inline struct queue *
qinit(void)
{
    struct queue *q = (struct queue *)calloc(1, sizeof(*q) + QSZ * sizeof(void *));
    q->p.size = q->c.size = QSZ;
    q->p.mask = q->c.mask = QMSK;

    return q;
}

static inline int
push(struct queue *q, volatile void *m)
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
        if ((mask + tail - head) < 1U)      // 这样用表示队列最大长度为mask, 而不是(mask + 1)
        //if ((head - tail) > mask)
        // if ((int32_t)(head - tail - mask) > 0)
        // if ((int32_t)(tail - head) <= (int32_t)-(mask + 1))
        // if ((int32_t)(tail - head) < (int32_t)-mask)
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
        ok = __sync_bool_compare_and_swap(&q->p.head, head, next);
    } while (!ok);

    q->msgs[head & mask] = m;
    asm volatile ("":::"memory");

#if defined(USE_SLEEP_AND_LOG) && (USE_SLEEP_AND_LOG != 0)
    loop_cnt = 0;
#endif

    while (unlikely((q->p.tail != head))) {
#if defined(USE_SLEEP_AND_LOG) && (USE_SLEEP_AND_LOG != 0)
        loop_cnt++;
        if (loop_cnt > 1000) {
            loop_cnt = 0;
            //printf("push() sleep(1)\n");
            //printf("q->p.tail = %u, head = %u\n", q->p.tail, head);
            jimi_wsleep(1);
        }
#endif
        _mm_pause();
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
        if ((tail - head) < 1U)
        //if (tail == head)
        //if (head >= tail && (tail - head) <= mask)
        //if (head == tail || (head > tail && (tail - head) > mask))
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
        ok = __sync_bool_compare_and_swap(&q->c.head, head, next);
    } while (!ok);

    ret = q->msgs[head & mask];
    asm volatile ("":::"memory");

#if defined(USE_SLEEP_AND_LOG) && (USE_SLEEP_AND_LOG != 0)
    loop_cnt = 0;
    loop_cnt2 = 0;
#endif

    while (unlikely((q->c.tail != head))) {
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
        _mm_pause();
    }

    q->c.tail = next;

    return (void *)ret;
}
