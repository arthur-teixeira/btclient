#include "tracker_udp.h"
#include "../peer_parser.h"
#include <endian.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#define PROTOCOL_ID 0x41727101980
#define MAX_RECV_BUFSIZE 2048
#define CONNECTION_EXPIRED(_time) (time(NULL) - _time > 30)

typedef enum {
  ACTION_CONNECT = 0,
  ACTION_ANNOUNCE = 1,
  ACTION_SCRAPE = 2,
  ACTION_ERROR = 3,
} udp_action_t;

typedef struct __attribute__((packed)) {
  uint64_t connection_id;
  uint32_t action;
  uint32_t transaction_id;
} udp_connect_req_t;

typedef struct __attribute__((packed)) {
  uint32_t action;
  uint32_t transaction_id;
  uint64_t connection_id;
} udp_connect_res_t;

typedef struct __attribute__((packed)) {
  uint64_t connection_id;
  uint32_t action;
  uint32_t transaction_id;
  char info_hash[20];
  char peer_id[20];
  uint64_t downloaded;
  uint64_t left;
  uint64_t uploaded;
  uint32_t event;
  uint32_t ip_address;
  uint32_t key;
  int32_t num_want;
  uint16_t port;
} udp_announce_req_t;

typedef struct __attribute__((packed)) {
  uint32_t action;
  uint32_t transaction_id;
  uint32_t interval;
  uint32_t leechers;
  uint32_t seeders;
} udp_announce_res_header_t;

typedef struct __attribute__((packed)) {
  uint32_t action;
  uint32_t transaction_id;
  char message[];
} udp_announce_err_header_t;

uint32_t new_transaction_id() {
  unsigned int seed = time(NULL);
  return rand_r(&seed);
}

int udp_send_dgram(int sockfd, char *buf, size_t buf_len) {
  ssize_t bytes_sent = send(sockfd, buf, buf_len, 0);
  if (bytes_sent < 0)
    return -1;

  assert(bytes_sent == (ssize_t)buf_len);
  return 0;
}

int udp_recv_dgram(int sockfd, time_t timeout, char *buf, size_t buf_len,
                   size_t *dgram_size) {
  struct timeval tv;
  tv.tv_sec = timeout;
  tv.tv_usec = 0;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  size_t n = recv(sockfd, buf, buf_len, 0);
  if (n < 0) {
    log_printf(LOG_ERROR, "Could not read dgram from tracker: %s\n",
               strerror(errno));
    return -1;
  }

  *dgram_size = n;
  return 0;
}

int udp_tryconnect(int sockfd, time_t timeout, uint32_t transaction_id,
                   udp_connect_res_t *out, size_t *outlen) {
  udp_connect_req_t req;
  req.action = htonl(ACTION_CONNECT);
  req.connection_id = htobe64(PROTOCOL_ID);
  req.transaction_id = transaction_id;
  assert(sizeof(req) == sizeof(uint64_t) + 2 * sizeof(uint32_t));

  if (udp_send_dgram(sockfd, (char *)&req, sizeof(req)) < 0) {
    return -1;
  }

  if (udp_recv_dgram(sockfd, timeout, (char *)out, sizeof(udp_connect_res_t),
                     outlen) < 0) {
    return -1;
  }

  return 0;
}

int udp_tryannounce(int sockfd, time_t timeout, udp_announce_req_t *req,
                    char *out, size_t *outlen) {

  if (udp_send_dgram(sockfd, (char *)req, sizeof(*req)) < 0) {
    return -1;
  };

  if (udp_recv_dgram(sockfd, timeout, out, MAX_RECV_BUFSIZE, outlen) < 0) {
    return -1;
  }

  return 0;
}

time_t timeout(int n) { return 30 * (1 << n); }

void fill_announce_request(tracker_request_t *req, udp_announce_req_t *out,
                           uint64_t connection_id, uint32_t transaction_id) {
  out->connection_id = connection_id;
  out->transaction_id = transaction_id;
  out->action = htonl(ACTION_ANNOUNCE);
  out->left = htonl(req->left);
  out->key = 0;
  out->port = htonl(req->port);
  out->event = htonl(req->event);
  memcpy(out->peer_id, req->peer_id, 20);
  memcpy(out->info_hash, req->info_hash, 20);
  out->ip_address = 0;
  out->downloaded = htonl(req->downloaded);
  out->uploaded = htonl(req->uploaded);
  out->num_want = htonl(50);
}

tracker_response_t *udp_announce(int sockfd, tracker_request_t *req) {
  int n = 0;
  uint32_t transaction_id = new_transaction_id();
  udp_connect_res_t res;
  size_t dgram_size;

reconnect:
  while (udp_tryconnect(sockfd, timeout(n++), transaction_id, &res,
                        &dgram_size) < 0) {
    if (n == 8) {
      log_printf(LOG_ERROR,
                 "Could not connect to tracker: Too many attempts\n");
      return NULL;
    }

    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      log_printf(LOG_ERROR, "Could not connect to UDP tracker\n");
      return NULL;
    }
  }

  n = 0;
  time_t request_time = time(NULL);

  assert(dgram_size == sizeof(res));
  log_printf(LOG_DEBUG, "Connection [%lu] to UDP tracker estabilished\n",
             res.connection_id);

  if (transaction_id != res.transaction_id) {
    log_printf(LOG_ERROR,
               "Transaction id returned from tracker differs from sent. Sent = "
               "%d, Received = %d\n",
               transaction_id, res.transaction_id);

    return NULL;
  }

  if (res.action != ACTION_CONNECT) {
    log_printf(LOG_ERROR, "Action received is not ACTION_CONNECT\n");
    return NULL;
  }

  udp_announce_req_t announce_req;
  fill_announce_request(req, &announce_req, res.connection_id, transaction_id);

  union {
    udp_announce_res_header_t header;
    udp_announce_err_header_t err_header;
    char all[MAX_RECV_BUFSIZE];
  } announce_response = {0};

  while (udp_tryannounce(sockfd, timeout(n++), &announce_req,
                         announce_response.all, &dgram_size)) {
    if (n == 8) {
      log_printf(LOG_ERROR,
                 "Could not connect to tracker: Too many attempts\n");
      return NULL;
    }

    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      log_printf(LOG_ERROR, "Could not connect to UDP tracker\n");
      return NULL;
    }

    if (CONNECTION_EXPIRED(request_time)) {
      goto reconnect;
    }
  }

  if (dgram_size < 20) {
    log_printf(LOG_ERROR, "Invalid dgram size received from tracker\n");
    return NULL;
  }

  if (ntohl(announce_response.header.action) == ACTION_ERROR) {
    log_printf(LOG_ERROR, "Received error from the tracker: %.*s\n",
               dgram_size - sizeof(udp_announce_err_header_t),
               announce_response.err_header.message);
    return NULL;
  }

  if (announce_response.header.transaction_id != transaction_id) {
    log_printf(LOG_ERROR,
               "Transaction id returned from tracker differs from sent. Sent = "
               "%d, Received = %d\n",
               transaction_id, res.transaction_id);
    return NULL;
  }

  log_printf(LOG_INFO, "Successfully connected to UDP tracker\n");

  tracker_response_t *response = malloc(sizeof(tracker_response_t));
  response->complete = ntohl(announce_response.header.seeders);
  response->incomplete = ntohl(announce_response.header.leechers);
  response->failure_reason = NULL;
  response->warning_message = NULL;
  response->interval = ntohl(announce_response.header.interval);
  response->tracker_id = NULL;

  size_t peer_buf_len = dgram_size - sizeof(announce_response.header);
  assert(peer_buf_len % 6 == 0);
  response->num_peers = peer_buf_len / 6;
  response->peers =
      parse_peers(announce_response.all + sizeof(announce_response.header),
                  response->num_peers);

  return response;
}
