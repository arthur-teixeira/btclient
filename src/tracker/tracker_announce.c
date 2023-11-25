#include "tracker_announce.h"
#include "tracker_http.h"

int tracker_connect(url_t *url, tracker_request_t *request) {
  log_printf(LOG_INFO, "parsing url\n");

  log_printf(LOG_INFO, "Getting info from %s\n", url->host);

  struct addrinfo hints, *head;
  memset(&hints, 0, sizeof(struct addrinfo));

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

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
    log_printf(LOG_INFO, "Opening socket\n");
    sockfd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
    if (sockfd < 0) {
      continue;
    }

    log_printf(LOG_INFO, "Attempting connection\n");
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

  log_printf(LOG_INFO, "Succesfully connected to host\n");
  errno = 0;

  freeaddrinfo(head);

  switch (url->protocol) {
  case PROTOCOL_HTTP:
    return http_announce(sockfd, url, request);
  case PROTOCOL_UDP:
  case PROTOCOL_HTTPS:
    assert(0 && "TODO");
  case PROTOCOL_UNKNOWN:
    log_printf(LOG_ERROR, "Unknown protocol for tracker %s\n", url->host);
    return -1;
  }

fail:
  freeaddrinfo(head);
  return -1;
}
