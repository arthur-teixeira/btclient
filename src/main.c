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

  log_printf(LOG_INFO, "parsing file %s\n", argv[1]);
  metainfo_t file = parse_file(argv[1]);

  log_printf(LOG_INFO, "Parsing announce url\n");
  url_t announce_url =
      url_from_string("http://tracker.opentrackr.org:1337/announce");
  // TODO: use url from file, not doing that now because we still don't support
  // UDP url_t announce_url = file.announce ? url_from_string(file.announce) :
  // url_from_string(file.announce_list[0]);

  log_printf(LOG_INFO, "Building request\n");
  tracker_request_t *req = build_tracker_announce_request(&file);

  log_printf(LOG_INFO, "Connecting \n");
  tracker_connect(&announce_url, req);
  return 0;
}
