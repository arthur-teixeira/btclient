#include "peer_msg.h"
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
