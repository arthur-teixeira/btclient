#include "piece_request.h"
#include "../deps/stb_bencode.h"
#include "../dl-file/dl_file.h"
#include "../peer-connection/peer-connection.h"

void skip_until_index(const dl_file_t **file_iter, off_t *offset, uint32_t index,
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
  }
}

block_request_t *next_block_request(dl_file_t *dl_file, off_t *offset,
                                    size_t *left, size_t piecelen,
                                    size_t *cur_file_index, size_t file_count) {
  if (!dl_file || *left == 0) {
    return NULL;
  }

  block_request_t *ret = malloc(sizeof(block_request_t));
  if (!ret) {
    return NULL;
  }

  ret->begin = piecelen - *left;
  ret->completed = false;
  ret->len = 0;
  da_init(ret->filemems, sizeof(filemem_t));
  if (!ret->filemems) {
    free(ret);
    return NULL;
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
    da_append(ret->filemems, mem);
    curr_size += mem.size;
  } while (curr_size < PEER_REQUEST_SIZE && *cur_file_index < file_count);

  ret->len = curr_size;
  return ret;
}

piece_request_t *piece_request_create(const metainfo_t *torrent,
                                      uint32_t index) {
  piece_request_t *ret = malloc(sizeof(piece_request_t));
  if (!ret) {
    return NULL;
  }

  filemem_t *dl_file = &torrent->files[0];

  ret->piece_index = index;
  ret->block_requests = NULL;

  size_t left = torrent->info.piece_length;

  return ret;
}
