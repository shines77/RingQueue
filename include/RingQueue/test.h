
#ifndef _RINGQUEUE_TEST_H_
#define _RINGQUEUE_TEST_H_

#include "vs_stdint.h"

/// RingQueue的容量(QSIZE, 队列长度, 必须是2的幂次方)和Mask值
#define QSIZE           (1 << 10)
#define QMASK           (QSIZE - 1)

/// 分别定义push(推送)和pop(弹出)的线程数
#define PUSH_CNT        2
#define POP_CNT         2

/// 分发给各个线程的消息总长度, 是各个线程消息数量的总和
#if 1
#define MSG_TOTAL_LENGTH    8000000
#else
#define MSG_TOTAL_LENGTH    80000
#endif

/// 等同于MSG_TOTAL_LENGTH
#define MSG_TOTAL_CNT       (MSG_TOTAL_LENGTH)

/// 分发给每个(push)线程的消息数量, 等同于MAX_PUSH_MSG_LENGTH
#define MAX_PUSH_MSG_LENGTH (MSG_TOTAL_LENGTH / PUSH_CNT)

/// 分发给每个(pop)线程的消息数量
#define MAX_POP_MSG_LENGTH  (MSG_TOTAL_LENGTH / POP_CNT)

/// 是否设置线程的CPU亲缘性(0不启用, 1启用, 默认不启用,
///       该选项在Windows下无效, 在虚拟机里更是不能启用)
#ifndef USE_THREAD_AFFINITY
#define USE_THREAD_AFFINITY     0
#endif

/// 是否运行q3.h的测试代码
#ifndef USE_DOUBAN_QUEUE
#define USE_DOUBAN_QUEUE        0
#endif

/// 是否运行jimi:RingQueue的测试代码
#ifndef USE_JIMI_RINGQUEUE
#define USE_JIMI_RINGQUEUE      1
#endif

///
/// RingQueue锁的类型定义: (如果该宏RINGQUEUE_LOCK_TYPE未定义, 则等同于定义为0)
///
/// 定义为0, 表示使用豆瓣上q3.h的lock-free改良型方案, 调用RingQueue.push(), RingQueue.pop();
/// 定义为1, 表示使用细粒度的标准spin_mutex锁, 调用RingQueue.spin_push(), RingQueue.spin_pop();
/// 定义为2, 表示使用细粒度的改进型spin_mutex锁, 调用RingQueue.spin1_push(), RingQueue.spin1_pop();
/// 定义为3, 表示使用细粒度的仿制spin_mutex锁(会死锁), 调用RingQueue.spin2_push(), RingQueue.spin2_pop();
/// 定义为4, 表示使用粗粒度的pthread_mutex_t锁, 调用RingQueue.locked_push(), RingQueue.locked_pop().
///
/// 其中只有1, 2, 4都可以得到正确结果, 而且1速度最快;
///
/// 0可能会导致逻辑错误, 结果错误, 而且当(PUSH_CNT + POP_CNT) > CPU物理核心数时,
///     有可能不能完成测试或运行时间很久(几十秒或几分钟不等, 而且结果还是错误的), 可自行验证.
///
/// 3可能会慢如蜗牛(消息在运行但是走得很慢很慢, 甚至死锁);
///

/// 取值范围是 0-3
#ifndef RINGQUEUE_LOCK_TYPE
#define RINGQUEUE_LOCK_TYPE     3
#endif

///
/// 在spin_mutex里是否使用spin_counter计数, 0为不使用(更快!建议设为该值), 1为使用
///
#define USE_SPIN_MUTEX_COUNTER  1

///
/// spin_mutex的最大spin_count值, 默认值为16, 建议设为0或1,2, 更快! 设为0则跟USE_SPIN_MUTEX_COUNTER设为0等价
///
#define MUTEX_MAX_SPIN_COUNT    1

#define SPIN_YIELD_THRESHOLD    1

/// 缓存的CacheLineSize(x86上是64字节)
#define CACHE_LINE_SIZE         64

#ifdef __cplusplus
extern "C" {
#endif

typedef
struct msg_t {
    uint64_t dummy;
} msg_t;

typedef
struct spin_mutex_t {
    volatile char padding1[CACHE_LINE_SIZE];
    volatile uint32_t locked;
    volatile uint32_t spin_counter;
    volatile uint32_t recurse_counter;
    volatile uint32_t thread_id;
    volatile char padding2[CACHE_LINE_SIZE - 4 * sizeof(uint32_t)];
} spin_mutex_t;

#ifdef __cplusplus
}
#endif

#endif  /* _RINGQUEUE_TEST_H_ */
