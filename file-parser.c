#include "deps/stb_hashtable.h"
#include <openssl/err.h>
#include <openssl/evp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char *compute_info_hash(const char *buf, size_t start, size_t end);
#define BENCODE_GET_SHA1(a, b, c) compute_info_hash(a, b, c)
#define BENCODE_HASH_INFO_DICT
#define BENCODE_IMPLEMENTATION
#include "deps/stb_bencode.h"

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
  char *pieces;
  char *name;
  bool private;
  size_t length;
  size_t files_count;
  file_info_t *files;
} info_t;

typedef struct {
  char *announce;
  size_t announce_list_size;
  char **announce_list;
  char *encoding;
  info_t info;
  uint8_t *info_hash;
} metainfo_t;

#define HT_LOOKUP(ht, key) hash_table_lookup(ht, key, strlen(key))
#define HT_DELETE(ht, key) hash_table_delete(ht, key, strlen(key))

#define MAX_BUFSIZE 2048

unsigned char *compute_info_hash(const char *in_buf, size_t start, size_t end) {
  OpenSSL_add_all_algorithms();
  ERR_load_crypto_strings();
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  const EVP_MD *hashptr = EVP_get_digestbyname("SHA1");

  EVP_DigestInit(ctx, hashptr);

  unsigned char buf[end - start + 1];

  for (size_t i = start, j = 0; i <= end; i++, j++) {
    buf[j] = in_buf[i];
  }

  EVP_DigestUpdate(ctx, buf, end - start + 1);

  unsigned int outlen;
  EVP_DigestFinal_ex(ctx, buf, &outlen);
  EVP_MD_CTX_free(ctx);

  unsigned char *outbuf = malloc(outlen);
  memcpy(outbuf, buf, outlen);
  printf("\n");

  return outbuf;
}

info_t parse_info(hash_table_t *parsed) {
  BencodeType *private = HT_LOOKUP(parsed, "private");
  info_t info;

  if (private) {
    info.private = private->asInt;
  } else {
    info.private = false;
  }

  BencodeType *piece_length = HT_LOOKUP(parsed, "piece length");
  info.piece_length = piece_length->asInt;

  BencodeType *pieces = HT_LOOKUP(parsed, "pieces");
  info.pieces = pieces->asString.str;

  BencodeType *name = HT_LOOKUP(parsed, "name");
  info.name = name->asString.str;

  BencodeType *length = HT_LOOKUP(parsed, "length");
  if (length) {
    info.mode = INFO_SINGLE;
    info.length = length->asInt;
  } else {
    info.mode = INFO_MULTI;
    info.length = 0;

    BencodeType *files = HT_LOOKUP(parsed, "files");
    BencodeList files_list = files->asList;
    info.files_count = files_list.len;
    info.files = calloc(info.files_count, sizeof(file_info_t));

    for (size_t i = 0; i < files_list.len; i++) {
      BencodeType *length = HT_LOOKUP(&files_list.values[i].asDict, "length");
      info.files[i].length = length->asInt;
      HT_DELETE(&files_list.values[i].asDict, "length");
      BencodeType *path = HT_LOOKUP(&files_list.values[i].asDict, "path");

      info.files[i].path_size = path->asList.len;
      info.files[i].path = calloc(info.files[i].path_size, sizeof(char *));
      for (size_t j = 0; j < path->asList.len; j++) {
        info.files[i].path[j] = strdup(path->asList.values[j].asString.str);
        free(path->asList.values[j].asString.str);
      }
    }
  }

  return info;
}

void parse_file(char *filename) {
  Lexer l = new_lexer(filename);
  Parser p = new_parser(l);
  hash_table_t parsed = parse_item(&p).asDict;

  metainfo_t metainfo;
  BencodeType *announce = HT_LOOKUP(&parsed, "announce");
  if (!announce) {
    fprintf(stderr, "ERROR: could not find announce in the torrent file");
    exit(EXIT_FAILURE);
  }

  metainfo.announce = announce->asString.str;
  HT_DELETE(&parsed, "announce");

  BencodeType *announce_list = HT_LOOKUP(&parsed, "announce-list");
  if (announce_list) {
    metainfo.announce_list = calloc(announce_list->asList.len, sizeof(void *));
    metainfo.announce_list_size = announce_list->asList.len;
    for (size_t i = 0; i < announce_list->asList.len; i++) {
      metainfo.announce_list[i] =
          announce_list->asList.values[i].asList.values[0].asString.str;

      free(announce_list->asList.values[i].asList.values);
    }

    free(announce_list->asList.values);
    HT_DELETE(&parsed, "announce-list");
  }

  BencodeType *enc = HT_LOOKUP(&parsed, "encoding");
  if (enc) {
    metainfo.encoding = enc->asString.str;
  }

  BencodeType *info = HT_LOOKUP(&parsed, "info");
  printf("info info_hash:\n");
  for (size_t i = 0; i < 20; ++i)
    printf("%02x", info->sha1_digest[i]);
  printf("\n");

  metainfo.info = parse_info(&info->asDict);

  printf("announce = %s\n", metainfo.announce);
#if 0
  printf("announce_list = \n");
  for (size_t i = 0; i < metainfo.announce_list_size; i++) {
    printf("%s\n", metainfo.announce_list[i]);
  }
#endif
}

int main() {
  parse_file("../bencode-parser/test-files/"
             "5B3C4E3CE058F566F9AB3EE8458E9C845FF619FB.torrent");

  return 0;
}
