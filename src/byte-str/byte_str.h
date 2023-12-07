#ifndef BYTE_STR_H
#define BYTE_STR_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  size_t size;
  uint8_t str[];
} byte_str_t;

byte_str_t *byte_str_new(size_t size, uint8_t *str);
void byte_str_free(byte_str_t *str);

#endif // BYTE_STR_H
