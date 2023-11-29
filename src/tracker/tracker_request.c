#include "tracker_request.h"
#include "tracker_announce.h"
#include <string.h>

size_t multi_left(info_t info) {
  size_t result = 0;
  for (size_t i = 0; i < info.files_count; i++) {
    result += info.files[i].length;
  }

  return result;
}

tracker_request_t *build_tracker_announce_request(metainfo_t *metainfo) {
  tracker_request_t *req = malloc(sizeof(tracker_request_t));
  if (!req)
    return NULL;

  memset(req, 0, sizeof(tracker_request_t));

  memcpy(req->info_hash, metainfo->info_hash, sizeof(metainfo->info_hash));
  memcpy(req->peer_id, peer_id, sizeof(peer_id));

  switch (metainfo->info.mode) {
  case INFO_SINGLE:
    req->left = metainfo->info.length;
    break;
  case INFO_MULTI:
    req->left = multi_left(metainfo->info);
    break;
  }
  req->port = 6889;
  req->uploaded = 0;
  req->downloaded = 0;
  SET_FLAG(req, COMPACT);

  return req;
}

void tracker_request_free(tracker_request_t *req) { free(req); }
