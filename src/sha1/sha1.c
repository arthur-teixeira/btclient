
#include "../file-parser/file-parser.h"
#include "../piece-request/piece_request.h"
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

bool torrent_sha1_verify(metainfo_t *torrent, size_t piece_index) {
  assert(piece_index < torrent->info.num_pieces);

  uint8_t *piece_hash = torrent->info.pieces[piece_index];

  piece_request_t *pr = piece_request_create(torrent, piece_index);
  OpenSSL_add_all_algorithms();
  ERR_load_crypto_strings();
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  const EVP_MD *hashptr = EVP_get_digestbyname("SHA1");
  EVP_DigestInit(ctx, hashptr);

  for (size_t i = 0; i < pr->block_requests->len; i++) {
    block_request_t *br = &pr->block_requests->values[i];

    for (size_t j = 0; j < br->filemems->len; j++) {
      filemem_t mem = br->filemems->values[j];
      EVP_DigestUpdate(ctx, mem.mem, mem.size);
    }
  }

  unsigned char buf[SHA1_LENGTH];
  unsigned int outlen;
  EVP_DigestFinal_ex(ctx, buf, &outlen);

  EVP_MD_CTX_free(ctx);
  piece_request_free(pr);

  return memcmp(piece_hash, buf, SHA1_LENGTH) == 0;
}
