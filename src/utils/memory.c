#include "../include/memory.h"
#include "Mm_pool.h"
#include <stdlib.h>

void *kvs_malloc(size_t size) {
    return malloc(size);
}

void kvs_free(void *ptr) {
    return free(ptr);
}