#ifndef PEER_MSG_H
#define PEER_MSG_H

#include <stddef.h>

int peer_send_handshake(int sockfd, char info_hash[20]);
int peer_recv_handshake(int sockfd, char info_hash[20], char out_peer_id[20]);

#endif // !PEER_MSG_H
