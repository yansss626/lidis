


#ifndef __KV_STORE_H__
#define __KV_STORE_H__

#include <stddef.h>
#include <liburing.h>

#define NETWORK_REACTOR 	0
#define NETWORK_PROACTOR	1
#define NETWORK_NTYCO		2

#define NETWORK_SELECT		NETWORK_NTYCO



#define KVS_MAX_TOKENS		128

#define ENABLE_ARRAY		1
#define ENABLE_RBTREE		1
#define ENABLE_HASH			1
#define ENABLE_SKIPLIST		1




typedef struct io_write_ctx_s{ // struct for io_uring context
    char * buf;
    size_t len;
	struct io_uring * ring;
	int tasks_count; //number of io_uring_prep_write
}io_write_ctx;

typedef struct client_info_s{
	int fd;
	char * rbuf;
	int r_cap;
	int r_pos;
	int cmd_tl; // total length includes head_length plus payload_length(data_length) 
	int cmd_hl; // head_length

	char * wbuf;
	int w_cap;
	int w_pos;

	int role; // 0:master 1:slave
}client_info;

#define IPV4_MAX_STR_LEN 16
#define PORT_MAX_STR_LEN 5  // "0"~"65535"
typedef struct kvs_conf_s
{
	int enable_log;
	int enable_save;
	int enable_sync;
	char master_ip[IPV4_MAX_STR_LEN]; // IPV4
	int master_port;
	int mode;
	int port;
	char rdma_server_ip[IPV4_MAX_STR_LEN]; // ip for rdma communication    IPV4
	char rdma_port[PORT_MAX_STR_LEN]; //port for rdma communiaction

	char agent_ip[IPV4_MAX_STR_LEN]; // IPV4
	int agent_port;


}kvs_conf_t; // kvstore configuration


#include "kvs_sync.h"


typedef int (*msg_handler)(client_info * cli);


int reactor_start(unsigned short port, msg_handler handler);



extern int proactor_start(unsigned short port, msg_handler handler);
extern int ntyco_start(unsigned short port, msg_handler handler);

int kvs_file_read(char * ptr, size_t size, msg_handler handler);
int kvs_split_token(char *msg, char *tokens[]);
int kvs_save_handler(client_info * cli);
int resp_parse_bulk_size(const char * buf, size_t buf_size, int * head_len);
int kvs_traversal_write(int fd, io_write_ctx * main_ctx);
int kvs_config_init(kvs_conf_t *  conf);

#if ENABLE_ARRAY

typedef struct kvs_array_item_s {
	char *key;
	char *value;
} kvs_array_item_t;

#define KVS_ARRAY_SIZE		102400

typedef struct kvs_array_s {
	kvs_array_item_t *table;
	int idx;
	int total;
} kvs_array_t;

int kvs_array_create(kvs_array_t *inst);
void kvs_array_destory(kvs_array_t *inst);

int kvs_array_set(kvs_array_t *inst, char *key, char *value);
char* kvs_array_get(kvs_array_t *inst, char *key);
int kvs_array_del(kvs_array_t *inst, char *key);
int kvs_array_mod(kvs_array_t *inst, char *key, char *value);
int kvs_array_exist(kvs_array_t *inst, char *key);


#endif


#if ENABLE_RBTREE

#define RED				1
#define BLACK 			2

#define ENABLE_KEY_CHAR		1

#if ENABLE_KEY_CHAR
typedef char* KEY_TYPE;
#else
typedef int KEY_TYPE; // key
#endif

typedef struct _rbtree_node {
	unsigned char color;
	struct _rbtree_node *right;
	struct _rbtree_node *left;
	struct _rbtree_node *parent;
	KEY_TYPE key;
	void *value;
} rbtree_node;

typedef struct _rbtree {
	rbtree_node *root;
	rbtree_node *nil;
} rbtree;


typedef struct _rbtree kvs_rbtree_t;

int kvs_rbtree_create(kvs_rbtree_t *inst);
void kvs_rbtree_destory(kvs_rbtree_t *inst);
int kvs_rbtree_set(kvs_rbtree_t *inst, char *key, char *value);
char* kvs_rbtree_get(kvs_rbtree_t *inst, char *key);
int kvs_rbtree_del(kvs_rbtree_t *inst, char *key);
int kvs_rbtree_mod(kvs_rbtree_t *inst, char *key, char *value);
int kvs_rbtree_exist(kvs_rbtree_t *inst, char *key);



#endif


#if ENABLE_HASH

#define MAX_KEY_LEN	128
#define MAX_VALUE_LEN	512
#define MAX_TABLE_SIZE	1000000

#define ENABLE_KEY_POINTER	1


typedef struct hashnode_s {
#if ENABLE_KEY_POINTER
	char *key;
	char *value;
#else
	char key[MAX_KEY_LEN];
	char value[MAX_VALUE_LEN];
#endif
	struct hashnode_s *next;
	
} hashnode_t;


typedef struct hashtable_s {

	hashnode_t **nodes; //* change **, 

	int max_slots;
	int count;

} hashtable_t;

typedef struct hashtable_s kvs_hash_t;


int kvs_hash_create(kvs_hash_t *hash);
void kvs_hash_destory(kvs_hash_t *hash);
int kvs_hash_set(hashtable_t *hash, char *key, char *value);
char * kvs_hash_get(kvs_hash_t *hash, char *key);
int kvs_hash_mod(kvs_hash_t *hash, char *key, char *value);
int kvs_hash_del(kvs_hash_t *hash, char *key);
int kvs_hash_exist(kvs_hash_t *hash, char *key);


#endif

#if ENABLE_SKIPLIST

typedef struct Node{
    char * key;
    char * value;
    struct Node ** forward;
}Node;

typedef struct SkipList{
    int level;
    Node * header;
}SkipList;

typedef SkipList kvs_skiplist_t;

int kvs_skiplist_create(kvs_skiplist_t * inst);
void kvs_skiplist_destroy(kvs_skiplist_t * inst);
int kvs_skiplist_set(kvs_skiplist_t * inst, char *key, char *value);
char * kvs_skiplist_get(kvs_skiplist_t * inst, char *key);
int kvs_skiplist_mod(kvs_skiplist_t * inst, char *key, char *value);
int kvs_skiplist_del(kvs_skiplist_t * inst, char *key);
int kvs_skiplist_exist(kvs_skiplist_t * inst, char *key);


#endif



void *kvs_malloc(size_t size);
void kvs_free(void *ptr);




// Mechanism of AOF****************//
int kvs_log_init(msg_handler handler);
int kvs_log_write(client_info * cli);
int kvs_log_close();
//*********************************//


// Mechanism of SAVE****************//
int kvs_save_init(msg_handler handler);
int kvs_save_write();
int kvs_save_close();
//*********************************//


#endif



