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
#define REPLY_SIZE 1024 * 1024


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

    int received = 0;


    ssize_t n = recv(connfd, msg, cap, 0);
    if (n < 0) {
        perror("recv");
        return -2;
    }
    if (n == 0) {
        fprintf(stderr, "connection closed\n");
        return -2;
    }
    received += n;
    

    msg[received] = '\0';
    return received;

}


static void testcase(int connfd, char * msg, int msg_len) {

	if (!msg || msg_len <= 0) return ;

	if (send_msg(connfd, msg, msg_len) < 0) {
        exit(1);
    }

    char reply[REPLY_SIZE] = {0};

    if (recv_msg(connfd, reply, REPLY_SIZE) < 0) {
        exit(1);
    }

    printf("reply: %s\n",reply);

    free(msg);
}



void rbtree_testcase(int fd, int count) {
    int buf_cap = 1024 * 1024;
    char * buf = (char *)malloc(buf_cap);
    if (buf == NULL) return;

    int offset = 0;

    for (int i = 0; i < count; i++) {
        char * cmd = "RSET";
        char key[BUFFER_SIZE];
        char value[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);
        snprintf(value, BUFFER_SIZE, "King%d", i);

        const char *argv[] = {cmd, key, value};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(3, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len >= buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }

    for (int i = 0; i < count; i++) {
        char * cmd = "RGET";
        char key[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);

        const char *argv[] = {cmd, key};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(2, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len >= buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }

    for (int i = 0; i < count; i++) {
        char * cmd = "RMOD";
        char key[BUFFER_SIZE];
        char value[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);
        snprintf(value, BUFFER_SIZE, "King-%d", i);

        const char *argv[] = {cmd, key, value};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(3, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len >= buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }

 
    for (int i = 0; i < count; i++) {
        char * cmd = "RDEL";
        char key[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);

        const char *argv[] = {cmd, key};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(2, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len >= buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }   

        for (int i = 0; i < count; i++) {
        char * cmd = "REXIST";
        char key[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);

        const char *argv[] = {cmd, key};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(2, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len > buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }

    testcase(fd, buf, offset);
}

void array_testcase(int fd, int count) {
    int buf_cap = 1024 * 1024;
    char * buf = (char *)malloc(buf_cap);
    if (buf == NULL) return;

    int offset = 0;

    for (int i = 0; i < count; i++) {
        char * cmd = "SET";
        char key[BUFFER_SIZE];
        char value[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);
        snprintf(value, BUFFER_SIZE, "King%d", i);

        const char *argv[] = {cmd, key, value};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(3, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len >= buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }

    for (int i = 0; i < count; i++) {
        char * cmd = "GET";
        char key[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);

        const char *argv[] = {cmd, key};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(2, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len >= buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }

    for (int i = 0; i < count; i++) {
        char * cmd = "MOD";
        char key[BUFFER_SIZE];
        char value[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);
        snprintf(value, BUFFER_SIZE, "King-%d", i);

        const char *argv[] = {cmd, key, value};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(3, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len >= buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }

 
    for (int i = 0; i < count; i++) {
        char * cmd = "DEL";
        char key[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);

        const char *argv[] = {cmd, key};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(2, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len >= buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }   

        for (int i = 0; i < count; i++) {
        char * cmd = "EXIST";
        char key[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);

        const char *argv[] = {cmd, key};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(2, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len > buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }

    testcase(fd, buf, offset);
}


void hash_testcase(int fd, int count) {
    int buf_cap = 1024 * 1024;
    char * buf = (char *)malloc(buf_cap);
    if (buf == NULL) return;

    int offset = 0;

    for (int i = 0; i < count; i++) {
        char * cmd = "HSET";
        char key[BUFFER_SIZE];
        char value[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);
        snprintf(value, BUFFER_SIZE, "King%d", i);

        const char *argv[] = {cmd, key, value};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(3, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len >= buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }

    for (int i = 0; i < count; i++) {
        char * cmd = "HGET";
        char key[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);

        const char *argv[] = {cmd, key};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(2, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len >= buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }

    for (int i = 0; i < count; i++) {
        char * cmd = "HMOD";
        char key[BUFFER_SIZE];
        char value[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);
        snprintf(value, BUFFER_SIZE, "King-%d", i);

        const char *argv[] = {cmd, key, value};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(3, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len >= buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }

 
    for (int i = 0; i < count; i++) {
        char * cmd = "HDEL";
        char key[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);

        const char *argv[] = {cmd, key};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(2, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len >= buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }   

        for (int i = 0; i < count; i++) {
        char * cmd = "HEXIST";
        char key[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);

        const char *argv[] = {cmd, key};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(2, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len > buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }

    testcase(fd, buf, offset);
}


void skiplist_testcase(int fd, int count) {
    int buf_cap = 1024 * 1024;
    char * buf = (char *)malloc(buf_cap);
    if (buf == NULL) return;

    int offset = 0;

    for (int i = 0; i < count; i++) {
        char * cmd = "LSET";
        char key[BUFFER_SIZE];
        char value[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);
        snprintf(value, BUFFER_SIZE, "King%d", i);

        const char *argv[] = {cmd, key, value};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(3, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len >= buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }

    for (int i = 0; i < count; i++) {
        char * cmd = "LGET";
        char key[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);

        const char *argv[] = {cmd, key};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(2, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len >= buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }

    for (int i = 0; i < count; i++) {
        char * cmd = "LMOD";
        char key[BUFFER_SIZE];
        char value[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);
        snprintf(value, BUFFER_SIZE, "King-%d", i);

        const char *argv[] = {cmd, key, value};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(3, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len >= buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }

 
    for (int i = 0; i < count; i++) {
        char * cmd = "LDEL";
        char key[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);

        const char *argv[] = {cmd, key};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(2, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len >= buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }   

        for (int i = 0; i < count; i++) {
        char * cmd = "LEXIST";
        char key[BUFFER_SIZE];
        
        snprintf(key, BUFFER_SIZE, "Teacher%d", i);

        const char *argv[] = {cmd, key};
        int msg_len = 0;
        char * msg = kvs_build_kvsp_frame(2, argv, &msg_len);
        if (msg == NULL || msg_len <= 0) exit(1);

        if (offset + msg_len > buf_cap) exit(1);
        memcpy(buf + offset, msg, msg_len);
        offset += msg_len;

        kvs_free(msg);
    }

    testcase(fd, buf, offset);
}


void testcase_all(int fd, int count) {
    rbtree_testcase(fd, count);
    skiplist_testcase(fd, count);
    // array_testcase(fd, count);
    hash_testcase(fd, count);
}

// usage: ./multicmd 127.0.0.1 2000 [mode]
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

    int count = 10;

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
