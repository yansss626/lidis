



#include "nty_coroutine.h"
#include "kvstore.h"
#include <arpa/inet.h>

#define BUFFER_SIZE 12

static msg_handler kvs_handler;



int kvs_recv_protocol(client_info * cli_info, int * head_len){
	if(cli_info == NULL) return -1;
	int data_len = 0;
	int protocol_len = 0;
	while(protocol_len < cli_info->r_pos){
		if(cli_info->rbuf[protocol_len] == '*'){
			cli_info->rbuf[protocol_len] = '\0';
			data_len = atoi(cli_info->rbuf);
			cli_info->rbuf[protocol_len] = '*';
			break;
		}
		protocol_len++;
	}
	if(protocol_len == cli_info->r_pos) return 0;
	(*head_len) = protocol_len + 1;
	int total_len = protocol_len + data_len + 1;
	if(total_len >= cli_info->r_cap){
		char * temp = (char *)realloc(cli_info->rbuf, total_len + 1);
		if(temp == NULL){
			assert(0);
		}
		cli_info->rbuf = temp;
		cli_info->r_cap = total_len;
	}

	return total_len;

}

client_info * client_info_init(int fd){
	client_info * cli_info = (client_info *)malloc(sizeof(client_info));
	if(cli_info == NULL) return NULL;
	memset(cli_info, 0, sizeof(client_info));

	cli_info->rbuf = (char *)malloc(BUFFER_SIZE + 1);
	if(cli_info->rbuf == NULL){
		free(cli_info);
		return NULL;
	}
	cli_info->r_cap = BUFFER_SIZE;
	memset(cli_info->rbuf, 0, BUFFER_SIZE + 1);


	cli_info->wbuf = (char *)malloc(BUFFER_SIZE + 1);
	if(cli_info->wbuf == NULL){
		free(cli_info);
		free(cli_info->rbuf);
		return NULL;
	}
	cli_info->w_cap = BUFFER_SIZE;
	memset(cli_info->wbuf, 0, BUFFER_SIZE + 1);
	cli_info->fd = fd;


	return cli_info;
	
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
// 			int slen = kvs_handler(buf, strlen(buf), response);

// 			ret = send(fd, response, slen, 0);
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
			ret = recv(cli_info->fd, cli_info->rbuf + cli_info->r_pos, cli_info->r_cap - cli_info->r_pos, 0);
			
			if(ret > 0) {
				cli_info->r_pos += ret;
				int head_len = 0;
				int total_len = kvs_recv_protocol(cli_info, &head_len);
				if(head_len == 0 || cli_info->r_pos < total_len) continue;

				
				//char response[BUFFER_SIZE] = {0};

				char * pure_data = cli_info->rbuf + head_len;
				int pure_len = total_len - head_len;
				memmove(cli_info->rbuf, cli_info->rbuf + head_len, total_len - head_len);
				cli_info->rbuf[total_len - head_len] = '\0';

				//printf("%s\n",cli_info->rbuf);
				int slen = kvs_handler(cli_info);
				
				if(slen < 0) break;
				//ret = send(cli_info->fd, response, slen, 0);	
				ret = send(cli_info->fd, cli_info->wbuf, slen, 0);	
				cli_info->r_pos = 0;
			}
			else if (ret <= 0) {	

				break;
			}
			
		}
		close(cli_info->fd);
		if(cli_info->rbuf != NULL) free(cli_info->rbuf);
		if(cli_info->wbuf != NULL) free(cli_info->wbuf);
		free(cli_info);
		cli_info = NULL;

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

		client_info * cli_info = client_info_init(cli_fd);
		if(cli_info == NULL) break; 


		nty_coroutine *read_co;
		//nty_coroutine_create(&read_co, server_reader, &cli_fd);
		nty_coroutine_create(&read_co, server_reader, cli_info);

	}
	
}





int ntyco_start(unsigned short port, msg_handler handler) {

	//int port = atoi(argv[1]);
	kvs_handler = handler;

	
	nty_coroutine *co = NULL;
	nty_coroutine_create(&co, server, &port);

	nty_schedule_run();

}




