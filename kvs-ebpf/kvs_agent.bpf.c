// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* Copyright (c) 2020 Facebook */
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "kvs_agent.h"

char LICENSE[] SEC("license") = "Dual BSD/GPL";

struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 1024 * 1024);
} rb SEC(".maps");

SEC("uprobe")
int BPF_KPROBE(trace_kvs_incr_sync, client_info * cli)
{
	if(!cli) 
		return 0;

	struct event *e = NULL;
	int total_length = 0;
	char * rbuf = NULL;
	/* reserve sample from BPF ringbuf */
	e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
	if (!e)
		return 0;	
	
	bpf_probe_read_user(&(total_length), sizeof(total_length), &(cli->cmd_tl));

	if (total_length >= KVS_MAX_COMMAND_LENGTH || total_length <= 0) {	
		bpf_ringbuf_discard(e, 0);
		return 0;
	}
	e->total_length = total_length;

	bpf_probe_read_user(&rbuf, sizeof(rbuf), &(cli->rbuf));

	if (!rbuf) {	
		bpf_ringbuf_discard(e, 0);
		return 0;
	}
	
	bpf_probe_read_user(e->command, sizeof(e->command), rbuf);



	/* successfully submit it to user-space for post-processing */
	bpf_ringbuf_submit(e, 0);
	
	return 0;
}