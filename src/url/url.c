#include "url.h"
#include <stdlib.h>
#include <string.h>

url_t url_from_string(char *input) {
  url_t url = {0};

  char buf[strlen(input) + 1];
  strcpy(buf, input);

  if (strncmp("http:", buf, 5) == 0) {
    url.protocol = PROTOCOL_HTTP;
  } else if (strncmp("https:", buf, 6) == 0) {
    url.protocol = PROTOCOL_HTTPS;
  } else if (strncmp("udp://", buf, 6) == 0) {
    url.protocol = PROTOCOL_UDP;
  } else {
    url.protocol = PROTOCOL_UNKNOWN;
  }

  char *save_ptr;

  const char *host = strtok_r(buf, ":/", &save_ptr);
  host = strtok_r(NULL, ":/", &save_ptr);
  url.host = strdup(host);

  input += strlen(host) + (host - buf);

  if (strncmp(":", input, 1) == 0) {
    const char *port = strtok_r(NULL, ":/", &save_ptr);
    url.port = atoi(port);
  } else if (url.protocol == PROTOCOL_HTTP) {
    url.port = 80;
  } else if (url.protocol == PROTOCOL_HTTPS) {
    url.port = 443;
  }

  const char *path = strtok_r(NULL, ":/", &save_ptr);
  url.path = strdup(path ? path : "");

  return url;
}

void free_url(url_t *url) {
  free(url->path);
  free(url->host);
}
