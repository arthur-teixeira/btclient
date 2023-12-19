#ifndef PEER_CONNECTION_H
#define PEER_CONNECTION_H
#include "../file-parser/file-parser.h"
#include "../piece-request/piece_request.h"
#include "../queue/queue.h"
#include "../tracker/peer_parser.h"
#include <pthread.h>
#include <stdint.h>

#define KB (1 << 10)
#define PEER_REQUEST_SIZE (16 * KB)

typedef struct {
  metainfo_t *torrent;
  peer_t peer;
  int sockfd;
} peer_arg_t;

typedef struct {
  bool choked;
  bool interested;
} peer_state_t;

typedef struct {
  size_t len;
  size_t cap;
  piece_request_t *values;
} piece_requests_t;

typedef struct {
  peer_state_t local;
  peer_state_t remote;
  uint8_t *peer_have;
  uint8_t *peer_wants;
  uint8_t *local_have;
  size_t bitlen;
  uint32_t blocks_sent;
  uint32_t block_recvd;
  piece_requests_t *local_requests;
  queue_t *peer_requests;

} conn_state_t;

int peer_connection_create(pthread_t *thread, peer_arg_t *arg);
void peer_connection_queue_name(pthread_t thread, char *out, size_t len);

#endif // !PEER_CONNECTION_H
