#ifndef SUTURE_ERROR_H
#define SUTURE_ERROR_H

#include <stdlib.h>

#define SU_TRY(ACTION, ERROR) \
  if ((ACTION) != SU_OK) {    \
    return ERROR;             \
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

enum su_error {
  SU_OK = 0,

  // virtual machine specific
  SU_JVM_NO_VIRTUAL_MACHINES,
  SU_JVM_JVMTI_ATTACH_FAILURE,
  SU_JVM_JNI_ATTACH_FAILURE,
  SU_JVM_GENERIC_FAILURE
};

#endif // SUTURE_ERROR_H
