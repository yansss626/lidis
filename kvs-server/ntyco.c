



#include "nty_coroutine.h"

#include <arpa/inet.h>

#define BUFFER_SIZE 4096
typedef int (*msg_handler)(char *msg, int length, char *response);
static msg_handler kvs_handler;

typedef struct client_info_s{
	int fd;
	char * rbuf;
	int capacity;
	int read_pos;

}client_info;

int kvs_recv_protocol(client_info * cli_info, int * head_length){
	if(cli_info == NULL) return -1;
	int data_length = 0;
	int protocol_length = 0;

	while(protocol_length < cli_info->read_pos){
		if(cli_info->rbuf[protocol_length] == '\r'){
			cli_info->rbuf[protocol_length] = '\0';
			data_length = atoi(cli_info->rbuf);
			cli_info->rbuf[protocol_length] = '\r';
			break;
		}
		protocol_length++;
	}
	if(protocol_length == cli_info->read_pos) return 0;
	(*head_length) = protocol_length + 1;
	int total_length = protocol_length + data_length + 1;
	if(total_length > cli_info->capacity){
		cli_info->rbuf = (char *)realloc(cli_info->rbuf, total_length + 1);
		cli_info->capacity = total_length;
	}

	return total_length;

}

// void server_reader(void *arg) {
// 	int fd = *(int *)arg;
// 	int ret = 0;

 
// 	while (1) {
		
// 		char buf[1024] = {0};
// 		ret = recv(fd, buf, 1024, 0);
// 		char * buf = kvs_client_server_protocol(fd);
// 		if (ret > 0) {
			
// 			char response[BUFFER_SIZE] = {0};
// 			int slength = kvs_handler(buf, strlen(buf), response);

// 			ret = send(fd, response, slength, 0);
// 			if (ret == -1) {
// 				close(fd);
// 				break;
// 			}
// 		} else if (ret == 0) {	
// 			close(fd);
// 			break;
// 		}

// 	}
// }


void server_reader(void *arg) {

		client_info * cli_info = (client_info *)arg;
		int ret = 0;
		while(1){	
			ret = recv(cli_info->fd, cli_info->rbuf + cli_info->read_pos, cli_info->capacity - cli_info->read_pos, 0);
			if(ret > 0) {
				cli_info->read_pos += ret;
				int head_length = 0;
				int total_length = kvs_recv_protocol(cli_info, &head_length);
				if(head_length == 0 || cli_info->read_pos < total_length) continue;
				cli_info->rbuf[total_length] = '\0';
				char response[BUFFER_SIZE] = {0};
				char * pure_data = cli_info->rbuf + head_length;
				int pure_len = total_length - head_length;
				int slength = kvs_handler(pure_data, pure_len, response);
				ret = send(cli_info->fd, response, slength, 0);	
				cli_info->read_pos = 0;
			}
			else if (ret <= 0) {	
				close(cli_info->fd);
				free(cli_info->rbuf);
				free(cli_info);
				cli_info = NULL;
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
	bind(fd, (struct sockaddr*)&local, sizeof(struct sockaddr_in));

	listen(fd, 20);
	printf("listen port : %d\n", port);


	while (1) {
		socklen_t len = sizeof(struct sockaddr_in);
		int cli_fd = accept(fd, (struct sockaddr*)&remote, &len);

		client_info * cli_info = (client_info *)malloc(sizeof(client_info));
		if(cli_info == NULL) return;
		memset(cli_info, 0, sizeof(client_info));
		cli_info->rbuf = (char *)malloc(BUFFER_SIZE + 1);
		if(cli_info->rbuf == NULL){
			free(cli_info);
			return ;
		}
		cli_info->capacity = BUFFER_SIZE;
		cli_info->fd = cli_fd;

		nty_coroutine *read_co;
		//nty_coroutine_create(&read_co, server_reader, &cli_fd);
		nty_coroutine_create(&read_co, server_reader, &cli_info);

	}
	
}





int ntyco_start(unsigned short port, msg_handler handler) {

	//int port = atoi(argv[1]);
	kvs_handler = handler;

	
	nty_coroutine *co = NULL;
	nty_coroutine_create(&co, server, &port);

	nty_schedule_run();

}




