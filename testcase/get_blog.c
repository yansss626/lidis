#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

int connect_tcpserver(const char *ip, unsigned short port) {

	int connfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(struct sockaddr_in));

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);
	server_addr.sin_port = htons(port);

	if (0 !=  connect(connfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in))) {
		perror("connect");
		return -1;
	}
	
	return connfd;
	
}



int get_and_recv_blog(int fd, char *cmd, char * key){
    
    char w_buf[1024] = {0};
    int payload_length = strlen(cmd) + strlen(key);
    int length = sprintf(w_buf, "%d*%s%s", payload_length, cmd, key);
    int ret = send(fd, w_buf, length, 0);


    char * r_buf = (char *)malloc(4096);
    if(r_buf == NULL) return -1;
    int r_cap = 4096;
    int r_pos = 0;
    while(1){
        ret = recv(fd, r_buf + r_pos, r_cap - r_pos - 1, 0);

        if(ret <= 0){
            break;
        }
        else{
            r_pos += ret;
            if(r_pos > 2 && r_buf[r_pos - 1] == '\n' && r_buf[r_pos - 2] == '\r'){
                r_buf[r_pos] ='\0';
                printf("%s", r_buf);
                break;
            }

            if(r_cap - r_pos < 128){
                r_cap *= 2;
                char * temp  = (char *)realloc(r_buf, r_cap);
                if(temp == NULL) break;
                r_buf = temp;
            }

        }

    }


    
    free(r_buf);

}

int array_get(int fd){

    get_and_recv_blog(fd, "GET ", "这是一篇博客");

}
int rbtree_get(int fd){


    get_and_recv_blog(fd, "RGET ", "这是一篇博客");

}
int hash_get(int fd){

    get_and_recv_blog(fd, "HGET ", "这是一篇博客");

}
//testcase 192.168.243.131  2000 mode: 0 for rbtree, 1 for array, 2 for hash
int main(int argc, char *argv[]) {

	if (argc != 4) {
		printf("arg error\n");
		return -1;
	}

	char *ip = argv[1];
	int port = atoi(argv[2]);
    int mode = atoi(argv[3]);
	int connfd = connect_tcpserver(ip, port);

    if(mode == 1){
        array_get(connfd);
    }
    else if(mode ==0){
        rbtree_get(connfd);
    }
    else if(mode == 2){
        hash_get(connfd);
    }

    close(connfd);
	return 0;
	
}