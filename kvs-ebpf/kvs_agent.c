#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <bpf/libbpf.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>
#include <sys/epoll.h>
#include "kvs_agent.h"
#include "kvs_agent.skel.h"

#define SERVER_PORT             2200

#define MAX_EPOLL_SIZE          4096

#define MAX_AGENT_CACHE_SIZE    (1024 * 1024 * 256)  //256 MB   
#define MAX_FULLSYNC_CACHE_SIZE (1024 * 1024 * 512)    // 512 MB

#define MAX_SLAVE_COUNT         4 // Max size for slaves count

#define BUFFER_SIZE             1024

static pthread_mutex_t cache_mutex;
static pthread_cond_t  cache_cond;

static pthread_mutex_t slaves_mutex;

static pthread_t server_tid;
static pthread_t agent_tid;

agent_cache global_agent_cache = {0};
kvs_slaves global_slaves = {0};

bool volatile existing = false;

int kvs_agent_init(agent_info * agent);
int kvs_agent_deinit(agent_info * agent);
void * server(void * arg);
void * kvs_agent(void * arg);

const char * kvs_command[] = {
	"SET", "DEL", "MOD",
    "RSET", "RDEL", "RMOD",
    "HSET", "HDEL", "HMOD",
    "LSET", "LDEL", "LMOD",
    "SYNC", "FULLSYNC FINISHED", "FULLSYNC ERROR"
};

enum {
	KVS_CMD_START = 0,
	// array
	KVS_CMD_SET = KVS_CMD_START,
	KVS_CMD_DEL,
	KVS_CMD_MOD,

	// rbtree
	KVS_CMD_RSET,
	KVS_CMD_RDEL,
	KVS_CMD_RMOD,

	// hash
	KVS_CMD_HSET,
	KVS_CMD_HDEL,
	KVS_CMD_HMOD,
	
	// skiplist
	KVS_CMD_LSET,
	KVS_CMD_LDEL,
	KVS_CMD_LMOD,

	KVS_CMD_FLAG,	// 	

	KVS_CMD_SYNC = KVS_CMD_FLAG,
    KVS_CMD_FULLSYNC_FINISHED,
    KVS_CMD_FULLSYNC_ERROR,

	KVS_CMD_COUNT,

	KVS_ALLOWED_SEND
};

static void sig_handler(int sig) {
    existing = true;
}

static int handle_event(void *ctx, void *data, size_t data_sz){

	const struct tcp_event * e = (struct tcp_event *)data;
    if (e->ret <= 0) return 0;

    pthread_mutex_lock(&cache_mutex);		

    int ret = kvs_agent_cache_write(&global_agent_cache, e->payload, e->payload_len);
    if (ret != 0) {
        fprintf(stderr, "kvs_agent_cache_write error\n");
        existing = true;
    }

    pthread_cond_signal(&cache_cond);
    pthread_mutex_unlock(&cache_mutex);

	return 0;
}

int main() {

    struct  ring_buffer * rb = NULL;
    struct kvs_agent_bpf * skel = NULL;
    int err = 0;
    
    agent_info agent = {0};
    err = kvs_agent_init(&agent);
    if (err != 0) {
        fprintf(stderr, "kvs_agent_init error\n");
        return -1;
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGPIPE, SIG_IGN);

    skel = kvs_agent_bpf__open_and_load();
    if (skel == NULL) {
        fprintf(stderr, "failed to open and load skeleton\n");
        goto cleanup;
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

    cleanup:
        ring_buffer__free(rb);
        kvs_agent_bpf__destroy(skel);
        kvs_agent_deinit(&agent);
    
    return err;
}

static int init_server(unsigned short port) {

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	struct sockaddr_in servaddr = {0};
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // 0.0.0.0
	servaddr.sin_port = htons(port); // , 

	if (-1 == bind(sockfd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr))) {
		printf("bind failed: %s\n", strerror(errno));
        return -1;
	}

	listen(sockfd, 10);

	return sockfd;
}

void * server(void * arg) {

	int listenfd = init_server(SERVER_PORT);
	if(listenfd < 0) exit(1);

    int epfd = epoll_create(1);
    struct epoll_event ev = {0};
    ev.data.fd = listenfd;
    ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

    struct epoll_event events[MAX_EPOLL_SIZE] = {0};

	struct sockaddr_in client_addr = {0};
	socklen_t client_len = sizeof(struct sockaddr);
	
	while(!existing){

        int nready = epoll_wait(epfd, events, MAX_EPOLL_SIZE, 100);

        if (nready < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nready; i++) {
            struct epoll_event event = events[i];

            if (event.data.fd == listenfd) {
                int client_fd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len);
                if (client_fd < 0) {
                    perror("accept");
                    return NULL;
                } 

                printf("client_fd: %d\n", client_fd);
                pthread_mutex_lock(&slaves_mutex);		
                kvs_slaves_insert(&global_slaves, client_fd);
                pthread_mutex_unlock(&slaves_mutex);	

                struct epoll_event ev = {0};
                ev.data.fd = client_fd;
                ev.events = EPOLLIN;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);
            }
            else {
                if (event.events == EPOLLIN) {
                    char buf[BUFFER_SIZE] = {0};
                    ssize_t ret = recv(event.data.fd, buf, BUFFER_SIZE, 0);
                    if (ret <= 0) {
                        if (ret < 0) perror("recv");

                        epoll_ctl(epfd, EPOLL_CTL_DEL, event.data.fd, &ev);

                        pthread_mutex_lock(&slaves_mutex);		
                        kvs_slaves_delete(&global_slaves, event.data.fd);
                        pthread_mutex_unlock(&slaves_mutex);    
                        
                        close(event.data.fd);
                    }
                }
            }
        }
	}

	return NULL;
}

static int send_all(int fd, char *buf, size_t buf_len) {
    if (fd < 0 || buf == NULL || buf_len == 0) return -1;

    size_t pos = 0;

    while(pos < buf_len) {
        ssize_t n = send(fd, buf + pos, buf_len - pos, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            perror("send");
            return -2;
        }

        if (n == 0) {
            fprintf(stderr, "connection closed\n");
            return -2;
        }

        pos += n;
    }

    return 0;
}

static int kvs_agent_send(char * buf, size_t buf_len) {
    if (buf == NULL || buf_len == 0) return -1;

    pthread_mutex_lock(&slaves_mutex);

    for(int i = 0; i < global_slaves.size; i++) {
        int fd = global_slaves.table[i].fd;
        if(fd > 0) { 
            int ret = send_all(fd, buf, buf_len); 
            if (ret != 0) {
                kvs_slaves_delete(&global_slaves, fd);
            }
        }        
    }

    pthread_mutex_unlock(&slaves_mutex);

    return 0;
}

static int kvs_agent_send_cache(agent_cache * ac) {
    if (ac == NULL) return -1;

    int ret = 0;

    while (ac->used > 0) {
        size_t buf_len = 0;
        char * buf = kvs_agent_cache_read(ac, &buf_len);

        if (buf_len == 0 || buf == NULL) {
            fprintf(stderr, "kvs_agent_cache_read error\n");
            return -2;
        }

        ret = kvs_agent_send(buf, buf_len);
        free(buf);
        if (ret != 0) return ret;
    }

    return 0;
}

char * kvs_agent_parse_command(agent_info * agent) {
    if (agent == NULL) return NULL;

	char * body = agent->buf + agent->cmd_hl;

	int pos = 0;
    int idx = 0;

    int body_len = agent->cmd_tl - agent->cmd_hl;

    if (body_len < KVSP_END_STR_LEN) return NULL;

    int parse_end = body_len - KVSP_END_STR_LEN;
    if (memcmp(body + parse_end, KVSP_END_STR, KVSP_END_STR_LEN) != 0) return NULL;

    int has_digit = 0;
    int tok_len = 0;

    if (body[pos] != KVSP_TOK_LEN_START_CHAR) return NULL;

    if (idx > 0) body[pos] = '\0';

    pos++;

    while (pos < parse_end && body[pos] <= '9' && body[pos] >= '0') {
        has_digit = 1;
        
        int digit = body[pos] - '0';
        if (tok_len > (INT_MAX - digit) / 10) return NULL;
        tok_len = 10 * tok_len + digit;

        pos++;
    }

    if (!has_digit) return NULL;
    
    if (pos >= parse_end) return NULL;

    if (body[pos] != KVSP_TOK_START_CHAR) return NULL;
    pos ++;

    if (tok_len > parse_end - pos) return NULL;
							
    char * command = (char *)malloc(tok_len + 1);
    if (command == NULL) {
        perror("malloc");
        return NULL;
    }  
    memcpy(command, body + pos, tok_len);
    command[tok_len] = '\0';

    return command;
} 

int kvs_agent_handler(agent_info * agent) {
    if (agent == NULL) return -1;

    char * command = kvs_agent_parse_command(agent);
    if (command == NULL) return -2;

    agent_cache * ac = &(agent->fullsync_cache);

    int cmd = KVS_CMD_START;
	for (cmd = KVS_CMD_START; cmd < KVS_CMD_COUNT; cmd ++) {

		if (strcmp(command, kvs_command[cmd]) == 0) {
            if (cmd < KVS_CMD_FLAG) cmd = KVS_ALLOWED_SEND;
			break;
		}
	}

    int ret = 0;

    switch (cmd)
    {
    case KVS_CMD_SYNC: 

        if (agent->state != KVS_SLAVE_FULL_SYNC) {
            agent->state = KVS_SLAVE_FULL_SYNC;
        }
        break;

    case KVS_CMD_FULLSYNC_FINISHED:

        if (agent->state == KVS_SLAVE_FULL_SYNC) {
            agent->state = KVS_SLAVE_INCR_SYNC;
            ret = kvs_agent_send_cache(ac);
        }
        break;

    case KVS_CMD_FULLSYNC_ERROR:

        if (agent->state == KVS_SLAVE_FULL_SYNC) {
            agent->state = KVS_SLAVE_INCR_SYNC;
            ret = kvs_agent_send_cache(ac);
        }
        break;

    case KVS_ALLOWED_SEND:

        if (agent->state == KVS_SLAVE_FULL_SYNC) {
            ret = kvs_agent_cache_write(ac, agent->buf, agent->cmd_tl);
            //printf("cache used: %zu\n", agent->fullsync_cache.used);
        }
        else ret = kvs_agent_send(agent->buf, agent->cmd_tl);
        break;

    default:
        break;
    }

    free(command);

    return ret;
}

int kvs_agent_send_to_slave (agent_info * agent, char * buf, size_t buf_len) {
    if (agent == NULL || buf == NULL || buf_len == 0) return -1;

    if (agent->buf_pos + buf_len > agent->buf_cap) {
        char * temp = (char *)realloc(agent->buf, agent->buf_pos + buf_len);
        if (temp == NULL) {
            perror("kvs_realloc error");
            return -2;
        }
        agent->buf = temp;
        agent->buf_cap = agent->buf_pos + buf_len;		
    }

    memcpy(agent->buf + agent->buf_pos, buf, buf_len);
    agent->buf_pos += buf_len;

    while (agent->buf_pos > 0) {

        int head_len = 0;
        int total_len = kvsp_parse_bulk_size(agent->buf, agent->buf_pos, &head_len);

        if (total_len == 0) return 0;
        else if (total_len < 0) return -2;
        
        agent->cmd_hl = head_len;
        agent->cmd_tl = total_len;

        int ret = kvs_agent_handler(agent);
        if (ret != 0) return -2;

        agent->buf_pos -= total_len;
        memmove(agent->buf, agent->buf + total_len, agent->buf_pos);
    }

    return 0;
}

void * kvs_agent(void * arg) {

    agent_info * agent = (agent_info *)arg;

    while (!existing) {
        pthread_mutex_lock(&cache_mutex);

        while (global_agent_cache.used == 0 && !existing) {
            pthread_cond_wait(&cache_cond, &cache_mutex);
        }

        if (existing && global_agent_cache.used == 0) {
            pthread_mutex_unlock(&cache_mutex);
            break;
        }

        size_t buf_len = 0;
        char * buf = kvs_agent_cache_read(&global_agent_cache, &buf_len);

        pthread_mutex_unlock(&cache_mutex);

        if (buf_len == 0 || buf == NULL) {
            fprintf(stderr, "kvs_agent_cache_read error\n");
            free(buf);
            existing = true;
            return NULL;
        }

        int ret = kvs_agent_send_to_slave(agent, buf, buf_len);
        if (ret != 0) {
            exit(1);
            free(buf);
            existing = true;
            return NULL;
        }

        free(buf);
    }

    return NULL;
}

int kvs_agent_info_init(agent_info * agent, size_t buf_size, size_t cache_size) {
    if (agent == NULL || buf_size == 0 || cache_size == 0) return -1;

    memset(agent, 0, sizeof(agent_info));

    agent->buf = (char *)malloc(buf_size);
    if (agent->buf == NULL) {
        perror("malloc");
        return -2;
    }
    
    int ret = kvs_agent_cache_create(&(agent->fullsync_cache), cache_size);
    if (ret != 0) {
        fprintf(stderr, "kvs_agent_cache_create error\n");
        free(agent->buf);
        return -2;
    }

    agent->buf_cap = buf_size;
    agent->state = KVS_SLAVE_UNKNOWN;

    return 0;
}

int kvs_agent_info_deinit(agent_info * agent) {
    if (agent == NULL) return -1;

    free(agent->buf);
    kvs_agent_cache_destroy(&(agent->fullsync_cache));

    return 0;
}

int kvs_agent_init(agent_info * agent) {
    if (agent == NULL) return -1;

    int ret = 0;

    ret = kvs_agent_info_init(agent, BUFFER_SIZE, MAX_FULLSYNC_CACHE_SIZE);
    if (ret != 0) {
        fprintf(stderr, "kvs_agent_info_init error\n");
        return -2;
    }

    ret = kvs_slaves_create(&global_slaves, MAX_SLAVE_COUNT);
    if (ret != 0) {
        fprintf(stderr, "kvs_slaves_create error\n");
        return -2;
    }

    ret = kvs_agent_cache_create(&(global_agent_cache), MAX_AGENT_CACHE_SIZE);
    if (ret != 0) {
        fprintf(stderr, "kvs_agent_cache_create error\n");
        return -2;
    }

    ret = pthread_mutex_init(&slaves_mutex, NULL);
    if (ret != 0) {
        fprintf(stderr, "pthread_mutex_init error: %d\n", ret);
        return -2;
    }

    ret = pthread_mutex_init(&cache_mutex, NULL);
    if (ret != 0) {
        fprintf(stderr, "pthread_mutex_init error: %d\n", ret);
        return -2;
    }

    ret = pthread_cond_init(&cache_cond, NULL);
    if (ret != 0) {
        fprintf(stderr, "pthread_cond_init error: %d\n", ret);
        return -2;
    }

    ret = pthread_create(&server_tid, NULL, server, NULL);
    if (ret != 0) {
        fprintf(stderr, "pthread_create error: %d\n", ret);
        return -2;
    }

    ret = pthread_create(&agent_tid, NULL, kvs_agent, agent);
    if (ret != 0) {
        fprintf(stderr, "pthread_create error: %d\n", ret);
        return -2;
    }
    
    return 0;
}

int kvs_agent_deinit(agent_info * agent) {
    if (agent == NULL) return -1;

    existing = true;

    pthread_mutex_lock(&cache_mutex);
    pthread_cond_broadcast(&cache_cond);
    pthread_mutex_unlock(&cache_mutex);

    pthread_join(server_tid, NULL);
    pthread_join(agent_tid, NULL);

    kvs_agent_info_deinit(agent);

    kvs_slaves_destroy(&global_slaves);

    kvs_agent_cache_destroy(&global_agent_cache);

    pthread_mutex_destroy(&cache_mutex);

    pthread_mutex_destroy(&slaves_mutex);

    pthread_cond_destroy(&cache_cond);

    return 0;
}

int kvsp_parse_bulk_size(const char * buf, size_t buf_size, int * head_len){
	if(buf == NULL || head_len == NULL || buf_size == 0) return -1;

	size_t pos = 0;
	int body_len = 0;
	int total_len = 0;

	if (buf_size < KVSP_START_STR_LEN) return 0;

	if (memcmp(buf, KVSP_START_STR, KVSP_START_STR_LEN) != 0) return -2;

	pos += KVSP_START_STR_LEN;

	int has_digit = 0;

	while (pos < buf_size && buf[pos] <= '9' && buf[pos] >= '0') {
		has_digit = 1;
		
		int digit = buf[pos] - '0';

		if (body_len > (INT_MAX - digit) / 10) return -2;

		body_len = 10 * body_len + digit;
		pos++;
	}

	if (!has_digit) {
		if (pos >= buf_size) {
			return 0;
		}
		return -2;
	}

	if (pos >= buf_size || (buf_size - pos) < KVSP_HEAD_TAIL_STR_LEN) return 0;

	if (memcmp(buf + pos, KVSP_HEAD_TAIL_STR, KVSP_HEAD_TAIL_STR_LEN) != 0 ) return -2;

	pos += KVSP_HEAD_TAIL_STR_LEN;

	if (body_len > (INT_MAX - pos) ) return -2;

	*head_len = pos;

	total_len = body_len + pos;

	return total_len > buf_size ? 0 : total_len;
}

