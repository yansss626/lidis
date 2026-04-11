#ifndef __KSV_SYNC_H__
#define __KSV_SYNC_H__

#include "kvstore.h"

#if ENABLE_MODULE_SYNC
    typedef struct kvs_slave_item_s{
        int fd;
    }kvs_slave_item;
    typedef struct kvs_slaves_s{
        kvs_slave_item * table;
        int total;
        int size;

    }kvs_slaves;

    void server_reader(void *arg);
    client_info * client_info_init(int fd);
    int kvs_connect_to_master(char * ip, unsigned short port);

    int kvs_slaves_create(kvs_slaves * inst);
    int kvs_slaves_insert(kvs_slaves * inst, int fd);
    int kvs_slaves_delete(kvs_slaves * inst, int fd);
    int kvs_slaves_destroy(kvs_slaves * inst);
    int kvs_full_sync(kvs_slaves * inst, client_info * cli);
    int kvs_incr_sync(kvs_slaves * inst, client_info * cli);

#else
    #define kvs_connect_to_master(ip, port) (0)
    #define kvs_slaves_delete(inst, fd) (0)
    #define kvs_full_sync(inst, cli)    (0)
    #define kvs_incr_sync(inst, cli) (0)
    #define kvs_slaves_destroy(inst)    (0)
#endif



#endif