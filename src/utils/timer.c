#include "timer.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

// 定时器节点（内部堆节点）
typedef struct timer_node {
    uint64_t expire;
    void (*callback)(void*);
    void *arg;
    uint64_t id;
} timer_node_t;

// 全局堆数组（动态扩容）
static timer_node_t **heap = NULL;   // 指针数组，每个元素指向 timer_node_t
static int heap_capacity = 0;        // 当前数组容量
static int heap_size = 0;            // 当前堆中元素个数
static uint64_t next_id = 1;         // 用于生成唯一 id

// 获取当前系统毫秒数（使用 CLOCK_MONOTONIC 避免系统时间调整影响）
static uint64_t current_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// 交换堆中两个元素
static void swap_node(int i, int j) {
    timer_node_t *tmp = heap[i];
    heap[i] = heap[j];
    heap[j] = tmp;
}

// 上浮（新插入元素时使用）
static void heap_up(int k) {
    while (k > 1 && heap[k]->expire < heap[k/2]->expire) {
        swap_node(k, k/2);
        k /= 2;
    }
}

// 下沉（删除堆顶时使用）
static void heap_down(int k, int n) {
    while (k * 2 <= n) {
        int j = k * 2;
        if (j < n && heap[j+1]->expire < heap[j]->expire)
            j++;
        if (heap[k]->expire <= heap[j]->expire)
            break;
        swap_node(k, j);
        k = j;
    }
}

// 初始化定时器
void timer_init(void) {
    heap_capacity = 16;
    heap = (timer_node_t**)malloc(sizeof(timer_node_t*) * (heap_capacity + 1)); // 下标从1开始
    heap_size = 0;
}

// 添加定时器
uint64_t timer_add(uint64_t timeout_ms, void (*cb)(void*), void *arg) {
    if (!heap) timer_init();

    uint64_t expire = current_ms() + timeout_ms;
    timer_node_t *node = (timer_node_t*)malloc(sizeof(timer_node_t));
    node->expire = expire;
    node->callback = cb;
    node->arg = arg;
    node->id = next_id++;

    // 扩容
    if (heap_size + 1 > heap_capacity) {
        heap_capacity *= 2;
        heap = (timer_node_t**)realloc(heap, sizeof(timer_node_t*) * (heap_capacity + 1));
    }
    // 放入堆尾
    heap[++heap_size] = node;
    heap_up(heap_size);
    return node->id;
}

// 处理已到期的定时器（由主循环调用）
void timer_process(void) {
    uint64_t now = current_ms();
    while (heap_size > 0 && heap[1]->expire <= now) {
        timer_node_t *node = heap[1];
        // 弹出堆顶
        heap[1] = heap[heap_size--];
        heap_down(1, heap_size);
        // 执行回调
        if (node->callback)
            node->callback(node->arg);
        free(node);
    }
}

// 获取 epoll_wait 应等待的超时时间（毫秒）
// 返回 -1 表示没有定时器，应无限等待
int timer_get_timeout(void) {
    if (heap_size == 0) return -1;
    uint64_t now = current_ms();
    if (heap[1]->expire <= now) return 0;  // 已经到期，不等待
    uint64_t diff = heap[1]->expire - now;
    // 防止溢出，最大不超过 1000 毫秒（也可以更大，但建议设上限）
    if (diff > 1000) diff = 1000;
    return (int)diff;
}

// 销毁定时器，释放所有内存
void timer_destroy(void) {
    for (int i = 1; i <= heap_size; i++) {
        free(heap[i]);
    }
    free(heap);
    heap = NULL;
    heap_size = heap_capacity = 0;
}