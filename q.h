
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct msg {
    struct msg *next;
};

struct queue {
    uint32_t head;
    uint32_t tail;
    struct msg    **msgs;
    struct msg    *list;
};

#define CNT   0x10000
#define GP(x) (x % (CNT))

static inline struct queue *
qinit(void)
{
    struct queue *q = malloc(sizeof(*q));

    bzero(q, sizeof(*q));
    q->msgs = calloc(sizeof(struct msg *), CNT);

    return q;
}

static inline int
push(struct queue *q, struct msg *m)
{
    uint32_t tail = GP(__sync_fetch_and_add(&q->tail, 1));

    if (!__sync_bool_compare_and_swap(&q->msgs[tail], NULL, m)) {
        struct msg *last;
        do {
            last = q->list;
            m->next = last;
        } while (!__sync_bool_compare_and_swap(&q->list, last, m));
    }
    return 0;
}

static inline struct msg *
pop(struct queue *q)
{
    uint32_t head = q->head;
    uint32_t h2;
    struct msg *list;

    if (head == q->tail)
        return NULL;

    h2 = GP(head);
    list = q->list;

    if (list) {
        struct msg *n = list->next;
        if (__sync_bool_compare_and_swap(&q->list, list, n)) {
            list->next = NULL;
            push(q, list);
        }
    }
    struct msg *m = q->msgs[h2];
    if (!m)
        return NULL;
    if (!__sync_bool_compare_and_swap(&q->head, head, head + 1))
        return NULL;
    if (!__sync_bool_compare_and_swap(&q->msgs[h2], m, NULL))
        return NULL;

    return m;
}
