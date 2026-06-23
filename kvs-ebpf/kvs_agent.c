#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <bpf/libbpf.h>
#include <string.h>
#include "kvs_agent.skel.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include "kvs_agent.h"

#define KVS_SLAVE_MAX_COUNT 32 // Max size for slaves count
#define SERVER_PORT 2200

kvs_slaves global_slaves = {0};
pthread_mutex_t mutex;

bool volatile existing = false;

static void sig_handler(int sig) {
    existing = true;
}

static int handle_event(void *ctx, void *data, size_t data_sz){

	const struct tcp_event * e = (struct tcp_event *)data;
    if (e->ret <= 0) return 0;
	printf("e->payload: %s\n", e->payload);

	pthread_mutex_lock(&mutex);
	for (int i = 0; i < global_slaves.size; i++) {
		int fd = global_slaves.table[i].fd;
		if (fd <= 0) continue;
		
		int len = send(fd, e->payload, e->payload_len, 0);
		if (len < 0) {
			close(fd);
			kvs_slaves_delete(&global_slaves, fd);
		}
		//printf("e->payload: %s\n", e->payload);
	}
	pthread_mutex_unlock(&mutex);

	return 0;
}

static int init_server(unsigned short port) {

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // 0.0.0.0
	servaddr.sin_port = htons(port); // , 

	if (-1 == bind(sockfd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr))) {
		printf("bind failed: %s\n", strerror(errno));
	}

	listen(sockfd, 10);

	return sockfd;
}

void * server(void * arg){

	int listenfd = init_server(SERVER_PORT);
	if(listenfd < 0) return NULL;

	struct sockaddr_in client_addr = {0};
	socklen_t client_len = sizeof(struct sockaddr);
	
	while(1){
		int client_fd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len);
		if (client_fd < 0) {
			perror("accept");
			return NULL;
		} 
		printf("client_fd: %d\n", client_fd);
		pthread_mutex_lock(&mutex);		
		kvs_slaves_insert(&global_slaves, client_fd);
		pthread_mutex_unlock(&mutex);		

	}

	return NULL;
}

int main() {

    struct  ring_buffer * rb = NULL;
    struct kvs_agent_bpf * skel = NULL;
    int err = 0;
    pthread_t pid;

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGPIPE, SIG_IGN);

    skel = kvs_agent_bpf__open_and_load();
    if (skel == NULL) {
        fprintf(stderr, "failed to open and load skeleton\n");
        return -1;
    }

    err = kvs_agent_bpf__attach(skel);
    if (err != 0) {
        fprintf(stderr, "failed to attach bpf program: %d\n", err);
        goto cleanup;
    }

    rb = ring_buffer__new(
        bpf_map__fd(skel->maps.rb),
        handle_event,
        NULL,
        NULL
    );
    if (rb == NULL) {
        fprintf(stderr, "ring_buffer__new error\n");
        err = -1;
        goto cleanup;
    }

    kvs_slaves_create(&global_slaves);
	pthread_mutex_init(&mutex, NULL);
	pthread_create(&pid, NULL, server, NULL);	

    while (!existing) {
        err = ring_buffer__poll(rb, 100);
        if (err == -EINTR) {
            err = 0;
            break;
        }
        if (err < 0) {
            fprintf(stderr, "ring_buffer__poll error\n");
            break;
        }

    }

    pthread_mutex_destroy(&mutex);
    cleanup:
        ring_buffer__free(rb);
        kvs_agent_bpf__destroy(skel);
        kvs_slaves_destroy(&global_slaves);
    
    return err;
}

int kvs_slaves_create(kvs_slaves * inst){
    if(inst == NULL) return -1;
    if(inst->table != NULL) return 0;

    inst->table = (kvs_slave_item *)malloc(sizeof(kvs_slave_item) * KVS_SLAVE_MAX_COUNT);
    if(inst->table == NULL) {
        printf("kvs_malloc error\n");
        return -2;
    }
    memset(inst->table, 0, sizeof(kvs_slave_item) * KVS_SLAVE_MAX_COUNT);

    inst->total = 0;
    inst->size = KVS_SLAVE_MAX_COUNT;

    return 0;
}

int kvs_slaves_insert(kvs_slaves * inst, int fd){
    if(inst == NULL) return -1;
    if(inst->total == inst->size) return -2;

    int i = 0;
    for(; i < inst->size; i++){
        if(inst->table[i].fd == 0) {
            inst->table[i].fd = fd;
            break;
        }
    }

    ++(inst->total);
    
    return 0;
}

int kvs_slaves_delete(kvs_slaves * inst, int fd){
    if(inst == NULL) return -1;
    if(inst->total == 0) return 0;

    int i = 0;
    for(; i < inst->size; i++){
        if(inst->table[i].fd == fd) {
            inst->table[i].fd = 0;
            --(inst->total);
            break;
        }
    }

    return 0;
}

int kvs_slaves_destroy(kvs_slaves * inst){
    if(inst == NULL) return -1;
    
    if (inst->table != NULL) {
		for (int i = 0; i < global_slaves.size; i++) {
			int fd = global_slaves.table[i].fd;
			if ( fd > 0) close(fd);
		}
	free(inst->table);
	}
    inst->table = NULL;

    return 0;
}