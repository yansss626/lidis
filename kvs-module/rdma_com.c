#include <stdio.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>
#include <endian.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

struct pdata{ // private data
    uint64_t buf_size; // remote buffer size 
    uint64_t raddr; // remote addr 
    uint32_t rkey; // remote key
};

#define KVS_SLAVE_FULLSYNC_READY       "FULL SYNC READY"

#define MSG_LENGTH 32

int rdma_client(const char * server_ip, const char * port, char * ptr, size_t size){
    if(server_ip == NULL || port == NULL || ptr == NULL || size <= 0) return -1;

    int ret = 0;
    int ibv_ret = 0;
    struct rdma_addrinfo hints = {0};
    struct rdma_addrinfo * res = NULL;
    struct rdma_cm_id * id = NULL;
    struct ibv_qp_init_attr qp_attr = {0};
    struct ibv_mr * mr = NULL;
    struct ibv_mr * send_mr = NULL;
    struct pdata server_data = {0};
    struct ibv_wc wc = {0};
    char msg[MSG_LENGTH] = {0};
    snprintf(msg, MSG_LENGTH, "%zu", size);

    hints.ai_port_space = RDMA_PS_TCP;
    if(rdma_getaddrinfo(server_ip, port, &hints, &res) != 0){
        perror("rdma_getaddrinfo"); 
        ret = -2;;
        goto cleanup;
    }

    qp_attr.cap.max_send_wr = 2;
    qp_attr.cap.max_recv_wr = 2;
    qp_attr.cap.max_recv_sge = 1;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.sq_sig_all = 1;
    qp_attr.qp_type = IBV_QPT_RC;

    if(rdma_create_ep(&id, res, NULL, &qp_attr) != 0){
        perror("rdma_create_ep"); 
        ret = -2;;
        goto cleanup;        
    }
    
    mr = ibv_reg_mr(id->pd, ptr, size, 0);
    if(mr == NULL){
        perror("ibv_reg_mr");
        ret = -2;
        goto cleanup;
    }

    send_mr = ibv_reg_mr(id->pd, msg, MSG_LENGTH, 0);
    if(send_mr == NULL){
        perror("ibv_reg_mr");
        ret = -2;
        goto cleanup;
    }


    int connected = 0;  // 
    if(rdma_connect(id, NULL) != 0){
        perror("rdma_connect"); 
        ret = -2;;
        goto cleanup;           
    }
    connected = 1; // connect success
    if(id->event == NULL || id->event->param.conn.private_data_len < sizeof(struct pdata)){
        fprintf(stderr, "There is no private data from server\n");
        ret = -3;
        goto cleanup;
    }

    memcpy(&server_data, id->event->param.conn.private_data, sizeof(struct pdata));
    uint64_t remote_buf_size = be64toh(server_data.buf_size);
    if(remote_buf_size < size){
        fprintf(stderr, "Server buffer size is not enough\n");
        
        snprintf(msg, MSG_LENGTH, "buffer size is not enough\n");
        if(rdma_post_send(id, NULL, msg, MSG_LENGTH, send_mr, 0) != 0){
            perror("rdma_post_write"); 
            ret = -2;;
            goto cleanup;         
        }
        while((ibv_ret = ibv_poll_cq(id->send_cq, 1, &wc)) == 0){
            if(ibv_ret < 0 || wc.status != IBV_WC_SUCCESS){
                fprintf(stderr, "rdma_post_write failed\n");
                ret = -3;
                goto cleanup;
            }
        }
        ret = -3;
        goto cleanup;
    }
    //printf("rdma_buf_size: %d\n", remote_buf_size);

    uint64_t remote_addr = be64toh(server_data.raddr);
    uint32_t remote_rkey = ntohl(server_data.rkey);

    if(rdma_post_write(id, NULL, ptr, size, mr, 0, remote_addr, remote_rkey) != 0){
        perror("rdma_post_write"); 
        ret = -2;;
        goto cleanup;         
    }
    while((ibv_ret = ibv_poll_cq(id->send_cq, 1, &wc)) == 0){
        if(ibv_ret < 0 || wc.status != IBV_WC_SUCCESS){
            fprintf(stderr, "rdma_post_write failed\n");
            ret = -3;
            goto cleanup;
        }
    }

    if(rdma_post_send(id, NULL, msg, MSG_LENGTH, send_mr, 0) != 0){
        perror("rdma_post_write"); 
        ret = -2;;
        goto cleanup;         
    }
    while((ibv_ret = ibv_poll_cq(id->send_cq, 1, &wc)) == 0){
        if(ibv_ret < 0 || wc.status != IBV_WC_SUCCESS){
            fprintf(stderr, "rdma_post_write failed\n");
            ret = -3;
            goto cleanup;
        }
    }


    cleanup:

        if(res != NULL)rdma_freeaddrinfo(res);
        if(connected) rdma_disconnect(id);  
        if(send_mr != NULL) ibv_dereg_mr(send_mr);
        if(mr != NULL) ibv_dereg_mr(mr);
        if(id != NULL) rdma_destroy_ep(id);
        return ret;
}



ssize_t rdma_server(const char * port, char * rdma_buf, size_t size, int sockfd){
    if(port == NULL || rdma_buf == NULL || size <= 0) return -1;

    ssize_t ret = 0;
    struct rdma_addrinfo hints = {0};
    struct rdma_addrinfo * res = NULL;
    struct rdma_cm_id * id = NULL;
    struct rdma_cm_id * listen_id = NULL;
    struct ibv_qp_init_attr qp_attr = {0};
    struct ibv_mr * mr = NULL;
    struct ibv_mr * recv_mr = NULL;
    char notify_msg[MSG_LENGTH] = {0};
    struct pdata server_data = {0};
    struct rdma_conn_param conn_parm = {0};
    struct ibv_wc wc = {0};

    hints.ai_port_space = RDMA_PS_TCP;
    hints.ai_flags = RAI_PASSIVE;
    if(rdma_getaddrinfo(NULL, port, &hints, &res) != 0){
        perror("rdma_getaddrinfo"); 
        ret = -2;;
        goto cleanup;
    }
    
    qp_attr.cap.max_recv_wr = 2;
    qp_attr.cap.max_send_wr = 2;
    qp_attr.cap.max_recv_sge = 1;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.sq_sig_all = 1;
    qp_attr.qp_type = IBV_QPT_RC;
    if(rdma_create_ep(&listen_id, res, NULL, &qp_attr) != 0){
        perror("rdma_create_ep"); 
        ret = -2;;
        goto cleanup;        
    }
   
    if(rdma_listen(listen_id, 1) != 0){
        perror("rdma_listen");
        ret = -2;
        goto cleanup;
    }

    char * send_msg= KVS_SLAVE_FULLSYNC_READY;  // notify master rdma server is ready
    send(sockfd, send_msg, strlen(send_msg), 0);

    int get_request = 0;
    if(rdma_get_request(listen_id, &id) != 0){
        perror("rdma_get_request");
        ret = -2;
        goto cleanup;
    }
    get_request = 1;
    mr = ibv_reg_mr(id->pd, rdma_buf, size, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    if(mr == NULL){
        perror("ibv_reg_mr");
        ret = -2;
        goto cleanup;
    }
    recv_mr = ibv_reg_mr(id->pd, notify_msg, MSG_LENGTH, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    if(recv_mr == NULL){
        perror("ibv_reg_mr");
        ret = -2;
        goto cleanup;
    }    
    
    if(rdma_post_recv(id, NULL, notify_msg, MSG_LENGTH, recv_mr) != 0){
        perror("rdma_post_recv");
        ret = -2;
        goto cleanup;
    }

    
    server_data.buf_size = htonl((uint64_t)size);
    server_data.raddr = htobe64((uintptr_t)rdma_buf);
    server_data.rkey = htonl(mr->rkey);
    conn_parm.private_data = &server_data;
    conn_parm.private_data_len = sizeof(struct pdata);
    conn_parm.responder_resources = 1;
    conn_parm.initiator_depth = 1;
   
    if(rdma_accept(id, &conn_parm) != 0){
        perror("rdma_accept");
        ret = -2;
        goto cleanup;
    }
    
    int ibv_ret = 0;
    while((ibv_ret = ibv_poll_cq(id->recv_cq, 1, &wc)) == 0){
        if(ibv_ret < 0 || wc.status != IBV_WC_SUCCESS){
            fprintf(stderr, "rdma_post_write failed\n");
            ret = -3;
            goto cleanup;
        }
    }
    //printf("notify_msg: %s\n", notify_msg);
    //printf("rdma_buf: %s\n", rdma_buf);

    ssize_t file_size = 0;
    size_t pos = 0;
    size_t msg_len = strnlen(notify_msg, MSG_LENGTH);
    while (pos < msg_len && notify_msg[pos] >= '0' && notify_msg[pos] <= '9') {
        int digit = notify_msg[pos] - '0';

        if (file_size > (SSIZE_MAX - digit) / 10) break;
        file_size = file_size * 10 + digit;
        pos++;
    }
    
    ret = file_size;
    if(ret <= 0 || pos != msg_len){
        ret = -4;
        goto cleanup;
    }

    cleanup:        
        if(res != NULL)rdma_freeaddrinfo(res);
        if(get_request) rdma_disconnect(id);  
        if(recv_mr != NULL) ibv_dereg_mr(recv_mr);
        if(mr != NULL) ibv_dereg_mr(mr);
        if(id != NULL) rdma_destroy_ep(id);
        if(listen_id != NULL)rdma_destroy_ep(listen_id);
        return ret;
}