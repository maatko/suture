/**
 * @file hook.h
 * @brief Method hooking interface for JVM bytecode interception.
 */

#ifndef SUTURE_HOOK_H
#define SUTURE_HOOK_H

#include "error.h"
#include "stream.h"
#include "transform.h"

#include <jni.h>

/**
 * @brief Supported hook strategies.
 */
enum su_hook_type {
  SU_HOOK_DETOUR /**< Redirects method execution to a detour function. */
};

/**
 * @brief Describes a method hook and its associated state.
 */
struct su_hook {
  enum su_hook_type type; /**< Hook strategy to apply. */

  char *name;          /**< Target method name. */
  char *signature;     /**< JNI method descriptor (e.g. @c "(I)V"). */
  char *class_name;    /**< Fully qualified class name (e.g. @c "java/lang/Object"). */
  char *original_name; /**< Synthesized name used to expose the original method. */

  jclass klass;        /**< Resolved class reference. */
  jmethodID *original; /**< Out parameter — receives the original method ID after hooking. */
  void *detour;        /**< Pointer to the detour function. */

  unsigned char *original_bytes; /**< Saved original bytecode, used for restoration. */
  jint original_length;          /**< Length in bytes of @c original_bytes. */
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Applies a detour hook to a JVM method via bytecode transformation.
 *
 * Transforms the target method's bytecode to redirect execution to the detour
 * specified in @p hook, writing the result into @p stream via @p transform.
 *
 * @param hook      Hook descriptor identifying the target method and detour. Must not be NULL.
 * @param transform Bytecode transform context to apply. Must not be NULL.
 * @param stream    Output stream to receive the transformed bytecode. Must not be NULL.
 *
 * @return @ref SU_OK                          on success.
 * @return @ref SU_MISSING_REQUIRED_PARAMETERS if any parameter is NULL.
 * @return @ref SU_DETOUR_INVALID_TARGET       if the target method could not be resolved.
 * @return @ref SU_DETOUR_NOT_SUPPORTED        if the hook type is unsupported.
 */
enum su_error su_hook_detour(const struct su_hook *hook, struct su_transform *transform, struct su_stream *stream);

/**
 * @brief Derives the original method name from a hooked method name.
 *
 * Generates the synthesized name used internally to retain access to the
 * pre-hook method after a detour is installed.
 *
 * @param name Source method name. Must not be NULL.
 *
 * @return Heap-allocated string with the derived name, or NULL on allocation failure.
 *         The caller is responsible for freeing the returned string.
 */
char *su_hook_original_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_HOOK_H