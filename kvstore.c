#include "kvstore.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

kvs_conf_t global_config = {0};

#if ENABLE_ARRAY
extern kvs_array_t global_array;
#endif

#if ENABLE_RBTREE
extern kvs_rbtree_t global_rbtree;
#endif

#if ENABLE_HASH
extern kvs_hash_t global_hash;
#endif

#if ENABLE_SKIPLIST
extern kvs_skiplist_t global_skiplist;
#endif

#define BUFFER_SIZE 1024


#if MEM_POOL 
extern kvs_mempool_t pools;
#endif

void *kvs_malloc(size_t size) {
#if MEM_POOL 
	return mp_malloc(&pools, size);
#else
	return malloc(size);
#endif
}

void kvs_free(void *ptr) {
#if MEM_POOL 
	return mp_free(&pools, ptr);
#else
	return free(ptr);
#endif
}

void * kvs_realloc(void *ptr, size_t size) {
#if MEM_POOL 
	return mp_realloc(&pools, ptr, size);
#else
	return realloc(ptr, size);
#endif
}


const char *command[] = {
	"SET", "GET", "DEL", "MOD", "EXIST",
	"RSET", "RGET", "RDEL", "RMOD", "REXIST",
	"HSET", "HGET", "HDEL", "HMOD", "HEXIST",
	"LSET", "LGET", "LDEL", "LMOD", "LEXIST",
	"SAVE", "SYNC", "COMMAND",
};

enum {
	KVS_CMD_START = 0,
	// array
	KVS_CMD_SET = KVS_CMD_START,
	KVS_CMD_GET,
	KVS_CMD_DEL,
	KVS_CMD_MOD,
	KVS_CMD_EXIST,

	// rbtree
	KVS_CMD_RSET,
	KVS_CMD_RGET,
	KVS_CMD_RDEL,
	KVS_CMD_RMOD,
	KVS_CMD_REXIST,

	// hash
	KVS_CMD_HSET,
	KVS_CMD_HGET,
	KVS_CMD_HDEL,
	KVS_CMD_HMOD,
	KVS_CMD_HEXIST,

	
		// skiplist
	KVS_CMD_LSET,
	KVS_CMD_LGET,
	KVS_CMD_LDEL,
	KVS_CMD_LMOD,
	KVS_CMD_LEXIST,

	KVS_CMD_SAVE,
	KVS_CMD_SYNC,

	KVS_CMD_COMMAND,

	KVS_CMD_COUNT,
};

int is_recovering = 1; // global sign for kvstore initilization. 1 indicates that kvstore is recovering, 0 indicates the opposite.

// const char *response[] = {

// };

enum kvs_reply_t{// enum used for client reply
	REPLY_START,
	REPLY_OK,
	REPLY_ERROR,
	REPLY_EXIST,
	REPLY_NO_EXIST,
	REPLY_VALUE,
	REPLY_NONE,
	REPLY_COMMAND,

}; 



int kvs_filter_protocol(char **tokens, int count, client_info * cli) {

	if (tokens[0] == NULL || count <= 0 || cli == NULL) return -1;

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
	//printf("key: %s, value: %s \n", tokens[1], tokens[2]);
	enum kvs_reply_t reply = REPLY_START;
	char * result = NULL;
	int is_write_success = 0;
	if(cli->w_cap - cli->w_pos < 32){
			char * temp = (char *)kvs_realloc(cli->wbuf, cli->w_cap * 2);
			if(temp == NULL) {
				perror("kvs_realloc error");
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
			reply = REPLY_ERROR;
		} else if (ret == 0) {
			reply = REPLY_OK;
			is_write_success = 1;
		} else {
			reply = REPLY_EXIST;
		} 
		
		break;
	case KVS_CMD_GET: {
		result = kvs_array_get(&global_array, key);		
		if (result == NULL) {
			reply = REPLY_NO_EXIST;
		} else {
			reply = REPLY_VALUE;
		}
		break;
	}
	case KVS_CMD_DEL:
		ret = kvs_array_del(&global_array ,key);		
		if (ret < 0) {
			reply = REPLY_ERROR;
		} else if (ret == 0) {
			reply = REPLY_OK;
			is_write_success = 1;
		} else {
			reply = REPLY_NO_EXIST;
		} 
		break;
	case KVS_CMD_MOD:
		ret = kvs_array_mod(&global_array ,key, value);
		if (ret < 0) {
			reply = REPLY_ERROR;
		} else if (ret == 0) {
			reply = REPLY_OK;
			is_write_success = 1;
		} else {
			reply = REPLY_NO_EXIST;
		} 
		break;
	case KVS_CMD_EXIST:
		ret = kvs_array_exist(&global_array ,key);
		if (ret == 0) {
			reply = REPLY_EXIST;
		} else {
			reply = REPLY_NO_EXIST;
		}
		break;
#endif
	// rbtree
#if ENABLE_RBTREE
	case KVS_CMD_RSET:
		ret = kvs_rbtree_set(&global_rbtree ,key, value);
		if (ret < 0) {
			reply = REPLY_ERROR;
		} else if (ret == 0) {
			reply = REPLY_OK;
			is_write_success = 1;
		} else {
			reply = REPLY_EXIST;
		} 
		
		break;
	case KVS_CMD_RGET: {
		result = kvs_rbtree_get(&global_rbtree, key);
		if (result == NULL) {
			reply = REPLY_NO_EXIST;
		} else {
			reply = REPLY_VALUE;
		}
		break;
	}
	case KVS_CMD_RDEL:
		ret = kvs_rbtree_del(&global_rbtree ,key);
		if (ret < 0) {
			reply = REPLY_ERROR;
		} else if (ret == 0) {
			reply = REPLY_OK;
			is_write_success = 1;
		} else {
			reply = REPLY_NO_EXIST;
		} 
		break;
	case KVS_CMD_RMOD:
		ret = kvs_rbtree_mod(&global_rbtree ,key, value);
		if (ret < 0) {
			reply = REPLY_ERROR;
		} else if (ret == 0) {
			reply = REPLY_OK;
			is_write_success = 1;
		} else {
			reply = REPLY_NO_EXIST;
		} 
		break;
	case KVS_CMD_REXIST:
		ret = kvs_rbtree_exist(&global_rbtree ,key);
		if (ret == 0) {
			reply = REPLY_EXIST;
		} else {
			reply = REPLY_NO_EXIST;
		}
		break;
#endif
	//hash
#if ENABLE_HASH
	case KVS_CMD_HSET:
		ret = kvs_hash_set(&global_hash ,key, value);
		if (ret < 0) {
			reply = REPLY_ERROR;
		} else if (ret == 0) {
			reply = REPLY_OK;
			is_write_success = 1;
		} else {
			reply = REPLY_EXIST;
		} 
		
		break;
	case KVS_CMD_HGET: {
		result = kvs_hash_get(&global_hash, key);
		if (result == NULL) {
			reply = REPLY_NO_EXIST;
		} else {
			reply = REPLY_VALUE;
		}
		break;
	}
	case KVS_CMD_HDEL:
		ret = kvs_hash_del(&global_hash ,key);
		if (ret < 0) {
			reply = REPLY_ERROR;
		} else if (ret == 0) {
			reply = REPLY_OK;
			is_write_success = 1;
		} else {
			reply = REPLY_NO_EXIST;
		} 
		break;
	case KVS_CMD_HMOD:
		ret = kvs_hash_mod(&global_hash ,key, value);
		if (ret < 0) {
			reply = REPLY_ERROR;
		} else if (ret == 0) {
			reply = REPLY_OK;
			is_write_success = 1;
		} else {
			reply = REPLY_NO_EXIST;
		} 
		break;
	case KVS_CMD_HEXIST:
		ret = kvs_hash_exist(&global_hash ,key);
		if (ret == 0) {
			reply = REPLY_EXIST;
		} else {
			reply = REPLY_NO_EXIST;
		}
		break;
#endif
	//skiplist
#if ENABLE_SKIPLIST
	case KVS_CMD_LSET:
		ret = kvs_skiplist_set(&global_skiplist ,key, value);
		if (ret < 0) {
			reply = REPLY_ERROR;
		} else if (ret == 0) {
			reply = REPLY_OK;
			is_write_success = 1;
		} else {
			reply = REPLY_EXIST;
		} 
		
		break;
	case KVS_CMD_LGET: {
		result = kvs_skiplist_get(&global_skiplist, key);
		if (result == NULL) {
			reply = REPLY_NO_EXIST;
		} else {
			reply = REPLY_VALUE;
		}
		break;
	}
	case KVS_CMD_LDEL:
		ret = kvs_skiplist_del(&global_skiplist ,key);
		if (ret < 0) {
			reply = REPLY_ERROR;
		} else if (ret == 0) {
			reply = REPLY_OK;
			is_write_success = 1;
		} else {
			reply = REPLY_NO_EXIST;
		} 
		break;
	case KVS_CMD_LMOD:
		ret = kvs_skiplist_mod(&global_skiplist ,key, value);
		if (ret < 0) {
			reply = REPLY_ERROR;
		} else if (ret == 0) {
			reply = REPLY_OK;
			is_write_success = 1;
		} else {
			reply = REPLY_NO_EXIST;
		} 
		break;
	case KVS_CMD_LEXIST:
		ret = kvs_skiplist_exist(&global_skiplist ,key);
		if (ret == 0) {
			reply = REPLY_EXIST;
		} else {
			reply = REPLY_NO_EXIST;
		}
		break;
#endif
	case KVS_CMD_SAVE:
		ret = kvs_save_write();
		if (ret == 0) { // 创建子进程，后台执行本次SAVE。
			reply = REPLY_OK;
		} else if (ret == 1) {
			reply = REPLY_OK; // 子进程被占用，本次SAVE未执行。
		}
		else {
			reply = REPLY_ERROR;
		}
		break;
	case KVS_CMD_SYNC:
		ret = kvs_master_full_sync(cli);
		if (ret < 0) {
			length = ret;
		}
		break;

	case KVS_CMD_COMMAND:
		reply = REPLY_COMMAND;
		break;
	default: 
		reply = REPLY_NONE;
		//assert(0);
	}

	switch (reply)
	{
	case REPLY_OK:
		length = sprintf(cli->wbuf + cli->w_pos, "+OK\r\n");
		break;
	case REPLY_ERROR:
		length = sprintf(cli->wbuf + cli->w_pos, "-ERR message\r\n");
		break;	
	case REPLY_EXIST:
		length = sprintf(cli->wbuf + cli->w_pos, ":1\r\n");
		break;
	case REPLY_NO_EXIST:
		length = sprintf(cli->wbuf + cli->w_pos, "$-1\r\n");
		break;

	case REPLY_VALUE:{
		int rlen = strlen(result);// $len\r\nvalue\r\n
		int needed = rlen + 15; // "$" + 最多10位数字 + "\r\n" + value + "\r\n"
		if(needed > cli->w_cap - cli->w_pos){  
			char * temp = (char *)kvs_realloc(cli->wbuf, cli->w_cap + needed + 1);
			if(temp == NULL) {
				perror("kvs_realloc error");
				return -1;
			}
			cli->wbuf = temp;
			cli->w_cap += needed;
		}
		length = sprintf(cli->wbuf + cli->w_pos, "$%d\r\n%s\r\n", rlen ,result);
		break;
	}
	case REPLY_NONE:
		length = sprintf(cli->wbuf + cli->w_pos, "-ERR no such command\r\n");
		break;
	case REPLY_COMMAND:
		length = sprintf(cli->wbuf + cli->w_pos, "*0\r\n");
		break;
	default:
		break;
	}


	if(is_write_success == 1 && is_recovering == 0) {

		kvs_log_write(tokens, count);
		kvs_master_incr_sync(cli, tokens, count);
	}

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

	int count = kvs_split_token(cli, tokens);
	if (count < 0) return -1;

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

#if ENABLE_SKIPLIST
	memset(&global_skiplist, 0, sizeof(kvs_skiplist_t));
	kvs_skiplist_create(&global_skiplist);
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

#if ENABLE_SKIPLIST
	kvs_skiplist_destroy(&global_skiplist);
#endif

}

void kvs_init(){
	kvs_config_init	(&global_config);
#if MEM_POOL 
	mp_create(&pools);
#endif
	init_kvengine();
	kvs_save_init(kvs_protocol);
	kvs_log_init(kvs_protocol);
}

void kvs_deinit(){
	dest_kvengine();
	kvs_log_close();
#if MEM_POOL
	mp_destroy(&pools);
#endif
}

// kvstore configuation definition
const char * configuation[] = {
	"ENABLE_MODULE_SYNC", "ENABLE_MODULE_LOG", "ENABLE_MODULE_SAVE",
	"Master_ip", "Master_port", "Mode", "Port", "Rdma_port",
	"Agent_ip", "Agent_port",
};

enum kvs_conf_t{ // enum used for configutaion setup
	KVS_CONF_START = 0,

	KVS_MODULE_SYC = KVS_CONF_START,
	KVS_MODULE_LOG,
	KVS_MODULE_SAVE,

	KVS_MASTER_IP,
	KVS_MASTER_PORT,

	KVS_MODE,
	KVS_PORT,


	KVS_RDMA_PORT,

	KVS_AGENT_IP,
	KVS_AGENT_PORT,
	
	KVS_CONF_COUNT
};

int kvs_config_init(kvs_conf_t *  conf){
	if(conf == NULL) return -1;
	FILE * fp = fopen("./conf/kvstore.conf", "r");;
	if(fp == NULL) return -2;
	char  buf[BUFFER_SIZE] = {0};

	enum kvs_conf_t temp = KVS_CONF_START;
	while(fgets(buf, BUFFER_SIZE, fp) != NULL){
		char * key = strtok(buf, " \n");
		char * value = strtok(NULL, "= \n");
		//printf("key: %s, value: %s\n", key, value);
		if(key == NULL) continue;
		for(temp = KVS_CONF_START; temp < KVS_CONF_COUNT; temp++){
			if(strcmp(key, configuation[temp]) == 0) break;
		}
		switch (temp)
		{
		case KVS_MODULE_SYC:
			conf->enable_sync = atoi(value);
			break;
		case KVS_MODULE_LOG:
			conf->enable_log = atoi(value);
			break;
		case KVS_MODULE_SAVE:
			conf->enable_save = atoi(value);
			break;
		case KVS_MASTER_IP:
			strncpy(conf->master_ip, value, strlen(value)+ 1);
			break;	
		case KVS_MASTER_PORT:
			conf->master_port = atoi(value);
			break;	
		case KVS_MODE:
			conf->mode = atoi(value);
			break;
		case KVS_PORT:
			conf->port = atoi(value);
			break;	
		case KVS_RDMA_PORT:
			strncpy(conf->rdma_port, value, strlen(value) + 1) ;
			break;	
		case KVS_AGENT_IP:
			strncpy(conf->agent_ip, value, strlen(value)+ 1);
			break;	
		case KVS_AGENT_PORT:
			conf->agent_port = atoi(value);
			break;				
		default:
			break;
		}
	}
	//printf("IP: %s, port: %d\n", global_config.master_ip, global_config.master_port);
	fclose(fp);
	return 0;
}
//

int main(int argc, char *argv[]) {


	kvs_init();

	int port = global_config.port;
	is_recovering = 0;
	
	if(global_config.mode == 1){
		if(0 != kvs_slave_sync()){
			printf("failed to sync\n");
		}
	}


#if (NETWORK_SELECT == NETWORK_REACTOR)
	reactor_start(port, kvs_protocol);  //
#elif (NETWORK_SELECT == NETWORK_NTYCO)
	ntyco_start(port, kvs_protocol);
#elif (NETWORK_SELECT == NETWORK_PROACTOR)
	proactor_start(port, kvs_protocol);
#endif

	
	kvs_deinit();
}


