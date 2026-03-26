
#include "kvstore.h"
#include <stdio.h>

#if ENABLE_MODULE_LOG
#define BUFFER_SIZE 1024
static FILE * fp_array = NULL;
static FILE * fp_hash = NULL;
static FILE * fp_rbtree = NULL;


msg_handler kvs_handler;

int kvs_log_read(FILE * fp){
    if(fp == NULL) return -1;

    char buffer[BUFFER_SIZE] = {0};
    char response[BUFFER_SIZE] = {0};
    while(fgets(buffer, BUFFER_SIZE, fp) != NULL){
        int length = strlen(buffer);
        buffer[length - 2] = '\0';
        kvs_handler(buffer, length, response);
        memset(buffer, 0, BUFFER_SIZE);
    }

}




int kvs_log_init(msg_handler handler){
    kvs_handler = handler;
#if ENABLE_ARRAY
    fp_array = fopen("./modules/kvs_array.log", "a+");
    if(fp_array == NULL) return -1;
    fseek(fp_array, 0, SEEK_SET);
    kvs_log_read(fp_array);
#endif

#if ENABLE_HASH
    fp_hash = fopen("./modules/kvs_hash.log", "a+");
    if(fp_hash == NULL) return -1;
    fseek(fp_hash, 0, SEEK_SET);
    kvs_log_read(fp_hash);
#endif

#if ENABLE_RBTREE
    fp_rbtree = fopen("./modules/kvs_rbtree.log", "a+");
    if(fp_rbtree == NULL) return -1;
    fseek(fp_rbtree, 0, SEEK_SET);
    kvs_log_read(fp_rbtree);
#endif

    return 0;

}


int kvs_log_write(KVS_LOG_TYPE cmd_type, char * kvs_cmd, char * key, char * value){
    if (kvs_cmd == NULL || key == NULL || value == NULL) return -1;

    FILE * fp = NULL;
    if(cmd_type == LOG_ARRAY) fp = fp_array;
    else if(cmd_type == LOG_HASH) fp = fp_hash;
    else if(cmd_type == LOG_RBTREE) fp = fp_rbtree;
    if(fp == NULL) return -2;
    
    fprintf(fp, "%s %s %s\r\n", kvs_cmd, key, value);
    fflush(fp);
    return 0;
}

int kvs_log_close(){
    if(fp_array != NULL) fclose(fp_array);
    if(fp_hash != NULL) fclose(fp_hash);
    if(fp_rbtree != NULL) fclose(fp_rbtree);
    return 0;
}

#endif