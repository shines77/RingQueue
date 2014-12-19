
#ifndef _RINGQUEUE_TEST_H_
#define _RINGQUEUE_TEST_H_

#define QSIZE           (1 << 10)
#define QMASK           (QSIZE - 1)

#define PUSH_CNT        2
#define POP_CNT         2

#if 1
#define MAX_MSG_LENGTH  1000000
#else
#define MAX_MSG_LENGTH  10000
#endif

#define MSG_CNT         (PUSH_CNT * MAX_MSG_LENGTH)
#define POP_MSG_LENGTH  (MSG_CNT / POP_CNT)

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

#ifdef __cplusplus
}
#endif

#endif  /* _RINGQUEUE_TEST_H_ */
