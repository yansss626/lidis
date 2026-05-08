#include "kvs_sync.h"
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include "nty_coroutine.h"

#define MSG_LENGTH 32
#define BUFFER_SIZE 1024
#define ENTRY_LENGTH 1024



extern kvs_conf_t global_config;


int kvs_connect_to_remote(const char * ip, unsigned int port){
    if (ip == NULL || port > 65535) return -1;

    int ret = 0;
    int sockfd = -1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -2;
    }
    struct sockaddr_in remote = {0};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port);
    remote.sin_addr.s_addr = inet_addr(ip);

    if (connect(sockfd, (struct sockaddr *)&remote, sizeof(struct sockaddr_in)) != 0) {
        perror("connect error");
        close(sockfd);
        return -3;
    }
    return sockfd;
}

int kvs_connect_to_sync(){
    if(global_config.enable_sync == 0) return 0;


    int master_fd = -1;
    int agent_fd = -1;
    int ret = 0;
    char * rdma_buf = NULL;
    int rdma_mod_size = 0; //rdma modified size
    int rdma_buf_size = 0;
    const char * master_ip = global_config.master_ip;
    unsigned int master_port = global_config.master_port;
    const char * agent_ip = global_config.agent_ip;
    unsigned int agent_port = global_config.agent_port;

// full sync
    master_fd =  kvs_connect_to_remote(master_ip, master_port);
    if (master_fd < 0) {
        fprintf(stderr, "Error: master_fd = %d\n", master_fd);
        return -1;
    }
    

    char * cmd = "SYNC";
    char msg[MSG_LENGTH] = {0};
    int length = snprintf(msg, MSG_LENGTH, "*1\r\n$%ld\r\n%s\r\n", strlen(cmd), cmd); // 
    send(master_fd, msg, length, 0);
    

    int n = recv(master_fd, msg, MSG_LENGTH - 1, 0);
    if (n <= 0 || strncmp(msg, "+FULLSYNC ", 10) != 0) {
        ret = -2;
        goto cleanup;
    }
    msg[n] = '\0';
    rdma_buf_size = atoi(msg + 10);

    //printf("size: %d\n", rdma_buf_size);
    if (rdma_buf_size > 0) {
        rdma_buf = (char *)malloc(rdma_buf_size + 1);
        if (rdma_buf == NULL) {
            perror("malloc");
            ret = -3;
            goto cleanup;
        }
        memset(rdma_buf, 0, rdma_buf_size + 1);

        rdma_mod_size = rdma_server(global_config.rdma_port, rdma_buf, rdma_buf_size, master_fd);
        kvs_file_read(rdma_buf, rdma_mod_size, kvs_save_handler);        
    }
    close(master_fd);
    master_fd = 1;
// end of full sync
  
// incr sync
    agent_fd = kvs_connect_to_remote(agent_ip, agent_port);
    if (agent_fd < 0) {
        fprintf(stderr, "Error: agent_fd = %d\n", agent_fd);
        return -1;
    }


#if (NETWORK_SELECT == NETWORK_NTYCO)
    client_info * cli_info = client_info_init(agent_fd);
    cli_info->role = 1; // slave;
    nty_coroutine * read_co = NULL;
    nty_coroutine_create(&read_co, server_reader, cli_info);
#endif
// end of sync
    

    cleanup:
        if(ret != 0 && master_fd > 0) close(master_fd);
        if(rdma_buf != NULL) free(rdma_buf);
        return ret;
}

int kvs_full_sync(client_info * cli){
    if (global_config.enable_sync == 0) return 0;
    if (cli == NULL) return -1;



    int ret = 0;
    struct io_uring ring = {0};
    int fd = 0;
    char * ptr = NULL;
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
        if(ret != 0 && fd > 0) close(fd);
        if(ptr != MAP_FAILED) munmap(ptr, statbuf.st_size);
        return ret;
}

int kvs_incr_sync(client_info * cli){
    if(cli == NULL) return -1;
    //printf("cli->rbuf: %s\n", cli->rbuf);
    return 0;
}



