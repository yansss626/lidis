#ifndef __KSV_SYNC_H__
#define __KSV_SYNC_H__

#include "kvstore.h"




void server_reader(void *arg);

client_info * client_info_init(int fd);

int kvs_connect_to_sync();



int kvs_full_sync(client_info * cli);
int kvs_incr_sync(client_info * cli);

int rdma_client(const char * server_ip, const char * port, char * ptr, size_t size);
int rdma_server(const char * port, char * rdma_buf, size_t size, int sockfd);



#endif