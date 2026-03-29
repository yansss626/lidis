
#include "kvstore.h"
#include <stdio.h>

#if ENABLE_MODULE_LOG
#define BUFFER_SIZE 1024
static FILE * fp_array = NULL;
static FILE * fp_hash = NULL;
static FILE * fp_rbtree = NULL;


static msg_handler kvs_handler;

// int kvs_log_read(FILE * fp){
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
//             //printf("%s\n", cli.rbuf);
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

int kvs_log_read(FILE * fp){
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





int kvs_log_init(msg_handler handler){
    kvs_handler = handler;
#if ENABLE_ARRAY
    fp_array = fopen("./kvs-module/kvs_array.log", "a+");
    if(fp_array == NULL) return -1;
    fseek(fp_array, 0, SEEK_SET);
    kvs_log_read(fp_array);
#endif

#if ENABLE_HASH
    fp_hash = fopen("./kvs-module/kvs_hash.log", "a+");
    if(fp_hash == NULL) return -1;
    fseek(fp_hash, 0, SEEK_SET);
    kvs_log_read(fp_hash);
#endif

#if ENABLE_RBTREE
    fp_rbtree = fopen("./kvs-module/kvs_rbtree.log", "a+");
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
    int payload_length = strlen(kvs_cmd) + strlen(key) + strlen(value) + 2;
    fprintf(fp, "%d*%s %s %s\r\n", payload_length, kvs_cmd, key, value);
    //fprintf(fp, "%s %s %s\r\n", kvs_cmd, key, value); // 无协议
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