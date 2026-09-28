
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "include/kvstore.h"
#include "utils/timer.h"
#include "include/config.h"
#include "utils/log.h"

#include "engine/engine.h"
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
    
    if(g_config.role == ROLE_MASTER && (g_config.aof_strategy == AOF_ALWAYS || g_config.aof_strategy == AOF_EVERYSEC))
    {
        AOF_init();
        AOF_restore();
    }
    else if(g_config.role== ROLE_MASTER && g_config.rdb_enable == 1)
    {
        engine_t *current = g_engine;
        if (!current) 
        { 
            LOG_ERROR("引擎初始化失败\n"); 
            return -1; 
        }
        int ret = RDB_load(current);
        printf("RDB_load完成\n");
        if (ret == 0) 
        {
            LOG_INFO("RDB 加载成功，条目数：%d\n", current->total);
            int count = 0;
            // while(current->total > 0&& count < current->total) 
            // {
            //     LOG_DEBUG("key: %.*s, value: %.*s\n", current->table[count].key.len, (char*)current->table[count].key.data, current->table[count].value.len, (char*)current->table[count].value.data);
            //     count++;
            // }
        } else 
        {
            LOG_ERROR("RDB 加载失败");
        }
    }
	
    
    //timer_init();


    if (g_config.role == ROLE_SLAVE) 
    {   
        LOG_DEBUG("调用slave_run\n");
        pthread_t tid;
        pthread_create(&tid, NULL, (void*)slave_run, NULL);
        pthread_detach(tid);
    }

#if (NETWORK_SELECT == NETWORK_REACTOR)
	reactor_start(port, kvs_protocol);
#elif (NETWORK_SELECT == NETWORK_NTYCO)
	ntyco_start(port, kvs_protocol);
#elif (NETWORK_SELECT == NETWORK_PROACTOR)
	proactor_start(port, kvs_protocol);
#endif

    AOF_destroy();
	dest_kvengine();
    close(g_master_fd);
    close(g_slave_fd);
	return 0;
}


