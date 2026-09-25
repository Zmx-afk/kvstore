
#include "engine.h"
#include "../include/memory.h"
#include "../utils/log.h"
#include <stdint.h>
#include <string.h>


engine_t* engine_create(void)
{
    /*分配哈希表本身*/
    engine_t *h = kvs_malloc(sizeof(engine_t));
    if(!h)
    {
        LOG_ERROR("哈希表创建失败");
        return NULL;
    }

    /*分配桶数组*/
    h->nodes = kvs_malloc(sizeof(engine_node_t*) * ENGINE_INIT_SLOTS);
    if(!h->nodes)
    {
        kvs_free(h);
        LOG_ERROR("哈希节点分配失败");
        return NULL;
    }

    /*清零桶数组*/
    memset(h->nodes,0, sizeof(engine_node_t*) * ENGINE_INIT_SLOTS);

    /*初始化哈希表信息*/
    h->slots = ENGINE_INIT_SLOTS;
    h->total = 0;

    return h;
}


void engine_destroy(engine_t *h)
{
    if(!h) return;

    /*遍历每个桶并释放桶内所有节点*/
    for(int i = 0;i<h->slots;i++)
    {
        engine_node_t *node = h->nodes[i];
        while(node)
        {
            engine_node_t *next = node->next;

            kvs_free(node->key.data);
            kvs_free(node->value.data);
            kvs_free(node);
            node = next;
        }
    }

    /*释放桶数组*/
    kvs_free(h->nodes);

    /*释放哈希表本身*/
    kvs_free(h);

}

/* DJB2 哈希：把任意字节序列混合成 32 位整数 */
static uint32_t djb2(const char *data, int len) {
    uint32_t engine = 5381;
    for (int i = 0; i < len; i++) {
        /* engine * 33 + data[i] 的位运算版 */
        engine = ((engine << 5) + engine) + (unsigned char)data[i];
    }
    return engine;
}

/* FNV-1a */
static uint32_t fnv1a(const char *data, int len) {
    uint32_t engine = 2166136261u;
    for (int i = 0; i < len; i++) {
        engine ^= (unsigned char)data[i];
        engine *= 16777619u;
    }
    return engine;
}

/*查找哈希节点: 由调用方传入桶下标和key的哈希值 这样就不用二次计算 
  其次判断过程需确保:engine值一致 键长度和键数值一致 加engine比较是因为可以快速判否*/
static engine_node_t* find_node(engine_t *h,kvs_blob_t *key,uint32_t hash,int idx)
{
    engine_node_t *node = h->nodes[idx];
    while(node)
    {
        if(node->hash == hash && node->key.len == key->len && memcmp(node->key.data,key->data,key->len)==0)
        {
            return node;
        }
        node = node->next;
    }
    return NULL;

}

kvs_blob_t* engine_get(engine_t *h,kvs_blob_t *key)
{
    /*参数检查*/
    if(!h||!key||!key->data||key->len<=0)
    {
        return NULL;
    }

    /*计算engine和桶下标*/
    uint32_t hash = fnv1a(key->data,key->len);
    int idx = hash & (h->slots -1);

    /*查找*/
    engine_node_t *node = find_node(h, key,hash,idx);
    if(!node)
    {
        return NULL;
    }

    /*返回value地址*/
    return &node->value;
}


/*engine_set: return : 1->exist 0->success -1->failed*/
int engine_set(engine_t *h,kvs_blob_t* key,kvs_blob_t* value)
{
    /*参数检查*/
    if(!h||!key||!value)
    {
        return -1;
    }
    if(!key->data||!value->data)
    {
        return -1;
    }
    if(key->len<=0||value->len<=0)
    {
        return -1;
    }

    /*算hash和桶下标*/
    uint32_t hash = fnv1a(key->data,key->len);
    int idx = hash &(h->slots-1);

    /*查重*/
    if(find_node(h, key,hash, idx)!=NULL)
    {
        return 1;   /*已存在*/
    }

    /*创建新节点*/
    engine_node_t *new_node = kvs_malloc(sizeof(engine_node_t));
    if(!new_node) 
    {
        return -2;
    }

    new_node->key.data = kvs_malloc(key->len);
    if(!new_node->key.data)
    {
        kvs_free(new_node);
        return -2;
    }
    memcpy(new_node->key.data, key->data, key->len);
    new_node->key.len = key->len;
    
    new_node->value.data = kvs_malloc( value->len);
    if (!new_node->value.data) {
        kvs_free(new_node->key.data);
        kvs_free(new_node);
        return -2;
    }
    memcpy(new_node->value.data, value->data, value->len);
    new_node->value.len = value->len;

    new_node->hash = hash;
    new_node->next = NULL;

    /*头插 */
    new_node->next = h->nodes[idx];
    h->nodes[idx] = new_node;
    h->total++;

    return 0;
}

int engine_del(engine_t *h,kvs_blob_t *key)
{
    /*参数检查*/
    if(!h||!key||!key->data||key->len<=0)
    {
        return -1;
    }

    /*算hash和桶下标*/
    uint32_t hash = fnv1a(key->data,key->len);
    int idx = hash & (h->slots-1);

    /*用二级指针遍历链表 这里不复用find_node原因是需要修改此节点前驱next指针*/
    engine_node_t **pp = &h->nodes[idx];
    while (*pp) {
        engine_node_t *cur = *pp;
        if (cur->hash == hash &&cur->key.len == key->len && memcmp(cur->key.data, key->data, key->len) == 0) 
        {
            /*从链表摘除 */
            *pp = cur->next;

            /* 5. 释放内存 */
            kvs_free(cur->key.data);
            kvs_free(cur->value.data);
            kvs_free(cur);

            h->total--;
            return 0;   /* 删除成功 */
        }
        pp = &cur->next;
    }

    return 1;   /* 不存在 */

}

/*
    engine_mod return: <0 fail =0->success >0->exist
*/
int engine_mod(engine_t *h,kvs_blob_t *key,kvs_blob_t *value)
{
    /*参数判断*/
    if(!h||!key||!value)
    {
        return -1;
    }
    if(!key->data||!value->data)
    {
        return -1;
    }
    if(key->len<=0||value->len<=0)
    {
        return -1;
    }

    /*算hash和桶下标*/
    uint32_t hash = fnv1a(key->data,key->len);
    int idx = hash & (h->slots-1);

    /*查找节点*/
    engine_node_t *node = find_node(h, key,hash,idx);
    if(!node)
    {
        return 1;   /*key不存在 不修改*/
    }

    /*分配新value*/
    void *new_value = kvs_malloc(value->len);
    if(!new_value)
    {
        return -2;
    }
    memcpy(new_value,value->data,value->len);

    /*释放旧value 换成新value*/
    kvs_free(node->value.data);
    node->value.data = new_value;
    node->value.len = value->len;

    return 0;
}

int engine_exist(engine_t *h, kvs_blob_t *key) 
{
    return engine_get(h, key) ? 0 : 1;
}

int engine_foreach(engine_t *engine,kv_callback cb,void *arg)
{
    /*参数判断*/
    if(!engine||!cb)
    {
        return -1;
    }
    
    for(int i=0;i<engine->slots;i++)
    {
        engine_node_t *node = engine->nodes[i];

        while(node)
        {
            engine_node_t *next = node->next;
            cb(arg,&node->key,&node->value,node->expire_ms);
            node = next;

        }
    }

    return 0;
}