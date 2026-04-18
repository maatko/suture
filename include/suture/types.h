#ifndef SUTURE_TYPES_H
#define SUTURE_TYPES_H

#include <stdint.h>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define bswap_16(x) (x)
#  define bswap_32(x) (x)
#  define bswap_64(x) (x)
#elif defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)))
#  define bswap_16(x) __builtin_bswap16(x)
#  define bswap_32(x) __builtin_bswap32(x)
#  define bswap_64(x) __builtin_bswap64(x)
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

#define JVM_INVOKE(ENV, FUNCTION, ...) (*(ENV))->FUNCTION((ENV), ##__VA_ARGS__)

#define JVM_TRY(ACTION, OK, ERROR) \
  if ((ACTION) != OK) {            \
    return (ERROR);                \
  }

#define JVM_TRY_CATCH(STATUS, ACTION, OK, ERROR, CATCH) \
  if ((ACTION) != OK) {                                 \
    (STATUS) = (ERROR);                                 \
    goto CATCH;                                         \
  }

#define JVM_FREE(JVMTI, PTR)                             \
  do {                                                   \
    if ((PTR) != NULL) {                                 \
      (*JVMTI)->Deallocate(JVMTI, (unsigned char *)PTR); \
      (PTR) = NULL;                                      \
    }                                                    \
  } while (0)

typedef uint8_t u1;
typedef uint16_t u2;
typedef uint32_t u4;
typedef uint64_t u8;

#endif