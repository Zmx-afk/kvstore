
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "include/kvstore.h"
#include "utils/timer.h"
#include "include/config.h"





/*
    argv[0] = ./kvstore argv[1] = port argv[2] = master/slave
*/
int main(int argc, char *argv[]) {
	// 加载配置文件
    if (load_config("config.conf") != 0) {
        printf("加载配置文件失败，使用默认配置\n");
        return -1;
        // // 你可以设置默认值
        // g_config.port = 2000;
        // strcpy(g_config.role, "master");
        // strcpy(g_config.ip, "0.0.0.0");
    }

	// 用 g_config.port, g_config.role 等替换原来的硬编码
    int port = g_config.port;
    if (strcmp(g_config.role, "slave") == 0) {
        g_role = ROLE_SLAVE;
    } else {
        g_role = ROLE_MASTER;
    }

	init_kvengine();
    if (init_kvengine() != 0) 
    {
        printf("引擎初始化失败，退出\n");
        return -1;
    }
    
    
    //AOF_restore();
	
    
    timer_init();

    if (g_role == ROLE_MASTER) {
        pthread_t tid;
        pthread_create(&tid, NULL, master_accept_slave, NULL);
        pthread_detach(tid);
    }

    if (g_role == ROLE_SLAVE) {
        pthread_t tid;
        pthread_create(&tid, NULL, (void*)slave_run, NULL);
        pthread_detach(tid);
    }

    printf("OK\n");
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


