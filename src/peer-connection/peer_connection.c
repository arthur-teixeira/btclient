#include "../bitfield/bitfield.h"
#include "../byte-str/byte_str.h"
#include "../peer-msg/peer_msg.h"
#include "../sha1/sha1.h"
#include "../queue/queue.h"
#include "peer-connection.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <mqueue.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define PEER_TIMEOUT_SEC 120

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

  da_init(state->local_requests, sizeof(piece_request_t));
  if (!state->local_requests->values) {
    goto fail_local_requests;
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

void unchoke(int sockfd, conn_state_t *state, const metainfo_t *torrent) {
  peer_msg_t unchoke_msg = {
      .type = MSG_UNCHOKE,
  };

  if (peer_msg_send(sockfd, &unchoke_msg, torrent) < 0) {
    return;
  }

  state->remote.choked = false;
  log_printf(LOG_DEBUG, "Unchoked peer\n");
}

void service_have_events(int sockfd, mqd_t queue, const metainfo_t *torrent,
                         uint8_t *havebf) {
  peer_msg_t msg = {
      .type = MSG_HAVE,
  };

  uint32_t have;
  int ret;
  while ((ret = mq_receive(queue, (char *)&have, sizeof(uint32_t), 0)) ==
         sizeof(uint32_t)) {
    msg.payload.have = have;
    BITFIELD_SET(have, havebf);
    if (peer_msg_send(sockfd, &msg, torrent) < 0) {
      break;
    }
    log_printf(LOG_INFO, "Event serviced: have (%u) sent to peer\n", have);
  }
}

void show_interested(int sockfd, conn_state_t *state,
                     const metainfo_t *torrent) {
  peer_msg_t interested_msg;
  interested_msg.type = MSG_INTERESTED;

  if (peer_msg_send(sockfd, &interested_msg, torrent) < 0) {
    return;
  }

  state->local.interested = true;
  log_printf(LOG_DEBUG, "Showed interested to the peer\n");
}

void process_piece_msg(int sockfd, conn_state_t *state, piece_msg_t *msg,
                       metainfo_t *torrent) {

  for (size_t i = 0; i < state->local_requests->len; i++) {
    piece_request_t *curr = &state->local_requests->values[i];

    if (curr->piece_index == msg->index) {
      for (size_t j = 0; j < curr->block_requests_count; j++) {
        block_request_t *br = &curr->block_requests[j];

        if (br->len == msg->blocklen && br->begin == msg->begin) {
          br->completed = true;
          curr->blocks_left--;
          break;
        }
      }

      if (curr->blocks_left == 0) {
        bool valid = torrent_sha1_verify(torrent, curr->piece_index);

        if (!valid) {
          log_printf(LOG_WARNING,
                     "Piece downloaded does not have expected SHA1 hash\n");

          pthread_mutex_lock(&torrent->sh.sh_lock);
          torrent->sh.piece_states[curr->piece_index] =
              PIECE_STATE_NOT_REQUESTED;
          pthread_mutex_unlock(&torrent->sh.sh_lock);
        } else {
          log_printf(LOG_INFO, "Successfully downloaded a piece %u\n",
                     curr->piece_index);
          handle_piece_dl_completion(sockfd, torrent, curr->piece_index);
        }

        piece_request_free(curr); // TODO: Might segfault
      }
    }
  }
}

void process_msg(int sockfd, peer_msg_t *msg, conn_state_t *state,
                 metainfo_t *torrent) {
  switch (msg->type) {
  case MSG_KEEPALIVE:
    break;
  case MSG_CHOKE:
    state->local.choked = true;
    break;
  case MSG_UNCHOKE:
    log_printf(LOG_DEBUG, "Unchoked\n");
    state->local.choked = false;
  case MSG_INTERESTED:
    state->remote.interested = true;
    log_printf(LOG_DEBUG, "Peer interested in us\n");
    break;
  case MSG_NOT_INTERESTED:
    state->remote.interested = false;
    break;
  case MSG_HAVE:
    if (!state->local.interested &&
        BITFIELD_ISSET(msg->payload.have, state->local_have)) {
      show_interested(sockfd, state, torrent);
    }
    BITFIELD_SET(msg->payload.have, state->peer_have);
    break;
  case MSG_BITFIELD:
    assert(msg->payload.bitfield->size == BITFIELD_NUM_BYTES(state->bitlen));
    memcpy(state->peer_have, msg->payload.bitfield->str,
           BITFIELD_NUM_BYTES(state->bitlen));

    pthread_mutex_lock(&torrent->sh.sh_lock);
    bool interested = false;
    for (int i = 0; i < torrent->info.num_pieces; i++) {
      if (torrent->sh.piece_states[i] != PIECE_STATE_HAVE &&
          BITFIELD_ISSET(i, state->peer_have)) {
        interested = true;
        break;
      }
    }
    pthread_mutex_unlock(&torrent->sh.sh_lock);
    if (interested) {
      show_interested(sockfd, state, torrent);
    }
    break;

  case MSG_REQUEST:
    log_printf(LOG_DEBUG,
               "pushing request:\n"
               "    index: %u\n"
               "    length: %u\n"
               "    begin: %u\n",
               msg->payload.request.index, msg->payload.request.length,
               msg->payload.request.begin);

    enqueue(state->peer_requests, &msg->payload.request);
    break;
  case MSG_PIECE:
    process_piece_msg(sockfd, state, &msg->payload.piece, torrent);
    state->block_recvd++;
    break;
  case MSG_CANCEL:
  case MSG_PORT:
    break;
  default:
    break;
  }
}

int process_queued_messages(int sockfd, metainfo_t *torrent,
                            conn_state_t *state, time_t *last) {
  while (peer_msg_buff_nonempty(sockfd)) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_testcancel();
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    peer_msg_t msg;
    if (peer_msg_recv(sockfd, &msg, torrent) < 0) {
      return -1;
    }
    *last = time(NULL);

    process_msg(sockfd, &msg, state, torrent);
    if (msg.type == MSG_BITFIELD) {
      byte_str_free(msg.payload.bitfield);
    }
  }
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

    char ipstr[INET_ADDRSTRLEN];
    print_ip(&parg->peer, ipstr, INET_ADDRSTRLEN);

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
        if (peer_msg_send(sockfd, &bitmsg, parg->torrent) < 0) {
          byte_str_free(bitmsg.payload.bitfield);
          goto abort_conn;
        }
        byte_str_free(bitmsg.payload.bitfield);

        unchoke(sockfd, state, parg->torrent);
        time_t last_msg_time = time(NULL);
        while (true) {
          time_t curr = time(NULL);
          if (curr - last_msg_time > PEER_TIMEOUT_SEC) {
            log_printf(LOG_WARNING, "Connection with peer [%s] timed out\n",
                       ipstr);
            goto abort_conn;
          }

          pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
          usleep(250000);
          pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

          service_have_events(sockfd, queue, parg->torrent, state->local_have);
        }

      abort_conn:;
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
