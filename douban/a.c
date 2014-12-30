
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>

#include "q3.h"
//#include "q.h"
//#include "qlock.h"

static struct msg *msgs;

#define POP_CNT     7
#define PUSH_CNT    7

#define MSG_CNT     (PUSH_CNT * 1000000)

static inline uint64_t
rdtsc(void)
{
  union {
    uint64_t tsc_64;
    struct {
      uint32_t lo_32;
      uint32_t hi_32;
    };
  } tsc;

  asm volatile("rdtsc" :
    "=a" (tsc.lo_32),
    "=d" (tsc.hi_32));
  return tsc.tsc_64;
}

static volatile int quit = 0;
static volatile int pop_total = 0;
static volatile int push_total = 0;

static volatile uint64_t push_cycles = 0;
static volatile uint64_t pop_cycles = 0;


static void *
pop_task(void *arg)
{
    struct queue *q = arg;
    uint64_t start = rdtsc();
    int cnt = 0;

    while (!quit)
        cnt += !!pop(q);

    pop_cycles += rdtsc() - start;
    pop_total += cnt;

    return NULL;
}

static void *
push_task(void *arg)
{
    struct queue *q = arg;
    uint64_t start = rdtsc();
    int i;

    for (i = 0; i < MSG_CNT / PUSH_CNT; i++)
        while (push(q, msgs + i) == -1);

    push_cycles += rdtsc() - start;
    push_total += MSG_CNT / PUSH_CNT;
    if (push_total == MSG_CNT)
        quit = 1;

    return NULL;
}

/* topology for Xeon E5-2670 Sandybridge */
static const int socket_top[] = {
  1,  2,  3,  4,  5,  6,  7,
  16, 17, 18, 19, 20, 21, 22, 23,
  8,  9,  10, 11, 12, 13, 14, 15,
  24, 25, 26, 27, 28, 29, 30, 31
};

#define CORE_ID(i)    socket_top[(i)]


static int
start_thread(int id,
       void *(*cb)(void *),
       void *arg,
       pthread_t *tid)
{
    pthread_t kid;
    pthread_attr_t attr;
    cpu_set_t cpuset;
    int core_id;

    if (id < 0 || id >= sizeof(socket_top) / sizeof(int))
        return -1;

    if (pthread_attr_init(&attr))
        return -1;

    CPU_ZERO(&cpuset);
    core_id = CORE_ID(id);
    CPU_SET(core_id, &cpuset);

    if (pthread_create(&kid, &attr, cb, arg))
        return -1;
    if (pthread_setaffinity_np(kid, sizeof(cpu_set_t), &cpuset))
        return -1;

    if (tid)
        *tid = kid;

    return 0;
}

static int
setaffinity(int core_id)
{
    cpu_set_t cpuset;
    pthread_t me = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    if (pthread_setaffinity_np(me, sizeof(cpu_set_t), &cpuset))
        return -1;

    return 0;
}

int
main(void)
{
    struct queue *q = qinit();
    int i;
    pthread_t kids[POP_CNT + PUSH_CNT];

    setaffinity(0);

    msgs = calloc(MSG_CNT, sizeof(struct msg));
    for (i = 0; i < POP_CNT; i++)
        start_thread(i, pop_task, q, &kids[i]);
    for (; i < POP_CNT + PUSH_CNT; i++)
        start_thread(i, push_task, q, &kids[i]);
    for (i = 0; i < POP_CNT + PUSH_CNT; i++)
        pthread_join(kids[i], NULL);

    printf("pop total: %d\n", pop_total);
    printf("pop cycles/msg: %lu\n", pop_cycles / pop_total);
    printf("push cycles/msg: %lu\n", push_cycles / MSG_CNT);

    return 0;
}
