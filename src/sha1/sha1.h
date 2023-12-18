#ifndef SHA1_H
#define SHA1_H

#include "../file-parser/file-parser.h"
#include <stddef.h>

bool torrent_sha1_verify(metainfo_t *torrent, size_t piece_index);

#endif // SHA1_H
