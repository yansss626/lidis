#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include "kvstore.h"

#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

#define BUFFER_SIZE 1024
#define REPLY_SIZE 1024


void *kvs_malloc(size_t size) {
    return malloc(size);
}

void kvs_free(void *ptr) {
	return free(ptr);
}


int connect_server(const char *ip, unsigned short port) {

	int connfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in server_addr = {0};

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);
	server_addr.sin_port = htons(port);

	if (0 !=  connect(connfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in))) {
		perror("connect");
		return -1;
	}
	
	return connfd;
	
}

int send_msg(int connfd, char *msg, int len) {
    if (msg == NULL || len <= 0) return -1;

    int sent = 0;
    while (sent < len) {
        ssize_t n = send(connfd, msg + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("send");
            return -2;
        }
        if (n == 0) {
            fprintf(stderr, "send returned 0\n");
            return -2;
        }
        sent += n;
    }

    return sent;
}

int recv_msg(int connfd, char *msg, int cap) {
    if (msg == NULL || cap <= 0) {
        return -1;
    }

    ssize_t n = recv(connfd, msg, cap, 0);
    if (n < 0) {
        perror("recv");
        return -2;
    }
    if (n == 0) {
        fprintf(stderr, "connection closed\n");
        return -2;
    }

    
    msg[n] = '\0';

    return n;

}


static void testcase(int connfd, int argc, const char * argv[], const char * expected_reply, const char *case_name) {

	if (!argv[0] || !expected_reply || !case_name) return ;

    int msg_len = 0;
    char * msg= kvs_build_kvsp_frame(argc, argv, &msg_len);
    if (msg == NULL || msg_len <= 0) {
        printf("FAILED to build request: %s\n", case_name);
        exit(1);
    }

	if (send_msg(connfd, msg, msg_len) < 0) {
        exit(1);
    }

    char reply[REPLY_SIZE] = {0};

    int recevied = recv_msg(connfd, reply, REPLY_SIZE);
    if (recevied <= 0) exit(1);

    if (memcmp(reply, expected_reply, recevied) == 0) {
        //printf("==> PASS -> %s\n", case_name);
    } else {
        printf("==> FAILED -> %s, reply='%s', expected='%s'\n",
               case_name, reply, expected_reply);
        exit(1);
    }

    kvs_free(msg);
}

void create_single_testcase(int fd, char * cmd) {

    const char *argv[] = {cmd};
    testcase(fd, 1, argv, "+OK\r\n", cmd); 
    
}

void create_testcase(int fd, int count, char * cmd, int interval) {

    for (int i = 1; i <= count; i++) {

        char key[BUFFER_SIZE];
        char value[BUFFER_SIZE];
        char casename[BUFFER_SIZE * 3];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);
        snprintf(value, BUFFER_SIZE, "King%d", i);
        snprintf(casename, BUFFER_SIZE * 3, "%s Teacher%d King%d", cmd, i, i);

        const char *argv[] = {cmd, key, value};
        testcase(fd, 3, argv, "+OK\r\n", casename);

        int flag = i % interval;
        if (flag == 0) {
            create_single_testcase(fd, "SAVE");
        }

    }    

}

void rbtree_testcase(int fd, int count, int interval) {
    struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);   

    create_testcase(fd, count, "RSET", interval);

    struct timeval tv_end;
	gettimeofday(&tv_end, NULL);
	int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms
	printf("rbtree testcase --> time_used: %d, qps: %d\n", time_used, (count * 1000 / time_used));
}

void array_testcase(int fd, int count, int interval) {
    struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);   

    create_testcase(fd, count, "SET", interval);

    struct timeval tv_end;
	gettimeofday(&tv_end, NULL);
	int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms
	printf("array testcase --> time_used: %d, qps: %d\n", time_used, (count * 1000 / time_used));
}


void hash_testcase(int fd, int count, int interval) {
    struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL); 

    create_testcase(fd, count, "HSET", interval);

    struct timeval tv_end;
	gettimeofday(&tv_end, NULL);
	int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms
	printf("hash testcase --> time_used: %d, qps: %d\n", time_used, (count * 1000 / time_used));
}


void skiplist_testcase(int fd, int count, int interval) {
    struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL); 
        
    create_testcase(fd, count, "LSET", interval);

    struct timeval tv_end;
	gettimeofday(&tv_end, NULL);
	int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms
	printf("skiplist testcase --> time_used: %d, qps: %d\n", time_used, (count * 1000 / time_used));
}


void testcase_all(int fd, int count, int interval) {
    rbtree_testcase(fd, count, interval);
    skiplist_testcase(fd, count, interval);
    // array_testcase(fd, count, interval);
    hash_testcase(fd, count, interval);
}


// testcase 192.168.184.138  2000 mode: 0 for rbtree, 1 for array, 2 for hash 3 for skiplist
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Lack of arg\n");
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int fd = connect_server(ip, port);
    if (fd < 0) {
        return 1;
    }


    int count = 1000000;
    int interval = 1000;

    if (argc == 4) {
        int mode = atoi(argv[3]);
        switch (mode) {
            case 0: 
                rbtree_testcase(fd, count, interval);
                break;
            case 1: 
                array_testcase(fd, count, interval);
                break;
            case 2: 
                hash_testcase(fd, count, interval);
                break;
            case 3: 
                skiplist_testcase(fd, count, interval);
                break;
            default: 
                fprintf(stderr, "invalid mode\n"); 
                break;
        }
    } else {
        testcase_all(fd, count, interval);
    }

    close(fd);
    return 0;
	
}