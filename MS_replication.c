#include "src/include/config.h"
#include "src/include/kvstore.h"
#include "src/utils/log.h"
#include <sys/socket.h>
#include <errno.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


#include "src/persist/persistence.h"

static int recv_exact(int fd, void *buf, size_t len) {
    size_t remain = len;
    char *p = (char *)buf;
    while (remain > 0) {
        ssize_t n = recv(fd, p, remain, 0);
        if (n == 0) return -1;
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += n;
        remain -= (size_t)n;
    }
    return 0;
}

static int send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t offset = 0;
    while (offset < len) {
        ssize_t n = send(fd, p + offset, len - offset, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        offset += (size_t)n;
    }
    return 0;
}

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
    if (send_all(slave_fd, &net_size, sizeof(net_size)) != 0) 
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
        if (send_all(slave_fd, buffer, bytes_read) != 0) {
            LOG_ERROR("发送 RDB 数据中断，已发送 %ld / %ld 字节\n", total_sent, file_size);
            fclose(fp);
            return -1;
        }
        total_sent += bytes_read;
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
        uint32_t net_len = htonl((uint32_t)len);
        if (send_all(g_slave_fd, &net_len, sizeof(net_len)) != 0 ||
            send_all(g_slave_fd, data, (size_t)len) != 0) {
            LOG_ERROR("主节点同步命令失败，预期 %d 字节,fd=%d\n", len, g_slave_fd);
            // 断开连接，避免继续使用无效 fd
            close(g_slave_fd);
            g_slave_fd = -1;
        } else {
            LOG_DEBUG("主节点同步命令成功，发送 %d 字节\n", len);
        }
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
    const char *sync_cmd = "*1\r\n$4\r\nSYNC\r\n";
    if (send_all(g_master_fd, sync_cmd, strlen(sync_cmd)) != 0) {
        LOG_ERROR("发送 SYNC 命令失败\n");
        close(g_master_fd);
        return;
    }
    LOG_DEBUG("从节点发送SYNC命令请求同步\n");

    //3.接收主节点发送的RDB数据并加载到本地
    //3.1先接收RDB文件长度
    uint32_t net_size = 0;
    if (recv_exact(g_master_fd, &net_size, sizeof(net_size)) != 0) {
        LOG_ERROR("接收 RDB 长度失败\n");
        return;
    }
    LOG_INFO("接收到 RDB 文件长度: %u 字节\n", ntohl(net_size));
    uint32_t file_size = ntohl(net_size);

    //循环接收数据 写入临时rdb文件
    FILE *fp = fopen("temp.rdb", "wb");
    if (!fp) {
        LOG_ERROR("创建 temp.rdb 失败\n");
        return;
    }
    char buf[8192];
    size_t remain = file_size;
    while (remain > 0) {
        int to_read = (remain < sizeof(buf)) ? remain : sizeof(buf);
        if (recv_exact(g_master_fd, buf, (size_t)to_read) != 0) {
            break;
        }
        if (fwrite(buf, 1, (size_t)to_read, fp) != (size_t)to_read) {
            remain = 1;
            break;
        }
        remain -= (size_t)to_read;
    }
    fclose(fp);

    if (remain > 0) 
    {
        LOG_ERROR("RDB 接收不完整，缺失 %zu 字节\n", remain);
        return;
    }
    LOG_INFO("RDB 文件接收完成，大小: %u 字节\n", file_size);

    // 先把刚接收到的快照落盘到正式 RDB 文件，再加载，避免加载旧的 RDB.rdb
    if (rename("temp.rdb", "RDB.rdb") != 0) {
        LOG_WARN("临时快照转正式 RDB 失败，尝试直接加载 temp.rdb\n");
    }

    g_engine.destroy(g_engine.impl);
    g_engine.impl = g_engine.create();
    RDB_load((kvs_array_t*)g_engine.impl);
    
    // 每条增量命令使用长度前缀，避免 TCP 粘包和拆包问题。
    while (1) {
        uint32_t net_len = 0;
        if (recv_exact(g_master_fd, &net_len, sizeof(net_len)) != 0) {
            break;
        }

        uint32_t command_len = ntohl(net_len);
        if (command_len == 0 || command_len > 65536) {
            LOG_ERROR("收到非法增量命令长度: %u\n", command_len);
            break;
        }

        char *command = malloc(command_len);
        if (!command) {
            LOG_ERROR("增量命令内存分配失败，长度: %u\n", command_len);
            break;
        }
        if (recv_exact(g_master_fd, command, command_len) != 0) {
            free(command);
            break;
        }

        g_is_sync = 1;
        char response[1024] = {0};
        int sync_flag = 1;
        kvs_protocol(command, (int)command_len, response, &sync_flag);
        g_is_sync = 0;
        free(command);
    }
    
}