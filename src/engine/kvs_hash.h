#ifndef KVS_HASH__H
#define KVS_HASH__H
#include "../include/kvstore.h"
#include "../include/memory.h"


#define MAX_KEY_LEN	128
#define MAX_VALUE_LEN	512
#define MAX_TABLE_SIZE	1024



typedef struct hashnode_s {
	kvs_blob_t key;
	kvs_blob_t value;
	struct hashnode_s *next;
	
} hashnode_t;


typedef struct hashtable_s {
	hashnode_t **nodes; //* change **, 

	int max_slots;
	int count;
	mp_pool_t *pool;
} hashtable_t;

typedef struct hashtable_s kvs_hash_t;


kvs_hash_t* kvs_hash_create(void);
void kvs_hash_destory(kvs_hash_t *hash);
int kvs_hash_set(kvs_hash_t *hash, kvs_blob_t *key, kvs_blob_t *value);
kvs_blob_t* kvs_hash_get(kvs_hash_t *hash, kvs_blob_t *key);
int kvs_hash_mod(kvs_hash_t *hash, kvs_blob_t *key, kvs_blob_t *value);
int kvs_hash_del(kvs_hash_t *hash, kvs_blob_t *key);
int kvs_hash_exist(kvs_hash_t *hash, kvs_blob_t *key);


#endif

