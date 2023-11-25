#ifndef TRACKER_REQUEST_H
#define TRACKER_REQUEST_H

#include "../file-parser/file-parser.h"
#include "../peer-id/peer-id.h"
#include "tracker_announce.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

tracker_request_t *build_tracker_announce_request(metainfo_t *metainfo);
void tracker_request_free(tracker_request_t *req);

#endif // !TRACKER_REQUEST_H
