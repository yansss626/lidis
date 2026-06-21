#include <stdio.h>
#include "kvstore.h"
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <liburing.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/syscall.h> 
#include "nty_coroutine.h"


extern kvs_conf_t global_config;

#define BUFFER_SIZE 1024
#define ENTRY_LENGTH 1024
#define TARGET_LENGTH 512
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

typedef enum kvs_engine_type_t{
    KVS_ARRAY,
    KVS_HASH,
    KVS_RBTREE,
    KVS_SKIPLIST,
}kvs_engine_type;


typedef enum kvs_save_status_t {
    SAVE_STATUS_IDLE,
    SAVE_STATUS_RUNNING,
    SAVE_STATUS_ERROR,
}kvs_save_status;

static kvs_save_status save_status = SAVE_STATUS_IDLE;
static pid_t kvs_save_child_pid = 0;
static int is_save_pending = 0; // 标志位：用于判断SAVE指令是否被忽略

static int kvs_fork_save_child();

int kvs_save_handler(client_info * cli){
    if(cli == NULL) return -1;

    char *tokens[KVS_MAX_TOKENS] = {0};

	int count = kvs_split_token(cli, tokens);
	if (count != 3 || tokens[0] == NULL || tokens[1] == NULL || tokens[2] == NULL) return -2;

    char type = tokens[0][0];
    char * key = tokens[1];
    char * value = tokens[2];
    
    int length = 0;

    switch (type)
    {
    case 'A': //array
        length = kvs_array_set(&global_array, key, value);
        break;
    case 'H': // hash
        length = kvs_hash_set(&global_hash, key, value);
        break;
    case 'R': // rbtree
        length = kvs_rbtree_set(&global_rbtree, key, value);
        break;
    case 'S': // skiplist
        length = kvs_skiplist_set(&global_skiplist, key, value);
        break;
    
    default:
        break;
    }

    return length;
}

//在初始化阶段，加载全量持久化文件的数据。
int kvs_save_init(msg_handler handler){
    if(global_config.enable_save == 0) return 0; 

    int fd = open("./kvs-data/kvs_dump.kvsdb", O_RDONLY);
    if (fd < 0) {
        return 0;  
    }

    struct stat statbuf = {0};
    if (fstat(fd, &statbuf) < 0) {
        close(fd);
        return -1;
    }

    if(statbuf.st_size <= 0) {
        close(fd);
        return 0;
    }

    char * ptr = (char *)mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if(ptr == MAP_FAILED){
        perror("mmap");
        close(fd);
        return -2;
    }

    kvs_file_read(ptr, statbuf.st_size, kvs_save_handler);
    munmap(ptr, statbuf.st_size);
    close(fd);
    return 0;
}

static char * build_save_protocol(char * key, char * value, kvs_engine_type type, int * ret_len) { // ret_len: return string length
    if (key == NULL || value == NULL || ret_len == NULL) return NULL;

    char * engine = NULL;
    switch (type)
    {
    case KVS_ARRAY:{
        engine = "ARRAY";
        break;
    }
    case KVS_HASH:
        engine = "HASH";
        break;
    case KVS_RBTREE:
        engine = "RBTREE";
        break;
    case KVS_SKIPLIST:
        engine = "SKIPLIST";
        break;    
    default:
        break;
    }
    if(engine == NULL) {
        printf("kvs_engine_type do not exist\n");
        return NULL;
    }

    // #<body_length>\r\n^<tok_len>&<tok>^<tok_len>&<tok>...\r\n

    int kstr_len = strlen(key); // key string len
    int vstr_len = strlen(value); // value string len 
    int engine_len = strlen(engine); // engine string len   


    int body_len = snprintf(NULL, 0, "^%d&%s^%d&%s^%d&%s\r\n", engine_len, engine, 
            kstr_len, key, vstr_len, value);

    int head_len = snprintf(NULL, 0, "#%d\r\n", body_len);

    int total_len = body_len + head_len;

    char * buf = (char *)kvs_malloc(total_len + 1);
    if (buf == NULL) {
        perror("kvs_malloc");
        return NULL;
    }
    
    *ret_len = snprintf(buf, total_len + 1, "#%d\r\n^%d&%s^%d&%s^%d&%s\r\n", body_len,engine_len, engine, 
            kstr_len, key, vstr_len, value);
    
    if (*ret_len != total_len) {
        kvs_free(buf);
        return NULL;
    }
        
    return buf;

}

// write .kvsdb file (for SAVE command)
int kvs_io_uring_write(int fd, char * buf, int buf_len, io_write_ctx * main_ctx){
    if(fd < 0 || buf == NULL || buf_len <= 0 || main_ctx == NULL) return -1;

    int ret = 0;

    io_write_ctx * ctx = (io_write_ctx *)kvs_malloc(sizeof(io_write_ctx));
    if(ctx == NULL) {
        perror("kvs_malloc error");
        ret = -2;
        goto cleanup;
    }
    memset(ctx, 0, sizeof(io_write_ctx));

    ctx->buf = buf;
    ctx->len = buf_len;

    if(main_ctx->tasks_count >= TARGET_LENGTH) io_uring_submit(main_ctx->ring);

    struct io_uring_cqe * cqes[TARGET_LENGTH] = {0};
    int nready = io_uring_peek_batch_cqe(main_ctx->ring, cqes, TARGET_LENGTH);
    for(int i = 0; i < nready; i++){
        struct io_uring_cqe * cqe = cqes[i];
        io_write_ctx * ctx = (io_write_ctx *)io_uring_cqe_get_data(cqe);
        kvs_free(ctx->buf);
        kvs_free(ctx);
        --main_ctx->tasks_count;
    }
    io_uring_cq_advance(main_ctx->ring, nready);

      
    struct io_uring_sqe * sqe = io_uring_get_sqe(main_ctx->ring);
    if(sqe == NULL){     
        io_uring_submit(main_ctx->ring);
        ret = -4;
        goto cleanup; 
    }
    ++main_ctx->tasks_count;
    io_uring_prep_write(sqe, fd, ctx->buf, buf_len, main_ctx->offset);
    main_ctx->offset += buf_len;
    io_uring_sqe_set_data(sqe, ctx);
    
    return 0;
    cleanup:
        kvs_free(buf);
        if(ctx != NULL){
            kvs_free(ctx);
        }

        return ret;
}


#if ENABLE_RBTREE
int kvs_save_write_rbtree(rbtree *T, rbtree_node *node, int fd, io_write_ctx *main_ctx) {
    if(T == NULL || fd < 0) return fd;
    int ret = 0;
	if (node != T->nil) {
        int buf_len = 0;
        char * buf = build_save_protocol(node->key, node->value, KVS_RBTREE, &buf_len);
        ret = kvs_io_uring_write(fd, buf, buf_len, main_ctx);
        if(ret < 0) fd = ret;

		kvs_save_write_rbtree(T, node->left, fd, main_ctx);

		kvs_save_write_rbtree(T, node->right, fd, main_ctx);
	}
    return ret;
}
#endif


int kvs_traversal_write(int fd, io_write_ctx * main_ctx){

    int ret = 0;
#if ENABLE_RBTREE    
    kvs_rbtree_t * R_inst = &global_rbtree;
    ret = kvs_save_write_rbtree(R_inst, R_inst->root, fd, main_ctx);
    if(ret != 0) return ret;
    
#endif

#if ENABLE_HASH
    kvs_hash_t * H_inst = &global_hash;
    if(H_inst->count > 0){
        int count = H_inst->count;
        for (int i = 0;i < H_inst->max_slots;i ++) {
            hashnode_t *node = H_inst->nodes[i];
            while (node != NULL) { 
                int buf_len = 0;
                char * buf = build_save_protocol(node->key, node->value, KVS_HASH, &buf_len);
                ret = kvs_io_uring_write(fd, buf, buf_len, main_ctx);

                if(ret != 0) return ret;

                node = node->next;
                --count;
            }
            if(count <= 0) break;
        }   
    }
    
#endif

#if ENABLE_ARRAY

    kvs_array_t * inst = &global_array;
    if(inst->total > 0){
        int count = inst->total;
        for (int i = 0;i < KVS_ARRAY_SIZE;i ++) {
            if (inst->table[i].key != NULL) {
                int buf_len = 0;
                char * buf = build_save_protocol(inst->table[i].key, inst->table[i].value, KVS_ARRAY, &buf_len);
                ret = kvs_io_uring_write(fd, buf, buf_len, main_ctx);

                if(ret != 0) return ret;
                    
                --count;
            }
            if(count <= 0) break;
        }
    }

       
#endif

#if ENABLE_SKIPLIST

    kvs_skiplist_t * L_inst = &global_skiplist;
    Node * current = L_inst->header->forward[0];
    while(current != NULL){
        int buf_len = 0;
        char * buf = build_save_protocol(current->key, current->value, KVS_SKIPLIST, &buf_len);
        ret = kvs_io_uring_write(fd, buf, buf_len, main_ctx);

        if(ret != 0) return ret;
        
        current = current->forward[0];
    }


#endif
    io_uring_submit(main_ctx->ring);
    //printf("main_ctx->tasks_count: %d\n", main_ctx->tasks_count);
    struct io_uring_cqe * cqe = NULL;

    if(main_ctx->tasks_count != 0){
        while(io_uring_wait_cqe(main_ctx->ring, &cqe) == 0){
            io_write_ctx * ctx = (io_write_ctx *)io_uring_cqe_get_data(cqe);
            kvs_free(ctx->buf);
            kvs_free(ctx);
            io_uring_cqe_seen(main_ctx->ring, cqe);
            --main_ctx->tasks_count;
            if(main_ctx->tasks_count == 0) break;
        }
    }
    return ret;
}

int kvs_save_start(){

    struct io_uring ring_save = {0};

    if(io_uring_queue_init(ENTRY_LENGTH, &ring_save, 0) != 0) {
        return -1;
    }
    

    int fd = open("./kvs-data/kvs_dump.kvsdb.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644); 
    if(fd < 0) {
        io_uring_queue_exit(&ring_save);
        return -2;
    }

    io_write_ctx main_ctx = {0};
    main_ctx.ring = &ring_save;
    main_ctx.tasks_count = 0;

    if (kvs_traversal_write(fd, &main_ctx) != 0) {
        close(fd);
        io_uring_queue_exit(&ring_save);
        return -3;
    }

    fsync(fd);
    close(fd);
    io_uring_queue_exit(&ring_save);
    rename("./kvs-data/kvs_dump.kvsdb.tmp", "./kvs-data/kvs_dump.kvsdb");

    return 0;

}

static void  kvs_save_child_done(void * arg) {
    int child_fd = *(int *)arg;

    char result = '1';
    ssize_t n = recv(child_fd, &result, 1, 0);
    
    int status = 0;
    pid_t pid = kvs_save_child_pid;
    int ret = waitpid(kvs_save_child_pid, &status, 0);

    if (n == 1 && result == '0' && ret == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        save_status = SAVE_STATUS_IDLE;
    }
    else {
        save_status = SAVE_STATUS_ERROR;
    }

    if (ret == pid) kvs_save_child_pid = 0;

    if (is_save_pending == 1 && save_status == SAVE_STATUS_IDLE) {
        is_save_pending = 0;
        kvs_fork_save_child();
    }

    kvs_free(arg);
    close(child_fd);
}


static int kvs_fork_save_child() {

    int notify_pair[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, notify_pair) != 0) {
        perror("socketpair");
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        save_status = SAVE_STATUS_ERROR;
        close(notify_pair[0]);
        close(notify_pair[1]);
        return -1;
    }

    if (pid == 0){
        close(notify_pair[0]);
        int ret = kvs_save_start();

        char result = (ret == 0) ? '0':'1';
        syscall(SYS_write, notify_pair[1], &result, 1);
        close(notify_pair[1]);

        _exit(ret == 0 ? 0 : 1);
    }

    save_status = SAVE_STATUS_RUNNING;
    kvs_save_child_pid = pid;

    close(notify_pair[1]);

#if (NETWORK_SELECT == NETWORK_NTYCO)
    int * notify_fd = kvs_malloc(sizeof(int));
    if (notify_fd == NULL) {
        close(notify_pair[0]);
        save_status = SAVE_STATUS_ERROR;
        return -1;
    }
    *notify_fd = notify_pair[0];

    nty_coroutine * co = NULL;
    nty_coroutine_create(&co, kvs_save_child_done, notify_fd);

#else
    close(notify_pair[0]);
#endif
    
    return 0;
}

void kvs_check_save_status () {
    if(global_config.enable_save == 0) return ;

    if (save_status != SAVE_STATUS_RUNNING || kvs_save_child_pid <= 0) return;

    int status;
    pid_t ret = waitpid(kvs_save_child_pid, &status, WNOHANG);

    if (ret < 0) {
        perror("waitpid");
        kvs_save_child_pid = 0;
        save_status = SAVE_STATUS_ERROR;
        return;
    }

    if (ret == kvs_save_child_pid) {
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            save_status = SAVE_STATUS_IDLE;
        }
        else {
            save_status = SAVE_STATUS_ERROR;
        }
        kvs_save_child_pid = 0;

        if (is_save_pending == 1) {
            is_save_pending = 0;
            kvs_fork_save_child();
        }
    }
    

}



int kvs_save_write() {
    if(global_config.enable_save == 0) return 0;


#if (NETWORK_SELECT != NETWORK_NTYCO)
    kvs_check_save_status();
#endif

    if (save_status == SAVE_STATUS_RUNNING) {
        is_save_pending = 1;
        return 1;
    }

    if (save_status == SAVE_STATUS_ERROR) {
        return -1;
    }

    return kvs_fork_save_child();
}