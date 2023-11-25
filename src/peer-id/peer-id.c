#include "peer-id.h"
#include <openssl/err.h>
#include <openssl/evp.h>
#include <string.h>
#include <unistd.h>

char peer_id[20];

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
  for (size_t i = 0; i < 20; i++) {
    sprintf((char *)&(peer_id[i * 2]), "%02x", buf[i]);
  }
}
