#include "peer_parser.h"
#include <stdlib.h>

peer_t *parse_peers(const char *buf, size_t peer_count) {
  peer_t *peers = calloc(peer_count, sizeof(peer_t));

  for (size_t i = 0; i < peer_count; i++) {
    uint32_t ip;
    memcpy(&ip, buf + (6 * i), sizeof(uint32_t));

    uint16_t port;
    memcpy(&port, buf + (6 * i) + sizeof(uint32_t), sizeof(uint16_t));

    peers[i].addr.sas.ss_family = AF_INET;
    peers[i].addr.sa_in.sin_addr.s_addr = ip;
    peers[i].addr.sa_in.sin_port = port;
    memset(peers[i].addr.sa_in.sin_zero, 0,
           sizeof(peers[i].addr.sa_in.sin_zero));

    memset(peers[i].peer_id, 0, sizeof(peers[i].peer_id));
  }

  return peers;
}
