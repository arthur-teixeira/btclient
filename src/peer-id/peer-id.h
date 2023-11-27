#ifndef PEER_ID_H
#define PEER_ID_H

#include <stdint.h>
#include <openssl/sha.h>

extern char peer_id[2 * SHA_DIGEST_LENGTH];

void create_peer_id();

#endif // PEER_ID_H
