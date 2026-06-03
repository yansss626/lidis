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


static inline slab_t * ptr_to_slab(void * ptr) {
    return (slab_t *)((uintptr_t)ptr & (~(SLAB_SIZE_ALIGNMENT - 1)));
}

static void * alignment_to_slab_size(size_t slab_size) {
    if ((slab_size & (slab_size - 1)) != 0) return NULL;
    size_t alloc_size = slab_size * 2;

    void * raw_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw_ptr == MAP_FAILED) return NULL;

    uintptr_t raw_addr = (uintptr_t)raw_ptr;
    uintptr_t alignment_addr = (raw_addr + (slab_size - 1)) & (~(slab_size - 1));
    
    size_t head_excesss = alignment_addr - raw_addr;
    if (head_excesss > 0) {
        munmap(raw_ptr, head_excesss);
    }

    size_t tail_excess = alloc_size - slab_size - head_excesss;
    if (tail_excess > 0) {
        munmap((void *)(alignment_addr + slab_size), tail_excess);
    }

    return (void *)alignment_addr;
}


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
        if (size_classes[classid] > ALIGNMENT_THRESHOLD) {
            munmap(slab->mem, slab_classes[classid]);
            free(slab);
        }
        else {
            munmap(slab->mem, SLAB_SIZE_ALIGNMENT);
        }
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

static int get_class_id (size_t size) {


    for (int i = 0; i < CLASS_COUNT; i++) {
        if (size <= size_classes[i]) {
            return i;
        }
    }

    return -1;

}

static int add_pool_with_header (mem_pool_t * pool, int classid) {
    if (pool == NULL) return -1;

    slab_t * slab = (slab_t *)malloc(sizeof(slab_t));
    if (slab == NULL) return -2;
    memset(slab, 0, sizeof(slab_t));

    slab->mem = (char *)mmap(NULL, slab_classes[classid], PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (slab->mem == MAP_FAILED) {
        free(slab);
        return -2;
    }
    
    slab->total_blocks = slab_classes[classid] / pool->block_size;
    slab->free_blocks = slab->total_blocks;
    slab->slab_magic = 0;

    char * curr = slab->mem;
    for (int j = 0; j < (slab_classes[classid] / pool->block_size) - 1; j++) {
        *(char **)curr = curr + pool->block_size;
        curr += pool->block_size;
    }       
    *(char **)curr = NULL;
    slab->free_list = slab->mem;

    LIST_INSERT(pool->empty_slabs, slab);
    pool->empty_slab_count++;
    return 0;
}

static int add_pool_without_header (mem_pool_t * pool, int classid) {
    if (pool == NULL) return -1;

    void * mem = alignment_to_slab_size(SLAB_SIZE_ALIGNMENT);
    if (mem == NULL) {
        printf("alignment_to_slab_size error\n");
        return -2;
    }
    size_t actual_size = SLAB_SIZE_ALIGNMENT - ALIGN_UP(sizeof(slab_t));
    slab_t * slab = (slab_t *)mem;

    slab->classid = classid;
    slab->slab_magic = SLAB_MAGIC;
    slab->mem = (char *)mem;
    slab->total_blocks = actual_size / size_classes[classid];
    slab->free_blocks = slab->total_blocks;
    slab->free_list = slab->mem + ALIGN_UP(sizeof(slab_t));
    char * curr = slab->free_list;
    for (int i = 0; i < actual_size / size_classes[classid] - 1; i++) {
        *(char **)curr = curr  + size_classes[classid];
        curr += size_classes[classid];
    }
    *(char **)curr = NULL;

    LIST_INSERT(pool->empty_slabs, slab);
    pool->empty_slab_count++;

    return 0;
    
}


static int add_pool (mem_pool_t * pool, int classid) {
    if (pool == NULL) return -1;

    if (size_classes[classid] <= ALIGNMENT_THRESHOLD) {
        if (add_pool_without_header(pool, classid) != 0) {
            return -3;
        }            
    }
    else {
        if (add_pool_with_header(pool, classid) != 0) {
            return -3;
        }
    }

    return 0;
}

static slab_t * slab_malloc(mem_pool_t * pool, int class_id) {
    if (pool == NULL || class_id < 0 || class_id >= CLASS_COUNT) return NULL;

    slab_t * slab = NULL;

    if (pool->partial_slabs != NULL) {
        slab = pool->partial_slabs;
    } 
    else if (pool->empty_slabs != NULL) {
        slab = pool->empty_slabs;
        LIST_REMOVE(pool->empty_slabs, slab);
        LIST_INSERT(pool->partial_slabs, slab);
        pool->empty_slab_count--;
    }
    else {
        if (add_pool(pool, class_id) != 0) return NULL;
        slab = pool->empty_slabs;
        LIST_REMOVE(pool->empty_slabs, slab);
        LIST_INSERT(pool->partial_slabs, slab);
        pool->empty_slab_count--;
    }



    return slab;
}

static void * block_with_header(kvs_mempool_t * pools, size_t size) {
    if (pools == NULL) return NULL;

    int class_id = get_class_id(size);
    if (class_id < 0) return NULL;
    
    mem_pool_t * pool = &(pools->classes[class_id]);   

    slab_t * slab = slab_malloc(pool, class_id);
    if (slab == NULL) {
        printf("slab_malloc error\n");
        return NULL;
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

    return (void *)(header + 1);
}


static void * block_without_header(kvs_mempool_t * pools, size_t size) {
    if (pools == NULL) return NULL;

    int class_id = get_class_id(size);
    if (class_id < 0) return NULL;
    
    mem_pool_t * pool = &(pools->classes[class_id]);    
    
    slab_t * slab = slab_malloc(pool, class_id);
    if (slab == NULL) {
        printf("slab_malloc error\n");
        return NULL;
    }

    char * ptr = slab->free_list;
    slab->free_list = *(char **)(slab->free_list);
    slab->free_blocks--;

    if (slab->free_blocks == 0) {
        LIST_REMOVE(pool->partial_slabs, slab);
        LIST_INSERT(pool->full_slabs, slab);
    }

    return (void *)(ptr);    
}


void * mp_malloc(kvs_mempool_t * pools, size_t size) {
    if (pools == NULL) return NULL;
    
    // small block without header
    if (ALIGN_UP(size) <= ALIGNMENT_THRESHOLD) {
        return block_without_header(pools, ALIGN_UP(size));
    }


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

    // small block with header
    return block_with_header(pools, actual_size);
}


static void slab_free(mem_pool_t * pool, slab_t * slab, size_t size) {
    if (pool == NULL || slab == NULL) return;

    if (slab->free_blocks == 1) {
        LIST_REMOVE(pool->full_slabs, slab);
        LIST_INSERT(pool->partial_slabs, slab);
    }

    if (slab->free_blocks == slab->total_blocks) {
        if (pool->empty_slab_count >= MAX_EMPTY_SLABS) {
            LIST_REMOVE(pool->partial_slabs, slab);
            int is_no_header = (slab->slab_magic == SLAB_MAGIC);
            if (is_no_header) {
                munmap(slab->mem, SLAB_SIZE_ALIGNMENT);
            } else {
                munmap(slab->mem, size);
                free(slab);
            }
            slab = NULL;            
        }
        else {
            LIST_REMOVE(pool->partial_slabs, slab);  
            LIST_INSERT(pool->empty_slabs, slab);    
            pool->empty_slab_count++;          
        }
    }
}

static void slab_free_with_header(kvs_mempool_t * pools, block_header_t * header) {
    if (pools == NULL || header == NULL) return;

    header->magic = 0;
    int classid = header->class_id;
    mem_pool_t * pool = &pools->classes[header->class_id];
    slab_t * slab = header->slab;
    char * list = (char *)header;
    *(char **)list = slab->free_list;
    slab->free_list = list;
    slab->free_blocks++;  
    slab_free(pool, slab, slab_classes[classid]);  
}

static void slab_free_without_header(kvs_mempool_t * pools, void * ptr, slab_t * slab) {
    if (pools == NULL || ptr == NULL || slab == NULL) return;

    ptr = (char *)ptr;
    *(char **)ptr = slab->free_list;
    slab->free_list = (char *)ptr;
    slab->free_blocks++;
    slab_free(&pools->classes[slab->classid], slab, SLAB_SIZE_ALIGNMENT);  
}

void mp_free(kvs_mempool_t * pools, void * ptr) {
    if (pools == NULL || ptr == NULL) return;


    //
    block_header_t * header = ((block_header_t *)ptr) - 1;
    if (header->magic == BLOCK_MAGIC) {
        if (header->flag == BLOCK_FLAG_LARGE) {
            header->magic = 0;
            free(header);
            ptr = NULL;
            return;
        }

        if ((header->class_id >= CLASS_COUNT || header->slab == NULL || header->slab->mem == NULL)) {
            return;
        }

        slab_free_with_header(pools, header);
        return;

    }
    


    slab_t * slab = ptr_to_slab(ptr);
    if (slab != NULL && slab->slab_magic == SLAB_MAGIC) {
        slab_free_without_header(pools, ptr, slab);   
        return;   
    }

    
}



#endif




