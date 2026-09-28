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
#include <sys/stat.h>
#include <sys/mman.h>

#include <liburing.h>
#include <sys/types.h>

#include "persistence.h"
#include "../engine/engine.h"
#include "../include/config.h"
#include "../include/protocol.h"


#include <sys/time.h>
#include <time.h>


//#include <sys/types.h>
//#include <sys/stat.h>
#include <fcntl.h>

#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)


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

static int write_all(int fd,const void *buf,size_t len)
{
    const char *p = buf;
    size_t offset = 0;
    while(offset<len)
    {
        ssize_t n = write(fd,buf,len - offset);
        if(n<0)
        {
            if(errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        if(n == 0)
        {
            return -1;
        }
        offset += n;
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
        把数据拷贝到ctx的buffer buffer满后刷入文件 对大条数据进行额外处理
    rdb_ctx_t *ctx: 上下文结构体
    data: 需要写入缓冲区的数据
    len: 写入缓冲区数据的长度 
*/
static int rdb_buffer_append(rdb_ctx_t *ctx, const void *data, size_t len)
{
    /*单条数据比整个缓冲区大->直接写文件*/
    if(len>ctx->buf_size)
    {
        /*先刷掉缓冲区已有数据*/
        if(ctx->buf_pos>0)
        {
            if(write_all(ctx->fd,ctx->buf, ctx->buf_pos)!=0)
            {
                return -1;
            }
            ctx->buf_pos = 0;
        }
        /*直接把大条数据写入*/
        if(write_all(ctx->fd,data,len)!=0)
        {
            return -1;
        }
        return 0;
    }

    /*正常路径*/
    if(ctx->buf_pos+len>ctx->buf_size)
    {
        //写入不了新的数据 因为buffer即将满 调用write清空
        if(write_all(ctx->fd,ctx->buf,ctx->buf_pos)!=0)
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
#define RDB_BUF_SIZE    (256*1024)
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
        exit(0);
        
    }
    return 0;

}

//RDB文件恢复函数
int RDB_load(engine_t *inst)
{
    int fd = open("RDB.rdb", O_RDONLY);
    if(fd<0)
    {
        LOG_ERROR("RDB_load fd open fail\n");
        return -1;
    }

    struct stat st;
    fstat(fd,&st);

    /* 文件小于头部 */
    if (st.st_size < 6) {
        LOG_ERROR("RDB 文件太小");
        close(fd);
        return -1;
    }
    struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);

    /* 一次性把整个文件映射到内存 */
    char *base = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) {
        LOG_ERROR("mmap fail");
        close(fd);
        return -1;
    }
    char *p = base;
    char *end = base + st.st_size;

    /* ★ 校验魔数 */
    if (memcmp(p, "REDIS", 5) != 0) {
        LOG_ERROR("不是合法 RDB 文件（魔数错误）");
        munmap(base, st.st_size);
        close(fd);
        return -1;
    }

    /* ★ 读版本号 */
    uint8_t version = (uint8_t)p[5];
    LOG_DEBUG("RDB 版本: %u", version);

    /*跳过魔数和版本*/
    p+=6;
    
    while(p<end)
    {
        if (p + 4 > end) 
        {
            break;
        }
        uint32_t klen;
        memcpy(&klen,p,4);
        p+=4;
        if(klen == 0xFFFFFFFF)
        {
            break;
        }
        if (p + klen > end) {
            LOG_ERROR("RDB 损坏:klen=%u 越界", klen);
            break;
        }
    
        char* key_data = p;
        p+=klen;

        if (p + 4 > end) 
        {
            break;
        }
        uint32_t vlen;
        memcpy(&vlen, p, 4); 
        p += 4;
        if (p + vlen > end) {
            LOG_ERROR("RDB 损坏:vlen=%u 越界", vlen);
            break;
        }
        char *val_data = p;
        p += vlen;

        if (p + 8 > end) 
        {
            break;
        }
        uint64_t expire;
        memcpy(&expire, p, 8);
        p += 8;

        kvs_blob_t key = {key_data,klen};
        kvs_blob_t val = {val_data,vlen};
        int ret = engine_set(inst,&key,&val);
        if (ret == -1) {
            LOG_ERROR("engine_set 参数错误:klen=%u, vlen=%u", klen, vlen);
        }
        
    }

    munmap(base,st.st_size);

    struct timeval tv_end;
	gettimeofday(&tv_end, NULL);

    int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms
                
    LOG_INFO("RDB_sync调用成功 timeused:%d\n", time_used);

    close(fd);
    return 0;

}

/*=============================================================================================
  =============================================================================================
  ==============================================================================================
*/
//AOF
static struct io_uring g_ring;      /*io_uring实例*/
static int g_aof_fd = -1;
static time_t g_last_fsync = 0;
static int g_pending = 0;           /*已提交但未确认的write数量*/

#define AOF_RING_SIZE   256

int AOF_init(void)
{
    /*初始化io_uring*/
    int ret = io_uring_queue_init(AOF_RING_SIZE,&g_ring, 0);
    if(ret < 0)
    {
        LOG_ERROR("io_uring_queue_init 失败: %s", strerror(-ret));
        return -1;
    }

    /*打开AOF文件*/
    g_aof_fd = open("AOF.aof",O_WRONLY | O_APPEND | O_CREAT, 0644);
    if(g_aof_fd < 0)
    {
        LOG_ERROR("打开 AOF 失败: %s", strerror(errno));
        io_uring_queue_exit(&g_ring);
        return -1;
    }

    LOG_INFO("AOF 初始化完成");
    return 0;
}

/*
 * 排空 CQE
 * wait_all = 1: 阻塞等所有 pending 完成
 * wait_all = 0: 非阻塞，只排空已完成的
 * 返回：成功排空的数量
 */
static int AOF_reap_completions(int wait_all)
{
    struct io_uring_cqe *cqe;
    int reaped = 0;

    while (g_pending > 0) {
        int ret;
        if (wait_all) {
            /* 阻塞等 */
            ret = io_uring_wait_cqe(&g_ring, &cqe);
        } else {
            /* 非阻塞 peek */
            ret = io_uring_peek_cqe(&g_ring, &cqe);
        }

        if (ret < 0) break;   /* 没有更多完成的 */

        /* 检查写入结果 */
        if (cqe->res < 0) {
            LOG_ERROR("io_uring write 失败: %s", strerror(-cqe->res));
        }

        /* 释放之前 malloc 的缓冲区 */
        free((void *)cqe->user_data);

        io_uring_cqe_seen(&g_ring, cqe);
        g_pending--;
        reaped++;

        if (!wait_all) break;   /* 非阻塞模式下只处理一个 */
    }

    return reaped;
}

int AOF(const char *msg,int len,int strategy)
{
    if(g_aof_fd < 0 || !msg || len<=0)
    {
        return -1;
    }

    /* 1. ★ 拷贝数据（内核在写完成前会读取这块内存） */
    char *buf = malloc(len);
    if (!buf) return -1;
    memcpy(buf, msg, len);

    /* 2. 拿一个 SQE */
    struct io_uring_sqe *sqe = io_uring_get_sqe(&g_ring);
    if (!sqe) {
        /* SQ 队列满，先排空一批 */
        AOF_reap_completions(1);
        sqe = io_uring_get_sqe(&g_ring);
        if (!sqe) {
            free(buf);
            LOG_ERROR("SQE 获取失败");
            return -1;
        }
    }

    /* 3. 填 SQE */
    io_uring_prep_write(sqe, g_aof_fd, buf, len, 0);
    sqe->user_data = (unsigned long long)buf;   /* ★ 保存指针，CQE 时释放 */

    /* 4. 提交 */
    if (io_uring_submit(&g_ring) < 0) {
        LOG_ERROR("io_uring_submit 失败");
        free(buf);
        return -1;
    }
    g_pending++;

     /* 5. 根据策略决定行为 */
    switch (strategy) {
        case AOF_ALWAYS:
            /* 等所有 pending 完成 + fsync */
            AOF_reap_completions(1);
            fsync(g_aof_fd);
            break;

        case AOF_EVERYSEC: {
            /* 队列快满时排空，防止 SQ 满 */
            if (g_pending >= AOF_RING_SIZE * 3 / 4) {
                AOF_reap_completions(0);
            }
            /* 每秒 fsync 一次 */
            time_t now = time(NULL);
            if (now - g_last_fsync >= 1) {
                AOF_reap_completions(1);   /* 先等所有 write 完成 */
                fsync(g_aof_fd);
                g_last_fsync = now;
            }
            break;
        }

        case AOF_NO:
            /* 只在必要时候排空 */
            if (g_pending >= AOF_RING_SIZE * 3 / 4) {
                AOF_reap_completions(0);
            }
            break;
    }
    return 0;
}

int AOF_rewrite()
{


    return 0;
}

int AOF_restore()
{
    int fd = open("AOF.aof", O_RDONLY);
    if(fd<0)
    {
        if (errno == ENOENT) {
            LOG_INFO("AOF.aof 不存在，跳过恢复");
            return 0;
        }
        LOG_ERROR("打开 AOF.aof 失败: %s", strerror(errno));
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return -1;
    }
    size_t file_size = st.st_size;   

    if (file_size == 0) 
    {
        LOG_INFO("AOF.aof 为空，跳过恢复");
        close(fd);
        return 0;
    }
    /* 读文件内容 */
    char *base = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) {
        LOG_ERROR("mmap 失败: %s", strerror(errno));
        close(fd);
        return -1;
    }

    /* 4. mmap 之后 fd 可以关了，不影响映射 */
    close(fd);

    struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);
    
    /* 当作 RESP 命令流解析 */
    int offset = 0;
    int count = 0;
    while (offset < (int)file_size) 
    {
        kvs_blob_t cmd={0}, key={0}, val={0};
        int consumed = 0;
        int ret = server_decode_resp(base + offset, file_size - offset,&cmd, &key, &val, &consumed);
        if(ret == -2)
        {
            LOG_ERROR("AOF 文件不完整(偏移 %d)", offset);
            break;
        }
        if (ret == -1) 
        {
            LOG_ERROR("AOF 文件损坏(偏移 %d)", offset);
            break;
        }
        if (cmd.len == 3 && memcmp(cmd.data, "SET", 3) == 0) {
            engine_set(g_engine, &key, &val);
        } else if (cmd.len == 3 && memcmp(cmd.data, "DEL", 3) == 0) {
            engine_del(g_engine, &key);
        } else if (cmd.len == 3 && memcmp(cmd.data, "MOD", 3) == 0) {
            engine_mod(g_engine, &key, &val);
        } else {
            LOG_WARN("AOF 中出现不支持的命令: %.*s", cmd.len, cmd.data);
        }

        offset += consumed;
        count++;
    
    }
    LOG_INFO("AOF 恢复完成，共 %d 条命令", count);

    struct timeval tv_end;
	gettimeofday(&tv_end, NULL);

    int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms
                
    LOG_INFO("AOF_restore调用成功 timeused:%d\n", time_used);
    /*解除映射*/
    munmap(base, file_size);
    return 0;
}

void AOF_destroy(void)
{
    if(g_aof_fd >=0)
    {
        AOF_reap_completions(1);   /* ★ 等所有 pending write 完成 */
        fsync(g_aof_fd);           /* ★ 强制落盘 */
        close(g_aof_fd);
        g_aof_fd = -1;
    }
    io_uring_queue_exit(&g_ring);
}
