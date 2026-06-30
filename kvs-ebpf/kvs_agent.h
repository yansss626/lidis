#ifndef __KVS_AGENT_H
#define __KVS_AGENT_H

#include <stddef.h>

#define KVS_AGENT_PAYLOAD_SIZE (4 * 1024)   // 4KB

enum kvs_slave_state{
    KVS_SLAVE_UNKNOWN,
    KVS_SLAVE_FULL_SYNC,
    KVS_SLAVE_INCR_SYNC
};

typedef struct kvs_slave_item_s{
    int fd;
}kvs_slave_item;

typedef struct kvs_slaves_s{
    kvs_slave_item * table;
    size_t total;
    size_t size;
}kvs_slaves;

typedef struct agent_cache_s {
    size_t read_pos;
    size_t write_pos;
    size_t capacity;
    size_t used;
    char * cache;
}agent_cache;

typedef struct agent_info_t {
    char * buf;
    size_t buf_cap;
    size_t buf_pos;
	int cmd_tl; // total length includes head_length plus payload_length(data_length) 
	int cmd_hl; // head_length
    enum kvs_slave_state state;
    agent_cache fullsync_cache;
}agent_info;

struct tcp_event {
    char payload[KVS_AGENT_PAYLOAD_SIZE];
    int payload_len;
    int ret;
};

int kvs_slaves_create(kvs_slaves * inst, size_t size);
int kvs_slaves_insert(kvs_slaves * inst, int fd);
int kvs_slaves_destroy(kvs_slaves * inst);
int kvs_slaves_delete(kvs_slaves * inst, int fd);

int kvs_agent_cache_create(agent_cache * ac, size_t cache_size);
void kvs_agent_cache_destroy(agent_cache * ac);
int kvs_agent_cache_write(agent_cache * ac, const char * buf, size_t buf_len);
char * kvs_agent_cache_read(agent_cache * ac, size_t * buf_len);

int kvsp_parse_bulk_size(const char * buf, size_t buf_size, int * head_len);

#define KVSP_START_STR	"#"
#define KVSP_START_STR_LEN	1

#define KVSP_HEAD_TAIL_STR	"\r\n"
#define KVSP_HEAD_TAIL_STR_LEN	2

#define KVSP_TOK_LEN_START_CHAR	'^'
#define KVSP_TOK_START_CHAR	'&'

#define KVSP_END_STR	"\r\n"
#define	KVSP_END_STR_LEN	2

#endif