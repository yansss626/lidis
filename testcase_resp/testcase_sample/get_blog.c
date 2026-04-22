#include <hiredis/hiredis.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#define BUFFER_SIZE 1024


void testcase(redisContext * c, const char * cmd, const char * key, const char * value) {

    redisReply * reply = NULL;
    if (value != NULL) {
        reply = redisCommand (c, "%s \"%s\" %s", cmd, key, value);
    } else {
        reply = redisCommand (c, "%s \"%s\"", cmd, key);
    }
    
    if (reply == NULL) {
        fprintf(stderr, "reply == NULL\n");
        exit(1);        
    } 

    if (reply->type == REDIS_REPLY_STRING) {
        printf("key: %s\n\nvalue: %s\n", key, reply->str);
    } else {
        fprintf(stderr, "reply->type mismatch\n");
        exit(1);
    }
    
    freeReplyObject(reply);
}

void rbtree_testcase(redisContext * c){
    testcase(c, "RGET", "This is a blog", NULL);
}
void hash_testcase(redisContext * c){
    testcase(c, "HGET", "This is a blog", NULL);
}
void array_testcase(redisContext * c){
    testcase(c, "GET", "This is a blog", NULL);
}
void skiplist_testcase(redisContext * c){
    testcase(c, "LGET", "This is a blog", NULL);
}


// testcase 192.168.184.138  2000 mode: 0 for rbtree, 1 for array, 2 for hash 3 for skiplist
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Lack of arg\n");
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    
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
    

    int count = 10000;

    if (argc == 4) {
        int mode = atoi(argv[3]);
        switch (mode) {
            case 0: 
                rbtree_testcase(c); 
                break;
            case 1: 
                array_testcase(c); 
                break;
            case 2: 
                hash_testcase(c); 
                break;
            case 3: 
                skiplist_testcase(c); 
                break;
            default: 
                fprintf(stderr, "invalid mode\n"); 
                break;
        }
    }

    redisFree(c);
    return 0;
	
}