#ifndef BITFIELD_H
#define BITFIELD_H

#include <limits.h>

#define BITFIELD_NUM_BYTES(len)                                                \
  (((len) / CHAR_BIT) + ((len) % CHAR_BIT ? 1 : 0))

#define BITFIELD_ISSET(idx, buf)                                               \
  !!((buf)[idx / CHAR_BIT] & (1 << (CHAR_BIT - ((idx) % CHAR_BIT) - 1)))

#define BITFIELD_SET(idx, buf)                                                 \
  ((buf)[(idx) / CHAR_BIT] |= (1 << (CHAR_BIT - ((idx) % CHAR_BIT) - 1)))

#define BITFIELD_CLEAR(idx, buf)                                               \
  ((buf)[(idx) / CHAR_BIT] &= ~(1 << (CHAR_BIT - ((idx) % CHAR_BIT) - 1)))

#endif // BITFIELD_H
