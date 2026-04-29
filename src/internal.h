#ifndef SUTURE_INTERNAL_H
#define SUTURE_INTERNAL_H

#include <suture/tracker.h>

#include <jni.h>
#include <jvmti.h>

#define SU_TRY(STATUS, ACTION) \
  if ((STATUS = ACTION) != SU_OK) {   \
    return STATUS;                    \
  }

#define SU_TRY_CATCH(STATUS, ACTION, CATCH) \
  if (((STATUS) = (ACTION)) != SU_OK) {     \
    goto CATCH;                             \
  }

#define SU_FREE(PTR)     \
  do {                   \
    if ((PTR) != NULL) { \
      free((PTR));       \
      (PTR) = NULL;      \
    }                    \
  } while (0)

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

#endif // SUTURE_INTERNAL_H
