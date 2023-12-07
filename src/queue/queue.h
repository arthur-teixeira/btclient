#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <stddef.h>

typedef struct queue {
  size_t entry_size;
  int capacity;
  size_t size;
  char *head;
  char *tail;
  char *mem;
} queue_t;

queue_t *queue_init(size_t entry_size, int init_capacity);
void queue_free(queue_t *q);
queue_t *queue_copy(const queue_t *queue);
int enqueue(queue_t *q, void *entry);
int dequeue(queue_t *queue, void *out);
int queue_resize(queue_t *queue, uint32_t new_cap);

#endif // !QUEUE_H
