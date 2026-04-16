
#include "kvstore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define BUFFER_SIZE 1024

static FILE * fp_log = NULL;

static msg_handler kvs_handler;

extern kvs_conf_t global_config;

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
    if(global_config.enable_log == 0) return 0;
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
    if(global_config.enable_log == 0) return 0;
    kvs_handler = handler;

    fp_log = fopen("./kvs-module/kvs_appendonly.aof", "a+");
    if(fp_log == NULL) return -1;
    fseek(fp_log, 0, SEEK_SET);
    kvs_log_read(fp_log);

    return 0;

}



int kvs_log_write(client_info * cli){
    if(global_config.enable_log == 0) return 0;
    if (cli == NULL) return -1;
    if (fp_log == NULL) return -2;
    fprintf(fp_log, "%s\r\n", cli->rbuf);
    fflush(fp_log);
    return 0;
}

int kvs_log_close(){
    if(global_config.enable_log == 0) return 0;
    if(fp_log != NULL) fclose(fp_log);
    return 0;
}

