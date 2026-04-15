
#include <stdio.h>
#include "kvstore.h"
#include <string.h>
#include <stdlib.h>
#if ENABLE_MODULE_SAVE

#define BUFFER_SIZE 1024

static msg_handler kvs_handler;

#if ENABLE_ARRAY
extern kvs_array_t global_array;
#endif

#if ENABLE_RBTREE
extern kvs_rbtree_t global_rbtree;
#endif

#if ENABLE_HASH
extern kvs_hash_t global_hash;
#endif

#if ENABLE_SKIPLIST
extern kvs_skiplist_t global_skiplist;
#endif

int kvs_save_init(msg_handler handler){
    kvs_handler = handler;
    FILE * fp = fopen("./kvs-module/kvs_dump.rdb", "r"); 
    if(fp == NULL) return -2;

    kvs_save_read(fp);
    fclose(fp);

    return 0;
}

#if ENABLE_RBTREE
void kvs_save_write_rbtree(rbtree *T, rbtree_node *node, FILE * fp) {
    if(T == NULL || fp == NULL) return;
    int payload_length = 0;
	if (node != T->nil) {
        payload_length = strlen(node->key) + strlen((char *)node->value) + 2 + strlen("RSET");
        //fprintf(fp, "RSET %s %s\r\n", node->key, (char *)node->value);
        fprintf(fp, "%d*RSET %s %s\r\n", payload_length, node->key, (char *)node->value);
		kvs_save_write_rbtree(T, node->left, fp);
  
		kvs_save_write_rbtree(T, node->right, fp);
	}

}
#endif

int kvs_save_write(){

    FILE * fp = fopen("./kvs-module/kvs_dump.rdb", "w+"); 
    if(fp == NULL) return -2;
#if ENABLE_RBTREE    
    kvs_rbtree_t * R_inst = &global_rbtree;
    kvs_save_write_rbtree(R_inst, R_inst->root, fp);
#endif

#if ENABLE_HASH
    kvs_hash_t * H_inst = &global_hash;
    if(H_inst->count > 0){
        for (int i = 0;i < H_inst->max_slots;i ++) {
            hashnode_t *node = H_inst->nodes[i];
            while (node != NULL) { 
                int payload_length = strlen(node->key) + strlen(node->value) + 2 + strlen("HSET");
                //fprintf(fp, "HSET %s %s\r\n", node->key, node->value);
                fprintf(fp, "%d*HSET %s %s\r\n", payload_length, node->key, (char *)node->value);
                node = node->next;
                
            }
        }   
    }
    
#endif

#if ENABLE_ARRAY

    kvs_array_t * inst = &global_array;
    if(inst->table > 0){
        for (int i = 0;i < KVS_ARRAY_SIZE;i ++) {
            if (inst->table[i].key != NULL) {
                int payload_length = strlen(inst->table[i].key) + strlen(inst->table[i].value) + 2 + strlen("SET");
                //fprintf(fp, "SET %s %s\r\n", inst->table[i].key, inst->table[i].value);      
                fprintf(fp, "%d*SET %s %s\r\n", payload_length, inst->table[i].key, inst->table[i].value);
            }
        }
    }

       
#endif

#if ENABLE_SKIPLIST

        kvs_skiplist_t * L_inst = &global_skiplist;
        Node * current = L_inst->header->forward[0];
        while(current != NULL){
            int payload_length = strlen(current->key) + strlen(current->value) + 2 + strlen("LSET");
            fprintf(fp, "%d*LSET %s %s\r\n", payload_length, current->key, current->value);
            current = current->forward[0];
        }


#endif

    fflush(fp);
    fclose(fp); 
    return 0;

}

// int kvs_save_read(FILE * fp){
//     if(fp == NULL) return -1;

//     client_info cli = {0};
//     cli.w_cap = 128; 
//     cli.wbuf = (char *)malloc(cli.w_cap);
//     if(cli.wbuf == NULL) return -1;

//     cli.rbuf = (char *)malloc(4096);
//     if(cli.rbuf == NULL) {
//         free(cli.wbuf);
//         return -1;
//     }
//     memset(cli.rbuf, 0, 4096);
//     cli.r_cap = 4096;
    


//     while(fgets(cli.rbuf + cli.r_pos, cli.r_cap - cli.r_pos - 1, fp) != NULL){
//         cli.r_pos += strlen(cli.rbuf + cli.r_pos);
        

//         if(cli.r_pos >=2 && cli.rbuf[cli.r_pos - 2] == '\r' && cli.rbuf[cli.r_pos - 1] == '\n'){
            
//             cli.rbuf[cli.r_pos - 2] = '\0';    
//             //printf("%s", cli.rbuf);
//             kvs_handler(&cli);
//             cli.r_pos = 0;
//             continue;
//         }
//         if(cli.r_cap - cli.r_pos < 128){
//             cli.r_cap *= 2;
//             char * temp  = (char *)realloc(cli.rbuf, cli.r_cap);
//             if(temp == NULL) break;
//             cli.rbuf = temp;
//         }
       


//     }

//     free(cli.wbuf);
//     free(cli.rbuf);
//     return 0;

    
// }



int kvs_save_read(FILE * fp){
    if(fp == NULL) return -1;

    client_info cli = {0};
    cli.w_cap = 128 - 1; 
    cli.wbuf = (char *)malloc(128);
    if(cli.wbuf == NULL) return -1;

    cli.r_cap = 4096 - 1; 
    cli.rbuf = (char *)malloc(4096);
    if(cli.rbuf == NULL) {
        free(cli.wbuf);
        return -1;
    }

    int payload_length = 0;
    while(fscanf(fp, "%d*", &payload_length) == 1){
        if(cli.r_cap < payload_length){
            char * temp = (char *)realloc(cli.rbuf, payload_length + 1);
            if(temp == NULL){
                break;
            }
            cli.rbuf = temp;
            cli.r_cap = payload_length;
        }

        int len = fread(cli.rbuf, 1, payload_length, fp);
        if(len == payload_length){
            cli.rbuf[payload_length] = '\0';
            kvs_handler(&cli);
        }

    }
    free(cli.rbuf);
    free(cli.wbuf);
    return 0;
}

#endif