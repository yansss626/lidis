#include  <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
// *************************************** test basis function ******************
// ****************  ****************
#define MAX_MSG_LENGTH		1024


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




void testcase(int connfd, char *msg, char *pattern, char *casename) {

	if (!msg || !pattern || !casename) return ;
	char new_msg[MAX_MSG_LENGTH] = {0};
	int total_length = snprintf(new_msg, MAX_MSG_LENGTH, "%ld*%s", strlen(msg), msg);
	//send_msg(connfd, msg, strlen(msg));
	send_msg(connfd, new_msg, total_length);

	char result[MAX_MSG_LENGTH] = {0};
	recv_msg(connfd, result, MAX_MSG_LENGTH);

	if (strcmp(result, pattern) == 0) {
	    printf("%s --> %s\n", msg, result);
	} else {
		printf("==> FAILED -> %s, '%s' != '%s' \n", casename, result, pattern);
		exit(1);
	}

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



void skiplist_testcase(int connfd, int count) {

	int i = 0;
    printf("skiptable: \n");

    printf("[LSET]: ");
	for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        snprintf(cmd, 128, "LSET Teacher%d King%d", i, i);
        testcase(connfd, cmd, "OK\r\n", "LSET");

	}
    printf("[LGET]: ");
    for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        char reply[128] = {0};
        snprintf(cmd, 128, "LGET Teacher%d",i);
        snprintf(reply, 128, "King%d\r\n",i);
        testcase(connfd, cmd, reply, "LGET");
		
	}
    printf("[LMOD]: ");
    for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        snprintf(cmd, 128, "LMOD Teacher%d King9%d", i, i);
        testcase(connfd, cmd, "OK\r\n", "LMOD");

	}
    printf("[LDEL]: ");
    for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        snprintf(cmd, 128, "LDEL Teacher%d", i);
        testcase(connfd, cmd, "OK\r\n", "LDEL");

	}
    printf("[LEXIST]: ");
    for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        snprintf(cmd, 128, "LEXIST Teacher%d", i);
        testcase(connfd, cmd, "NO EXIST\r\n", "LEXIST");
	}

    printf("\n");


}


void rbtree_testcase(int connfd, int count) {

	int i = 0;
    printf("rbtree: \n");

    printf("[RSET]: ");
	for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        snprintf(cmd, 128, "RSET Teacher%d King%d", i, i);
        testcase(connfd, cmd, "OK\r\n", "RSET");

	}
    printf("[RGET]: ");
    for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        char reply[128] = {0};
        snprintf(cmd, 128, "RGET Teacher%d",i);
        snprintf(reply, 128, "King%d\r\n",i);
        testcase(connfd, cmd, reply, "RGET");
		
	}
    printf("[RMOD]: ");
    for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        snprintf(cmd, 128, "RMOD Teacher%d King9%d", i, i);
        testcase(connfd, cmd, "OK\r\n", "RMOD");

	}
    printf("[RDEL]: ");
    for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        snprintf(cmd, 128, "RDEL Teacher%d", i);
        testcase(connfd, cmd, "OK\r\n", "RDEL");

	}
    printf("[REXIST]: ");
    for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        snprintf(cmd, 128, "REXIST Teacher%d", i);
        testcase(connfd, cmd, "NO EXIST\r\n", "REXIST");
	}

    printf("\n");


}

void hash_testcase(int connfd, int count) {

	int i = 0;
    printf("hash: \n");

    printf("[HSET]: ");
	for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        snprintf(cmd, 128, "HSET Teacher%d King%d", i, i);
        testcase(connfd, cmd, "OK\r\n", "HSET");

	}
    printf("[HGET]: ");
    for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        char reply[128] = {0};
        snprintf(cmd, 128, "HGET Teacher%d",i);
        snprintf(reply, 128, "King%d\r\n",i);
        testcase(connfd, cmd, reply, "HGET");
		
	}
    printf("[HMOD]: ");
    for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        snprintf(cmd, 128, "HMOD Teacher%d King9%d", i, i);
        testcase(connfd, cmd, "OK\r\n", "HMOD");

	}
    printf("[HDEL]: ");
    for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        snprintf(cmd, 128, "HDEL Teacher%d", i);
        testcase(connfd, cmd, "OK\r\n", "HDEL");

	}
    printf("[HEXIST]: ");
    for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        snprintf(cmd, 128, "HEXIST Teacher%d", i);
        testcase(connfd, cmd, "NO EXIST\r\n", "HEXIST");
	}

    printf("\n");


}

void array_testcase(int connfd, int count) {

	int i = 0;
    printf("array: \n");

    printf("[SET]: ");
	for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        snprintf(cmd, 128, "SET Teacher%d King%d", i, i);
        testcase(connfd, cmd, "OK\r\n", "SET");

	}
    printf("[GET]: ");
    for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        char reply[128] = {0};
        snprintf(cmd, 128, "GET Teacher%d",i);
        snprintf(reply, 128, "King%d\r\n",i);
        testcase(connfd, cmd, reply, "GET");
		
	}
    printf("[MOD]: ");
    for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        snprintf(cmd, 128, "MOD Teacher%d King9%d", i, i);
        testcase(connfd, cmd, "OK\r\n", "MOD");

	}
    printf("[DEL]: ");
    for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        snprintf(cmd, 128, "DEL Teacher%d", i);
        testcase(connfd, cmd, "OK\r\n", "DEL");

	}
    printf("[EXIST]: ");
    for (i = 0;i < count;i ++) {
        
        char cmd[128] = {0};
        snprintf(cmd, 128, "EXIST Teacher%d", i);
        testcase(connfd, cmd, "NO EXIST\r\n", "EXIST");
	}

    printf("\n");


}

void testcase_(int connfd, int count){

    skiplist_testcase(connfd, count);
    rbtree_testcase(connfd, count);
    array_testcase(connfd, count);
    hash_testcase(connfd, count);
}

// testcase 192.168.243.131  2000 mode: 0 for rbtree, 1 for array, 2 for hash 3 for skiplist
int main(int argc, char *argv[]) {

	if (argc < 3) {
		printf("arg error\n");
		return -1;
	}

	char *ip = argv[1];
	int port = atoi(argv[2]);

	int connfd = connect_tcpserver(ip, port);
	int count = 1;
	if(argc == 4){
		int mode = atoi(argv[3]);
		if(mode == 0) rbtree_testcase(connfd, count);
		else if(mode == 1) array_testcase(connfd, count); 
		else if (mode == 2) hash_testcase(connfd, count);
		else if (mode == 3) skiplist_testcase(connfd,count);
	}
	else if(argc == 3){
		testcase_(connfd, count);
	}


	return 0;
	
}


