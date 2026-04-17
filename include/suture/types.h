#ifndef SUTURE_TYPES_H
#define SUTURE_TYPES_H

#include <stdint.h>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define bswap_16(x) (x)
#  define bswap_32(x) (x)
#  define bswap_64(x) (x)
#elif defined(_MSC_VER)
#  include <stdlib.h>
#  define bswap_16(x) _byteswap_ushort(x)
#  define bswap_32(x) _byteswap_ulong(x)
#  define bswap_64(x) _byteswap_uint64(x)
#elif defined(__APPLE__)
#  include <libkern/OSByteOrder.h>
#  define bswap_16(x) OSSwapInt16(x)
#  define bswap_32(x) OSSwapInt32(x)
#  define bswap_64(x) OSSwapInt64(x)
#else
#  include <byteswap.h>
#endif

typedef uint8_t u1;
typedef uint16_t u2;
typedef uint32_t u4;
typedef uint64_t u8;

#endif