
#ifndef _RINGQUEUE_TEST_H_
#define _RINGQUEUE_TEST_H_

#define QSIZE       (1 << 10)
#define QMASK       (QSIZE - 1)

#define CACHE_LINE_SIZE     64

typedef
struct msg_t {
    uint64_t dummy;
} msg_t;

#ifdef __cplusplus
extern "C" {
#endif

//

#ifdef __cplusplus
}
#endif

#endif  /* _RINGQUEUE_TEST_H_ */
