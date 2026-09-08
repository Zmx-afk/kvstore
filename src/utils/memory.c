#include "../include/memory.h"
#include "Mm_pool.h"
#include <stdlib.h>

void *kvs_malloc(mp_pool_t *pool, size_t size) {
    //return malloc(size);
    return mp_alloc(pool, size);
}

void kvs_free(mp_pool_t *pool, void *ptr) {
    return free(ptr);
    return mp_free(pool, ptr);
}