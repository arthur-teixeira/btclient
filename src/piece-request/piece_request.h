#ifndef PIECE_REQUEST_H
#define PIECE_REQUEST_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct {
  void *filemems;
  off_t begin;
  size_t len;
  bool completed;
} block_request_t;

typedef struct {
  uint32_t piece_index;
  void *block_requests;
  uint32_t blocks_left;
} piece_request_t;

#endif // PIECE_REQUEST_H
