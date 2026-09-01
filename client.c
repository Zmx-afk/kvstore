
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

#define BUFFER_SIZE 1024

int is_batch_mode = 0;               // 0=普通模式 1=批量模式
char batch_buf[100][BUFFER_SIZE] = {0};
int  batch_count = 0; 

// 打印缓冲区（只打印有效长度）
void print_buffer(const char* buf, int len) {
    const char *p = buf;
    int remaining = len;

    printf("===== 协议包【长度+内容】=====\n");

    // cmd
    int cmd_len;
    memcpy(&cmd_len, p, 4);
    p += 4;
    printf("cmd长度: %d | cmd内容: %.*s\n", cmd_len, cmd_len, p);
    p += cmd_len;

    // key
    int key_len;
    memcpy(&key_len, p, 4);
    p += 4;
    printf("key长度: %d | key内容: %.*s\n", key_len, key_len, p);
    p += key_len;

    // value
    if (p - buf < len) {
        int val_len;
        memcpy(&val_len, p, 4);
        p += 4;
        printf("val长度: %d | val内容: %.*s\n", val_len, val_len, p);
        p += val_len;
    }
    printf("=============================\n\n");
}


/*
    line:用户输入的数据
    packet:传出参数 打包后的数据
    out:传出参数 数据占用大小
*/
int handle_mes(char* line,char*packet ,int *out)
{
    char* p = line; //p指向msg的起始地址 一个字节一个字节偏移
    int flag;
    //解析cmd
    char* cmd = p;
    while(*p != ' ') p++;
    int cmd_len = p-cmd;

    char cmd_buf[16];
    memcpy(cmd_buf, cmd, cmd_len);  // 把命令拷贝出来
    cmd_buf[cmd_len] = '\0';       
    flag = handle_cmd(cmd_buf);//flag=1说明有value 为0说明无value

    //解析key
    p++;
    char* key = p;
    while(*p != ' ' ) p++;
    int key_len = p-key;

    //填入缓冲区
    int pos = 0;

    // [cmd长度][cmd]
    memcpy(packet + pos, &cmd_len, 4); pos += 4;
    memcpy(packet + pos, cmd, cmd_len); pos += cmd_len;

    // [key长度][key]
    memcpy(packet + pos, &key_len, 4); pos += 4;
    memcpy(packet + pos, key, key_len); pos += key_len;

    if(flag)
    {
        //解析value
        p++;
        char* val = p;
        while(*p != '\0') p++;
        int val_len = p-val;
        // [value长度][value]
        memcpy(packet + pos, &val_len, 4); pos += 4;
        memcpy(packet + pos, val, val_len); pos += val_len;
    }
    // 只打印有效部分！
    print_buffer(packet, pos);


    return 0;
}




int main()
{
#if 0
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd < 0)
    {
        perror("sockfd create failed\n");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2000);
    server_addr.sin_addr.s_addr = inet_addr("192.168.47.128");

    int ret = connect(sockfd,(struct sockaddr*)&server_addr,sizeof(server_addr));
    if(ret < 0)
    {
        perror("connection failed\n");
        exit(1);
    }

    printf("connection success\n");

    char buf[BUFFER_SIZE];

    while(1)
    {
        char line[BUFFER_SIZE] = {0};
        printf("请输入:\n");

        fgets(line,BUFFER_SIZE,stdin);

        // 去掉换行符（关键！否则长度多1）
        line[strcspn(line, "\n")] = 0;

        if(is_batch_mode)
        {
            if(strcmp(line,"OK")==0)
            {
                printf("批量结束 发送所有命令\n");
                char total_packet[BUFFER_SIZE*100] = {0};
                int total_len = 0;
                for(int i =0 ;i<batch_count;i++)
                {
                    int packet_len;
                    char packet_buf[BUFFER_SIZE] = {0};
                    handle_mes(line,packet_buf,&packet_len);
                    memcpy(total_packet+total_len,packet_buf,packet_len);
                    total_len+=packet_len;
                }  

                send(sockfd,total_packet,total_len,0);

                //清空
                batch_count=0;
                is_batch_mode = 0;
            }
            strcpy(batch_buf[batch_count],line);
            batch_count++;
            printf("已缓存:%s\n",line);
            continue;
        }
        if(strcmp(line,"batch")==0)
        {
            is_batch_mode = 1;
            printf("进入批量处理模式\n");
            continue;
        }

        //普通模式
        char packet[BUFFER_SIZE] = {0};
        int pkt_len;
        handle_mes(line,packet,&pkt_len);
        send(sockfd,packet,pkt_len,0);
        //接收服务端信息
        char recv_buf[1024] = {0};
        int n = recv(sockfd, recv_buf, sizeof(recv_buf), 0);
        if (n > 0) {
            printf("\n✅ 服务端回复了 %d 字节:\n", n);
            printf("recv:%s",recv_buf);
        }
        else {
            printf("服务端未回复\n");
        }
    }

    close(sockfd);
    return 0;

#endif

    char line[BUFFER_SIZE] = {0};
    printf("请输入:\n");
    fgets(line,BUFFER_SIZE,stdin);
    int packet_len;
    char packet_buf[BUFFER_SIZE] = {0};
    client_encode_resp(line,packet_buf);



}