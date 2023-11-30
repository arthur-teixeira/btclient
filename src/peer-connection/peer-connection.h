#ifndef PEER_CONNECTION_H
#define PEER_CONNECTION_H
#include "../tracker/peer_parser.h"
#include <pthread.h>

typedef struct {
  peer_t peer;
  pthread_t thread;
} peer_connection_t;

typedef struct {
  struct metainfo_t *torrent;
  peer_t peer;
  int sockfd;
} peer_arg_t;

int peer_connection_create(pthread_t *thread, peer_arg_t *arg);

#endif // !PEER_CONNECTION_H
