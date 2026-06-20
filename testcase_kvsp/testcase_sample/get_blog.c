#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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

int recv_msg(int connfd, char *msg, int cap, int expected_len) {
    if (msg == NULL || cap <= 0 || expected_len <= 0 || expected_len >= cap) {
        return -1;
    }

    int received = 0;

    while (received < expected_len) {
        ssize_t n = recv(connfd, msg + received, expected_len - received, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recv");
            return -2;
        }
        if (n == 0) {
            fprintf(stderr, "connection closed\n");
            return -2;
        }
        received += n;

        if(memcmp(msg, "$-1\r\n", 5) == 0) break;
    }

    msg[received] = '\0';
    return received;

}




static void testcase(int connfd, int argc, const char * argv[], const char * expected_reply, off_t expected_reply_len, const char *case_name) {

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

    char * reply = (char *)kvs_malloc(expected_reply_len + 1);

    // if (recv_msg(connfd, reply, expected_reply_len + 1, expected_reply_len) < 0) {
    //     exit(1);
    // }
    int recevied = recv_msg(connfd, reply, expected_reply_len + 1, expected_reply_len);
    if (recevied <= 0) exit(1);
    printf("received: %d\n", recevied);
    if (memcmp(reply, expected_reply, expected_reply_len) == 0) {
        printf("==> PASS -> %s\n", expected_reply);
    } else {
        printf("==> FAILED -> %s \n", reply);
        exit(1);
    }

    kvs_free(msg);
    kvs_free(reply);
}

char *build_bulk_reply_alloc(const char *value, size_t value_size, int *reply_len) {
    if (value == NULL || reply_len == NULL) return NULL;

    int header_len = snprintf(NULL, 0, "$%zu\r\n", value_size);
    if (header_len <= 0) return NULL;

    int total_len = header_len + value_size + 2;

    char *buf = (char *)kvs_malloc(total_len + 1);
    if (buf == NULL) return NULL;

    int offset = snprintf(buf, total_len + 1, "$%zu\r\n", value_size);

    memcpy(buf + offset, value, value_size);
    offset += value_size;

    memcpy(buf + offset, "\r\n", 2);
    offset += 2;

    buf[offset] = '\0';

    *reply_len = total_len;
    return buf;
}

void create_testcase(int fd, char * value, off_t value_size, char * cmd) {

    char * key = "This is a blog";
    const char *argv[] = {cmd, key};

    int expected_len = 0;
    char *expected_reply = build_bulk_reply_alloc(value, value_size, &expected_len);

    testcase(fd, 2, argv, expected_reply, expected_len, cmd);

}

void rbtree_testcase(int fd, char * value, off_t value_size) {
    create_testcase(fd, value, value_size, "RGET");
}

void array_testcase(int fd, char * value, off_t value_size) {
    create_testcase(fd, value, value_size, "GET");
}


void hash_testcase(int fd, char * value, off_t value_size) {
    create_testcase(fd, value, value_size, "HGET");
}


void skiplist_testcase(int fd, char * value, off_t value_size) {
    create_testcase(fd, value, value_size, "LGET");
}



// usage: ./get_blog 127.0.0.1 2000 [mode]
// mode: 0 rbtree, 1 array, 2 hash, 3 skiplist
int main(int argc, char *argv[]) {
    if (argc < 3) {
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int sockfd = connect_server(ip, port);
    if (sockfd < 0) {
        return 1;
    }

    int fd = open("./testcase_sample/blog.txt", O_RDONLY);
    if (fd < 0){
        perror("open");
        return 1;
    }

    struct stat statbuf = {0};
    fstat(fd, &statbuf);
    if (statbuf.st_size == 0) {
        fprintf(stderr, "blog is empty\n");
        close(fd);
        return 1;
    }


    char * ptr = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("open");
        close(fd);
        return 1;
    }

    if (argc == 4) {
        int mode = atoi(argv[3]);
        switch (mode) {
            case 0:
                rbtree_testcase(sockfd, ptr, statbuf.st_size);
                break;
            case 1:
                array_testcase(sockfd, ptr, statbuf.st_size);
                break;
            case 2:
                hash_testcase(sockfd, ptr, statbuf.st_size);
                break;
            case 3:
                skiplist_testcase(fd, ptr, statbuf.st_size);
                break;
            default:
                fprintf(stderr, "invalid mode\n");
                close(fd);
                return 1;
        }
    }

    close(fd);
    munmap(ptr, statbuf.st_size);
    return 0;
}
