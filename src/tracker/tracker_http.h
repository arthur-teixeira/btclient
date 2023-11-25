#ifndef TRACKER_HTTP_H
#define TRACKER_HTTP_H

#include "tracker_announce.h"
#include "../url/url.h"

int http_announce(int sockfd, url_t *url, tracker_request_t *request);
size_t build_http_request(url_t *url, tracker_request_t *req, char *buff, size_t bufsize);

#endif // !TRACKER_HTTP_H
