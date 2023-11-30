#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>

typedef struct queue {
  size_t entry_size;
  int capacity;
  size_t size;
  char *head;
  char *tail;
  char *mem;
} queue_t;

#endif // !QUEUE_H
