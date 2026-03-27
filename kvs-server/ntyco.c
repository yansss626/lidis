



#include "nty_coroutine.h"

#include <arpa/inet.h>

#define BUFFER_SIZE 1024
typedef int (*msg_handler)(char *msg, int length, char *response);
static msg_handler kvs_handler;

char * kvs_client_server_protocol(int fd){
	char * buf = (char *)malloc(BUFFER_SIZE);
	if(buf == NULL) return NULL;
	memset(buf, 0, BUFFER_SIZE);
	int ret = recv(fd, buf, BUFFER_SIZE, 0);
	int data_length = 0;
	int protocol_length = 0;
	for(int i = 0; i < ret; i++){
		if(buf[i] == "\n"){
			char * temp = (char *)malloc(i + 1);
			if(temp == NULL) return NULL;
			memset(temp, 0, i + 1);
			strncpy(temp, buf, i);
			data_length = atoi(temp);
			free(temp);
			protocol_length = i + 1;
			break;
		}
	}

	if(data_length + protocol_length > BUFFER_SIZE){
		char * new_buf = (char *)malloc(data_length + 1);
		if(new_buf == NULL) return NULL;
		strcpy(new_buf, buf + protocol_length);
		recv(fd, new_buf + ret - protocol_length, data_length - ret + protocol_length, 0);
		return new_buf;
	}

	return buf;

}

void server_reader(void *arg) {
	int fd = *(int *)arg;
	int ret = 0;

 
	while (1) {
		
		// char buf[1024] = {0};
		// ret = recv(fd, buf, 1024, 0);
		char * buf = kvs_client_server_protocol(fd);
		if (ret > 0) {
			
			char response[BUFFER_SIZE] = {0};
			int slength = kvs_handler(buf, strlen(buf), response);

			ret = send(fd, response, slength, 0);
			if (ret == -1) {
				close(fd);
				break;
			}
		} else if (ret == 0) {	
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
	bind(fd, (struct sockaddr*)&local, sizeof(struct sockaddr_in));

	listen(fd, 20);
	printf("listen port : %d\n", port);


	while (1) {
		socklen_t len = sizeof(struct sockaddr_in);
		int cli_fd = accept(fd, (struct sockaddr*)&remote, &len);
		

		nty_coroutine *read_co;
		nty_coroutine_create(&read_co, server_reader, &cli_fd);

	}
	
}





int ntyco_start(unsigned short port, msg_handler handler) {

	//int port = atoi(argv[1]);
	kvs_handler = handler;

	
	nty_coroutine *co = NULL;
	nty_coroutine_create(&co, server, &port);

	nty_schedule_run();

}




