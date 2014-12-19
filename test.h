
#ifndef _RINGQUEUE_TEST_H_
#define _RINGQUEUE_TEST_H_

#define QSIZE           (1 << 10)
#define QMASK           (QSIZE - 1)

#define PUSH_CNT        4
#define POP_CNT         4

#if 1
#define MAX_MSG_LENGTH  1000000
#else
#define MAX_MSG_LENGTH  10000
#endif

#define MSG_TOTAL_CNT   (PUSH_CNT * MAX_MSG_LENGTH)
#define POP_MSG_LENGTH  (MSG_TOTAL_CNT / POP_CNT)

#ifndef USE_JIMI_RINGQUEUE
#define USE_JIMI_RINGQUEUE      1
#endif

#ifndef USE_LOCKED_RINGQUEUE
#define USE_LOCKED_RINGQUEUE    0
#endif

#define CACHE_LINE_SIZE     64

#ifdef __cplusplus
extern "C" {
#endif

typedef
struct msg_t {
    uint64_t dummy;
} msg_t;

typedef
struct lock_t {
    volatile char pad1[CACHE_LINE_SIZE];
    volatile uint32_t lock;
    volatile uint32_t counter;
    volatile uint32_t spin_count;
    volatile uint32_t ref;
    volatile char pad2[CACHE_LINE_SIZE - 4 * sizeof(uint32_t)];
} lock_t;

#ifdef __cplusplus
}
#endif

#endif  /* _RINGQUEUE_TEST_H_ */
