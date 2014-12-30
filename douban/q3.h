
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

    void        *msgs[0];
};

static inline struct queue *
qinit(void)
{
    struct queue *q = calloc(1, sizeof(*q) + QSZ * sizeof(void *));
    q->p.size = q->c.size = QSZ;
    q->p.mask = q->c.mask = QMSK;

    return q;
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
        if ((mask + tail - head) < 1U)
            return -1;
        next = head + 1;
        ok = __sync_bool_compare_and_swap(&q->p.head, head, next);
    } while (!ok);

    q->msgs[head & mask] = m;
    asm volatile ("":::"memory");

    while (unlikely((q->p.tail != head)))
        _mm_pause();

    q->p.tail = next;

    return 0;
}

static inline void *
pop(struct queue *q)
{
    uint32_t head, tail, mask, next;
    int ok;
    void *ret;

    mask = q->c.mask;

    do {
        head = q->c.head;
        tail = q->p.tail;
        if ((tail - head) < 1U)
            return NULL;
        next = head + 1;
        ok = __sync_bool_compare_and_swap(&q->c.head, head, next);
    } while (!ok);

    ret = q->msgs[head & mask];
    asm volatile ("":::"memory");

    while (unlikely((q->c.tail != head)))
        _mm_pause();

    q->c.tail = next;

    return ret;
}
