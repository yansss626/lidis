
#include "kvstore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <liburing.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

#define ENTRY_LENGTH 1024

static int fd_log = -1;

static struct io_uring ring = {0};
static int ring_inited = 0;


static msg_handler kvs_handler;

extern kvs_conf_t global_config;


typedef struct log_write_ctx_s{
    char * buf;
    size_t len;
}log_write_ctx;


int kvs_log_read(char * ptr, size_t size){
    if(ptr == NULL || size <= 0) return -1;
    client_info cli = {0};

    cli.w_cap = BUFFER_SIZE; 
    cli.wbuf = (char *)malloc(BUFFER_SIZE + 1);
    if(cli.wbuf == NULL) return -2;

    cli.r_cap = BUFFER_SIZE; 
    cli.rbuf = (char *)malloc(BUFFER_SIZE + 1);
    if(cli.rbuf == NULL) {
        free(cli.wbuf);
        return -2;
    }



    int pos = 0;
    int total_len = 0;
    while(pos < size){
        int head_len = 0;
        total_len = resp_parse_bulk_size(ptr, size - total_len, &head_len);
        if(total_len <= 0){
            printf("Log file error\n");
            break;
        }
        if(total_len > cli.r_cap){
            char * temp = (char *)realloc(cli.rbuf, total_len + 1);
            if(temp == NULL){
                break;
            }
            cli.rbuf = temp;
            cli.r_cap = total_len;            
        }
        cli.cmd_hl = head_len;
        memcpy(cli.rbuf , ptr, total_len);
        cli.rbuf[total_len] = '\0';
        //printf("cli.rbuf: %s\n", cli.rbuf);
        kvs_handler(&cli);

        pos += total_len;
        ptr += total_len;

    }

    free(cli.rbuf);
    free(cli.wbuf);

    return 0;
}

int kvs_uring_init(){
    int ret = io_uring_queue_init(ENTRY_LENGTH, &ring, 0);
    if(ret < 0){
        fprintf(stderr, "io_uring_queue_init error: %s\n", strerror(-ret));
        return -1;
    }
    ring_inited = 1;
    return 0;

}

int kvs_log_init(msg_handler handler){
    if(global_config.enable_log == 0) return 0;
    fd_log = open("./kvs-module/kvs_appendonly.aof", O_RDWR | O_CREAT | O_APPEND, 0644);
    if(fd_log < 0){
        perror("open");
        return -1;
    }
    kvs_uring_init();
    kvs_handler = handler;
    struct stat statbuf = {0};
    fstat(fd_log, &statbuf);
    if(statbuf.st_size <= 0) return 0;
    
    char * ptr = (char *)mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd_log, 0);
    if(ptr == MAP_FAILED){
        perror("mmap");
        close(fd_log);
        fd_log = -1;
        return -1;
    }
    madvise(ptr, statbuf.st_size, MADV_SEQUENTIAL);
    kvs_log_read(ptr, statbuf.st_size);

    munmap(ptr, statbuf.st_size);


    

    return 0;
}



int kvs_log_write(client_info * cli){
    if(global_config.enable_log == 0) return 0;
    if(cli == NULL) return -1;
    if(fd_log < 0 || ring_inited != 1) return -2;
    struct io_uring_cqe * cqe = NULL;
    
    while(io_uring_peek_cqe(&ring, &cqe) == 0){
        log_write_ctx * ctx = (log_write_ctx *)io_uring_cqe_get_data(cqe);
        free(ctx->buf);
        free(ctx);
        ctx = NULL;
        io_uring_cqe_seen(&ring, cqe);
    }
    
    struct io_uring_sqe * sqe = io_uring_get_sqe(&ring);
    if(sqe == NULL){
        io_uring_submit(&ring);
        return -4;
    }
    log_write_ctx * ctx = (log_write_ctx *)malloc(sizeof(log_write_ctx));
    if(ctx == NULL){
        perror("malloc");
        return -3;
    }
    ctx->buf = (char *)malloc(cli->cmd_tl);
    if(ctx->buf == NULL){
        perror("malloc");
        free(ctx);
        return -3;
    }
    
    ctx->len = cli->cmd_tl;
    memcpy(ctx->buf, cli->rbuf, cli->cmd_tl);
    //printf("ctx->buf: %s\n", ctx->buf);
    
    io_uring_prep_write(sqe, fd_log, ctx->buf, ctx->len, 0);
    io_uring_sqe_set_data(sqe, ctx);
    io_uring_submit(&ring);



    return 0;
}

int kvs_log_close(){
    if(global_config.enable_log == 0) return 0;
    if(ring_inited == 1){
        io_uring_submit(&ring);
        struct io_uring_cqe * cqe = NULL;
        while(io_uring_peek_cqe(&ring, &cqe) == 0){
            log_write_ctx * ctx = io_uring_cqe_get_data(cqe);
            free(ctx->buf);
            free(ctx);
            ctx = NULL;
            io_uring_cqe_seen(&ring, cqe);
        }        
        io_uring_queue_exit(&ring);
    } 
    if(fd_log >= 0) close(fd_log);
    return 0;
}