#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/kvstore.h"

const char *kv_command[] = {
	"SET", "GET", "DEL", "MOD", "EXIST",
};

const char *admin_command[] = {
    "SAVE","BGSAVE","FLUSHALL"
};


int handle_cmd(char* cmd)//cmd是SET或者GET就返回1 未完成
{
    if(!strcmp(cmd, "SET")||!strcmp(cmd, "MOD")||!strcmp(cmd, "RSET")||!strcmp(cmd, "RMOD")||!strcmp(cmd, "HSET")||!strcmp(cmd, "HMOD"))
    {
        return 1;
    }
    else {
        return 0;
    }
}

/*客户端发送的数据打包成resp协议格式

*/
int client_encode_resp(char *line,char *packet)
{
    char *p = line;
    char cmd_buf[128],key_data[128],val_data[128];
    int flag;//标识是否有val

    //跳过空格和制表符
    while(*p==' '||*p =='\t') p++;

    //解析cmd
    char *cmd = p;
    while(*p!= ' '&&*p != '\n'&&*p!= '\0') p++;
    int cmd_len = p-cmd;
    memcpy(cmd_buf,cmd, cmd_len);
    cmd_buf[cmd_len] = '\0';
    flag = handle_cmd(cmd_buf);
    p++;

    //解析key
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

    //解析val(如果有)
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

    //printf("packet:%s\n",packet);

    return 0;

}

//服务端接收到的数据解码成原先格式
int server_decode_resp(char *msg,kvs_blob_t *cmd_blob,kvs_blob_t *key_blob,kvs_blob_t *val_blob)
{
    if (!msg || !cmd_blob || !key_blob || !val_blob) return -1;

    char *p = msg;
    int data_num;

    // 1. 解析数组元素个数（跳过 * 和数字）
    while (*p != '*') p++;
    p++;
    data_num = atoi(p);
    while (*p >= '0' && *p <= '9') p++;
    while (*p == '\r' || *p == '\n') p++;

    // 2. 解析命令（第一个 Bulk String）
    if (*p != '$') return -1;
    p++;
    int cmd_len = atoi(p);
    while (*p >= '0' && *p <= '9') p++;
    while (*p == '\r' || *p == '\n') p++;
    char *cmd_start = p;      // 指向命令数据的起始
    p += cmd_len;             // 跳过数据
    while (*p == '\r' || *p == '\n') p++;  // 跳过数据后的 \r\n
    cmd_blob->data = cmd_start;
    cmd_blob->len = cmd_len;

    // 3. 解析 key（第二个 Bulk String）
    if (*p != '$') return -1;
    p++;
    int key_len = atoi(p);
    while (*p >= '0' && *p <= '9') p++;
    while (*p == '\r' || *p == '\n') p++;
    char *key_start = p;
    p += key_len;
    while (*p == '\r' || *p == '\n') p++;
    key_blob->data = key_start;
    key_blob->len = key_len;

    // 4. 解析 value（如果有第三个参数）
    if (data_num == 3) {
        if (*p != '$') return -1;
        p++;
        int val_len = atoi(p);
        while (*p >= '0' && *p <= '9') p++;
        while (*p == '\r' || *p == '\n') p++;
        char *val_start = p;
        p += val_len;
        while (*p == '\r' || *p == '\n') p++;
        val_blob->data = val_start;
        val_blob->len = val_len;
    } else {
        val_blob->data = NULL;
        val_blob->len = 0;
    }

    return 0;
}