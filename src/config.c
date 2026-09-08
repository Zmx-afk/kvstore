#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "include/config.h"
#include "include/kvstore.h"
#include "utils/log.h"

ServerConfig g_config = {0};  // 全局配置变量


// 去除字符串首尾空格
static void trim(char *str) {
    char *start = str;
    while (*start == ' ' || *start == '\t') start++;
    char *end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    *(end + 1) = '\0';
    // 如果 start 不是原 str，需要将内容前移
    if (start != str) {
        memmove(str, start, end - start + 2);
    }
}

int load_config(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen config file");
        return -1;
    }

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        // 跳过空行和注释（以 # 开头）
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;

        // 找分隔符 '='
        char *delim = strchr(line, '=');
        if (!delim) continue;

        *delim = '\0';  // 把 '=' 替换成 '\0'，左边就是 key
        char *key = line;
        char *val = delim + 1;

        trim(key);
        trim(val);
        // printf("key:%s val:%s\n",key,val);

        // 按 key 赋值到全局配置
        if (strcmp(key, "ip") == 0) 
        {
            strncpy(g_config.ip, val, MAX_IP_LEN - 1);
            LOG_DEBUG("ip读取成功");
        } 
        else if (strcmp(key, "port") == 0) 
        {
            g_config.port = atoi(val);
            LOG_DEBUG("port读取成功");
        } 
        else if (strcmp(key, "role") == 0) 
        {
            if(strcmp(val, "master") == 0) 
            {
                g_config.role = ROLE_MASTER;
            } 
            else if(strcmp(val, "slave") == 0) 
            {
                g_config.role = ROLE_SLAVE;
            } 
            else
            {
                LOG_ERROR("Invalid role: %s, defaulting to master\n", val);
                g_config.role = ROLE_MASTER; // 默认值
            }
            LOG_DEBUG("role读取成功:%d",g_config.role);
        } 
        else if (strcmp(key, "engine") == 0) 
        {
            strncpy(g_config.engine, val, sizeof(g_config.engine) - 1);
            g_config.engine[sizeof(g_config.engine)-1]='\0';
            LOG_DEBUG("engine读取成功:%s",g_config.engine);
        } 
        else if (strcmp(key, "loglevel") == 0) 
        {
            if(strcmp(val,"DEBUG")==0)
            {
                g_config.loglevel = LOG_DEBUG;
            }
            else if(strcmp(val,"INFO")==0)
            {
                g_config.loglevel = LOG_INFO;
            }
            else if(strcmp(val,"WARN")==0)
            {
                g_config.loglevel = LOG_WARN;
            }
            else if(strcmp(val,"ERROR")==0)
            {
                g_config.loglevel = LOG_ERROR;
            }
            else
            {
                LOG_ERROR("Invalid loglevel: %s, defaulting to DEBUG", val);
                g_config.loglevel = LOG_DEBUG;
            }
            LOG_DEBUG("loglevel读取成功:%d", g_config.loglevel);
        }
        else if (strcmp(key, "rdb_enable") == 0) 
        {
            g_config.rdb_enable = atoi(val);
            LOG_DEBUG("rdb_enable读取成功:%d", g_config.rdb_enable);
        }
        else if (strcmp(key, "aof_strategy") == 0) 
        {
            if(strcmp(val, "always") == 0) {
                g_config.aof_strategy = AOF_ALWAYS;
            } 
            else if(strcmp(val, "everysec") == 0) {
                g_config.aof_strategy = AOF_EVERYSEC;
            }
            else if(strcmp(val,"no")==0)
            {
                g_config.aof_strategy = AOF_NO;
            }
            else 
            {
                LOG_ERROR("Invalid value for aof_strategy: %s\n", val);
                g_config.aof_strategy = AOF_ALWAYS; // 默认值
            }
            LOG_DEBUG("aof_strategy读取成功:%d", g_config.aof_strategy);
        } 
        else if (strcmp(key, "master_ip") == 0) 
        {
            strncpy(g_config.master_ip, val, MAX_IP_LEN - 1);
        } else if (strcmp(key, "master_port") == 0) 
        {
            g_config.master_port = atoi(val);
        }

    }



    fclose(fp);
    return 0;
}