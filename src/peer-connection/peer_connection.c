#include "peer-connection.h"
#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>

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
