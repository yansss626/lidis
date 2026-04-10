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
#define BUFFER_SIZE 1024
kvs_slaves global_slaves = {0};


#if ENABLE_ARRAY
extern kvs_array_t global_array;
#endif

#if ENABLE_RBTREE
extern kvs_rbtree_t global_rbtree;
#endif

#if ENABLE_HASH
extern kvs_hash_t global_hash;
#endif
int kvs_write_snapshot(FILE * fp);

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

    FILE * fp = fopen("kvs_snapshot.txt", "w+");
    if(kvs_write_snapshot(fp) > 0){
        fseek(fp, 0, SEEK_SET);
        int payload_length = 0;
        char * buffer = (char *)kvs_malloc(BUFFER_SIZE);
        int cap = BUFFER_SIZE;
        if(buffer == NULL) return -2;
        memset(buffer, 0, BUFFER_SIZE);
        while(fscanf(fp, "%d*", &payload_length) > 0){
            int head_len = sprintf(buffer, "%d*", payload_length);
            int total_len = payload_length + head_len;
            if(cap < total_len - 1){
                char * temp = realloc(buffer, total_len);
                if(temp == NULL){
                    perror("realloc error");
                    return -2;
                }
                cap = total_len;
                buffer = temp;
            }
            fread(buffer + head_len, 1, payload_length, fp);
            buffer[total_len] = '\0';
            int ret = send(cli->fd, buffer, total_len, 0);
        }
        kvs_free(buffer);
    }
    else{
        printf("NO need to sync\n");
    }
    fclose(fp);
    
    


    return 0;
}





#if ENABLE_RBTREE
int kvs_write_snapshot_rbtree(rbtree *T, rbtree_node *node, FILE * fp) {
    if(T == NULL || fp == NULL) return -1;
    int payload_length = 0;
	if (node != T->nil) {
        payload_length = strlen(node->key) + strlen((char *)node->value) + 2 + strlen("RSET");
        //fprintf(fp, "RSET %s %s\r\n", node->key, (char *)node->value);
        fprintf(fp, "%d*RSET %s %s\r\n", payload_length, node->key, (char *)node->value);
		kvs_write_snapshot_rbtree(T, node->left, fp);
  
		kvs_write_snapshot_rbtree(T, node->right, fp);
	}
    return payload_length;
}
#endif

int kvs_write_snapshot(FILE * fp){
    if(fp == NULL) return -1;
    int payload_length = 0;
#if ENABLE_RBTREE    
    kvs_rbtree_t * R_inst = &global_rbtree; 
    payload_length = kvs_write_snapshot_rbtree(R_inst, R_inst->root, fp);
#endif
    
#if ENABLE_HASH
    kvs_hash_t *  H_inst = &global_hash;
    if(H_inst->count > 0){
        for (int i = 0;i < H_inst->max_slots;i ++) {
            hashnode_t *node = H_inst->nodes[i];
            while (node != NULL) { 
                payload_length = strlen(node->key) + strlen(node->value) + 2 + strlen("HSET");
                //fprintf(fp, "HSET %s %s\r\n", node->key, node->value);
                fprintf(fp, "%d*HSET %s %s\r\n", payload_length, node->key, (char *)node->value);
                node = node->next;
                
            }
        }
    }
   
#endif

#if ENABLE_ARRAY

    kvs_array_t * inst = &global_array;
    if(inst->total > 0){
        for (int i = 0;i < KVS_ARRAY_SIZE;i ++) {
            if (inst->table[i].key != NULL) {
                payload_length = strlen(inst->table[i].key) + strlen(inst->table[i].value) + 2 + strlen("SET");
                //fprintf(fp, "SET %s %s\r\n", inst->table[i].key, inst->table[i].value);      
                fprintf(fp, "%d*SET %s %s\r\n", payload_length, inst->table[i].key, inst->table[i].value);
            }
        }
    }

       
#endif
    return payload_length;
}


int kvs_incr_sync(kvs_slaves * inst, client_info * cli){
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

#endif