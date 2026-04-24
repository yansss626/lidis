// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
/* Copyright (c) 2020 Facebook */
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include "kvs_agent.skel.h"
#include "kvs_agent.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>


#define SYNC_SIZE 32 // Max size for slaves count
#define SERVER_PORT 2200

kvs_slaves global_slaves = {0};
pthread_mutex_t mutex;

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	return vfprintf(stderr, format, args);
}
static volatile bool exiting = false;

static void sig_handler(int sig)
{
	exiting = true;
}

static int handle_event(void *ctx, void *data, size_t data_sz){
	const struct event * e = (struct event *)data;
	
	pthread_mutex_lock(&mutex);
	for (int i = 0; i < global_slaves.size; i++) {
		int fd = global_slaves.table[i].fd;
		if (fd <= 0) continue;
		
		int len = send(fd, e->command, e->total_length, 0);
		if (len < 0) {
			close(fd);
			kvs_slaves_delete(&global_slaves, fd);
		}
		//printf("e->command: %s\n", e->command);
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
	//printf("listen finshed: %d\n", sockfd); // 3 

	return sockfd;

}


void * server(void * arg){

	int listenfd = init_server(SERVER_PORT);
	if(listenfd < 0) return NULL;
	struct sockaddr_in client_addr = {0};
	socklen_t client_len = sizeof(struct sockaddr);
	printf("listen_fd: %d\n", listenfd);	
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



int main(){
	struct kvs_agent_bpf * skel = NULL;
	int err = 0;
	struct ring_buffer * rb = NULL;
	pthread_t pid;



	LIBBPF_OPTS(bpf_uprobe_opts, uprobe_opts);
	/* Set up libbpf errors and debug info callback */
	libbpf_set_print(libbpf_print_fn);

	/* Cleaner handling of Ctrl-C */
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGPIPE, SIG_IGN);
	/* Load and verify BPF application */
	skel = kvs_agent_bpf__open_and_load();
	if (!skel) {
		fprintf(stderr, "Failed to open and load BPF skeleton\n");
		return 1;
	}


	/* we can also attach uprobe/uretprobe to any existing or future
	 * processes that use the same binary executable; to do that we need
	 * to specify -1 as PID, as we do here
	 */
	uprobe_opts.func_name = "kvs_incr_sync";
	uprobe_opts.retprobe = false;
	/* uprobe/uretprobe expects relative offset of the function to attach
	 * to. libbpf will automatically find the offset for us if we provide the
	 * function name. If the function name is not specified, libbpf will try
	 * to use the function offset instead.
	 */
	skel->links.trace_kvs_incr_sync = bpf_program__attach_uprobe_opts(
		skel->progs.trace_kvs_incr_sync, -1 /* self pid */, "/home/yansss/share/9.1-kvstore/kvstore",
		0 /* offset for function */, &uprobe_opts /* opts */);
	if (!skel->links.trace_kvs_incr_sync) {
		err = -errno;
		fprintf(stderr, "Failed to attach uprobe: %d\n", err);
		goto cleanup;
	}

	rb = ring_buffer__new(bpf_map__fd(skel->maps.rb), handle_event, NULL, NULL);
	if (!rb) {
		err = -1;
		fprintf(stderr, "Failed to create ring buffer\n");
		goto cleanup;
	}	
	kvs_slaves_create(&global_slaves);
	pthread_mutex_init(&mutex, NULL);
	pthread_create(&pid, NULL, server, NULL);	
	

	
	while (!exiting) {
		err = ring_buffer__poll(rb, 100 /* timeout, ms */);
		/* Ctrl-C will cause -EINTR */
		if (err == -EINTR) {
			err = 0;
			break;
		}
		if (err < 0) {
			printf("Error polling perf buffer: %d\n", err);
			break;
		}
	}
	pthread_mutex_destroy(&mutex);
	
	cleanup:
		/* Clean up */
		ring_buffer__free(rb);
		kvs_agent_bpf__destroy(skel);
		kvs_slaves_destroy(&global_slaves);		
		return -err;
}





int kvs_slaves_create(kvs_slaves * inst){

    if(inst == NULL) return -1;
    if(inst->table != NULL) return 0;

    inst->table = (kvs_slave_item *)malloc(sizeof(kvs_slave_item) * SYNC_SIZE);
    if(inst->table == NULL) {
        printf("kvs_malloc error\n");
        return -2;
    }
    memset(inst->table, 0, sizeof(kvs_slave_item) * SYNC_SIZE);

    inst->total = 0;
    inst->size = SYNC_SIZE;

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