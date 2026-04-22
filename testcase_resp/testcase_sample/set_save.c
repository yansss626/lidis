#include <hiredis/hiredis.h>
#include <string.h>
#define BUFFER_SIZE 1024




void testcase(redisContext *c, const char *casename, const char *expect_reply, const char *format) {

    redisReply * reply = redisCommand(c, format);
    if(reply == NULL){
        fprintf(stderr, "reply == NULL\n");
        exit(1);
    }
    if (reply->type == REDIS_REPLY_STATUS) {
        if(strcmp(reply->str, expect_reply) == 0){
            printf("==> PASS ->  %s\n", casename);
        }
        else{
            printf("==> FAILED -> %s, '%s' != '%s' \n", casename, reply->str, expect_reply);
            exit(1);
        }
    } else {
        fprintf(stderr, "reply->type mismatch\n");
        exit(1);
    }

    
    freeReplyObject(reply);
}




void rbtree_testcase(redisContext *c, int count) {
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "RSET Teacher%d King%d", i, i);
        testcase(c, casename, "OK", casename);
    }
}
void array_testcase(redisContext *c, int count) {
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "SET Teacher%d King%d", i, i);
        testcase(c, casename, "OK", casename);
    }
}
void hash_testcase(redisContext *c, int count) {
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "HSET Teacher%d King%d", i, i);
        testcase(c, casename, "OK", casename);
    }
}
void skiplist_testcase(redisContext *c, int count) {
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "LSET Teacher%d King%d", i, i);
        testcase(c, casename, "OK", casename);
    }
}

void testcase_all(redisContext *c, int count){
    rbtree_testcase(c, count);
    skiplist_testcase(c, count);
    //array_testcase(c, count);
    hash_testcase(c, count);
    testcase(c, "SAVE", "OK", "SAVE");    
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
                rbtree_testcase(c, count); 
                break;
            case 1: 
                array_testcase(c, count); 
                break;
            case 2: 
                hash_testcase(c, count); 
                break;
            case 3: 
                skiplist_testcase(c, count); 
                break;
            default: 
                fprintf(stderr, "invalid mode\n"); 
                break;
        }
    } else {
        testcase_all(c, count);
    }

    redisFree(c);
    return 0;
	
}