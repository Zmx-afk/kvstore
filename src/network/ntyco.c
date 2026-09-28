



#include "../../NtyCo/core/nty_coroutine.h"

#include <arpa/inet.h>
#include <stdio.h>
#include "../include/kvstore.h"

static msg_handler kvs_handler;


#define BUFFER_LENGTH 1024
void server_reader(void *arg) {
    int fd = *(int *)arg;
    free(arg);   /* ★ 记得 free，之前 malloc 出来的 */

    /* ★ 累积缓冲区 */
    char rbuf[BUFFER_LENGTH * 4];   /* 4KB */
    int rlen = 0;

    while (1) {
        /* 1. 从 rlen 位置继续接收 */
        int n = recv(fd, rbuf + rlen, sizeof(rbuf) - rlen, 0);
        if (n <= 0) {
            close(fd);
            break;
        }
        rlen += n;

        /* 2. 一次性交给 kvs_protocol，它内部循环解析所有完整命令 */
        char response[BUFFER_LENGTH * 4] = {0};
        int consumed = 0;
		int is_sync = 0; 
        int resp_len = kvs_handler(rbuf, rlen, response, &is_sync, &consumed);

        /* 3. 发送响应 */
        if (resp_len > 0) {
            send(fd, response, resp_len, 0);
        }

        /* 4. 根据 consumed 移动缓冲区 */
        if (consumed > 0) {
            memmove(rbuf, rbuf + consumed, rlen - consumed);
            rlen -= consumed;
        }

        /* 5. 缓冲区满但还没解析出完整命令 → 异常 */
        if (rlen == sizeof(rbuf)) {
            close(fd);
            break;
        }
    }
}


void server(void *arg) {

	unsigned short port = *(unsigned short *)arg;

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) return ;

	struct sockaddr_in local, remote;
	local.sin_family = AF_INET;
	local.sin_port = htons(port);
	local.sin_addr.s_addr = INADDR_ANY;
	if(bind(fd, (struct sockaddr*)&local, sizeof(struct sockaddr_in))==-1)
	{
		perror("bind failed\n");
		return;
	}

	listen(fd, 20);



	while (1) {
		socklen_t len = sizeof(struct sockaddr_in);
		int cli_fd = accept(fd, (struct sockaddr*)&remote, &len);
		

		nty_coroutine *read_co;
		int *pfd = malloc(sizeof(int));
		*pfd = cli_fd;
		nty_coroutine_create(&read_co, server_reader, pfd);

	}
	
}





int ntyco_start(unsigned short port, msg_handler handler) {

	//int port = atoi(argv[1]);
	kvs_handler = handler;

	
	nty_coroutine *co = NULL;
	nty_coroutine_create(&co, server, &port);

	nty_schedule_run();

}




