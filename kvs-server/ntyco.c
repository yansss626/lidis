



#include "nty_coroutine.h"
#include "kvstore.h"
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

static msg_handler kvs_handler;

extern kvs_slaves global_slaves;



/// redis serialization protocol: resp ************************
int resp_parse_bulk_size(const char * buf, size_t buf_size, int * head_len){
	if(buf == NULL || head_len == NULL || buf_size <= 0) return -1;


    size_t pos = 0;  // pointr position
    int argc = 0; //// *<argc>\r\n.....

    int pc_pos = 0; ////Redis Serialiation Protocol Characters: '*', '$'
    int es_len = 2; //end string: \r\n

    if(buf[pos] == '*'){
        pc_pos = pos;
        ++pos;
    }
    else return -2;
            
            
    while(pos < buf_size){
        if(buf[pos] == '\n' && buf[pos - 1] == '\r'){
            argc = atoi(buf + pc_pos + 1);
            (*head_len) = pos + 1;

            break;
        }
        ++pos;
    }            
    

    if(pos >= buf_size)  return 0;
    

    int bs_len = 0; // bulk string length
    for(int i = 0; i < argc; i++){
        ++pos; // pos for "$"
        if(pos > buf_size) return 0;
        if(buf[pos] != '$') return -2;

        pc_pos = pos;
        while(pos < buf_size){
            if(buf[pos] == '\n' && buf[pos - 1] == '\r'){
                bs_len = atoi(buf + pc_pos + 1);
				pos += bs_len + es_len;
                break;
            }
            ++pos;
        }               
        
        if(pos >= buf_size) return 0;
         
    }

    return pos + 1;

}
//********************************************

int kvs_recv_protocol(client_info * cli_info, int * head_len){
	return resp_parse_bulk_size(cli_info->rbuf, cli_info->r_pos, head_len);
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




void server_reader(void *arg) {
		client_info * cli_info = (client_info *)arg;
		int ret = 0;
		while(1){
			if(cli_info->r_pos >= cli_info->r_cap) {
				char * temp = (char *)realloc(cli_info->rbuf, 2 * cli_info->r_cap + 1);
                if(temp == NULL) {
                    perror("realloc error");
                    break;
                }
                cli_info->rbuf = temp;
                cli_info->r_cap *= 2;		
			}
			ret = recv(cli_info->fd, cli_info->rbuf + cli_info->r_pos, cli_info->r_cap - cli_info->r_pos, 0);

			if (ret <= 0) {	
				if(cli_info->role == 1) kvs_slaves_delete(&global_slaves, cli_info->fd);
				break;
			}
			else {
				cli_info->r_pos += ret;
			}

			while(cli_info->r_pos > 0) {
				
				int head_len = 0;

				int total_len = kvs_recv_protocol(cli_info, &head_len);
				if(total_len == 0) break;
				if(total_len < 0){
					if(cli_info->w_cap - cli_info->w_pos < 22){
							char * temp = (char *)realloc(cli_info->wbuf, cli_info->w_cap + 23);
							if(temp == NULL) {
								perror("realloc error");
								goto cleanup;
							}
							cli_info->wbuf = temp;
							cli_info->w_cap += 22;			
					}
					int len = sprintf(cli_info->wbuf + cli_info->w_pos, "-ERR protocol error\r\n");
					send(cli_info->fd, cli_info->wbuf + cli_info->w_pos, len, 0);
					goto cleanup;
				}

				cli_info->cmd_tl = total_len;
				cli_info->cmd_hl = head_len;



				char temp = cli_info->rbuf[total_len];
				cli_info->rbuf[total_len] = '\0';
				//printf("%s\n", cli_info->rbuf);

				int slen = kvs_handler(cli_info);
				cli_info->w_pos += slen;

				cli_info->rbuf[total_len] = temp;
				if(slen < 0) goto cleanup;
				cli_info->r_pos -= total_len;
				if(cli_info->r_pos > 0){
					memmove(cli_info->rbuf, cli_info->rbuf + total_len, cli_info->r_pos);
					
				}

				
			}
			if(cli_info->w_pos > 0){
				if(cli_info->role == 1) cli_info->w_pos = 0; // slave doesn't reply
				else ret = send(cli_info->fd, cli_info->wbuf, cli_info->w_pos, 0);
				//printf("sbuf: %s\n", cli_info->wbuf);
				cli_info->w_pos = 0;
			}
			
		}
		cleanup:
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
		nty_coroutine_create(&read_co, server_reader, cli_info);

	}
	
}





int ntyco_start(unsigned short port, msg_handler handler) {


	kvs_handler = handler;

	
	nty_coroutine *co = NULL;
	nty_coroutine_create(&co, server, &port);

	nty_schedule_run();

	return 0;

}




