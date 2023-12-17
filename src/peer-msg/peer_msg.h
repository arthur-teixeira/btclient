#ifndef PEER_MSG_H
#define PEER_MSG_H

#include "../byte-str/byte_str.h"
#include "../file-parser/file-parser.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint32_t index;
  uint32_t begin;
  uint32_t length;
} request_msg_t;

typedef struct {
  uint32_t index;
  uint32_t begin;
  size_t blocklen;
} piece_msg_t;

typedef enum {
  MSG_CHOKE,
  MSG_UNCHOKE,
  MSG_INTERESTED,
  MSG_NOT_INTERESTED,
  MSG_HAVE,
  MSG_BITFIELD,
  MSG_REQUEST,
  MSG_PIECE,
  MSG_CANCEL,
  MSG_PORT,
  MSG_KEEPALIVE,
  MSG_MAX
} msg_type_t;

typedef struct {
  msg_type_t type;
  union {
    uint32_t have;
    byte_str_t *bitfield;
    request_msg_t request;
    piece_msg_t piece;
    uint32_t listen_port;
  } payload;
} peer_msg_t;

int peer_send_handshake(int sockfd, char info_hash[20]);
int peer_recv_handshake(int sockfd, char info_hash[20], char out_peer_id[20]);
int peer_msg_send(int sockfd, peer_msg_t *msg, const metainfo_t *torrent);
bool peer_msg_buff_nonempty(int sockfd);

#endif // !PEER_MSG_H
