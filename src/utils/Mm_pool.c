#include <memory.h>
#include <stddef.h>
#include <stdlib.h>
#include "Mm_pool.h"

// ======================== 模式开关 =========================
// 0 -> 使用内存池（原有逻辑）
// 1 -> 直接透传 malloc/free（用于对比测试）
#define MM_USE_SYSTEM_MALLOC 1 
// ===========================================================

int mp_create(mp_pool_t *pool, size_t size);
void mp_destory(mp_pool_t *pool);
void *mp_alloc(mp_pool_t *pool, size_t size);
void mp_free(mp_pool_t *pool, void *ptr);


size_t mm_pool_size(void)
{
	return sizeof(mp_pool_t);
}


/* size : 4096
	return --> 0:success -1:failed
*/
int mp_create(mp_pool_t *pool, size_t size) {
	if (!pool || size <= 0) return -1;

#if MM_USE_SYSTEM_MALLOC
	// 透传模式：只做个样子，不真的分配大块内存
	// 但为了兼容后续代码，仍设置 head = NULL，max = size
	pool->head = NULL;
	pool->max = size;
	pool->large = NULL;
	return 0;
#else
	// 原有内存池逻辑
	void *mem = malloc(size);
	if(!mem) return -1;

	struct mp_node_s *node = (struct mp_node_s *)mem;
	node->last = (unsigned char *)mem + sizeof(struct mp_node_s);
	node->end = (unsigned char *)mem + size;
	node->next = NULL;

	pool->head = node;
	pool->max = size;
	pool->large = NULL;
	return 0;
#endif
}

void mp_destory(mp_pool_t *pool) {
	if(!pool) return;

#if MM_USE_SYSTEM_MALLOC
	// 透传模式：什么也不用做，因为内存都是直接 malloc/free 的，
	// 不归池子管理，此处留空即可（避免二次释放）
	return;
#else
	// 原有销毁逻辑：释放所有 large 块和所有节点块
	mp_large_t *l;
	for (l = pool->large; l; l = l->next) {
		if (l->alloc) {
			free(l->alloc);
		}
	}
	pool->large = NULL;

	mp_node_t *node = pool->head;
	while (node) {
		mp_node_t *tmp = node->next;
		free(node);
		node = tmp;
	}
#endif
}


// 内部函数：分配新的大块（只在池模式下使用）
static void *mp_alloc_block(mp_pool_t *pool, size_t size) {
#if MM_USE_SYSTEM_MALLOC
	// 透传模式下不应调用此函数，但为了安全，直接返回 NULL 或调用 malloc
	// 由于透传模式走的是 mp_alloc 中的直接 malloc，这里不会进入
	return malloc(size);
#else
	void *mem = malloc(pool->max);
	struct mp_node_s *node = (struct mp_node_s *)mem;
	node->last = (unsigned char *)mem + sizeof(struct mp_node_s);
	node->end = (unsigned char *)mem + pool->max;
	node->next = NULL;
	
	void *ptr = node->last;
	node->last += size;

	mp_node_t *iter = pool->head;
	while (iter->next != NULL) {
		iter = iter->next;
	}
	iter->next = node;

	return ptr;
#endif
}

// 内部函数：分配大块（仅池模式使用）
static void *mp_alloc_large(mp_pool_t *pool, size_t size) {
#if MM_USE_SYSTEM_MALLOC
	// 透传模式直接 malloc
	return malloc(size);
#else
	if (!pool) return NULL;

	void *ptr = malloc(size);
	if (ptr == NULL) return NULL;

	mp_large_t *l;
	for (l = pool->large; l; l = l->next) {
		if (l->alloc == NULL) {
			l->alloc = ptr;
			return ptr;
		}
	}

	l = mp_alloc(pool, sizeof(mp_large_t));
	if (l == NULL) {
		free(ptr);
		return NULL;
	}
	l->alloc = ptr;
	l->next = pool->large;
	pool->large = l;
	
	return ptr;
#endif
}


void *mp_alloc(mp_pool_t *pool, size_t size) {
	if (!pool || size == 0) return NULL;

#if MM_USE_SYSTEM_MALLOC
	// 透传模式：直接调用 malloc，忽略 pool
	return malloc(size);
#else
	// 原内存池逻辑
	if (size > pool->max) {
		return mp_alloc_large(pool, size);
	} 

	void *ptr = NULL;
	mp_node_t *node = pool->head;

	do {
		if (node->end - node->last > size) {
			ptr = node->last;
			node->last += size;
			return ptr;
		} 
		node = node->next;
	} while (node);

	return mp_alloc_block(pool, size);
#endif
}


void mp_free(mp_pool_t *pool, void *ptr) {
	if (!ptr) return;

#if MM_USE_SYSTEM_MALLOC
	// 透传模式：直接 free
	free(ptr);
#else
	// 原内存池：只释放 large 块，小块不回收（仅置标志）
	mp_large_t *l;
	for (l = pool->large; l; l = l->next) {
		if (l->alloc == ptr) {
			free(l->alloc);
			l->alloc = NULL;
			return;
		}
	}
	// 小块无法回收（设计如此）
#endif
}