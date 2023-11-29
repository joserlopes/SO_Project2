#include "producer-consumer.h"

int pcq_create(pc_queue_t *queue, size_t capacity) {
    (void)queue;
    (void)capacity;
    return -1;
}

int pcq_enqueue(pc_queue_t *queue, void *elem) {
    (void)queue;
    (void)elem;
    return -1;
}

void *pcq_dequeue(pc_queue_t *queue) {
    (void)queue;
    return NULL;
}

int pcq_destroy(pc_queue_t *queue) {
    (void)queue;
    return -1;
}