#ifndef KVS_ARRAY__H
#define KVS_ARRAY__H

#include "../include/kvstore.h"
#include "../include/memory.h"
#include "../utils/log.h"



/*
	expire_ms --> how much time if left before timeout
	table --> the head of array body
	idx --> the alloced array index
	total --> how many items
*/

typedef void (*kv_callback)(void *arg,kvs_blob_t *key,kvs_blob_t *val,uint64_t expire_ms);

typedef struct kvs_array_item_s {
	kvs_blob_t key;
	kvs_blob_t value;
	uint64_t expire_ms;
} kvs_array_item_t;



typedef struct kvs_array_s 
{
	kvs_array_item_t *table;
	int total;
	mp_pool_t *pool;
} kvs_array_t;

kvs_array_t* kvs_array_create(void);
void kvs_array_destroy(kvs_array_t *inst);

int kvs_array_set(kvs_array_t *inst, kvs_blob_t *key, kvs_blob_t *value);
kvs_blob_t* kvs_array_get(kvs_array_t *inst,kvs_blob_t *key);
int kvs_array_del(kvs_array_t *inst, kvs_blob_t *key);
int kvs_array_mod(kvs_array_t *inst, kvs_blob_t *key, kvs_blob_t *value);
int kvs_array_exist(kvs_array_t *inst, kvs_blob_t *key);


void kvs_array_foreach(kvs_array_t *inst, kv_callback cb, void *arg);

#endif