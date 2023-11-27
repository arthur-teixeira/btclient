#include "peer-id.h"
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <string.h>
#include <unistd.h>

char peer_id[SHA_DIGEST_LENGTH];

void create_peer_id() {
  int pid = getpid();

  uint8_t buf[100];
  int *intlocation = (int *)(&buf[0]);
  *intlocation = pid;

  buf[0] = pid;
  for (size_t i = sizeof(int) + 1; i <= sizeof(buf); i++) {
    buf[i] = rand();
  }

  OpenSSL_add_all_algorithms();
  ERR_load_crypto_strings();
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  const EVP_MD *hashptr = EVP_get_digestbyname("SHA1");

  EVP_DigestInit(ctx, hashptr);

  EVP_DigestUpdate(ctx, buf, 100);

  unsigned int outlen;
  EVP_DigestFinal_ex(ctx, buf, &outlen);
  EVP_MD_CTX_free(ctx);

  memcpy(peer_id, buf, SHA_DIGEST_LENGTH);
}
