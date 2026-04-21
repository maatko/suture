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
  SU_MISSING_REQUIRED_PARAMETERS,
  SU_MEMORY_ALLOCATION_FAILURE,
  SU_DETOURING_NOT_SUPPORTED,

  // stream
  SU_STREAM_INVALID_CHUNK,
  SU_STREAM_UNFINISHED_READ,
  SU_STREAM_AT_END,

  // virtual machine specific
  SU_JVM_ENVIRONMENT_STORAGE_FAILURE,
  SU_JVM_NO_VIRTUAL_MACHINES,
  SU_JVM_JVMTI_ATTACH_FAILURE,
  SU_JVM_JVMTI_DETACH_FAILURE,
  SU_JVM_JNI_ATTACH_FAILURE,
  SU_JVM_FLAG_PATCH_FAILURE,
  SU_JVM_RETRANSFORM_FAILURE,
  SU_JVM_CAPABILITIES_FAILURE,
  SU_JVM_INVALID_CLASS_MAGIC,
  SU_JVM_INVALID_CONSTANT_POOL,
  SU_JVM_GENERIC_FAILURE
};

#endif // SUTURE_ERROR_H
