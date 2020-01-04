
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef __USE_GNU
#define __USE_GNU
#endif

#define __STDC_LIMIT_MACROS     // for INT64_MAX

#include "msvc/targetver.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>       // For gcc's mktime() and vc++'s _mktime32()
#include "vs_stdint.h"

#ifndef _MSC_VER
#include <sched.h>
#include <pthread.h>
#include "msvc/sched.h"
#include "msvc/pthread.h"       // For define PTW32_API
#else
#include "msvc/sched.h"
#include "msvc/pthread.h"
#endif  // _MSC_VER

#define __STDC_FORMAT_MACROS
#include "vs_inttypes.h"

#include "port.h"
#include "Attributes.h"
#include "q3.h"
//#include "q.h"
//#include "qlock.h"

#include "get_char.h"
#include "sys_timer.h"
#include "console.h"

#include "RingQueue.h"
#include "SerialRingQueue.h"
#include "SingleRingQueue.h"

#include "MessageEvent.h"
#include "DisruptorRingQueue.h"
#include "DisruptorRingQueueOld.h"
#include "DisruptorRingQueueEx.h"

#include "SpinMutex.h"

//#include <vld.h>
#include <errno.h>

#if (defined(_WIN32) || defined(__MINGW32__) || defined(__CYGWIN__))

#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#elif defined(__MINGW32__) || defined(__CYGWIN__)

#ifndef MMSYSERR_BASE
#define MMSYSERR_BASE          0
#endif

#ifndef TIMERR_BASE
#define TIMERR_BASE            96
#endif

/* timer error return values */
#define TIMERR_NOERROR        (0)                  /* no error */
#define TIMERR_NOCANDO        (TIMERR_BASE + 1)    /* request not completed */
#define TIMERR_STRUCT         (TIMERR_BASE + 33)   /* time struct size */

/* general error return values */
#define MMSYSERR_NOERROR      0                    /* no error */
#define MMSYSERR_ERROR        (MMSYSERR_BASE + 1)  /* unspecified error */

#ifdef __cplusplus
extern "C" {
#endif

/* timer device capabilities data structure */
typedef struct timecaps_tag {
    UINT    wPeriodMin;     /* minimum period supported  */
    UINT    wPeriodMax;     /* maximum period supported  */
} TIMECAPS, *PTIMECAPS, *NPTIMECAPS, *LPTIMECAPS;

#ifndef _MMRESULT_
typedef UINT    MMRESULT;   /* error return code, 0 means no error */
#define _MMRESULT_
#endif  /* _MMRESULT_ */

__declspec(dllimport) MMRESULT __stdcall timeGetDevCaps(LPTIMECAPS ptc, UINT cbtc);
__declspec(dllimport) MMRESULT __stdcall timeBeginPeriod(UINT uPeriod);
__declspec(dllimport) MMRESULT __stdcall timeEndPeriod(UINT uPeriod);

#ifdef __cplusplus
}
#endif

#endif  /* defined(__MINGW32__) || defined(__CYGWIN__) */

using namespace jimi;

typedef RingQueue<message_t, QSIZE> RingQueue_t;

typedef CValueEvent<uint64_t>   ValueEvent_t;

#if defined(USE_64BIT_SEQUENCE) && (USE_64BIT_SEQUENCE != 0)
typedef DisruptorRingQueue<ValueEvent_t, int64_t, QSIZE, PUSH_CNT, POP_CNT> DisruptorRingQueue_t;
typedef DisruptorRingQueueEx<ValueEvent_t, int64_t, QSIZE, PUSH_CNT, POP_CNT> DisruptorRingQueueEx_t;
#else
typedef DisruptorRingQueue<ValueEvent_t, int32_t, QSIZE, PUSH_CNT, POP_CNT> DisruptorRingQueue_t;
typedef DisruptorRingQueueEx<ValueEvent_t, int32_t, QSIZE, PUSH_CNT, POP_CNT> DisruptorRingQueueEx_t;
#endif

typedef SingleRingQueue<ValueEvent_t, uint32_t, QSIZE> SingleRingQueue_t;

typedef struct thread_arg_t
{
    int     idx;
    int     funcType;
    void *  queue;
} thread_arg_t;

static volatile struct message_t *msgs = NULL;
static volatile ValueEvent_t *valEvents = NULL;

//static struct msg_t **popmsg_list = NULL;

static struct message_t *popmsg_list[POP_CNT][MAX_POP_MSG_COUNT];
static ValueEvent_t dis_popevent_list[POP_CNT][MAX_POP_MSG_COUNT];

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
  #if defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) || defined(_M_IA64) || defined(__amd64__) || defined(__x86_64__)
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
static volatile unsigned int push_fail_total = 0;
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
    push_fail_total = 0;
    pop_total = 0;
    pop_fail_total = 0;

    push_cycles = 0;
    pop_cycles = 0;
}

static void *
PTW32_API
RingQueue_push_task(void * arg)
{
    thread_arg_t *thread_arg;
    struct queue *q;
    RingQueue_t *queue;
    DisruptorRingQueue_t *disRingQueue;
    DisruptorRingQueueEx_t *disRingQueueEx;
    message_t *msg;
    ValueEvent_t *valueEvent = NULL;
    uint64_t start;
    int i, idx, funcType;

#if (!defined(TEST_FUNC_TYPE) || (TEST_FUNC_TYPE == 0)) \
    || (defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN2_PUSH \
    || TEST_FUNC_TYPE == FUNC_DISRUPTOR_RINGQUEUE || TEST_FUNC_TYPE == FUNC_DISRUPTOR_RINGQUEUE_EX))
    int32_t pause_cnt;
    uint32_t loop_cnt, yeild_cnt, spin_cnt = 1;
#endif
    uint32_t fail_cnt;
    static const uint32_t YIELD_THRESHOLD = SPIN_YIELD_THRESHOLD;

    idx = 0;
    funcType = 0;
    q = NULL;
    queue = NULL;
    disRingQueue = NULL;
    disRingQueueEx = NULL;

    thread_arg = (thread_arg_t *)arg;
    if (thread_arg) {
        idx          = thread_arg->idx;
        funcType     = thread_arg->funcType;
        if (funcType == FUNC_DOUBAN_Q3H) {
            q = (struct queue *)thread_arg->queue;
            if (q == NULL)
                return NULL;
        }
        else if (funcType == FUNC_DISRUPTOR_RINGQUEUE) {
            disRingQueue = (DisruptorRingQueue_t *)thread_arg->queue;
            if (disRingQueue == NULL)
                return NULL;
        }
        else if (funcType == FUNC_DISRUPTOR_RINGQUEUE_EX) {
            disRingQueueEx = (DisruptorRingQueueEx_t *)thread_arg->queue;
            if (disRingQueueEx == NULL)
                return NULL;
        }
        else {
            queue = (RingQueue_t *)thread_arg->queue;
            if (queue == NULL)
                return NULL;
        }
    }

    if (thread_arg)
        free(thread_arg);

#if (defined(USE_TIME_PERIOD) && (USE_TIME_PERIOD != 0)) && (0) \
    && (defined(_WIN32) || defined(__MINGW32__) || defined(__CYGWIN__))
    TIMECAPS tc;
    MMRESULT mm_result, time_result;
    time_result = TIMERR_NOCANDO;
    tc.wPeriodMin = 0;
    tc.wPeriodMax = 0;
    // See: http://msdn.microsoft.com/zh-cn/dd757627%28v=vs.85%29
    mm_result = timeGetDevCaps(&tc, sizeof(tc));
#ifdef _DEBUG
    printf("wPeriodMin = %u, wPeriodMax = %u\n", tc.wPeriodMin, tc.wPeriodMax);
#endif
    if (mm_result == MMSYSERR_NOERROR) {
        // Returns TIMERR_NOERROR if successful
        // or TIMERR_NOCANDO if the resolution specified in uPeriod is out of range.
        // See: http://msdn.microsoft.com/zh-cn/dd757624%28v=vs.85%29
        time_result = timeBeginPeriod(tc.wPeriodMin);
#ifdef _DEBUG
        if (time_result == TIMERR_NOERROR)
            printf("timeBeginPeriod(%u) = TIMERR_NOERROR\n", tc.wPeriodMin);
        else
            printf("timeBeginPeriod(%u) = %u\n", tc.wPeriodMin, time_result);
#endif
    }
#endif

    fail_cnt = 0;
    msg = (message_t *)&msgs[idx * MAX_PUSH_MSG_COUNT];
    valueEvent = (ValueEvent_t *)&msgs[idx * MAX_PUSH_MSG_COUNT];
    start = read_rdtsc();

#if !defined(TEST_FUNC_TYPE) || (TEST_FUNC_TYPE == 0)
    if (funcType == FUNC_RINGQUEUE_SPIN_PUSH) {
        // 细粒度的标准spin_mutex自旋锁
        for (i = 0; i < MAX_PUSH_MSG_COUNT; ++i) {
            while (queue->spin_push(msg) == -1) {
                fail_cnt++;
            };
            msg++;
        }
    }
    else if (funcType == FUNC_RINGQUEUE_SPIN1_PUSH) {
        // 细粒度的改进型spin_mutex自旋锁
        for (i = 0; i < MAX_PUSH_MSG_COUNT; ++i) {
            while (queue->spin1_push(msg) == -1) {
                fail_cnt++;
            };
            msg++;
        }
    }
    else if (funcType == FUNC_RINGQUEUE_SPIN2_PUSH) {
        // 细粒度的通用型spin_mutex自旋锁
        for (i = 0; i < MAX_PUSH_MSG_COUNT; ++i) {
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
#elif 0
                jimi_wsleep(0);
#endif
                fail_cnt++;
            };
            msg++;
        }
    }
    else if (funcType == FUNC_RINGQUEUE_MUTEX_PUSH) {
        // 粗粒度的pthread_mutex_t锁(Windows上为临界区, Linux上为pthread_mutex_t)
        for (i = 0; i < MAX_PUSH_MSG_COUNT; ++i) {
            while (queue->mutex_push(msg) == -1) {
                fail_cnt++;
            };
            msg++;
        }
    }
    else if (funcType == FUNC_DOUBAN_Q3H) {
        // 豆瓣上q3.h的原版文件
        for (i = 0; i < MAX_PUSH_MSG_COUNT; ++i) {
            while (push(q, (void *)msg) == -1) {
                fail_cnt++;
            };
            msg++;
        }
    }
    else if (funcType == FUNC_RINGQUEUE_SPIN3_PUSH) {
        // 细粒度的通用型spin_mutex自旋锁
        for (i = 0; i < MAX_PUSH_MSG_COUNT; ++i) {
            loop_cnt = 0;
            while (queue->spin3_push(msg) == -1) {
#if 0
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
#endif
                fail_cnt++;
            };
            msg++;
        }
    }
    else if (funcType == FUNC_RINGQUEUE_SPIN9_PUSH) {
        // 细粒度的仿制spin_mutex自旋锁(会死锁)
        for (i = 0; i < MAX_PUSH_MSG_COUNT; ++i) {
            while (queue->spin9_push(msg) == -1) {
                fail_cnt++;
            };
            msg++;
        }
    }
    else if (funcType == FUNC_RINGQUEUE_PUSH) {
        // 豆瓣上q3.h的lock-free改良型方案
        for (i = 0; i < MAX_PUSH_MSG_COUNT; ++i) {
            while (queue->push(msg) == -1) {
                fail_cnt++;
            };
            msg++;
        }
    }
    else if (funcType == FUNC_DISRUPTOR_RINGQUEUE) {
        // disruptor 3.3 (C++版)
        static const uint32_t DISRUPTOR_YIELD_THRESHOLD = 20;
        for (i = 0; i < MAX_PUSH_MSG_COUNT; ++i) {
            loop_cnt = 0;
            spin_cnt = 1;
            while (disRingQueue->push(*valueEvent) == -1) {
#if 1
                if (loop_cnt >= DISRUPTOR_YIELD_THRESHOLD) {
                    yeild_cnt = loop_cnt - DISRUPTOR_YIELD_THRESHOLD;
                    if ((yeild_cnt & 63) == 63) {
                        jimi_wsleep(1);
                    }
                    else if ((yeild_cnt & 3) == 3) {
                        jimi_wsleep(0);
                    }
                    else {
                        if (!jimi_yield()) {
                            jimi_wsleep(0);
                        }
                    }
                }
                else {
                    for (pause_cnt = spin_cnt; pause_cnt > 0; --pause_cnt) {
                        jimi_mm_pause();
                    }
                    spin_cnt = spin_cnt + 1;
                }
                loop_cnt++;
#endif
                fail_cnt++;
            };
            valueEvent++;
        }
    }
    else if (funcType == FUNC_DISRUPTOR_RINGQUEUE_EX) {
        // disruptor 3.3 (C++版), 改进版
        static const uint32_t DISRUPTOR_YIELD_THRESHOLD = 20;
        for (i = 0; i < MAX_PUSH_MSG_COUNT; ++i) {
            loop_cnt = 0;
            spin_cnt = 1;
            while (disRingQueueEx->push(*valueEvent) == -1) {
#if 1
                if (loop_cnt >= DISRUPTOR_YIELD_THRESHOLD) {
                    yeild_cnt = loop_cnt - DISRUPTOR_YIELD_THRESHOLD;
                    if ((yeild_cnt & 63) == 63) {
                        jimi_wsleep(1);
                    }
                    else if ((yeild_cnt & 3) == 3) {
                        jimi_wsleep(0);
                    }
                    else {
                        if (!jimi_yield()) {
                            jimi_wsleep(0);
                        }
                    }
                }
                else {
                    for (pause_cnt = spin_cnt; pause_cnt > 0; --pause_cnt) {
                        jimi_mm_pause();
                    }
                    spin_cnt = spin_cnt + 1;
                }
                loop_cnt++;
#endif
                fail_cnt++;
            };
            valueEvent++;
        }
    }
    else {
        // TODO: push() - Unknown test function type
    }

#else  /* !TEST_FUNC_TYPE */

    int push_cnt = 0;
    for (i = 0; i < MAX_PUSH_MSG_COUNT; ++i) {
#if defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN_PUSH)
        while (queue->spin_push(msg) == -1) { fail_cnt++; };
        msg++;
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN1_PUSH)
        while (queue->spin1_push(msg) == -1) { fail_cnt++; };
        msg++;
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN2_PUSH)
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
#endif
            fail_cnt++;
        }
        msg++;
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_MUTEX_PUSH)
        while (queue->mutex_push(msg) == -1) { fail_cnt++; };
        msg++;
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_DOUBAN_Q3H)
        while (push(q, (void *)msg) == -1) { fail_cnt++; };
        msg++;
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN3_PUSH)
        while (queue->spin3_push(msg) == -1) { fail_cnt++; };
        msg++;
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN9_PUSH)
        while (queue->spin9_push(msg) == -1) { fail_cnt++; };
        msg++;
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_PUSH)
        while (queue->push(msg) == -1) { fail_cnt++; };
        msg++;
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_DISRUPTOR_RINGQUEUE)
        static const uint32_t DISRUPTOR_YIELD_THRESHOLD = 20;
        loop_cnt = 0;
        spin_cnt = 1;
        while (disRingQueue->push(*valueEvent) == -1) {
#if 1
            if (loop_cnt >= DISRUPTOR_YIELD_THRESHOLD) {
                yeild_cnt = loop_cnt - DISRUPTOR_YIELD_THRESHOLD;
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
                for (pause_cnt = spin_cnt; pause_cnt > 0; --pause_cnt) {
                    jimi_mm_pause();
                }
                spin_cnt = spin_cnt + 1;
            }
            loop_cnt++;
#endif
            fail_cnt++;
        }
        valueEvent++;
        push_cnt++;
        if (i == MAX_PUSH_MSG_COUNT - 1)
            fail_cnt++;
#if defined(DISPLAY_DEBUG_INFO) && (DISPLAY_DEBUG_INFO != 0)
        if ((i & 0x03FF) == 0x03FF) {
            printf("Push(): Thread id = %2d, count = %8d, fail_cnt = %8d\n", idx, i, fail_cnt);
        }
#endif
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_DISRUPTOR_RINGQUEUE_EX)
        static const uint32_t DISRUPTOR_YIELD_THRESHOLD = 20;
        loop_cnt = 0;
        spin_cnt = 1;
        while (disRingQueueEx->push(*valueEvent) == -1) {
#if 1
            if (loop_cnt >= DISRUPTOR_YIELD_THRESHOLD) {
                yeild_cnt = loop_cnt - DISRUPTOR_YIELD_THRESHOLD;
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
                for (pause_cnt = spin_cnt; pause_cnt > 0; --pause_cnt) {
                    jimi_mm_pause();
                }
                spin_cnt = spin_cnt + 1;
            }
            loop_cnt++;
#endif
            fail_cnt++;
        }
        valueEvent++;
        push_cnt++;
        if (i == MAX_PUSH_MSG_COUNT - 1)
            fail_cnt++;
#else
        // Unknown test function type.
#endif
    }

#endif  /* TEST_FUNC_TYPE */

    //push_cycles += read_rdtsc() - start;
    jimi_fetch_and_add64(&push_cycles, read_rdtsc() - start);
    //push_total += MAX_PUSH_MSG_COUNT;
    jimi_fetch_and_add32(&push_total, MAX_PUSH_MSG_COUNT);
    //push_fail_total += fail_cnt;
    jimi_fetch_and_add32(&push_fail_total, fail_cnt);
    if (push_total == MAX_MSG_CNT)
        quit = 1;

    //printf("Push() Thread %d has exited.\n", idx);

#if (defined(USE_TIME_PERIOD) && (USE_TIME_PERIOD != 0)) && (0) \
    && (defined(_WIN32) || defined(__MINGW32__) || defined(__CYGWIN__))
    if (time_result == TIMERR_NOERROR) {
        time_result = timeEndPeriod(tc.wPeriodMin);
    }
#endif

    return NULL;
}

static void *
PTW32_API
RingQueue_pop_task(void * arg)
{
    thread_arg_t *thread_arg;
    struct queue *q;
    RingQueue_t *queue;
    DisruptorRingQueue_t *disRingQueue;
    DisruptorRingQueueEx_t *disRingQueueEx;
    
    message_t *msg = NULL;
    message_t **record_list;
    ValueEvent_t *valueEvent = NULL;
    ValueEvent_t *dis_record_list;
    uint64_t start;
    int idx, funcType;

#if (!defined(TEST_FUNC_TYPE) || (TEST_FUNC_TYPE == 0)) \
    || (defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN2_PUSH \
    || TEST_FUNC_TYPE == FUNC_DISRUPTOR_RINGQUEUE || TEST_FUNC_TYPE == FUNC_DISRUPTOR_RINGQUEUE_EX))
    int32_t pause_cnt;
    uint32_t loop_cnt, yeild_cnt, spin_cnt = 1;
#endif
    uint32_t pop_cnt, fail_cnt;
    static const uint32_t YIELD_THRESHOLD = SPIN_YIELD_THRESHOLD;

    idx = 0;
    funcType = 0;
    q = NULL;
    queue = NULL;
    disRingQueue = NULL;
    disRingQueueEx = NULL;

    thread_arg = (thread_arg_t *)arg;
    if (thread_arg) {
        idx      = thread_arg->idx;
        funcType = thread_arg->funcType;
        if (funcType == FUNC_DOUBAN_Q3H) {
            q = (struct queue *)thread_arg->queue;
            if (q == NULL)
                return NULL;
        }
        else if (funcType == FUNC_DISRUPTOR_RINGQUEUE) {
            disRingQueue = (DisruptorRingQueue_t *)thread_arg->queue;
            if (disRingQueue == NULL)
                return NULL;
        }
        else if (funcType == FUNC_DISRUPTOR_RINGQUEUE_EX) {
            disRingQueueEx = (DisruptorRingQueueEx_t *)thread_arg->queue;
            if (disRingQueueEx == NULL)
                return NULL;
        }
        else {
            queue = (RingQueue_t *)thread_arg->queue;
            if (queue == NULL)
                return NULL;
        }
    }

    if (thread_arg)
        free(thread_arg);

#if (defined(USE_TIME_PERIOD) && (USE_TIME_PERIOD != 0)) && (0) \
    && (defined(_WIN32) || defined(__MINGW32__) || defined(__CYGWIN__))
    TIMECAPS tc;
    MMRESULT mm_result, time_result;
    time_result = TIMERR_NOCANDO;
    tc.wPeriodMin = 0;
    tc.wPeriodMax = 0;
    // See: http://msdn.microsoft.com/zh-cn/dd757627%28v=vs.85%29
    mm_result = timeGetDevCaps(&tc, sizeof(tc));
#ifdef _DEBUG
    printf("wPeriodMin = %u, wPeriodMax = %u\n", tc.wPeriodMin, tc.wPeriodMax);
#endif
    if (mm_result == MMSYSERR_NOERROR) {
        // Returns TIMERR_NOERROR if successful
        // or TIMERR_NOCANDO if the resolution specified in uPeriod is out of range.
        // See: http://msdn.microsoft.com/zh-cn/dd757624%28v=vs.85%29
        time_result = timeBeginPeriod(tc.wPeriodMin);
#ifdef _DEBUG
        if (time_result == TIMERR_NOERROR)
            printf("timeBeginPeriod(%u) = TIMERR_NOERROR\n", tc.wPeriodMin);
        else
            printf("timeBeginPeriod(%u) = %u\n", tc.wPeriodMin, time_result);
#endif
    }
#endif

    pop_cnt = 0;
    fail_cnt = 0;
    record_list = &popmsg_list[idx][0];
    //record_list = &popmsg_list[idx * MAX_POP_MSG_COUNT];
    dis_record_list = &dis_popevent_list[idx][0];
    start = read_rdtsc();

#if !defined(TEST_FUNC_TYPE) || (TEST_FUNC_TYPE == 0)
    if (funcType == FUNC_RINGQUEUE_SPIN_PUSH) {
        // 细粒度的标准spin_mutex自旋锁
        while (true) {
            msg = (message_t *)queue->spin_pop();
            if (msg != NULL) {
                *record_list++ = (struct message_t *)msg;
                pop_cnt++;
                if (pop_cnt >= MAX_POP_MSG_COUNT)
                    break;
            }
            else {
                fail_cnt++;
            }
        }
    }
    else if (funcType == FUNC_RINGQUEUE_SPIN1_PUSH) {
        // 细粒度的改进型spin_mutex自旋锁
        while (true) {
            msg = (message_t *)queue->spin1_pop();
            if (msg != NULL) {
                *record_list++ = (struct message_t *)msg;
                pop_cnt++;
                if (pop_cnt >= MAX_POP_MSG_COUNT)
                    break;
            }
            else {
                fail_cnt++;
            }
        }
    }
    else if (funcType == FUNC_RINGQUEUE_SPIN2_PUSH) {
        // 细粒度的通用型spin_mutex自旋锁
        loop_cnt = 0;
        while (true) {
            msg = (message_t *)queue->spin2_pop();
            if (msg != NULL) {
                *record_list++ = (struct message_t *)msg;
                loop_cnt = 0;
                pop_cnt++;
                if (pop_cnt >= MAX_POP_MSG_COUNT)
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
                        }
                    }
                }
                else {
                    for (pause_cnt = 1; pause_cnt > 0; --pause_cnt) {
                        jimi_mm_pause();
                    }
                }
                loop_cnt++;
#elif 0
                jimi_wsleep(0);
#endif
            }
        }
    }
    else if (funcType == FUNC_RINGQUEUE_MUTEX_PUSH) {
        // 粗粒度的pthread_mutex_t锁(Windows上为临界区, Linux上为pthread_mutex_t)
        while (true) {
            msg = (message_t *)queue->mutex_pop();
            if (msg != NULL) {
                *record_list++ = (struct message_t *)msg;
                pop_cnt++;
                if (pop_cnt >= MAX_POP_MSG_COUNT)
                    break;
            }
            else {
                fail_cnt++;
            }
        }
    }
    else if (funcType == FUNC_DOUBAN_Q3H) {
        // 豆瓣上q3.h的原版文件
        while (true) {
            msg = (struct message_t *)pop(q);
            if (msg != NULL) {
                *record_list++ = (struct message_t *)msg;
                pop_cnt++;
                if (pop_cnt >= MAX_POP_MSG_COUNT)
                    break;
            }
            else {
                fail_cnt++;
            }
        }
    }
    else if (funcType == FUNC_RINGQUEUE_SPIN3_PUSH) {
        // 细粒度的通用型spin_mutex自旋锁
        loop_cnt = 0;
        while (true) {
            msg = (message_t *)queue->spin3_pop();
            if (msg != NULL) {
                *record_list++ = (struct message_t *)msg;
                loop_cnt = 0;
                pop_cnt++;
                if (pop_cnt >= MAX_POP_MSG_COUNT)
                    break;
            }
            else {
                fail_cnt++;
#if 0
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
#endif
            }
        }
    }
    else if (funcType == FUNC_RINGQUEUE_PUSH) {
        // 豆瓣上q3.h的lock-free改良型方案
        while (true) {
            msg = (message_t *)queue->pop();
            if (msg != NULL) {
                *record_list++ = (struct message_t *)msg;
                pop_cnt++;
                if (pop_cnt >= MAX_POP_MSG_COUNT)
                    break;
            }
            else {
                fail_cnt++;
            }
        }
    }
    else if (funcType == FUNC_RINGQUEUE_SPIN9_PUSH) {
        // 细粒度的仿制spin_mutex自旋锁(会死锁)
        while (true) {
            msg = (message_t *)queue->spin9_pop();
            if (msg != NULL) {
                *record_list++ = (struct message_t *)msg;
                pop_cnt++;
                if (pop_cnt >= MAX_POP_MSG_COUNT)
                    break;
            }
            else {
                fail_cnt++;
            }
        }
    }
    else if (funcType == FUNC_DISRUPTOR_RINGQUEUE) {
        // C++ 版 Disruptor 3.30
        loop_cnt = 0;
        spin_cnt = 1;

        ValueEvent_t _ValueEvent;
        valueEvent = &_ValueEvent;
        DisruptorRingQueue_t::PopThreadStackData stackData;
        DisruptorRingQueue_t::Sequence tailSequence;
        DisruptorRingQueue_t::Sequence *pTailSequence = disRingQueue->getGatingSequences(idx);
        if (pTailSequence == NULL)
            pTailSequence = &tailSequence;
        tailSequence.set(Sequence::INITIAL_CURSOR_VALUE);
        stackData.tailSequence = pTailSequence;
        stackData.nextSequence = stackData.tailSequence->get();
        stackData.cachedAvailableSequence = Sequence::INITIAL_CURSOR_VALUE;
        stackData.processedSequence = true;

        static const uint32_t DISRUPTOR_YIELD_THRESHOLD = 5;

        while (true) {
            if (disRingQueue->pop(*valueEvent, stackData) == 0) {
                *dis_record_list++ = *valueEvent;
                loop_cnt = 0;
                spin_cnt = 1;
                pop_cnt++;
                if (pop_cnt >= MAX_POP_MSG_COUNT)
                    break;

#if defined(DISPLAY_DEBUG_INFO) && (DISPLAY_DEBUG_INFO != 0)
                if ((pop_cnt & 0x03FF) == 0x03FF) {
                    printf("Pop():  Thread id = %2d, count = %8d, fail_cnt = %8d\n", idx, pop_cnt, fail_cnt);
                }
#endif
            }
            else {
                fail_cnt++;
#if 1
                if (loop_cnt >= DISRUPTOR_YIELD_THRESHOLD) {
                    yeild_cnt = loop_cnt - DISRUPTOR_YIELD_THRESHOLD;
                    if ((yeild_cnt & 63) == 63) {
                        jimi_wsleep(1);
                    }
                    else if ((yeild_cnt & 3) == 3) {
                        jimi_wsleep(0);
                    }
                    else {
                        if (!jimi_yield()) {
                            jimi_wsleep(0);
                        }
                    }
                }
                else {
                    for (pause_cnt = spin_cnt; pause_cnt > 0; --pause_cnt) {
                        jimi_mm_pause();
                    }
                    spin_cnt = spin_cnt + 1;
                }
                loop_cnt++;
#endif
            }
        }

        if (pTailSequence) {
            pTailSequence->setMaxValue();
        }
    }
    else if (funcType == FUNC_DISRUPTOR_RINGQUEUE_EX) {
        // C++ 版 Disruptor 3.30, 改进版
        loop_cnt = 0;
        spin_cnt = 1;

        ValueEvent_t _ValueEvent;
        valueEvent = &_ValueEvent;
        DisruptorRingQueueEx_t::PopThreadStackData stackData;
        DisruptorRingQueueEx_t::Sequence tailSequence;
        DisruptorRingQueueEx_t::Sequence *pTailSequence = disRingQueueEx->getGatingSequences(idx);
        if (pTailSequence == NULL)
            pTailSequence = &tailSequence;
        tailSequence.set(Sequence::INITIAL_CURSOR_VALUE);
        stackData.tailSequence = pTailSequence;
        stackData.nextSequence = stackData.tailSequence->get();
        stackData.cachedAvailableSequence = Sequence::INITIAL_CURSOR_VALUE;
        stackData.processedSequence = true;

        static const uint32_t DISRUPTOR_YIELD_THRESHOLD = 5;

        while (true) {
            if (disRingQueueEx->pop(*valueEvent, stackData) == 0) {
                *dis_record_list++ = *valueEvent;
                loop_cnt = 0;
                spin_cnt = 1;
                pop_cnt++;
                if (pop_cnt >= MAX_POP_MSG_COUNT)
                    break;

#if defined(DISPLAY_DEBUG_INFO) && (DISPLAY_DEBUG_INFO != 0)
                if ((pop_cnt & 0x03FF) == 0x03FF) {
                    printf("Pop():  Thread id = %2d, count = %8d, fail_cnt = %8d\n", idx, pop_cnt, fail_cnt);
                }
#endif
            }
            else {
                fail_cnt++;
#if 1
                if (loop_cnt >= DISRUPTOR_YIELD_THRESHOLD) {
                    yeild_cnt = loop_cnt - DISRUPTOR_YIELD_THRESHOLD;
                    if ((yeild_cnt & 63) == 63) {
                        jimi_wsleep(1);
                    }
                    else if ((yeild_cnt & 3) == 3) {
                        jimi_wsleep(0);
                    }
                    else {
                        if (!jimi_yield()) {
                            jimi_wsleep(0);
                        }
                    }
                }
                else {
                    for (pause_cnt = spin_cnt; pause_cnt > 0; --pause_cnt) {
                        jimi_mm_pause();
                    }
                    spin_cnt = spin_cnt + 1;
                }
                loop_cnt++;
#endif
            }
        }

        if (pTailSequence) {
            pTailSequence->setMaxValue();
        }
    }
    else {
        // TODO: pop() - Unknown test function type
    }

#else  /* !TEST_FUNC_TYPE */

#if defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN2_PUSH \
    || TEST_FUNC_TYPE == FUNC_DISRUPTOR_RINGQUEUE || TEST_FUNC_TYPE == FUNC_DISRUPTOR_RINGQUEUE_EX)
    loop_cnt = 0;
    spin_cnt = 1;
#endif

#if defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_DISRUPTOR_RINGQUEUE)
    ValueEvent_t _ValueEvent;
    valueEvent = &_ValueEvent;
    DisruptorRingQueue_t::PopThreadStackData stackData;
    DisruptorRingQueue_t::Sequence tailSequence;
    DisruptorRingQueue_t::Sequence *pTailSequence = disRingQueue->getGatingSequences(idx);
    if (pTailSequence == NULL)
        pTailSequence = &tailSequence;
    tailSequence.set(Sequence::INITIAL_CURSOR_VALUE);
    stackData.tailSequence = pTailSequence;
    stackData.nextSequence = stackData.tailSequence->get();
    stackData.cachedAvailableSequence = Sequence::INITIAL_CURSOR_VALUE;
    stackData.processedSequence = true;

    static const uint32_t DISRUPTOR_YIELD_THRESHOLD = 5;

    while (true) {
        if (disRingQueue->pop(*valueEvent, stackData) == 0) {
            *dis_record_list++ = *valueEvent;
            loop_cnt = 0;
            spin_cnt = 1;
            pop_cnt++;
            if (pop_cnt >= MAX_POP_MSG_COUNT)
                break;

#if defined(DISPLAY_DEBUG_INFO) && (DISPLAY_DEBUG_INFO != 0)
            if ((pop_cnt & 0x03FF) == 0x03FF) {
                printf("Pop():  Thread id = %2d, pop_cnt = %8d, fail_cnt = %8d\n", idx, pop_cnt, fail_cnt);
            }
#endif
        }
        else {
            fail_cnt++;
  #if 1
            if (loop_cnt >= DISRUPTOR_YIELD_THRESHOLD) {
                yeild_cnt = loop_cnt - DISRUPTOR_YIELD_THRESHOLD;
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
                for (pause_cnt = spin_cnt; pause_cnt > 0; --pause_cnt) {
                    jimi_mm_pause();
                }
                spin_cnt = spin_cnt + 1;
            }
            loop_cnt++;
  #endif
        }
    }

    if (pTailSequence) {
        //pTailSequence->set(INT64_MAX);
        pTailSequence->setMaxValue();
    }
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_DISRUPTOR_RINGQUEUE_EX)
    ValueEvent_t _ValueEvent;
    valueEvent = &_ValueEvent;
    DisruptorRingQueueEx_t::PopThreadStackData stackData;
    DisruptorRingQueueEx_t::Sequence tailSequence;
    DisruptorRingQueueEx_t::Sequence *pTailSequence = disRingQueueEx->getGatingSequences(idx);
    if (pTailSequence == NULL)
        pTailSequence = &tailSequence;
    tailSequence.set(Sequence::INITIAL_CURSOR_VALUE);
    stackData.tailSequence = pTailSequence;
    stackData.nextSequence = stackData.tailSequence->get();
    stackData.cachedAvailableSequence = Sequence::INITIAL_CURSOR_VALUE;
    stackData.processedSequence = true;

    static const uint32_t DISRUPTOR_YIELD_THRESHOLD = 5;

    while (true) {
        if (disRingQueueEx->pop(*valueEvent, stackData) == 0) {
            *dis_record_list++ = *valueEvent;
            loop_cnt = 0;
            spin_cnt = 1;
            pop_cnt++;
            if (pop_cnt >= MAX_POP_MSG_COUNT)
                break;

#if defined(DISPLAY_DEBUG_INFO) && (DISPLAY_DEBUG_INFO != 0)
            if ((pop_cnt & 0x03FF) == 0x03FF) {
                printf("Pop():  Thread id = %2d, pop_cnt = %8d, fail_cnt = %8d\n", idx, pop_cnt, fail_cnt);
            }
#endif
        }
        else {
            fail_cnt++;
  #if 1
            if (loop_cnt >= DISRUPTOR_YIELD_THRESHOLD) {
                yeild_cnt = loop_cnt - DISRUPTOR_YIELD_THRESHOLD;
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
                for (pause_cnt = spin_cnt; pause_cnt > 0; --pause_cnt) {
                    jimi_mm_pause();
                }
                spin_cnt = spin_cnt + 1;
            }
            loop_cnt++;
  #endif
        }
    }

    if (pTailSequence) {
        //pTailSequence->set(INT64_MAX);
        pTailSequence->setMaxValue();
    }
#else
    while (true || !quit) {
#if defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN_PUSH)
        msg = (message_t *)queue->spin_pop();
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN1_PUSH)
        msg = (message_t *)queue->spin1_pop();
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN2_PUSH)
        msg = (message_t *)queue->spin2_pop();
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_MUTEX_PUSH)
        msg = (message_t *)queue->mutex_pop();
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_DOUBAN_Q3H)
        msg = (message_t *)pop(q);
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN3_PUSH)
        msg = (message_t *)queue->spin3_pop();
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN9_PUSH)
        msg = (message_t *)queue->spin9_pop();
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_PUSH)
        msg = (message_t *)queue->pop();
#else
        msg = NULL;
#endif
        if (msg != NULL) {
            *record_list++ = (struct message_t *)msg;
#if defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN2_PUSH \
            || TEST_FUNC_TYPE == FUNC_DISRUPTOR_RINGQUEUE || TEST_FUNC_TYPE == FUNC_DISRUPTOR_RINGQUEUE_EX)
            loop_cnt = 0;
#endif
            pop_cnt++;
            if (pop_cnt >= MAX_POP_MSG_COUNT)
                break;
        }
        else {
            fail_cnt++;
#if defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN2_PUSH \
            || TEST_FUNC_TYPE == FUNC_DISRUPTOR_RINGQUEUE || TEST_FUNC_TYPE == FUNC_DISRUPTOR_RINGQUEUE_EX)
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
                    }
                }
            }
            else {
                for (pause_cnt = 1; pause_cnt > 0; --pause_cnt) {
                    jimi_mm_pause();
                }
            }
            loop_cnt++;
  #endif
#endif  /* TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN2_PUSH */
        }
    }
#endif  /* TEST_FUNC_TYPE == FUNC_DISRUPTOR_RINGQUEUE */

#endif  /* TEST_FUNC_TYPE */

    //pop_cycles += read_rdtsc() - start;
    jimi_fetch_and_add64(&pop_cycles, read_rdtsc() - start);
    //pop_total += pop_cnt;
    jimi_fetch_and_add32(&pop_total, pop_cnt);
    //pop_fail_total += fail_cnt;
    jimi_fetch_and_add32(&pop_fail_total, fail_cnt);

    //printf("Pop()  Thread %d has exited: message pop_cnt = %d\n", idx, pop_cnt);

#if (defined(USE_TIME_PERIOD) && (USE_TIME_PERIOD != 0)) && (0) \
    && (defined(_WIN32) || defined(__MINGW32__) || defined(__CYGWIN__))
    if (time_result == TIMERR_NOERROR) {
        time_result = timeEndPeriod(tc.wPeriodMin);
    }
#endif

    return NULL;
}

static void *
PTW32_API
single_push_task(void * arg)
{
    thread_arg_t *thread_arg;
    SingleRingQueue_t *queue;
    ValueEvent_t *valueEvent = NULL;
    uint64_t start;
    int i, idx, funcType;

    int32_t pause_cnt;
    uint32_t loop_cnt, yeild_cnt, spin_cnt = 1;
    uint32_t fail_cnt;

    idx = 0;
    funcType = 0;
    queue = NULL;

    thread_arg = (thread_arg_t *)arg;
    if (thread_arg) {
        idx      = thread_arg->idx;
        funcType = thread_arg->funcType;
        queue    = (SingleRingQueue_t *)thread_arg->queue;
        if (queue == NULL)
            return NULL;
    }

    if (thread_arg)
        free(thread_arg);

    fail_cnt = 0;
    valueEvent = (ValueEvent_t *)&msgs[0];
    start = read_rdtsc();

    static const uint32_t YIELD_THRESHOLD = 4;

    for (i = 0; i < MAX_MSG_COUNT; ++i) {
        loop_cnt = 0;
        spin_cnt = 1;
        while (queue->push(*valueEvent) == -1) {
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
                    }
                }
            }
            else {
                for (pause_cnt = spin_cnt; pause_cnt > 0; --pause_cnt) {
                    jimi_mm_pause();
                }
                spin_cnt = spin_cnt + 1;
            }
            loop_cnt++;
#endif
            fail_cnt++;
        };
        valueEvent++;
    }

    //push_cycles += read_rdtsc() - start;
    jimi_fetch_and_add64(&push_cycles, read_rdtsc() - start);
    //push_total += MAX_MSG_COUNT;
    jimi_fetch_and_add32(&push_total, MAX_MSG_COUNT);
    //push_fail_total += fail_cnt;
    jimi_fetch_and_add32(&push_fail_total, fail_cnt);

    return NULL;
}

static void *
PTW32_API
single_pop_task(void * arg)
{
    thread_arg_t *thread_arg;
    SingleRingQueue_t *queue;
    ValueEvent_t *pValueEvent = NULL;
    ValueEvent_t *record_list;
    uint64_t start;
    int idx, funcType;

    int32_t pause_cnt;
    uint32_t loop_cnt, yeild_cnt, spin_cnt = 1;
    uint32_t fail_cnt;
    uint64_t pop_cnt;

    idx = 0;
    funcType = 0;
    queue = NULL;

    thread_arg = (thread_arg_t *)arg;
    if (thread_arg) {
        idx      = thread_arg->idx;
        funcType = thread_arg->funcType;
        queue    = (SingleRingQueue_t *)thread_arg->queue;
        if (queue == NULL)
            return NULL;
    }

    if (thread_arg)
        free(thread_arg);

    pop_cnt = 0;
    fail_cnt = 0;
    record_list = &dis_popevent_list[idx][0];
    start = read_rdtsc();

    loop_cnt = 0;
    spin_cnt = 1;

    static const uint32_t YIELD_THRESHOLD = 4;

    ValueEvent_t valueEvent;
    while (true) {
        if (queue->pop(valueEvent) == 0) {
            *record_list++ = valueEvent;
            loop_cnt = 0;
            spin_cnt = 1;
            pop_cnt++;
            if (pop_cnt >= MAX_MSG_COUNT)
                break;

#if defined(DISPLAY_DEBUG_INFO) && (DISPLAY_DEBUG_INFO != 0)
            if ((pop_cnt & 0x03FF) == 0x03FF) {
                printf("Pop():  Thread id = %2d, pop_cnt = %8d, fail_cnt = %8d\n", idx, pop_cnt, fail_cnt);
            }
#endif
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
                    }
                }
            }
            else {
                for (pause_cnt = spin_cnt; pause_cnt > 0; --pause_cnt) {
                    jimi_mm_pause();
                }
                spin_cnt = spin_cnt + 1;
            }
            loop_cnt++;
#endif
        }
    }

    //pop_cycles += read_rdtsc() - start;
    jimi_fetch_and_add64(&pop_cycles, read_rdtsc() - start);
    //pop_total += pop_cnt;
    jimi_fetch_and_add32(&pop_total, pop_cnt);
    //pop_fail_total += fail_cnt;
    jimi_fetch_and_add32(&pop_fail_total, fail_cnt);

    //printf("Pop()  Thread %d has exited: message pop_cnt = %d\n", idx, pop_cnt);

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
    core_id = id % get_num_of_processors();
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
push_task(void * arg)
{
    struct thread_arg_t *thread_arg;
    struct queue *q;
    struct message_t *msg;
    uint64_t start;
    int i, idx;
    unsigned int fail_cnt = 0;

    idx = 0;
    q = NULL;

    thread_arg = (thread_arg_t *)arg;
    if (thread_arg) {
        idx = thread_arg->idx;
        q   = (struct queue *)thread_arg->queue;
    }

    if (q == NULL)
        return NULL;

    if (thread_arg)
        free(thread_arg);

    msg = (struct message_t *)&msgs[idx * MAX_PUSH_MSG_COUNT];
    start = read_rdtsc();

    for (i = 0; i < MAX_PUSH_MSG_COUNT; ++i) {
        while (push(q, (void *)msg) == -1) {
            fail_cnt++;
        };
        msg++;
    }

    //push_cycles += read_rdtsc() - start;
    jimi_fetch_and_add64(&push_cycles, read_rdtsc() - start);
    //push_total += MAX_PUSH_MSG_COUNT;
    jimi_fetch_and_add32(&push_total, MAX_PUSH_MSG_COUNT);
    //push_fail_total += fail_cnt;
    jimi_fetch_and_add32(&push_fail_total, fail_cnt);
    if (push_total == MAX_MSG_CNT)
        quit = 1;

    return NULL;
}

static void *
PTW32_API
pop_task(void * arg)
{
    struct thread_arg_t *thread_arg;
    struct queue *q;
    struct message_t *msg;
    struct message_t **record_list;
    uint64_t start;
    int idx;
    unsigned int pop_cnt, fail_cnt;

    idx = 0;
    q = NULL;

    thread_arg = (thread_arg_t *)arg;
    if (thread_arg) {
        idx = thread_arg->idx;
        q   = (struct queue *)thread_arg->queue;
    }

    if (q == NULL)
        return NULL;

    if (thread_arg)
        free(thread_arg);

    pop_cnt = 0;
    fail_cnt = 0;
    record_list = &popmsg_list[idx][0];
    //record_list = &popmsg_list[idx * MAX_POP_MSG_COUNT];
    start = read_rdtsc();

    while (true || !quit) {
        msg = (struct message_t *)pop(q);
        if (msg != NULL) {
            *record_list++ = (struct message_t *)msg;
            pop_cnt++;
            if (pop_cnt >= MAX_POP_MSG_COUNT)
                break;
        }
        else {
            fail_cnt++;
        }
    }

    //pop_cycles += read_rdtsc() - start;
    jimi_fetch_and_add64(&pop_cycles, read_rdtsc() - start);
    //pop_total += pop_cnt;
    jimi_fetch_and_add32(&pop_total, pop_cnt);
    //pop_fail_total += fail_cnt;
    jimi_fetch_and_add32(&pop_fail_total, fail_cnt);

    return NULL;
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
    core_id = id % get_num_of_processors();
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

void pop_list_reset(void)
{
    unsigned int i, j;
    //struct msg_t **cur_popmsg_list = popmsg_list;
    for (i = 0; i < POP_CNT; ++i) {
        for (j = 0; j < MAX_POP_MSG_COUNT; ++j) {
            popmsg_list[i][j] = NULL;
            //*cur_popmsg_list++ = NULL;
        }
    }

    ValueEvent_t event(0ULL);
    for (i = 0; i < POP_CNT; ++i) {
        for (j = 0; j < MAX_POP_MSG_COUNT; ++j) {
            dis_popevent_list[i][j] = event;
            //*cur_popmsg_list++ = NULL;
        }
    }
}

int pop_list_verify(void)
{
    int i, j;
    uint32_t index;
    uint32_t *verify_list;
    struct message_t *msg;
    int empty, overlay, correct, errors, times;

    verify_list = (uint32_t *)calloc(MAX_MSG_CNT, sizeof(uint32_t));
    if (verify_list == NULL)
        return -1;

    //printf("popmsg_list = %016p\n", popmsg_list);

    for (i = 0; i < POP_CNT; ++i) {
        for (j = 0; j < MAX_POP_MSG_COUNT; ++j) {
            //msg = popmsg_list[i * MAX_POP_MSG_COUNT + j];
            msg = popmsg_list[i][j];
            if (msg != NULL) {
                index = (uint32_t)(msg->dummy - 1);
                if (index < MAX_MSG_CNT)
                    verify_list[index] = verify_list[index] + 1;
            }
        }
    }

    empty = 0;
    overlay = 0;
    correct = 0;
    errors = 0;
    for (i = 0; i < MAX_MSG_CNT; ++i) {
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

int disruptor_pop_list_verify(void)
{
    int i, j;
    uint32_t index;
    uint32_t *verify_list;
    ValueEvent_t event;
    int empty, overlay, correct, errors, times;

    verify_list = (uint32_t *)calloc(MAX_MSG_CNT, sizeof(uint32_t));
    if (verify_list == NULL)
        return -1;

    //printf("dis_popevent_list = %016p\n", dis_popevent_list);

    for (i = 0; i < POP_CNT; ++i) {
        for (j = 0; j < MAX_POP_MSG_COUNT; ++j) {
            //event = dis_popevent_list[i * MAX_POP_MSG_COUNT + j];
            event = dis_popevent_list[i][j];
            index = (uint32_t)(event.getValue() - 1);
            if (index < MAX_MSG_CNT)
                verify_list[index] = verify_list[index] + 1;
        }
    }

    empty = 0;
    overlay = 0;
    correct = 0;
    errors = 0;
    for (i = 0; i < MAX_MSG_CNT; ++i) {
        times = verify_list[i];
        if (times == 0) {
#if 0
            if (errors == 0)
                printf("Empty Errors:\n");
            printf("verify_list[%8d] = %d\n", i, times);
#endif
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

    if (errors > 0 || empty > 0)
        printf("\n");
    //printf("pop-list verify result:\n\n");
    printf("empty = %d, overlay = %d, correct = %d, totals = %d.\n\n",
           empty, overlay, correct, empty + overlay + correct);

    if (verify_list)
        free(verify_list);

    //jimi_console_readkeyln(false, true, false);

    return correct;
}

void RingQueue_Test(int funcType, bool bContinue = true)
{
    struct queue *q;
    RingQueue_t ringQueue(true, true);
    DisruptorRingQueue_t disRingQueue;
    DisruptorRingQueueEx_t disRingQueueEx;
    
    pthread_t kids[POP_CNT + PUSH_CNT] = { 0 };
    thread_arg_t *thread_arg;
    jmc_timestamp_t startTime, stopTime;
    jmc_timefloat_t elapsedTime = 0.0;
    int i;

#if defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE != 0)
    if (funcType != TEST_FUNC_TYPE) {
        return;
    }
#endif

    q = qinit();

    printf("---------------------------------------------------------------\n");

    if (funcType == FUNC_RINGQUEUE_SPIN_PUSH) {
        // 细粒度的标准spin_mutex自旋锁
        printf("RingQueue.spin_push() test: (FuncId = %d)\n", funcType);
    }
    else if (funcType == FUNC_RINGQUEUE_SPIN1_PUSH) {
        // 细粒度的改进型spin_mutex自旋锁
        printf("RingQueue.spin1_push() test: (FuncId = %d)\n", funcType);
    }
    else if (funcType == FUNC_RINGQUEUE_SPIN2_PUSH) {
        // 细粒度的通用型spin_mutex自旋锁
        printf("RingQueue.spin2_push() test: (FuncId = %d)\n", funcType);
    }
    else if (funcType == FUNC_RINGQUEUE_MUTEX_PUSH) {
        // 粗粒度的pthread_mutex_t锁(Windows上为临界区, Linux上为pthread_mutex_t)
        printf("RingQueue.mutex_push() test: (FuncId = %d)\n", funcType);
    }
    else if (funcType == FUNC_DOUBAN_Q3H) {
        // 豆瓣上q3.h的原版文件
        printf("DouBan's q3.h test: (FuncId = %d)\n", funcType);
    }
    else if (funcType == FUNC_RINGQUEUE_PUSH) {
        // 豆瓣上q3.h的lock-free改良型方案
        printf("RingQueue.push() test (modified base on q3.h): (FuncId = %d)\n", funcType);
    }
    else if (funcType == FUNC_RINGQUEUE_SPIN3_PUSH) {
        // 细粒度的通用型spin_mutex自旋锁
        printf("RingQueue.spin3_push() test: (FuncId = %d)\n", funcType);
    }
    else if (funcType == FUNC_RINGQUEUE_SPIN9_PUSH) {
        // 细粒度的仿制spin_mutex自旋锁(会死锁)
        printf("RingQueue.spin9_push() test (maybe deadlock): (FuncId = %d)\n", funcType);
    }
    else if (funcType == FUNC_DISRUPTOR_RINGQUEUE) {
        // disruptor 3.3 (C++版)
        printf("DisruptorRingQueue test: (FuncId = %d)\n", funcType);
    }
    else if (funcType == FUNC_DISRUPTOR_RINGQUEUE_EX) {
        // disruptor 3.3 (C++版) 改进版
        printf("DisruptorRingQueueEx test: (FuncId = %d)\n", funcType);
    }
    else {
        printf("a unknown test function: (FuncId = %d)\n", funcType);
    }

#if 0
    //printf("\n");
#if defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN_PUSH)
    printf("RingQueue.spin_push() test: (FuncId = %d)\n", funcType);
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN1_PUSH)
    printf("RingQueue.spin1_push() test: (FuncId = %d)\n", funcType);
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN2_PUSH)
    printf("RingQueue.spin2_push() test: (FuncId = %d)\n", funcType);
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_MUTEX_PUSH)
    printf("RingQueue.mutex_push() test: (FuncId = %d)\n", funcType);
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_DOUBAN_Q3H)
    printf("DouBan's q3.h test: (FuncId = %d)\n", funcType);
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN3_PUSH)
    printf("RingQueue.spin3_push() test: (FuncId = %d)\n", funcType);
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_SPIN9_PUSH)
    printf("RingQueue.spin9_push() test (maybe deadlock): (FuncId = %d)\n", funcType);
#elif defined(TEST_FUNC_TYPE) && (TEST_FUNC_TYPE == FUNC_RINGQUEUE_PUSH)
    printf("RingQueue.push() test (modified base on q3.h): (FuncId = %d)\n", funcType);
#else
    printf("a unknown test function: (%d)\n", funcType);
#endif
#endif

    printf("---------------------------------------------------------------\n");

    init_globals();
    setaffinity(0);

    test_msg_reset();
    pop_list_reset();

    disRingQueue.start();

    startTime = jmc_get_timestamp();

    for (i = 0; i < PUSH_CNT; ++i) {
        thread_arg = (thread_arg_t *)malloc(sizeof(struct thread_arg_t));
        thread_arg->idx = i;
        thread_arg->funcType = funcType;
        if (funcType == FUNC_DOUBAN_Q3H)
            thread_arg->queue = (void *)q;
        else if (funcType == FUNC_DISRUPTOR_RINGQUEUE)
            thread_arg->queue = (void *)&disRingQueue;
        else if (funcType == FUNC_DISRUPTOR_RINGQUEUE_EX)
            thread_arg->queue = (void *)&disRingQueueEx;
        else
            thread_arg->queue = (void *)&ringQueue;
        RingQueue_start_thread(i, RingQueue_push_task, (void *)thread_arg, &kids[i]);
    }
    for (i = 0; i < POP_CNT; ++i) {
        thread_arg = (thread_arg_t *)malloc(sizeof(struct thread_arg_t));
        thread_arg->idx = i;
        thread_arg->funcType = funcType;
        if (funcType == FUNC_DOUBAN_Q3H)
            thread_arg->queue = (void *)q;
        else if (funcType == FUNC_DISRUPTOR_RINGQUEUE)
            thread_arg->queue = (void *)&disRingQueue;
        else if (funcType == FUNC_DISRUPTOR_RINGQUEUE_EX)
            thread_arg->queue = (void *)&disRingQueueEx;
        else
            thread_arg->queue = (void *)&ringQueue;
        RingQueue_start_thread(i + PUSH_CNT, RingQueue_pop_task, (void *)thread_arg,
                               &kids[i + PUSH_CNT]);
    }
    for (i = 0; i < PUSH_CNT; ++i)
        pthread_join(kids[i], NULL);

    disRingQueue.shutdown();

    for (; i < (PUSH_CNT + POP_CNT); ++i)
        pthread_join(kids[i], NULL);

    stopTime = jmc_get_timestamp();
    elapsedTime += jmc_get_interval_millisecf(stopTime - startTime);

#if defined(DISPLAY_EXTRA_RESULT) && (DISPLAY_EXTRA_RESULT != 0)
  #if defined(__clang__) || defined(__CLANG__) || defined(__APPLE__) || defined(__FreeBSD__)
    printf("\n");
    printf("push total: %u + %u\n", MAX_MSG_CNT, push_fail_total);
    printf("push cycles/msg: %u\n", (uint32_t)(push_cycles / MAX_MSG_CNT));
    printf("pop  total: %u + %u\n", pop_total, pop_fail_total);
    if (pop_total == 0)
        printf("pop  cycles/msg: %u\n", 0UL);
    else
        printf("pop  cycles/msg: %u\n", (uint32_t)(pop_cycles / pop_total));
  #else
    printf("\n");
    printf("push total: %u + %u\n", MAX_MSG_CNT, push_fail_total);
    printf("push cycles/msg: %"PRIuFAST64"\n", push_cycles / MAX_MSG_CNT);
    printf("pop  total: %u + %u\n", pop_total, pop_fail_total);
    if (pop_total == 0)
        printf("pop  cycles/msg: %"PRIuFAST64"\n", 0ULL);
    else
        printf("pop  cycles/msg: %"PRIuFAST64"\n", pop_cycles / pop_total);
  #endif
#endif  /* DISPLAY_EXTRA_RESULT */

    //printf("---------------------------------------------------------------\n");

    printf("\n");
    printf("time elapsed: %9.3f ms, ", elapsedTime);
    if (elapsedTime != 0.0)
        printf("throughput: %u ops/sec\n\n", (uint32_t)((MAX_MSG_CNT * 1000.0) / elapsedTime));
    else
        printf("throughput: %u ops/sec\n\n", 0U);

    //jimi_console_readkeyln(false, true, false);

    if (funcType == FUNC_DISRUPTOR_RINGQUEUE || funcType == FUNC_DISRUPTOR_RINGQUEUE_EX)
        disruptor_pop_list_verify();
    else
        pop_list_verify();

    //printf("---------------------------------------------------------------\n\n");

#if 0
    for (j = 0; j < POP_CNT; ++j) {
        for (i = 0; i <= 256; ++i) {
            printf("pop_list[%2d, %3d] = ptr: 0x%08p, %02"PRIuFAST64" : %"PRIuFAST64"\n", j, i,
                   (struct msg_t *)(popmsg_list[j][i]),
                   popmsg_list[j][i]->dummy / (MAX_PUSH_MSG_COUNT),
                   popmsg_list[j][i]->dummy % (MAX_PUSH_MSG_COUNT));
        }
        printf("\n");
        if (j < (POP_CNT - 1)) {
            jimi_console_readkeyln(false, true, false);
        }
    }
    printf("\n");
#endif

    qfree(q);

    // if do not need "press any key to continue..." prompt, exit to function directly.
    if (!bContinue) {
        printf("---------------------------------------------------------------\n\n");

#if !defined(USE_DOUBAN_QUEUE) || (USE_DOUBAN_QUEUE == 0)
        jimi_console_readkeyln(false, true, false);
#else
        jimi_console_readkeyln(false, true, false);
#endif
    }
}

void SingleProducerSingleConsumer_Test(bool bContinue = true)
{
    SingleRingQueue_t srq;

    static const int kPushCnt = 1;
    static const int kPopCnt  = 1;
    pthread_t kids[kPushCnt + kPopCnt] = { 0 };
    thread_arg_t *thread_arg;
    jmc_timestamp_t startTime, stopTime;
    jmc_timefloat_t elapsedTime = 0.0;
    int i;

    printf("---------------------------------------------------------------\n");
    printf("(One Producer + One Consumer) SingleRingQueue test:\n");
    printf("---------------------------------------------------------------\n");

    init_globals();
    setaffinity(0);

    test_msg_reset();
    pop_list_reset();

    startTime = jmc_get_timestamp();

    for (i = 0; i < kPushCnt; ++i) {
        thread_arg = (thread_arg_t *)malloc(sizeof(struct thread_arg_t));
        thread_arg->idx = i;
        thread_arg->funcType = FUNC_DOUBAN_Q3H;
        thread_arg->queue = (void *)&srq;
        RingQueue_start_thread(i, single_push_task, (void *)thread_arg, &kids[i]);
    }
    for (i = 0; i < kPopCnt; ++i) {
        thread_arg = (thread_arg_t *)malloc(sizeof(struct thread_arg_t));
        thread_arg->idx = i;
        thread_arg->funcType = FUNC_DOUBAN_Q3H;
        thread_arg->queue = (void *)&srq;
        RingQueue_start_thread(i + kPushCnt, single_pop_task, (void *)thread_arg, &kids[i + kPushCnt]);
    }
    for (i = 0; i < (kPushCnt + kPopCnt); ++i)
        pthread_join(kids[i], NULL);

    stopTime = jmc_get_timestamp();
    elapsedTime += jmc_get_interval_millisecf(stopTime - startTime);

#if defined(DISPLAY_EXTRA_RESULT) && (DISPLAY_EXTRA_RESULT != 0)
  #if defined(__clang__) || defined(__CLANG__) || defined(__APPLE__) || defined(__FreeBSD__)
    printf("\n");
    printf("push total: %u + %u\n", MAX_MSG_CNT, push_fail_total);
    printf("push cycles/msg: %u\n", (uint32_t)(push_cycles / MAX_MSG_CNT));
    printf("pop  total: %u + %u\n", pop_total, pop_fail_total);
    if (pop_total == 0)
        printf("pop  cycles/msg: %u\n", 0UL);
    else
        printf("pop  cycles/msg: %u\n", (uint32_t)(pop_cycles / pop_total));
  #else
    printf("\n");
    printf("push total: %u + %u\n", MAX_MSG_CNT, push_fail_total);
    printf("push cycles/msg: %"PRIuFAST64"\n", push_cycles / MAX_MSG_CNT);
    printf("pop  total: %u + %u\n", pop_total, pop_fail_total);
    if (pop_total == 0)
        printf("pop  cycles/msg: %"PRIuFAST64"\n", 0ULL);
    else
        printf("pop  cycles/msg: %"PRIuFAST64"\n", pop_cycles / pop_total);
  #endif
#endif  /* DISPLAY_EXTRA_RESULT */

    printf("\n");
    printf("time elapsed: %9.3f ms, ", elapsedTime);
    if (elapsedTime != 0.0)
        printf("throughput: %u ops/sec\n", (uint32_t)((MAX_MSG_CNT * 1000.0) / elapsedTime));
    else
        printf("throughput: %u ops/sec\n", 0U);
    printf("\n");

    disruptor_pop_list_verify();

    // if do not need "press any key to continue..." prompt, exit to function directly.
    if (!bContinue) {
        printf("---------------------------------------------------------------\n\n");

#if !defined(USE_DOUBAN_QUEUE) || (USE_DOUBAN_QUEUE == 0)
        jimi_console_readkeyln(false, true, false);
#else
        jimi_console_readkeyln(false, true, false);
#endif
    }
}

void SerialRingQueue_Test()
{
    SerialRingQueue<ValueEvent_t, QSIZE>  srq;
    ValueEvent_t *pushEvent, popEvent;
    ValueEvent_t *popEventList;
    jmc_timestamp_t startTime, stopTime;
    jmc_timefloat_t elapsedTime = 0.0;

    int i;
    int remain, step;
    int push_cnt = 0, pop_cnt = 0;
    const static int max_step = JIMI_MIN(16, QSIZE);

    printf("---------------------------------------------------------------\n");
    printf("Single Thread SerialRingQueue test: (Step = %d)\n", max_step);
    printf("---------------------------------------------------------------\n");

    init_globals();
    setaffinity(0);

    test_msg_reset();
    pop_list_reset();

    startTime = jmc_get_timestamp();

    pushEvent = (ValueEvent_t *)&msgs[0];
    popEventList = &dis_popevent_list[0][0];

    remain = MAX_MSG_CNT;
    while (remain > 0) {
        step = JIMI_MIN(max_step, remain);
        for (i = 0; i < step; ++i) {
            if (srq.push(*pushEvent) != -1) {
                pushEvent++;
                push_cnt++;
            }
        }
        for (i = 0; i < step; ++i) {
            if (srq.pop(popEvent) != -1) {
                *popEventList = popEvent;
                popEventList++;
                pop_cnt++;
            }
        }
        remain -= step;
    }

    stopTime = jmc_get_timestamp();
    elapsedTime += jmc_get_interval_millisecf(stopTime - startTime);

    printf("\n");
    printf("time elapsed: %9.3f ms, ", elapsedTime);
    if (elapsedTime != 0.0)
        printf("throughput: %u ops/sec\n", (uint32_t)((MAX_MSG_CNT * 1000.0) / elapsedTime));
    else
        printf("throughput: %u ops/sec\n", 0U);
    printf("\n");

    disruptor_pop_list_verify();
}

void q3_test(void)
{
    struct queue *q;
    pthread_t kids[POP_CNT + PUSH_CNT] = { 0 };
    thread_arg_t *thread_arg;
    jmc_timestamp_t startTime, stopTime;
    jmc_timefloat_t elapsedTime = 0.0;
    int i;

    q = qinit();

    printf("---------------------------------------------------------------\n");
    printf("DouBan's q3.h test:\n");
    printf("---------------------------------------------------------------\n");

    init_globals();
    setaffinity(0);

    test_msg_reset();
    pop_list_reset();

    startTime = jmc_get_timestamp();

    for (i = 0; i < PUSH_CNT; ++i) {
        thread_arg = (thread_arg_t *)malloc(sizeof(struct thread_arg_t));
        thread_arg->idx = i;
        thread_arg->funcType = FUNC_DOUBAN_Q3H;
        thread_arg->queue = (void *)q;
        start_thread(i, push_task, (void *)thread_arg, &kids[i]);
    }
    for (i = 0; i < POP_CNT; ++i) {
        thread_arg = (thread_arg_t *)malloc(sizeof(struct thread_arg_t));
        thread_arg->idx = i;
        thread_arg->funcType = FUNC_DOUBAN_Q3H;
        thread_arg->queue = (void *)q;
        start_thread(i + PUSH_CNT, pop_task, (void *)thread_arg, &kids[i + PUSH_CNT]);
    }
    for (i = 0; i < POP_CNT + PUSH_CNT; ++i)
        pthread_join(kids[i], NULL);

    stopTime = jmc_get_timestamp();
    elapsedTime += jmc_get_interval_millisecf(stopTime - startTime);

#if defined(DISPLAY_EXTRA_RESULT) && (DISPLAY_EXTRA_RESULT != 0)
  #if defined(__clang__) || defined(__CLANG__) || defined(__APPLE__) || defined(__FreeBSD__)
    printf("\n");
    printf("push total: %u + %u\n", MAX_MSG_CNT, push_total);
    printf("push cycles/msg: %u\n", (uint32_t)(push_cycles / MAX_MSG_CNT));
    printf("pop  total: %u + %u\n", pop_total, pop_fail_total);
    if (pop_total == 0)
        printf("pop  cycles/msg: %u\n", 0UL);
    else
        printf("pop  cycles/msg: %u\n", (uint32_t)(pop_cycles / pop_total));
  #else
    printf("\n");
    printf("push total: %u + %u\n", MAX_MSG_CNT, push_total);
    printf("push cycles/msg: %"PRIuFAST64"\n", push_cycles / MAX_MSG_CNT);
    printf("pop  total: %u + %u\n", pop_total, pop_fail_total);
    if (pop_total == 0)
        printf("pop  cycles/msg: %"PRIuFAST64"\n", 0ULL);
    else
        printf("pop  cycles/msg: %"PRIuFAST64"\n", pop_cycles / pop_total);
  #endif
#endif  /* DISPLAY_EXTRA_RESULT */

    printf("\n");
    printf("time elapsed: %9.3f ms, ", elapsedTime);
    if (elapsedTime != 0.0)
        printf("throughput: %u ops/sec\n\n", (uint32_t)((MAX_MSG_CNT * 1000.0) / elapsedTime));
    else
        printf("throughput: %u ops/sec\n\n", 0U);

    pop_list_verify();

    qfree(q);

    printf("---------------------------------------------------------------\n");

    jimi_console_readkeyln(false, true, false);
}

void
RingQueue_UnitTest(void)
{
    RingQueue_t ringQueue(true, true);
    message_t queue_msg = { 123ULL };

    init_globals();

    printf("---------------------------------------------------------------\n");
    printf("RingQueue2() test begin...\n\n");

    printf("ringQueue.capacity() = %u\n", ringQueue.capacity());
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

    msgs = (struct message_t *)calloc(MAX_MSG_CNT, sizeof(struct message_t));
    if (msgs != NULL) {
        for (i = 0; i < MAX_MSG_CNT; ++i)
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
        for (i = 0; i < MAX_MSG_CNT; ++i)
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

    popmsg_list = (struct msg_t **)calloc(MAX_MSG_CNT, sizeof(struct msg_t *));
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
display_test_info(int time_noerror)
{
#if (defined(USE_TIME_PERIOD) && (USE_TIME_PERIOD != 0)) \
    && (defined(_WIN32) || defined(__MINGW32__) || defined(__CYGWIN__))
    TIMECAPS tc;
    MMRESULT mm_result;
    time_result = TIMERR_NOCANDO;
    tc.wPeriodMin = 0;
    tc.wPeriodMax = 0;
    // See: http://msdn.microsoft.com/zh-cn/dd757627%28v=vs.85%29
    mm_result = timeGetDevCaps(&tc, sizeof(tc));
    if (mm_result == MMSYSERR_NOERROR) {
        // Returns TIMERR_NOERROR if successful
        // or TIMERR_NOCANDO if the resolution specified in uPeriod is out of range.
        // See: http://msdn.microsoft.com/zh-cn/dd757624%28v=vs.85%29
        //time_result = timeBeginPeriod(tc.wPeriodMin);
    }
#endif

    //printf("\n");
    printf("MAX_MSG_COUNT       = %u\n"
           "BUFFER_SIZE         = %u\n"
           "PUSH_CNT            = %u\n"
           "POP_CNT             = %u\n",
            MAX_MSG_COUNT, QSIZE, PUSH_CNT, POP_CNT);
#if defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) || defined(_M_IA64) || defined(__amd64__) || defined(__x86_64__)
    printf("x64 Mode            = Yes\n");
#else
    printf("x64 Mode            = No\n");
#endif
#if defined(USE_THREAD_AFFINITY) && (USE_THREAD_AFFINITY != 0)
    printf("USE_THREAD_AFFINITY = Yes\n");
#else
    printf("USE_THREAD_AFFINITY = No\n");
#endif
#if defined(_WIN32) || defined(__MINGW32__) || defined(__CYGWIN__)
  #if (defined(USE_TIME_PERIOD) && (USE_TIME_PERIOD != 0))
    if ((mm_result == MMSYSERR_NOERROR) && (time_noerror != 0))
        printf("USE_TIME_PERIOD     = Yes (%u)\n", tc.wPeriodMin);
    else
        printf("USE_TIME_PERIOD     = Yes (Failed)\n");
  #else
    //printf("USE_TIME_PERIOD     = No\n");
  #endif
#endif  /* _WIN32 || __MINGW32__ || __CYGWIN__ */
    printf("\n");

#if 0
#if defined(__linux__)
#if defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) || defined(_M_IA64) || defined(__amd64__) || defined(__x86_64__)
    printf("msgs ptr         = %016p\n", (void *)msgs);
#else
    printf("msgs ptr         = %08p\n", (void *)msgs);
#endif
#else  /* !__linux__ */
#if defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) || defined(_M_IA64) || defined(__amd64__) || defined(__x86_64__)
    printf("msgs ptr         = 0x%016p\n", (void *)msgs);
#else
    printf("msgs ptr         = 0x%08p\n", (void *)msgs);
#endif  /* _M_X64 || _WIN64 */
#endif  /* __linux__ */

    printf("\n");
#endif
}

void SpinMutex_Test(bool bReadKey = false)
{
    SpinMutex<DefaultSMHelper> spinMutex2;

    typedef SpinMutexHelper<
        1,      // _YieldThreshold, The threshold of enter yield(), the spin loop times.
        2,      // _SpinCountInitial, The initial value of spin counter.
        2,      // _A
        1,      // _B
        0,      // _C, Next loop: spin_count = spin_count * _A / _B + _C;
        4,      // _Sleep_0_Interval, After how many yields should we Sleep(0)?
        32,     // _Sleep_1_Interval, After how many yields should we Sleep(1)?
        true,   // _UseYieldProcessor? Whether use jimi_yield() function in loop.
        false   // _NeedReset? After run Sleep(1), reset the loop_count if need.
    > MySpinMutexHelper;

    SpinMutex<MySpinMutexHelper> spinMutex;

    spinMutex.lock();
    printf(" ... ");
    spinMutex.spinWait(4000);
    printf(" ... ");
    spinMutex.unlock();

    spinMutex.lock();
    printf(" ... ");
    spinMutex.spinWait(4000);
    printf(" ... ");
    spinMutex.unlock();

    printf("\n");

    if (bReadKey)
        jimi_console_readkeyln(false, true, false);
}

void run_some_queue_tests(void)
{
    SmallRingQueue<uint64_t, 1024> smallRingQueue;
    uint64_t ev = 1;
    uint64_t *msg;
    smallRingQueue.push(&ev);
    msg = smallRingQueue.pop();

    DisruptorRingQueue<CValueEvent<uint64_t>, int64_t, QSIZE, PUSH_CNT, POP_CNT> disRingQueue2;
    CValueEvent<uint64_t> event2;
    volatile CValueEvent<uint64_t> ev2(0x12345678ULL);
    CValueEvent<uint64_t> ev3(ev2);
    CValueEvent<uint64_t> ev4(ev3);
    CValueEvent<uint64_t> ev5;
    CValueEvent<uint64_t> event6(0x66666666U);
    ev5 = ev2;
    ev5 = ev3;

    DisruptorRingQueue<CValueEvent<uint64_t>, int64_t, QSIZE, PUSH_CNT, POP_CNT>::PopThreadStackData stackData;
    Sequence tailSequence;
    tailSequence.set(Sequence::INITIAL_CURSOR_VALUE);
    stackData.tailSequence = &tailSequence;
    stackData.nextSequence = stackData.tailSequence->get();
    stackData.cachedAvailableSequence = Sequence::INITIAL_CURSOR_VALUE;
    stackData.processedSequence = true;

    event2.update(ev2);
    disRingQueue2.push(event2);
    disRingQueue2.push(event6);
    disRingQueue2.push(event6);

    disRingQueue2.pop (ev3, stackData);
    disRingQueue2.pop (ev4, stackData);
    disRingQueue2.pop (ev5, stackData);

    disRingQueue2.dump_detail();
    disRingQueue2.dump();

    jimi_console_readkeyln(true, true, false);
}

void run_some_queue_ex_tests(void)
{
    SmallRingQueue<uint64_t, 1024> smallRingQueue;
    uint64_t ev = 1;
    uint64_t *msg;
    smallRingQueue.push(&ev);
    msg = smallRingQueue.pop();

    DisruptorRingQueueEx<CValueEvent<uint64_t>, int64_t, QSIZE, PUSH_CNT, POP_CNT> disRingQueue2;
    CValueEvent<uint64_t> event2;
    volatile CValueEvent<uint64_t> ev2(0x12345678ULL);
    CValueEvent<uint64_t> ev3(ev2);
    CValueEvent<uint64_t> ev4(ev3);
    CValueEvent<uint64_t> ev5;
    CValueEvent<uint64_t> event6(0x66666666U);
    ev5 = ev2;
    ev5 = ev3;

    DisruptorRingQueueEx<CValueEvent<uint64_t>, int64_t, QSIZE, PUSH_CNT, POP_CNT>::PopThreadStackData stackData;
    Sequence tailSequence;
    tailSequence.set(Sequence::INITIAL_CURSOR_VALUE);
    stackData.tailSequence = &tailSequence;
    stackData.nextSequence = stackData.tailSequence->get();
    stackData.cachedAvailableSequence = Sequence::INITIAL_CURSOR_VALUE;
    stackData.processedSequence = true;

    event2.update(ev2);
    disRingQueue2.push(event2);
    disRingQueue2.push(event6);
    disRingQueue2.push(event6);

    disRingQueue2.pop (ev3, stackData);
    disRingQueue2.pop (ev4, stackData);
    disRingQueue2.pop (ev5, stackData);

    disRingQueue2.dump_detail();
    disRingQueue2.dump();

    jimi_console_readkeyln(true, true, false);
}

static const unsigned char s_month_days[] {
    0,
    31, 28, 31, 30, 31, 30,    /* month 1-6  */
    31, 31, 30, 31, 30, 31,    /* month 7-12 */
};

static const unsigned short s_month_ydays[16] {
    0,
    0,   31,  59,  90,  120, 151,   /* month 1-6  */
    181, 212, 243, 273, 304, 334,   /* month 7-12 */
    365, 0, 0
};

static const unsigned short s_month_ydays2[2][16] {
    // Normal year
    {
        0,
        0,   31,  59,  90,  120, 151,   /* month 1-6  */
        181, 212, 243, 273, 304, 334,   /* month 7-12 */
        365, 0, 0
    },
    // Leap year
    {
        0,
        0,   31,  60,  91,  121, 152,   /* month 1-6  */
        182, 213, 244, 274, 305, 335,   /* month 7-12 */
        366, 0, 0
    }
};

static const unsigned short s_month_ydays_v2[2][16] {
    // Normal year
    {
        0,   31,  59,  90,  120, 151,   /* month 1-6  */
        181, 212, 243, 273, 304, 334,   /* month 7-12 */
        365, 0, 0, 0
    },
    // Leap year
    {
        0,   31,  60,  91,  121, 152,   /* month 1-6  */
        182, 213, 244, 274, 305, 335,   /* month 7-12 */
        366, 0, 0, 0
    }
};

static const short s_month_ydays_ex[2][16] {
    // Normal year
    {
        0,
        -1,  30,  58,  89,  119, 150,   /* month 1-6  */
        180, 211, 242, 272, 303, 333,   /* month 7-12 */
        364, 0, 0
    },
    // Leap year
    {
        0,
        -1,  30,  59,  90,  120, 151,   /* month 1-6  */
        181, 212, 243, 273, 304, 334,   /* month 7-12 */
        365, 0, 0
    }
};

static const short s_month_ydays_v3[2][16] {

    // Normal year
    {
        -1,  30,  58,  89,  119, 150,   /* month 1-6  */
        180, 211, 242, 272, 303, 333,   /* month 7-12 */
        364, 0, 0, 0
    },
    // Leap year
    {
        -1,  30,  59,  90,  120, 151,   /* month 1-6  */
        181, 212, 243, 273, 304, 334,   /* month 7-12 */
        365, 0, 0, 0
    }
};

struct year_days_t {
    unsigned short total_days;
    unsigned short is_leap;
};

static const year_days_t s_year_days[] {
    /* 1970 */ {     0, 0 },
    /* 1971 */ {   365, 0 },
    /* 1972 */ {   730, 1 },
    /* 1973 */ {  1096, 0 },
    /* 1974 */ {  1461, 0 },
    /* 1975 */ {  1826, 0 },
    /* 1976 */ {  2191, 1 },
    /* 1977 */ {  2557, 0 },
    /* 1978 */ {  2922, 0 },
    /* 1979 */ {  3287, 0 },
    /* 1980 */ {  3652, 1 },
    /* 1981 */ {  4018, 0 },
    /* 1982 */ {  4383, 0 },
    /* 1983 */ {  4748, 0 },
    /* 1984 */ {  5113, 1 },
    /* 1985 */ {  5479, 0 },
    /* 1986 */ {  5844, 0 },
    /* 1987 */ {  6209, 0 },
    /* 1988 */ {  6574, 1 },
    /* 1989 */ {  6940, 0 },
    /* 1990 */ {  7305, 0 },
    /* 1991 */ {  7670, 0 },
    /* 1992 */ {  8035, 1 },
    /* 1993 */ {  8401, 0 },
    /* 1994 */ {  8766, 0 },
    /* 1995 */ {  9131, 0 },
    /* 1996 */ {  9496, 1 },
    /* 1997 */ {  9862, 0 },
    /* 1998 */ { 10227, 0 },
    /* 1999 */ { 10592, 0 },
    /* 2000 */ { 10957, 1 },
    /* 2001 */ { 11323, 0 },
    /* 2002 */ { 11688, 0 },
    /* 2003 */ { 12053, 0 },
    /* 2004 */ { 12418, 1 },
    /* 2005 */ { 12784, 0 },
    /* 2006 */ { 13149, 0 },
    /* 2007 */ { 13514, 0 },
    /* 2008 */ { 13879, 1 },
    /* 2009 */ { 14245, 0 },
    /* 2010 */ { 14610, 0 },
    /* 2011 */ { 14975, 0 },
    /* 2012 */ { 15340, 1 },
    /* 2013 */ { 15706, 0 },
    /* 2014 */ { 16071, 0 },
    /* 2015 */ { 16436, 0 },
    /* 2016 */ { 16801, 1 },
    /* 2017 */ { 17167, 0 },
    /* 2018 */ { 17532, 0 },
    /* 2019 */ { 17897, 0 },
    /* 2020 */ { 18262, 1 },
    /* 2021 */ { 18628, 0 },
    /* 2022 */ { 18993, 0 },
    /* 2023 */ { 19358, 0 },
    /* 2024 */ { 19723, 1 },
    /* 2025 */ { 20089, 0 },
    /* 2026 */ { 20454, 0 },
    /* 2027 */ { 20819, 0 },
    /* 2028 */ { 21184, 1 },
    /* 2029 */ { 21550, 0 },
    /* 2030 */ { 21915, 0 },
    /* 2031 */ { 22280, 0 },
    /* 2032 */ { 22645, 1 },
    /* 2033 */ { 23011, 0 },
    /* 2034 */ { 23376, 0 },
    /* 2035 */ { 23741, 0 },
    /* 2036 */ { 24106, 1 },
    /* 2037 */ { 24472, 0 },
    /* 2038 */ { 24837, 0 },
    /* 2039 */ { 25202, 0 },
    /* 2040 */ { 25567, 1 },
    /* 2041 */ { 25933, 0 },
    /* 2042 */ { 26298, 0 },
    /* 2043 */ { 26663, 0 },
    /* 2044 */ { 27028, 1 },
    /* 2045 */ { 27394, 0 },
    /* 2046 */ { 27759, 0 },
    /* 2047 */ { 28124, 0 },
    /* 2048 */ { 28489, 1 },
    /* 2049 */ { 28855, 0 },
    /* 2050 */ { 29220, 0 },
    /* 2051 */ { 29585, 0 },
    /* 2052 */ { 29950, 1 },
    /* 2053 */ { 30316, 0 },
    /* 2054 */ { 30681, 0 },
    /* 2055 */ { 31046, 0 },
    /* 2056 */ { 31411, 1 },
    /* 2057 */ { 31777, 0 },
    /* 2058 */ { 32142, 0 },
    /* 2059 */ { 32507, 0 },
    /* 2060 */ { 32872, 1 },
    /* 2061 */ { 33238, 0 },
    /* 2062 */ { 33603, 0 },
    /* 2063 */ { 33968, 0 },
    /* 2064 */ { 34333, 1 },
    /* 2065 */ { 34699, 0 },
    /* 2066 */ { 35064, 0 },
    /* 2067 */ { 35429, 0 },
    /* 2068 */ { 35794, 1 },
    /* 2069 */ { 36160, 0 },
    /* 2070 */ { 36525, 0 },
    /* 2071 */ { 36890, 0 },
    /* 2072 */ { 37255, 1 },
    /* 2073 */ { 37621, 0 },
    /* 2074 */ { 37986, 0 },
    /* 2075 */ { 38351, 0 },
    /* 2076 */ { 38716, 1 },
    /* 2077 */ { 39082, 0 },
    /* 2078 */ { 39447, 0 },
    /* 2079 */ { 39812, 0 },
    /* 2080 */ { 40177, 1 },
    /* 2081 */ { 40543, 0 },
    /* 2082 */ { 40908, 0 },
    /* 2083 */ { 41273, 0 },
    /* 2084 */ { 41638, 1 },
    /* 2085 */ { 42004, 0 },
    /* 2086 */ { 42369, 0 },
    /* 2087 */ { 42734, 0 },
    /* 2088 */ { 43099, 1 },
    /* 2089 */ { 43465, 0 },
    /* 2090 */ { 43830, 0 },
    /* 2091 */ { 44195, 0 },
    /* 2092 */ { 44560, 1 },
    /* 2093 */ { 44926, 0 },
    /* 2094 */ { 45291, 0 },
    /* 2095 */ { 45656, 0 },
    /* 2096 */ { 46021, 1 },
    /* 2097 */ { 46387, 0 },
};

struct year_info_t {
    unsigned int total_days;
    unsigned short is_leap;
    short month_ydays[13];
};

static const year_info_t s_year_info[] = {
    /* 1970 */ {     0, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1971 */ {   365, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1972 */ {   730, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 1973 */ {  1096, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1974 */ {  1461, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1975 */ {  1826, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1976 */ {  2191, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 1977 */ {  2557, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1978 */ {  2922, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1979 */ {  3287, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1980 */ {  3652, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 1981 */ {  4018, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1982 */ {  4383, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1983 */ {  4748, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1984 */ {  5113, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 1985 */ {  5479, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1986 */ {  5844, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1987 */ {  6209, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1988 */ {  6574, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 1989 */ {  6940, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1990 */ {  7305, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1991 */ {  7670, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1992 */ {  8035, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 1993 */ {  8401, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1994 */ {  8766, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1995 */ {  9131, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1996 */ {  9496, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 1997 */ {  9862, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1998 */ { 10227, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 1999 */ { 10592, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2000 */ { 10957, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2001 */ { 11323, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2002 */ { 11688, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2003 */ { 12053, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2004 */ { 12418, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2005 */ { 12784, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2006 */ { 13149, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2007 */ { 13514, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2008 */ { 13879, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2009 */ { 14245, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2010 */ { 14610, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2011 */ { 14975, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2012 */ { 15340, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2013 */ { 15706, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2014 */ { 16071, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2015 */ { 16436, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2016 */ { 16801, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2017 */ { 17167, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2018 */ { 17532, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2019 */ { 17897, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2020 */ { 18262, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2021 */ { 18628, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2022 */ { 18993, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2023 */ { 19358, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2024 */ { 19723, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2025 */ { 20089, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2026 */ { 20454, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2027 */ { 20819, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2028 */ { 21184, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2029 */ { 21550, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2030 */ { 21915, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2031 */ { 22280, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2032 */ { 22645, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2033 */ { 23011, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2034 */ { 23376, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2035 */ { 23741, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2036 */ { 24106, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2037 */ { 24472, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2038 */ { 24837, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2039 */ { 25202, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2040 */ { 25567, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2041 */ { 25933, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2042 */ { 26298, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2043 */ { 26663, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2044 */ { 27028, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2045 */ { 27394, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2046 */ { 27759, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2047 */ { 28124, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2048 */ { 28489, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2049 */ { 28855, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2050 */ { 29220, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2051 */ { 29585, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2052 */ { 29950, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2053 */ { 30316, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2054 */ { 30681, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2055 */ { 31046, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2056 */ { 31411, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2057 */ { 31777, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2058 */ { 32142, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2059 */ { 32507, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2060 */ { 32872, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2061 */ { 33238, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2062 */ { 33603, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2063 */ { 33968, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2064 */ { 34333, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2065 */ { 34699, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2066 */ { 35064, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2067 */ { 35429, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2068 */ { 35794, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2069 */ { 36160, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2070 */ { 36525, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2071 */ { 36890, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2072 */ { 37255, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2073 */ { 37621, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2074 */ { 37986, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2075 */ { 38351, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2076 */ { 38716, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2077 */ { 39082, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2078 */ { 39447, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2079 */ { 39812, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2080 */ { 40177, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2081 */ { 40543, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2082 */ { 40908, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2083 */ { 41273, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2084 */ { 41638, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2085 */ { 42004, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2086 */ { 42369, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2087 */ { 42734, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2088 */ { 43099, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2089 */ { 43465, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2090 */ { 43830, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2091 */ { 44195, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2092 */ { 44560, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2093 */ { 44926, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2094 */ { 45291, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2095 */ { 45656, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
    /* 2096 */ { 46021, 1, { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, } },
    /* 2097 */ { 46387, 0, { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, } },
};

//
// Linux's mktime()
//
// See: https://blog.csdn.net/axx1611/article/details/1792827
// See: https://blog.csdn.net/ok2222991/article/details/21019977
//

JIMI_NOINLINE unsigned long
linux_mktime(unsigned int year, unsigned int month,
                           unsigned int day, unsigned int hour,
                           unsigned int minute, unsigned int second)
{
    if (0 >= (int)(month -= 2)) {   /* 1..12 -> 11,12,1..10 */
        month += 12;                /* Puts Feb last since it has leap day */
        year -= 1;
    }

    return (((
        (unsigned long)(year / 4 - year / 100 + year / 400 + 367 * month / 12 + day) +
                        year * 365 - 719499
        ) * 24 + hour       /* now have hours */
        ) * 60 + minute     /* now have minutes */
        ) * 60 + second;    /* finally seconds */
}

JIMI_NOINLINE unsigned long
__linux_mktime(struct tm * time)
{
    unsigned int year = time->tm_year + 1900;
    unsigned int month = time->tm_mon + 1;
    if (0 >= (int)(month -= 2)) {   /* 1..12 -> 11,12,1..10 */
        month += 12;                /* Puts Feb last since it has leap day */
        year -= 1;
    }

    return (((
        (unsigned long)(year / 4 - year / 100 + year / 400 + 367 * month / 12 + time->tm_mday) +
                        year * 365 - 719499
        ) * 24 + time->tm_hour      /* now have hours */
        ) * 60 + time->tm_min       /* now have minutes */
        ) * 60 + time->tm_sec;      /* finally seconds */
}

JIMI_NOINLINE unsigned long
fast_mktime_v1(unsigned int year, unsigned int month,
                             unsigned int day, unsigned int hour,
                             unsigned int minute, unsigned int second)
{
    int yindex = year - 1970;
    unsigned int year_days = s_year_days[yindex].total_days;

#if 0
    unsigned int is_leap;
    if (month >= 3)
        is_leap = (1 - s_year_days[yindex].is_leap)
    else
        is_leap = 1;
#else
    unsigned int is_leap = (month >= 3) ? (1 - s_year_days[yindex].is_leap) : 1;
#endif

    return (((
        (unsigned long)(year_days + s_month_ydays[month] + day - is_leap)
        * 24 + hour)        /* now have hours */
        * 60 + minute)      /* now have minutes */
        * 60 + second);     /* finally seconds */
}

JIMI_NOINLINE unsigned long
__fast_mktime_v1(struct tm * time)
{
    int yindex = time->tm_year - 70;
    unsigned int year_days = s_year_days[yindex].total_days;

#if 0
    unsigned int is_leap;
    if (time->tm_mon >= 2)
        is_leap = (1 - s_year_days[yindex].is_leap);
    else
        is_leap = 1;
#else
    unsigned int is_leap = (time->tm_mon >= 2) ? (1 - s_year_days[yindex].is_leap) : 1;
#endif

    return (((
        (unsigned long)(year_days + s_month_ydays[time->tm_mon + 1] + time->tm_mday - is_leap)
        * 24 + time->tm_hour)   /* now have hours */
        * 60 + time->tm_min)    /* now have minutes */
        * 60 + time->tm_sec);   /* finally seconds */
}

JIMI_NOINLINE unsigned long
fast_mktime_v2(unsigned int year, unsigned int month,
                             unsigned int day, unsigned int hour,
                             unsigned int minute, unsigned int second)
{
    int yindex = year - 1970;
    unsigned int year_days = s_year_days[yindex].total_days;
    unsigned int is_leap = s_year_days[yindex].is_leap;

    return (((
        (unsigned long)(year_days + s_month_ydays2[is_leap][month] + day - 1)
        * 24 + hour)        /* now have hours */
        * 60 + minute)      /* now have minutes */
        * 60 + second);     /* finally seconds */
}

JIMI_NOINLINE unsigned long
__fast_mktime_v2(struct tm * time)
{
    int yindex = time->tm_year - 70;
    unsigned int year_days = s_year_days[yindex].total_days;
    unsigned int is_leap = s_year_days[yindex].is_leap;

    return (((
        (unsigned long)(year_days + s_month_ydays_v2[is_leap][time->tm_mon] + time->tm_mday - 1)
        * 24 + time->tm_hour)   /* now have hours */
        * 60 + time->tm_min)    /* now have minutes */
        * 60 + time->tm_sec);   /* finally seconds */
}

JIMI_NOINLINE unsigned long
fast_mktime_v3(unsigned int year, unsigned int month,
                             unsigned int day, unsigned int hour,
                             unsigned int minute, unsigned int second)
{
    int yindex = year - 1970;
    unsigned int year_days = s_year_days[yindex].total_days;
    unsigned int is_leap = s_year_days[yindex].is_leap;

    return (((
        (unsigned long)(year_days + s_month_ydays_ex[is_leap][month] + day)
        * 24 + hour)        /* now have hours */
        * 60 + minute)      /* now have minutes */
        * 60 + second);     /* finally seconds */
}

JIMI_NOINLINE unsigned long
__fast_mktime_v3(struct tm * time)
{
    int yindex = time->tm_year - 70;
    unsigned int year_days = s_year_days[yindex].total_days;
    unsigned int is_leap = s_year_days[yindex].is_leap;

    return (((
        (unsigned long)(year_days + s_month_ydays_v3[is_leap][time->tm_mon] + time->tm_mday)
        * 24 + time->tm_hour)   /* now have hours */
        * 60 + time->tm_min)    /* now have minutes */
        * 60 + time->tm_sec);   /* finally seconds */
}

JIMI_NOINLINE unsigned long
fast_mktime_v4(unsigned int year, unsigned int month,
                             unsigned int day, unsigned int hour,
                             unsigned int minute, unsigned int second)
{
    int yindex = year - 1970;
    year_info_t * year_info = (year_info_t *)&s_year_info[yindex];
    unsigned int year_days = year_info->total_days;

    return (((
        (unsigned long)(year_days + year_info->month_ydays[month] + day)
        * 24 + hour)        /* now have hours */
        * 60 + minute)      /* now have minutes */
        * 60 + second);     /* finally seconds */
}

JIMI_NOINLINE unsigned long
__fast_mktime_v4(struct tm * time)
{
    int yindex = time->tm_year - 70;
    year_info_t * year_info = (year_info_t *)&s_year_info[yindex];
    unsigned int year_days = year_info->total_days;

    return (((
        (unsigned long)(year_days + year_info->month_ydays[time->tm_mon + 1] + time->tm_mday)
        * 24 + time->tm_hour)   /* now have hours */
        * 60 + time->tm_min)    /* now have minutes */
        * 60 + time->tm_sec);   /* finally seconds */
}

struct date_time_t {
    unsigned short year; 
    unsigned char month;
    unsigned char day;
    unsigned char day_of_week;
    unsigned char hour;
    unsigned char minute;
    unsigned char second;
};

unsigned int random(unsigned int maximum)
{
    unsigned int rnd;
#if RAND_MAX == 0x7FFF
    rnd = (rand() << 30) | (rand() << 15) | rand();
#else
    rnd = rand();
#endif
    return (rnd % (maximum + 1));
}

unsigned int random(unsigned int minimum, unsigned int maximum)
{
    unsigned int rnd;
#if RAND_MAX == 0x7FFF
    rnd = (rand() << 30) | (rand() << 15) | rand();
#else
    rnd = rand();
#endif
    if (minimum <= maximum)
        return (rnd % (maximum - minimum + 1)) + minimum;
    else
        return (rnd % (minimum - maximum + 1)) + maximum;
}

int get_day_of_week(int year, int month, int day)
{
    int yindex = year - 1970;
    unsigned int year_days = s_year_days[yindex].total_days;
    unsigned int is_leap = s_year_days[yindex].is_leap;

    return ((year_days + s_month_ydays_ex[is_leap][month] + day + 4) % 7);
}

static const unsigned int kMaxRepeatTime = 100;
static const unsigned int kMaxTestTime = 10000;

void test_mktime()
{
    date_time_t test_time[kMaxTestTime];
    for (unsigned int i = 0; i < kMaxTestTime; i++) {
        int year = 1970 + random(67);
        test_time[i].year = year;
        int month = random(1, 12);
        test_time[i].month = month;
        int day = random(1, s_month_days[month]);
        test_time[i].day = day;
        test_time[i].day_of_week = get_day_of_week(year, month, day);
        test_time[i].hour = random(0, 23);
        test_time[i].minute = random(0, 59);
        test_time[i].second = random(0, 59);
    }

    printf("---------------------------------------------------------------\n\n");

    jmc_timestamp_t startTime, stopTime;
    jmc_timefloat_t elapsedTime = 0.0;
    unsigned long timestamp, checksum;

    // linux_mktime()
    printf("test: linux_mktime()\n\n");

    startTime = jmc_get_timestamp();

    checksum = 0;
    for (unsigned int repeat = 0; repeat < kMaxRepeatTime; repeat++) {
        for (unsigned int i = 0; i < kMaxTestTime; i++) {
            timestamp = linux_mktime(test_time[i].year, test_time[i].month, test_time[i].day,
                                     test_time[i].hour, test_time[i].minute, test_time[i].second);
            checksum += timestamp;
        }
    }

    stopTime = jmc_get_timestamp();
    elapsedTime = jmc_get_interval_millisecf(stopTime - startTime);

    printf("time elapsed: %9.3f ms, ", elapsedTime);
    printf("checksum: %u\n", checksum);
    printf("\n");

    // fast_mktime_v1()
    printf("test: fast_mktime_v1()\n\n");

    startTime = jmc_get_timestamp();

    checksum = 0;
    for (unsigned int repeat = 0; repeat < kMaxRepeatTime; repeat++) {
        for (unsigned int i = 0; i < kMaxTestTime; i++) {
            timestamp = fast_mktime_v1(test_time[i].year, test_time[i].month, test_time[i].day,
                                       test_time[i].hour, test_time[i].minute, test_time[i].second);
            checksum += timestamp;
        }
    }

    stopTime = jmc_get_timestamp();
    elapsedTime = jmc_get_interval_millisecf(stopTime - startTime);

    printf("time elapsed: %9.3f ms, ", elapsedTime);
    printf("checksum: %u\n", checksum);
    printf("\n");

    // fast_mktime_v2()
    printf("test: fast_mktime_v2()\n\n");

    startTime = jmc_get_timestamp();

    checksum = 0;
    for (unsigned int repeat = 0; repeat < kMaxRepeatTime; repeat++) {
        for (unsigned int i = 0; i < kMaxTestTime; i++) {
            timestamp = fast_mktime_v2(test_time[i].year, test_time[i].month, test_time[i].day,
                                       test_time[i].hour, test_time[i].minute, test_time[i].second);
            checksum += timestamp;
        }
    }

    stopTime = jmc_get_timestamp();
    elapsedTime = jmc_get_interval_millisecf(stopTime - startTime);

    printf("time elapsed: %9.3f ms, ", elapsedTime);
    printf("checksum: %u\n", checksum);
    printf("\n");

    // fast_mktime_v3()
    printf("test: fast_mktime_v3()\n\n");

    startTime = jmc_get_timestamp();

    checksum = 0;
    for (unsigned int repeat = 0; repeat < kMaxRepeatTime; repeat++) {
        for (unsigned int i = 0; i < kMaxTestTime; i++) {
            timestamp = fast_mktime_v3(test_time[i].year, test_time[i].month, test_time[i].day,
                                       test_time[i].hour, test_time[i].minute, test_time[i].second);
            checksum += timestamp;
        }
    }

    stopTime = jmc_get_timestamp();
    elapsedTime = jmc_get_interval_millisecf(stopTime - startTime);

    printf("time elapsed: %9.3f ms, ", elapsedTime);
    printf("checksum: %u\n", checksum);
    printf("\n");

    // fast_mktime_v4()
    printf("test: fast_mktime_v4()\n\n");

    startTime = jmc_get_timestamp();

    checksum = 0;
    for (unsigned int repeat = 0; repeat < kMaxRepeatTime; repeat++) {
        for (unsigned int i = 0; i < kMaxTestTime; i++) {
            timestamp = fast_mktime_v4(test_time[i].year, test_time[i].month, test_time[i].day,
                                       test_time[i].hour, test_time[i].minute, test_time[i].second);
            checksum += timestamp;
        }
    }

    stopTime = jmc_get_timestamp();
    elapsedTime = jmc_get_interval_millisecf(stopTime - startTime);

    printf("time elapsed: %9.3f ms, ", elapsedTime);
    printf("checksum: %u\n", checksum);
    printf("\n");

    // verify
    unsigned long timestamp1, timestamp2;
    for (unsigned int i = 0; i < kMaxTestTime; i++) {
        timestamp1 = linux_mktime(test_time[i].year, test_time[i].month, test_time[i].day,
                                  test_time[i].hour, test_time[i].minute, test_time[i].second);
        timestamp2 = fast_mktime_v4(test_time[i].year, test_time[i].month, test_time[i].day,
                                    test_time[i].hour, test_time[i].minute, test_time[i].second);
#ifndef NDEBUG
        if (timestamp1 != timestamp2) {
            printf("[%u] -- year: %u, month: %u, day: %u, hour: %u, minute: %u, second: %u\n", i,
                   test_time[i].year, test_time[i].month, test_time[i].day,
                   test_time[i].hour, test_time[i].minute, test_time[i].second);
        }
#endif
    }

    printf("---------------------------------------------------------------\n\n");

    //jimi_console_readkeyln(false, true, false);
}

//
// 英国夏令时: 3月的最后一个周日 01:00 ~ 10月的最后一个周日 01:00
// 中国夏令时: 1986年至1991年, 四月中旬第一个星期日的凌晨 2 时开始 ~ 九月中旬第一个星期日的凌晨 2 时结束. 
//

void test_mktime_tm()
{
    struct tm when[kMaxTestTime];
    for (unsigned int i = 0; i < kMaxTestTime; i++) {
        // Zero initialize
        memset((void *)&when[i], 0, sizeof(struct tm));
        // Limit is 23:59:59 January 18, 2038
        int year = 1970 + random(67);
        when[i].tm_year = year - 1900;
        int month = random(1, 12);
        when[i].tm_mon = month - 1;
        int day = random(1, s_month_days[month]);
        when[i].tm_mday = day;
        when[i].tm_wday = get_day_of_week(year, month, day);
        when[i].tm_yday = s_month_ydays[month] + day - 1;
        when[i].tm_hour = random(0, 23);
        when[i].tm_min = random(0, 59);
        when[i].tm_sec = random(0, 59);
        when[i].tm_isdst = 0;
    }

    printf("---------------------------------------------------------------\n\n");

    jmc_timestamp_t startTime, stopTime;
    jmc_timefloat_t elapsedTime = 0.0;
    unsigned long timestamp, checksum;

#if defined(__GUNC__) || defined(__linux__) \
   || defined(__clang__) || defined(__APPLE__) || defined(__FreeBSD__) \
   || defined(__CYGWIN__) || defined(__MINGW32__)
    // mktime(tm)
    printf("test: mktime(tm)\n\n");

    setenv("TZ", "Asia/Shanghai", 0);

    startTime = jmc_get_timestamp();

    checksum = 0;
    for (unsigned int repeat = 0; repeat < kMaxRepeatTime; repeat++) {
        for (unsigned int i = 0; i < kMaxTestTime; i++) {
            timestamp = (unsigned long)mktime(&when[i]) + 8 * 3600;
            checksum += timestamp;
        }
    }

    stopTime = jmc_get_timestamp();
    elapsedTime = jmc_get_interval_millisecf(stopTime - startTime);

    printf("time elapsed: %9.3f ms, ", elapsedTime);
    printf("checksum: %u\n", checksum);
    printf("\n");
#endif // __GUNC__

#if defined(_MSC_VER) || ((defined(__INTER_COMPILER) || defined(__ICC)) && !(defined(GNUC) || defined(__linux__)))
    // mktime(tm)
    printf("test: _mktime32(tm)\n\n");

    startTime = jmc_get_timestamp();

    checksum = 0;
    for (unsigned int repeat = 0; repeat < kMaxRepeatTime; repeat++) {
        for (unsigned int i = 0; i < kMaxTestTime; i++) {
            timestamp = (unsigned long)_mktime32(&when[i]) + 8 * 3600;
            checksum += timestamp;
        }
    }

    stopTime = jmc_get_timestamp();
    elapsedTime = jmc_get_interval_millisecf(stopTime - startTime);

    printf("time elapsed: %9.3f ms, ", elapsedTime);
    printf("checksum: %u\n", checksum);
    printf("\n");
#endif // _MSC_VER

    // linux_mktime(tm)
    printf("test: linux_mktime(tm)\n\n");

    startTime = jmc_get_timestamp();

    checksum = 0;
    for (unsigned int repeat = 0; repeat < kMaxRepeatTime; repeat++) {
        for (unsigned int i = 0; i < kMaxTestTime; i++) {
            timestamp = __linux_mktime(&when[i]);
            checksum += timestamp;
        }
    }

    stopTime = jmc_get_timestamp();
    elapsedTime = jmc_get_interval_millisecf(stopTime - startTime);

    printf("time elapsed: %9.3f ms, ", elapsedTime);
    printf("checksum: %u\n", checksum);
    printf("\n");

    // fast_mktime_v1(tm)
    printf("test: fast_mktime_v1(tm)\n\n");

    startTime = jmc_get_timestamp();

    checksum = 0;
    for (unsigned int repeat = 0; repeat < kMaxRepeatTime; repeat++) {
        for (unsigned int i = 0; i < kMaxTestTime; i++) {
            timestamp = __fast_mktime_v1(&when[i]);
            checksum += timestamp;
        }
    }

    stopTime = jmc_get_timestamp();
    elapsedTime = jmc_get_interval_millisecf(stopTime - startTime);

    printf("time elapsed: %9.3f ms, ", elapsedTime);
    printf("checksum: %u\n", checksum);
    printf("\n");

    // fast_mktime_v2(tm)
    printf("test: fast_mktime_v2(tm)\n\n");

    startTime = jmc_get_timestamp();

    checksum = 0;
    for (unsigned int repeat = 0; repeat < kMaxRepeatTime; repeat++) {
        for (unsigned int i = 0; i < kMaxTestTime; i++) {
            timestamp = __fast_mktime_v2(&when[i]);
            checksum += timestamp;
        }
    }

    stopTime = jmc_get_timestamp();
    elapsedTime = jmc_get_interval_millisecf(stopTime - startTime);

    printf("time elapsed: %9.3f ms, ", elapsedTime);
    printf("checksum: %u\n", checksum);
    printf("\n");

    // fast_mktime_v3(tm)
    printf("test: fast_mktime_v3(tm)\n\n");

    startTime = jmc_get_timestamp();

    checksum = 0;
    for (unsigned int repeat = 0; repeat < kMaxRepeatTime; repeat++) {
        for (unsigned int i = 0; i < kMaxTestTime; i++) {
            timestamp = __fast_mktime_v3(&when[i]);
            checksum += timestamp;
        }
    }

    stopTime = jmc_get_timestamp();
    elapsedTime = jmc_get_interval_millisecf(stopTime - startTime);

    printf("time elapsed: %9.3f ms, ", elapsedTime);
    printf("checksum: %u\n", checksum);
    printf("\n");

    // fast_mktime_v4(tm)
    printf("test: fast_mktime_v4(tm)\n\n");

    startTime = jmc_get_timestamp();

    checksum = 0;
    for (unsigned int repeat = 0; repeat < kMaxRepeatTime; repeat++) {
        for (unsigned int i = 0; i < kMaxTestTime; i++) {
            timestamp = __fast_mktime_v4(&when[i]);
            checksum += timestamp;
        }
    }

    stopTime = jmc_get_timestamp();
    elapsedTime = jmc_get_interval_millisecf(stopTime - startTime);

    printf("time elapsed: %9.3f ms, ", elapsedTime);
    printf("checksum: %u\n", checksum);
    printf("\n");

    // verify
    unsigned long timestamp1, timestamp2;
    for (unsigned int i = 0; i < kMaxTestTime; i++) {
#if defined(_MSC_VER) || ((defined(__INTER_COMPILER) || defined(__ICC)) && !(defined(GNUC) || defined(__linux__)))
        timestamp1 = _mktime32(&when[i]) + 8 * 3600;
#else
        timestamp1 = mktime(&when[i]) + 8 * 3600;
#endif
        timestamp2 = __linux_mktime(&when[i]);
#ifndef NDEBUG
        if (timestamp1 != timestamp2) {
            printf("[%u] -- year: %u, month: %u, day: %u, hour: %u, minute: %u, second: %u\n", i,
                   when[i].tm_year + 1900, when[i].tm_mon + 1, when[i].tm_mday,
                   when[i].tm_hour, when[i].tm_min, when[i].tm_sec);
            printf("[%u] -- timestamp1: %u, timestamp2: %u, diff: %u\n", i, timestamp1, timestamp2,
                    (timestamp1 >= timestamp2) ? (timestamp1 - timestamp2) : (timestamp2 - timestamp1));
        }
#endif
    }

    printf("---------------------------------------------------------------\n\n");

    //jimi_console_readkeyln(false, true, false);
}

void print_year_info()
{
    static const unsigned int kStartYear = 1970;
    static const unsigned int kYearStep = 128;
    unsigned int year;
    unsigned int years, days, is_leap;
    printf("static const unsigned short s_year_days[] {\r\n");
    for (year = kStartYear; year < (kStartYear + kYearStep); year++) {
        years = year - 1;
        days = ((years / 4 - years / 100 + years / 400)
                + 367 * (1 + 12 - 2) / 12 + 1)
                + years * 365 - 719499;
        is_leap = ((((year % 4) == 0) && ((year % 100) != 0)) || ((year % 400) == 0)) ? 1 : 0;
        printf("    /* %u */ { %5u, %u },\r\n", year, days, is_leap);
    }
    printf("};\r\n");
    printf("\r\n");

    printf("static const year_info_t s_year_info[] {\r\n");
    for (year = kStartYear; year < (kStartYear + kYearStep); year++) {
        years = year - 1;
        days = ((years / 4 - years / 100 + years / 400)
                + 367 * (1 + 12 - 2) / 12 + 1)
                + years * 365 - 719499;
        is_leap = ((((year % 4) == 0) && ((year % 100) != 0)) || ((year % 400) == 0)) ? 1 : 0;
        printf("    /* %u */ { %5u, %u, { 0, ", year, days, is_leap);
        for (unsigned int month = 1; month <= 12; month++) {
            if (month >= 13)
                printf("0 ");
            else if (month > 12)
                printf("0, ");
            else if (month >= 3 && is_leap != 0)
                printf("%d, ", (int)(s_month_ydays[month]));
            else
                printf("%d, ", (int)(s_month_ydays[month] - 1));
        }
        printf("} },\r\n");
    }
    printf("};\r\n");
    printf("\r\n");
}

int main(int argn, char * argv[])
{
#if defined(USE_DOUBAN_QUEUE) && (USE_DOUBAN_QUEUE != 0)
    bool bContinue = true;
#else
    bool bContinue = false;
#endif

    ::srand((unsigned int)::time(NULL));

#if (defined(USE_TIME_PERIOD) && (USE_TIME_PERIOD != 0)) \
    && (defined(_WIN32) || defined(__MINGW32__) || defined(__CYGWIN__))
    TIMECAPS tc;
    MMRESULT mm_result, time_result;
    time_result = TIMERR_NOCANDO;
    tc.wPeriodMin = 0;
    tc.wPeriodMax = 0;
    // See: http://msdn.microsoft.com/zh-cn/dd757627%28v=vs.85%29
    mm_result = timeGetDevCaps(&tc, sizeof(tc));
#ifdef _DEBUG
    printf("wPeriodMin = %u, wPeriodMax = %u\n", tc.wPeriodMin, tc.wPeriodMax);
#endif
    if (mm_result == MMSYSERR_NOERROR) {
        // Returns TIMERR_NOERROR if successful
        // or TIMERR_NOCANDO if the resolution specified in uPeriod is out of range.
        // See: http://msdn.microsoft.com/zh-cn/dd757624%28v=vs.85%29
        time_result = timeBeginPeriod(tc.wPeriodMin);
#ifdef _DEBUG
        if (time_result == TIMERR_NOERROR)
            printf("timeBeginPeriod(%u) = TIMERR_NOERROR\n", tc.wPeriodMin);
        else
            printf("timeBeginPeriod(%u) = %u\n", tc.wPeriodMin, time_result);
#endif
    }
#ifdef _DEBUG
    printf("\n");
#endif
#endif  /* defined(USE_TIME_PERIOD) && (USE_TIME_PERIOD != 0) */

    jimi_cpu_warmup(500);

#ifdef _DEBUG
    //run_some_queue_tests();
#endif
    //run_some_queue_ex_tests();

    test_msg_init();
    popmsg_list_init();

#if (defined(USE_TIME_PERIOD) && (USE_TIME_PERIOD != 0)) \
    && (defined(_WIN32) || defined(__MINGW32__) || defined(__CYGWIN__))
    display_test_info((int)(time_result == TIMERR_NOERROR));
#else
    display_test_info(0);
#endif

    //print_year_info();
    test_mktime();
    test_mktime_tm();

    // 单线程顺序执行的版本, 每次push(), pop()的消息数量(即步长)由max_step决定, 最大步长为QSIZE
    SerialRingQueue_Test();

#if 0
    // Test (Single Producer + Single Consumer) RingQueue
    SingleProducerSingleConsumer_Test();

#if defined(USE_JIMI_RINGQUEUE) && (USE_JIMI_RINGQUEUE != 0)
  #if !defined(TEST_FUNC_TYPE) || (TEST_FUNC_TYPE == 0)
    //RingQueue_UnitTest();

    //RingQueue_Test(0, true);

    //RingQueue_Test(3, true);

    // 使用pthread_mutex_t, 调用RingQueue.mutex_push().
    //RingQueue_Test(FUNC_RINGQUEUE_MUTEX_PUSH, true);

    // 混合自旋锁, 速度较快, 不够稳定, 调用RingQueue.spin_push().
    RingQueue_Test(FUNC_RINGQUEUE_SPIN_PUSH,  true);

    // 混合自旋锁, 速度较快, 不够稳定, 调用RingQueue.spin1_push().
    RingQueue_Test(FUNC_RINGQUEUE_SPIN1_PUSH, true);

    // 混合自旋锁, 速度快, 且稳定, 调用RingQueue.spin2_push().
    RingQueue_Test(FUNC_RINGQUEUE_SPIN2_PUSH, true);

    // C++ 版的 Disruptor (多生产者 + 多消费者)实现方案.
    RingQueue_Test(FUNC_DISRUPTOR_RINGQUEUE, true);

    // C++ 版的 Disruptor (多生产者 + 多消费者)实现方案.
    RingQueue_Test(FUNC_DISRUPTOR_RINGQUEUE_EX, bContinue);

    // 调用RingQueue.spin3_push().
    //RingQueue_Test(FUNC_RINGQUEUE_SPIN3_PUSH, bContinue);

  #else
    // 连续测试3次
    static const int kMaxPassNum = 3;
    bConti = true;
    for (int n = 1; n <= kMaxPassNum; ++n) {
        // 根据指定的 TEST_FUNC_TYPE 执行RingQueue相应的push()和pop()函数
#if !defined(USE_DOUBAN_QUEUE) || (USE_DOUBAN_QUEUE == 0)
        if (n == kMaxPassNum)
            bConti = false;
#endif
        RingQueue_Test(TEST_FUNC_TYPE, bContinue);
    }
  #endif
#endif

    //RingQueue_Test(0, true);

#if defined(USE_DOUBAN_QUEUE) && (USE_DOUBAN_QUEUE != 0)
    // 豆瓣上的 q3.h 的修正版
    q3_test();
    //RingQueue_Test(FUNC_DOUBAN_Q3H, false);
#endif

    //SpinMutex_Test();
#endif

    popmsg_list_destory();
    test_msg_destory();

#if (defined(USE_TIME_PERIOD) && (USE_TIME_PERIOD != 0)) \
    && (defined(_WIN32) || defined(__MINGW32__) || defined(__CYGWIN__))
    if (time_result == TIMERR_NOERROR) {
        time_result = timeEndPeriod(tc.wPeriodMin);
    }
#endif

    jimi_console_readkeyln(false, true, false);
    return 0;
}
