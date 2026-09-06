#include "src/include/config.h"
#include "src/include/kvstore.h"
#include "src/utils/log.h"
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>



void *master_accept_slave(void *arg) {
    int master_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (master_fd < 0) {
        perror("socket failed");
        pthread_exit(NULL);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr)); // 必须初始化，防止脏数据
    addr.sin_family = AF_INET;
    addr.sin_port = htons(2020); 
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int reuse = 1;
    if (setsockopt(master_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt failed");
        close(master_fd);
        pthread_exit(NULL);
    }

    // 处理 bind 返回值，消除警告 + 防止继续执行
    if (bind(master_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(master_fd);
        pthread_exit(NULL);
    }

    // 处理 listen 返回值
    if (listen(master_fd, 1) < 0) {
        perror("listen failed");
        close(master_fd);
        pthread_exit(NULL);
    }

    LOG_DEBUG("主节点等待从节点连接...\n");
    g_slave_fd = accept(master_fd, NULL, NULL);
    if (g_slave_fd < 0) {
        perror("accept failed");
        close(master_fd);
        pthread_exit(NULL);
    }
    LOG_INFO("从节点已连接主节点!g_slave_fd=%d\n", g_slave_fd);

    close(master_fd); // 监听 fd 用完可以关闭，不影响已建立的连接
    return NULL;
}

void master_sync(char *data, int len) {
    if (g_role == ROLE_MASTER && g_slave_fd > 0) {
        send(g_slave_fd, data, len, 0);
        LOG_INFO("主节点同步命令到从节点\n");
    }
}


void slave_run() 
{
    //1.创建socket连接主节点
    g_master_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(g_master_port);
    addr.sin_addr.s_addr = inet_addr(g_master_ip);

    if (connect(g_master_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
    {
        LOG_ERROR("从节点连接主节点失败");
        return;
    }
    LOG_DEBUG("从节点已连接主节点，等待同步\n");

    //2.发送SYNC命令请求同步
    char *sync_cmd = "*1\r\n$4\r\nSYNC\r\n";
    send(g_master_fd, sync_cmd, strlen(sync_cmd), 0);

    
    
}