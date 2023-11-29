#include "tracker_https.h"
#include "../http/tracker_http.h"
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <unistd.h>

tracker_response_t *https_announce(url_t *url, tracker_request_t *req) {
  SSL_library_init();

  SSL_CTX *ctx = SSL_CTX_new(SSLv23_client_method());
  if (!ctx) {
    log_printf(LOG_ERROR, "Could not create SSL context\n");
    return NULL;
  }

  char port[strlen(url->host) + 6];
  int nb = snprintf(port, sizeof(port), "%s:%hu", url->host, url->port);
  port[nb] = '\0';

  BIO *bio = BIO_new_ssl_connect(ctx);
  BIO_set_conn_hostname(bio, port);

  if (BIO_do_connect(bio) <= 0) {
    log_printf(LOG_ERROR, "Could not connect to tracker\n");
    return NULL;
  }
  log_printf(LOG_INFO, "Connected to HTTPS tracker\n");

  char req_buf[1024] = {0};
  size_t req_size = build_http_request(url, req, req_buf, sizeof(req_buf));
  log_printf(LOG_DEBUG, "\nBEGIN HTTPS REQUEST\n%s\nEND HTTPS REQUEST\n",
             req_buf);

retry:
  if (BIO_write(bio, req_buf, req_size <= 0)) {
    log_printf(LOG_ERROR, "Could not write to HTTPS tracker\n");
    if (BIO_should_retry(bio)) {
      log_printf(LOG_DEBUG, "Retrying write...\n");
      sleep(5);
      goto retry;
    }

    return NULL;
  }

  char buf[2048] = {0};
  size_t n;
  size_t total_received = 0;
  do {
    n = BIO_read(bio, buf + total_received, sizeof(buf) - total_received);
    if (n < 0) {
      log_printf(LOG_ERROR, "Could not read from stream\n");
      return NULL;
    }

    total_received += n;
  } while (n > 0);

  log_printf(LOG_DEBUG, "\nBEGIN HTTPS RESPONSE\n%s\nEND HTTPS RESPONSE\n",
             buf);
  assert(total_received < 2048);

  BIO_free_all(bio);
  SSL_CTX_free(ctx);

  tracker_response_t *res = parse_tracker_response(buf);
  return res;
}
