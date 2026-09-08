#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "kvstore.h"

typedef enum {
	PROTO_CMD_SET   = 0,
    PROTO_CMD_GET   = 1,
    PROTO_CMD_DEL   = 2,
    PROTO_CMD_MOD   = 3,
    PROTO_CMD_EXIST = 4,
    PROTO_CMD_COUNT = 5
} kvs_proto_cmd_t;

typedef enum {
    ADMIN_CMD_SAVE = 0,     //同步阻塞进行写入
    ADMIN_CMD_BGSAVE,    //异步后台进行写入
    ADMIN_CMD_SYNC,
    ADMIN_CMD_PING,       // 新增
    ADMIN_CMD_FLUSHALL,
    ADMIN_CMD_COUNT
}kvs_admin_cmd_t;



int handle_cmd(char* cmd);
int client_encode_resp(char *line,char *packet);
int server_decode_resp(char *msg,kvs_blob_t *cmd_blob,kvs_blob_t *key_blob,kvs_blob_t *val_blob);


extern const char *kv_command[];
extern const char *admin_command[];




#endif
