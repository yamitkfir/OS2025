#include <stdlib.h>
#include <stdbool.h>
#include <threads.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>

void initQueue(void); // called before the queue is used. chance to initialize your data structure.
void enqueue(void*); // Adds an item to the queue
void* dequeue(void); // Remove an item from the queue. Will block if empty
void destroyQueue(void); // cleanup when the queue is no longer needed 
size_t visited(void); // amount of items that have passed inside the queue (i.e., inserted and then removed).
// This should not block due to concurrent operations, i.e., you may not take a lock at all.

typedef struct LinkedList{
    void* item;
    struct LinkedList* next;
} LinkedList;

static LinkedList* queue_head;
static LinkedList* queue_tail;
static LinkedList* cond_vars_head;
static LinkedList* cond_vars_tail;
static mtx_t mutex;
static volatile size_t visited_count;
static volatile size_t len_of_queue; // holds num of Q items that aren't destroyed or will be destroyed soon

void initQueue(void)
{
    mtx_init(&mutex, mtx_plain);
    mtx_lock(&mutex); // take lock
    queue_head = NULL; queue_tail = NULL;
    cond_vars_head = NULL; cond_vars_tail = NULL;
    visited_count = 0;
    len_of_queue = 0;
    mtx_unlock(&mutex); // release lock
}

void enqueue(void* item)
{
    mtx_lock(&mutex); // take lock
    LinkedList* new_item;
    len_of_queue++;

    new_item = (LinkedList*)malloc(sizeof(LinkedList));
    new_item->item = item;
    new_item->next = NULL;
    if (queue_tail != NULL){
        queue_tail->next = new_item;
        queue_tail = queue_tail->next;
    }
    else{
        queue_head = new_item;
        queue_tail = new_item;
    }
    if (cond_vars_head != NULL){
        cnd_signal(cond_vars_head->item);
    }
    mtx_unlock(&mutex); // release lock
}

void* dequeue(void)
{
    mtx_lock(&mutex); // take lock
    cnd_t new_cond;
    LinkedList* new_cond_var = NULL;
    LinkedList* to_free;
    void* returned;
    int did_wait = 0;

    cnd_init(&new_cond);
    if(queue_head == NULL || len_of_queue <= 0 || cond_vars_head != NULL)
    // if(queue_head == NULL || len_of_queue <= 0)
    // if(queue_head == NULL || cond_vars_head != NULL)
    {
        new_cond_var = (LinkedList*)malloc(sizeof(LinkedList));
        new_cond_var->item = &new_cond;
        new_cond_var->next = NULL;
        if (cond_vars_head != NULL){
            cond_vars_tail->next = new_cond_var;
            cond_vars_tail = cond_vars_tail->next;
        }
        else{
            cond_vars_head = new_cond_var;
            cond_vars_tail = new_cond_var;
        }
        did_wait = 1;
        cnd_wait(&new_cond, &mutex);
    }
    if (queue_head == NULL || len_of_queue <= 0){ // It's a thread with no purpose, since got woken up here it's from "destroy"
        // printf("useless thread alert\n");
        if (did_wait) {
            to_free = cond_vars_head;
            cond_vars_head = cond_vars_head->next;
            free(to_free);
            if (cond_vars_head == NULL) cond_vars_tail = NULL;
        }
        cnd_destroy(&new_cond);
        mtx_unlock(&mutex);
        return NULL;
    }
    visited_count++;
    len_of_queue--;
    returned = queue_head->item;
    to_free = queue_head;
    queue_head = queue_head->next;
    free(to_free);
    if (queue_head == NULL){
        queue_tail = NULL;
    }
    if (did_wait) {
        to_free = cond_vars_head;
        cond_vars_head = cond_vars_head->next;
        free(to_free);
        if (cond_vars_head == NULL) cond_vars_tail = NULL;
        // Signal next waiter if there are still items and waiters
        if (cond_vars_head != NULL && queue_head != NULL && len_of_queue > 0) {
            cnd_signal(cond_vars_head->item);
        }
    }
    cnd_destroy(&new_cond);
    mtx_unlock(&mutex);
    return returned;
}

void destroyQueue(void){
    mtx_lock(&mutex); // take lock
    LinkedList* to_free;
    while (queue_head != NULL) {
        to_free = queue_head;
        queue_head = queue_head->next;
        free(to_free);
    }

    // TODO needed?
    // while (cond_vars_head != NULL){
    //     cnd_signal(cond_vars_head->item); // If there're threads with no purpose, dont keep them waiting 4ever
    //     // cnd_destroy(cond_vars_head->item);
    //     to_free = cond_vars_head;
    //     cond_vars_head = cond_vars_head->next;
    //     free(to_free);
    // }

    mtx_unlock(&mutex);
    mtx_destroy(&mutex);
}

size_t visited(void){
    // don't take lock
    return visited_count;
}