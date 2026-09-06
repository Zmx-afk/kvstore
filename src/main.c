
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "include/kvstore.h"
#include "utils/timer.h"
#include "include/config.h"
#include "utils/log.h"

#include "engine/kvs_array.h"
#include "persist/persistence.h"

/*
    argv[0] = ./kvstore argv[1] = port argv[2] = master/slave
*/
int main(int argc, char *argv[]) 
{
    const char *config_file = "config.conf";
    if(argc > 1) 
    {
        if(strcmp(argv[1], "1") == 0) 
        {
            config_file = "config_slave.conf";
        } 
    }
    LOG_INFO("argv[0]: %s,argv[1]: %s\n", argv[0], argv[1]);
    LOG_INFO("使用配置文件: %s\n", config_file);
	// 加载配置文件
    if (load_config(config_file) != 0) {
        LOG_ERROR("加载配置文件失败，使用默认配置\n");
        return -1;
    }

    int port = g_config.port;

    log_set_level(g_config.loglevel);

    if (init_kvengine() != 0) 
    {
        LOG_ERROR("引擎初始化失败，退出\n");
        return -1;
    }
    
    
    if(strcmp(g_config.persistence,"aof")==0)
    {
        AOF_restore();
    }
    else if(strcmp(g_config.persistence,"rdb")==0)
    {
        kvs_array_t *current = (kvs_array_t*)g_engine.impl;
        if (!current) 
        { 
            LOG_ERROR("引擎初始化失败\n"); 
            return -1; 
        }
        int ret = RDB_load(current);
        if (ret == 0) 
        {
            LOG_INFO("RDB 加载成功，条目数：%d\n", current->total);
        } else 
        {
            LOG_ERROR("RDB 加载失败\n");
        }
    }
	
    
    //timer_init();

    if (g_config.role == ROLE_MASTER) 
    {
        pthread_t tid;
        pthread_create(&tid, NULL, master_accept_slave, NULL);
        pthread_detach(tid);
    }

    if (g_config.role == ROLE_SLAVE) 
    {   
        LOG_DEBUG("调用slave_run\n");
        pthread_t tid;
        pthread_create(&tid, NULL, (void*)slave_run, NULL);
        pthread_detach(tid);
    }

#if (NETWORK_SELECT == NETWORK_REACTOR)
	reactor_start(port, kvs_protocol);
#elif (NETWORK_SELECT == NETWORK_PROACTOR)
	ntyco_start(port, kvs_protocol);
#elif (NETWORK_SELECT == NETWORK_NTYCO)
	proactor_start(port, kvs_protocol);
#endif

	dest_kvengine();
    close(g_master_fd);
    close(g_slave_fd);
	return 0;
}


