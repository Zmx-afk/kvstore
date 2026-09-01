#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

typedef struct timer_entry {
    uint64_t expire;            // 绝对到期时间（毫秒）
    void (*callback)(void *arg); // 回调函数
    void *arg;                  // 回调参数
    uint64_t id;                // 可选，用于删除
} timer_entry_t;

// 初始化定时器（需要传入事件循环的 epoll fd，但实际上这里不需要，我们只用堆）
void timer_init(void);

// 添加一个定时器，timeout_ms 为多少毫秒后触发，回调函数 cb 会被调用，参数 arg
// 返回定时器 id，可用于删除（本例暂不实现删除）
uint64_t timer_add(uint64_t timeout_ms, void (*cb)(void*), void *arg);

// 处理所有已到期的定时器，由主循环调用
void timer_process(void);

// 获取堆顶的到期时间（绝对时间），用于计算 epoll_wait 的超时
// 返回：如果没有定时器，返回 -1；否则返回距离现在还有多少毫秒（最小 0）
int timer_get_timeout(void);

// 可选：销毁定时器（释放堆内存）
void timer_destroy(void);

#endif