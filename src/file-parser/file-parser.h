#ifndef FILE_PARSER_H
#define FILE_PARSER_H

#include "../dl-file/dl_file.h"
#include "../tracker/tracker_announce.h"
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <pthread.h>
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
  PIECE_STATE_NOT_REQUESTED,
  PIECE_STATE_REQUESTED,
  PIECE_STATE_HAVE
} piece_state_t;

typedef enum {
    TORRENT_STATE_LEECHING,
    TORRENT_STATE_SEEDING
} torrent_state_t;

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
  peer_t peer;
  pthread_t thread;
} peer_connection_t;

typedef struct {
    size_t len;
    size_t cap;
    peer_connection_t *values;
} peer_connections_t;

typedef struct metainfo_t {
  char *announce;
  size_t announce_list_size;
  char **announce_list;
  info_t info;
  char info_hash[SHA_DIGEST_LENGTH];
  size_t max_peers;
  struct {
    torrent_state_t state;
    pthread_mutex_t sh_lock;
    peer_connections_t *peer_connections;
    char *piece_states;
    size_t pieces_left;
    bool completed;
  } sh;
  dl_file_t **files;
} metainfo_t;

#define HT_LOOKUP(ht, key) hash_table_lookup(ht, key, strlen(key))
#define HT_DELETE(ht, key) hash_table_delete(ht, key, strlen(key))

#define MAX_BUFSIZE 2048

metainfo_t parse_file(char *filename);

#endif // FILE_PARSER_H
