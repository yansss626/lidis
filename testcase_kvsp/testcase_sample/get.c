#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "kvstore.h"

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
        printf("==> PASS -> %s\n", case_name);
    } else {
        printf("==> FAILED -> %s, reply='%s', expected='%s'\n",
               case_name, reply, expected_reply);
        exit(1);
    }

    kvs_free(msg);
}

static int build_bulk_reply(char *buf, size_t cap, const char *value) {
    if (buf == NULL || value == NULL || cap == 0) return -1;
    size_t v_len = strlen(value);
    if (v_len > cap) return -2;
    return snprintf(buf, cap, "$%zu\r\n%s\r\n", v_len, value);
}

void create_testcase(int fd, int count, char * cmd) {
    for (int i = 0; i < count; i++) {
        char key[BUFFER_SIZE];
        char value[BUFFER_SIZE];
        char casename[BUFFER_SIZE * 2];
        char expected_reply[BUFFER_SIZE] = {0};

        snprintf(key, BUFFER_SIZE, "Teacher%d", i);
        snprintf(value, BUFFER_SIZE, "King%d", i);
        snprintf(casename, BUFFER_SIZE * 2, "%s Teacher%d", cmd, i);

        const char *argv[] = {cmd, key};

        build_bulk_reply(expected_reply, BUFFER_SIZE, value);

        testcase(fd, 2, argv, expected_reply, casename);
    }    
}

void rbtree_testcase(int fd, int count) {
    create_testcase(fd, count, "RGET");
}

void array_testcase(int fd, int count) {
    create_testcase(fd, count, "GET");
}


void hash_testcase(int fd, int count) {
    create_testcase(fd, count, "HGET");
}


void skiplist_testcase(int fd, int count) {
    create_testcase(fd, count, "LGET");
}


void testcase_all(int fd, int count) {
    rbtree_testcase(fd, count);
    skiplist_testcase(fd, count);
    // array_testcase(fd, count);
    hash_testcase(fd, count);
}

// usage: ./get 127.0.0.1 2000 [mode]
// mode: 0 rbtree, 1 array, 2 hash, 3 skiplist
int main(int argc, char *argv[]) {
    if (argc < 3) {
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int fd = connect_server(ip, port);
    if (fd < 0) {
        return 1;
    }

    int count = 10000;

    if (argc == 4) {
        int mode = atoi(argv[3]);
        switch (mode) {
            case 0:
                rbtree_testcase(fd, count);
                break;
            case 1:
                array_testcase(fd, count);
                break;
            case 2:
                hash_testcase(fd, count);
                break;
            case 3:
                skiplist_testcase(fd, count);
                break;
            default:
                fprintf(stderr, "invalid mode\n");
                close(fd);
                return 1;
        }
    } else {
        testcase_all(fd, count);
    }

    close(fd);
    return 0;
}
