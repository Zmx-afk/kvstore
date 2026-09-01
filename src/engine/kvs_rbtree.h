#ifndef KVS_RBTREE_H
#define KVS_RBTREE_H

#include "../include/kvstore.h"
#include "../include/memory.h"

//RBTREE

#define RED				1
#define BLACK 			2


typedef struct _rbtree_node {
	unsigned char color;
	struct _rbtree_node *right;
	struct _rbtree_node *left;
	struct _rbtree_node *parent;
	kvs_blob_t key;
	kvs_blob_t value;
} rbtree_node;

typedef struct _rbtree {
	rbtree_node *root;
	rbtree_node *nil;
	mp_pool_t *pool;
} rbtree;


typedef struct _rbtree kvs_rbtree_t;

kvs_rbtree_t* kvs_rbtree_create(void);
void kvs_rbtree_destory(kvs_rbtree_t *inst);
int kvs_rbtree_set(kvs_rbtree_t *inst,kvs_blob_t *key, kvs_blob_t *value);
kvs_blob_t* kvs_rbtree_get(kvs_rbtree_t *inst, kvs_blob_t *key);
int kvs_rbtree_del(kvs_rbtree_t *inst,kvs_blob_t *key);
int kvs_rbtree_mod(kvs_rbtree_t *inst, kvs_blob_t *key, kvs_blob_t *value);
int kvs_rbtree_exist(kvs_rbtree_t *inst,kvs_blob_t *key);




#endif