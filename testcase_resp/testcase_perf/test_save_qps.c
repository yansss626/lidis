#include <hiredis/hiredis.h>
#include <string.h>
#include <sys/time.h>
#define BUFFER_SIZE 1024

#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

void testcase(redisContext *c, const char *casename, const char *expect_reply, const char *format) {

    redisReply * reply = redisCommand(c, format);
    if (reply == NULL) {
        fprintf(stderr, "reply == NULL\n");
        exit(1);
    }
    if (reply->type == REDIS_REPLY_STATUS) {
        if (strcmp(reply->str, expect_reply) == 0) {
            //printf("==> PASS ->  %s\n", casename);
        }
        else {
            printf("==> FAILED -> %s, '%s' != '%s' \n", casename, reply->str, expect_reply);
            exit(1);
        }
    } else {
        fprintf(stderr, "reply->type mismatch\n");
        exit(1);
    }

    
    freeReplyObject(reply);
}



void rbtree_testcase(redisContext *c, int count, int interval) {

	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);
    for (int i = 1; i <= count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "RSET Teacher%d King%d", i, i);
        testcase(c, casename, "OK", casename);
        
        int flag = i % interval;
        if (flag == 0) testcase(c, "SAVE", "OK", "SAVE");
        
    }
	struct timeval tv_end;
	gettimeofday(&tv_end, NULL);

	int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms

	printf("rbtree testcase --> time_used: %d, qps: %d\n", time_used, (count * 1000 / time_used));


}
void array_testcase(redisContext *c, int count, int interval) {

	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);    
    for (int i = 1; i <= count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "SET Teacher%d King%d", i, i);
        testcase(c, casename, "OK", casename);

        int flag = i % interval;
        if (flag == 0) testcase(c, "SAVE", "OK", "SAVE");
    }

	struct timeval tv_end;
	gettimeofday(&tv_end, NULL);

	int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms

	printf("array testcase --> time_used: %d, qps: %d\n", time_used, (count * 1000 / time_used));

}
void hash_testcase(redisContext *c, int count, int interval) {

	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);
    for (int i = 1; i <= count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "HSET Teacher%d King%d", i, i);
        testcase(c, casename, "OK", casename);

        int flag = i % interval;
        if (flag == 0) testcase(c, "SAVE", "OK", "SAVE");
    }
	struct timeval tv_end;
	gettimeofday(&tv_end, NULL);

	int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms

	printf("hash testcase --> time_used: %d, qps: %d\n", time_used, (count * 1000 / time_used));


}
void skiplist_testcase(redisContext *c, int count, int interval) {
	struct timeval tv_begin;


	gettimeofday(&tv_begin, NULL);
    for (int i = 1; i <= count; i++) {
        char casename[BUFFER_SIZE];
        snprintf(casename, BUFFER_SIZE, "LSET Teacher%d King%d", i, i);
        testcase(c, casename, "OK", casename);

        int flag = i % interval;
        if (flag == 0) {
            testcase(c, "SAVE", "OK", "SAVE");

            //printf("count: %d\n", i);
        }
    }
	struct timeval tv_end;
	gettimeofday(&tv_end, NULL);

	int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms

	printf("skiplist testcase --> time_used: %d, qps: %d\n", time_used, (count * 1000 / time_used));

}

void testcase_all(redisContext *c, int count, int interval){
    rbtree_testcase(c, count, interval);
    skiplist_testcase(c, count, interval);
    //array_testcase(c, count, interval);
    hash_testcase(c, count, interval);
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
    

    int count = 1000000;
    int interval = 1000;

    if (argc == 4) {
        int mode = atoi(argv[3]);
        switch (mode) {
            case 0: 
                rbtree_testcase(c, count, interval); 
                break;
            case 1: 
                array_testcase(c, count, interval); 
                break;
            case 2: 
                hash_testcase(c, count, interval); 
                break;
            case 3: 
                skiplist_testcase(c, count, interval); 
                break;
            default: 
                fprintf(stderr, "invalid mode\n"); 
                break;
        }
    } else {
        testcase_all(c, count, interval);
    }

    redisFree(c);
    return 0;
	
}