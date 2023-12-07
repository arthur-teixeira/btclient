#include "../bitfield/bitfield.h"
#include "../byte-str/byte_str.h"
#include "../peer-msg/peer_msg.h"
#include "../queue/queue.h"
#include "peer-connection.h"
#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>

mqd_t peer_queue_open(int flags);
void queue_cleanup(void *arg);

uint8_t *make_bitfield(const metainfo_t *torrent) {
  size_t num_pieces = torrent->info.num_pieces;
  size_t len = BITFIELD_NUM_BYTES(num_pieces);
  uint8_t *ret = calloc(len, 1);
  if (!ret) {
    return NULL;
  }

  for (size_t i = 0; i < num_pieces; i++) {
    if (torrent->sh.piece_states[i] == PIECE_STATE_HAVE) {
      BITFIELD_SET(i, ret);
    }
  }

  return ret;
}

conn_state_t *conn_state_init(metainfo_t *torrent) {
  conn_state_t *state = malloc(sizeof(conn_state_t));
  if (!state) {
    return state;
  }

  state->local.choked = true;
  state->local.interested = false;
  state->remote.choked = true;
  state->remote.interested = false;

  state->bitlen = torrent->info.num_pieces;
  uint16_t num_bytes = BITFIELD_NUM_BYTES(state->bitlen);

  state->peer_have = malloc(num_bytes);
  if (!state->peer_have) {
    goto fail_peer_have;
  }

  state->peer_wants = malloc(num_bytes);
  if (!state->peer_wants) {
    goto fail_peer_wants;
  }

  state->peer_requests = queue_init(sizeof(request_msg_t), 16);
  if (!state->peer_requests) {
    goto fail_peer_requests;
  }

  pthread_mutex_lock(&torrent->sh.sh_lock);
  state->local_have = make_bitfield(torrent);
  pthread_mutex_unlock(&torrent->sh.sh_lock);
  if (!state->local_have) {
    goto fail_local_have;
  }

  state->blocks_sent = 0;
  state->block_recvd = 0;

  return state;

fail_local_have:
fail_local_requests:
  free(state->peer_requests);
fail_peer_requests:
  free(state->peer_wants);
fail_peer_wants:
  free(state->peer_have);
fail_peer_have:
  free(state);

  return NULL;
}

void conn_state_cleanup(void *arg) {
  conn_state_t *state = arg;
  log_printf(LOG_INFO,
             "Peer connection summary: Block uploaded: %u, downloaded: %u\n",
             state->blocks_sent, state->block_recvd);

  free(state->peer_have);
  free(state->peer_wants);
  free(state->local_have);
  queue_free(state->peer_requests);

  free(state);
}

void print_ip(peer_t *peer, char *outbuf, size_t buf_len) {
  inet_ntop(AF_INET, &peer->addr.sa_in.sin_addr, outbuf, buf_len);
}

void peer_connection_cleanup(void *arg) {
  peer_arg_t *parg = (peer_arg_t *)arg;
  if (parg->sockfd >= 0) {
    shutdown(parg->sockfd, SHUT_RDWR);
    close(parg->sockfd);
  }

  char ipstr[INET_ADDRSTRLEN];
  print_ip(&parg->peer, ipstr, INET_ADDRSTRLEN);
  log_printf(LOG_INFO, "Closed peer connection %s\n", ipstr);

  free(arg);
}

int handshake(int sockfd, char info_hash[20], char out_peer_id[20]) {
  if (peer_send_handshake(sockfd, info_hash) < 0) {
    return -1;
  }

  if (peer_recv_handshake(sockfd, info_hash, out_peer_id) < 0) {
    return -1;
  }

  return 0;
}

int peer_connect(peer_arg_t *arg) {
  char ipstr[INET_ADDRSTRLEN];
  print_ip(&arg->peer, ipstr, sizeof(ipstr));

  int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (sockfd < 0) {
    return sockfd;
  }
  socklen_t len = sizeof(arg->peer.addr.sa_in);
  int result = connect(sockfd, &arg->peer.addr.sa, len);
  if (result < 0 && errno != EINPROGRESS) {
    close(sockfd);
    return -1;
  }

  struct timeval timeout;
  timeout.tv_sec = 5;
  timeout.tv_usec = 0;
  fd_set fdset;
  FD_ZERO(&fdset);
  FD_SET(sockfd, &fdset);
  if (select(sockfd + 1, NULL, &fdset, NULL, &timeout)) {
    int err;
    socklen_t len = sizeof(int);
    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &len);
    if (err) {
      close(sockfd);
      return -1;
    }
  } else { // Timeout
    log_printf(LOG_INFO, "Peer %s timed out after %u seconds\n", ipstr, 5);
    close(sockfd);
    return -1;
  }

  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));
  setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(struct timeval));

  int opts = fcntl(sockfd, F_GETFL);
  opts &= ~O_NONBLOCK;
  fcntl(sockfd, F_SETFL, opts);

  log_printf(LOG_INFO, "Connection to peer %s estabilished successfully\n",
             ipstr);

  return sockfd;
}

void *peer_connection(void *arg) {
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
  pthread_cleanup_push(peer_connection_cleanup, arg);
  {
    int sockfd;

    peer_arg_t *parg = (peer_arg_t *)arg;
    if (parg->sockfd < 0) {
      sockfd = peer_connect(arg);
      if (sockfd < 0) {
        goto fail_init;
      }

      parg->sockfd = sockfd;
    } else {
      sockfd = parg->sockfd;
    }

    char out_peer_id[20];
    if (handshake(sockfd, parg->torrent->info_hash, out_peer_id) < 0) {
      log_printf(LOG_ERROR, "Handshake failed\n");
      goto fail_init;
    };

    log_printf(LOG_INFO, "Successful handshake with peer %s\n", out_peer_id);

    mqd_t queue = peer_queue_open(O_RDONLY | O_CREAT | O_NONBLOCK);
    if (queue == (mqd_t)-1) {
      goto fail_init;
    }
    pthread_cleanup_push(queue_cleanup, NULL);
    {
      conn_state_t *state = conn_state_init(parg->torrent);
      if (!state) {
        goto fail_init_state;
      }
      pthread_cleanup_push(conn_state_cleanup, state);
      {
        peer_msg_t bitmsg = {
            .type = MSG_BITFIELD,
            .payload.bitfield = byte_str_new(BITFIELD_NUM_BYTES(state->bitlen),
                                             state->local_have),
        };

      }
      pthread_cleanup_pop(1);

    fail_init_state:;
    };
    pthread_cleanup_pop(1);

  fail_init:;
  };
  pthread_cleanup_pop(1);

  return arg;
}

int peer_connection_create(pthread_t *thread, peer_arg_t *arg) {
  if (pthread_create(thread, NULL, peer_connection, arg)) {
    log_printf(LOG_ERROR, "Failed to create peer conneciton thread\n");
    return -1;
  };

  return 0;
}

void peer_connection_queue_name(pthread_t thread, char *out, size_t len) {
  assert(len >= strlen("/") + 2 * sizeof(pthread_t) + strlen("_queue") + 1);

  size_t plen = 0;
  plen += snprintf(out, len - plen, "/");

  for (uint8_t *cp = (uint8_t *)thread;
       cp < ((uint8_t *)thread) + sizeof(pthread_t); cp++) {
    plen += snprintf(out + plen, len - plen, "%02X", *cp);
    if (plen == len)
      return;
  }

  snprintf(out + plen, len - plen, "_queue");
}

void queue_cleanup(void *arg) {
  char queue_name[64];
  peer_connection_queue_name(pthread_self(), queue_name, sizeof(queue_name));
  if (mq_unlink(queue_name) < 0) {
    log_printf(LOG_ERROR, "Failed to close queue %s\n", queue_name);
  } else {
    log_printf(LOG_DEBUG, "Successfully closed queue %s\n", queue_name);
  }
}

mqd_t peer_queue_open(int flags) {
  char queue_name[64];
  peer_connection_queue_name(pthread_self(), queue_name, sizeof(queue_name));

  struct mq_attr attr;
  attr.mq_flags = O_NONBLOCK;
  attr.mq_maxmsg = 10;
  attr.mq_msgsize = sizeof(unsigned int);
  attr.mq_curmsgs = 0;

  mqd_t result = mq_open(queue_name, flags, 0600, &attr);
  if (result != (mqd_t)-1) {
    log_printf(LOG_INFO,
               "successfully opened mqueue from receiver thread: %s\n",
               queue_name);
  } else {
    log_printf(LOG_WARNING, "Failed to open mqueue in received thread: %s\n",
               queue_name);
    perror("mq_open");
  }

  return result;
}
