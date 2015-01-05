
#include <stdint.h>
#include <stdlib.h>
#include <emmintrin.h>

#ifndef likely
#define likely(x)     __builtin_expect((x), 1)
#endif

#ifndef unlikely
#define unlikely(x)   __builtin_expect((x), 0)
#endif


#define QSZ   (1024 * 1)
#define QMSK  (QSZ - 1)

struct msg {
    uint64_t dummy;
};

#define CACHE_LINE_SIZE     64

struct queue {
    struct {
        uint32_t mask;
        uint32_t size;
        volatile uint32_t first;
        volatile uint32_t second;
    } head;
    char pad1[CACHE_LINE_SIZE - 4 * sizeof(uint32_t)];

    struct {
        uint32_t mask;
        uint32_t size;
        volatile uint32_t first;
        volatile uint32_t second;
    } tail;
    char pad2[CACHE_LINE_SIZE - 4 * sizeof(uint32_t)];

    void        *msgs[0];
};

static inline struct queue *
qinit(void)
{
    struct queue *q = calloc(1, sizeof(*q) + QSZ * sizeof(void *));
    q->head.size = q->tail.size = QSZ;
    q->head.mask = q->tail.mask = QMSK;

    return q;
}

static inline int
push(struct queue *q, void *m)
{
    uint32_t head, tail, mask, next;
    int ok;

    mask = q->head.mask;

    do {
        head = q->head.first;
        tail = q->tail.second;
        //if ((mask + tail - head) < 1U)
        if ((head - tail) > mask)
            return -1;
        next = head + 1;
        ok = __sync_bool_compare_and_swap(&q->head.first, head, next);
    } while (!ok);

    q->msgs[head & mask] = m;
    asm volatile ("":::"memory");

    while (unlikely((q->head.second != head)))
        _mm_pause();

    q->head.second = next;

    return 0;
}

static inline void *
pop(struct queue *q)
{
    uint32_t tail, head, mask, next;
    int ok;
    void *ret;

    mask = q->tail.mask;

    do {
        tail = q->tail.first;
        head = q->head.second;
        //if ((head - tail) < 1U)
        if ((tail == head) || (tail > head && (head - tail) > mask))
            return NULL;
        next = tail + 1;
        ok = __sync_bool_compare_and_swap(&q->tail.first, tail, next);
    } while (!ok);

    ret = q->msgs[tail & mask];
    asm volatile ("":::"memory");

    while (unlikely((q->tail.second != tail)))
        _mm_pause();

    q->tail.second = next;

    return ret;
}
