
#ifndef _RINGQUEUE_TEST_H_
#define _RINGQUEUE_TEST_H_

#include "vs_stdint.h"

////////////////////////////////////////////////////////////////////////////////

/// RingQueue的容量(QSIZE, 队列长度, 必须是2的幂次方)和Mask值
#define QSIZE               (1 << 10)
/// 下面一行请不要修改, 切记!!! qmask = qsize - 1
#define QMASK               (QSIZE - 1)

/// 分别定义push(推送)和pop(弹出)的线程数
#define PUSH_CNT            2
#define POP_CNT             2

/// 分发给各个线程的消息总长度, 是各个线程消息数量的总和
/// 如果是虚拟机里测试, 请自己修改为后面那个定义 8000
#if 1
#define MAX_MSG_COUNT           (8000000 * 1)
#else
#define MAX_MSG_COUNT           8000
#endif

/// 等同于MAX_MSG_COUNT
#define MAX_MSG_CNT             (MAX_MSG_COUNT)

/// 分发给每个(push)线程的消息数量, 等同于MAX_PUSH_MSG_COUNT
#define MAX_PUSH_MSG_COUNT      (MAX_MSG_COUNT / PUSH_CNT)

/// 分发给每个(pop)线程的消息数量
#define MAX_POP_MSG_COUNT       (MAX_MSG_COUNT / POP_CNT)

////////////////////////////////////////////////////////////////////////////////

/// 是否根据编译环境自动决定是否使用 64bit 的 sequence (序号), 默认为 0 (不自动)
#define AUTO_SCAN_64BIT_SEQUENCE    0

/// 是否使用 64bit 的 sequence (序号), 默认值为 0 (不使用)
#define USE_64BIT_SEQUENCE          0

/// 根据实际编译环境决定是否使用 64 bit sequence ?
#if defined(AUTO_SCAN_64BIT_SEQUENCE) && (AUTO_SCAN_64BIT_SEQUENCE != 0)
  #undef USE_64BIT_SEQUENCE
  #if defined(_M_X64) || defined(_M_AMD64) || defined(_M_IA64)
    #define USE_64BIT_SEQUENCE      1
  #else
    #define USE_64BIT_SEQUENCE      0
  #endif
#endif

////////////////////////////////////////////////////////////////////////////////

/// 是否设置线程的CPU亲缘性(0不启用, 1启用, 默认不启用,
///       该选项在虚拟机里最好不要启用, VirtualBox虚拟机只用了一个 CPU核心)
#ifndef USE_THREAD_AFFINITY
#define USE_THREAD_AFFINITY     0
#endif

/// 是否设置系统的时间片最小间隔时间, 对Sleep()的精度有影响(0不启用, 1启用, 默认不启用,
///       该选项只有Windows下才有效)
#ifndef USE_TIME_PERIOD
#define USE_TIME_PERIOD         0
#endif

/// 是否运行jimi:RingQueue的测试代码
#ifndef USE_JIMI_RINGQUEUE
#define USE_JIMI_RINGQUEUE      1
#endif

/// 是否运行q3.h的测试代码
#ifndef USE_DOUBAN_QUEUE
#define USE_DOUBAN_QUEUE        1
#endif

////////////////////////////////////////////////////////////////////////////////

///
/// RingQueue测试函数类型定义: (如果该宏TEST_FUNC_TYPE未定义, 则等同于定义为0)
///
/// 定义为1, 表示使用细粒度的标准spin_mutex自旋锁,   调用RingQueue.spin_push(),  RingQueue.spin_pop();
/// 定义为2, 表示使用细粒度的改进型spin_mutex自旋锁, 调用RingQueue.spin1_push(), RingQueue.spin1_pop();
/// 定义为3, 表示使用细粒度的通用型spin_mutex自旋锁, 调用RingQueue.spin2_push(), RingQueue.spin2_pop();
/// 定义为7, 表示使用粗粒度的pthread_mutex_t锁(Windows上为临界区, Linux上为pthread_mutex_t),
///          调用RingQueue.mutex_push(), RingQueue.mutex_pop();
/// 定义为8, 表示使用豆瓣上q3.h的lock-free改进版,    调用RingQueue.push(), RingQueue.pop();
/// 定义为9, 表示使用细粒度的仿制spin_mutex自旋锁(会死锁), 调用RingQueue.spin3_push(), RingQueue.spin3_pop();
///
/// 其中 8 可能会导致逻辑错误, 结果错误, 而且当(PUSH_CNT + POP_CNT) > CPU物理核心数时,
///     有可能不能完成测试或运行时间很久(几十秒或几分钟不等, 而且结果还是错误的), 可自行验证.
///
/// 其中只有1, 2, 3, 7都可以得到正确结果, 2的速度可能最快, 3最稳定(推荐);
///
/// 此外, 9 可能会慢如蜗牛(消息在运行但是走得很慢很慢, 甚至死锁);
///

/// 运行 main() 里指定的某几种 RingQueue 测试
#define FUNC_RINGQUEUE_MULTI_TEST       0

/// 标准型spin_mutex自旋锁, 调用RingQueue::spin_push(), RingQueue::spin_pop(),   速度较快, 但不够稳定
#define FUNC_RINGQUEUE_SPIN_PUSH        1

/// 改进型spin_mutex自旋锁, 调用RingQueue::spin1_push(), RingQueue::spin1_pop(), 速度较快, 但不够稳定
#define FUNC_RINGQUEUE_SPIN1_PUSH       2

/// 通用型spin_mutex自旋锁, 调用RingQueue::spin2_push(), RingQueue::spin2_pop(), 最稳定, 且速度快
#define FUNC_RINGQUEUE_SPIN2_PUSH       3

/// 仿制的spin_mutex自旋锁(会死锁), 调用RingQueue::spin3_push(), RingQueue::spin3_pop(), 不推荐
#define FUNC_RINGQUEUE_SPIN3_PUSH       9

/// TODO:
#define FUNC_RINGQUEUE_SPIN6_PUSH       6

/// 系统自带的互斥锁, Windows上为临界区, Linux上为pthread_mutex_t,
/// 调用: RingQueue::mutex_push(), RingQueue::mutex_pop();
#define FUNC_RINGQUEUE_MUTEX_PUSH       4

/// 豆瓣上 q3.h 的lock-free改进版, 调用RingQueue.push(), RingQueue.pop();
#define FUNC_RINGQUEUE_PUSH             8

/// TODO:
#define FUNC_RINGQUEUE_SPIN9_PUSH       10

///
/// RingQueue测试函数类型定义: (如果该宏TEST_FUNC_TYPE未定义, 则等同于定义为0)
///
/// 定义为 1-9, 表示只运行 TEST_FUNC_TYPE 指定的测试类型, 类型值定义具体见上.
///
/// 定义为: FUNC_RINGQUEUE_MULTI_TEST   0, 运行多个测试, 在 main() 里指定某几种 RingQueue 测试;
/// 定义为: FUNC_RINGQUEUE_SPIN2_PUSH   3, spin2_push(), 最快, 且最稳定
///
/// 建议你定义为: FUNC_RINGQUEUE_MULTI_TEST (测多个),
///          或者 FUNC_RINGQUEUE_SPIN2_PUSH (只测最全面的一个)
///
#ifndef TEST_FUNC_TYPE
#define TEST_FUNC_TYPE          FUNC_RINGQUEUE_MULTI_TEST
#endif

/// 是否显示 push 次数, pop 次数 和 rdtsc计数 等额外的测试信息
#define DISPLAY_EXTRA_RESULT    0

///
/// 在spin_mutex里是否使用spin_counter计数, 0为不使用(更快!建议设为该值), 1为使用
///
#define USE_SPIN_MUTEX_COUNTER  0

///
/// spin_mutex的最大spin_count值, 默认值为16, 建议设为0或1,2, 更快! 设为0则跟USE_SPIN_MUTEX_COUNTER设为0等价
///
#define MUTEX_MAX_SPIN_COUNT    1

#define SPIN_YIELD_THRESHOLD    1

////////////////////////////////////////////////////////////////////////////////

/// 缓存的CacheLineSize(x86上是64字节)
#define CACHE_LINE_SIZE         64

#ifdef __cplusplus
extern "C" {
#endif

struct msg_t
{
    uint64_t dummy;
};

typedef struct msg_t msg_t;

struct spin_mutex_t
{
    volatile char padding1[CACHE_LINE_SIZE];
    volatile uint32_t locked;
    volatile char padding2[CACHE_LINE_SIZE - 1 * sizeof(uint32_t)];
    volatile uint32_t spin_counter;
    volatile uint32_t recurse_counter;
    volatile uint32_t thread_id;
    volatile uint32_t reserve;
    volatile char padding3[CACHE_LINE_SIZE - 4 * sizeof(uint32_t)];
};

typedef struct spin_mutex_t spin_mutex_t;

#ifdef __cplusplus
}
#endif

#endif  /* _RINGQUEUE_TEST_H_ */
