#include <hiredis/hiredis.h>
#include <string.h>
#define BUFFER_SIZE 1024



void testcase(redisContext *c, int batch) {

    redisReply * reply = NULL;
    for (int i = 0; i < batch; i ++) {
        int ret = redisGetReply(c, (void **)&reply);
        if(reply == NULL || ret != REDIS_OK) {
            fprintf(stderr, "redisGetReply error\n");
            exit(1);
        }

        switch (reply->type)
        {
        case REDIS_REPLY_STATUS:
            printf("%s\n", reply->str);
            break;
        case REDIS_REPLY_STRING:
            printf("%s\n", reply->str);
            break;
        case REDIS_REPLY_INTEGER:
            printf("%lld(EXIST)\n", reply->integer);
            break;
        case REDIS_REPLY_NIL:
            printf("nil(NO EXIST)\n");
            break;
        case REDIS_REPLY_ERROR:
            printf("ERROR\n");
            break;
        default:
            break;
        }

         
    }
    
    freeReplyObject(reply);
}



void rbtree_testcase(redisContext *c, int count) {
    int batch = 0;
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "RSET Teacher%d King%d", i, i);
        redisAppendCommand(c, casename);
        ++batch;
    }
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "RGET Teacher%d", i);
        redisAppendCommand(c, casename);
        ++batch;
    }
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "RMOD Teacher%d King-%d", i, i);
        redisAppendCommand(c, casename);
        ++batch;
    }
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "RDEL Teacher%d", i);
        redisAppendCommand(c, casename);
        ++batch;
    }
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "REXIST Teacher%d", i);
        redisAppendCommand(c, casename);
        ++batch;
    }

    testcase(c, batch);
}
void array_testcase(redisContext *c, int count) {
    int batch = 0;
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "SET Teacher%d King%d", i, i);
        redisAppendCommand(c, casename);
        ++batch;
    }
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "GET Teacher%d", i);
        redisAppendCommand(c, casename);
        ++batch;
    }
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "MOD Teacher%d King-%d", i, i);
        redisAppendCommand(c, casename);
        ++batch;
    }
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "DEL Teacher%d", i);
        redisAppendCommand(c, casename);
        ++batch;
    }
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "EXIST Teacher%d", i);
        redisAppendCommand(c, casename);
        ++batch;
    }

    testcase(c, batch);
}
void hash_testcase(redisContext *c, int count) {
    int batch = 0;
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "HSET Teacher%d King%d", i, i);
        redisAppendCommand(c, casename);
        ++batch;
    }
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "HGET Teacher%d", i);
        redisAppendCommand(c, casename);
        ++batch;
    }
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "HMOD Teacher%d King-%d", i, i);
        redisAppendCommand(c, casename);
        ++batch;
    }
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "HDEL Teacher%d", i);
        redisAppendCommand(c, casename);
        ++batch;
    }
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "HEXIST Teacher%d", i);
        redisAppendCommand(c, casename);
        ++batch;
    }

    testcase(c, batch);
}
void skiplist_testcase(redisContext *c, int count) {
    int batch = 0;
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "LSET Teacher%d King%d", i, i);
        redisAppendCommand(c, casename);
        ++batch;
    }
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "LGET Teacher%d", i);
        redisAppendCommand(c, casename);
        ++batch;
    }
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "LMOD Teacher%d King-%d", i, i);
        redisAppendCommand(c, casename);
        ++batch;
    }
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "LDEL Teacher%d", i);
        redisAppendCommand(c, casename);
        ++batch;
    }
    for (int i = 0; i < count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "LEXIST Teacher%d", i);
        redisAppendCommand(c, casename);
        ++batch;
    }

    testcase(c, batch);
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
    

    int count = 10;

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
     }

    redisFree(c);
    return 0;
	
}