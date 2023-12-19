#ifndef PIECE_REQUEST_H
#define PIECE_REQUEST_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include "../file-parser/file-parser.h"
#include "../dl-file/dl_file.h"

typedef struct {
    size_t cap;
    size_t len;
    filemem_t *values;
} da_filemems_t;

typedef struct {
  da_filemems_t *filemems;
  off_t begin;
  size_t len;
  bool completed;
} block_request_t;

typedef struct {
    size_t cap;
    size_t len;
    block_request_t *values;
} block_requests_t;

typedef struct {
  uint32_t piece_index;
  block_requests_t *block_requests;
  uint32_t blocks_left;
} piece_request_t;

piece_request_t *piece_request_create(const metainfo_t *torrent, uint32_t index);
void piece_request_free(piece_request_t *request);
block_request_t *piece_request_block_at(piece_request_t *request, off_t offset);

#endif // PIECE_REQUEST_H
