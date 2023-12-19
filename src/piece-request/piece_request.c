#include "piece_request.h"
#include "../deps/stb_bencode.h"
#include "../dl-file/dl_file.h"
#include "../peer-connection/peer-connection.h"

void skip_until_index(const dl_file_t **file_iter, size_t *cur_file_index,
                      off_t *offset, uint32_t index,
                      const metainfo_t *torrent) {
  size_t skip = torrent->info.piece_length * index;

  while (skip > 0) {
    filemem_t mem;
    dl_file_getfilemem(*file_iter, &mem);

    if (mem.size > skip) {
      *offset = skip;
      return;
    }

    skip -= mem.size;
    *file_iter = *file_iter + 1; // TODO: check pointer arithmetic correctness
    *cur_file_index = *cur_file_index + 1;
  }
}

int next_block_request(block_request_t *out, const dl_file_t *dl_file,
                       off_t *offset, size_t *left, size_t piecelen,
                       size_t *cur_file_index, size_t file_count) {
  if (!dl_file || *left == 0) {
    return -1;
  }

  out->begin = piecelen - *left;
  out->completed = false;
  out->len = 0;
  out->filemems = malloc(sizeof(da_filemems_t));
  if (!out->filemems) {
    return -1;
  }
  da_init(out->filemems, sizeof(filemem_t));
  if (!out->filemems) {
    return -1;
  }

  uint32_t curr_size = 0;

  do {
    assert(dl_file);
    filemem_t mem;
    dl_file_getfilemem(dl_file, &mem);
    mem.mem = ((char *)mem.mem + *offset);
    mem.size -= *offset;

    if (mem.size > PEER_REQUEST_SIZE - curr_size) {
      mem.size = PEER_REQUEST_SIZE - curr_size;
      *offset += mem.size;
    } else {
      *cur_file_index += 1;
      dl_file += 1; // TODO: check pointer arithmetic correctness
      *offset = 0;
    }

    *left -= mem.size;
    da_append(out->filemems, mem);
    curr_size += mem.size;
  } while (curr_size < PEER_REQUEST_SIZE && *cur_file_index < file_count);

  out->len = curr_size;
  return 0;
}

piece_request_t *piece_request_create(const metainfo_t *torrent,
                                      uint32_t index) {
  piece_request_t *ret = malloc(sizeof(piece_request_t));
  if (!ret) {
    return NULL;
  }

  ret->piece_index = index;
  ret->block_requests = malloc(sizeof(block_requests_t));
  if (!ret->block_requests) {
    free(ret);
  }
  da_init(ret->block_requests, sizeof(block_request_t));
  if (!ret->block_requests->values) {
    free(ret->block_requests);
    free(ret);
    return NULL;
  }

  size_t left = torrent->info.piece_length;
  off_t offset = 0;

  const dl_file_t *iter = torrent->files;
  size_t cur_file_index = 0;

  skip_until_index(&iter, &cur_file_index, &offset, index, torrent);

  block_request_t block;
  while (next_block_request(&block, iter, &offset, &left,
                            torrent->info.piece_length, &cur_file_index,
                            torrent->info.files_count) == 0) {
    da_append(ret->block_requests, block);
  }

  ret->blocks_left = ret->block_requests->len;
  return ret;
}

block_request_t *piece_request_block_at(piece_request_t *request,
                                        off_t offset) {
  const uint8_t *entry;
  for (size_t i = 0; i < request->block_requests->len; i++) {
    if (request->block_requests->values[i].begin == offset) {
      return &request->block_requests->values[i];
    }
  }

  return NULL;
}

void piece_request_free(piece_request_t *request) {
  for (size_t i = 0; i < request->block_requests->len; i++) {
    block_request_t *br = &request->block_requests->values[i];
    free(br->filemems->values);
  }

  free(request->block_requests->values);
  free(request);
}
