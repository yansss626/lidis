#include <stdio.h>
#include "kvstore.h"
#include <string.h>
#include <stdlib.h>

#if ENABLE_SKIPLIST
#define  SKIPLIST_MAX_LEVEL 32


kvs_skiplist_t global_skiplist = {0};


static Node * createNode(int level, char * key, char * value){
    Node * new_node = (Node *)kvs_malloc(sizeof(Node));
    if(new_node == NULL) return NULL;

    new_node->forward = (Node **)kvs_malloc(sizeof(Node *) * (level + 1));
    if(new_node->forward == NULL){
        kvs_free(new_node);
        return NULL;
    }
    memset(new_node->forward, 0, sizeof(Node *) * (level + 1));

    if(key != NULL && value != NULL){
        char *kcopy = kvs_malloc(strlen(key) + 1);
        if (kcopy == NULL){
            kvs_free(new_node->forward);
            kvs_free(new_node);
            return NULL;
        } 
        memset(kcopy, 0, strlen(key) + 1);
        strncpy(kcopy, key, strlen(key));

        new_node->key = kcopy;

        char *kvalue = kvs_malloc(strlen(value) + 1);
        if (kvalue == NULL) { 
            kvs_free(new_node->forward);
            kvs_free(kcopy);
            kvs_free(new_node);
            return NULL;
        }
        memset(kvalue, 0, strlen(value) + 1);
        strncpy(kvalue, value, strlen(value));

        new_node->value = kvalue;
    }
    else{
        new_node->key = NULL;
        new_node->value = NULL;
    }

    return new_node;

}

int kvs_skiplist_create(kvs_skiplist_t *inst){
    if(inst == NULL) return -1;
    inst->header = createNode(SKIPLIST_MAX_LEVEL, NULL, NULL);
    if(inst->header == NULL) return -2;
    inst->level = 0;    

    return 0;

}
void kvs_skiplist_destroy(kvs_skiplist_t *inst){
    if(inst == NULL) return ;
    Node * node = inst->header->forward[0];
    while(node != NULL){
        Node * temp = node->forward[0];
        if(node->key != NULL){
            kvs_free(node->key);
            kvs_free(node->value);
        }
        kvs_free(node->forward);
        kvs_free(node);
        node = temp;
    }
    kvs_free(inst->header->forward);
    kvs_free(inst->header);
    return ;
}

int randomLevel(){
    int level = 0;
    while(random() < (RAND_MAX / 2) && level < SKIPLIST_MAX_LEVEL - 1){
        level++;
    }
    return level;
}

int kvs_skiplist_set(kvs_skiplist_t *inst, char *key, char *value){
    if(inst == NULL || key == NULL || value == NULL) return -1;
    Node * current = inst->header;
    Node * update[SKIPLIST_MAX_LEVEL];    
    for(int i = inst->level; i >= 0; --i){
        while(current->forward[i] != NULL && strcmp(current->forward[i]->key, key) < 0){
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];

    if(current != NULL && strcmp(current->key, key) == 0) return 1;

    int level  = randomLevel();
    Node * new_node = createNode(level, key, value);
    if(new_node == NULL) return -2;
    if(level > inst->level){
        for(int i = inst->level + 1; i <= level; i++){
            update[i] = inst->header;
        }
        inst->level = level;
    }
    for(int i = level; i >= 0; --i){
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }
    return 0;

}
char* kvs_skiplist_get(kvs_skiplist_t *inst, char *key){
    if(inst == NULL || key == NULL) return NULL;
    Node * current = inst->header;
    for(int i = inst->level; i >= 0; --i){
        while(current->forward[i] != NULL && strcmp(current->forward[i]->key, key) < 0){
            current = current->forward[i];
        }
    }

    current = current->forward[0];

    if (current == NULL || strcmp(current->key, key) != 0) return NULL;
        
    return current->value;

    
}

int kvs_skiplist_del(kvs_skiplist_t *inst, char *key){
    if(inst == NULL || key == NULL) return -1;
    Node * current = inst->header;
    Node * update[inst->level + 1];
    for(int i = inst->level; i >= 0; --i){
        while(current->forward[i] != NULL && strcmp(current->forward[i]->key, key) < 0){
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];

    if (current == NULL || strcmp(current->key, key) != 0) return 1;


    for(int i = 0; i <= inst->level; ++i){
        if(update[i]->forward[i] == current){
            update[i]->forward[i] = current->forward[i];
        }
        else{
            break;
        }
    }
    kvs_free(current->key);
    kvs_free(current->value);
    kvs_free(current->forward);
    kvs_free(current);

    while(inst->level > 0 && inst->header->forward[inst->level] == NULL) --inst->level;


    return 0;
    



}
int kvs_skiplist_mod(kvs_skiplist_t *inst, char *key, char *value){
    if(inst == NULL || key == NULL || value == NULL) return -1;
    Node * current = inst->header;
    for(int i = inst->level; i >= 0; --i){
        while(current->forward[i] != NULL && strcmp(current->forward[i]->key, key) < 0){
            current = current->forward[i];
        }
    }

    current = current->forward[0];

    if (current == NULL || strcmp(current->key, key) != 0) return 1;


    char * kvalue = (char *)kvs_malloc(strlen(value) + 1);
    if(kvalue == NULL) return -2;
    memset(kvalue, 0, strlen(value) + 1);
    strncpy(kvalue, value, strlen(value));
    kvs_free(current->value);
    current->value = kvalue;

    return 0;

}
int kvs_skiplist_exist(kvs_skiplist_t *inst, char *key){
    if(inst == NULL || key == NULL) return -1;
    if(kvs_skiplist_get(inst, key) == NULL) return 1;

    return 0;
}


#endif