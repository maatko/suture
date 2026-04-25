#ifndef SUTURE_TYPES_H
#define SUTURE_TYPES_H

#include <stdint.h>

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