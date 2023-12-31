#include "file-parser.h"
#include <openssl/sha.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define BENCODE_IMPLEMENTATION
#include "../deps/stb_bencode.h"

piece_hash *split_piece_hashes(const char *buf, size_t len) {
  if (len % SHA_DIGEST_LENGTH != 0) {
    fprintf(stderr, "ERROR: malformed hashes\n");
    exit(EXIT_FAILURE);
  }

  size_t num_pieces = len / SHA_DIGEST_LENGTH;

  piece_hash *out = calloc(num_pieces, sizeof(piece_hash));

  for (size_t i = 0; i < num_pieces; i++) {
    for (size_t j = i * SHA_DIGEST_LENGTH, k = 0;
         j < (i + 1) * SHA_DIGEST_LENGTH; j++, k++) {
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
  info.num_pieces = pieces->asString.len / SHA_DIGEST_LENGTH;
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
  memcpy(metainfo.info_hash, info->sha1_digest, SHA_DIGEST_LENGTH);

  metainfo.files = calloc(metainfo.info.files_count, sizeof(filemem_t));

  pthread_mutex_init(&metainfo.sh.sh_lock, NULL);
  metainfo.max_peers = 50;
  metainfo.sh.piece_states = malloc(metainfo.info.num_pieces);
  memset(metainfo.sh.piece_states, PIECE_STATE_NOT_REQUESTED,
         metainfo.info.num_pieces);
  metainfo.sh.pieces_left = metainfo.info.num_pieces;
  metainfo.sh.state = TORRENT_STATE_LEECHING;
  metainfo.sh.completed = false;
  metainfo.sh.peer_connections = malloc(sizeof(peer_connections_t));
  da_init(metainfo.sh.peer_connections, sizeof(peer_connection_t));

  if (metainfo.info.mode == INFO_MULTI) {
    metainfo.files = calloc(metainfo.info.files_count, sizeof(dl_file_t *));
    for (size_t i = 0; i < metainfo.info.files_count; i++) {
      file_info_t *cur_file = &metainfo.info.files[i];
      char path[512];
      strcpy(path, "./");
      strcat(path, metainfo.info.name);
      strcat(path, "/");
      mkdir(path, 0777);

      for (size_t j = 0; j < cur_file->path_size; j++) {
        strcat(path, cur_file->path[j]);
        if (j < cur_file->path_size - 1) {
          mkdir(path, 0777);
          strcat(path, "/");
        }
      }
      log_printf(LOG_INFO, "Target file: %s\n", path);
      metainfo.files[i] = dl_file_create_and_open(cur_file->length, path);
    }
  } else {
    assert(metainfo.info.mode == INFO_SINGLE);
    char path[512];
    strcpy(path, "./");
    strcat(path, metainfo.info.name);
    metainfo.info.files_count = 1;
    metainfo.info.files =
        calloc(metainfo.info.files_count, sizeof(file_info_t));
    metainfo.info.files[0] = (file_info_t){
        .length = metainfo.info.length,
        .path = (char *[]){path},
    };

    metainfo.files = calloc(1, sizeof(peer_connections_t));
    metainfo.files[0] = dl_file_create_and_open(metainfo.info.length, path);
  }

  return metainfo;
}
