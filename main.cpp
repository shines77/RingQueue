
#define _GNU_SOURCE
#define __USE_GNU

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sched.h>
#include <pthread.h>

#include "q3.h"
//#include "q.h"
//#include "qlock.h"

#include "get_char.h"
#include "console.h"
#include "RingQueue.h"

using namespace jimi;

typedef RingQueue2<msg_t, QSZ> RingQueue;

#ifndef USE_JIMI_RINGQUEUE
#define USE_JIMI_RINGQUEUE     1
#endif

#if defined(__MINGW__) || defined(__MINGW32__) || defined(__CYGWIN__) || defined(_WIN32)
typedef unsigned int cpu_set_t;
#endif // defined

static volatile struct msg_t *msgs;
//static volatile struct msg_t *msg_list;

#define POP_CNT         1
#define PUSH_CNT        1

#if 1
#define MAX_MSG_LENGTH  1000000
#else
#define MAX_MSG_LENGTH  10000
#endif

#define MSG_CNT         (PUSH_CNT * MAX_MSG_LENGTH)

static struct msg_t *msg_list[MSG_CNT];

static pthread_mutex_t s_queue_rwlock = NULL;

typedef struct thread_arg_t
{
    int         idx;
    RingQueue  *queue;
} thread_arg_t;

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

static void
init_globals(void)
{
    pthread_mutexattr_t attr;
    quit = 0;
    pop_total = 0;
    push_total = 0;

    push_cycles = 0;
    pop_cycles = 0;

    if (s_queue_rwlock == NULL) {
        if (!pthread_mutexattr_init(&attr)) {
            pthread_mutex_init(&s_queue_rwlock, NULL);
        }
    }
}

static void *
ringqueue_push_task(void *arg)
{
    thread_arg_t *thread_arg;
    RingQueue *queue;
    msg_t *msg;
    uint64_t start;
    int i, idx, base;
    int cnt = 0;

    idx = -1;
    queue = NULL;
    base = 0;

    thread_arg = (thread_arg_t *)arg;
    if (thread_arg) {
        idx   = thread_arg->idx;
        queue = (RingQueue *)thread_arg->queue;
        base  = idx * MAX_MSG_LENGTH;
    }

    if (queue == NULL)
        return NULL;

    start = rdtsc();

    for (i = 0; i < MAX_MSG_LENGTH; i++) {
        msg = (msg_t *)(&msgs[base + i]);
        //while (queue->push((msg_t *)msg) == -1);
        while (queue->locked_push((msg_t *)msg, &s_queue_rwlock) == -1);
#if 0
        //if ((i & 0x3FFFU) == 0x3FFFU) {
        if ((i & 0xFFFFU) == 0xFFFFU) {
            printf("thread [%d] have push %d\n", idx, i);
        }
#endif
    }

#if 0
    printf("thread [%d] have push %d\n", idx, i);
#endif

    push_cycles += rdtsc() - start;
    push_total += MAX_MSG_LENGTH;
    if (push_total == MSG_CNT)
        quit = 1;

    if (thread_arg)
        free(thread_arg);

    return NULL;
}

static void *
ringqueue_pop_task(void *arg)
{
    thread_arg_t *thread_arg;
    RingQueue *queue;
    struct msg_t *msg;
    uint64_t start;
    int i, idx, base;
    int cnt = 0;

    idx = 0;
    queue = NULL;
    base = 0;

    thread_arg = (thread_arg_t *)arg;
    if (thread_arg) {
        idx   = thread_arg->idx;
        queue = (RingQueue *)thread_arg->queue;
        base  = idx * MAX_MSG_LENGTH;
    }

    if (queue == NULL)
        return NULL;

    start = rdtsc();

    while (!quit) {
        //msg = (struct msg_t *)queue->pop();
        msg = (struct msg_t *)queue->locked_pop(&s_queue_rwlock);
        if (msg != NULL) {
            msg_list[cnt * POP_CNT + idx] = (struct msg_t *)msg;
            cnt++;
            if (cnt >= MAX_MSG_LENGTH)
                break;
        }
    }

    pop_cycles += rdtsc() - start;
    pop_total += cnt;

    if (thread_arg)
        free(thread_arg);

    return NULL;
}

static void *
push_task(void *arg)
{
    struct queue *q = (struct queue *)arg;
    uint64_t start = rdtsc();
    int i;

    for (i = 0; i < MAX_MSG_LENGTH; i++) {
        while (push(q, msgs + i) == -1);
    }

    push_cycles += rdtsc() - start;
    push_total += MSG_CNT / PUSH_CNT;
    if (push_total == MSG_CNT)
        quit = 1;

    return NULL;
}

static void *
pop_task(void *arg)
{
    struct queue *q = (struct queue *)arg;
    uint64_t start = rdtsc();
    int cnt = 0;

    while (!quit) {
        cnt += !!pop(q);
    }

    pop_cycles += rdtsc() - start;
    pop_total += cnt;

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

#if !(defined(__MINGW__) || defined(__MINGW32__) || defined(__CYGWIN__))
    if (id < 0 || id >= sizeof(socket_top) / sizeof(int))
        return -1;
#endif

    if (pthread_attr_init(&attr))
        return -1;

#if !(defined(__MINGW__) || defined(__MINGW32__) || defined(__CYGWIN__))
    CPU_ZERO(&cpuset);
    core_id = CORE_ID(id);
    CPU_SET(core_id, &cpuset);
#endif // defined

    if (pthread_create(&kid, &attr, cb, arg))
        return -1;

#if !(defined(__MINGW__) || defined(__MINGW32__) || defined(__CYGWIN__))
    if (pthread_setaffinity_np(kid, sizeof(cpu_set_t), &cpuset))
        return -1;
#endif

    if (tid)
        *tid = kid;

    return 0;
}

static int
setaffinity(int core_id)
{
#if !(defined(__MINGW__) || defined(__MINGW32__) || defined(__CYGWIN__))
    cpu_set_t cpuset;
    pthread_t me = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    if (pthread_setaffinity_np(me, sizeof(cpu_set_t), &cpuset))
        return -1;
#endif
    return 0;
}

void
q3_test(void)
{
    struct queue *q = qinit();
    int i;
    pthread_t kids[POP_CNT + PUSH_CNT];

    init_globals();
    setaffinity(0);

    msgs     = (struct msg_t *)calloc(MSG_CNT, sizeof(struct msg_t));
    //msg_list = (struct msg_t *)calloc(MSG_CNT, sizeof(struct msg_t));

    for (i = 0; i < MSG_CNT; i++)
        msgs[i].dummy = (uint64_t)(i + 1);

    for (i = 0; i < POP_CNT; i++)
        start_thread(i, pop_task, q, &kids[i]);
    for (; i < POP_CNT + PUSH_CNT; i++)
        start_thread(i, push_task, q, &kids[i]);
    for (i = 0; i < POP_CNT + PUSH_CNT; i++)
        pthread_join(kids[i], NULL);

    printf("\n");
    printf("pop total: %d\n", pop_total);
    printf("pop cycles/msg: %lu\n", pop_cycles / pop_total);
    printf("push total: %d\n", push_total);
    printf("push cycles/msg: %lu\n", push_cycles / MSG_CNT);
    printf("\n");

    if (msgs) {
        free((void *)msgs);
        msgs = NULL;
    }

    //getchar();
    jimi_console_readkeyln(false, true, false);
}

static int
ringqueue_start_thread(int id,
                       void *(*cb)(void *),
                       void *arg,
                       pthread_t *tid)
{
    pthread_t kid;
    pthread_attr_t attr;
    cpu_set_t cpuset;
    int core_id;

#if !(defined(__MINGW__) || defined(__MINGW32__) || defined(__CYGWIN__))
    if (id < 0 || id >= sizeof(socket_top) / sizeof(int))
        return -1;
#endif

    if (pthread_attr_init(&attr))
        return -1;

#if !(defined(__MINGW__) || defined(__MINGW32__) || defined(__CYGWIN__))
    CPU_ZERO(&cpuset);
    core_id = CORE_ID(id);
    CPU_SET(core_id, &cpuset);
#endif // defined

    if (pthread_create(&kid, &attr, cb, arg))
        return -1;

#if !(defined(__MINGW__) || defined(__MINGW32__) || defined(__CYGWIN__))
    if (pthread_setaffinity_np(kid, sizeof(cpu_set_t), &cpuset))
        return -1;
#endif

    if (tid)
        *tid = kid;

    return 0;
}

void
RingQueue_UnitTest(void)
{
    RingQueue ringQueue(true, true);

    msg_t queue_msg = { 123ULL };

    printf("---------------------------------------------------------------\n");
    printf("RingQueue2() test begin...\n\n");

    printf("ringQueue.capcity() = %u\n", ringQueue.capcity());
    printf("ringQueue.mask()    = %u\n\n", ringQueue.mask());
    printf("ringQueue.sizes()   = %u\n\n", ringQueue.sizes());

    ringQueue.dump_detail();

    ringQueue.push(&queue_msg);
    ringQueue.dump_detail();
    ringQueue.push(&queue_msg);
    ringQueue.dump_detail();

    printf("ringQueue.sizes()   = %u\n\n", ringQueue.sizes());

    ringQueue.pop();
    ringQueue.dump_detail();
    ringQueue.pop();
    ringQueue.dump_detail();

    ringQueue.pop();
    ringQueue.dump_detail();

    printf("ringQueue.sizes()   = %u\n\n", ringQueue.sizes());

    ringQueue.dump_info();

    printf("RingQueue2() test end...\n");
    printf("---------------------------------------------------------------\n\n");

    //getchar();
    jimi_console_readkeyln(true, true, false);
}

void
RingQueue_Test(void)
{
    RingQueue ringQueue;
    int i;
    pthread_t kids[POP_CNT + PUSH_CNT];
    thread_arg_t *thread_arg;

    init_globals();
    setaffinity(0);

    msgs     = (struct msg_t *)calloc(MSG_CNT, sizeof(struct msg_t));
    //msg_list = (struct msg_t *)calloc(MSG_CNT, sizeof(struct msg_t));

    for (i = 0; i < MSG_CNT; i++)
        msgs[i].dummy = (uint64_t)(i + 1);

    for (i = 0; i < PUSH_CNT; i++) {
        thread_arg = (thread_arg_t *)malloc(sizeof(struct thread_arg_t));
        thread_arg->idx = i;
        thread_arg->queue = &ringQueue;
        ringqueue_start_thread(i, ringqueue_push_task, (void *)thread_arg,
                               &kids[i]);
    }
    for (i = 0; i < POP_CNT; i++) {
        thread_arg = (thread_arg_t *)malloc(sizeof(struct thread_arg_t));
        thread_arg->idx = i;
        thread_arg->queue = &ringQueue;
        ringqueue_start_thread(i + PUSH_CNT, ringqueue_pop_task, (void *)thread_arg,
                               &kids[i + PUSH_CNT]);
    }
    for (i = 0; i < POP_CNT + PUSH_CNT; i++)
        pthread_join(kids[i], NULL);

    printf("\n");
    printf("pop total: %d\n", pop_total);
    printf("pop cycles/msg: %lu\n", pop_cycles / pop_total);
    printf("push total: %d\n", push_total);
    printf("push cycles/msg: %lu\n", push_cycles / MSG_CNT);
    printf("\n");

    printf("msgs ptr = 0x%08X\n\n", msgs);

    jimi_console_readkeyln(false, true, false);

    for (i = 0; i <= 256; ++i) {
        printf("msg_list[%3d] = ptr: 0x%08X, %02llu : %llu\n", i,
               //((char *)(&msg_list[i]) - (char *)msgs) / sizeof(void *),
               (msg_t *)(msg_list[i]),
               msg_list[i]->dummy / (MSG_CNT / PUSH_CNT),
               msg_list[i]->dummy % (MSG_CNT / PUSH_CNT));
    }
    printf("\n");

    jimi_console_readkeyln(false, true, false);

    for (i = 0; i <= 256; ++i) {
        printf("msg_list[%3d] = %02llu : %llu\n", i, msgs[i].dummy / (MSG_CNT / PUSH_CNT),
               msgs[i].dummy % (MSG_CNT / PUSH_CNT));
    }
    printf("\n");

#if 0
    for (i = MSG_CNT - 128; i < MSG_CNT; ++i) {
        printf("msg_list[%d] = %llu\n", i, msg_list[i].dummy);
    }
    printf("\n\n");
#endif

    if (msgs) {
        free((void *)msgs);
        msgs = NULL;
    }

    //getchar();
    jimi_console_readkeyln(true, true, false);
}

int
main(void)
{
#if defined(USE_JIMI_RINGQUEUE) && (USE_JIMI_RINGQUEUE != 0)
    //RingQueue_UnitTest();
    RingQueue_Test();

    q3_test();
#else
    q3_test();
#endif // defined

    if (s_queue_rwlock) {
        pthread_mutex_destroy(&s_queue_rwlock);
        s_queue_rwlock = NULL;
    }

    //jimi_console_readkeyln(false, true, false);
    return 0;
}
