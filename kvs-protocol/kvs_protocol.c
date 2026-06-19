#include "kvstore.h"
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//kvstore serialization protocol: kvsp ************************
// kvsp: #<body_length>\r\n^<tok_len>&<tok>^<tok_len>&<tok>...\r\n
int kvsp_parse_bulk_size(const char * buf, int buf_size, int * head_len){
	if(buf == NULL || head_len == NULL || buf_size <= 0) return -1;

	int pos = 0;  
	int body_len = 0;
	int total_len = 0;

	if (buf_size < KVSP_START_STR_LEN) return 0;

	if (memcmp(buf, KVSP_START_STR, KVSP_START_STR_LEN) != 0) return -2;

	pos += KVSP_START_STR_LEN;

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

	if (pos >= buf_size || (buf_size - pos) < KVSP_HEAD_TAIL_STR_LEN) return 0;

	if (memcmp(buf + pos, KVSP_HEAD_TAIL_STR, KVSP_HEAD_TAIL_STR_LEN) != 0 ) return -2;

	pos += KVSP_HEAD_TAIL_STR_LEN;

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

	if (cli_info->r_pos < KVSP_START_STR_LEN) {
		return 1;
	}

	if (memcmp(cli_info->rbuf, KVSP_START_STR, KVSP_START_STR_LEN) == 0) { 
		cli_info->protocol = PROTO_KVSP;
		cli_info->recv_protocol = kvsp_parse_bulk_size;
		return 0;
	}
	
	return -2;
		
}

/// redis serialization protocol: resp ************************
/// kvstore serialization protocol: body: ^<tok_len>&<tok>^<tok_len>&<tok>...\r\n ************************
int kvs_split_token(client_info * cli, char *tokens[]) {
	if (cli == NULL || tokens == NULL) return -1;

	char * body = cli->rbuf + cli->cmd_hl;

	int pos = 0;
    int idx = 0;

	if (cli->protocol == PROTO_KVSP) {

		int body_len = cli->cmd_tl - cli->cmd_hl;

		if (body_len < KVSP_END_STR_LEN) return -2;

		int parse_end = body_len - KVSP_END_STR_LEN;
		if (memcmp(body + parse_end, KVSP_END_STR, KVSP_END_STR_LEN) != 0) return -2;

		while (pos < parse_end) {

			int has_digit = 0;
			int tok_len = 0;

			if (body[pos] != KVSP_TOK_LEN_START_CHAR) return -2;

			if (idx > 0) body[pos] = '\0';

			pos++;

			while (pos < parse_end && body[pos] <= '9' && body[pos] >= '0') {
				has_digit = 1;
				
				int digit = body[pos] - '0';
				if (tok_len > (INT_MAX - digit) / 10) return -2;
				tok_len = 10 * tok_len + digit;

				pos++;
			}

			if (!has_digit) return -2;
			
			if (pos >= parse_end) return -2;

			if (body[pos] != KVSP_TOK_START_CHAR) return -2;
			pos ++;

			if (tok_len > parse_end - pos) return -2;

			if (idx >= KVS_MAX_TOKENS) return -2;
			tokens[idx++] = body + pos;

			pos += tok_len;							
			
		}
		if (pos != parse_end) return -2;
		body[parse_end] = '\0';

		return idx;
	}
    
	if (cli->protocol == PROTO_RESP) {

		int es_len = 2; // end string: \r\n
    	int bs_len = 0; // bulk string length

		while(body[pos] != '\0'){   
			if(body[pos] == '$') {
				++pos;
				bs_len = atoi(body + pos);
				while(body[pos] != '\r'){
					pos++;
				}
				pos += es_len;
				tokens[idx++] = body + pos;
				body[pos + bs_len] = '\0';
				pos += bs_len + es_len;
			}
			else{
				return -2;
			}		
		}
		return idx;
	}

    return -3;

}

// create kvsp like command: #<body_length>\r\n^<str_len>&<str>^<str_len>&<str>...\r\n
char * kvs_build_kvsp_frame(int argc, char * argv[], int * buf_len) {
    if (argc <= 0 || argv == NULL || buf_len == NULL) return NULL;

    int body_len = 0;
    int head_len = 0;
    int total_len = 0;
    int ret = 0;

    for (int i = 0; i < argc; i++) {
        char * str = argv[i];
        if (argv[i] == NULL) {
            return NULL;
        }
        
        size_t str_len = strlen(str);
        ret = snprintf(NULL, 0, "^%zu&%s", str_len, str);
        if (ret < 0) return NULL;
        body_len += ret;
    }
    body_len += KVSP_END_STR_LEN;

    head_len = snprintf(NULL, 0, "#%d\r\n", body_len);
    if (head_len < 0) return NULL;

    total_len = head_len + body_len;

    char * buf = (char *)kvs_malloc(total_len + 1);
    if (buf == NULL) {
        printf("kvs_malloc error\n");
        return NULL;
    }

    int offset = 0;

    ret = snprintf(buf + offset, total_len + 1 - offset, "#%d\r\n", body_len);
    if (ret < 0) {
        kvs_free(buf);
        return NULL;
    }
    offset += ret;

    for (int i = 0; i < argc; i++) {
        char * str = argv[i];
        size_t str_len = strlen(str);
        ret = snprintf(buf + offset, total_len + 1 - offset, "^%zu&%s", str_len, str);
        if (ret < 0) {
            kvs_free(buf);
            return NULL;
        }
        offset += ret;
    }
    memcpy(buf + offset, KVSP_END_STR, KVSP_END_STR_LEN);
    offset += KVSP_END_STR_LEN;

    if (offset != total_len) {
        kvs_free(buf);
        return NULL;
    }

    *buf_len = total_len;
    return buf;
}