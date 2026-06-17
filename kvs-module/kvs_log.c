
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

        memcpy(cli.rbuf , ptr, total_len);
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
    fstat(fd_log, &statbuf);
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

char * add_kvsp_head(char * buf, int buf_len, int * ret_len) {
    if (buf == NULL || buf_len <= 0 || ret_len == NULL) return NULL;

    int head_len = snprintf(NULL, 0, "kvsp/1\r\n#%d\r\n", buf_len);
    if (head_len <= 0) {
        return NULL;
    }

    char * ret_buf = (char *)kvs_malloc(head_len + buf_len);
    if(ret_buf == NULL){
        perror("kvs_malloc");
        return NULL;
    }

    *ret_len = head_len + buf_len;

    if (snprintf(ret_buf, *ret_len, "kvsp/1\r\n#%d\r\n", buf_len) != head_len) {
        kvs_free(ret_buf);
        return NULL;
    }

    memcpy(ret_buf + head_len, buf, buf_len);

    return ret_buf;
}


io_write_ctx * create_log_ctx(client_info * cli) {
    if (cli == NULL) return NULL;
    
    char * log_data = NULL;
    int data_len = 0;

    io_write_ctx * ctx = (io_write_ctx *)kvs_malloc(sizeof(io_write_ctx));
    if(ctx == NULL){
        perror("kvs_malloc");
        return NULL;
    }

    if (cli->protocol == PROTO_RESP) {
        log_data = add_kvsp_head(cli->rbuf + cli->cmd_hl, cli->cmd_tl - cli->cmd_hl, &data_len);
    }
    else if (cli->protocol == PROTO_KVSP) {
        log_data = (char *)kvs_malloc(cli->cmd_tl);
        if(log_data == NULL){
            perror("kvs_malloc");
            kvs_free(ctx);
            return NULL;
        }
        memcpy(log_data, cli->rbuf, cli->cmd_tl);
        data_len = cli->cmd_tl;
    }

    if (log_data == NULL || data_len <= 0) {
        kvs_free(ctx);
        return NULL;
    }

    ctx->buf = log_data;
    ctx->len = data_len;
    
    //printf("ctx->buf: %s\n", ctx->buf);    

    return ctx;
}

int kvs_log_write(client_info * cli){
    if(global_config.enable_log == 0) return 0;
    if(cli == NULL) return -1;
    if(fd_log < 0 || ring_log_inited != 1) return -2;

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
        return -3;
    }

    io_write_ctx * ctx = create_log_ctx(cli);
    if (ctx == NULL) {
        return -4;
    }

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