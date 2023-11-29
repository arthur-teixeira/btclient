#ifndef TRACKER_HTTP_H
#define TRACKER_HTTP_H

#include "../tracker_announce.h"
#include "../../url/url.h"

tracker_response_t *http_announce(int sockfd, url_t *url, tracker_request_t *req);
size_t build_http_request(url_t *url, tracker_request_t *req, char *buff, size_t bufsize);
tracker_response_t *parse_content(size_t content_length, char *buf);
tracker_response_t *parse_tracker_response(char *buf);

#endif // !TRACKER_HTTP_H
