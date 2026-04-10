#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>


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

char * read_blog(FILE * fp){
    if(fp == NULL) return NULL;



    char *buffer = NULL;
    size_t cap = 0;
    ssize_t read_bytes;
    char *full_content = NULL;
    size_t total_size = 0;

    
    while ((read_bytes = getline(&buffer, &cap, fp)) != -1) {
        
        char *temp = realloc(full_content, total_size + read_bytes + 1);
        if (temp == NULL) {
            perror("realloc error");
            free(buffer);
            free(full_content);
            fclose(fp);
            return NULL;
        }
        full_content = temp;

        
        memcpy(full_content + total_size, buffer, read_bytes);
        total_size += read_bytes;
    }

    

    full_content[total_size] = '\0';

    
    free(buffer);

    return full_content;


}

int send_blog(int fd, char * cmd, char *key, char *value){
    if(cmd == NULL || key == NULL || value == NULL) return -1;


    int payload_length = strlen(cmd) + strlen(key) + strlen(value);
    int header_length = snprintf(NULL, 0, "%d*", payload_length);
    char * buffer = (char*)malloc(payload_length + header_length + 1);
    if(buffer == NULL) {
        free(value);
        return -1;
    }
    int total_length = sprintf(buffer, "%d*%s%s%s", payload_length, cmd, key, value);
    int ret = send(fd, buffer, total_length, 0);



    ret = recv(fd, buffer, total_length, 0);
    buffer[ret] = '\0';
    printf("%s", buffer);   
    free(value);
    
}

int array_send_blog(int fd){
    FILE * fp = fopen("./testcase_sample/blog.txt", "r");;
    if(fp == NULL) return -1;
    char * value = read_blog(fp);
    send_blog(fd, "SET ", "这是一篇博客 ", value);
    fclose(fp);
}

int rbtree_send_blog(int fd){
    FILE * fp = fopen("./testcase_sample/blog.txt", "r");;
    if(fp == NULL) return -1;
    char * value = read_blog(fp);
    send_blog(fd, "RSET ", "这是一篇博客 ", value);
    fclose(fp);
}
int hash_send_blog(int fd){
    FILE * fp = fopen("./testcase_sample/blog.txt", "r");;
    if(fp == NULL) return -1;
    char * value = read_blog(fp);
    send_blog(fd, "HSET ", "这是一篇博客 ", value);
    fclose(fp);
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

    if(mode == 0){
        rbtree_send_blog(connfd);
    }
    else if(mode == 1){

        array_send_blog(connfd);
    }
    else if(mode == 2){
        hash_send_blog(connfd);
    }


	return 0;
	
}