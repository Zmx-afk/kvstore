#ifndef MM_POOL_H
#define MM_POOL_H

#include <stddef.h>

//mmpool
typedef struct mp_node_s {
	unsigned char *last;
	unsigned char *end;
	struct mp_node_s *next;
} mp_node_t;

typedef struct mp_large_s {
	struct mp_large_s *next;
	void *alloc;
} mp_large_t;


typedef struct mp_pool_s {
	size_t max; //
	struct mp_node_s *head;
	struct mp_large_s *large;
} mp_pool_t;

int mp_create(mp_pool_t *pool, size_t size);
void mp_destory(mp_pool_t *pool);
void *mp_alloc(mp_pool_t *pool, size_t size);
void mp_free(mp_pool_t *pool, void *ptr);





#endif