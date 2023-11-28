#ifndef PEER_PARSER_H
#define PEER_PARSER_H

#include "tracker_announce.h"

peer_t *parse_peers(const char *buf, size_t peer_count);

#endif // PEER_PARSER_H
