#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/time.h>
// **************** testcase for send multiple command at once  ****************
// ***************************************     建议服务端BUFFER SIZE 设置为 1024          ******************
#define MAX_MSG_LENGTH		4096



int send_msg(int connfd, char *msg, int length) {

	int res = send(connfd, msg, length, 0);
	if (res < 0) {
		perror("send");
		exit(1);
	}
	return res;
}

int recv_msg(int connfd, char *msg, int length) {

	int res = recv(connfd, msg, length, 0);
	if (res < 0) {
		perror("recv");
		exit(1);
	}
	return res;

}




void testcase(int connfd, char *msg) {

	if (!msg) return ;

    printf("\nsend:\n%s\n", msg);
	send_msg(connfd, msg, strlen(msg));
	//send_msg(connfd, new_msg, total_length);

	char result[MAX_MSG_LENGTH] = {0};
	recv_msg(connfd, result, MAX_MSG_LENGTH);
    printf("\nrecv:\n%s\n", result);

}



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


void rbtree_testcase(int connfd, int count) {

	int i = 0;

    char cmd[MAX_MSG_LENGTH] = {0};
    int total_length = 0;
	for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "RSET Teacher%d King%d", i, i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);

	}
    for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "RGET Teacher%d",i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);
		
	}
    for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "RMOD Teacher%d King9%d", i, i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);

	}
    for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "RDEL Teacher%d", i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);

	}
    for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "REXIST Teacher%d", i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);

	}
    // printf("strlen: %ld\n", strlen(cmd));

    testcase(connfd, cmd);

}

void array_testcase(int connfd, int count) {

	int i = 0;

    char cmd[MAX_MSG_LENGTH] = {0};
    int total_length = 0;
	for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "SET Teacher%d King%d", i, i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);

	}
    for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "GET Teacher%d",i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);
		
	}
    for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "MOD Teacher%d King9%d", i, i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);

	}
    for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "DEL Teacher%d", i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);

	}
    for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "EXIST Teacher%d", i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);

	}
    // printf("strlen: %ld\n", strlen(cmd));
    // printf("%s\n", cmd);

    testcase(connfd, cmd);


}
void hash_testcase(int connfd, int count) {

	int i = 0;

    char cmd[MAX_MSG_LENGTH] = {0};
    int total_length = 0;
	for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "HSET Teacher%d King%d", i, i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);

	}
    for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "HGET Teacher%d",i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);
		
	}
    for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "HMOD Teacher%d King9%d", i, i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);

	}
    for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "HDEL Teacher%d", i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);

	}
    for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "HEXIST Teacher%d", i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);

	}
    // printf("strlen: %ld\n", strlen(cmd));
    // printf("%s\n", cmd);

    testcase(connfd, cmd);

}


void skiplist_testcase(int connfd, int count) {

	int i = 0;

    char cmd[MAX_MSG_LENGTH] = {0};
    int total_length = 0;
	for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "LSET Teacher%d King%d", i, i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);

	}
    for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "LGET Teacher%d",i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);
		
	}
    for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "LMOD Teacher%d King9%d", i, i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);

	}
    for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "LDEL Teacher%d", i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);

	}
    for (i = 0;i < count;i ++) {
        
        char temp[128] = {0};
        snprintf(temp, 128, "LEXIST Teacher%d", i);
        
        total_length  += snprintf(cmd + total_length, MAX_MSG_LENGTH, "%ld*%s", strlen(temp), temp);

	}
    // printf("strlen: %ld\n", strlen(cmd));
    // printf("%s\n", cmd);

    testcase(connfd, cmd);

}

// testcase 192.168.243.131  2000 mode: 0 for rbtree, 1 for array, 2 for hash 3 for skiplist
int main(int argc, char *argv[]) {

	if (argc != 4) {
		printf("arg error\n");
		return -1;
	}

	char *ip = argv[1];
	int port = atoi(argv[2]);
	int mode = atoi(argv[3]);

	int connfd = connect_tcpserver(ip, port);
    int count = 10;
	if(mode == 0) rbtree_testcase(connfd, count);
	else if(mode == 1) array_testcase(connfd, count);
	else if (mode == 2) hash_testcase(connfd, count);
    else if (mode == 3) skiplist_testcase(connfd, count);

	return 0;
	
}