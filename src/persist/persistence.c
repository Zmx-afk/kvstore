/*
    This file is used to realize AOF and RDB persistence stragety
*/

#include "../include/kvstore.h"


//#include <cstdint>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "persistence.h"
#include "../engine/engine.h"


//#include <sys/types.h>
//#include <sys/stat.h>
#include <fcntl.h>

//从fd精确读n字节到buf 成功返回0 出错/EOF返回-1
int read_n(int fd, void *buf,size_t n)
{
    char *p = (char*)buf;
    size_t remain = n;
    while(remain>0)
    {
        ssize_t r = read(fd,p,remain);
        if(r==0) return -1;
        if(r==-1)
        {
            if(errno == EINTR) continue;
            return -1;
        }
        p+=r;
        remain -= r;

    }
    return 0;
}

//======================================================================================
/*
2.0版本
    RDB_sync()-->RDB_realize()-->rdb_do_dump()-->kvs_array_foreach()使用回调-->rdb_entry_callback()-->rdb_write_kv()
    rdb_write_kv()主要问题:多次系统调用write 使得开销变大 改造思路 创建一块大缓冲区 存满后存入文件 只需要调用一次write

*/


/*
    rdb_buffer_append():
        把数据拷贝到ctx的buffer buffer满后刷入文件
    rdb_ctx_t *ctx: 上下文结构体
    data: 需要写入缓冲区的数据
    len: 写入缓冲区数据的长度 
*/
static int rdb_buffer_append(rdb_ctx_t *ctx, const void *data, size_t len)
{
    if(ctx->buf_pos+len>ctx->buf_size)
    {
        //写入不了新的数据 因为buffer即将满 调用write清空
        ssize_t write_len = write(ctx->fd,ctx->buf,ctx->buf_pos);
        if(write_len != ctx->buf_pos)
        {
            LOG_ERROR("write buffer to file failed");
            return -1;
        } 
        ctx->buf_pos=0;

    }

    //写入新数据
    memcpy(ctx->buf+ctx->buf_pos,data,len);
    ctx->buf_pos += len;

    return 0;
}


/**
 *rdb_write_kv作用:调用rdb_buffer_append 将每条kv记录写入缓冲区 缓冲区满后的写入逻辑由rdb_buffer_append处理 
 *序列化单条记录，把内存的blob转成二进制字节写到fd
 * 格式：key_len(4) | key数据 | val_len(4) | val数据 | expire_ms(8)
 *

 */
static void rdb_write_kv(rdb_ctx_t *ctx, kvs_blob_t *key, kvs_blob_t *val, uint64_t expire_ms)
{
    uint32_t klen = key->len;
    uint32_t vlen = val->len;

    rdb_buffer_append(ctx,&klen,sizeof(uint32_t));    
    rdb_buffer_append(ctx,key->data,klen);
    rdb_buffer_append(ctx,&vlen,sizeof(uint32_t));
    rdb_buffer_append(ctx,val->data,vlen);
    rdb_buffer_append(ctx,&expire_ms,sizeof(uint64_t));

}

/**
 * 回调函数：由kvs_array_foreach循环调用
 * arg: 缓冲区上下文
 * 
 */
static void rdb_entry_callback(void *arg,kvs_blob_t *key,kvs_blob_t *val,uint64_t expire_ms)
{
    rdb_ctx_t *ctx = (rdb_ctx_t *)arg;
    //LOG_DEBUG("[rdb callback] key len:%d\n", key->len);
    rdb_write_kv(ctx,key,val,expire_ms);
}


/*
    rdb_do_dump()
    RDB文件头部是REDIS加版本号

*/
#define RDB_BUF_SIZE    4096
int rdb_do_dump(engine_t *inst,const char *tmp_path, const char *real_path)
{

    //LOG_DEBUG("【RDB调试】rdb_do_dump被调用,引擎内总条目数: %d\n", inst->total);

    int fd = open(tmp_path,O_RDWR | O_CREAT | O_TRUNC,0644);
    if(fd < 0)
    {
        LOG_ERROR("open tmp rdb failed\n");
        return -1;
    }
    LOG_DEBUG("fd create success\n");

    //初始化缓冲区参数(缓冲区创建在堆区)
    char *buf = malloc(RDB_BUF_SIZE);
    rdb_ctx_t ctx = {
        .fd = fd,
        .buf = buf,
        .buf_pos = 0,
        .buf_size = RDB_BUF_SIZE
    };

    //RDB文件头部是4字节魔数REDI加1字节版本
    rdb_buffer_append(&ctx,"REDIS",5);
    uint8_t version = 0;
    rdb_buffer_append(&ctx,&version,sizeof(uint8_t));

    //遍历存储引擎全部kv 在array.c中回调 回调函数为rdb_entry_callback
    engine_foreach(inst,rdb_entry_callback, &ctx);
    LOG_DEBUG("遍历完成\n");

    //遍历结束 若buffer里面残留数据 需强制刷盘
    if(ctx.buf_pos>0)
    {
        ssize_t write_len = write(fd, ctx.buf, ctx.buf_pos);
        if(write_len != ctx.buf_pos)
        {
            LOG_ERROR("flush remain buffer fail");
            free(buf);
            close(fd);
            return -1;
        }
        ctx.buf_pos = 0;
    }

    //写结束标记
    uint8_t eof_mark[8] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
    write(fd, eof_mark, 8);
    LOG_DEBUG("写eof结束\n");

    //把数据刷到磁盘
    fsync(fd);
    LOG_DEBUG("刷盘完成\n");

    //关闭fd
    close(fd);
    LOG_DEBUG("关闭fd完成\n");

    //释放缓冲区
    free(buf);

    //原子重命名
    if(rename(tmp_path,real_path)!=0)
    {
        LOG_ERROR("rename rdb fail\n");
        unlink(tmp_path);
        return -1;
    }
    LOG_DEBUG("原子重命名完成\n");

    return 0;
}

int RDB_realize()
{
    char *real_name = "RDB.rdb";
    char tmp_name[256];
    sprintf(tmp_name,"%s.tmp",real_name);


    engine_t *current = g_engine;

    rdb_do_dump(current,tmp_name, real_name);
    LOG_DEBUG("rdb_do_dump被调用");


    
    //exit(0);

    return 0;   


}

/*
RDB_sync
同步进行rdb持久化
*/

int RDB_sync()
{
    RDB_realize();
    fflush(stdout);
    return 0;
}

/*
RDB_async
create a new process it will take a photo to main process's data
*/
int RDB_async()
{
    pid_t pid = fork();
    
    if(pid<0)
    {
        printf("fork failed\n");
        return -1;
    }
    else if(pid ==0)
    {
       
        for(int fd = 3; fd < 1024; fd++){
            close(fd);
        }
        RDB_realize();
        fflush(stdout);
        exit(0);
        
    }
    return 0;

}

//RDB文件恢复函数
int RDB_load(engine_t *inst)
{
    char rdb_path[256] = "RDB.rdb";
    int fd = open(rdb_path, O_RDONLY);
    if(fd<0)
    {
        LOG_ERROR("RDB_load fd open fail\n");
        return -1;
    }
    
    //1.检查魔数头
    char magic[5] = {0};
    if(read_n(fd,magic,5)!=0)
    {
        LOG_ERROR("读魔数失败 rdb损坏\n");
        close(fd);
        return -1;
    }

    //校验魔数
    if(magic[0]!='R'||magic[1]!='E'||magic[2]!='D'||magic[3]!='I'||magic[4]!='S'){
        LOG_ERROR("不是合法RDB文件\n");
        close(fd);
        return -1;
    }

    LOG_DEBUG("魔数校验通过:REDIS\n");

    //2.读取1字节版本号
    uint8_t ver;
    if(read_n(fd,&ver, 1)!=0)
    {
        LOG_ERROR("读版本失败\n");
        close(fd);
        return -1;
    }
    
    LOG_DEBUG("RDB版本:%u\n",ver);

    int count = 0;
    //3.循环读取kv键值对
    while(1)
    {
        uint32_t klen;
        if(read_n(fd,&klen,4)!=0)
        {
            LOG_DEBUG("RDB文件读取结束\n");
            break;
        }

        if(klen == 0xFFFFFFFFU)
        {
            // 已经读到eof_mark头部，直接退出
            break;
        }

        char *key_buf = malloc(klen);
        if(!key_buf)
        {
            LOG_ERROR("malloc key fail\n");
            close(fd);
            return -1;
        }
        if(read_n(fd,key_buf,klen)!=0)
        {
            free(key_buf);
            break;
        }

        uint32_t vlen;
        if(read_n(fd,&vlen,4)!=0)
        {
            free(key_buf);
            break;
        }
        char *val_buf = malloc(vlen);
        if(!val_buf)
        {
            LOG_ERROR("malloc val fail\n");
            free(key_buf);
            close(fd);
            return -1;
        }
        if(read_n(fd,val_buf,vlen)!=0)
        {
            free(key_buf);
            free(val_buf);
            break;
        }

        uint64_t expire_ms;
        if(read_n(fd, &expire_ms, sizeof(uint64_t))!=0)
        {
            free(key_buf);
            free(val_buf);
            break;
        }

        kvs_blob_t key = {.data = key_buf,.len = klen};
        kvs_blob_t val = {.data = val_buf,.len = vlen};

        engine_set(inst, &key, &val);
        free(key_buf);
        free(val_buf);

        count++;
        LOG_DEBUG("加载完成第%d条数据\n",count);
    }
    close(fd);
    return 0;

}

/*=============================================================================================
  =============================================================================================
  ==============================================================================================
*/
//AOF
int AOF(const char *msg)
{
/*
    use aof_always stragety
*/

    if(!msg) return -1;
    FILE *fp = fopen("AOF.aof", "a");
    if(!fp) return -1;
    if(fputs(msg, fp) == EOF)
    {
        perror("fputs 写入失败\n");
        fclose(fp);
        return -1;
    }
    if(fputc('\n', fp) == EOF) {
        perror("fputc 写入换行失败\n");
        fclose(fp);
        return -1;
    }

    //刷盘
    fflush(fp);
    fsync(fileno(fp));

    fclose(fp);
    return 0;
}

int AOF_rewrite()
{


    return 0;
}

int AOF_restore()
{
    FILE* fp = fopen("AOF.aof","r");
    if(fp == NULL)
    {
        perror("AOF.aof不存在\n");
        return -1;
    }

    char line[BUFFER_LENGTH]={0};

    int count = 0;
    while(fgets(line,sizeof(line), fp)!=NULL)
    {
        //去掉换行符 为sscanf函数做准备
        line[strcspn(line,"\n")]=0;
        //跳过空行
        if(strlen(line)==0) continue;

        //把一行数据解析
        char cmd[16] = {0};
        char key[256] = {0};
        char val[256] = {0};

        //拆字符串
        sscanf(line,"%s %s %s",cmd,key,val);

        kvs_blob_t key_blob = {.data = key,.len = strlen(key)};
        kvs_blob_t val_blob = {.data = val,.len = strlen(val)};

        if(strcmp(cmd,"SET")==0)
        {
            engine_set(g_engine,&key_blob,&val_blob);
        }
        else if(strcmp(cmd,"MOD")==0)
        {
            engine_mod(g_engine,&key_blob,&val_blob);
        }
        else if(strcmp(cmd,"DEL")==0)
        {
            engine_del(g_engine,&key_blob);
        }
        count++;
    }

    LOG_INFO("AOF恢复完成,共恢复%d条数据\n",count);
    return 0;
}
