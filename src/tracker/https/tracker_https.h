#ifndef TRACKER_HTTPS_H
#define TRACKER_HTTPS_H

#include "../tracker_announce.h"
#include "../../url/url.h"

tracker_response_t *https_announce(url_t *url, tracker_request_t *req);

#endif // !TRACKER_HTTPS_H
