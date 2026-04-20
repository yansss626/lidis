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
static struct io_uring ring_save = {0};
static int ring_save_inited = 0;

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





int kvs_save_handler(client_info * cli){
    if(cli == NULL) return -1;
    char *tokens[KVS_MAX_TOKENS] = {0};
	int count = kvs_split_token(cli->rbuf + cli->cmd_hl, tokens);
	if (count < 0) return -2;

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


int kvs_save_init(msg_handler handler){
    if(global_config.enable_save == 0) return 0; 
    int fd = open("./kvs-module/kvs_dump.rdb", O_RDONLY);
    struct stat statbuf = {0};
    fstat(fd, &statbuf);
    if(statbuf.st_size <= 0) return 0;
    char * ptr = (char *)mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if(ptr == MAP_FAILED){
        perror("mmap");
        close(fd);
        return -2;
    }

    kvs_file_read(ptr, statbuf.st_size, kvs_save_handler);
    munmap(ptr, statbuf.st_size);

    return 0;
}

int kvs_io_uring_write(int fd, char * key, char * value, kvs_engine_type type, io_write_ctx * main_ctx){
    if(fd < 0 || key == NULL || value == NULL || main_ctx == NULL) return -1;
    int ret = 0;


    io_write_ctx * ctx = (io_write_ctx *)malloc(sizeof(io_write_ctx));
    if(ctx == NULL) {
        perror("malloc error");
        ret = -2;
        goto cleanup;
    }
    memset(ctx, 0, sizeof(io_write_ctx));

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
        ret = -3;
        printf("kvs_engine_type do not exist\n");
        goto cleanup;
    }

    // *3\r\n$<length>\r\n$<length>\r\n<key>\r\n$<length>\r\n<value>\r\n

    int kstr_len = strlen(key); // key string len
    int vstr_len = strlen(value); // value string len 
    int engine_len = strlen(engine); // engine string len   

    // 固定开销：*3\r\n + 三个$的bulk头 + 三个\r\n末尾 + length所占字节长度， 保险给64
    int max_len = 64 + kstr_len + vstr_len + engine_len;
    
    ctx->buf = (char *)malloc(max_len);
    if(ctx->buf == NULL){
        ret = -2;
        perror("malloc error");
        goto cleanup;
    }
    
    int total_len = snprintf(ctx->buf, max_len, "*3\r\n$%d\r\n%s\r\n$%d\r\n%s\r\n$%d\r\n%s\r\n", engine_len, engine, 
            kstr_len, key, vstr_len, value);
    ctx->len = total_len;
    //printf("ctx->buf: %s\n", ctx->buf);

    if(main_ctx->tasks_count >= TARGET_LENGTH) io_uring_submit(main_ctx->ring);

    struct io_uring_cqe * cqes[TARGET_LENGTH] = {0};
    int nready = io_uring_peek_batch_cqe(main_ctx->ring, cqes, TARGET_LENGTH);
    for(int i = 0; i < nready; i++){
        struct io_uring_cqe * cqe = cqes[i];
        io_write_ctx * ctx = (io_write_ctx *)io_uring_cqe_get_data(cqe);
        free(ctx->buf);
        free(ctx);
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
    io_uring_prep_write(sqe, fd, ctx->buf, total_len, -1);
    io_uring_sqe_set_data(sqe, ctx);
    
    return 0;
    cleanup:
        if(ctx != NULL){
            free(ctx->buf);
            free(ctx);
        }
        return ret;
}


#if ENABLE_RBTREE
int kvs_save_write_rbtree(rbtree *T, rbtree_node *node, int fd, io_write_ctx *main_ctx) {
    if(T == NULL || fd < 0) return fd;
    int ret = 0;
	if (node != T->nil) {
        ret = kvs_io_uring_write(fd, node->key, node->value, KVS_RBTREE, main_ctx);
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
                ret = kvs_io_uring_write(fd, node->key, node->value, KVS_HASH, main_ctx);
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
                ret = kvs_io_uring_write(fd, inst->table[i].key, inst->table[i].value, KVS_ARRAY, main_ctx);
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
        ret = kvs_io_uring_write(fd, current->key, current->value, KVS_SKIPLIST, main_ctx);
        if(ret != 0) return ret;
        
        current = current->forward[0];
    }


#endif
    io_uring_submit(main_ctx->ring);
    printf("count: %d\n", main_ctx->tasks_count);
    struct io_uring_cqe * cqe = NULL;

    if(main_ctx->tasks_count != 0){
        while(io_uring_wait_cqe(main_ctx->ring, &cqe) == 0){
            io_write_ctx * ctx = (io_write_ctx *)io_uring_cqe_get_data(cqe);
            free(ctx->buf);
            free(ctx);
            io_uring_cqe_seen(main_ctx->ring, cqe);
            --main_ctx->tasks_count;
            if(main_ctx->tasks_count == 0) break;
        }
    }
    return ret;
    printf("count: %d\n", main_ctx->tasks_count);
}

int kvs_save_write(){
    if(global_config.enable_save == 0) return 0;

    if(ring_save_inited == 0){
    if(io_uring_queue_init(ENTRY_LENGTH, &ring_save, 0) != 0) {
        return -1;
    }
    ring_save_inited = 1;
}
    int fd = open("./kvs-module/kvs_dump.rdb", O_WRONLY | O_CREAT | O_TRUNC, 0644); 
    if(fd < 0) return -2;

    io_write_ctx main_ctx = {0};
    main_ctx.ring = &ring_save;
    main_ctx.tasks_count = 0;

    kvs_traversal_write(fd, &main_ctx);
      
    close(fd);

    return 0;

}




int kvs_save_close(){
    if(global_config.enable_save == 0) return 0;
    if(ring_save_inited == 1){      
        io_uring_queue_exit(&ring_save);
    } 

    return 0;
}

