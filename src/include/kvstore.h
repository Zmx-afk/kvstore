


#ifndef __KV_STORE_H__
#define __KV_STORE_H__


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define NETWORK_REACTOR 	0
#define NETWORK_PROACTOR	1
#define NETWORK_NTYCO		2

#define NETWORK_SELECT		NETWORK_REACTOR

#define KVS_MAX_TOKENS		128


//允许使用在kvstore.h末尾
#define ENABLE_ARRAY 		1
#define ENABLE_RBTREE		0
#define ENABLE_HASH			0


typedef struct {
	char *data;	
	int len;
}kvs_blob_t;



extern struct memory_kvs g_kvs;

typedef struct {
    void *impl;//指向存储引擎

    int (*set)(void *inst, kvs_blob_t *key, kvs_blob_t *value);
    kvs_blob_t* (*get)(void *inst, kvs_blob_t *key);
    int (*del)(void *inst, kvs_blob_t *key);
    int (*mod)(void *inst, kvs_blob_t *key, kvs_blob_t *value);
    int (*exist)(void *inst, kvs_blob_t *key);
    
    void* (*create)(void);
    void (*destroy)(void *inst);

}kvs_engine_t;
extern kvs_engine_t g_engine;


#define BUFFER_LENGTH		1024


typedef int (*msg_handler)(char *msg, int length, char *response,int *is_sync);


extern int reactor_start(unsigned short port, msg_handler handler);
extern int proactor_start(unsigned short port, msg_handler handler);
extern int ntyco_start(unsigned short port, msg_handler handler);

//protocal
int kvs_protocol(char *msg, int length, char *response,int *is_sync);



struct sdshdr {

    // 记录 buf 数组中已使用字节的数量
    // 等于 SDS 所保存字符串的长度
    int len;

    // 记录 buf 数组中未使用字节的数量
    int free;
    // 字节数组，用于保存字符串
    char buf[];
};


#define PROTO_MIN_LEN 5





//kvstore.c
int init_kvengine();
void dest_kvengine(void);


//AOF
int AOF(const char *msg);
void AOF_restore();

//RDB




//MS_replication

typedef enum 
{
    ROLE_MASTER = 0,  
    ROLE_SLAVE = 1,    
} NodeRole;

extern NodeRole g_role;
extern int g_master_fd;              
extern int g_slave_fd;                 
extern char g_master_ip[];  
extern int g_master_port;
extern int g_is_sync;

void *master_accept_slave(void *arg);
void master_sync(char *data, int len);
void slave_run();


typedef struct memory_kvs memory_kvs_t;

int memory_kvs_init(memory_kvs_t* kvs);




#endif




