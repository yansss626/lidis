#include <hiredis/hiredis.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#define BUFFER_SIZE 1024


void testcase(redisContext * c, const char * cmd, const char * key, const char * value) {

    redisReply * reply = redisCommand (c, "%s \"%s\" %s" ,cmd ,key ,value);
    if (reply == NULL) {
        fprintf(stderr, "reply == NULL\n");
        exit(1);        
    }

    if (reply->type == REDIS_REPLY_STATUS) {    
        if(strcmp(reply->str, "OK") == 0){
            printf("%s\n", reply->str);
        }
        else{
            printf("==> FAILED -> '%s' != '%s' \n", reply->str, "OK");
            exit(1);
        }
    } else {
        fprintf(stderr, "reply->type mismatch\n");
        exit(1);
    }

    freeReplyObject(reply);
}

void rbtree_testcase(redisContext * c, const char * value){
    testcase(c, "RSET", "This is a blog", value);
}
void hash_testcase(redisContext * c, const char * value){
    testcase(c, "HSET", "This is a blog", value);
}
void array_testcase(redisContext * c, const char * value){
    testcase(c, "SET", "This is a blog", value);
}
void skiplist_testcase(redisContext * c, const char * value){
    testcase(c, "LSET", "This is a blog", value);
}


// testcase 192.168.184.138  2000 mode: 0 for rbtree, 1 for array, 2 for hash 3 for skiplist
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Lack of arg\n");
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int fd = 0;
    char * ptr = NULL;
    struct stat statbuf = {0};

    fd = open("./testcase_sample/blog.txt", O_RDONLY);
    if (fd < 0){
        perror("open");
        return 1;
    }
    
    fstat(fd, &statbuf);
    if (statbuf.st_size == 0) {
        fprintf(stderr, "blog is empty\n");
        close(fd);
        return 1;
    }


    ptr = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("open");
        close(fd);
        return 1;
    }
    
    char * value = (char *)calloc(1, statbuf.st_size + 1);
    if (value == NULL) {
        fprintf(stderr, "calloc");
        exit(1);
    }
    memcpy(value, ptr, statbuf.st_size);

    
    redisContext *c = redisConnect(ip, port);
    if (c == NULL || c->err) {
        if (c != NULL) {
            fprintf(stderr, "Error: %s\n", c->errstr);
            // handle error
        } else {
            fprintf(stderr, "Can't allocate redis context\n");
        }

        exit(1);
    }
    



    if (argc == 4) {
        int mode = atoi(argv[3]);
        switch (mode) {
            case 0: 
                rbtree_testcase(c, value); 
                break;
            case 1: 
                array_testcase(c, value); 
                break;
            case 2: 
                hash_testcase(c, value); 
                break;
            case 3: 
                skiplist_testcase(c, value); 
                break;
            default: 
                fprintf(stderr, "invalid mode\n"); 
                break;
        }
    } 

    free(value);
    redisFree(c);
    munmap(ptr, statbuf.st_size);
    return 0;
	
}