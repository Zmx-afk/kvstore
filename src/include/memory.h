#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

typedef struct mp_pool_s mp_pool_t;

void *kvs_malloc(size_t size);
void kvs_free(void *ptr);

size_t mm_pool_size(void);


#endif
