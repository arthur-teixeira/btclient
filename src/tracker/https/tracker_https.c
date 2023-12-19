#include "tracker_https.h"
#include "../http/tracker_http.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  char data[4096];
  size_t size;
} curl_response_t;

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  curl_response_t *res = (curl_response_t *)userdata;
  res->size = size * nmemb;
  assert(res->size <= 4096);
  memcpy(res->data, ptr, res->size);

  return res->size;
}

tracker_response_t *https_announce(url_t *url, tracker_request_t *req) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    log_printf(LOG_ERROR, "Could not create Curl instance\n");
    return NULL;
  }

  char req_buf[1024];
  int nb = snprintf(req_buf, sizeof(req_buf), "https://%s:%hu/%s", url->host,
                    url->port, url->path);
  nb += build_http_url(req, req_buf + nb, sizeof(req_buf) - nb);
  req_buf[nb] = 0;
  curl_response_t response = {0};

  curl_easy_setopt(curl, CURLOPT_STDERR, log_file);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl, CURLOPT_URL, req_buf);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 604800L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    log_printf(LOG_ERROR, "Request failed: %s\n", curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return NULL;
  }

  curl_global_cleanup();

  return parse_content(response.size, response.data);
}
