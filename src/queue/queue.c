#include "queue.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define QUEUE_BYTES(q) ((q)->entry_size * (q)->capacity)

int queue_resize(queue_t *queue, uint32_t new_cap) {
  ptrdiff_t offhead = queue->head - queue->mem;
  ptrdiff_t offtail = queue->tail - queue->mem;

  void *ret = realloc(queue->mem, queue->entry_size * new_cap);
  if (ret) {
    queue->mem = ret;
    queue->head = queue->mem + offhead;
    queue->tail = queue->mem + offtail;
  } else {
    return -1;
  }

  queue->capacity = new_cap;

  if (queue->head > queue->tail) {
    ptrdiff_t top = queue->tail + queue->entry_size - queue->mem;
    ptrdiff_t bot = queue->mem + QUEUE_BYTES(queue) - queue->mem;

    char tmp[top];
    memcpy(tmp, queue->mem, top);
    memmove(queue->mem, queue->head, bot);
    memcpy(queue->mem + bot, tmp, top);

    queue->head = queue->mem;
    queue->tail = queue->mem + bot;
  }

  return 0;
}

queue_t *queue_init(size_t entry_size, int init_capacity) {
  queue_t *q = malloc(sizeof(queue_t));
  if (!q) {
    return NULL;
  }

  q->mem = calloc(init_capacity, entry_size);
  if (!q->mem) {
    free(q);
    return NULL;
  }
  q->entry_size = entry_size;
  q->capacity = init_capacity;
  q->head = q->mem;
  q->tail = q->mem - entry_size;
  q->size = 0;

  return q;
}

queue_t *queue_copy(const queue_t *queue) {
  queue_t *q = malloc(sizeof(queue_t));
  if (!q) {
    return NULL;
  }

  q->mem = calloc(queue->capacity, queue->entry_size);
  if (!q->mem) {
    free(q);
    return NULL;
  }

  memcpy(q->mem, queue->mem, queue->entry_size * queue->capacity);

  q->size = queue->size;
  q->capacity = queue->capacity;
  q->entry_size = queue->entry_size;
  q->head = q->mem + (queue->head - queue->mem);
  q->tail = q->mem + (queue->tail - queue->mem);

  return q;
}

void queue_free(queue_t *q) {
  free(q->mem);
  free(q);
}

int enqueue(queue_t *q, void *entry) {
  if (q->size == q->capacity) {
    if (queue_resize(q, q->capacity * 2)) {
      return -1;
    }
  }

  q->tail += q->entry_size;

  if (q->tail >= q->mem + QUEUE_BYTES(q)) {
    q->tail = q->mem;
  }

  assert(q->tail >= q->mem && q->tail < q->mem + QUEUE_BYTES(q));

  memcpy(q->tail, entry, q->entry_size);
  q->size++;

  return 0;
}

int dequeue(queue_t *queue, void *out) {
  if (queue->size == 0) {
    return -1;
  }

  assert(queue->head >= queue->mem &&
         queue->head < queue->mem + QUEUE_BYTES(queue));
  memcpy(out, queue->head, queue->entry_size);
  queue->head += queue->entry_size;

  if (queue->head >= queue->mem + QUEUE_BYTES(queue)) {
    queue->head = queue->mem;
  }

  queue->size--;

  return 0;
}
