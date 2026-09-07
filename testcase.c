

#include "src/engine/kvs_array.h"
#include "src/include/kvstore.h"
#include "src/include/protocol.h"
#include "src/utils/log.h"
#include "src/include/config.h"

#include "src/persist/persistence.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <unistd.h>

#define MAX_MSG_LENGTH		1024
#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

#define COUNT 50000

int send_msg(int connfd, char *msg) {
	char packet[1024] = {0};
	client_encode_resp(msg,packet);
	//printf("packet:%s",packet);
	int length = strlen(packet);

	int res = send(connfd, packet, length, 0);
	if (res < 0) {
		perror("send");
		exit(1);
	}
	return res;
}

int recv_msg(int connfd, char *msg, int length) {
    int res = recv(connfd, msg, length, 0); // 修正recv调用格式
    if (res < 0) {
        perror("recv");
        exit(1);
    }

    //printf("%s\n", msg); // 直接打印char*指针指向的字符串
    return res;
}




void testcase(int connfd, char *msg, char *pattern, char *casename) {

	if (!msg || !pattern || !casename) return ;

	send_msg(connfd, msg);

	char result[MAX_MSG_LENGTH] = {0};
	recv_msg(connfd, result, MAX_MSG_LENGTH);

	if (strcmp(result, pattern) == 0) 
    {

    }else 
    {
		LOG_ERROR("==> FAILED -> %s, '%s' != '%s' \n", casename, result, pattern);
		exit(1);
	}

}



int connect_tcpserver(const char *ip, unsigned short port) {

	int connfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(struct sockaddr_in));

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);
	server_addr.sin_port = htons(port);

	if (0 !=  connect(connfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in))) {
		perror("connect");
		return -1;
	}
    LOG_INFO("connect to %s:%d success\n", ip, port);
	
	return connfd;
	
}


void kvs_testcase(int connfd) {

	testcase(connfd, "SET Teacher King", "+OK\r\n", "SET-Teacher");
	testcase(connfd, "GET Teacher", "King\r\n", "GET-Teacher");
	testcase(connfd, "MOD Teacher Darren", "+OK\r\n", "MOD-Teacher");
	testcase(connfd, "GET Teacher", "Darren\r\n", "GET-Teacher");
	testcase(connfd, "EXIST Teacher", ":1\r\n", "EXIST-Teacher");
	testcase(connfd, "DEL Teacher", ":1\r\n", "DEL-Teacher");
	testcase(connfd, "GET Teacher", "$-1\r\n", "GET-Teacher");
	testcase(connfd, "MOD Teacher KING", "-ERR key not exist\r\n", "MOD-Teacher");
	testcase(connfd, "EXIST Teacher", ":0\r\n", "EXIST-Teacher");

	LOG_DEBUG("test完成\n");
    //send_msg(connfd,"SAVE");
}

void kvs_testrdb(int connfd)
{
    testcase(connfd, "SET Teacher King", "+OK\r\n", "SET-Teacher");
    LOG_DEBUG("SET完成\n");
    send_msg(connfd,"SAVE");
}

void kvs_testcase_100w(int connfd) {

	int i = 0;
    //log_set_level(1);

	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);

	for (i = 0;i < COUNT;i ++) {

		kvs_testcase(connfd);

	}

	struct timeval tv_end;
	gettimeofday(&tv_end, NULL);

	int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms

	LOG_INFO("array testcase --> time_used: %d, qps: %d\n", time_used,  (COUNT* 1000) / time_used);

}

void kvs_testcase1_100w(int connfd)
{
	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);
    
    for(int i = 0; i < COUNT; i++)
    {
        char total_key[128] = {0};
        char total_val[128] = {0};    
        char total[1024] = {0};
        char key[] = "Teacher";
        char val[] = "King";
        snprintf(total_key, sizeof(total_key), "%s%d", key, i);
        snprintf(total_val, sizeof(total_val), "%s%d", val, i);
        snprintf(total, sizeof(total), "SET %s %s", total_key, total_val);

		//printf("SET %s\n",total_key);
        testcase(connfd, total, "+OK\r\n", "SET-Teacher");
    }

	printf("11%%\n");

    for(int i = 0; i < COUNT; i++)
    {
        char total_key[128] = {0};
        char total_val[128] = {0};     
        char total[1024] = {0};
        char key[] = "Teacher";
        char val[] = "King";
        snprintf(total_key, sizeof(total_key), "%s%d", key, i);
        snprintf(total_val, sizeof(total_val), "%s%d\r\n", val, i);
        snprintf(total, sizeof(total), "GET %s", total_key);

		//printf("GET %s\n",total_key);
        testcase(connfd, total, total_val, "GET-Teacher");
    }

	printf("22%%\n");

    for(int i = 0; i < COUNT; i++)
    {
        char total_key[128] = {0};
        char total_val[128] = {0};       
        char total[1024] = {0};
        char key[] = "Teacher";
        char val[] = "Darren";
        snprintf(total_key, sizeof(total_key), "%s%d", key, i);
        snprintf(total_val, sizeof(total_val), "%s%d", val, i);
        snprintf(total, sizeof(total), "MOD %s %s", total_key, total_val);

		//printf("MOD %s\n",total_key);
        testcase(connfd, total, "+OK\r\n", "MOD-Teacher");
    }

	printf("33%%\n");

    for(int i = 0; i < COUNT; i++)
    {
        char total_key[128] = {0};
        char total_val[128] = {0};      
        char total[1024] = {0};
        char key[] = "Teacher";
        char val[] = "Darren";
        snprintf(total_key, sizeof(total_key), "%s%d", key, i);
        snprintf(total_val, sizeof(total_val), "%s%d\r\n", val, i);
        snprintf(total, sizeof(total), "GET %s", total_key);
        testcase(connfd, total, total_val, "GET-Teacher");
    }

	printf("44%%\n");


    for(int i = 0; i < COUNT; i++)
    {
        char total_key[128] = {0};
        char total_val[128] = {0};      
        char total[1024] = {0};
        char key[] = "Teacher";
        char val[] = "Darren";
        snprintf(total_key, sizeof(total_key), "%s%d", key, i);
        snprintf(total_val, sizeof(total_val), "%s%d\r\n", val, i);
        snprintf(total, sizeof(total), "EXIST %s", total_key);
        testcase(connfd, total, ":1\r\n", "EXIST-Teacher");
    }

	printf("55%%\n");

    for(int i = 0; i < COUNT; i++)
    {
        char total_key[128] = {0};
        char total_val[128] = {0};       
        char total[1024] = {0};
        char key[] = "Teacher";
        char val[] = "Darren";
        snprintf(total_key, sizeof(total_key), "%s%d", key, i);
        snprintf(total_val, sizeof(total_val), "%s%d\r\n", val, i);
        snprintf(total, sizeof(total), "DEL %s", total_key);

        testcase(connfd, total, ":1\r\n", "DEL-Teacher");
    }

	printf("66%%\n");

    for(int i = 0; i < COUNT; i++)
    {
        char total_key[128] = {0};
        char total_val[128] = {0};      
        char total[1024] = {0};
        char key[] = "Teacher";
        char val[] = "Darren";
        snprintf(total_key, sizeof(total_key), "%s%d", key, i);
        snprintf(total_val, sizeof(total_val), "%s%d\r\n", val, i);
        snprintf(total, sizeof(total), "GET %s", total_key);
        testcase(connfd, total, "$-1\r\n", "GET-Teacher");
    }

	printf("77%%\n");

    for(int i = 0; i < COUNT; i++)
    {
        char total_key[128] = {0};
        char total_val[128] = {0};      
        char total[1024] = {0};
        char key[] = "Teacher";
        char val[] = "King";
        snprintf(total_key, sizeof(total_key), "%s%d", key, i);
        snprintf(total_val, sizeof(total_val), "%s%d\r\n", val, i);
        snprintf(total, sizeof(total), "MOD %s %s", total_key, total_val);
        testcase(connfd, total, "-ERR key not exist\r\n", "MOD-Teacher");
    }

	printf("88%%\n");

    for(int i = 0; i < COUNT; i++)
    {
        char total_key[128] = {0};
        char total_val[128] = {0};      
        char total[1024] = {0};
        char key[] = "Teacher";
        char val[] = "Darren";
        snprintf(total_key, sizeof(total_key), "%s%d", key, i);
        snprintf(total_val, sizeof(total_val), "%s%d\r\n", val, i);
        snprintf(total, sizeof(total), "EXIST %s", total_key);
        testcase(connfd, total, ":0\r\n", "EXIST-Teacher");
    }

	printf("100%%\n");

	struct timeval tv_end;
	gettimeofday(&tv_end, NULL);

	int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms

	printf("testcase --> time_used: %d, qps: %d\n", time_used,  (COUNT* 9000) / time_used);

    
}

void persistence_testcase(int connfd)
{	
    
    for(int i = 0; i < COUNT; i++)
    {
        char total_key[128] = {0};
        char total_val[128] = {0};    
        char total[1024] = {0};
        char key[] = "Teacher";
        char val[] = "King";
        snprintf(total_key, sizeof(total_key), "%s%d", key, i);
        snprintf(total_val, sizeof(total_val), "%s%d", val, i);
        snprintf(total, sizeof(total), "SET %s %s", total_key, total_val);

        testcase(connfd, total, "+OK\r\n", "SET-Teacher");
    }
    

}

void MS_testcase(int connfd)
{	
    
    for(int i = 50000; i < COUNT+50000; i++)
    {
        char total_key[128] = {0};
        char total_val[128] = {0};    
        char total[1024] = {0};
        char key[] = "Teacher";
        char val[] = "King";
        snprintf(total_key, sizeof(total_key), "%s%d", key, i);
        snprintf(total_val, sizeof(total_val), "%s%d", val, i);
        snprintf(total, sizeof(total), "SET %s %s", total_key, total_val);

        testcase(connfd, total, "+OK\r\n", "SET-Teacher");
    }
    

}


// testcase 192.168.254.100  2000
int main(int argc, char *argv[]) {

    char *ip = "192.168.254.100";
    int master_port = 2000;
    int slave_port = 3000;

    int mode = atoi(argv[1]);

    //1.全量持久化演示
    if(mode == 1) 
    {
        int fd = connect_tcpserver(ip, master_port);
        if(fd < 0) 
        {
            LOG_ERROR("连接主节点失败\n");
            return -1;
        }
        persistence_testcase(fd);
        send_msg(fd, "SAVE");

        sleep(2);
        LOG_INFO("RDB数据保存完成 请重启服务端");
        getchar();
        close(fd);
    }
    if(mode ==2)
    {
        int master_fd = connect_tcpserver(ip, master_port);
        if (master_fd < 0) {
            LOG_ERROR("重启后无法连接主节点\n");
            return -1;
        }
        int miss = 0;
        for (int i = 0; i < COUNT; i++) 
        {
            char buf[128]={0}, resp[128]={0}, expected[128]={0};
            sprintf(buf, "GET Teacher%d", i);
            send_msg(master_fd, buf);
            recv_msg(master_fd, resp, sizeof(resp));
            sprintf(expected, "King%d", i);
            // 去除 \r\n
            LOG_DEBUG("resp: %s, expected: %s\n", resp, expected);
            int len = strlen(resp);
            if (len >= 2 && resp[len-2] == '\r' && resp[len-1] == '\n')
                resp[len-2] = '\0';
            if (strcmp(resp, expected) != 0) miss++;
        }
        close(master_fd);
        LOG_DEBUG("RDB 恢复校验：缺失/错误 %d 条\n", miss);
    }

    //2.增量持久化演示
    if(mode == 3)
    {
        int fd = connect_tcpserver(ip, master_port);
        if(fd < 0) 
        {
            LOG_ERROR("连接主节点失败\n");
            return -1;
        }
        persistence_testcase(fd);

        sleep(2);
        LOG_INFO("AOF数据保存完成 请重启服务端");
        getchar();
        close(fd);
    }
    
    if(mode == 4)
    {
        int master_fd = connect_tcpserver(ip, master_port);
        if (master_fd < 0) 
        {
            LOG_ERROR("重启后无法连接主节点\n");
            return -1;
        }
        int miss = 0;
        for (int i = 0; i < COUNT; i++) 
        {
            char buf[128]={0}, resp[128]={0}, expected[128]={0};
            sprintf(buf, "GET Teacher%d", i);
            send_msg(master_fd, buf);
            recv_msg(master_fd, resp, sizeof(resp));
            sprintf(expected, "King%d", i);
            // 去除 \r\n
            LOG_DEBUG("resp: %s, expected: %s\n", resp, expected);
            int len = strlen(resp);
            if (len >= 2 && resp[len-2] == '\r' && resp[len-1] == '\n')
                resp[len-2] = '\0';
            if (strcmp(resp, expected) != 0) miss++;
        }
        close(master_fd);
        LOG_DEBUG("AOF 恢复校验：缺失/错误 %d 条\n", miss);
    }

    if(mode == 5)
    {
        // ========== 主从同步测试 ==========
        printf("=== 主从同步测试开始 ===\n");
        printf("请确保主节点已启动，从节点尚未启动。\n");

        // 阶段1：向主节点插入前 5w 条
        int master_fd = connect_tcpserver(ip, master_port);
        if (master_fd < 0) {
            LOG_ERROR("连接主节点失败\n");
            return -1;
        }
        printf("阶段1:向主节点插入 %d 条数据...\n", COUNT);
        persistence_testcase(master_fd);
        close(master_fd);
        printf("阶段1完成。\n");

        // 提示启动从节点
        printf("请启动从节点（例如：./kvstore config_slave.conf),然后按任意键继续...\n");
        getchar();  // 等待用户按键

        // 阶段2：向主节点插入后 5w 条
        master_fd = connect_tcpserver(ip, master_port);
        if (master_fd < 0) {
            LOG_ERROR("重新连接主节点失败\n");
            return -1;
        }
        printf("阶段2:向主节点再插入 %d 条数据...\n", COUNT);
        MS_testcase(master_fd);
        close(master_fd);
        printf("阶段2完成。\n");

        // 等待从节点同步（根据你的网络状况，建议 3~5 秒）
        printf("等待从节点同步(5秒)...\n");
        sleep(5);

        // 阶段3：从从节点读取全部 10w 条数据并比对
        int slave_fd = connect_tcpserver(ip, slave_port);
        if (slave_fd < 0) {
            LOG_ERROR("连接从节点失败\n");
            return -1;
        }
        printf("阶段3:从从节点读取 %d 条数据并比对...\n", COUNT * 2);
        int miss = 0;
        for (int i = 0; i < COUNT * 2; i++) {
            char cmd[128]={0}, expected[128]={0}, resp[128]={0};
            sprintf(cmd, "GET Teacher%d", i);
            send_msg(slave_fd, cmd);
            recv_msg(slave_fd, resp, sizeof(resp));
            // 去除 RESP 返回值的 \r\n 尾部
            int len = strlen(resp);
            if (len >= 2 && resp[len-2] == '\r' && resp[len-1] == '\n')
                resp[len-2] = '\0';
            sprintf(expected, "King%d", i);
            if (strcmp(resp, expected) != 0) {
                miss++;
                if (miss < 10) {  // 只打印前10个错误，避免刷屏
                    LOG_INFO("key%d 期望 '%s'，实际 '%s'\n", i, expected, resp);
                }
            }
            if ((i+1) % 10000 == 0) {
                printf("已校验 %d 条，缺失/错误 %d 条\n", i+1, miss);
            }
        }
        close(slave_fd);

        printf("=== 主从同步测试结束 ===\n");
        printf("总数据条数：%d,缺失/错误条数：%d\n", COUNT * 2, miss);
        if (miss == 0) {
            printf("✅ 主从同步测试通过！\n");
        } else {
            printf("❌ 主从同步测试失败！\n");
        }
    }
    

	return 0;
	
}



