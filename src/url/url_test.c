#include "url.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <unity/unity.h>
#include <unity/unity_internals.h>

#define LEN(xs) sizeof(xs) / sizeof(*xs)

url_t result;

void setUp() {}

void tearDown() { free_url(&result); }

void test_http_url() {
  char *url = "http://tracker.opentrackr.org:1337/announce";

  result = url_from_string(url);

  TEST_ASSERT_EQUAL(PROTOCOL_HTTP, result.protocol);
  TEST_ASSERT_EQUAL_STRING("tracker.opentrackr.org", result.host);
  TEST_ASSERT_EQUAL(1337, result.port);
  TEST_ASSERT_EQUAL_STRING("announce", result.path);
}

void test_https_url() {
  char *url = "https://tracker.opentrackr.org:1337/announce";

  result = url_from_string(url);

  TEST_ASSERT_EQUAL(PROTOCOL_HTTPS, result.protocol);
  TEST_ASSERT_EQUAL_STRING("tracker.opentrackr.org", result.host);
  TEST_ASSERT_EQUAL(1337, result.port);
  TEST_ASSERT_EQUAL_STRING("announce", result.path);
}

void test_udp_url() {
  char *url = "udp://concen.org:6969/announce";

  result = url_from_string(url);

  TEST_ASSERT_EQUAL(PROTOCOL_UDP, result.protocol);
  TEST_ASSERT_EQUAL_STRING("concen.org", result.host);
  TEST_ASSERT_EQUAL(6969, result.port);
  TEST_ASSERT_EQUAL_STRING("announce", result.path);
}

void test_default_ports() {
  char *urls[] = {
      "http://tracker.opentrackr.org/announce",
      "https://tracker.opentrackr.org/announce",
  };

  uint16_t expected_ports[] = {
      80,
      443,
  };

  assert(LEN(urls) == LEN(expected_ports));
  for (size_t i = 0; i < LEN(urls); i++) {
    if (i == 1)
      tearDown();
    result = url_from_string(urls[i]);
    TEST_ASSERT_EQUAL(expected_ports[i], result.port);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_http_url);
  RUN_TEST(test_https_url);
  RUN_TEST(test_udp_url);
  RUN_TEST(test_default_ports);
  return UNITY_END();
}
