#include "kvs_sync.h"
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include "nty_coroutine.h"

#define MSG_LENGTH 32
#define SYNC_SIZE 32 // Max size for slaves count
#define BUFFER_SIZE 1024
#define ENTRY_LENGTH 1024

kvs_slaves global_slaves = {0};

extern kvs_conf_t global_config;


int kvs_connect_to_master(const char * ip, unsigned short port){
    if(global_config.enable_sync == 0) return 0;
    if(ip == NULL) return -1;

    int sockfd = -1;
    int ret = 0;
    char * rdma_buf;
    int rdma_mod_size = 0; //rdma modified size
    int rdma_buf_size = 0;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        ret = -2; 
        goto cleanup;
    }

    struct sockaddr_in remote = {0};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port);
    remote.sin_addr.s_addr = inet_addr(ip);

    if(connect(sockfd, (struct sockaddr *)&remote, sizeof(struct sockaddr_in)) != 0){
        perror("connect error");
        ret = -3;
        goto cleanup;
    }
    

    char * cmd = "SYNC";
    char msg[MSG_LENGTH] = {0};
    int length = snprintf(msg, MSG_LENGTH, "*1\r\n$%ld\r\n%s\r\n", strlen(cmd), cmd); // 
    send(sockfd, msg, length, 0);
    

    int n = recv(sockfd, msg, MSG_LENGTH - 1, 0);
    if(n <= 0 || strncmp(msg, "+FULLSYNC ", 10) != 0){
        ret = -4;
        goto cleanup;
    }
    msg[n] = '\0';
    rdma_buf_size = atoi(msg + 10);

    //printf("size: %d\n", rdma_buf_size);
    if(rdma_buf_size > 0){
        rdma_buf = (char *)malloc(rdma_buf_size + 1);
        if(rdma_buf == NULL){
            perror("malloc");
            ret = -5;
            goto cleanup;
        }
        memset(rdma_buf, 0, rdma_buf_size + 1);

        rdma_mod_size = rdma_server(global_config.rdma_port, rdma_buf, rdma_buf_size, sockfd);
        kvs_file_read(rdma_buf, rdma_mod_size, kvs_save_handler);        
    }


#if (NETWORK_SELECT == NETWORK_NTYCO)
    client_info * cli_info = client_info_init(sockfd);
    cli_info->role = 1; // slave;
    nty_coroutine * read_co = NULL;
    nty_coroutine_create(&read_co, server_reader, cli_info);
#endif

    

    cleanup:
        if(ret != 0 && sockfd > 0) close(sockfd);
        if(rdma_buf != NULL) free(rdma_buf);
        return ret;
}

int kvs_full_sync(kvs_slaves * inst, client_info * cli){
    if (global_config.enable_sync == 0) return 0;
    if (cli == NULL || inst == NULL) return -1;
    if (inst->table == NULL) {
        kvs_slaves_create(inst);
    }
    kvs_slaves_insert(inst, cli->fd);
    cli->role = 1;

    int ret = 0;
    struct io_uring ring = {0};
    int fd = 0;
    char * ptr;
    if(io_uring_queue_init(ENTRY_LENGTH, &ring, 0) < 0){
        perror("io_uring_queue_init");
        ret = -2;
        goto cleanup;
    }
        
    
    fd = open("kvs_snapshot.rdb", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if(fd < 0){
        perror("open");
        ret = -3;
        goto cleanup;
    }

    io_write_ctx main_ctx = {0};
    main_ctx.ring = &ring;
    main_ctx.tasks_count = 0;
    if(kvs_traversal_write(fd, &main_ctx) < 0) {
        ret = -4; 
        goto cleanup;
    }

    struct stat statbuf = {0};
    fstat(fd, &statbuf);

    char reply[MSG_LENGTH];
    int len = snprintf(reply, MSG_LENGTH, "+FULLSYNC %ld\r\n", statbuf.st_size);
    send(cli->fd, reply, len, 0);
    if(statbuf.st_size == 0) goto cleanup;

    len = recv(cli->fd, reply, MSG_LENGTH - 1, 0);
    if(len <= 0 || strncmp(reply, "+READY", 6) != 0 ) {
        ret = -6; 
        goto cleanup;
    }
    //printf("reply: %s\n", reply);

    ptr = (char *)mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if(ptr == MAP_FAILED){
        perror("mmap");
        ret = -5;
        goto cleanup;
    }
    
    rdma_client(global_config.rdma_server_ip, global_config.rdma_port, ptr, statbuf.st_size);
    

    cleanup:
        
        if(ret != -2) io_uring_queue_exit(&ring);
        if(ret != 0 && fd > 0) {kvs_slaves_delete(&global_slaves, fd); close(fd);}
        if(ptr != MAP_FAILED) munmap(ptr, statbuf.st_size);
        return ret;
}










int kvs_incr_sync(kvs_slaves * inst, client_info * cli){
    if(global_config.enable_sync == 0) return 0;
    if(inst == NULL || inst->table == NULL || cli == NULL) return -1;



    int synced_num = 0;
    for(int i = 0; i < inst->size; i++){
        int fd = inst->table[i].fd;
        if(fd > 0){
            send(fd, cli->rbuf, cli->cmd_tl, 0);
            synced_num++;
            if(synced_num == inst->total) break;
        }
        
    }   



    return 0;
}









int kvs_slaves_create(kvs_slaves * inst){

    if(inst == NULL) return -1;
    if(inst->table != NULL) return 0;

    inst->table = (kvs_slave_item *)kvs_malloc(sizeof(kvs_slave_item) * SYNC_SIZE);
    if(inst->table == NULL) {
        printf("kvs_malloc error\n");
        return -2;
    }
    memset(inst->table, 0, sizeof(kvs_slave_item) * SYNC_SIZE);

    inst->total = 0;
    inst->size = SYNC_SIZE;

    return 0;
}


int kvs_slaves_insert(kvs_slaves * inst, int fd){
    if(inst == NULL) return -1;
    if(inst->total == inst->size) return -2;

    int i = 0;
    for(; i < inst->size; i++){
        if(inst->table[i].fd == 0) {
            inst->table[i].fd = fd;
            break;
        }
    }

    ++(inst->total);
    
    return 0;
}

int kvs_slaves_delete(kvs_slaves * inst, int fd){
    if(inst == NULL) return -1;
    if(inst->total == 0) return 0;
    int i = 0;
    for(; i < inst->size; i++){
        if(inst->table[i].fd == fd) {
            inst->table[i].fd = 0;
            --(inst->total);
            break;
        }
    }




    return 0;
}

int kvs_slaves_destroy(kvs_slaves * inst){

    if(inst == NULL) return -1;
    
    if(inst->table != NULL) kvs_free(inst->table);
    inst->table = NULL;

    return 0;
}

