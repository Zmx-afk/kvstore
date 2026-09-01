#ifndef CONFIG_H
#define CONFIG_H

#define MAX_IP_LEN 64
#define MAX_LINE_LEN 256

#include <stdio.h>  // 需要 printf




typedef struct{
    char ip[MAX_IP_LEN];
    int port;
    char role[16];
    char engine[16];
    char persistence[16];
}ServerConfig;




extern ServerConfig g_config;

int load_config(const char *filename);



#endif