#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

typedef struct mp_pool_s mp_pool_t;

void *kvs_malloc(mp_pool_t *pool, size_t size);
void kvs_free(mp_pool_t *pool, void *ptr);

size_t mm_pool_size(void);


#endif
