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



/**
 * 序列化单条记录，把内存的blob转成二进制字节写到fd
 * 格式：key_len(4) | key数据 | val_len(4) | val数据 | expire_ms(8)
 */
static void rdb_write_kv(int fd, kvs_blob_t *key, kvs_blob_t *val, uint64_t expire_ms)
{
    uint32_t klen = key->len;
    uint32_t vlen = val->len;

    if(write(fd,&klen,sizeof(uint32_t))==-1)
    {
        LOG_DEBUG("write klen fail\n");
    }
    
    if(write(fd,key->data,klen)==-1)
    {
        LOG_DEBUG("write kdata fail\n");
    }

    if(write(fd,&vlen,sizeof(uint32_t))==-1)
    {
        LOG_DEBUG("write vlen fail\n");
    }
    
    if(write(fd,val->data,vlen)==-1)
    {
        LOG_DEBUG("write vdata fail\n");
    }

    if(write(fd,&expire_ms,sizeof(uint64_t))==-1)
    {
        LOG_DEBUG("write expire_ms fail\n");
    }

}

// 写文件末尾结束标记
static void rdb_write_eof(int fd)
{
    uint8_t eof_mark[8] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
    write(fd, eof_mark, 8);
}

/**
 * 回调函数：由kvs_array_foreach循环调用
 * arg 传入fd的地址
 */
static void rdb_entry_callback(void *arg,kvs_blob_t *key,kvs_blob_t *val,uint64_t expire_ms)
{
    int fd = *(int*)arg;
    LOG_INFO("[rdb callback] key len:%d\n", key->len);
    rdb_write_kv(fd,key,val,expire_ms);
}

int rdb_do_dump(kvs_array_t *inst,const char *tmp_path, const char *real_path)
{

    LOG_INFO("[rdb] inst=%p, array size=%d\n", inst, inst->total);

    //1.创建临时文件
    int fd = open(tmp_path,O_RDWR | O_CREAT | O_TRUNC,0644);
    if(fd < 0)
    {
        LOG_ERROR("open tmp rdb failed\n");
        return -1;
    }
    LOG_DEBUG("fd create success\n");

    //2.RDB文件头部是4字节魔数REDI 1字节版本
    write(fd,"REDIS",5);
    uint8_t version = 0;
    write(fd,&version,sizeof(uint8_t));

    //3.遍历全部kv 在array.c中回调 回调函数为rdb_entry_callback
    kvs_array_foreach(inst,rdb_entry_callback, &fd);
    LOG_DEBUG("遍历完成\n");

    //4.写结束标记
    rdb_write_eof(fd);
    LOG_DEBUG("写eof结束\n");

    //5.把数据刷到磁盘
    fsync(fd);
    LOG_DEBUG("刷盘完成\n");

    //6.关闭fd
    close(fd);
    LOG_DEBUG("关闭fd完成\n");

    //7.原子重命名
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


#if ENABLE_ARRAY
    kvs_array_t *current = (kvs_array_t*)g_engine.impl;

    rdb_do_dump(current,tmp_name, real_name);

#elif ENABLE_RBTREE
    kvs_rbtree_t *current = (kvs_rbtree_t*)g_engine.impl;
    

#elif ENABEL_HASH

#endif
    
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
RDB
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
int RDB_load(kvs_array_t *inst)
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

        kvs_array_set(inst, &key, &val);
        free(key_buf);
        free(val_buf);

    }
    close(fd);
    return 0;

}

//=============================================================================================

//AOF
int AOF(const char *msg)
{
/*
    use aof_always stragety
*/
#if AOF_ALWAYS
    if(!msg) return -1;
    FILE *fp = fopen("kv.txt", "a");
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
#endif

}

int AOF_rewrite()
{


    return 0;
}

void AOF_restore()
{
    FILE* fp = fopen("kv.txt","r");
    if(fp == NULL)
    {
        perror("kv.txt不存在\n");
        return;
    }

    char line[BUFFER_LENGTH]={0};
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
/*
        #if ENABLE_ARRAY
        if (strcmp(cmd, "SET") == 0) {
            kvs_array_set(&global_array, &key_blob, &val_blob);
        } else if (strcmp(cmd, "MOD") == 0) {
            kvs_array_mod(&global_array, &key_blob, &val_blob);
        }
        #endif

        #if ENABLE_RBTREE
        if (strcmp(cmd, "RSET") == 0) {
            kvs_rbtree_set(&global_rbtree, &key_blob, &val_blob);
        } else if (strcmp(cmd, "RMOD") == 0) {
            kvs_rbtree_mod(&global_rbtree, &key_blob, &val_blob);
        }
        #endif

        #if ENABLE_HASH
        if (strcmp(cmd, "HSET") == 0) {
            kvs_hash_set(&global_hash, &key_blob, &val_blob);
        } else if (strcmp(cmd, "HMOD") == 0) {
            kvs_hash_mod(&global_hash, &key_blob, &val_blob);
        }
        #endif
    }

    fclose(fp);
    printf("AOF_restore success\n");
*/
    }

}
