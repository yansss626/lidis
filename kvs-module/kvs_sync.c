#include "kvs_sync.h"
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include "nty_coroutine.h"

#if ENABLE_MODULE_SYNC

#define MSG_LENGTH 32
#define SYNC_SIZE 32

kvs_slaves global_slaves = {0};

int kvs_connect_to_master(char * ip, unsigned short port){
    if(ip == NULL) return -1;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) return -2;

    struct sockaddr_in remote = {0};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port);
    remote.sin_addr.s_addr = inet_addr(ip);

    if(connect(sockfd, (struct sockaddr *)&remote, sizeof(struct sockaddr_in)) != 0){
        perror("connect error");
        return -3;
    }   

    char * cmd = "SYNC";
    char msg[MSG_LENGTH] = {0};
    int length = snprintf(msg, MSG_LENGTH, "%ld*%s", strlen(cmd), cmd); // add protocol <length>*<cmd>
    send(sockfd, msg, length, 0);


#if (NETWORK_SELECT == NETWORK_NTYCO)
    client_info * cli_info = client_info_init(sockfd);
    cli_info->role = 1; // slave;
    nty_coroutine * read_co = NULL;
    nty_coroutine_create(&read_co, server_reader, cli_info);
#endif


    return 0;
}

int kvs_full_sync(kvs_slaves * inst, client_info * cli){
    if(cli == NULL || inst == NULL) return -1;
    if(inst->table == NULL){
        kvs_slaves_create(inst);
    }
    kvs_slaves_insert(inst, cli->fd);
    cli->role = 1;

    





    return 0;
}

int kvs_incr_sync(kvs_slaves * inst, client_info * cli, int count, char ** tokens){
    if(inst == NULL || inst->table == NULL || cli == NULL) return -1;
    for(int i = 0; i < count - 1; i++){
        int pos = strlen(tokens[i]);
        (tokens[i])[pos] = ' ';
    } // repair cli->rbuf due to kvs_split_token


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

#endif