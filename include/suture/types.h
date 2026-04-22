/**
 * @file types.h
 * @brief Primitive type aliases and JVM/JVMTI utility macros.
 */

#ifndef SUTURE_TYPES_H
#define SUTURE_TYPES_H

#include <stdint.h>

/**
 * @brief Invokes a JNI or JVMTI interface function through its environment pointer.
 *
 * @param ENV      JNI (@c JNIEnv*) or JVMTI (@c jvmtiEnv*) environment pointer.
 * @param FUNCTION Interface function name (e.g. @c FindClass, @c GetMethodID).
 * @param ...      Arguments forwarded to @p FUNCTION.
 */
#define JVM_INVOKE(ENV, FUNCTION, ...) (*(ENV))->FUNCTION((ENV), ##__VA_ARGS__)

/**
 * @brief Executes an action and returns an error code if it does not match the expected result.
 *
 * @param ACTION Expression to evaluate.
 * @param OK     Expected success value to compare against.
 * @param ERROR  Error code to return on mismatch.
 */
#define JVM_TRY(ACTION, OK, ERROR) \
  if ((ACTION) != OK) {            \
    return (ERROR);                \
  }

/**
 * @brief Executes an action and jumps to a catch label if it does not match the expected result.
 *
 * @param STATUS Variable to store the error code on failure.
 * @param ACTION Expression to evaluate.
 * @param OK     Expected success value to compare against.
 * @param ERROR  Error code assigned to @p STATUS on mismatch.
 * @param CATCH  Label to jump to on failure.
 */
#define JVM_TRY_CATCH(STATUS, ACTION, OK, ERROR, CATCH) \
  if ((ACTION) != OK) {                                 \
    (STATUS) = (ERROR);                                 \
    goto CATCH;                                         \
  }

/**
 * @brief Deallocates a JVMTI-managed pointer and sets it to NULL.
 *
 * Uses @c jvmtiEnv::Deallocate rather than @c free, as JVMTI memory must be
 * released through the originating environment. No-op if @p PTR is already NULL.
 *
 * @param JVMTI Active @c jvmtiEnv* environment used to release the memory.
 * @param PTR   Pointer to deallocate.
 */
#define JVM_FREE(JVMTI, PTR)                             \
  do {                                                   \
    if ((PTR) != NULL) {                                 \
      (*JVMTI)->Deallocate(JVMTI, (unsigned char *)PTR); \
      (PTR) = NULL;                                      \
    }                                                    \
  } while (0)

typedef uint8_t u1;  /**< 1-byte unsigned integer — maps to JVM @c u1. */
typedef uint16_t u2; /**< 2-byte unsigned integer — maps to JVM @c u2. */
typedef uint32_t u4; /**< 4-byte unsigned integer — maps to JVM @c u4. */
typedef uint64_t u8; /**< 8-byte unsigned integer — maps to JVM @c u8. */

#endif