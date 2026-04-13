
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
// *************************************** SET for 10000 times ******************
// **************** testcase for log module ****************
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
		printf("==> PASS ->  %s\n", casename);
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


void rbtree_testcase(int connfd, int count) {

	int i = 0;

	for (i = 0;i < count;i ++) {

		char cmd[128] = {0};
		snprintf(cmd, 128, "RDEL Teacher%d King%d", i, i);
		testcase(connfd, cmd, "OK\r\n", cmd);
	}


}
void array_testcase(int connfd, int count) {

	int i = 0;


	for (i = 0;i < count;i ++) {

		char cmd[128] = {0};
		snprintf(cmd, 128, "DEL Teacher%d King%d", i, i);
		testcase(connfd, cmd, "OK\r\n", cmd);
	}


}
void hash_testcase(int connfd, int count) {

	int i = 0;



	for (i = 0;i < count;i ++) {

		char cmd[128] = {0};
		snprintf(cmd, 128, "HDEL Teacher%d King%d", i, i);
		testcase(connfd, cmd, "OK\r\n", cmd);
	}


}

void skiplist_testcase(int connfd, int count) {

	int i = 0;



	for (i = 0;i < count;i ++) {

		char cmd[128] = {0};
		snprintf(cmd, 128, "LDEL Teacher%d King%d", i, i);
		testcase(connfd, cmd, "OK\r\n", cmd);
	}


}

void testcase_(int connfd, int count){

	int i = 0;

	rbtree_testcase(connfd, count);
	skiplist_testcase(connfd, count);
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
	int count = 10000;
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
