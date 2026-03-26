// *************************************** RSET for 10000 times ******************
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/time.h>


#define MAX_MSG_LENGTH		1024
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




void testcase(int connfd, char *msg, char *pattern, char *casename) {

	if (!msg || !pattern || !casename) return ;

	send_msg(connfd, msg, strlen(msg));

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


void rbtree_testcase_100(int connfd) {

	int count = 100;
	int i = 0;

	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);

	// for (i = 0;i < count;i ++) {

	// 	char cmd[128] = {0};
	// 	snprintf(cmd, 128, "RSET Teacher%d King%d", i, i);
	// 	testcase(connfd, cmd, "OK\r\n", "RSET-Teacher");
	// }
    
	// for (i = 0;i < count;i ++) {

	// 	char cmd[128] = {0};
	// 	snprintf(cmd, 128, "RMOD Teacher%d King-%d", i, i);
	// 	testcase(connfd, cmd, "OK\r\n", "RMOD-Teacher");
	// }
    
	// for (i = 0;i < count;i ++) {

	// 	char cmd[128] = {0};
	// 	snprintf(cmd, 128, "RDEL Teacher%d King-%d", i, i);
	// 	testcase(connfd, cmd, "OK\r\n", "RDEL-Teacher");
	// }

	for (i = 0;i < count;i ++) {

		char cmd[128] = {0};
		snprintf(cmd, 128, "HSET Teacher%d King2222%d", i, i);
		testcase(connfd, cmd, "OK\r\n", "HSET-Teacher");
	}
	char cmd[128] = {0};
	snprintf(cmd, 128, "HSAVE");
	testcase(connfd, cmd, "OK\r\n", "HSAVE");




    // for (i = 0;i < count;i ++) {
    //     char cmd[128] = {0};
    //     snprintf(cmd, 128, "SET Teacher%d King%d", i, i);
    //     testcase(connfd, cmd, "OK\r\n", "SET-Teacher");
    // }
    
	// for (i = 0;i < count;i ++) {

	// 	char cmd[128] = {0};
	// 	snprintf(cmd, 128, "MOD Teacher%d King-%d", i, i);
	// 	testcase(connfd, cmd, "OK\r\n", "MOD-Teacher");
	// }
    
	// for (i = 0;i < count;i ++) {

	// 	char cmd[128] = {0};
	// 	snprintf(cmd, 128, "DEL Teacher%d King-%d", i, i);
	// 	testcase(connfd, cmd, "OK\r\n", "DEL-Teacher");
	// }

    // for (i = 0;i < count;i ++) {
    // char cmd[128] = {0};
    // snprintf(cmd, 128, "HSET Teacher%d King%d", i, i);
    // testcase(connfd, cmd, "OK\r\n", "HSET-Teacher");
    // }
    
	// for (i = 0;i < count;i ++) {

	// 	char cmd[128] = {0};
	// 	snprintf(cmd, 128, "HMOD Teacher%d King-%d", i, i);
	// 	testcase(connfd, cmd, "OK\r\n", "HMOD-Teacher");
	// }
    
	// for (i = 0;i < count;i ++) {

	// 	char cmd[128] = {0};
	// 	snprintf(cmd, 128, "HDEL Teacher%d King-%d", i, i);
	// 	testcase(connfd, cmd, "OK\r\n", "HDEL-Teacher");
	// }
    

	struct timeval tv_end;
	gettimeofday(&tv_end, NULL);

	int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms

	printf("rbtree testcase --> time_used: %d, qps: %d\n", time_used, 900 * 1000 / time_used);

}



// testcase 192.168.243.131  2000
int main(int argc, char *argv[]) {

	if (argc != 3) {
		printf("arg error\n");
		return -1;
	}

	char *ip = argv[1];
	int port = atoi(argv[2]);

	int connfd = connect_tcpserver(ip, port);

	rbtree_testcase_100(connfd);


	return 0;
	
}
