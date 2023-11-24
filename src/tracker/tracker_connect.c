#include "../log/log.h"
#include "../url/url.h"
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void tracker_connect() {
  log_set_logfile(stderr);
  log_set_lvl(LOG_DEBUG);

  // url_t url =
  // url_from_string("https://btclient-test.free.beeceptor.com/test");
  log_printf(LOG_INFO, "parsing url\n");
  url_t url = url_from_string("http://0.0.0.0:8000");

  log_printf(LOG_INFO, "Getting info from %s\n", url.host);

  struct addrinfo hints, *head;
  memset(&hints, 0, sizeof(struct addrinfo));

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  char port[6];
  int n = snprintf(port, sizeof(port), "%hu", url.port);
  port[n] = '\0';

  if (getaddrinfo(url.host, port, &hints, &head) != 0) {
    log_printf(LOG_ERROR, "Could not get info from %s\n", url.host);
    return;
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

  char buf[1024];
  write(sockfd, "GET / HTTP/1.1\r\n", strlen("GET / HTTP/1.1\r\n"));
  bzero(buf, 1024);

  while (read(sockfd, buf, 1023) != 0) {
    fprintf(stderr, "%s", buf);
    bzero(buf, 1024);
  }

  shutdown(sockfd, SHUT_RDWR);
  close(sockfd);

  return;
fail:
  freeaddrinfo(head);
  return;
}
