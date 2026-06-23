#ifndef __KVS_AGENT_H
#define __KVS_AGENT_H

typedef struct kvs_slave_item_s{
    int fd;
}kvs_slave_item;

typedef struct kvs_slaves_s{
    kvs_slave_item * table;
    int total;
    int size;

}kvs_slaves;

#define BUFFER_SIZE (4 * 1024) // 4KB

struct tcp_event {
    char payload[BUFFER_SIZE];
    int payload_len;
    int ret;
};

int kvs_slaves_create(kvs_slaves * inst);
int kvs_slaves_insert(kvs_slaves * inst, int fd);
int kvs_slaves_destroy(kvs_slaves * inst);
int kvs_slaves_delete(kvs_slaves * inst, int fd);

#endif