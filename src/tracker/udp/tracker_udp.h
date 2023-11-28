#ifndef TRACKER_UDP_H
#define TRACKER_UDP_H

#include "../tracker_announce.h"
#include "../../url/url.h"

tracker_response_t *udp_announce(int sockfd, tracker_request_t *req);

#endif // TRACKER_UDP_H
