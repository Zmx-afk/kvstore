#ifndef CONFIG_H
#define CONFIG_H

#include <sys/types.h>
#define MAX_IP_LEN 64
#define MAX_LINE_LEN 256

#include <stdio.h>  // 需要 printf
#include <stdint.h>


typedef enum {
    AOF_ALWAYS = 0,
    AOF_EVERYSEC=1,
} AOF_FSYNC;

typedef struct{
    char ip[MAX_IP_LEN];
    int port;
    
    // 0: master, 1: slave
    uint8_t role; 
    char engine[16];
    uint8_t loglevel;
    char persistence[16];
    char master_ip[MAX_IP_LEN];
    int master_port;
    uint8_t aof_fsync;
}ServerConfig;




extern ServerConfig g_config;

int load_config(const char *filename);



#endif