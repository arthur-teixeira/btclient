#ifndef URL_H
#define URL_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
typedef enum {
  PROTOCOL_UNKNOWN = -1,
  PROTOCOL_HTTP,
  PROTOCOL_HTTPS,
  PROTOCOL_UDP,
} protocol_t;

typedef struct {
  protocol_t protocol;
  char *host;
  uint16_t port;
  char *path;
} url_t;

url_t url_from_string(char *input);
void free_url(url_t *url);

#endif // URL_H
