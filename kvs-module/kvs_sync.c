#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include "nty_coroutine.h"
#include "kvstore.h"

#define BUFFER_SIZE 1024
#define ENTRY_LENGTH 1024

#define ENABLE_RDMA     0
#define ENABLE_SENDFILE     1

#define ENABLE_EBPF     1
#define ENABLE_SEND     0


#define KVS_SYNC_COMMAND               "SYNC"

#define KVS_SLAVE_FULLSYNC_READY       "FULL SYNC READY"
#define KVS_SLAVE_FULLSYNC_FINISHED    "FULL SYNC FINISHED"
#define KVS_SLAVE_FULLSYNC_ERROR       "FULL SYNC ERROR" 

#define KVS_MASTER_FULLSYNC_OK         "FULL SYNC OK %zu"
#define KVS_MASTER_FULLSYNC_ERROR      "FULL SYNC ERROR"
#define KVS_MASTER_FULLSYNC_BUSY       "FULL SYNC BUSY"


#define KVS_SYNC_RET_ERROR      -100
#define KVS_SYNC_RET_BUSY       -101

static int is_full_sync = 0;    // 用于判断主端是否正在与某个从端进行全量同步

extern kvs_conf_t global_config;

int kvs_connect_to_remote(const char * ip, unsigned int port) {
    if (ip == NULL || port > 65535) return -1;

    int ret = 0;
    int sockfd = -1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -2;
    }

    struct sockaddr_in remote = {0};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port);
    remote.sin_addr.s_addr = inet_addr(ip);

    if (connect(sockfd, (struct sockaddr *)&remote, sizeof(struct sockaddr_in)) != 0) {
        perror("connect error");
        close(sockfd);
        return -3;
    }

    return sockfd;
}

int kvs_send_single_command(int sockfd, char * cmd) {
    if (sockfd < 0) return -1;

    const char * argv[] = {cmd};
    int msg_len = 0;
    char * send_msg = kvs_build_kvsp_frame(1, argv, &msg_len);
    if (send_msg == NULL || msg_len <= 0) {
        fprintf(stderr, "kvs_build_kvsp_frame error\n");
        return -2;
    }

    ssize_t n = send(sockfd, send_msg, msg_len, 0);
    if (n != msg_len) {
        fprintf(stderr, "kvs_send_single_command: n != msg_len\n");
        kvs_free(send_msg);
        return -2;
    }

    kvs_free(send_msg);

    return 0;
}

static ssize_t kvs_slave_obtain_file_size(int sockfd) {
    if (sockfd < 0) return -1;

    if (kvs_send_single_command(sockfd, KVS_SYNC_COMMAND) != 0) {
        return -2;
    }

    // obtain snapshot file size
    char recv_buf[BUFFER_SIZE] = {0};
    ssize_t n = recv(sockfd, recv_buf, BUFFER_SIZE, 0);

    if (n <= 0) {
        return -2;
    }

    if (strcmp(recv_buf, KVS_MASTER_FULLSYNC_BUSY) == 0) {
        return KVS_SYNC_RET_BUSY;
    }

    if (strcmp(recv_buf, KVS_MASTER_FULLSYNC_ERROR) == 0) {
        return KVS_SYNC_RET_ERROR;
    }

    size_t file_size = 0;

    if (sscanf(recv_buf, KVS_MASTER_FULLSYNC_OK, &file_size) != 1 || file_size > SSIZE_MAX) {
        return KVS_SYNC_RET_ERROR;
    }
    printf("msg: %s\n", recv_buf);
    return (ssize_t)file_size;

}

static char * kvs_slave_obtain_file(ssize_t file_size, int sockfd, ssize_t * mod_size) {
    if (file_size <= 0 || sockfd < 0 || mod_size == NULL) return NULL;

    char * buf = (char *)kvs_malloc(file_size + 1);
    if (buf == NULL) {
        perror("kvs_malloc");
        return NULL;
    }
    memset(buf, 0, file_size + 1);

#if ENABLE_RDMA

        int rdma_mod_size = rdma_server(global_config.rdma_port, buf, file_size, sockfd);
        if (rdma_mod_size <= 0) {
            kvs_free(buf);
            return NULL;
        }

        *mod_size = rdma_mod_size;

#elif ENABLE_SENDFILE

        char * send_msg= KVS_SLAVE_FULLSYNC_READY;
        send(sockfd, send_msg, strlen(send_msg), 0);

        size_t received = 0;
        while (received < file_size) {
            int n = recv(sockfd, buf + received, file_size - received, 0);

            if (n <= 0) {
                break;
            }
            
            received += n;
        }

        if (received < file_size) {
            kvs_free(buf);
            return NULL;
        }

        *mod_size = received;

#endif


    return buf;

}

int kvs_slave_full_sync(int sockfd) {
    if (sockfd < 0) return -1;

    int ret = 0;
    char * buf = NULL;

    // 已有数据同步:
    ssize_t file_size = kvs_slave_obtain_file_size(sockfd);

    if (file_size == KVS_SYNC_RET_BUSY) {
        fprintf(stderr, "master is busy\n");
        return KVS_SYNC_RET_BUSY;
    }
    
    if (file_size == KVS_SYNC_RET_ERROR) {
        fprintf(stderr, "full sync error\n");
        return KVS_SYNC_RET_ERROR;
    }

    if (file_size == 0) {
        return 0;
    }

    if (file_size < 0) {
        ret = -2;
        goto cleanup;     
    }
    
    ssize_t buf_len = 0;
    buf = kvs_slave_obtain_file(file_size, sockfd, &buf_len);

    if (buf_len <= 0 || buf == NULL) {
        fprintf(stderr, "slave obtain file_data failed\n");
        ret = -3;
        goto cleanup;
    }
    
    if (kvs_file_read(buf, buf_len, kvs_save_handler) != 0) {
        fprintf(stderr, "kvs_file_read failed\n");
        ret = -3;
        goto cleanup;
    }


    cleanup:
        if (ret != 0) kvs_send_single_command(sockfd, KVS_SLAVE_FULLSYNC_ERROR);
        kvs_free(buf);

    return ret;
    
}

int kvs_slave_incr_sync(int sockfd) {
    if (sockfd < 0) return -1;

    int recv_fd;

#if ENABLE_EBPF

    close(sockfd);

    int agent_fd = kvs_connect_to_remote(global_config.agent_ip, global_config.agent_port);
    if (agent_fd < 0) {
        fprintf(stderr, "Error: agent_fd = %d\n", agent_fd);
        return -1;
    }

    recv_fd = agent_fd;

#elif ENABLE_SEND

    recv_fd = sockfd;

#endif


#if (NETWORK_SELECT == NETWORK_NTYCO)
    client_info * cli_info = client_info_init(recv_fd);
    cli_info->role = 1; // slave;
    nty_coroutine * read_co = NULL;
    nty_coroutine_create(&read_co, server_reader, cli_info);
#endif 

    return 0;
}

int kvs_slave_sync(){
    if (global_config.enable_sync == 0) return 0;

    int master_fd =  kvs_connect_to_remote(global_config.master_ip, global_config.master_port);
    if (master_fd < 0) {
        fprintf(stderr, "Error: master_fd = %d\n", master_fd);
        return -1;
    }

    int ret = 0;

    ret = kvs_slave_full_sync(master_fd);
    if (ret != 0 && ret != KVS_SYNC_RET_BUSY) {
        close(master_fd);
        return -2;
    }

    ret = kvs_send_single_command(master_fd, KVS_SLAVE_FULLSYNC_FINISHED);
    if (ret != 0) {
        close(master_fd);
        return -2;
    }

    if (kvs_slave_incr_sync(master_fd) != 0) {
        return -2;
    }

    return 0;

}

static int kvs_sync_write_snapshot(int fd) {
    if (fd < 0) return -1;

    struct io_uring ring = {0};

    if (io_uring_queue_init(ENTRY_LENGTH, &ring, 0) < 0) {
        perror("io_uring_queue_init");
        return -2;
    } 
    
    io_write_ctx main_ctx = {0};
    main_ctx.ring = &ring;
    if (kvs_traversal_write(fd, &main_ctx) < 0) {
        io_uring_queue_exit(&ring);
        return -2;
    }

    io_uring_queue_exit(&ring);

    return 0;

}

static int kvs_master_write_snapshot() {

    int fd = open("./kvs-data/kvs_snapshot.tmp", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if( fd < 0) {
        perror("open");
        return -1;
    }

    int notify_pair[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, notify_pair) != 0) {
        perror("socketpair");
        close(fd);
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        close(notify_pair[0]);
        close(notify_pair[1]);
        close(fd);
        return -1;
    }

    if (pid == 0) {
        close(notify_pair[0]);
        int ret = kvs_sync_write_snapshot(fd);

        char result = (ret == 0) ? '0':'1';
        syscall(SYS_write, notify_pair[1], &result, 1);
        close(notify_pair[1]);

        _exit(ret == 0 ? 0 : 1);
    }

    close(notify_pair[1]);

    char result = '1';
    ssize_t n = recv(notify_pair[0], &result, 1, 0);

    int status = 0;
    pid_t wret = waitpid(pid, &status, 0);
    if (wret != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        close(notify_pair[0]);
        close(fd);
        return -1;
    }

    if (n != 1 || result != '0') {
        fprintf(stderr, "kvs_master_write_snapshot error\n");
        close(notify_pair[0]);
        close(fd);
        return -1;
    }

    close(notify_pair[0]);  

    return fd;

}

static int kvs_master_send_file(int fd, size_t file_size, client_info * cli) {
    if (file_size == 0) return 0;
    if (fd < 0 || cli == NULL) return -1;

#if ENABLE_RDMA

    char * ptr = (char *)mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ptr == MAP_FAILED){
        perror("mmap");
        return -2;
    }

    struct sockaddr_in peer_addr = {0};
    socklen_t peer_len = sizeof(peer_addr);
    getpeername(cli->fd, (struct sockaddr *)&peer_addr, &peer_len);
    
    char slave_ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &peer_addr.sin_addr, slave_ip, INET_ADDRSTRLEN);

    if (rdma_client(slave_ip, global_config.rdma_port, ptr, file_size) != 0) {
        fprintf(stderr, "rdma_client error\n");
        munmap(ptr, file_size);
        return -2;
    }

    munmap(ptr, file_size);

#elif ENABLE_SENDFILE

    off_t start_pos = 0;
    size_t remaining = file_size;
    ssize_t bytes = 0;

    while (remaining > 0) {
        bytes = sendfile(cli->fd, fd, &start_pos, remaining);
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { 
                continue;
            }

            perror("sendfile");
            break;
        }
        else if (bytes == 0) {
            break;
        }

        remaining -= bytes;
    }  
    
    if (remaining != 0) {
        fprintf(stderr, "full sync imcomplete");
        return -2;
    }

#endif

    return 0;

}

int kvs_master_notify_and_wait_slave(client_info * cli, size_t file_size) {
    if (cli == NULL) return -1;

    char send_msg[BUFFER_SIZE] = {0};
    int msg_len = snprintf(send_msg, BUFFER_SIZE, KVS_MASTER_FULLSYNC_OK, file_size); // send file size
    ssize_t n = send(cli->fd, send_msg, msg_len, 0);
    if (n != msg_len) {
        return -2;
    }    

    if (file_size == 0) return 0;

    char recv_buf[BUFFER_SIZE] = {0};
    n = recv(cli->fd, recv_buf, BUFFER_SIZE, 0); // recv slave info

    char * expected_reply = KVS_SLAVE_FULLSYNC_READY;
    size_t reply_len = strlen(expected_reply);
    if (n < reply_len || strncmp(recv_buf, expected_reply, reply_len) != 0) {
        return -2;
    }

    return 0;

}

int kvs_master_full_sync(client_info * cli) {
    if(global_config.enable_sync == 0) return 0;

    if(cli == NULL) return -1;

    if (is_full_sync != 0) {
        send(cli->fd, KVS_MASTER_FULLSYNC_BUSY, strlen(KVS_MASTER_FULLSYNC_BUSY), 0);
        return 0;
    }
        
    is_full_sync = 1;
    
    int fd = -1;
    int ret = 0;
    int is_fullsync_ok_sent = 0;

    fd = kvs_master_write_snapshot();
    if (fd < 0) {
        ret = -2;
        goto cleanup;
    }

    struct stat statbuf = {0};
    if (fstat(fd, &statbuf) < 0) {
        perror("fstat");
        ret = -2;
        goto cleanup;
    } 

    if (kvs_master_notify_and_wait_slave(cli, statbuf.st_size) != 0) {
        fprintf(stderr, "kvs_master_notify_and_wait_slave error\n");
        ret = -2;
        goto cleanup;
    }
    is_fullsync_ok_sent = 1;

    if (kvs_master_send_file(fd, statbuf.st_size, cli) != 0) {
        fprintf(stderr, "kvs_master_send_file error\n");
        ret = -2;
        goto cleanup;
    }

    cleanup:
        if (fd >= 0) close(fd);
        if (ret != 0 && is_fullsync_ok_sent == 0) kvs_send_single_command(cli->fd, KVS_MASTER_FULLSYNC_ERROR);
        is_full_sync = 0;

    return ret;

}

int kvs_master_incr_sync(client_info * cli, char ** tokens, int count) {
    if(global_config.enable_sync == 0) return 0;

    if (cli == NULL || tokens == NULL || count <= 0) return -1;

#if ENABLE_SNED



#endif

    return 0;
}