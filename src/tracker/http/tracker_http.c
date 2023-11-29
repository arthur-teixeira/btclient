#include "tracker_http.h"
#include "../../deps/stb_bencode.h"
#include "../peer_parser.h"
#include "../tracker_announce.h"
#include <ctype.h>
#include <curl/curl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define HT_LOOKUP(ht, key) hash_table_lookup(ht, key, strlen(key))

tracker_response_t *http_announce(int sockfd, url_t *url,
                                  tracker_request_t *req) {
  char req_buf[1024] = {0};

  size_t req_size = build_http_request(url, req, req_buf, sizeof(req_buf));
  log_printf(LOG_DEBUG, "\nBEGIN HTTP REQUEST\n%s\nEND HTTP REQUEST\n",
             req_buf);
  write(sockfd, req_buf, req_size);

  char buf[2048] = {0};
  size_t n;
  size_t total_received = 0;
  do {
    n = recv(sockfd, buf + total_received, sizeof(buf) - total_received, 0);
    if (n < 0) {
      log_printf(LOG_ERROR, "Could not read from stream\n");
      return NULL;
    }

    total_received += n;
  } while (n > 0);

  log_printf(LOG_DEBUG, "\nBEGIN HTTP RESPONSE\n%s\nEND HTTP RESPONSE\n", buf);
  assert(total_received < 2048);

  tracker_response_t *res = parse_tracker_response(buf);

  return res;
}

size_t build_http_request(url_t *url, tracker_request_t *req, char *buff,
                          size_t bufsize) {
  size_t written = 0;

  written += snprintf(buff, bufsize, "GET /%s", url->path);

  written += snprintf(buff + written, bufsize - written, "?info_hash=");
  char *info_hash_escaped =
      curl_easy_escape(NULL, req->info_hash, sizeof(req->info_hash));
  written +=
      snprintf(buff + written, bufsize - written, "%s", info_hash_escaped);
  curl_free(info_hash_escaped);

  written += snprintf(buff + written, bufsize - written, "&peer_id=");
  char *peer_id_escaped =
      curl_easy_escape(NULL, req->peer_id, sizeof(req->peer_id));
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

tracker_response_t *parse_tracker_response(char *buf) {
  char *saveptr;
  char *line = strtok_r(buf, "\n", &saveptr);

  if (strncmp(buf, "HTTP/1.1 200 OK", strlen("HTTP/1.1 200 OK")) != 0 &&
      strncmp(buf, "HTTP/1.0 200 OK", strlen("HTTP/1.0 200 OK")) != 0) {
    log_printf(LOG_ERROR, "Tracker responded with malformed http response\n");
    return NULL;
  }

  strtok_r(NULL, "\n", &saveptr);

  size_t content_length = 0;

  char *saveptrtoken;
  do {
    line = strtok_r(NULL, "\n", &saveptr);
    if (strncmp(line, "Content-Length:", strlen("Content-Length:")) == 0) {
      char *token = strtok_r(line, ":", &saveptrtoken);
      token = strtok_r(NULL, ":", &saveptrtoken);
      content_length = strtoul(token, NULL, 10);
    }
  } while (strlen(line) != 1);

  char *content = line + strlen(line) + 1;

  return parse_content(content_length, content);
}

tracker_response_t *parse_content(size_t content_length, char *buf) {
  tracker_response_t *res = malloc(sizeof(tracker_response_t));
  Lexer l = {0};
  l.bufsize = content_length;
  l.buf = buf;

  Parser p = {0};
  p.l = l;
  parser_next_token(&p);
  parser_next_token(&p);

  hash_table_t dict = parse_item(&p).asDict;
  BencodeType *tracker_id = HT_LOOKUP(&dict, "tracker id");
  if (tracker_id) {
    res->tracker_id = tracker_id->asString.str;
  }

  BencodeType *failure_reason = HT_LOOKUP(&dict, "failure reason");
  if (failure_reason) {
    log_printf(LOG_ERROR, "Error on tracker response: %s\n",
               failure_reason->asString.str);
    res->failure_reason = failure_reason->asString.str;
  }

  BencodeType *warning_message = HT_LOOKUP(&dict, "warning message");
  if (warning_message) {
    log_printf(LOG_WARNING, "Tracker response warning: %\n",
               warning_message->asString.str);
    res->warning_message = warning_message->asString.str;
  }

  res->interval = ((BencodeType *)HT_LOOKUP(&dict, "interval"))->asInt;
  res->complete = ((BencodeType *)HT_LOOKUP(&dict, "complete"))->asInt;
  res->incomplete = ((BencodeType *)HT_LOOKUP(&dict, "incomplete"))->asInt;
  BencodeType *peers = HT_LOOKUP(&dict, "peers");

  assert(peers->asString.len % 6 == 0);
  res->num_peers = peers->asString.len / 6;
  res->peers = parse_peers(peers->asString.str, res->num_peers);

  return res;
}
