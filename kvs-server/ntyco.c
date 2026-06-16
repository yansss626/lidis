



#include "nty_coroutine.h"
#include "kvstore.h"
#include <arpa/inet.h>

#define BUFFER_SIZE 1024 * 1024 // 1MB

#define KVS_ERR_PROTO_LEN	22
#define KVSP_START_STRING	"kvsp/1\r\n"
#define KVSP_START_LENGTH	8 // kvsp/1\r\n
#define KVSP_BULK_END_STR	"\r\n"
#define	KVSP_BULK_END_STR_LEN	2
#define KVSP_BL_START_CHAR	'#'	//BL:body length



static msg_handler kvs_handler;


//kvstore serialization protocol: kvsp ************************
// example: kvsp/1\r\n#<body_length>\r\n$4\r\nSAVE\r\n
int kvsp_parse_bulk_size(const char * buf, int buf_size, int * head_len){
	if(buf == NULL || head_len == NULL || buf_size <= 0) return -1;

	int pos = 0;  
	int body_len = 0;
	int total_len = 0;

	if (buf_size < KVSP_START_LENGTH) return 0;

	if (memcmp(buf, KVSP_START_STRING, KVSP_START_LENGTH) != 0) return -2;

	pos += KVSP_START_LENGTH;

	if (pos >= buf_size) return 0;

	if (buf[pos] != KVSP_BL_START_CHAR) return -2;

    pos++;

	if (pos >= buf_size) return 0;

	int has_digit = 0;

	while (pos < buf_size && buf[pos] <= '9' && buf[pos] >= '0') {
		has_digit = 1;
		
		int digit = buf[pos] - '0';

		if (body_len > (INT_MAX - digit) / 10) return -2;

		body_len = 10 * body_len + digit;
		pos++;
	}

	if (!has_digit) {
		if (pos >= buf_size) {
			return 0;
		}
		return -2;
	}

	if (pos >= buf_size) return 0;

	if (buf[pos] != '\r') return -2;
	pos++;

	if (pos >= buf_size) return 0;

	if (buf[pos] != '\n') return -2;
	pos ++;

	if (body_len > (INT_MAX - pos) ) return -2;

	*head_len = pos;

	total_len = body_len + pos;


	return total_len > buf_size ? 0 : total_len;


}



/// redis serialization protocol: resp ************************
int resp_parse_bulk_size(const char * buf, int buf_size, int * head_len){
	if(buf == NULL || head_len == NULL || buf_size <= 0) return -1;


    size_t pos = 0;  // pointer position
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
        if(pos >= buf_size) return 0;
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

int kvs_detect_protocol(client_info * cli_info){
	if (cli_info == NULL || cli_info->rbuf == NULL) {
		return -1;
	}


	if (cli_info->r_pos == 0) {
		return 1;
	}

	if (cli_info->rbuf[0] == '*') {
		cli_info->protocol = PROTO_RESP;
		cli_info->recv_protocol = resp_parse_bulk_size;
		return 0;
	}

	if (cli_info->r_pos < KVSP_START_LENGTH) {
		return 1;
	}

	if (memcmp(cli_info->rbuf, KVSP_START_STRING, KVSP_START_LENGTH) == 0) { 
		cli_info->protocol = PROTO_KVSP;
		cli_info->recv_protocol = kvsp_parse_bulk_size;
		return 0;
	}
	
	return -2;
		
}


client_info * client_info_init(int fd){
	client_info * cli_info = (client_info *)kvs_malloc(sizeof(client_info));
	if(cli_info == NULL) return NULL;
	memset(cli_info, 0, sizeof(client_info));

	cli_info->rbuf = (char *)kvs_malloc(BUFFER_SIZE + 1);
	if(cli_info->rbuf == NULL){
		kvs_free(cli_info);
		return NULL;
	}
	cli_info->r_cap = BUFFER_SIZE;
	memset(cli_info->rbuf, 0, BUFFER_SIZE + 1);


	cli_info->wbuf = (char *)kvs_malloc(BUFFER_SIZE + 1);
	if(cli_info->wbuf == NULL){
		kvs_free(cli_info->rbuf);
		kvs_free(cli_info);
		return NULL;
	}
	cli_info->w_cap = BUFFER_SIZE;
	memset(cli_info->wbuf, 0, BUFFER_SIZE + 1);
	cli_info->fd = fd;

	cli_info->protocol = PROTO_UNKNOWN;
	cli_info->recv_protocol = NULL;

	return cli_info;
	
}

void echo_server(void *arg) {
    client_info * cli_info = (client_info *)arg;
    int ret = 0;

    while (1) {

        ret = recv(cli_info->fd, cli_info->rbuf, 1024, 0); 
        
        if (ret > 0) {
            
            ret = send(cli_info->fd, cli_info->rbuf, ret, 0);
            if (ret == -1) {
                break; 
            }
        } else if (ret == 0) {  
            break; 
        } else {
           
            break; 
        }
    }

    
cleanup:
    printf("client close fd: %d\n", cli_info->fd);
    close(cli_info->fd);
    if(cli_info->rbuf != NULL) kvs_free(cli_info->rbuf);
    if(cli_info->wbuf != NULL) kvs_free(cli_info->wbuf);
    kvs_free(cli_info);
}


void server_reader(void *arg) {
		client_info * cli_info = (client_info *)arg;
		int ret = 0;
		while(1){
			if(cli_info->r_pos >= cli_info->r_cap) {
				char * temp = (char *)kvs_realloc(cli_info->rbuf, 2 * cli_info->r_cap + 1);
                if(temp == NULL) {
                    perror("kvs_realloc error");
                    break;
                }
                cli_info->rbuf = temp;
                cli_info->r_cap *= 2;		
			}
			ret = recv(cli_info->fd, cli_info->rbuf + cli_info->r_pos, cli_info->r_cap - cli_info->r_pos, 0);

			if (ret <= 0) {
				break;
			}
			else {
				cli_info->r_pos += ret;
			}
			
			if (cli_info->protocol == PROTO_UNKNOWN) {
				int dp_ret = kvs_detect_protocol(cli_info);

				if (dp_ret < 0) {
					break;
				}
				
				if (dp_ret > 0) {
					continue;
				}
			}
			

			while(cli_info->r_pos > 0) {
				
				int head_len = 0;
				if (cli_info->recv_protocol == NULL) {
					goto cleanup;
				}
				int total_len = cli_info->recv_protocol(cli_info->rbuf, cli_info->r_pos, &head_len);
				if(total_len == 0) break;
				if(total_len < 0){
					if(cli_info->w_cap - cli_info->w_pos < KVS_ERR_PROTO_LEN){
							char * temp = (char *)kvs_realloc(cli_info->wbuf, cli_info->w_cap + 23);
							if(temp == NULL) {
								perror("kvs_realloc error");
								goto cleanup;
							}
							cli_info->wbuf = temp;
							cli_info->w_cap += KVS_ERR_PROTO_LEN;			
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
		if(cli_info->rbuf != NULL) kvs_free(cli_info->rbuf);
		if(cli_info->wbuf != NULL) kvs_free(cli_info->wbuf);
		kvs_free(cli_info);
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
		//nty_coroutine_create(&read_co, echo_server, cli_info);
	}
	
}





int ntyco_start(unsigned short port, msg_handler handler) {


	kvs_handler = handler;

	
	nty_coroutine *co = NULL;
	nty_coroutine_create(&co, server, &port);

	nty_schedule_run();

	return 0;

}




