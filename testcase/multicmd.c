#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/time.h>
// **************** testcase for send multiple command at once  ****************
// ***************************************               ******************
#define MAX_MSG_LENGTH		4096
#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)


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


void rbtree_testcase_1w(int connfd) {

	int count = 10;
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
    // printf("%s\n", cmd);

    testcase(connfd, cmd);

}

void array_testcase_1w(int connfd) {

	int count = 10;
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
void hash_testcase_1w(int connfd) {

	int count = 10;
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


// testcase 192.168.243.131  2000 mode: 0 for rbtree, 1 for array, 2 for hash 
int main(int argc, char *argv[]) {

	if (argc != 4) {
		printf("arg error\n");
		return -1;
	}

	char *ip = argv[1];
	int port = atoi(argv[2]);
	int mode = atoi(argv[3]);

	int connfd = connect_tcpserver(ip, port);

	if(mode == 0) rbtree_testcase_1w(connfd);
	else if(mode == 1) array_testcase_1w(connfd);
	else if (mode == 2) hash_testcase_1w(connfd);


	return 0;
	
}