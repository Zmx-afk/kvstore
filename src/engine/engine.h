#ifndef ENGINE_H
#define ENGINE_H

#include "../include/kvstore.h"
#include <stdint.h>

typedef void (*kv_callback)(void *arg,kvs_blob_t *key,kvs_blob_t *val,uint64_t expire_ms);

#define ENGINE_INIT_SLOTS     1048576     /*初始桶数量 必须是2的幂*/

/*==========链表节点=============*/
typedef struct engine_node_s {
    kvs_blob_t key;                 /*键*/
    kvs_blob_t value;               /*值*/
    uint64_t expire_ms;
    uint32_t hash;                  /*缓存哈希值*/
    struct engine_node_s *next;       /*冲突链表下一跳*/
} engine_node_t;

/*==========哈希表主体===========*/
typedef struct engine_s {
    engine_node_t **nodes;            /* 桶数组，每个元素是链表头 */
    int slots;                      /* 桶数量（2 的幂） */
    int total;                      /* 当前 key 数量 */
} engine_t;


/* ========== 对外接口 ========== */
engine_t* engine_create(void);
void engine_destroy(engine_t *h);

kvs_blob_t* engine_get(engine_t *h,kvs_blob_t *key);
int engine_set(engine_t *h,kvs_blob_t* key,kvs_blob_t* value);
int engine_del(engine_t *h,kvs_blob_t *key);
int engine_mod(engine_t *h,kvs_blob_t *key,kvs_blob_t *value);
int engine_exist(engine_t *h, kvs_blob_t *key);
int engine_foreach(engine_t *engine,kv_callback cb,void *arg);








#endif