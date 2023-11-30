#include "file-parser/file-parser.h"
#include "log/log.h"
#include "peer-connection/peer-connection.h"
#include "peer-id/peer-id.h"
#include "tracker/tracker_announce.h"
#include "tracker/tracker_request.h"
#include "url/url.h"
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define TRACKER_RETRY_INTERVAL 15

int create_peer_connection(peer_t *peer, metainfo_t *torrent) {
  peer_connection_t conn;
  conn.peer = *peer;

  peer_arg_t *arg = malloc(sizeof(peer_arg_t));
  if (!arg) {
    return -1;
  }

  arg->torrent = torrent;
  arg->peer = *peer;
  arg->sockfd = -1;

  if (peer_connection_create(&conn.thread, arg) < 0) {
    log_printf(LOG_ERROR, "Could not create peer thread\n");
    free(arg);

    return -1;
  }

  pthread_mutex_lock(&torrent->sh.sh_lock);
  if (torrent->max_peers == torrent->sh.num_connections) {
    pthread_mutex_unlock(&torrent->sh.sh_lock);
    return 0;
  }

  torrent->sh.peer_connections[torrent->sh.num_connections++] = conn;

  pthread_mutex_unlock(&torrent->sh.sh_lock);

  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("usage: %s [file name]\n", argv[0]);
    return 0;
  }
  srand(time(NULL));
  create_peer_id();
  log_set_logfile(stderr);
  log_set_lvl(LOG_DEBUG);

  metainfo_t file = parse_file(argv[1]);

  url_t announce_url = file.announce ? url_from_string(file.announce)
                                     : url_from_string(file.announce_list[0]);

  bool started = false;
  bool completed = false;
  unsigned interval;
  while (true) {
    tracker_request_t *req = build_tracker_announce_request(&file);
    if (!started) {
      req->event = EVENT_STARTED;
      started = true;
    }

    tracker_response_t *res = tracker_announce(&announce_url, req);
    if (res) {
      log_printf(LOG_DEBUG, "Tracker interval is %ld\n", res->interval);
      interval = res->interval;
      for (size_t i = 0; i < res->num_peers; i++) {
        create_peer_connection(&res->peers[i], &file);
      }
    } else {
      interval = TRACKER_RETRY_INTERVAL;
      log_printf(LOG_INFO, "Retrying announce to tracker in %d seconds\n",
                 interval);
    }

    free(req);
    if (res) {
      free(res);
    }

    log_printf(LOG_DEBUG, "Sleeping for %ld\n", interval);
    sleep(interval);
  }

  return 0;
}
