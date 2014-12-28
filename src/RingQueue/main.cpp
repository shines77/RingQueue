
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef __USE_GNU
#define __USE_GNU
#endif

#include "msvc/targetver.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include "vs_stdint.h"

#ifndef _MSC_VER
#include <sched.h>
#include <pthread.h>
#include "msvc/sched.h"
#include "msvc/pthread.h"   // For define PTW32_API
#else
#include "msvc/sched.h"
#include "msvc/pthread.h"
#endif  // _MSC_VER

#define __STDC_FORMAT_MACROS
#include "vs_inttypes.h"

#include "q3.h"
//#include "q.h"
//#include "qlock.h"

#include "get_char.h"
#include "sys_timer.h"
#include "console.h"
#include "RingQueue.h"
#include "SpinMutex.h"

//#include <vld.h>
#include <errno.h>

using namespace jimi;

typedef RingQueue<msg_t, QSIZE> RingQueue_t;

typedef struct thread_arg_t
{
    int             idx;
    int             funcType;
    RingQueue_t    *queue;
    struct queue   *q;
} thread_arg_t;

static volatile struct msg_t *msgs = NULL;

//static struct msg_t **popmsg_list = NULL;

static struct msg_t *popmsg_list[POP_CNT][MAX_POP_MSG_LENGTH];

int  test_msg_init(void);
void test_msg_reset(void);

static inline uint64_t
read_rdtsc(void)
{
    union {
        uint64_t tsc_64;
        struct {
            uint32_t lo_32;
            uint32_t hi_32;
        };
    } tsc;

#ifndef _MSC_VER
    __asm __volatile__ (
        "rdtsc" :
        "=a" (tsc.lo_32),
        "=d" (tsc.hi_32)
    );
#else
  #if defined(_M_X64) || defined(_WIN64)
    return 0ULL;
  #else
    __asm {
        rdtsc
        mov tsc.lo_32, eax
        mov tsc.hi_32, edx
    }
  #endif
#endif

  return tsc.tsc_64;
}

static volatile int quit = 0;
static volatile unsigned int push_total = 0;
static volatile unsigned int pop_total = 0;
static volatile unsigned int pop_fail_total = 0;

static volatile uint64_t push_cycles = 0;
static volatile uint64_t pop_cycles = 0;

/* topology for Xeon E5-2670 Sandybridge */
static const int socket_top[] = {
  1,  2,  3,  4,  5,  6,  7,
  16, 17, 18, 19, 20, 21, 22, 23,
  8,  9,  10, 11, 12, 13, 14, 15,
  24, 25, 26, 27, 28, 29, 30, 31
};

#define CORE_ID(i)    socket_top[(i)]

static void
init_globals(void)
{
    quit = 0;

    push_total = 0;
    pop_total = 0;
    pop_fail_total = 0;

    push_cycles = 0;
    pop_cycles = 0;
}

static void *
PTW32_API
RingQueue_push_task(void *arg)
{
    thread_arg_t *thread_arg;
    RingQueue_t *queue;
    struct queue *q;
    msg_t *msg;
    uint64_t start;
    int i, idx, funcType;
    int32_t pause_cnt;
    uint32_t fail_cnt, loop_cnt, yeild_cnt;
    static const uint32_t YIELD_THRESHOLD = SPIN_YIELD_THRESHOLD;

    idx = 0;
    funcType = 0;
    queue = NULL;
    q = NULL;

    thread_arg = (thread_arg_t *)arg;
    if (thread_arg) {
        idx         = thread_arg->idx;
        funcType    = thread_arg->funcType;
        queue       = (RingQueue_t *)thread_arg->queue;
        q           = thread_arg->q;
    }

    if (queue == NULL)
        return NULL;

    if (thread_arg)
        free(thread_arg);

    fail_cnt = 0;
    msg = (msg_t *)&msgs[idx * MAX_PUSH_MSG_LENGTH];
    start = read_rdtsc();

#if defined(USE_FUNC_TYPE) && (USE_FUNC_TYPE != 0)
    if (funcType == 1) {
        // 细粒度的标准spin_mutex自旋锁
        for (i = 0; i < MAX_PUSH_MSG_LENGTH; i++) {
            while (queue->spin_push(msg) == -1) {
                fail_cnt++;
            };
            msg++;
        }
    }
    else if (funcType == 2) {
        // 细粒度的改进型spin_mutex自旋锁
        for (i = 0; i < MAX_PUSH_MSG_LENGTH; i++) {
            while (queue->spin1_push(msg) == -1) {
                fail_cnt++;
            };
            msg++;
        }
    }
    else if (funcType == 3) {
        // 细粒度的通用型spin_mutex自旋锁
        for (i = 0; i < MAX_PUSH_MSG_LENGTH; i++) {
            loop_cnt = 0;
            while (queue->spin2_push(msg) == -1) {
#if 1
                if (loop_cnt >= YIELD_THRESHOLD) {
                    yeild_cnt = loop_cnt - YIELD_THRESHOLD;
                    if ((yeild_cnt & 63) == 63) {
                        jimi_wsleep(1);
                    }
                    else if ((yeild_cnt & 3) == 3) {
                        jimi_wsleep(0);
                    }
                    else {
                        if (!jimi_yield()) {
                            jimi_wsleep(0);
                            //jimi_mm_pause();
                        }
                    }
                }
                else {
                    for (pause_cnt = 1; pause_cnt > 0; --pause_cnt) {
                        jimi_mm_pause();
                    }
                }
                loop_cnt++;
#elif 1
                jimi_wsleep(0);
                //jimi_mm_pause();
                //jimi_mm_pause();
#else
                jimi_mm_pause();
                jimi_mm_pause();
                jimi_mm_pause();
                jimi_mm_pause();
#endif
                fail_cnt++;
            };
            msg++;
        }
    }
    else if (funcType == 4) {
        // 粗粒度的pthread_mutex_t锁(Windows上为临界区, Linux上为pthread_mutex_t)
        for (i = 0; i < MAX_PUSH_MSG_LENGTH; i++) {
            while (queue->mutex_push(msg) == -1) {
                fail_cnt++;
            };
            msg++;
        }
    }
    else if (funcType == 5) {
        // 豆瓣上q3.h的原版文件
        for (i = 0; i < MAX_PUSH_MSG_LENGTH; i++) {
            while (push(q, (void *)msg) == -1) {
                fail_cnt++;
            };
            msg++;
        }
    }
    else if (funcType == 9) {
        // 细粒度的仿制spin_mutex自旋锁(会死锁)
        for (i = 0; i < MAX_PUSH_MSG_LENGTH; i++) {
            while (queue->spin3_push(msg) == -1) {
                fail_cnt++;
            };
            msg++;
        }
    }
    else {
        // 豆瓣上q3.h的lock-free改良型方案
        for (i = 0; i < MAX_PUSH_MSG_LENGTH; i++) {
            while (queue->push(msg) == -1) {
                fail_cnt++;
            };
            msg++;
        }
    }

#else  /* !USE_FUNC_TYPE */

    for (i = 0; i < MAX_PUSH_MSG_LENGTH; i++) {
#if defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 1)
        while (queue->spin_push(msg) == -1) { fail_cnt++; };
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 2)
        while (queue->spin1_push(msg) == -1) { fail_cnt++; };
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 3)
        while (queue->spin2_push(msg) == -1) { fail_cnt++; };
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 4)
        while (queue->mutex_push(msg) == -1) { fail_cnt++; };
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 5)
        while (push(q, (void *)msg) == -1) { fail_cnt++; };
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 9)
        while (queue->spin3_push(msg) == -1) { fail_cnt++; };
#else
        while (queue->push(msg) == -1) { fail_cnt++; };
#endif
        msg++;
#if 0
        //if ((i & 0x3FFFU) == 0x3FFFU) {
        if ((i & 0xFFFFU) == 0xFFFFU) {
            printf("thread [%d] have push %d\n", idx, i);
        }
#endif
    }

#endif  /* USE_FUNC_TYPE */

#if 0
    printf("thread [%d] have push %d\n", idx, i);
#endif

    //push_cycles += read_rdtsc() - start;
    jimi_fetch_and_add64(&push_cycles, read_rdtsc() - start);
    //push_total += MAX_PUSH_MSG_LENGTH;
    jimi_fetch_and_add32(&push_total, fail_cnt);
    if (push_total == MSG_TOTAL_CNT)
        quit = 1;

    return NULL;
}

static void *
PTW32_API
RingQueue_pop_task(void *arg)
{
    thread_arg_t *thread_arg;
    RingQueue_t *queue;
    struct queue *q;
    msg_t *msg;
    msg_t **record_list;
    uint64_t start;
    int idx, funcType;
    int32_t pause_cnt;
    uint32_t cnt, fail_cnt, loop_cnt, yeild_cnt;
    static const uint32_t YIELD_THRESHOLD = SPIN_YIELD_THRESHOLD;

    idx = 0;
    funcType = 0;
    queue = NULL;
    q = NULL;

    thread_arg = (thread_arg_t *)arg;
    if (thread_arg) {
        idx         = thread_arg->idx;
        funcType    = thread_arg->funcType;
        queue       = (RingQueue_t *)thread_arg->queue;
        q           = thread_arg->q;
    }

    if (queue == NULL)
        return NULL;

    if (thread_arg)
        free(thread_arg);

    cnt = 0;
    fail_cnt = 0;
    record_list = &popmsg_list[idx][0];
    //record_list = &popmsg_list[idx * MAX_POP_MSG_LENGTH];
    start = read_rdtsc();

#if defined(USE_FUNC_TYPE) && (USE_FUNC_TYPE != 0)
    if (funcType == 1) {
        // 细粒度的标准spin_mutex自旋锁
        while (true) {
            msg = (msg_t *)queue->spin_pop();
            if (msg != NULL) {
                *record_list++ = (struct msg_t *)msg;
                cnt++;
                if (cnt >= MAX_POP_MSG_LENGTH)
                    break;
            }
            else {
                fail_cnt++;
            }
        }
    }
    else if (funcType == 2) {
        // 细粒度的改进型spin_mutex自旋锁
        while (true) {
            msg = (msg_t *)queue->spin1_pop();
            if (msg != NULL) {
                *record_list++ = (struct msg_t *)msg;
                cnt++;
                if (cnt >= MAX_POP_MSG_LENGTH)
                    break;
            }
            else {
                fail_cnt++;
            }
        }
    }
    else if (funcType == 3) {
        // 细粒度的通用型spin_mutex自旋锁
        loop_cnt = 0;
        while (true) {
            msg = (msg_t *)queue->spin2_pop();
            if (msg != NULL) {
                *record_list++ = (struct msg_t *)msg;
                loop_cnt = 0;
                cnt++;
                if (cnt >= MAX_POP_MSG_LENGTH)
                    break;
            }
            else {
                fail_cnt++;
#if 1
                if (loop_cnt >= YIELD_THRESHOLD) {
                    yeild_cnt = loop_cnt - YIELD_THRESHOLD;
                    if ((yeild_cnt & 63) == 63) {
                        jimi_wsleep(1);
                    }
                    else if ((yeild_cnt & 3) == 3) {
                        jimi_wsleep(0);
                    }
                    else {
                        if (!jimi_yield()) {
                            jimi_wsleep(0);
                            //jimi_mm_pause();
                        }
                    }
                }
                else {
                    for (pause_cnt = 1; pause_cnt > 0; --pause_cnt) {
                        jimi_mm_pause();
                    }
                }
                loop_cnt++;
#elif 1
                jimi_wsleep(0);
                //jimi_mm_pause();
                //jimi_mm_pause();
#else
                jimi_mm_pause();
                jimi_mm_pause();
                jimi_mm_pause();
                jimi_mm_pause();
#endif
            }
        }
    }
    else if (funcType == 4) {
        // 粗粒度的pthread_mutex_t锁(Windows上为临界区, Linux上为pthread_mutex_t)
        while (true) {
            msg = (msg_t *)queue->mutex_pop();
            if (msg != NULL) {
                *record_list++ = (struct msg_t *)msg;
                cnt++;
                if (cnt >= MAX_POP_MSG_LENGTH)
                    break;
            }
            else {
                fail_cnt++;
            }
        }
    }
    else if (funcType == 5) {
        // 豆瓣上q3.h的原版文件
        while (true) {
            msg = (struct msg_t *)pop(q);
            if (msg != NULL) {
                *record_list++ = (struct msg_t *)msg;
                cnt++;
                if (cnt >= MAX_POP_MSG_LENGTH)
                    break;
            }
            else {
                fail_cnt++;
            }
        }
    }
    else if (funcType == 9) {
        // 细粒度的仿制spin_mutex自旋锁(会死锁)
        while (true) {
            msg = (msg_t *)queue->spin3_pop();
            if (msg != NULL) {
                *record_list++ = (struct msg_t *)msg;
                cnt++;
                if (cnt >= MAX_POP_MSG_LENGTH)
                    break;
            }
            else {
                fail_cnt++;
            }
        }
    }
    else {
        // 豆瓣上q3.h的lock-free改良型方案
        while (true) {
            msg = (msg_t *)queue->pop();
            if (msg != NULL) {
                *record_list++ = (struct msg_t *)msg;
                cnt++;
                if (cnt >= MAX_POP_MSG_LENGTH)
                    break;
            }
            else {
                fail_cnt++;
            }
        }
    }

#else  /* !USE_FUNC_TYPE */

    while (true || !quit) {
#if defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 1)
        msg = (msg_t *)queue->spin_pop();
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 2)
        msg = (msg_t *)queue->spin1_pop();
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 3)
        msg = (msg_t *)queue->spin2_pop();
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 4)
        msg = (msg_t *)queue->mutex_pop();
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 5)
        msg = (msg_t *)pop(q);
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 9)
        msg = (msg_t *)queue->spin3_pop();
#else
        msg = (msg_t *)queue->pop();
#endif
        if (msg != NULL) {
            *record_list++ = (struct msg_t *)msg;
            cnt++;
            if (cnt >= MAX_POP_MSG_LENGTH)
                break;
        }
    }

#endif  /* USE_FUNC_TYPE */

    //pop_cycles += read_rdtsc() - start;
    jimi_fetch_and_add64(&pop_cycles, read_rdtsc() - start);
    //pop_total += cnt;
    jimi_fetch_and_add32(&pop_total, cnt);
    //pop_fail_total += cnt;
    jimi_fetch_and_add32(&pop_fail_total, fail_cnt);

    return NULL;
}

static int
RingQueue_start_thread(int id,
                       void *(PTW32_API *cb)(void *),
                       void *arg,
                       pthread_t *tid)
{
    pthread_t kid;
    pthread_attr_t attr;
    int retval;
#if (defined(USE_THREAD_AFFINITY) && (USE_THREAD_AFFINITY != 0))
    cpu_set_t cpuset;
    int core_id;
#endif

#if (defined(USE_THREAD_AFFINITY) && (USE_THREAD_AFFINITY != 0))
    if (id < 0 || id >= sizeof(socket_top) / sizeof(int))
        return -1;
#endif

    ///
    /// pthread_attr_init() 在 cygwin 或 MinGW + MSYS 环境下可能返回错误代码 16 (EBUSY)
    ///
    /// See: http://www.impredicative.com/pipermail/ur/2013-October/001461.html
    ///
    if (retval = pthread_attr_init(&attr)) {
        if (retval != EBUSY) {
            //printf("retval = %04d, attr = %016p\n", retval, &attr);
            return -1;
        }
    }

    //printf("retval = %04d, attr = %016p\n", retval, &attr);

#if (defined(USE_THREAD_AFFINITY) && (USE_THREAD_AFFINITY != 0))
    CPU_ZERO(&cpuset);
    //core_id = CORE_ID(id);
    core_id = id % jimi_get_processor_num();
    //printf("id = %d, core_id = %d.\n", core_id, id);
    CPU_SET(core_id, &cpuset);
#endif

    if (retval = pthread_create(&kid, &attr, cb, arg))
        return -1;

    if (tid)
        *tid = kid;

    //printf("retval = %04d, kid = %016p\n", retval, &kid);

#if (defined(USE_THREAD_AFFINITY) && (USE_THREAD_AFFINITY != 0))
    if (pthread_setaffinity_np(kid, sizeof(cpu_set_t), &cpuset))
        return -1;
#endif

    return 0;
}

static void *
PTW32_API
push_task(void *arg)
{
    struct thread_arg_t *thread_arg;
    struct queue *q;
    struct msg_t *msg;
    uint64_t start;
    int i, idx;
    unsigned int cnt = 0;

    idx = 0;
    q = NULL;

    thread_arg = (thread_arg_t *)arg;
    if (thread_arg) {
        idx = thread_arg->idx;
        q   = thread_arg->q;
    }

    if (q == NULL)
        return NULL;

    if (thread_arg)
        free(thread_arg);

    msg = (struct msg_t *)&msgs[idx * MAX_PUSH_MSG_LENGTH];
    start = read_rdtsc();

    for (i = 0; i < MAX_PUSH_MSG_LENGTH; i++) {
        while (push(q, (void *)msg) == -1) {
            cnt++;
        };
        msg++;
    }

    //push_cycles += read_rdtsc() - start;
    jimi_fetch_and_add64(&push_cycles, read_rdtsc() - start);
    //push_total += MAX_PUSH_MSG_LENGTH;
    jimi_fetch_and_add32(&push_total, cnt);
    if (push_total == MSG_TOTAL_CNT)
        quit = 1;

    return NULL;
}

static void *
PTW32_API
pop_task(void *arg)
{
    struct thread_arg_t *thread_arg;
    struct queue *q;
    struct msg_t *msg;
    struct msg_t **record_list;
    uint64_t start;
    int idx;
    unsigned int cnt, fail_cnt;

    idx = 0;
    q = NULL;

    thread_arg = (thread_arg_t *)arg;
    if (thread_arg) {
        idx = thread_arg->idx;
        q   = thread_arg->q;
    }

    if (q == NULL)
        return NULL;

    if (thread_arg)
        free(thread_arg);

    cnt = 0;
    fail_cnt = 0;
    record_list = &popmsg_list[idx][0];
    //record_list = &popmsg_list[idx * MAX_POP_MSG_LENGTH];
    start = read_rdtsc();

    while (true || !quit) {
        msg = (struct msg_t *)pop(q);
        if (msg != NULL) {
            *record_list++ = (struct msg_t *)msg;
            cnt++;
            if (cnt >= MAX_POP_MSG_LENGTH)
                break;
        }
        else {
            fail_cnt++;
        }
    }

    //pop_cycles += read_rdtsc() - start;
    jimi_fetch_and_add64(&pop_cycles, read_rdtsc() - start);
    //pop_total += cnt;
    jimi_fetch_and_add32(&pop_total, cnt);
    //pop_fail_total += cnt;
    jimi_fetch_and_add32(&pop_fail_total, fail_cnt);

    return NULL;
}

static int
start_thread(int id,
             void *(PTW32_API *cb)(void *),
             void *arg,
             pthread_t *tid)
{
    pthread_t kid;
    pthread_attr_t attr;
    int retval;
#if (defined(USE_THREAD_AFFINITY) && (USE_THREAD_AFFINITY != 0))
    cpu_set_t cpuset;
    int core_id;
#endif

#if (defined(USE_THREAD_AFFINITY) && (USE_THREAD_AFFINITY != 0))
    if (id < 0 || id >= sizeof(socket_top) / sizeof(int))
        return -1;
#endif

    ///
    /// pthread_attr_init() 在 cygwin 或 MinGW + MSYS 环境下可能返回错误代码 16 (EBUSY)
    ///
    /// See: http://www.impredicative.com/pipermail/ur/2013-October/001461.html
    ///
    if (retval = pthread_attr_init(&attr)) {
        if (retval != EBUSY) {
            //printf("retval = %04d, attr = %016p\n", retval, &attr);
            return -1;
        }
    }

    //printf("retval = %04d, attr = %016p\n", retval, &attr);

#if (defined(USE_THREAD_AFFINITY) && (USE_THREAD_AFFINITY != 0))
    CPU_ZERO(&cpuset);
    //core_id = CORE_ID(id);
    core_id = id % jimi_get_processor_num();
    //printf("core_id = %d, id = %d.\n", core_id, id);
    CPU_SET(core_id, &cpuset);
#endif

    if (pthread_create(&kid, &attr, cb, arg))
        return -1;

    if (tid)
        *tid = kid;

#if (defined(USE_THREAD_AFFINITY) && (USE_THREAD_AFFINITY != 0))
    if (pthread_setaffinity_np(kid, sizeof(cpu_set_t), &cpuset)) {
        return -1;
    }
#endif

    return 0;
}

static int
setaffinity(int core_id)
{
#if (defined(USE_THREAD_AFFINITY) && (USE_THREAD_AFFINITY != 0))
    cpu_set_t cpuset;
    pthread_t me = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    if (pthread_setaffinity_np(me, sizeof(cpu_set_t), &cpuset))
        return -1;
#endif
    return 0;
}

void pop_list_reset(void)
{
    unsigned int i, j;
    //struct msg_t **cur_popmsg_list = popmsg_list;
    for (i = 0; i < POP_CNT; i++) {
        for (j = 0; j < MAX_POP_MSG_LENGTH; ++j) {
            popmsg_list[i][j] = NULL;
            //*cur_popmsg_list++ = NULL;
        }
    }
}

int pop_list_verify(void)
{
    int i, j;
    uint32_t index;
    uint32_t *verify_list;
    struct msg_t *msg;
    int empty, overlay, correct, errors, times;

    verify_list = (uint32_t *)calloc(MSG_TOTAL_CNT, sizeof(uint32_t));
    if (verify_list == NULL)
        return -1;

    //printf("popmsg_list = %016p\n", popmsg_list);

    for (i = 0; i < POP_CNT; ++i) {
        for (j = 0; j < MAX_POP_MSG_LENGTH; ++j) {
            //msg = popmsg_list[i * MAX_POP_MSG_LENGTH + j];
            msg = popmsg_list[i][j];
            if (msg != NULL) {
                index = (uint32_t)(msg->dummy - 1);
                if (index < MSG_TOTAL_CNT)
                    verify_list[index] = verify_list[index] + 1;
            }
        }
    }

    empty = 0;
    overlay = 0;
    correct = 0;
    errors = 0;
    for (i = 0; i < MSG_TOTAL_CNT; ++i) {
        times = verify_list[i];
        if (times == 0) {
            empty++;
        }
        else if (times > 1) {
            overlay++;
            if (times >= 3) {
                if (errors == 0)
                    printf("Serious Errors:\n");
                errors++;
                printf("verify_list[%8d] = %d\n", i, times);
            }
        }
        else {
            correct++;
        }
    }

    if (errors > 0)
        printf("\n");
    //printf("pop-list verify result:\n\n");
    printf("empty = %d, overlay = %d, correct = %d, totals = %d.\n\n",
           empty, overlay, correct, empty + overlay + correct);

    if (verify_list)
        free(verify_list);

    //jimi_console_readkeyln(false, true, false);

    return correct;
}

void
RingQueue_Test(int funcType, bool bContinue = true)
{
    RingQueue_t ringQueue(true, true);
    struct queue *q;
    int i;
    pthread_t kids[POP_CNT + PUSH_CNT] = { 0 };
    thread_arg_t *thread_arg;
    jmc_timestamp_t startTime, stopTime;
    jmc_timefloat_t elapsedTime = 0.0;

    q = qinit();

    printf("---------------------------------------------------------------\n");

    if (funcType == 1) {
        // 细粒度的标准spin_mutex自旋锁
        printf("This is RingQueue.spin_push() test: (%d)\n", funcType);
    }
    else if (funcType == 2) {
        // 细粒度的改进型spin_mutex自旋锁
        printf("This is RingQueue.spin1_push() test: (%d)\n", funcType);
    }
    else if (funcType == 3) {
        // 细粒度的通用型spin_mutex自旋锁
        printf("This is RingQueue.spin2_push() test: (%d)\n", funcType);
    }
    else if (funcType == 4) {
        // 粗粒度的pthread_mutex_t锁(Windows上为临界区, Linux上为pthread_mutex_t)
        printf("This is RingQueue.mutex_push() test: (%d)\n", funcType);
    }
    else if (funcType == 5) {
        // 豆瓣上q3.h的原版文件
        printf("This is DouBan's q3.h test: (%d)\n", funcType);
    }
    else if (funcType == 9) {
        // 细粒度的仿制spin_mutex自旋锁(会死锁)
        printf("This is RingQueue.spin3_push() test (maybe deadlock): (%d)\n", funcType);
    }
    else {
        // 豆瓣上q3.h的lock-free改良型方案
        printf("This is RingQueue.push() test (modified base on q3.h): (%d)\n", funcType);
    }

#if 0
    //printf("\n");
#if defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 1)
    printf("This is RingQueue.spin_push() test: (%d)\n", funcType);
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 2)
    printf("This is RingQueue.spin1_push() test: (%d)\n", funcType);
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 3)
    printf("This is RingQueue.spin2_push() test: (%d)\n", funcType);
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 4)
    printf("This is RingQueue.mutex_push() test: (%d)\n", funcType);
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 5)
    printf("This is DouBan's q3.h test: (%d)\n", funcType);
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 9)
    printf("This is RingQueue.spin3_push() test (maybe deadlock): (%d)\n", funcType);
#else
    printf("This is RingQueue.push() test (modified base on q3.h): (%d)\n", funcType);
#endif
#endif

    printf("---------------------------------------------------------------\n");

    init_globals();
    setaffinity(0);

    test_msg_reset();
    pop_list_reset();

    startTime = jmc_get_timestamp();

    for (i = 0; i < PUSH_CNT; i++) {
        thread_arg = (thread_arg_t *)malloc(sizeof(struct thread_arg_t));
        thread_arg->idx = i;
        thread_arg->funcType = funcType;
        thread_arg->queue = &ringQueue;
        thread_arg->q = q;
        RingQueue_start_thread(i, RingQueue_push_task, (void *)thread_arg, &kids[i]);
    }
    for (i = 0; i < POP_CNT; i++) {
        thread_arg = (thread_arg_t *)malloc(sizeof(struct thread_arg_t));
        thread_arg->idx = i;
        thread_arg->funcType = funcType;
        thread_arg->queue = &ringQueue;
        thread_arg->q = q;
        RingQueue_start_thread(i + PUSH_CNT, RingQueue_pop_task, (void *)thread_arg,
                               &kids[i + PUSH_CNT]);
    }
    for (i = 0; i < POP_CNT + PUSH_CNT; i++)
        pthread_join(kids[i], NULL);

    stopTime = jmc_get_timestamp();
    elapsedTime += jmc_get_interval_millisecf(stopTime - startTime);

#if defined(DISPLAY_PUSH_POP_DATA) && (DISPLAY_PUSH_POP_DATA != 0)
    printf("\n");
    printf("push total: %u + %u\n", MSG_TOTAL_CNT, push_total);
    printf("push cycles/msg: %"PRIuFAST64"\n", push_cycles / MSG_TOTAL_CNT);
    printf("pop  total: %u + %u\n", pop_total, pop_fail_total);
    if (pop_total == 0)
        printf("pop  cycles/msg: %"PRIuFAST64"\n", 0ULL);
    else
        printf("pop  cycles/msg: %"PRIuFAST64"\n", pop_cycles / pop_total);
#endif  /* DISPLAY_PUSH_POP_DATA */

    //printf("---------------------------------------------------------------\n");

    printf("\n");
    printf("time elapsed: %9.3f ms\n\n", elapsedTime);

    //jimi_console_readkeyln(false, true, false);

    pop_list_verify();

    //printf("---------------------------------------------------------------\n\n");

#if 0
    for (j = 0; j < POP_CNT; ++j) {
        for (i = 0; i <= 256; ++i) {
            printf("pop_list[%2d, %3d] = ptr: 0x%08p, %02"PRIuFAST64" : %"PRIuFAST64"\n", j, i,
                   (struct msg_t *)(popmsg_list[j][i]),
                   popmsg_list[j][i]->dummy / (MAX_PUSH_MSG_LENGTH),
                   popmsg_list[j][i]->dummy % (MAX_PUSH_MSG_LENGTH));
        }
        printf("\n");
        if (j < (POP_CNT - 1)) {
            jimi_console_readkeyln(false, true, false);
        }
    }
    printf("\n");
#endif

    qfree(q);

    // 如果不需要"press any key to continue..."提示则直接退出
    if (!bContinue) {
        printf("---------------------------------------------------------------\n\n");

#if !defined(USE_DOUBAN_QUEUE) || (USE_DOUBAN_QUEUE == 0)
        jimi_console_readkeyln(false, true, false);
#else
        jimi_console_readkeyln(false, true, false);
#endif
    }
}

void
q3_test(void)
{
    struct queue *q;
    int i;
    pthread_t kids[POP_CNT + PUSH_CNT] = { 0 };
    thread_arg_t *thread_arg;
    jmc_timestamp_t startTime, stopTime;
    jmc_timefloat_t elapsedTime = 0.0;

    q = qinit();

    printf("---------------------------------------------------------------\n");
    printf("This is DouBan's q3.h test:\n");
    printf("---------------------------------------------------------------\n");

    init_globals();
    setaffinity(0);

    test_msg_reset();
    pop_list_reset();

    for (i = 0; i < MSG_TOTAL_CNT; i++)
        msgs[i].dummy = (uint64_t)(i + 1);

    startTime = jmc_get_timestamp();

    for (i = 0; i < PUSH_CNT; i++) {
        thread_arg = (thread_arg_t *)malloc(sizeof(struct thread_arg_t));
        thread_arg->idx = i;
        thread_arg->funcType = 5;
        thread_arg->queue = NULL;
        thread_arg->q = q;
        start_thread(i, push_task, (void *)thread_arg, &kids[i]);
    }
    for (i = 0; i < POP_CNT; i++) {
        thread_arg = (thread_arg_t *)malloc(sizeof(struct thread_arg_t));
        thread_arg->idx = i;
        thread_arg->funcType = 5;
        thread_arg->queue = NULL;
        thread_arg->q = q;
        start_thread(i + PUSH_CNT, pop_task, (void *)thread_arg, &kids[i + PUSH_CNT]);
    }
    for (i = 0; i < POP_CNT + PUSH_CNT; i++)
        pthread_join(kids[i], NULL);

    stopTime = jmc_get_timestamp();
    elapsedTime += jmc_get_interval_millisecf(stopTime - startTime);

#if defined(DISPLAY_PUSH_POP_DATA) && (DISPLAY_PUSH_POP_DATA != 0)
    printf("\n");
    printf("push total: %u + %u\n", MSG_TOTAL_CNT, push_total);
    printf("push cycles/msg: %"PRIuFAST64"\n", push_cycles / MSG_TOTAL_CNT);
    printf("pop  total: %u + %u\n", pop_total, pop_fail_total);
    if (pop_total == 0)
        printf("pop  cycles/msg: %"PRIuFAST64"\n", 0ULL);
    else
        printf("pop  cycles/msg: %"PRIuFAST64"\n", pop_cycles / pop_total);
#endif  /* DISPLAY_PUSH_POP_DATA */

    printf("\n");
    printf("time elapsed: %9.3f ms\n\n", elapsedTime);

    pop_list_verify();

    qfree(q);

    printf("---------------------------------------------------------------\n");

    jimi_console_readkeyln(false, true, false);
}

void
RingQueue_UnitTest(void)
{
    RingQueue_t ringQueue(true, true);
    msg_t queue_msg = { 123ULL };

    init_globals();

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

    jimi_console_readkeyln(true, true, false);
}

int
test_msg_init(void)
{
    unsigned int i;
    if (msgs != NULL)
        return -1;

    msgs = (struct msg_t *)calloc(MSG_TOTAL_CNT, sizeof(struct msg_t));
    if (msgs != NULL) {
        for (i = 0; i < MSG_TOTAL_CNT; i++)
            msgs[i].dummy = (uint64_t)(i + 1);
        return 1;
    }
    else return 0;
}

void
test_msg_reset(void)
{
    unsigned int i;
    if (msgs == NULL)
        test_msg_init();

    if (msgs != NULL) {
        for (i = 0; i < MSG_TOTAL_CNT; i++)
            msgs[i].dummy = (uint64_t)(i + 1);
    }
}

void
test_msg_destory(void)
{
    if (msgs) {
        free((void *)msgs);
        msgs = NULL;
    }
}

int
popmsg_list_init(void)
{
#if 0
    if (popmsg_list != NULL)
        return -1;

    popmsg_list = (struct msg_t **)calloc(MSG_TOTAL_CNT, sizeof(struct msg_t *));
    if (popmsg_list != NULL)
        return 1;
    else
        return 0;
#else
    return 1;
#endif
}

void
popmsg_list_destory(void)
{
#if 0
    if (popmsg_list) {
        free((void *)popmsg_list);
        popmsg_list = NULL;
    }
#endif
}

void
display_test_info(void)
{
    //printf("\n");
    printf("PUSH_CNT            = %u\n"
           "POP_CNT             = %u\n"
           "MSG_TOTAL_LENGTH    = %u\n", PUSH_CNT, POP_CNT, MSG_TOTAL_LENGTH);
#if defined(_M_X64) || defined(_WIN64)
    printf("x64 Mode            = Yes\n");
#else
    printf("x64 Mode            = No\n");
#endif
#if defined(USE_THREAD_AFFINITY) && (USE_THREAD_AFFINITY != 0)
    printf("USE_THREAD_AFFINITY = Yes\n");
#else
    printf("USE_THREAD_AFFINITY = No\n");
#endif
    printf("\n");

#if 0
#if defined(__linux__)
#if defined(_M_X64) || defined(_WIN64)
    printf("msgs ptr         = %016p\n", (void *)msgs);
#else
    printf("msgs ptr         = %08p\n", (void *)msgs);
#endif
#else  /* !__linux__ */
#if defined(_M_X64) || defined(_WIN64)
    printf("msgs ptr         = 0x%016p\n", (void *)msgs);
#else
    printf("msgs ptr         = 0x%08p\n", (void *)msgs);
#endif  /* _M_X64 || _WIN64 */
#endif  /* __linux__ */

    printf("\n");
#endif
}

void SpinMutex_Test(void)
{
    SpinMutex<DefaultSMHelper> spinMutex2;

    typedef SpinMutexHelper<
        5,      // _YieldThreshold, The threshold of enter yield(), the spin loop times.
        16,     // _SpinCount, The initial value of spin counter.
        2,      // _A
        1,      // _B
        0,      // _C, Next loop: spin_count = spin_count * _A / _B + _C;
        true,   // _UseYield? Whether use yield() function in loop.
        false   // _NeedReset? After run Sleep(1), reset the loop_count if need.
    > MySpinMutexHelper;

    SpinMutex<MySpinMutexHelper> spinMutex;

    spinMutex.lock();
    spinMutex.unlock();

    spinMutex.lock();
    spinMutex.unlock();

    spinMutex.spinWait(4000);

    printf("\n");

    jimi_console_readkeyln(false, true, false);
}

int
main(int argn, char * argv[])
{
#if defined(USE_DOUBAN_QUEUE) && (USE_DOUBAN_QUEUE != 0)
    bool bConti = true;
#else
    bool bConti = false;
#endif
    jimi_cpu_warmup(500);

    test_msg_init();
    popmsg_list_init();

    display_test_info();

    //RingQueue_Test(0, true);

#if defined(USE_JIMI_RINGQUEUE) && (USE_JIMI_RINGQUEUE != 0)
  #if defined(USE_FUNC_TYPE) && (USE_FUNC_TYPE != 0)
    //RingQueue_UnitTest();

    //RingQueue_Test(3, true);

    RingQueue_Test(4, true);    // 使用pthread_mutex_t, 调用RingQueue.mutex_push().

    RingQueue_Test(1, true);    // 使用自旋锁, 调用RingQueue.spin_push().
    RingQueue_Test(2, true);    //             调用RingQueue.spin1_push().
    RingQueue_Test(3, bConti);  //             调用RingQueue.spin2_push().
  #else
    // 根据指定的 RINGQUEUE_LOCK_TYPE 执行RingQueue相应的push()和pop()函数
    RingQueue_Test(RINGQUEUE_LOCK_TYPE, bConti);
  #endif
#endif

    //RingQueue_Test(0, true);

    //SpinMutex_Test();

#if defined(USE_DOUBAN_QUEUE) && (USE_DOUBAN_QUEUE != 0)
    // 豆瓣上的 q3.h 的修正版
    q3_test();
    //RingQueue_Test(5, false);
#endif

    popmsg_list_destory();
    test_msg_destory();

    //jimi_console_readkeyln(false, true, false);
    return 0;
}
