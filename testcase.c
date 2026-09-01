

#include "src/engine/kvs_array.h"
#include "src/include/kvstore.h"
#include "src/include/protocol.h"
#include "src/utils/log.h"

#include "src/persist/persistence.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>


#define MAX_MSG_LENGTH		1024
#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)


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

	if (strcmp(result, pattern) == 0) {
		//printf("==> PASS ->  %s\n", casename);
	} else {
		printf("==> FAILED -> %s, '%s' != '%s' \n", casename, result, pattern);
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
    send_msg(connfd,"SAVE");
}

void kvs_testcase_100w(int connfd) {

	int count = 10000;
	int i = 0;
    //log_set_level(1);

	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);

	for (i = 0;i < count;i ++) {

		kvs_testcase(connfd);

	}

	struct timeval tv_end;
	gettimeofday(&tv_end, NULL);

	int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms

	LOG_INFO("array testcase --> time_used: %d, qps: %d\n", time_used,  (count* 1000) / time_used);

}

void kvs_testcase1_100w(int connfd)
{
    int count = 10;
	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);
    
    for(int i = 0; i < count; i++)
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

    for(int i = 0; i < count; i++)
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

    for(int i = 0; i < count; i++)
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

    for(int i = 0; i < count; i++)
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

    for(int i = 0; i < count; i++)
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

    for(int i = 0; i < count; i++)
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

    for(int i = 0; i < count; i++)
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

    for(int i = 0; i < count; i++)
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

    for(int i = 0; i < count; i++)
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

	printf("testcase --> time_used: %d, qps: %d\n", time_used,  (count* 9000) / time_used);

    send_msg(connfd,"SAVE");
}


// testcase 192.168.254.100  2000
int main(int argc, char *argv[]) {

#if 0
	if (argc != 4) {
		printf("arg error\n");
		return -1;
	}

	char *ip = argv[1];
	int port = atoi(argv[2]);
	int mode = atoi(argv[3]);

	int connfd = connect_tcpserver(ip, port);

	if (mode == 0) {
		rbtree_testcase_1w(connfd);
	} else if (mode == 1) {
		rbtree_testcase_3w(connfd);
	} else if (mode == 2) {
		array_testcase_1w(connfd);
	} else if (mode == 3) {
		hash_testcase(connfd);
	}
#elif 1
	char *ip = "192.168.254.100";
	int port = atoi("2000");
	int connfd = connect_tcpserver(ip, port);
	if (connfd < 0) {
    	printf("连接服务器失败,请检查服务端是否启动,IP/端口是否正确。\n");
    	return -1;
	}
	//kvs_testcase(connfd);
	//kvs_testcase_100w(connfd);
	//kvs_testcase1_100w(connfd);
    init_kvengine();
    kvs_array_t *current = (kvs_array_t*)g_engine.impl;
   
    if(!current)
    {
        LOG_ERROR("current create fail\n");
        return -1;
    }

    RDB_load(current);

    LOG_DEBUG("klen:%#x,kdata:%#x,vlen:%#x,vdata:%#x\n",current->table[0].key.len,current->table[0].key.data,current->table[0].value.len,current->table[0].value.data);

#endif
	// char packet[128] = {0};
	// client_encode_resp("SET king wang",packet);
	
	// char original[128] = {0};
	// server_decode_resp(packet,original);
	// printf("original:%s\n",original);


	return 0;
	
}



