#ifndef PERSISTENCE__H
#define PERSISTENCE__H

#include "../engine/engine.h"
#include "../utils/log.h"


typedef struct
{
    int fd;         //rdb文件的文件描述符
    char *buf;      //写入数据的缓冲区
    int buf_pos;    //当前buffer写到哪个位置
    int buf_size;   //buffer的大小
}rdb_ctx_t;

int RDB_sync();
int RDB_async();

int RDB_load(engine_t *inst);

void AOF_destroy(void);
int AOF_init(void);

#endif