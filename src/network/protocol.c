#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/kvstore.h"
#include "../utils/log.h"

/*
    kv_command 和 admin_command 是命令数组 用于表示所有命令
*/
const char *kv_command[] = {
	"SET", "GET", "DEL", "MOD", "EXIST",/*基础操作*/
    "INCR", "DECR",            /* 原子计数 */
    "APPEND", "STRLEN",        /* 字符串操作 */
    "EXPIRE", "TTL", "PERSIST",/* 过期 */
    "KEYS", "DBSIZE", "TYPE",  /* 遍历/元信息 */
    "MSET", "MGET",            /* 批量 */
};

const char *admin_command[] = {
    "SAVE", "BGSAVE",          /* 持久化 */
    "SYNC",                    /* 主从 */
    "PING", "INFO",            /* 健康检查 */
    "FLUSHALL",                /* 清空 */
    "SHUTDOWN",                /* 关闭 */
};

/*
cmd是admin指令就返回-1
cmd是普通指令且是SET或者GET就返回1 未完成
*/
int handle_cmd(char* cmd)
{
    if(!strcmp(cmd, "SAVE")||!strcmp(cmd, "BGSAVE")||!strcmp(cmd, "SYNC"))
    {
        return -1;
    }
    else if(!strcmp(cmd, "SET")||!strcmp(cmd, "MOD")||!strcmp(cmd, "RSET")||!strcmp(cmd, "RMOD")||!strcmp(cmd, "HSET")||!strcmp(cmd, "HMOD"))
    {
        return 1;
    }
    else 
    {
        return 0;
    }
}

/*
客户端发送的数据打包成resp协议格式
*/
int client_encode_resp(char *line,char *packet)
{
    char *p = line;
    char cmd_buf[128],key_data[128],val_data[128];
    int flag;//标识是否有val

    //跳过空格和制表符
    while(*p==' '||*p =='\t') p++;

    //编码cmd
    char *cmd = p;
    while(*p!= ' '&&*p != '\n'&&*p!= '\0') p++;
    int cmd_len = p-cmd;
    memcpy(cmd_buf,cmd, cmd_len);
    cmd_buf[cmd_len] = '\0';
    flag = handle_cmd(cmd_buf);
    p++;

    //解析key(admin指令则不用编码key)
    if(flag == -1)
    {
        sprintf(packet,"*1\r\n$%d\r\n%s\r\n",cmd_len,cmd_buf);
        return 0;
    }
    char *key = p;
    while(*p!= ' '&&*p != '\n'&&*p!= '\0') p++;
    int key_len = p-key;
    memcpy(key_data,key, key_len);
    key_data[key_len] = '\0';

    char *current = packet;
    if (flag == 1) 
    {
        current += sprintf(current, "*3\r\n");
    } 
    else {
        // GET: 2个参数 *2
        current += sprintf(current, "*2\r\n");
    }
    current += sprintf(current, "$%d\r\n%s\r\n",cmd_len,cmd_buf);
    current += sprintf(current, "$%d\r\n%s\r\n",key_len,key_data);

    //编码val(如果有)
    if(flag)
    {
        p++;
        char* val = p;
        while(*p != '\n'&&*p!= '\0') p++;
        int val_len = p-val;
        memcpy(val_data,val, val_len);
        val_data[val_len] = '\0';

        current += sprintf(current, "$%d\r\n%s\r\n",val_len,val_data);
    }
    return 0;

}


/*
 * 解析一个 RESP Bulk String：$N\r\n<data>\r\n
 *
 * 参数：
 *   pp       - 输入输出，指向当前解析位置。函数会更新它。
 *   end      - 缓冲区末尾指针（用于边界检查）
 *   out_len  - 输出，数据长度
 *   out_data - 输出，数据起始地址（指向 msg 内部，不分配新内存）
 *
 * 返回值：
 *   0  = 解析成功
 *  -1  = 数据不完整（需要更多数据）
 *  -2  = 格式错误
 */
static int parse_bulk(char **pp,char *end, int *out_len,char **out_data)
{
    char *p = *pp;

    /*检查'$'前缀*/
    if(p>=end)
    {
        return -1;
    }
    if(*p != '$')
    {
        return -2;
    }
    p++;

    /*读长度数字*/
    if(p>=end)
    {
        return -1;
    }
    if(*p<'0'||*p>'9')
    {
        return -2;
    }

    int len = 0;
    while(p<end && *p>='0' &&*p<='9')
    {
        len = len*10 + (*p - '0');
        p++;
    }

    /*读\r\n*/
    if(p+2>end)
    {
        return -1;
    }
    if(*p!='\r' || *(p+1) != '\n')
    {
        return -2;
    }
    p+=2;

    /*读n字节数据*/
    if(p+len>end)
    {
        return -1;
    }
    *out_data = p;
    p+=len;

    /*读数据后的\r\n*/
    if(p+2>end)
    {
        return -1;
    }
    if(*p != '\r'|| *(p+1)!= '\n')
    {
        return -2;
    }
    p+=2;

    /*成功返回*/
    *out_len = len;
    *pp = p;
    return 0;
}


/*
服务端接收到的数据解码成原先格式
return 0表示成功解码，返回1表示解码admin cmd，返回-1表示解码失败
*/
int server_decode_resp(char *msg,int msg_len,kvs_blob_t *cmd_blob,kvs_blob_t *key_blob,kvs_blob_t *val_blob,int *consumed)
{
    if (!msg || !cmd_blob || !key_blob || !val_blob) 
    {
        return -1;
    }
    *consumed = 0;

    char *p = msg;
    char *end = msg + msg_len;

    /*解析*N\r\n*/
    if(p>=end || *p != '*')
    {
        /*格式有误或传入的msg_len为0*/
        return -1;
    }
    p++;

    int data_num = 0;
    if(p>=end||*p<'0'||*p>'9')
    {
        return -1;
    }

    while (p<end&&*p>='0'&&*p<='9') 
    {
        data_num = data_num*10 + (*p - '0');
        p++;
    }

    /*边界检查 查看是否能塞得下\r\n*/
    if(p+2>end) 
    {
        return -2;
    }

    if(*p != '\r'||*(p+1) != '\n')
    {
        return -1;
    }
    
    p+=2;

    /*解析cmd Bulk String*/
   if(parse_bulk(&p, end, &cmd_blob->len,&cmd_blob->data)!=0)
   {
        return -2;
   }

   /*admin cmd情况*/
   if(data_num == 1)
   {
        *consumed = (int)(p-msg);
        return 1;
   }

    /* ===== 3. 解析 key Bulk String ===== */
    int bulk_ret = parse_bulk(&p, end, &key_blob->len, &key_blob->data);
    if (bulk_ret == -1) 
    {
        return -2;   // 数据不完整
    }
    if (bulk_ret == -2)
    {
        return -1;   // 格式错误
    } 

    /* ===== 4. 解析 value（可选） ===== */
    if(data_num == 3)
    {
        if (parse_bulk(&p, end, &val_blob->len, &val_blob->data) != 0) 
        {
            return -2;
        } 
    }
    else 
    {
        val_blob->data = NULL;
        val_blob->len = 0;
    }

    *consumed = (int)(p-msg);

    return 0;
}