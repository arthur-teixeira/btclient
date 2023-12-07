#include "byte_str.h"

byte_str_t *byte_str_new(size_t size, uint8_t *str) {
  byte_str_t *s = malloc(sizeof(byte_str_t) + size + 1);
  if (!s) {
    return s;
  }

  memcpy(s->str, str, size);
  s->str[size] = '\0';
  s->size = size;

  return s;
}

void byte_str_free(byte_str_t *str) { free(str); }
