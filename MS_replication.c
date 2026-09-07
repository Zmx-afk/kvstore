#include "src/include/config.h"
#include "src/include/kvstore.h"
#include "src/utils/log.h"
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


#include "src/persist/persistence.h"
/**
 * 发送 RDB 文件给从节点
 * @param slave_fd 从节点的 socket fd
 * @return 0 成功, -1 失败
 */
int send_rdb_to_slave(int slave_fd) 
{
    const char *rdb_path = "RDB.rdb";
    FILE *fp = fopen(rdb_path, "rb");
    if (!fp) 
    {
        LOG_ERROR("RDB文件打开失败");
        return -1;
    }

    //1.获取文件大小
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    if(file_size < 0) 
    {
        LOG_ERROR("RDB文件获取大小失败");
        fclose(fp);
        return -1;
    }
    else if(file_size == 0) 
    {
        LOG_WARN("RDB文件为空 即将发送空文件");
    }

    //2.发送文件大小
    uint32_t net_size = htonl((uint32_t)file_size);
    if (send(slave_fd, &net_size, sizeof(net_size), 0) != sizeof(net_size)) 
    {
        LOG_ERROR("发送 RDB 文件长度失败\n");
        fclose(fp);
        return -1;
    }

    // 3. 循环发送文件内容
    char buffer[8192];
    size_t bytes_read;
    long total_sent = 0;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        ssize_t sent = send(slave_fd, buffer, bytes_read, 0);
        if (sent != (ssize_t)bytes_read) {
            LOG_ERROR("发送 RDB 数据中断，已发送 %ld / %ld 字节\n", total_sent + sent, file_size);
            fclose(fp);
            return -1;
        }
        total_sent += sent;
    }

    fclose(fp);
    LOG_INFO("RDB 文件发送完成，大小: %ld 字节\n", file_size);

    return 0;
}   


// void *master_accept_slave(void *arg) {
//     int master_fd = socket(AF_INET, SOCK_STREAM, 0);
//     if (master_fd < 0) {
//         perror("socket failed");
//         pthread_exit(NULL);
//     }

//     struct sockaddr_in addr;
//     memset(&addr, 0, sizeof(addr)); // 必须初始化，防止脏数据
//     addr.sin_family = AF_INET;
//     addr.sin_port = htons(2020); 
//     addr.sin_addr.s_addr = htonl(INADDR_ANY);

//     int reuse = 1;
//     if (setsockopt(master_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
//         perror("setsockopt failed");
//         close(master_fd);
//         pthread_exit(NULL);
//     }

//     // 处理 bind 返回值，消除警告 + 防止继续执行
//     if (bind(master_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
//         perror("bind failed");
//         close(master_fd);
//         pthread_exit(NULL);
//     }

//     // 处理 listen 返回值
//     if (listen(master_fd, 1) < 0) {
//         perror("listen failed");
//         close(master_fd);
//         pthread_exit(NULL);
//     }

//     LOG_DEBUG("主节点等待从节点连接...\n");
//     g_slave_fd = accept(master_fd, NULL, NULL);
//     if (g_slave_fd < 0) {
//         perror("accept failed");
//         close(master_fd);
//         pthread_exit(NULL);
//     }
//     LOG_INFO("从节点已连接主节点!g_slave_fd=%d\n", g_slave_fd);

//     close(master_fd); // 监听 fd 用完可以关闭，不影响已建立的连接
//     return NULL;
// }

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
    addr.sin_port = htons(g_config.master_port);
    addr.sin_addr.s_addr = inet_addr(g_config.master_ip);

    if (connect(g_master_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
    {
        LOG_ERROR("从节点连接主节点失败");
        return;
    }
    LOG_DEBUG("从节点已连接主节点，等待同步\n");

    //2.发送SYNC命令请求同步
    char *sync_cmd = "*1\r\n$4\r\nSYNC\r\n";
    send(g_master_fd, sync_cmd, strlen(sync_cmd), 0);
    LOG_DEBUG("从节点发送SYNC命令请求同步\n");

    //3.接收主节点发送的RDB数据并加载到本地
    //3.1先接收RDB文件长度
    uint32_t net_size;
    if (recv(g_master_fd, &net_size, 4, 0) != 4) {
        LOG_ERROR("接收 RDB 长度失败\n");
        return;
    }
    uint32_t file_size = ntohl(net_size);

    //循环接收数据 写入临时rdb文件
    FILE *fp = fopen("temp.rdb", "wb");
    char buf[8192];
    size_t remain = file_size;
    while (remain > 0) {
        int to_read = (remain < sizeof(buf)) ? remain : sizeof(buf);
        int n = recv(g_master_fd, buf, to_read, 0);
        if (n <= 0) break;
        fwrite(buf, 1, n, fp);
        remain -= n;
    }
    fclose(fp);

    if (remain > 0) 
    {
        LOG_ERROR("RDB 接收不完整，缺失 %zu 字节\n", remain);
        return;
    }
    LOG_INFO("RDB 文件接收完成，大小: %u 字节\n", file_size);

    g_engine.destroy(g_engine.impl);
    g_engine.impl = g_engine.create();
    RDB_load((kvs_array_t*)g_engine.impl);
    
    //增量持久化部分
    while (1) 
    {
        char cmd_buf[1024] = {0};
        int n = recv(g_master_fd, cmd_buf, sizeof(cmd_buf), 0);
        if (n <= 0) break;
        
        g_is_sync = 1;   // 关键！告诉业务层这是同步来的命令
        char response[1024] = {0};

        int sync_flag = 1;
        kvs_protocol(cmd_buf, n, response,&sync_flag);  // 执行命令
        g_is_sync = 0;
    }
    
}