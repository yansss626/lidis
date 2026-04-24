#ifndef __KVS_AGENT_H
#define __KVS_AGENT_H

#define KVS_MAX_COMMAND_LENGTH 1024

typedef struct kvs_slave_item_s{
    int fd;
}kvs_slave_item;
typedef struct kvs_slaves_s{
    kvs_slave_item * table;
    int total;
    int size;

}kvs_slaves;

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

struct event{
	char command[KVS_MAX_COMMAND_LENGTH]; // kvstore command
	int total_length; // kvstore command total length
};



int kvs_slaves_create(kvs_slaves * inst);
int kvs_slaves_insert(kvs_slaves * inst, int fd);
int kvs_slaves_destroy(kvs_slaves * inst);
int kvs_slaves_delete(kvs_slaves * inst, int fd);

#endif