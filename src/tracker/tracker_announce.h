#ifndef TRACKER_ANNOUNCE_H
#define TRACKER_ANNOUNCE_H
#include "../log/log.h"
#include "../url/url.h"
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef enum {
  EVENT_STARTED,
  EVENT_STOPPED,
  EVENT_COMPLETED,
} tracker_event_t;

enum {
  NO_PEER_ID = (1 << 0),
  COMPACT = (1 << 1),
};

#define SET_FLAG(_ptr, _flag) (_ptr->flags |= _flag)
#define CLEAR_FLAG(_ptr, _flag) (_ptr->flags &= ~(_flag))
#define HAS_FLAG(_ptr, _flag) !!(_ptr->flags &= _flag)

typedef struct {
  uint8_t flags;
  char info_hash[20];
  char peer_id[20];
  uint16_t port;
  size_t uploaded;
  size_t downloaded;
  size_t left;
  tracker_event_t event;
} tracker_request_t;

int tracker_connect(url_t *url, tracker_request_t *request);

#endif // TRACKER_ANNOUNCE_H
