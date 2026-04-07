#include "kvstore.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#if ENABLE_ARRAY
extern kvs_array_t global_array;
#endif

#if ENABLE_RBTREE
extern kvs_rbtree_t global_rbtree;
#endif

#if ENABLE_HASH
extern kvs_hash_t global_hash;
#endif

#if ENABLE_MODULE_SYNC
extern kvs_slaves global_slaves;
#endif

void *kvs_malloc(size_t size) {
	return malloc(size);
}

void kvs_free(void *ptr) {
	return free(ptr);
}


const char *command[] = {
	"SET", "GET", "DEL", "MOD", "EXIST", "SAVE",
	"RSET", "RGET", "RDEL", "RMOD", "REXIST", "RSAVE",
	"HSET", "HGET", "HDEL", "HMOD", "HEXIST", "HSAVE",
	"SYNC"
};

enum {
	KVS_CMD_START = 0,
	// array
	KVS_CMD_SET = KVS_CMD_START,
	KVS_CMD_GET,
	KVS_CMD_DEL,
	KVS_CMD_MOD,
	KVS_CMD_EXIST,
	KVS_CMD_SAVE,
	// rbtree
	KVS_CMD_RSET,
	KVS_CMD_RGET,
	KVS_CMD_RDEL,
	KVS_CMD_RMOD,
	KVS_CMD_REXIST,
	KVS_CMD_RSAVE,
	// hash
	KVS_CMD_HSET,
	KVS_CMD_HGET,
	KVS_CMD_HDEL,
	KVS_CMD_HMOD,
	KVS_CMD_HEXIST,
	KVS_CMD_HSAVE,
	
	KVS_CMD_SYNC,

	KVS_CMD_COUNT,
};

int is_recovering = 1;

// const char *response[] = {

// };


// int kvs_split_token(char *msg, char *tokens[]) {

// 	if (msg == NULL || tokens == NULL) return -1;

// 	int idx = 0;
// 	char *token = strtok(msg, " ");
	
// 	while (token != NULL) {
// 		//printf("idx: %d, %s\n", idx, token);
		
// 		tokens[idx ++] = token;
// 		token = strtok(NULL, " ");
// 	}

// 	return idx;
// }


int kvs_split_token(char *msg, char *tokens[]) {

	if (msg == NULL || tokens == NULL) return -1;

	int idx = 0;
	int pos = 0;

	tokens[idx++] = msg + pos;
// ********* 获取第一个token***********
	while(msg[pos] != ' ' && msg[pos] != '\0') pos++;
	if(msg[pos] == '\0') return idx;
	
	msg[pos++] = '\0';
// ***********************************


	while(msg[pos] == ' ')pos++;
	if(msg[pos] == '\0') return 0;

// ********* 获取第二个token***********
	tokens[idx++] = msg + pos;

	while(msg[pos] != ' ' && msg[pos] != '\0') pos++;
    if(msg[pos] == '\0')  {
		// for(int i = 0; i < idx; i++){
		// 	printf("%s\n", tokens[i]);
		// }		
		return idx;
	}
    msg[pos++] = '\0';
// ***********************************

// ********* 获取第三个token***********
	while(msg[pos] == ' ') pos++;
	tokens[idx++] = msg + pos;
// ***********************************

	// for(int i = 0; i < idx; i++){
	// 	printf("%s\n", tokens[i]);
	// }

	return idx;
}

// SET Key Value
// tokens[0] : SET
// tokens[1] : Key
// tokens[2] : Value

int kvs_filter_protocol(char **tokens, int count, client_info * cli) {

	if (tokens[0] == NULL || count == 0 || cli == NULL) return -1;
	int cmd = KVS_CMD_START;
	for (cmd = KVS_CMD_START;cmd < KVS_CMD_COUNT;cmd ++) {
		if (strcmp(tokens[0], command[cmd]) == 0) {
			break;
		} 
	}
	//printf("tokens[0]: %s\n", tokens[0]);
	int length = 0;
	int ret = 0;
	char *key = tokens[1];
	char *value = tokens[2];
	int is_write_success = 0;
	if(cli->w_cap - cli->w_pos < 16){
			char * temp = (char *)realloc(cli->wbuf, cli->w_cap * 2);
			if(temp == NULL) {
				perror("realloc error");
				return -1;
			}
			cli->wbuf = temp;
			cli->w_cap *= 2;			
	}
	
	switch(cmd) {
#if ENABLE_ARRAY
	case KVS_CMD_SET:
		ret = kvs_array_set(&global_array ,key, value);
		if (ret < 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "ERROR\r\n");
		} else if (ret == 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "OK\r\n");
			is_write_success = 1;
			if(is_recovering == 0) kvs_log_write(LOG_ARRAY, "SET", key, value);
		} else {
			length = sprintf(cli->wbuf + cli->w_pos, "EXIST\r\n");
		} 
		
		break;
	case KVS_CMD_GET: {
		char *result = kvs_array_get(&global_array, key);		
		if (result == NULL) {
			length = sprintf(cli->wbuf + cli->w_pos, "NO EXIST\r\n");
		} else {
			int len = strlen(result);
			if(len > cli->w_cap - cli->w_pos - 2){  // 2 for "\r\n"
				char * temp = (char *)realloc(cli->wbuf, len + 3);
				if(temp == NULL) {
					perror("realloc error");
					return -1;
				}
				cli->wbuf = temp;
				cli->w_cap = len + 2;
			}
			length = sprintf(cli->wbuf + cli->w_pos, "%s\r\n", result);
		}
		break;
	}
	case KVS_CMD_DEL:
		ret = kvs_array_del(&global_array ,key);		
		if (ret < 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "ERROR\r\n");
 		} else if (ret == 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "OK\r\n");
			is_write_success = 1;
			if(is_recovering == 0) kvs_log_write(LOG_ARRAY, "DEL", key, "");
		} else {
			length = sprintf(cli->wbuf + cli->w_pos, "NO EXIST\r\n");
		}
		break;
	case KVS_CMD_MOD:
		ret = kvs_array_mod(&global_array ,key, value);
		if (ret < 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "ERROR\r\n");
 		} else if (ret == 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "OK\r\n");
			is_write_success = 1;
			if(is_recovering == 0) kvs_log_write(LOG_ARRAY, "MOD", key, value);
		} else {
			length = sprintf(cli->wbuf + cli->w_pos, "NO EXIST\r\n");
		}
		break;
	case KVS_CMD_EXIST:
		ret = kvs_array_exist(&global_array ,key);
		if (ret == 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "EXIST\r\n");
		} else {
			length = sprintf(cli->wbuf + cli->w_pos, "NO EXIST\r\n");
		}
		break;
	case KVS_CMD_SAVE:
		ret = kvs_save_write(&global_array, SAVE_ARRAY);
		if (ret == 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "OK\r\n");
			is_write_success = 1;
		} else {
			length = sprintf(cli->wbuf + cli->w_pos, "ERROR\r\n");
		}
		break;
#endif
	// rbtree
#if ENABLE_RBTREE
	case KVS_CMD_RSET:
		ret = kvs_rbtree_set(&global_rbtree ,key, value);
		if (ret < 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "ERROR\r\n");
		} else if (ret == 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "OK\r\n");
			is_write_success = 1;
			if(is_recovering == 0) kvs_log_write(LOG_RBTREE, "RSET", key, value);
		} else {
			length = sprintf(cli->wbuf + cli->w_pos, "EXIST\r\n");
		} 
		
		break;
	case KVS_CMD_RGET: {
		char *result = kvs_rbtree_get(&global_rbtree, key);
		if (result == NULL) {
			length = sprintf(cli->wbuf + cli->w_pos, "NO EXIST\r\n");
		} else {
			int len = strlen(result);
			if(len > cli->w_cap - cli->w_pos - 2){  // 2 for "\r\n"
				char * temp = (char *)realloc(cli->wbuf, len + 3);
				if(temp == NULL) {
					perror("realloc error");
					return -1;
				}
				cli->wbuf = temp;
				cli->w_cap = len + 2;
			}
			length = sprintf(cli->wbuf + cli->w_pos, "%s\r\n", result);
		}
		break;
	}
	case KVS_CMD_RDEL:
		ret = kvs_rbtree_del(&global_rbtree ,key);
		if (ret < 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "ERROR\r\n");
 		} else if (ret == 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "OK\r\n");
			is_write_success = 1;
			if(is_recovering == 0) kvs_log_write(LOG_RBTREE, "RDEL", key, "");
		} else {
			length = sprintf(cli->wbuf + cli->w_pos, "NO EXIST\r\n");
		}
		break;
	case KVS_CMD_RMOD:
		ret = kvs_rbtree_mod(&global_rbtree ,key, value);
		if (ret < 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "ERROR\r\n");
 		} else if (ret == 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "OK\r\n");
			is_write_success = 1;
			if(is_recovering == 0) kvs_log_write(LOG_RBTREE, "RMOD", key, value);
		} else {
			length = sprintf(cli->wbuf + cli->w_pos, "NO EXIST\r\n");
		}
		break;
	case KVS_CMD_REXIST:
		ret = kvs_rbtree_exist(&global_rbtree ,key);
		if (ret == 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "EXIST\r\n");
		} else {
			length = sprintf(cli->wbuf + cli->w_pos, "NO EXIST\r\n");
		}
		break;
	case KVS_CMD_RSAVE:
		ret = kvs_save_write(&global_rbtree, SAVE_RBTREE);
		if (ret == 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "OK\r\n");
			is_write_success = 1;
		} else {
			length = sprintf(cli->wbuf + cli->w_pos, "ERROR\r\n");
		}
		break;
#endif
#if ENABLE_HASH
	case KVS_CMD_HSET:
		ret = kvs_hash_set(&global_hash ,key, value);
		if (ret < 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "ERROR\r\n");
		} else if (ret == 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "OK\r\n");
			is_write_success = 1;
			if(is_recovering == 0) kvs_log_write(LOG_HASH, "HSET", key, value);
		} else {
			length = sprintf(cli->wbuf + cli->w_pos, "EXIST\r\n");
		} 
		
		break;
	case KVS_CMD_HGET: {
		char *result = kvs_hash_get(&global_hash, key);
		if (result == NULL) {
			length = sprintf(cli->wbuf + cli->w_pos, "NO EXIST\r\n");
		} else {
			int len = strlen(result);
			if(len > cli->w_cap - cli->w_pos - 2){  // 2 for "\r\n"
				char * temp = (char *)realloc(cli->wbuf, len + 3);
				if(temp == NULL) {
					perror("realloc error");
					return -1;
				}
				cli->wbuf = temp;
				cli->w_cap = len + 2;
			}
			length = sprintf(cli->wbuf + cli->w_pos, "%s\r\n", result);
		}
		break;
	}
	case KVS_CMD_HDEL:
		ret = kvs_hash_del(&global_hash ,key);
		if (ret < 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "ERROR\r\n");
 		} else if (ret == 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "OK\r\n");
			is_write_success = 1;
			if(is_recovering == 0) kvs_log_write(LOG_HASH, "HDEL", key, "");
		} else {
			length = sprintf(cli->wbuf + cli->w_pos, "NO EXIST\r\n");
		}
		break;
	case KVS_CMD_HMOD:
		ret = kvs_hash_mod(&global_hash ,key, value);
		if (ret < 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "ERROR\r\n");
 		} else if (ret == 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "OK\r\n");
			is_write_success = 1;
			if(is_recovering == 0) kvs_log_write(LOG_HASH, "HMOD", key, value);
		} else {
			length = sprintf(cli->wbuf + cli->w_pos, "NO EXIST\r\n");
		}
		break;
	case KVS_CMD_HEXIST:
		ret = kvs_hash_exist(&global_hash ,key);
		if (ret == 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "EXIST\r\n");
		} else {
			length = sprintf(cli->wbuf + cli->w_pos, "NO EXIST\r\n");
		}
		break;
	case KVS_CMD_HSAVE:
		ret = kvs_save_write(&global_hash, SAVE_HASH);
		if (ret == 0) {
			length = sprintf(cli->wbuf + cli->w_pos, "OK\r\n");
			is_write_success = 1;
		} else {
			length = sprintf(cli->wbuf + cli->w_pos, "ERROR\r\n");
		}
		break;
	case KVS_CMD_SYNC:
		kvs_full_sync(&global_slaves, cli);
		length = 0;
		break;
#endif

	default: 
		assert(0);
	}
	if(is_write_success == 1) kvs_incr_sync(&global_slaves, cli, count, tokens);
	return length;
}


/*
 * msg: request message
 * length: length of request message
 * response: need to send
 * @return : length of response
 */

int kvs_protocol(client_info * cli) {  //
	
// SET Key Value
// GET Key
// DEL Key
	if (cli == NULL) return -1;

	//printf("recv %d : %s\n", length, msg);
	//printf("%s\n", cli->rbuf);
	char *tokens[KVS_MAX_TOKENS] = {0};

	int count = kvs_split_token(cli->rbuf + cli->cmd_hl, tokens);
	if (count == -1) return -1;

	//memcpy(response, msg, length);

	return kvs_filter_protocol(tokens, count, cli);
}


int init_kvengine(void) {

#if ENABLE_ARRAY
	memset(&global_array, 0, sizeof(kvs_array_t));
	kvs_array_create(&global_array);
#endif

#if ENABLE_RBTREE
	memset(&global_rbtree, 0, sizeof(kvs_rbtree_t));
	kvs_rbtree_create(&global_rbtree);
#endif

#if ENABLE_HASH
	memset(&global_hash, 0, sizeof(kvs_hash_t));
	kvs_hash_create(&global_hash);
#endif

	return 0;
}

void dest_kvengine(void) {
#if ENABLE_ARRAY
	kvs_array_destory(&global_array);
#endif
#if ENABLE_RBTREE
	kvs_rbtree_destory(&global_rbtree);
#endif
#if ENABLE_HASH
	kvs_hash_destory(&global_hash);
#endif

}

void kvs_init(){
	init_kvengine();
	kvs_save_init(kvs_protocol);
	kvs_log_init(kvs_protocol);
}

void kvs_deinit(){
	dest_kvengine();
	kvs_log_close();
	kvs_slaves_destroy(&global_slaves);
}



int main(int argc, char *argv[]) {

	if (argc < 2) return -1;

	int port = atoi(argv[1]);

	kvs_init();

	is_recovering = 0;

#if ENABLE_MODULE_SYNC
	if(argc == 4){
		int master_port = atoi(argv[3]);
		if(0 != kvs_connect_to_master(argv[2], master_port)){
			printf("failed to sync\n");
		}
	}
#endif


#if (NETWORK_SELECT == NETWORK_REACTOR)
	reactor_start(port, kvs_protocol);  //
#elif (NETWORK_SELECT == NETWORK_NTYCO)
	ntyco_start(port, kvs_protocol);
#elif (NETWORK_SELECT == NETWORK_PROACTOR)
	proactor_start(port, kvs_protocol);
#endif

	
	kvs_deinit();
}


