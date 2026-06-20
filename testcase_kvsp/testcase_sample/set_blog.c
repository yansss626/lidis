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

void create_testcase(int fd, char * value, char * cmd) {

    char * key = "This is a blog";
    const char *argv[] = {cmd, key, value};
    testcase(fd, 3, argv, "+OK\r\n", cmd);
    
}

void rbtree_testcase(int fd, char * value) {
    create_testcase(fd, value, "RSET");
}

void array_testcase(int fd, char * value) {
    create_testcase(fd, value, "SET");
}


void hash_testcase(int fd, char * value) {
    create_testcase(fd, value, "HSET");
}


void skiplist_testcase(int fd, char * value) {
    create_testcase(fd, value, "LSET");
}


// usage: ./set_blog 127.0.0.1 2000 [mode]
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

    char *blog = (char *)malloc(statbuf.st_size + 1);
    if (blog == NULL) {
        perror("malloc");
        munmap(ptr, statbuf.st_size);
        close(fd);
        return 1;
    }

    memcpy(blog, ptr, statbuf.st_size);
    blog[statbuf.st_size] = '\0';    


    if (argc == 4) {
        int mode = atoi(argv[3]);
        switch (mode) {
            case 0:
                rbtree_testcase(sockfd, blog);
                break;
            case 1:
                array_testcase(sockfd, blog);
                break;
            case 2:
                hash_testcase(sockfd, blog);
                break;
            case 3:
                skiplist_testcase(sockfd, blog);
                break;
            default:
                fprintf(stderr, "invalid mode\n");
                close(fd);
                return 1;
        }
    }

    free(blog);
    close(fd);
    munmap(ptr, statbuf.st_size);
    return 0;
}
