#include "peer_msg.h"
#include "../bitfield/bitfield.h"
#include "../file-parser/file-parser.h"
#include "../log/log.h"
#include "../peer-connection/peer-connection.h"
#include "../peer-id/peer-id.h"
#include "../piece-request/piece_request.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

int send_buff(int sockfd, char *buff, size_t bufflen) {
  size_t total_sent = 0;
  while (total_sent < bufflen) {
    ssize_t n = send(sockfd, buff + total_sent, bufflen - total_sent, 0);
    if (n < 0) {
      log_printf(LOG_ERROR, "Could not send data to peer\n");
      return -1;
    }

    total_sent += n;
  }
  if (total_sent == bufflen) {
    return 0;
  } else {
    return -1;
  }
}

int peer_send_handshake(int sockfd, char info_hash[20]) {
  const char *pstr = "BitTorrent protocol";
  uint8_t pstrlen = strlen(pstr);
  char reserved[8] = {0};
  size_t offset = 0;

  size_t bufflen = 1 + pstrlen + sizeof(reserved) + 20 + sizeof(peer_id);
  char req_buf[bufflen];
  req_buf[0] = pstrlen;
  offset++;

  memcpy(req_buf + offset, pstr, pstrlen);
  offset += pstrlen;

  memcpy(req_buf + offset, reserved, sizeof(reserved));
  offset += sizeof(reserved);

  memcpy(req_buf + offset, info_hash, 20);
  offset += 20;

  memcpy(req_buf + offset, peer_id, sizeof(peer_id));
  offset += sizeof(peer_id);

  assert(offset == bufflen);

  return send_buff(sockfd, req_buf, bufflen);
}

int peer_recv(int sockfd, char *outbuff, size_t len) {
  size_t received = 0;
  ssize_t nb;

  do {
    nb = recv(sockfd, outbuff + received, len - received, 0);
    if (nb < 0) {
      return -1;
    }

    received += nb;
  } while (nb > 0 && received < len);

  if (received == len) {
    return 0;
  }

  return -1;
}

int peer_recv_handshake(int sockfd, char info_hash[20], char out_peer_id[20]) {
  const char *pstr = "BitTorrent protocol";
  uint8_t pstrlen = strlen(pstr);
  char reserved[8] = {0};
  size_t offset = 0;

  size_t bufflen = 1 + pstrlen + sizeof(reserved) + 20 + sizeof(peer_id);
  char buff[bufflen];

  if (peer_recv(sockfd, buff, bufflen) < 0) {
    return -1;
  }

  if (buff[offset] != pstrlen) {
    return -1;
  }
  offset++;

  if (strncmp(buff + offset, pstr, pstrlen) != 0) {
    return -1;
  }
  offset += pstrlen;

  // Skip reserved
  offset += 8;

  if (strncmp(buff + offset, info_hash, 20) != 0) {
    log_printf(LOG_ERROR, "Received wrong info_hash from peer\n");
    return -1;
  }

  offset += 20;

  memcpy(out_peer_id, buff + offset, 20);

  return 0;
}

uint32_t msgbuf_len(msg_type_t type, const metainfo_t *torrent) {
  switch (type) {
  case MSG_KEEPALIVE:
    return 0;
  case MSG_PIECE:
    return 1 + 2 * sizeof(uint32_t) + PEER_REQUEST_SIZE;
  case MSG_BITFIELD:
    return 1 + BITFIELD_NUM_BYTES(torrent->info.num_pieces);
  case MSG_REQUEST:
    return 1 + 3 * sizeof(uint32_t);
  case MSG_HAVE:
  case MSG_PORT:
    return 1 + sizeof(uint32_t);
  default:
    return 1;
  }
}

int peer_msg_send(int sockfd, peer_msg_t *msg, const metainfo_t *torrent) {
  uint32_t len = msgbuf_len(msg->type, torrent);
  log_printf(LOG_INFO, "Sending message of type: %ld, len: %u\n", msg->type,
             len);
  len = htonl(len);

  if (send_buff(sockfd, (char *)&len, sizeof(uint32_t)) < 0) {
    return -1;
  }

  if (msg->type == MSG_KEEPALIVE) {
    return 0;
  }

  char out = msg->type;
  if (send_buff(sockfd, &out, 1) < 0) {
    return -1;
  }

  switch (msg->type) {
  case MSG_CHOKE:
  case MSG_UNCHOKE:
  case MSG_INTERESTED:
  case MSG_NOT_INTERESTED:
    assert(ntohl(len) == 1);
    return 0;
  case MSG_PIECE: {
  }
  case MSG_BITFIELD: {
    assert(msg->payload.bitfield);
    return send_buff(sockfd, (char *)msg->payload.bitfield->str,
                     msg->payload.bitfield->size);
  }
  case MSG_REQUEST: {
    uint32_t u32 = htonl(msg->payload.request.index);
    if (send_buff(sockfd, (char *)&u32, sizeof(uint32_t)) < 0) {
      return -1;
    }
    u32 = htonl(msg->payload.request.begin);
    if (send_buff(sockfd, (char *)&u32, sizeof(uint32_t)) < 0) {
      return -1;
    }
    u32 = htonl(msg->payload.request.length);
    if (send_buff(sockfd, (char *)&u32, sizeof(uint32_t)) < 0) {
      return -1;
    }

    return 0;
  }

  case MSG_HAVE: {
    uint32_t payload = htonl(msg->payload.have);
    return send_buff(sockfd, (char *)&payload, sizeof(uint32_t));
  }
  case MSG_PORT: {
    uint32_t port = htonl(msg->payload.listen_port);
    return send_buff(sockfd, (char *)&port, sizeof(uint32_t));
  }
  default:
    return -1;
  }
}

static inline bool valid_len(msg_type_t type, const metainfo_t *torrent,
                             uint32_t len) {
  if (type == MSG_PIECE) {
    return (len >= (1 + 2 * sizeof(uint32_t) + 1)) &&
           (len <= (1 + 2 * sizeof(uint32_t) + PEER_REQUEST_SIZE));
  }

  return len == msgbuf_len(type, torrent);
}

int peer_msg_recv_piece(int sockfd, peer_msg_t *out, const metainfo_t *torrent,
                        uint32_t len) {
  log_printf(LOG_INFO, "Receiving piece\n");
  uint32_t u32, left = len;

  if (peer_recv(sockfd, (char *)&u32, sizeof(u32)) < 0) {
    return -1;
  }

  out->payload.piece.index = ntohl(u32);
  left -= sizeof(uint32_t);

  if (peer_recv(sockfd, (char *)&u32, sizeof(u32)) < 0) {
    return -1;
  }

  out->payload.piece.begin = ntohl(u32);
  left -= sizeof(uint32_t);

  out->payload.piece.blocklen = left;
  piece_request_t *pr = piece_request_create(torrent, out->payload.piece.index);
  if (!pr) {
    return -1;
  }

  block_request_t *br = piece_request_block_at(pr, out->payload.piece.begin);
  if (!br) {
    log_printf(LOG_ERROR, "Could not find block_request\n");
    return -1;
  }

  for (size_t i = 0; i < br->filemems->len; i++) {
    filemem_t mem = br->filemems->values[i];
    log_printf(LOG_DEBUG, "Writing %zu bytes to %p\n", mem.size, mem.mem);
    if (peer_recv(sockfd, mem.mem, mem.size) < 0) {
      piece_request_free(pr);
      return -1;
    }
  }

  piece_request_free(pr);
  return 0;
}

int peer_msg_recv(int sockfd, peer_msg_t *out, const metainfo_t *torrent) {
  uint32_t len;
  if (peer_recv(sockfd, (char *)&len, sizeof(len)) < 0) {
    return -1;
  }

  len = ntohl(len);

  log_printf(LOG_INFO, "Receiving message of length %u\n", len);

  if (len == 0) {
    out->type = MSG_KEEPALIVE;
    return 0;
  }

  uint8_t type;
  if (peer_recv(sockfd, (char *)&type, 1) < 0) {
    return -1;
  }

  if (type >= MSG_MAX) {
    return -1;
  }

  if (!valid_len(type, torrent, len)) {
    return -1;
  }

  out->type = type;
  uint32_t left = len - 1;

  switch (type) {
  case MSG_CHOKE:
  case MSG_UNCHOKE:
  case MSG_INTERESTED:
  case MSG_NOT_INTERESTED:
    assert(left == 0);
    break;
  case MSG_PIECE:
    assert(left > 0);
    if (peer_msg_recv_piece(sockfd, out, torrent, left) < 0) {
      return -1;
    }
    break;
  case MSG_BITFIELD: {
    char buf[left];
    if (peer_recv(sockfd, buf, left) < 0) {
      return -1;
    }

    out->payload.bitfield = byte_str_new(left, (uint8_t *)"");
    if (!out->payload.bitfield) {
      return -1;
    }

    memcpy(out->payload.bitfield->str, buf, left);
    break;
  }
  case MSG_REQUEST: {
    char buf[left];
    if (peer_recv(sockfd, buf, left) < 0) {
      return -1;
    }
    assert(left == 3 * sizeof(uint32_t));
    uint32_t u32;
    memcpy(&u32, buf, sizeof(uint32_t));
    out->payload.request.index = ntohl(u32);

    memcpy(&u32, buf + sizeof(uint32_t), sizeof(uint32_t));
    out->payload.request.begin = ntohl(u32);

    memcpy(&u32, buf + 2 * sizeof(uint32_t), sizeof(uint32_t));
    out->payload.request.length = ntohl(u32);
    break;
  }
  case MSG_HAVE: {
    uint32_t u32;
    assert(left == sizeof(uint32_t));
    if (peer_recv(sockfd, (char *)&u32, left) < 0) {
      return -1;
    }
    out->payload.have = ntohl(u32);
    break;
  }
  case MSG_PORT: {
    uint32_t u32;
    assert(left == sizeof(uint32_t));
    if (peer_recv(sockfd, (char *)&u32, left) < 0) {
      return -1;
    }
    out->payload.listen_port = ntohl(u32);
    break;
  }
  default:
    return -1;
  }

  log_printf(LOG_DEBUG, "Successfully received message from peer, Type %hhu\n",
             type);

  return 0;
}

bool peer_msg_buff_nonempty(int sockfd) {
  uint32_t len;
  int n = recv(sockfd, (char *)&len, sizeof(uint32_t), MSG_PEEK | MSG_DONTWAIT);
  if ((uint32_t)n < sizeof(uint32_t)) {
    return false;
  }

  len = ntohl(len);

  int bytes_available;
  if (ioctl(sockfd, FIONREAD, &bytes_available)) {
    return false;
  }

  return (unsigned)bytes_available >= len + sizeof(uint16_t);
}
