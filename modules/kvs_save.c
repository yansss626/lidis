
#include <stdio.h>
#include "kvstore.h"

#if ENABLE_MODULE_SAVE

#define BUFFER_SIZE 1024

msg_handler kvs_handler;



int kvs_save_init(msg_handler handler){
    kvs_handler = handler;
    FILE * fp = NULL;
#if ENABLE_ARRAY
    fp = fopen("./modules/kvs_array.txt", "r");
    if(fp != NULL) {
        kvs_save_read(fp);
        fclose(fp);
    }

#endif

#if ENABLE_HASH
    fp = fopen("./modules/kvs_hash.txt", "r");
    if(fp != NULL) {
        kvs_save_read(fp);
        fclose(fp);
    }
#endif

#if ENABLE_RBTREE
    fp = fopen("./modules/kvs_rbtree.txt", "r");
    if(fp != NULL) {
        kvs_save_read(fp);
        fclose(fp);
    }
#endif


    return 0;
}

#if ENABLE_RBTREE
void kvs_save_write_rbtree(rbtree *T, rbtree_node *node, FILE * fp) {
    if(T == NULL || fp == NULL) return;
	if (node != T->nil) {
        fprintf(fp, "RSET %s %s\r\n", node->key, (char *)node->value);
		kvs_save_write_rbtree(T, node->left, fp);
  
		kvs_save_write_rbtree(T, node->right, fp);
	}

}
#endif

int kvs_save_write(void * arg, KVS_SAVE_TYPE cmd_type){
    if(arg == NULL) return -1;
    FILE * fp = NULL;

#if ENABLE_RBTREE    
    if(cmd_type == SAVE_RBTREE) {
        kvs_rbtree_t * inst = (kvs_rbtree_t *) arg; 
        fp = fopen("./modules/kvs_rbtree.txt", "w");
        if(fp == NULL) return -2;
        kvs_save_write_rbtree(inst, inst->root, fp);
        fflush(fp);
        fclose(fp);
    }
#endif

#if ENABLE_HASH
    if(cmd_type == SAVE_HASH){
        kvs_hash_t * inst = (kvs_hash_t *)arg;
        fp = fopen("./modules/kvs_hash.txt", "w");
        if(fp == NULL) return -2;    
        for (int i = 0;i < inst->max_slots;i ++) {
            hashnode_t *node = inst->nodes[i];
            while (node != NULL) { 
                fprintf(fp, "HSET %s %s\r\n", node->key, node->value);
                
                node = node->next;
                
            }
        }   
        fflush(fp);
        fclose(fp);
    }
#endif

#if ENABLE_ARRAY
    if(cmd_type == SAVE_ARRAY){
        kvs_array_t * inst = (kvs_array_t *)arg;
        fp = fopen("./modules/kvs_array.txt", "w");
        if(fp == NULL) return -2;
        for (int i = 0;i < inst->total;i ++) {
            if (inst->table[i].key != NULL) {

                fprintf(fp, "SET %s %s\r\n", inst->table[i].key, inst->table[i].value);      
                
            }
        }
        fflush(fp);
        fclose(fp);
    }
       
#endif
    
    return 0;
    

}

int kvs_save_read(FILE * fp){
    if(fp == NULL) return -1;

    char buffer[BUFFER_SIZE] = {0};
    char response[BUFFER_SIZE] = {0};
    while(fgets(buffer, BUFFER_SIZE, fp) != NULL){
        int length = strlen(buffer);
        buffer[length - 2] = '\0';
        kvs_handler(buffer, length, response);
        memset(buffer, 0, length);
    }

    return 0;

    
}

#endif