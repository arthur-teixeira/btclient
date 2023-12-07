#include "peer_msg.h"
#include "../bitfield/bitfield.h"
#include "../file-parser/file-parser.h"
#include "../log/log.h"
#include "../peer-id/peer-id.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
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

#define KB (1 << 10)
#define PEER_REQUEST_SIZE (16 * KB)

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

int peer_msg_send_piece(int sockfd, piece_msg_t *piece,
                        const metainfo_t *torrent) {
    piece

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
