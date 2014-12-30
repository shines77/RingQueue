
#include <stdlib.h>

struct msg {
    struct msg *next;
};

struct queue {
    struct msg *head;
    struct msg *tail;
    int lock;
};

#define LOCK(q) while (__sync_lock_test_and_set(&(q)->lock,1)) {}
#define UNLOCK(q) __sync_lock_release(&(q)->lock);

static inline struct queue *
qinit(void)
{
    struct queue *q = calloc(1, sizeof(*q));
    return q;
}

static inline int
push(struct queue *q, struct msg *m)
{
    LOCK(q)
    if (q->tail) {
        q->tail->next = m;
        q->tail = m;
    } else {
        q->head = q->tail = m;
    }
    UNLOCK(q)

    return 0;
}

static inline struct msg *
pop(struct queue *q)
{
    struct msg *m;

    LOCK(q)
    m = q->head;
    if (m) {
        q->head = m->next;
        if (q->head == NULL) {
            q->tail = NULL;
        }
        m->next = NULL;
    }
    UNLOCK(q)

    return m;
}
