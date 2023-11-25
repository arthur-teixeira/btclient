#include "file-parser.h"
#include <string.h>

#define BENCODE_IMPLEMENTATION
#include "../deps/stb_bencode.h"

piece_hash *split_piece_hashes(const char *buf, size_t len) {
  size_t hash_len = 20;
  if (len % hash_len != 0) {
    fprintf(stderr, "ERROR: malformed hashes\n");
    exit(EXIT_FAILURE);
  }

  size_t num_pieces = len / hash_len;

  piece_hash *out = calloc(num_pieces, sizeof(piece_hash));

  for (size_t i = 0; i < num_pieces; i++) {
    for (size_t j = i * hash_len, k = 0; j < (i + 1) * hash_len; j++, k++) {
      out[i][k] = buf[j];
    }
  }

  return out;
}

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

  return outbuf;
}

info_t parse_info(hash_table_t *parsed) {
  info_t info;
  BencodeType *piece_length = HT_LOOKUP(parsed, "piece length");
  info.piece_length = piece_length->asInt;

  BencodeType *pieces = HT_LOOKUP(parsed, "pieces");
  info.num_pieces = pieces->asString.len / 20;
  info.pieces = split_piece_hashes(pieces->asString.str, pieces->asString.len);

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

metainfo_t parse_file(char *filename) {
  Lexer l = new_lexer(filename);
  Parser p = new_parser(l);
  hash_table_t parsed = parse_item(&p).asDict;

  metainfo_t metainfo = {0};
  BencodeType *announce = HT_LOOKUP(&parsed, "announce");
  if (announce) {
    metainfo.announce = announce->asString.str;
    HT_DELETE(&parsed, "announce");
  }

  // https://www.bittorrent.org/beps/bep_0012.html
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

  BencodeType *info = HT_LOOKUP(&parsed, "info");

  metainfo.info = parse_info(&info->asDict);
  for (size_t i = 0; i < 20; i++) {
    sprintf((char *)&(metainfo.info_hash[i * 2]), "%02x", info->sha1_digest[i]);
  }

  return metainfo;
}
