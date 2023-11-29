#include "tracker_announce.h"
#include "http/tracker_http.h"
#include "https/tracker_https.h"
#include "udp/tracker_udp.h"
#include <sys/socket.h>

int tracker_connect(url_t *url) {
  log_printf(LOG_DEBUG, "Getting info from %s\n", url->host);
  struct addrinfo hints, *head;
  memset(&hints, 0, sizeof(struct addrinfo));

  hints.ai_family = AF_INET;
  if (url->protocol == PROTOCOL_UDP) {
    hints.ai_socktype = SOCK_DGRAM;
  } else {
    hints.ai_socktype = SOCK_STREAM;
  }

  char port[6];
  int n = snprintf(port, sizeof(port), "%hu", url->port);
  port[n] = '\0';

  if (getaddrinfo(url->host, port, &hints, &head) != 0) {
    log_printf(LOG_ERROR, "Could not get info from %s\n", url->host);
    return -1;
  }

  struct addrinfo *cur;
  int sockfd;
  for (cur = head; cur; cur = cur->ai_next) {
    log_printf(LOG_DEBUG, "Opening socket\n");
    sockfd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
    if (sockfd < 0) {
      continue;
    }

    log_printf(LOG_DEBUG, "Attempting connection\n");
    int conn_result = connect(sockfd, cur->ai_addr, cur->ai_addrlen);
    if (conn_result < 0) {
      close(sockfd);
      continue;
    }

    break;
  }

  if (!cur) {
    log_printf(LOG_ERROR, "Could not connect to host: %s\n", strerror(errno));
    goto fail;
  }

  log_printf(LOG_DEBUG, "Succesfully connected to host\n");
  errno = 0;

  freeaddrinfo(head);
  return sockfd;

fail:
  freeaddrinfo(head);
  return -1;
}

tracker_response_t *tracker_announce(url_t *url, tracker_request_t *req) {
  if (url->protocol == PROTOCOL_HTTPS) {
    return https_announce(url, req);
  }

  int sockfd = tracker_connect(url);
  if (sockfd < 0) {
    return NULL;
  }

  tracker_response_t *res;
  switch (url->protocol) {
  case PROTOCOL_HTTP:
    res = http_announce(sockfd, url, req);
    break;
  case PROTOCOL_UDP:
    res = udp_announce(sockfd, req);
    break;
  case PROTOCOL_HTTPS:
    assert(0 && "unreachable");
  case PROTOCOL_UNKNOWN:
    log_printf(LOG_ERROR, "Unknown protocol for tracker %s\n", url->host);
    return NULL;
  }

  shutdown(sockfd, SHUT_RDWR);
  close(sockfd);

  return res;
}
