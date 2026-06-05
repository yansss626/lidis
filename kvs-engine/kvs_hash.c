#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#define XXH_INLINE_ALL 
#include "xxhash.h"
#include "kvstore.h"


// Key, Value --> 
// Modify 

#if ENABLE_HASH

kvs_hash_t global_hash;


//Connection 
// 'C' + 'o' + 'n'

// 
static inline uint32_t rotl32(uint32_t x, int8_t r) {
    return (x << r) | (x >> (32 - r));
}

// MurmurHash3 
static uint32_t MurmurHash3_x86_32(const void *key, int len, uint32_t seed) {
    const uint8_t *data = (const uint8_t*)key;
    const int nblocks = len / 4;

    uint32_t h1 = seed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    // 1. 
    const uint32_t *blocks = (const uint32_t *)(data + nblocks * 4);
    for(int i = -nblocks; i; i++) {
        uint32_t k1 = blocks[i];

        k1 *= c1;
        k1 = rotl32(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = rotl32(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }

    // 2. 
    const uint8_t *tail = (const uint8_t*)(data + nblocks * 4);
    uint32_t k1 = 0;

    switch(len & 3) {
    case 3: k1 ^= tail[2] << 16;
    case 2: k1 ^= tail[1] << 8;
    case 1: k1 ^= tail[0];
            k1 *= c1; 
            k1 = rotl32(k1, 15); 
            k1 *= c2; 
            h1 ^= k1;
    };

    // 3. fmix
    h1 ^= len;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}

static int _hash(char *key, int size) {

	if (!key) return -1;


    // uint32_t seed = 0x9747b28c; 
    
    // // MurmurHash3
    // uint32_t hash_value = MurmurHash3_x86_32(key, strlen(key), seed);
    // return hash_value % size;	

	// xxhash
	uint64_t hash_value = XXH3_64bits(key, strlen(key));

    // 
	return (int)(hash_value % (uint64_t)size);

	// // FNV-1a
	// uint32_t hash = 2166136261U;
	// int i = 0;

	// while (key[i] != 0) {
	// 	hash ^= (uint8_t)key[i];
	// 	hash *= 16777619U;
	// 	i++;
	// }

	// return (int)(hash % size);

}

hashnode_t *_create_node(char *key, char *value) {

	hashnode_t *node = (hashnode_t*)kvs_malloc(sizeof(hashnode_t));
	if (!node) return NULL;
	
#if ENABLE_KEY_POINTER
	char *kcopy = kvs_malloc(strlen(key) + 1);
	if (kcopy == NULL) {
		kvs_free(node);
		return NULL;
	}
	memset(kcopy, 0, strlen(key) + 1);
	strncpy(kcopy, key, strlen(key));

	node->key = kcopy;

	char *kvalue = kvs_malloc(strlen(value) + 1);
	if (kvalue == NULL) { 
		kvs_free(node);
		kvs_free(kcopy);
		return NULL;
	}
	memset(kvalue, 0, strlen(value) + 1);
	strncpy(kvalue, value, strlen(value));

	node->value = kvalue;
	
#else
	strncpy(node->key, key, MAX_KEY_LEN);
	strncpy(node->value, value, MAX_VALUE_LEN);
#endif
	node->next = NULL;

	return node;
}


//
int kvs_hash_create(kvs_hash_t *hash) {

	if (!hash) return -1;

	hash->nodes = (hashnode_t**)kvs_malloc(sizeof(hashnode_t*) * MAX_TABLE_SIZE);
	if (!hash->nodes) return -1;

	hash->max_slots = MAX_TABLE_SIZE;
	hash->count = 0; 

	return 0;
}

// 
void kvs_hash_destory(kvs_hash_t *hash) {

	if (!hash) return;

	int i = 0;
	for (i = 0;i < hash->max_slots;i ++) {
		hashnode_t *node = hash->nodes[i];

		while (node != NULL) { // error

			hashnode_t *tmp = node;
			node = node->next;
			hash->nodes[i] = node;
			kvs_free(tmp->key);
			kvs_free(tmp->value);
			kvs_free(tmp);
			
		}
	}

	kvs_free(hash->nodes);
	hash->nodes = NULL;
	
}

// 5 + 2

// mp
int count = 0;
int kvs_hash_set(kvs_hash_t *hash, char *key, char *value) {

	if (!hash || !key || !value) return -1;

	int idx = _hash(key, MAX_TABLE_SIZE);

	hashnode_t *node = hash->nodes[idx];
#if 1
	while (node != NULL) {
		if (strcmp(node->key, key) == 0) { // exist
			return 1;
		}
		node = node->next;
	}
#endif

	hashnode_t *new_node = _create_node(key, value);
	if(!new_node) return -2;
	new_node->next = hash->nodes[idx];
	hash->nodes[idx] = new_node;
	
	hash->count ++;

	return 0;
}


char * kvs_hash_get(kvs_hash_t *hash, char *key) {

	if (!hash || !key) return NULL;

	int idx = _hash(key, MAX_TABLE_SIZE);

	hashnode_t *node = hash->nodes[idx];

	while (node != NULL) {

		if (strcmp(node->key, key) == 0) {
			return node->value;
		}

		node = node->next;
	}

	return NULL;

}


int kvs_hash_mod(kvs_hash_t *hash, char *key, char *value) {

	if (!hash || !key) return -1;

	int idx = _hash(key, MAX_TABLE_SIZE);

	hashnode_t *node = hash->nodes[idx];

	while (node != NULL) {

		if (strcmp(node->key, key) == 0) {
			break;
		}

		node = node->next;
	}

	if (node == NULL) {
		return 1;
	}



	char *kvalue = kvs_malloc(strlen(value) + 1);
	if (kvalue == NULL) return -2;
		// node --> 
	kvs_free(node->value);

	memset(kvalue, 0, strlen(value) + 1);
	strncpy(kvalue, value, strlen(value));

	node->value = kvalue;
	
	return 0;
}

int kvs_hash_count(kvs_hash_t *hash) {
	return hash->count;
}

int kvs_hash_del(kvs_hash_t *hash, char *key) {
	if (!hash || !key) return -2;

	int idx = _hash(key, MAX_TABLE_SIZE);

	hashnode_t *head = hash->nodes[idx];
	if (head == NULL) return -1; // noexist
	// head node
	if (strcmp(head->key, key) == 0) {
		hashnode_t *tmp = head->next;
		hash->nodes[idx] = tmp;
		
		kvs_free(head->key);
		kvs_free(head->value);
		kvs_free(head);
		hash->count --;
		
		return 0;
	}

	hashnode_t *cur = head;
	while (cur->next != NULL) {
		if (strcmp(cur->next->key, key) == 0) break; // search node
		
		cur = cur->next;
	}

	if (cur->next == NULL) {
		
		return -1;
	}

	hashnode_t *tmp = cur->next;
	cur->next = tmp->next;
#if ENABLE_KEY_POINTER
	kvs_free(tmp->key);
	kvs_free(tmp->value);
#endif
	kvs_free(tmp);
	
	hash->count --;

	return 0;
}


int kvs_hash_exist(kvs_hash_t *hash, char *key) {

	char *value = kvs_hash_get(hash, key);
	if (!value) return 1;

	return 0;
	
}

#if 0
int main() {

	kvs_hash_create(&hash);

	kvs_hash_set(&hash, "Teacher1", "King");
	kvs_hash_set(&hash, "Teacher2", "Darren");
	kvs_hash_set(&hash, "Teacher3", "Mark");
	kvs_hash_set(&hash, "Teacher4", "Vico");
	kvs_hash_set(&hash, "Teacher5", "Nick");

	char *value1 = kvs_hash_get(&hash, "Teacher1");
	printf("Teacher1 : %s\n", value1);

	int ret = kvs_hash_mod(&hash, "Teacher1", "King1");
	printf("mode Teacher1 ret : %d\n", ret);
	
	char *value2 = kvs_hash_get(&hash, "Teacher1");
	printf("Teacher2 : %s\n", value1);

	ret = kvs_hash_del(&hash, "Teacher1");
	printf("delete Teacher1 ret : %d\n", ret);

	ret = kvs_hash_exist(&hash, "Teacher1");
	printf("Exist Teacher1 ret : %d\n", ret);

	kvs_hash_destory(&hash);

	return 0;
}

#endif


#endif