#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "kvs_hash.h"
#include "../utils/Mm_pool.h"

kvs_hash_t global_hash;

// 二进制哈希函数
static int _hash(kvs_blob_t *key, int size) {
	int sum = 0;
	for (int i = 0; i < key->len; i++) {
		sum += ((unsigned char*)key->data)[i];
	}
	return sum % size;
}

hashnode_t *_create_node(kvs_hash_t *hash, kvs_blob_t *key, kvs_blob_t *value) {
	hashnode_t *node = (hashnode_t*)kvs_malloc(hash->pool, sizeof(hashnode_t));
	if (!node) return NULL;

	// 复制 key
	node->key.data = kvs_malloc(hash->pool, key->len);
	node->key.len = key->len;
	memcpy(node->key.data, key->data, key->len);

	// 复制 value
	node->value.data = kvs_malloc(hash->pool, value->len);
	node->value.len = value->len;
	memcpy(node->value.data, value->data, value->len);

	node->next = NULL;
	return node;
}

// 创建哈希表
kvs_hash_t* kvs_hash_create(void) {
	kvs_hash_t* hash = malloc(sizeof(kvs_hash_t));
	if(!hash) return NULL;
	memset(hash,0,sizeof(kvs_hash_t));

	hash->pool = (mp_pool_t*)malloc(mm_pool_size());
	if (!hash->pool) return NULL;
	mp_create(hash->pool, 4096);

	hash->nodes = (hashnode_t**)kvs_malloc(hash->pool, sizeof(hashnode_t*) * MAX_TABLE_SIZE);
	if (!hash->nodes) return NULL;

	hash->max_slots = MAX_TABLE_SIZE;
	hash->count = 0;
	return hash;
}

// 销毁
void kvs_hash_destory(kvs_hash_t *hash) {
	if (!hash) return;
	mp_destory(hash->pool);
	free(hash->pool);
}

int kvs_hash_set(kvs_hash_t *hash, kvs_blob_t *key, kvs_blob_t *value) {
	if (!hash || !key || !value) return -1;

	int idx = _hash(key, MAX_TABLE_SIZE);
	hashnode_t *node = hash->nodes[idx];

	while (node != NULL) {
		if (memcmp(node->key.data, key->data, key->len) == 0) {
			return 1;
		}
		node = node->next;
	}

	hashnode_t *new_node = _create_node(hash, key, value);
	new_node->next = hash->nodes[idx];
	hash->nodes[idx] = new_node;
	hash->count++;
	return 0;
}

kvs_blob_t* kvs_hash_get(kvs_hash_t *hash, kvs_blob_t *key) {
	if (!hash || !key) return NULL;

	int idx = _hash(key, MAX_TABLE_SIZE);
	hashnode_t *node = hash->nodes[idx];

	while (node != NULL) {
		if (memcmp(node->key.data, key->data, key->len) == 0) {
			return &node->value;
		}
		node = node->next;
	}
	return NULL;
}

int kvs_hash_mod(kvs_hash_t *hash, kvs_blob_t *key, kvs_blob_t *value) {
	if (!hash || !key || !value) return -1;

	int idx = _hash(key, MAX_TABLE_SIZE);
	hashnode_t *node = hash->nodes[idx];

	while (node != NULL) {
		if (memcmp(node->key.data, key->data, key->len) == 0) break;
		node = node->next;
	}

	if (!node) return 1;

	// 释放旧 value
	kvs_free(hash->pool, node->value.data);

	// 新 value
	node->value.data = kvs_malloc(hash->pool, value->len);
	node->value.len = value->len;
	memcpy(node->value.data, value->data, value->len);

	return 0;
}

int kvs_hash_del(kvs_hash_t *hash, kvs_blob_t *key) {
	if (!hash || !key) return -1;

	int idx = _hash(key, MAX_TABLE_SIZE);
	hashnode_t *head = hash->nodes[idx];
	if (!head) return 1;

	// 删除头节点
	if (memcmp(head->key.data, key->data, key->len) == 0) {
		hash->nodes[idx] = head->next;
		kvs_free(hash->pool, head->key.data);
		kvs_free(hash->pool, head->value.data);
		kvs_free(hash->pool, head);
		hash->count--;
		return 0;
	}

	hashnode_t *cur = head;
	while (cur->next) {
		if (memcmp(cur->next->key.data, key->data, key->len) == 0) break;
		cur = cur->next;
	}

	if (!cur->next) return 1;

	hashnode_t *tmp = cur->next;
	cur->next = tmp->next;
	kvs_free(hash->pool, tmp->key.data);
	kvs_free(hash->pool, tmp->value.data);
	kvs_free(hash->pool, tmp);
	hash->count--;
	return 0;
}

// EXIST
int kvs_hash_exist(kvs_hash_t *hash, kvs_blob_t *key) {
	return kvs_hash_get(hash, key) ? 0 : 1;
}