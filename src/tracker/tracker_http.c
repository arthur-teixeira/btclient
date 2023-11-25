#include "tracker_http.h"
#include "tracker_announce.h"
#include <ctype.h>
#include <curl/curl.h>
#include <stdbool.h>
#include <sys/socket.h>

int http_announce(int sockfd, url_t *url, tracker_request_t *req) {
  char req_buf[1024] = {0};

  size_t req_size = build_http_request(url, req, req_buf, sizeof(req_buf));
  log_printf(LOG_DEBUG, "BEGIN HTTP REQUEST\n%s\nEND HTTP REQUEST\n", req_buf);
  write(sockfd, req_buf, req_size);

  char buf[2048] = {0};
  size_t n;
  size_t total_received = 0;
  do {
    n = recv(sockfd, buf + total_received, sizeof(buf) - total_received, 0);
    if (n < 0)
      return -1;

    total_received += n;
  } while (n > 0);

  fprintf(stderr, "%s", buf);

  shutdown(sockfd, SHUT_RDWR);
  close(sockfd);

  return 0;
}

size_t build_http_request(url_t *url, tracker_request_t *req, char *buff,
                          size_t bufsize) {
  size_t written = 0;

  written += snprintf(buff, bufsize, "GET /%s", url->path);

  written += snprintf(buff + written, bufsize - written, "?info_hash=");
  char *info_hash_escaped = curl_easy_escape(NULL, req->info_hash, 20);
  written += snprintf(buff + written, bufsize - written, "%s", info_hash_escaped);
  curl_free(info_hash_escaped);

  written += snprintf(buff + written, bufsize - written, "&peer_id=");
  char *peer_id_escaped = curl_easy_escape(NULL, req->peer_id, 20);
  written += snprintf(buff + written, bufsize - written, "%s", peer_id_escaped);
  curl_free(peer_id_escaped);

  written +=
      snprintf(buff + written, bufsize - written, "&port=%hu", req->port);
  written += snprintf(buff + written, bufsize - written, "&uploaded=%lu",
                      req->uploaded);
  written += snprintf(buff + written, bufsize - written, "&downloaded=%lu",
                      req->downloaded);
  written +=
      snprintf(buff + written, bufsize - written, "&left=%lu", req->left);

  if (HAS_FLAG(req, COMPACT)) {
    written += snprintf(buff + written, bufsize - written, "&compact=1");
  }
  written += snprintf(buff + written, bufsize - written, " HTTP/1.1\r\n");
  written += snprintf(buff + written, bufsize - written, "Host: %s\r\n\r\n",
                      url->host);

  return written;
}
