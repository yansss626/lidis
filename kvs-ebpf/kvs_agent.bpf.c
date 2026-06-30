#include "vmlinux.h"
#include "kvs_agent.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

#define TASK_COMM_LEN 16

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024 * 1024);
} rb SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10 * 1024);
    __type(key, __u64);
    __type(value, __u64);

} h_map SEC(".maps");

SEC("fentry/tcp_recvmsg")
int BPF_PROG(tcp_recvmsg, struct sock *sk, struct msghdr *msg, size_t len, int flags, int *addr_len) {

    char comm[TASK_COMM_LEN] = {0};
    bpf_get_current_comm(comm, TASK_COMM_LEN);
    if (comm[0] != 'k' || comm[1] != 'v' || comm[2] != 's' || comm[3] != 't' || comm[4] != 'o'
         || comm[5] != 'r' || comm[6] != 'e') return 0;

    __u64 pid = bpf_get_current_pid_tgid() ;
    __u64 ubuf = 0;
    struct iov_iter iter = {0};

    bpf_probe_read_kernel(&iter, sizeof(iter), &msg->msg_iter);

    if (iter.iter_type != ITER_UBUF) return 0;

    ubuf = (__u64)iter.ubuf + iter.iov_offset;
    

    if (ubuf != 0) bpf_map_update_elem(&h_map, &pid, &ubuf, BPF_ANY);

    return 0;
}

SEC("fexit/tcp_recvmsg")
int BPF_PROG(tcp_recvmsg_exit, struct sock *sk, struct msghdr *msg, size_t len, int flags, int *addr_len, int ret) {
    
    __u64 pid = bpf_get_current_pid_tgid();
    __u64 * ubuf_ptr = NULL;
    struct tcp_event * e = NULL;
    int payload_len = ret;     

    ubuf_ptr = bpf_map_lookup_elem(&h_map, &pid);
    if (ubuf_ptr == NULL) return 0;
    
    bpf_map_delete_elem(&h_map, &pid);

    if (ret <= 0) return 0;
    
    e = bpf_ringbuf_reserve(&rb, sizeof(struct tcp_event), 0);
    if (e == NULL) return 0;
    

    if (payload_len >= BUFFER_SIZE) {
        payload_len = BUFFER_SIZE;
    } else {
        payload_len &= (BUFFER_SIZE - 1);
    }

    bpf_probe_read_user(e->payload, payload_len, (void *)(*ubuf_ptr));
    e->payload_len = payload_len;
    e->ret = ret;
    
    bpf_ringbuf_submit(e, 0);

    return 0;
}


