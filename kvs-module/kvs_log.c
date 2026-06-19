
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

static struct io_uring ring_log = {0};
static int ring_log_inited = 0;


static msg_handler kvs_handler;

extern kvs_conf_t global_config;

static off_t global_log_offset = 0;



int kvs_file_read(char * ptr, size_t size, msg_handler handler){
    if(ptr == NULL || size == 0) return -1;

    client_info cli = {0};

    cli.w_cap = BUFFER_SIZE; 
    cli.wbuf = (char *)kvs_malloc(BUFFER_SIZE + 1);
    if(cli.wbuf == NULL) return -2;

    cli.r_cap = BUFFER_SIZE; 
    cli.rbuf = (char *)kvs_malloc(BUFFER_SIZE + 1);
    if(cli.rbuf == NULL) {
        kvs_free(cli.wbuf);
        return -2;
    }

    cli.protocol = PROTO_KVSP;
    cli.recv_protocol = kvsp_parse_bulk_size;

    size_t pos = 0;

    while(pos < size){
        int head_len = 0;
        int total_len = kvsp_parse_bulk_size(ptr, size - pos, &head_len);
        if(total_len <= 0){
            printf("Log file error\n");
            break;
        }
        if(total_len > cli.r_cap){
            char * temp = (char *)kvs_realloc(cli.rbuf, total_len + 1);
            if(temp == NULL){
                break;
            }
            cli.rbuf = temp;
            cli.r_cap = total_len;            
        }

        cli.cmd_hl = head_len;
        cli.cmd_tl = total_len;

        memcpy(cli.rbuf, ptr, total_len);
        cli.rbuf[total_len] = '\0';
        //printf("cli.rbuf: %s\n", cli.rbuf);

        int ret = handler(&cli);
        if(ret < 0){
            printf("handler error\n");
            break;
        }

        pos += total_len;
        ptr += total_len;

    }

    kvs_free(cli.rbuf);
    kvs_free(cli.wbuf);

    return 0;
}



int kvs_log_init(msg_handler handler){
    if(global_config.enable_log == 0) return 0;
    fd_log = open("./kvs-data/kvs_appendonly.aof", O_RDWR | O_CREAT, 0644);
    if(fd_log < 0){
        perror("open");
        return -1;
    }
    io_uring_queue_init(ENTRY_LENGTH, &ring_log, 0);

    ring_log_inited = 1;

    struct stat statbuf = {0};
    if (fstat(fd_log, &statbuf) < 0) {
        close(fd_log);
        return -1;
    }

    if(statbuf.st_size <= 0) return 0;

    global_log_offset = statbuf.st_size;
    
    char * ptr = (char *)mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd_log, 0);
    if(ptr == MAP_FAILED){
        perror("mmap");
        close(fd_log);
        fd_log = -1;
        return -2;
    }
    kvs_file_read(ptr, statbuf.st_size, handler);

    munmap(ptr, statbuf.st_size);


    

    return 0;
}

int kvs_log_write(char **tokens, int count){
    if(global_config.enable_log == 0) return 0;
    if (tokens[0] == NULL || count <= 0) return -1;
    if(fd_log < 0 || ring_log_inited != 1) return -1;
    
    int buf_len = 0;
    char * buf = kvs_build_kvsp_frame(count, tokens, &buf_len);
    if (buf == NULL || buf_len <= 0) return -2;

    struct io_uring_cqe * cqe = NULL;
    while(io_uring_peek_cqe(&ring_log, &cqe) == 0){
        io_write_ctx * ctx = (io_write_ctx *)io_uring_cqe_get_data(cqe);
        kvs_free(ctx->buf);
        kvs_free(ctx);
        ctx = NULL;
        io_uring_cqe_seen(&ring_log, cqe);
    }
    
    struct io_uring_sqe * sqe = io_uring_get_sqe(&ring_log);
    if(sqe == NULL){
        io_uring_submit(&ring_log);
        kvs_free(buf);
        return -2;
    }

    io_write_ctx * ctx = (io_write_ctx *)kvs_malloc(sizeof(io_write_ctx));
    if(ctx == NULL){
        perror("kvs_malloc");
        kvs_free(buf);
        return-2;
    }
    ctx->buf = buf;
    ctx->len = buf_len;
    ctx->offset = global_log_offset;
    global_log_offset += ctx->len;
    
    io_uring_prep_write(sqe, fd_log, ctx->buf, ctx->len, ctx->offset);
    io_uring_sqe_set_data(sqe, ctx);
    io_uring_submit(&ring_log);

    return 0;
}

int kvs_log_close(){
    if(global_config.enable_log == 0) return 0;
    if(ring_log_inited == 1){
        io_uring_submit(&ring_log);
        struct io_uring_cqe * cqe = NULL;
        while(io_uring_peek_cqe(&ring_log, &cqe) == 0){
            io_write_ctx * ctx = io_uring_cqe_get_data(cqe);
            kvs_free(ctx->buf);
            kvs_free(ctx);
            ctx = NULL;
            io_uring_cqe_seen(&ring_log, cqe);
        }        
        io_uring_queue_exit(&ring_log);
    } 
    if(fd_log >= 0) close(fd_log);
    return 0;
}