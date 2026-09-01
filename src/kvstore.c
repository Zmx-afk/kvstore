


#include "include/kvstore.h"
#include "include/protocol.h"
#include "network/server.h"
#include "utils/timer.h"

#include "engine/kvs_array.h"
#include "engine/kvs_rbtree.h"
#include "engine/kvs_hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>

#include "include/config.h"


#include "persist/persistence.h"

// extern ServerConfig g_config;

static int g_epfd = -1;   // 供定时器使用


/*
    this struct and these function is used to load config and choose engine
    at the same time set the corresponding function for the engine

*/


kvs_engine_t g_engine = {0};

static int array_set_wrap(void *inst,kvs_blob_t *k,kvs_blob_t *v)
{
    return kvs_array_set((kvs_array_t*)inst, k, v);
}

static kvs_blob_t* array_get_wrap(void *inst, kvs_blob_t *k) {
    return kvs_array_get((kvs_array_t*)inst, k);
}

static int array_del_wrap(void *inst, kvs_blob_t *k) {
    return kvs_array_del((kvs_array_t*)inst, k);
}

static int array_mod_wrap(void *inst,kvs_blob_t *k,kvs_blob_t *v)
{
    return kvs_array_mod((kvs_array_t*)inst, k, v);
}

static int array_exist_wrap(void *inst, kvs_blob_t *k) {
    return kvs_array_exist((kvs_array_t*)inst, k);
}

static void* array_create_wrap(void){
    return (void*)kvs_array_create();
}

static void array_destroy_wrap(void *inst){
    kvs_array_destroy((kvs_array_t*)inst);
}


static int rbtree_set_wrap(void *inst,kvs_blob_t *k,kvs_blob_t *v)
{
    return kvs_rbtree_set((kvs_rbtree_t*)inst, k, v);
}

static kvs_blob_t* rbtree_get_wrap(void *inst, kvs_blob_t *k) {
    return kvs_rbtree_get((kvs_rbtree_t*)inst, k);
}

static int rbtree_del_wrap(void *inst, kvs_blob_t *k) {
    return kvs_rbtree_del((kvs_rbtree_t*)inst, k);
}

static int rbtree_mod_wrap(void *inst,kvs_blob_t *k,kvs_blob_t *v)
{
    return kvs_rbtree_mod((kvs_rbtree_t*)inst, k, v);
}

static int rbtree_exist_wrap(void *inst, kvs_blob_t *k) {
    return kvs_rbtree_exist((kvs_rbtree_t*)inst, k);
}

static void* rbtree_create_wrap(void){
    return (void*)kvs_rbtree_create();
}

static void rbtree_destroy_wrap(void *inst){
    return kvs_rbtree_destory((kvs_rbtree_t*)inst);
}


static int hash_set_wrap(void *inst,kvs_blob_t *k,kvs_blob_t *v)
{
    return kvs_hash_set((kvs_hash_t*)inst, k, v);
}

static kvs_blob_t* hash_get_wrap(void *inst, kvs_blob_t *k) {
    return kvs_hash_get((kvs_hash_t*)inst, k);
}

static int hash_del_wrap(void *inst, kvs_blob_t *k) {
    return kvs_hash_del((kvs_hash_t*)inst, k);
}

static int hash_mod_wrap(void *inst,kvs_blob_t *k,kvs_blob_t *v)
{
    return kvs_hash_mod((kvs_hash_t*)inst, k, v);
}

static int hash_exist_wrap(void *inst, kvs_blob_t *k) {
    return kvs_hash_exist((kvs_hash_t*)inst, k);
}

static void* hash_create_wrap(void){
    return (void*)kvs_hash_create();
}

static void hash_destroy_wrap(void *inst){
    return kvs_hash_destory((kvs_hash_t*)inst);
}

/*
    compatible with corresponding operations
*/

static int kvs_set(kvs_blob_t *key,kvs_blob_t* val)
{   
    return g_engine.set(g_engine.impl,key,val);
}

static kvs_blob_t* kvs_get(kvs_blob_t *key)
{   
    return g_engine.get(g_engine.impl,key);
}

static int kvs_mod(kvs_blob_t *key,kvs_blob_t* val)
{   
    return g_engine.mod(g_engine.impl,key,val);
}

static int kvs_del(kvs_blob_t *key)
{   
    return g_engine.del(g_engine.impl,key);
}

static int kvs_exist(kvs_blob_t *key)
{   
    return g_engine.exist(g_engine.impl,key);
}

/*
    init the corresponding enging
    destroy engine
*/

int init_kvengine(void) {
    if(!strcmp(g_config.engine,"array"))
    {
        g_engine.set = array_set_wrap;
        g_engine.get = array_get_wrap;
        g_engine.del = array_del_wrap;
        g_engine.mod = array_mod_wrap;
        g_engine.exist = array_exist_wrap;
        g_engine.create = array_create_wrap;
        g_engine.destroy = array_destroy_wrap;
        printf("array\n");
    }
    else if(!strcmp(g_config.engine,"rbtree"))
    {
        g_engine.set = rbtree_set_wrap;
        g_engine.get = rbtree_get_wrap;
        g_engine.del = rbtree_del_wrap;
        g_engine.mod = rbtree_mod_wrap;
        g_engine.exist = rbtree_exist_wrap;
        g_engine.create = rbtree_create_wrap;
        g_engine.destroy = rbtree_destroy_wrap;
        printf("rbtree\n");
    }
    else if(!strcmp(g_config.engine,"hash"))
    {
        g_engine.set = hash_set_wrap;
        g_engine.get = hash_get_wrap;
        g_engine.del = hash_del_wrap;
        g_engine.mod = hash_mod_wrap;
        g_engine.exist = hash_exist_wrap;
        g_engine.create = hash_create_wrap;
        g_engine.destroy = hash_destroy_wrap;
        printf("hash\n");
    }
    else 
    {
        printf("未读取到使用的引擎:%s 默认使用数组\n",g_config.engine);
        //strcpy(g_config.engine,"array");
        g_engine.set = array_set_wrap;
        g_engine.get = array_get_wrap;
        g_engine.del = array_del_wrap;
        g_engine.mod = array_mod_wrap;
        g_engine.exist = array_exist_wrap;
        g_engine.create = array_create_wrap;
        g_engine.destroy = array_destroy_wrap;
    }
    g_engine.impl = g_engine.create();
    return g_engine.impl ? 0:-1;
}

void dest_kvengine(void)
{
    g_engine.destroy(g_engine.impl);
}


#define MAX_ARGV 10

typedef struct {
    int argc;
    char* argv[MAX_ARGV];
}Command;

int g_port = 2000;
NodeRole g_role = ROLE_MASTER;       // 默认是主节点
int g_master_fd = -1;                // 从节点 → 连接主节点的socket
int g_slave_fd = -1;                 // 主节点 → 保存从节点的socket
char g_master_ip[64] = "127.0.0.1";  
int g_master_port = 2000;
int g_is_sync= 0;
int g_aof_enabled = 0;





int is_write_cmd(int cmd) {
    switch(cmd) {
        case PROTO_CMD_SET:case PROTO_CMD_MOD:case PROTO_CMD_DEL:
            return 1; // 写命令：需要同步
        default: return 0; // 读命令：不同步
    }
}



/*
    resp协议
*/

int kvs_filter_protocol(char *msg, int length, char *response) {
    if (msg == NULL || length < PROTO_MIN_LEN || response == NULL) return -1;

    kvs_blob_t cmd_blob = {0};
    kvs_blob_t key_blob = {0};
    kvs_blob_t val_blob = {0};
    
    server_decode_resp(msg,&cmd_blob,&key_blob,&val_blob);

    int ret_len = 0;
	int ret = 0;


    int admin_cmd = -1;
    //第一步优先匹配管理命令
    for(int i=0;i<ADMIN_CMD_COUNT;i++)
    {
        if(strlen(admin_command[i])==cmd_blob.len&&strncmp(cmd_blob.data, admin_command[i], cmd_blob.len)==0)
        {
            admin_cmd = i;
            break;
        }
    }

    if(admin_cmd != -1)
    {
        //处理管理指令
        switch(admin_cmd)
        {
            case ADMIN_CMD_SAVE:
            {
                RDB();
            }
            default:
                ret_len = sprintf(response,"-ERR unknown admin cmd\r\n");
        }

    }
    else
    {
        //用整形代表cmd
        int cmd;
        for (cmd = PROTO_CMD_SET; cmd < PROTO_CMD_COUNT; cmd++) {
            if (strlen(kv_command[cmd]) == cmd_blob.len &&
                strncmp(cmd_blob.data, kv_command[cmd], cmd_blob.len) == 0) {
                break;
            }
        }

        switch (cmd) 
        {
            case PROTO_CMD_SET:
                ret = kvs_set(&key_blob, &val_blob);
                if (ret < 0) {
                    ret_len = sprintf(response, "-ERR internal error\r\n");
                } else if (ret == 0) {
                    ret_len = sprintf(response, "+OK\r\n");
                } else {
                    ret_len = sprintf(response, ":0\r\n");
                }
                break;

            case PROTO_CMD_GET: {
                kvs_blob_t *val = kvs_get(&key_blob);
                if (val == NULL)
                    ret_len = sprintf(response, "$-1\r\n");
                else
                    // %.*s ：按长度打印，不怕\0截断
                    ret_len = sprintf(response, "%.*s\r\n", val->len, (char*)val->data);
                break;
                }

            case PROTO_CMD_DEL:
                ret = kvs_del(&key_blob);
                if (ret == 0)
                    ret_len = sprintf(response, ":1\r\n");
                else
                    ret_len = sprintf(response, ":0\r\n");
                break;

            case PROTO_CMD_MOD:
                ret = kvs_mod(&key_blob, &val_blob);
                if (ret == 0)
                    ret_len = sprintf(response, "+OK\r\n");
                else
                    ret_len = sprintf(response, "-ERR key not exist\r\n");
                break;

            case PROTO_CMD_EXIST:
                ret = kvs_exist(&key_blob);
                if (ret == 0)
                    ret_len = sprintf(response, ":1\r\n");
                else
                    ret_len = sprintf(response, ":0\r\n");
                break;
        default: 
            ret_len = sprintf(response, "-ERR unknown command\r\n");
            break;
            //assert(0);
	    }

    }


  
    // if (cmd == PROTO_CMD_COUNT) {
    //     ret_len = sprintf(response, "-ERR unknown command\r\n");
    //     return ret_len;
    // }

    

	
    // if (g_role == ROLE_SLAVE && is_write_cmd(cmd)&&!g_is_sync) {
    //     ret_len = sprintf(response, "READONLY: cannot write to slave\r\n");
    //     return ret_len;
    // }

    // printf("val:%s,val_len:%d\n",val_blob.data,val_blob.len);
/*
    RESP协议
    简单字符串前加+
    错误前加-
    整数前加:
*/

	


#if 0
    //持久化
    char aof_buf[BUFFER_LENGTH] ={0};
    char key_buf[BUFFER_LENGTH] = {0};
    memcpy(key_buf, key_data, key_len);

    if (val_data != NULL) 
    {
        char val_buf[BUFFER_LENGTH] = {0};
        memcpy(val_buf, val_data, val_len);
        sprintf(aof_buf, "%s %s %s", cmd_buf, key_buf, val_buf);
    } 
    else
    {
        sprintf(aof_buf, "%s %s", cmd_buf, key_buf);
    }
    printf("aof_buf:%s\n", aof_buf);
    AOF(aof_buf);

    int total_cmd_len = 4 + cmd_len + 4 + key_len;
    if(val_data != NULL) {
        total_cmd_len += 4 + val_len;
    }
    // 主节点执行写命令，同步到从节点
    if (g_role == ROLE_MASTER && is_write_cmd(cmd)) {
        master_sync(msg, total_cmd_len);
    }
#endif

	return ret_len;




}



/*
 * msg: request message
 * length: length of request message
 * response: need to send
 * @return : length of response
 */

int kvs_protocol(char *msg, int length, char *response) 
{ 
	if (msg == NULL || length <= 0 || response == NULL) return -1;
    
    return kvs_filter_protocol(msg,length, response);
}





