#include "file-parser/file-parser.h"
#include "log/log.h"
#include "peer-id/peer-id.h"
#include "tracker/tracker_announce.h"
#include "tracker/tracker_request.h"
#include "url/url.h"
#include <stdio.h>
#include <time.h>

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

  tracker_request_t *req = build_tracker_announce_request(&file);

  tracker_announce(&announce_url, req);
  return 0;
}
