#include <hiredis/hiredis.h>
#include <string.h>
#define BUFFER_SIZE 1024

void testcase(redisContext * c, const char * cmd, const char * key, const char * value) {
    char command[BUFFER_SIZE] = {0};
    redisReply * reply = NULL;
    if (value != NULL) {
        reply = redisCommand (c, "%s \"%s\" %s", cmd, key, value);
        snprintf(command, BUFFER_SIZE, "%s %s %s", cmd, key, value);
    } else {
        reply = redisCommand (c, "%s \"%s\"", cmd, key);
        snprintf(command, BUFFER_SIZE, "%s %s", cmd, key);
    }
    
    if (reply == NULL) {
        fprintf(stderr, "reply == NULL\n");
        exit(1);        
    } 
        switch (reply->type)
        {
        case REDIS_REPLY_STATUS:
            printf("%s: %s\n", command, reply->str);
            break;
        case REDIS_REPLY_STRING:
            printf("%s: %s\n", command, reply->str);
            break;
        case REDIS_REPLY_INTEGER:
            printf("%s: %lld(EXIST)\n", command, reply->integer);
            break;
        case REDIS_REPLY_NIL:
            printf("%s: nil(NO EXIST)\n", command);
            break;
        case REDIS_REPLY_ERROR:
            printf("ERROR\n");
            break;
        default:
            break;
        }
    
    freeReplyObject(reply);
}

void rbtree_testcase(redisContext *c) {
    printf("[RBTREE]:\n");
    testcase(c, "RSET", "Teacher", "King");
    testcase(c, "RGET", "Teacher", NULL);
    testcase(c, "RMOD", "Teacher", "Kvstore");
    testcase(c, "RDEL", "Teacher", NULL);
    testcase(c, "REXIST", "Teacher", NULL);
    printf("\n");
}

void array_testcase(redisContext *c) {
    printf("[ARRAY]:\n");
    testcase(c, "SET", "Teacher", "King");
    testcase(c, "GET", "Teacher", NULL);
    testcase(c, "MOD", "Teacher", "Kvstore");
    testcase(c, "DEL", "Teacher", NULL);
    testcase(c, "EXIST", "Teacher", NULL);
    printf("\n");
}

void hash_testcase(redisContext *c) {
    printf("[HASH]:\n");
    testcase(c, "HSET", "Teacher", "King");
    testcase(c, "HGET", "Teacher", NULL);
    testcase(c, "HMOD", "Teacher", "Kvstore");
    testcase(c, "HDEL", "Teacher", NULL);
    testcase(c, "HEXIST", "Teacher", NULL);
    printf("\n");
}

void skiplist_testcase(redisContext *c) {
    printf("[SKIPLIST]:\n");
    testcase(c, "LSET", "Teacher", "King");
    testcase(c, "LGET", "Teacher", NULL);
    testcase(c, "LMOD", "Teacher", "Kvstore");
    testcase(c, "LDEL", "Teacher", NULL);
    testcase(c, "LEXIST", "Teacher", NULL);
    printf("\n");
}

void testcase_all(redisContext * c){
    rbtree_testcase(c);
    skiplist_testcase(c);
    array_testcase(c);
    hash_testcase(c);
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
     } else {
        testcase_all(c);
    }

    redisFree(c);
    return 0;
	
}