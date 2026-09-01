#include <memory.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "kvs_array.h"

#include "../utils/Mm_pool.h"
#include "../utils/timer.h"

#include "time.h"

#define KVS_ARRAY_SIZE		1048576

// singleton
kvs_array_t global_array = {0};

static uint64_t current_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// 定时器回调函数，用于删除过期的 key
void on_key_expire(void *arg) {
    kvs_blob_t *key = (kvs_blob_t*)arg;
    uint64_t now = current_ms();
    for (int i = 0; i < global_array.total; i++) {
        if (global_array.table[i].key.len == key->len &&
            memcmp(global_array.table[i].key.data, key->data, key->len) == 0) {
            if (global_array.table[i].expire_ms != 0 && now > global_array.table[i].expire_ms) {
                kvs_array_del(&global_array, key);
            }
            break;
        }
    }
    free(key->data);
    free(key);
}


kvs_array_t* kvs_array_create(void) 
{	
	/*
		create a new kvs_array_t
	*/
	kvs_array_t *inst = malloc(sizeof(kvs_array_t));
	if(!inst) return NULL;
	memset(inst,0,sizeof(kvs_array_t));

	/*
		create mm_pool
	*/
	inst->pool = (mp_pool_t*)malloc(mm_pool_size());
	if(!inst->pool)
	{
		free(inst);
		return NULL;
	}

	if(mp_create(inst->pool, 4096)!=0)
	{
		free(inst->pool);
		free(inst);
		return NULL;
	}

	inst->table = kvs_malloc(inst->pool, KVS_ARRAY_SIZE * sizeof(kvs_array_item_t));
	if (!inst->table) {
		mp_destory(inst->pool);
		free(inst->pool);
		free(inst);
		return NULL;
	}
	//数据清零
	memset(inst->table,0,KVS_ARRAY_SIZE*sizeof(kvs_array_item_t));
	inst->total = 0;

	return inst;
}

void kvs_array_destroy(kvs_array_t *inst) {
	if (!inst) return ;

	// 销毁内存池
	mp_destory(inst->pool);
	// 销毁内存池结构体本身
	free(inst->pool);

	free(inst);
}

/*
 * @return: <0, error; =0, success; >0, exist
 */
int kvs_array_set(kvs_array_t *inst, kvs_blob_t *key, kvs_blob_t *value) {
	if (inst == NULL || key == NULL || value == NULL) return -1;
	if(key->data == NULL || key->len <= 0) return -1;
	if(value->data == NULL || value->len <= 0) return -1;
	if (inst->total == KVS_ARRAY_SIZE) return -1;

	

	// 检查key是否已经存在
	kvs_blob_t *old_value = kvs_array_get(inst, key);
	if (old_value) {
		return 1; 
	}

	// 按真实长度分配内存
	void *kcopy = kvs_malloc(inst->pool, key->len);
	void *kvalue = kvs_malloc(inst->pool, value->len);
	if (kcopy == NULL || kvalue == NULL) return -2;
	memcpy(kcopy, key->data, key->len);
	memcpy(kvalue, value->data, value->len);

	int i = 0;
	for (i = 0; i < inst->total; i++) {
		// 查找空位置：判断data是否为空
		if (inst->table[i].key.data == NULL || inst->table[i].key.len == 0) \
		{
			inst->table[i].key.data = kcopy;
			inst->table[i].key.len = key->len;
			inst->table[i].value.data = kvalue;
			inst->table[i].value.len = value->len;
			//inst->table[i].expire_ms = expire;
			inst->total++;
			kvs_blob_t *key_copy = (kvs_blob_t*)malloc(sizeof(kvs_blob_t));
    		key_copy->data = malloc(key->len);
    		memcpy(key_copy->data, key->data, key->len);
    		key_copy->len = key->len;
			return 0;
		}
	}

	// 无空位置，追加到末尾
	if (i == inst->total && i < KVS_ARRAY_SIZE) {
		inst->table[i].key.data = kcopy;
		inst->table[i].key.len = key->len;
		inst->table[i].value.data = kvalue;
		inst->table[i].value.len = value->len;
		//inst->table[i].expire_ms = expire;
		inst->total++;
		kvs_blob_t *key_copy = (kvs_blob_t*)malloc(sizeof(kvs_blob_t));
		key_copy->data = malloc(key->len);
		memcpy(key_copy->data, key->data, key->len);
		key_copy->len = key->len;
	}

    // if(inst->total>1024)
	// {
	// 	printf("数组超标:\n");
	// }

	return 0;

}

kvs_blob_t* kvs_array_get(kvs_array_t *inst, kvs_blob_t *key) {
	if (inst == NULL || key == NULL || key->data == NULL) return NULL;

	uint64_t now = current_ms();
	int i = 0;
	for (i = 0; i < inst->total; i++) {
	
		if (inst->table[i].key.data == NULL || inst->table[i].key.len <= 0) {
			continue;
		}
		if (inst->table[i].key.len == key->len && memcmp(inst->table[i].key.data, key->data, key->len) == 0) {
			//检查key是否过期
			 if (inst->table[i].expire_ms != 0 && now > inst->table[i].expire_ms)
			 {
				kvs_array_del(inst, key);
                return NULL;
			 }
			// 返回value的地址
			return &inst->table[i].value;
		}
	}

	return NULL;
}

/*
 * @return < 0, error;  =0,  success; >0, no exist
 */
int kvs_array_del(kvs_array_t *inst, kvs_blob_t *key) {
	if (inst == NULL || key == NULL || key->data == NULL) return -1;


	int i = 0;
	//printf("key:%s\n",key->data);
	for (i = 0; i < KVS_ARRAY_SIZE; i++) {
		// 跳过空位置
		if (inst->table[i].key.data == NULL || inst->table[i].key.len <= 0) {
			continue;
		}
		// 匹配key
		if (inst->table[i].key.len == key->len && 
			memcmp(inst->table[i].key.data, key->data, inst->table[i].key.len) == 0) {

			// 只释放data，不释放结构体
			kvs_free(inst->pool, inst->table[i].key.data);
			inst->table[i].key.data = NULL;
			inst->table[i].key.len = 0;

			kvs_free(inst->pool, inst->table[i].value.data);
			inst->table[i].value.data = NULL;
			inst->table[i].value.len = 0;

			inst->total--;
			return 0;
		}
	}

	return i;
}

/*
 * @return : < 0, error; =0, success; >0, no exist 
 */
int kvs_array_mod(kvs_array_t *inst, kvs_blob_t *key, kvs_blob_t *value) {
	if (inst == NULL || key == NULL || value == NULL) return -1;
	if (key->data == NULL || value->data == NULL) return -1;
	if (inst->total == 0) return KVS_ARRAY_SIZE;


	int i = 0;
	for (i = 0; i < inst->total; i++) {
		if (inst->table[i].key.data == NULL || inst->table[i].key.len <= 0) {
			continue;
		}

		if (inst->table[i].key.len == key->len && memcmp(inst->table[i].key.data, key->data, key->len) == 0) {
			
			// 释放旧value
			kvs_free(inst->pool, inst->table[i].value.data);

			// 分配新value内存
			void *kvalue = kvs_malloc(inst->pool, value->len);
			if (kvalue == NULL) return -2;
			memcpy(kvalue, value->data, value->len);

			// 更新value
			inst->table[i].value.data = kvalue;
			inst->table[i].value.len = value->len;

			inst->table[i].expire_ms = 0;

			return 0;
		}
	}

	return i;
}

/*
 * @return 0: exist, 1: no exist
 */
int kvs_array_exist(kvs_array_t *inst, kvs_blob_t *key) {
	if (!inst || !key) return -1;
	
	kvs_blob_t *str = kvs_array_get(inst, key);
	if (!str) {
		return 1; 
	}
	return 0;
}

void kvs_array_foreach(kvs_array_t *inst, kv_callback cb,void *arg)
{
	if(!inst || !cb)
	{
		return;
	}

	for(int i = 0;i<inst->total;i++)
	{
		kvs_array_item_t *item = &inst->table[i];
		if(item->key.len == 0)
		{
			continue;
		}

		cb(arg,&item->key,&item->value,item->expire_ms);
	}


}