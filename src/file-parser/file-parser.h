#ifndef FILE_PARSER_H
#define FILE_PARSER_H

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char *compute_info_hash(const char *buf, size_t start, size_t end);
#define BENCODE_GET_SHA1(a, b, c) compute_info_hash(a, b, c)
#define BENCODE_HASH_INFO_DICT
#include "../deps/stb_bencode.h"

typedef uint8_t piece_hash[SHA_DIGEST_LENGTH];

typedef enum {
  INFO_SINGLE,
  INFO_MULTI,
} info_mode_t;

typedef struct {
  size_t length;
  size_t path_size;
  char **path;
} file_info_t;

typedef struct {
  info_mode_t mode;
  size_t piece_length;
  char *name;
  size_t num_pieces;
  piece_hash *pieces;
  size_t length;
  size_t files_count;
  file_info_t *files;
} info_t;

typedef struct {
  char *announce;
  size_t announce_list_size;
  char **announce_list;
  info_t info;
  char info_hash[SHA_DIGEST_LENGTH * 2];
} metainfo_t;

#define HT_LOOKUP(ht, key) hash_table_lookup(ht, key, strlen(key))
#define HT_DELETE(ht, key) hash_table_delete(ht, key, strlen(key))

#define MAX_BUFSIZE 2048

metainfo_t parse_file(char *filename);

#endif // FILE_PARSER_H
