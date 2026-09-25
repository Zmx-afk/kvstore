


#include "include/kvstore.h"
#include "include/protocol.h"
#include "network/server.h"
#include "utils/log.h"
#include "utils/timer.h"

#include "engine/engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>

#include "include/config.h"


#include "persist/persistence.h"


#include <sys/time.h>
#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

// extern ServerConfig g_config;
extern void send_rdb_to_slave(int slave_fd);

static int g_epfd = -1;   // 供定时器使用

engine_t *g_engine = {0};

int init_kvengine(void) 
{
    g_engine = engine_create();
    return 0;
}

void dest_kvengine(void)
{
    engine_destroy(g_engine);
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

int kvs_filter_protocol(char *msg, int length, char *response,int *is_sync) {
    if (msg == NULL || length < PROTO_MIN_LEN || response == NULL) return -1;

    kvs_blob_t cmd_blob = {0};
    kvs_blob_t key_blob = {0};
    kvs_blob_t val_blob = {0};
    
    server_decode_resp(msg,&cmd_blob,&key_blob,&val_blob);

    int ret_len = 0;
	int ret = 0;

    int admin_cmd = -1;
    int cmd;
    //第一步优先匹配管理命令
    for(int i=0;i<ADMIN_CMD_COUNT;i++)
    {
        if(strlen(admin_command[i])==cmd_blob.len&&strncmp(cmd_blob.data, admin_command[i], cmd_blob.len)==0)
        {
            admin_cmd = i;
            break;
        }
    }
    //处理管理命令 没有则处理普通命令
    if(admin_cmd != -1)
    {
        LOG_INFO("admin_cmd:%s\n",admin_command[admin_cmd]);
        //处理管理指令
        switch(admin_cmd)
        {
            case ADMIN_CMD_SAVE:
            {
                engine_t* current = g_engine;
                struct timeval tv_begin;
	            gettimeofday(&tv_begin, NULL);

                RDB_sync();
                struct timeval tv_end;
	            gettimeofday(&tv_end, NULL);

	            int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms
                
                LOG_INFO("RDB_sync调用成功 timeused:%d\n", time_used);
                ret_len = sprintf(response, "+OK\r\n");
                break;
            }
            case ADMIN_CMD_BGSAVE:
            {
                engine_t* current = g_engine;
                RDB_async();
                LOG_DEBUG("RDB_async调用成功\n");
                ret_len = sprintf(response, "+OK\r\n");
                break;
            }
            case ADMIN_CMD_SYNC:
            {
                LOG_INFO("收到 SYNC 命令，当前连接将标记为从节点！");                
                *is_sync = 1;
                LOG_INFO("已设置 *is_sync = 1");
                RDB_sync();

                ret_len = 0;
                break;
            }
            case ADMIN_CMD_PING:
                ret_len = sprintf(response, "+PONG\r\n");
                break;
            default:
                ret_len = sprintf(response,"-ERR unknown admin cmd\r\n");
        }

    }
    else
    {
        for (cmd = PROTO_CMD_SET; cmd < PROTO_CMD_COUNT; cmd++) {
            if (strlen(kv_command[cmd]) == cmd_blob.len &&
                strncmp(cmd_blob.data, kv_command[cmd], cmd_blob.len) == 0) {
                break;
            }
        }

        switch (cmd) 
        {
            case PROTO_CMD_SET:
                //LOG_DEBUG(">>> SET command executed!\n");
                ret = engine_set(g_engine,&key_blob, &val_blob);
                if (ret < 0) {
                    ret_len = sprintf(response, "-ERR internal error\r\n");
                } else if (ret == 0) {
                    ret_len = sprintf(response, "+OK\r\n");
                } else {
                    ret_len = sprintf(response, ":0\r\n");
                }
                if (g_role == ROLE_MASTER && g_slave_fd > 0) 
                {
                    master_sync(msg, length);
                }
                break;

            case PROTO_CMD_GET:
            {
                kvs_blob_t *val = engine_get(g_engine,&key_blob);
                if (val == NULL)
                    ret_len = sprintf(response, "$-1\r\n");
                else
                    // %.*s ：按长度打印，不怕\0截断
                    ret_len = sprintf(response, "$%d\r\n%.*s\r\n", val->len,val->len,(char*)val->data);
                break;
            }

            case PROTO_CMD_DEL:
                ret = engine_del(g_engine,&key_blob);
                if (ret == 0)
                    ret_len = sprintf(response, ":1\r\n");
                else
                    ret_len = sprintf(response, ":0\r\n");
                if (g_role == ROLE_MASTER && g_slave_fd > 0) 
                {
                    master_sync(msg, length);
                }
                break;

            case PROTO_CMD_MOD:
                ret = engine_mod(g_engine,&key_blob, &val_blob);
                if (ret == 0)
                    ret_len = sprintf(response, "+OK\r\n");
                else
                    ret_len = sprintf(response, "-ERR key not exist\r\n");
                if (g_role == ROLE_MASTER && g_slave_fd > 0) 
                {
                    master_sync(msg, length);
                }
                break;

            case PROTO_CMD_EXIST:
                ret = engine_exist(g_engine,&key_blob);
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

    //AOF持久化使用
        if(g_config.role == ROLE_MASTER && g_config.aof_strategy == AOF_ALWAYS)
        {
            //持久化
            char aof_buf[BUFFER_LENGTH] ={0};
            if(admin_cmd != -1)
            {
                snprintf(aof_buf,sizeof(aof_buf),"%.*s\r\n",cmd_blob.len, cmd_blob.data);
            }
            else 
            {
                snprintf(aof_buf,sizeof(aof_buf),"%.*s %.*s %.*s\r\n",cmd_blob.len, cmd_blob.data,key_blob.len, key_blob.data,val_blob.len, val_blob.data);
            }
            LOG_DEBUG("AOF持久化被调用");
            LOG_DEBUG("aof_buf:%s", aof_buf);

            AOF(aof_buf);
        }
  


    

	
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

	




	return ret_len;




}



/*
 * msg: request message
 * length: length of request message
 * response: need to send
 * @return : length of response
 */

int kvs_protocol(char *msg, int length, char *response,int *is_sync) 
{ 
	if (msg == NULL || length <= 0 || response == NULL) return -1;
    
    return kvs_filter_protocol(msg,length, response,is_sync);
}





