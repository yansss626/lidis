#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include "kvstore.h"


#if MEM_POOL


kvs_mempool_t pools = {0};

#define LIST_INSERT(list, item) do{ \
    if (list != NULL) list->prev = item; \
    item->prev = NULL; item->next = list; \
    list = item; \
}while(0)

#define LIST_REMOVE(list, item) do{ \
    if (item->next != NULL) (item->next)->prev = item->prev; \
    if (item->prev != NULL) (item->prev)->next = item->next; \
    if (item == list) list = list->next; \
    item->prev = NULL; item->next = NULL; \
}while(0)


int mp_create(kvs_mempool_t * pools) {
    if (pools == NULL) return -1;

    memset(pools, 0, sizeof(kvs_mempool_t));

    for (int i = 0; i < CLASS_COUNT; i++) {
        mem_pool_t * pool = &(pools->classes[i]);
        pool->block_size = size_classes[i];
    }

    return 0;
}

void mp_free_slab(slab_t * slab, int classid) {

    while (slab != NULL) {
        slab_t * tmp = slab->next;
        munmap(slab->mem, slab_classes[classid]);
        free(slab);
        slab = tmp;
    }
}

void mp_destroy(kvs_mempool_t * pools) {
    if (pools == NULL) return;

    for (int i = 0; i < CLASS_COUNT; i++) {
        mem_pool_t * pool = &(pools->classes[i]);

        mp_free_slab(pool->empty_slabs, i);
        pool->empty_slabs = NULL;

        mp_free_slab(pool->full_slabs, i);
        pool->full_slabs = NULL;

        mp_free_slab(pool->partial_slabs, i);
        pool->partial_slabs = NULL;  

        pool->empty_slab_count = 0;
    }
    return;
}

int get_class_id (size_t size) {


    for (int i = 0; i < CLASS_COUNT; i++) {
        if (size <= size_classes[i]) {
            return i;
        }
    }

    return -1;

}

int add_pool (mem_pool_t * pool, int classid) {
    if (pool == NULL) return -1;

    slab_t * slab = (slab_t *)malloc(sizeof(slab_t));
    if (slab == NULL) return -2;
    memset(slab, 0, sizeof(slab_t));
    slab->mem = (char *)mmap(NULL, slab_classes[classid], PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (slab->mem == MAP_FAILED) {
        free(slab);
        return -2;
    }
    memset(slab->mem, 0, slab_classes[classid]);
    
    slab->total_blocks = slab_classes[classid] / pool->block_size;
    slab->free_blocks = slab->total_blocks;

    
    LIST_INSERT(pool->empty_slabs, slab);

    char * curr = slab->mem;
    for (int j = 0; j < (slab_classes[classid] / pool->block_size) - 1; j++) {
        *(char **)curr = curr + pool->block_size;
        curr += pool->block_size;
    }       
    *(char **)curr = NULL;
    slab->free_list = slab->mem;

    pool->empty_slab_count++;

    return 0;
}

void * slab_malloc(mem_pool_t * pool, int class_id) {
    if (pool == NULL || class_id < 0 || class_id >= CLASS_COUNT) return NULL;

    slab_t * slab = NULL;

    if (pool->partial_slabs != NULL) {
        slab = pool->partial_slabs;
    } 
    else if (pool->empty_slabs != NULL) {
        slab = pool->empty_slabs;
    }
    else {
        if (add_pool(pool, class_id) != 0) return NULL;
        slab = pool->empty_slabs;
    }


    block_header_t * header = (block_header_t *)slab->free_list;
    slab->free_list = *(char **)(slab->free_list);

    header->class_id = class_id;
    header->flag = BLOCK_FLAG;
    header->slab = slab;
    header->magic = BLOCK_MAGIC;
    
    slab->free_blocks--;

    if (slab->free_blocks == 0) {
        LIST_REMOVE(pool->partial_slabs, slab);
        LIST_INSERT(pool->full_slabs, slab);
    }
    else if (slab->free_blocks == slab->total_blocks - 1) {
        LIST_REMOVE(pool->empty_slabs, slab);
        LIST_INSERT(pool->partial_slabs, slab);
        pool->empty_slab_count--;
    }



    return (void *)(header + 1);
}

void * mp_malloc(kvs_mempool_t * pools, size_t size) {
    if (pools == NULL || size <= 0) return NULL;
    
    size_t actual_size =  ALIGN_UP(size + sizeof(block_header_t));
    //printf("size: %ld, header: %ld, actual_size: %ld\n", size, sizeof(block_header_t), actual_size);
    if (actual_size > LARGE_THRESHOLD) {
        block_header_t * large_header = (block_header_t *)malloc(actual_size);
        if (large_header == NULL) return NULL;

        large_header->flag = BLOCK_FLAG_LARGE;
        large_header->magic = BLOCK_MAGIC;
        large_header->slab = NULL;

        return (void *)(large_header + 1);
    }

    int class_id = get_class_id(actual_size);
    if (class_id < 0) return NULL;

    mem_pool_t * pool = &(pools->classes[class_id]);
    
    void * ptr = NULL;
    ptr = slab_malloc(pool, class_id);

    return ptr;
}

void mp_free(kvs_mempool_t * pools, void * ptr) {
    if (pools == NULL || ptr == NULL) return;
    block_header_t * header = ((block_header_t *)ptr) - 1;
    if (header->magic != BLOCK_MAGIC) return;
    

    if (header->flag == BLOCK_FLAG_LARGE) {
        header->magic = 0;
        free(header);
        ptr = NULL;
        return;
    }

    if ((header->class_id >= CLASS_COUNT || header->slab == NULL || header->slab->mem == NULL)) {
        return;
    }
    header->magic = 0;
    int classid = header->class_id;
    mem_pool_t * pool = &pools->classes[header->class_id];
    slab_t * slab = header->slab;
    char * list = (char *)header;
    *(char **)list = slab->free_list;
    slab->free_list = list;
    slab->free_blocks++;
    
    if (slab->free_blocks == 1) {
        LIST_REMOVE(pool->full_slabs, slab);
        LIST_INSERT(pool->partial_slabs, slab);
    }

    if (slab->free_blocks == slab->total_blocks) {
        if (pool->empty_slab_count >= MAX_EMPTY_SLABS) {
            LIST_REMOVE(pool->partial_slabs, slab);
            munmap(slab->mem, slab_classes[classid]);
            free(slab);
            slab = NULL;            
        }
        else {
            LIST_REMOVE(pool->partial_slabs, slab);  
            LIST_INSERT(pool->empty_slabs, slab);    
            pool->empty_slab_count++;          
        }
    }

}



#endif




