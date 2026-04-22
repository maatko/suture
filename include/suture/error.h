/**
 * @file error.h
 * @brief Error handling utilities and status codes for the Suture library.
 */

#ifndef SUTURE_ERROR_H
#define SUTURE_ERROR_H

#include <stdlib.h>

/**
 * @brief Executes an action and returns the given error if it fails.
 *
 * @param ACTION Expression returning an @ref su_error status code.
 * @param ERROR  Error code to return on failure.
 */
#define SU_TRY(ACTION, ERROR) \
  if ((ACTION) != SU_OK) {    \
    return ERROR;             \
  }

/**
 * @brief Executes an action, stores its status, and jumps to a catch label on failure.
 *
 * @param STATUS Variable to store the resulting @ref su_error status code.
 * @param ACTION Expression returning an @ref su_error status code.
 * @param CATCH  Label to jump to on failure.
 */
#define SU_TRY_CATCH(STATUS, ACTION, CATCH) \
  if (((STATUS) = (ACTION)) != SU_OK) {     \
    goto CATCH;                             \
  }

/**
 * @brief Safely frees a pointer and sets it to NULL.
 *
 * @param PTR Pointer to free. No-op if already NULL.
 */
#define SU_FREE(PTR)     \
  do {                   \
    if ((PTR) != NULL) { \
      free((PTR));       \
      (PTR) = NULL;      \
    }                    \
  } while (0)

enum su_error {
  SU_OK = 0,                      /**< Operation completed successfully. */
  SU_MISSING_REQUIRED_PARAMETERS, /**< One or more required parameters were NULL or missing. */
  SU_MEMORY_ALLOCATION_FAILURE,   /**< A memory allocation call failed. */
  SU_DETOUR_NOT_SUPPORTED,        /**< Detouring is not supported on this platform or target. */
  SU_DETOUR_INVALID_TARGET,       /**< The detour target address is invalid. */

  /* stream */
  SU_STREAM_INVALID_CHUNK,   /**< A stream chunk is malformed or out of bounds. */
  SU_STREAM_UNFINISHED_READ, /**< A stream read operation did not consume all expected data. */
  SU_STREAM_AT_END,          /**< The stream has reached its end. */

  /* virtual machine specific */
  SU_JVM_ENVIRONMENT_STORAGE_FAILURE, /**< Failed to store the JVM environment reference. */
  SU_JVM_NO_VIRTUAL_MACHINES,         /**< No running JVM instances were found. */
  SU_JVM_JVMTI_ATTACH_FAILURE,        /**< Failed to attach to the JVMTI interface. */
  SU_JVM_JVMTI_DETACH_FAILURE,        /**< Failed to detach from the JVMTI interface. */
  SU_JVM_JNI_ATTACH_FAILURE,          /**< Failed to attach to the JNI environment. */
  SU_JVM_RETRANSFORM_FAILURE,         /**< Class retransformation via JVMTI failed. */
  SU_JVM_CAPABILITIES_FAILURE,        /**< Failed to acquire required JVMTI capabilities. */
  SU_JVM_INVALID_CLASS_MAGIC,         /**< Class file does not contain a valid magic number. */
  SU_JVM_INVALID_CONSTANT_POOL,       /**< Class constant pool is malformed or unreadable. */
  SU_JVM_GENERIC_FAILURE              /**< Unspecified JVM-related failure. */
};

#endif // SUTURE_ERROR_H
