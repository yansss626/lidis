#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include "kvs_agent.h"

int kvs_slaves_create(kvs_slaves * inst, size_t size){
    if(inst == NULL) return -1;
    if(inst->table != NULL) return 0;

    inst->table = (kvs_slave_item *)malloc(sizeof(kvs_slave_item) * size);
    if(inst->table == NULL) {
        perror("malloc");
        return -2;
    }
    memset(inst->table, -1, sizeof(kvs_slave_item) * size);

    inst->total = 0;
    inst->size = size;

    return 0;
}

int kvs_slaves_insert(kvs_slaves * inst, int fd){
    if(inst == NULL || inst->table == NULL) return -1;
    if(inst->total == inst->size) return -2;

    int i = 0;
    for(; i < inst->size; i++){
        if(inst->table[i].fd < 0) {
            inst->table[i].fd = fd;
            ++(inst->total);
            break;
        }
    }
    
    return 0;
}

int kvs_slaves_delete(kvs_slaves * inst, int fd){
    if(inst == NULL || inst->table == NULL) return -1;
    if(inst->total == 0) return 0;

    int i = 0;
    for(; i < inst->size; i++){
        if(inst->table[i].fd == fd) {
            inst->table[i].fd = -1;
            --(inst->total);
            break;
        }
    }

    return 0;
}

int kvs_slaves_destroy(kvs_slaves * inst){
    if(inst == NULL) return -1;
    
    if (inst->table != NULL) {
		for (int i = 0; i < inst->size; i++) {
			int fd = inst->table[i].fd;
			if (fd > 0) close(fd);
		}
	    free(inst->table);
	}

    inst->table = NULL;
    inst->total = 0;

    return 0;
}

int kvs_agent_cache_create(agent_cache * ac, size_t cache_size) {
    if (ac == NULL || cache_size == 0) return -1;

    ac->cache = (char *)malloc(cache_size);
    if (ac->cache == NULL) {
        perror("malloc");
        return -2;
    }

    ac->capacity = cache_size;
    ac->read_pos = 0;
    ac->write_pos = 0;
    ac->used = 0;

    return 0;
}

void kvs_agent_cache_destroy(agent_cache * ac) {
    if (ac == NULL || ac->cache == NULL) return;

    free(ac->cache);
    ac->cache = NULL;
}

static int cache_write_bytes(agent_cache * ac, const char * buf, size_t buf_len) {
    if (ac == NULL || ac->cache == NULL || buf == NULL || buf_len == 0) return - 1;

    if (ac->capacity - ac->used < buf_len) return - 2;

    if (ac->capacity - ac->write_pos >= buf_len) {
        memcpy(ac->cache + ac->write_pos, buf, buf_len);
        ac->write_pos = (ac->write_pos + buf_len) % ac->capacity;
    }
    else {
        memcpy(ac->cache + ac->write_pos, buf, ac->capacity - ac->write_pos);
        size_t last_copy_len = ac->capacity - ac->write_pos;
        ac->write_pos = (ac->write_pos + last_copy_len) % ac->capacity;

        memcpy(ac->cache + ac->write_pos, buf + last_copy_len, buf_len - last_copy_len);
        last_copy_len = buf_len - last_copy_len;
        ac->write_pos = (ac->write_pos + last_copy_len) % ac->capacity;
    }

    ac->used += buf_len;
    
    return 0;
}

// [buf_len][buf]
int kvs_agent_cache_write(agent_cache * ac, const char * buf, size_t buf_len) {
    if (ac == NULL || ac->cache == NULL || buf == NULL || buf_len == 0) return -1;

    if (buf_len > SIZE_MAX - sizeof(size_t)) return -2;

    size_t req_len = sizeof(size_t) + buf_len; // required length
    if (ac->capacity - ac->used < req_len) return -2;

    int ret = cache_write_bytes(ac, (const char *)&buf_len, sizeof(buf_len));
    if (ret != 0) return -3;

    ret = cache_write_bytes(ac, buf, buf_len);
    if (ret != 0) return -3;

    return 0;
}

static int cache_read_bytes(agent_cache * ac, char * buf, size_t buf_len) {
    if (ac == NULL || ac->cache == NULL || buf == NULL || buf_len == 0) return - 1;

    if (ac->used < buf_len) return -2;

    if (ac->capacity - ac->read_pos >= buf_len) {
        memcpy(buf, ac->cache + ac->read_pos, buf_len);
        ac->read_pos = (ac->read_pos + buf_len) % ac->capacity;
    }
    else {
        memcpy(buf, ac->cache + ac->read_pos, ac->capacity - ac->read_pos);
        size_t last_copy_len = ac->capacity - ac->read_pos;
        ac->read_pos = (ac->read_pos + last_copy_len) % ac->capacity;

        memcpy(buf + last_copy_len, ac->cache + ac->read_pos, buf_len - last_copy_len);
        last_copy_len = buf_len - last_copy_len;
        ac->read_pos = (ac->read_pos + last_copy_len) % ac->capacity;
    }

    ac->used -= buf_len;

    return 0;
}

int cache_peek_len(agent_cache * ac, char * buf) {
    if (ac == NULL || ac->cache == NULL || buf == NULL) return -1;

    size_t buf_len = sizeof(size_t);

    if (ac->used < buf_len) return -2;

    if (ac->capacity - ac->read_pos >= buf_len) {
        memcpy(buf, ac->cache + ac->read_pos, buf_len);
    }
    else {
        size_t tmp_read_pos = ac->read_pos;

        memcpy(buf, ac->cache + tmp_read_pos, ac->capacity - tmp_read_pos);
        size_t last_copy_len = ac->capacity - tmp_read_pos;
        tmp_read_pos = (tmp_read_pos + last_copy_len) % ac->capacity;

        memcpy(buf + last_copy_len, ac->cache + tmp_read_pos, buf_len - last_copy_len);
    }

    return 0;
}

char * kvs_agent_cache_read(agent_cache * ac, size_t * buf_len) {
    if (ac == NULL || ac->cache == NULL || buf_len == NULL) return NULL;

    if (ac->used < sizeof(size_t)) {
        return NULL;
    }
    // get buffer length
    if (cache_peek_len(ac, (char *)buf_len) != 0) {
        return NULL;
    }
    size_t len = *buf_len;

    if (len > ac->used - sizeof(size_t) || buf_len == 0) return NULL;

    // get buffer
    char * buf = (char *)malloc(len);
    if (buf == NULL) {
        perror("malloc");
        return NULL;
    }

    ac->read_pos = (ac->read_pos + sizeof(size_t)) % ac->capacity;
    ac->used -= sizeof(size_t);

    if (cache_read_bytes(ac, buf, len) != 0) {
        free(buf);
        return NULL;
    }

    return buf;
}