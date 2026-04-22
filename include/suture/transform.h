/**
 * @file transform.h
 * @brief JVM class file parsing, mutation, and bytecode transform interface.
 */

#ifndef SUTURE_TRANSFORM_H
#define SUTURE_TRANSFORM_H

#include "error.h"
#include "stream.h"
#include "types.h"

#include <jni.h>
#include <jvmti.h>

/**
 * @brief Represents a single method entry within a transformed class.
 */
struct su_method {
  char *name;             /**< Method name. */
  char *desc;             /**< JNI method descriptor (e.g. @c "(I)V"). */
  struct su_chunk *chunk; /**< Bytecode chunks comprising the method body. */
};

/**
 * @brief Holds the full mutable state of a class file under transformation.
 */
struct su_transform {
  struct su_chunk *chunks; /**< Linked list of raw class file byte chunks. */

  struct su_method *methods; /**< Array of methods parsed from or added to the class. */
  u2 methods_count;          /**< Number of entries in @c methods. */

  void **constant_pool;   /**< Array of constant pool entries indexed by CP index. */
  u2 constant_pool_count; /**< Number of entries in @c constant_pool. */
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief JVMTI class file load hook that intercepts and retransforms loaded classes.
 *
 * Registered with JVMTI's @c ClassFileLoadHook event. Invoked for every class load
 * or retransformation, allowing Suture to rewrite bytecode before the JVM resolves it.
 *
 * @param jvmti                 JVMTI environment.
 * @param jni                   JNI environment.
 * @param class_being_redefined Class being redefined, or NULL on initial load.
 * @param loader                Class loader, or NULL for the bootstrap loader.
 * @param name                  Internal class name (e.g. @c "java/lang/Object").
 * @param protection_domain     Protection domain of the class, or NULL.
 * @param class_data_len        Length in bytes of @p class_data.
 * @param class_data            Original class file bytes.
 * @param new_class_data_len    Out parameter for the length of the rewritten class data.
 * @param new_class_data        Out parameter for the rewritten class file bytes.
 */
extern void JNICALL su_transform_class_file_load_hook(
    jvmtiEnv *jvmti, JNIEnv *jni,
    jclass class_being_redefined, jobject loader,
    const char *name, jobject protection_domain,
    jint class_data_len, const unsigned char *class_data,
    jint *new_class_data_len, unsigned char **new_class_data);

/**
 * @brief Parses a raw class file buffer into a mutable @ref su_transform context.
 *
 * @param transform     Transform context to initialise. Must not be NULL.
 * @param buffer        Raw class file bytes. Must not be NULL.
 * @param buffer_length Length of @p buffer in bytes.
 *
 * @return @ref SU_OK                          on success.
 * @return @ref SU_MISSING_REQUIRED_PARAMETERS if @p transform or @p buffer is NULL.
 * @return @ref SU_MEMORY_ALLOCATION_FAILURE   if internal allocation fails.
 * @return @ref SU_JVM_INVALID_CLASS_MAGIC     if @p buffer is not a valid class file.
 * @return @ref SU_JVM_INVALID_CONSTANT_POOL   if the constant pool is malformed.
 */
enum su_error su_transform_init(struct su_transform *transform, u1 *buffer, u2 buffer_length);

/**
 * @brief Interns a UTF-8 string into the transform's constant pool.
 *
 * If an identical entry already exists, its index is returned without duplication.
 *
 * @param transform Transform context. Must not be NULL.
 * @param utf8      Null-terminated string to intern. Must not be NULL.
 * @param cp_index  Out parameter receiving the constant pool index. Must not be NULL.
 *
 * @return @ref SU_OK                          on success.
 * @return @ref SU_MISSING_REQUIRED_PARAMETERS if any parameter is NULL.
 * @return @ref SU_MEMORY_ALLOCATION_FAILURE   if the pool cannot be expanded.
 */
enum su_error su_const_add_utf8(struct su_transform *transform, const char *utf8, u2 *cp_index);

/**
 * @brief Appends a new method to the transform and returns its bytecode stream.
 *
 * @param transform    Transform context. Must not be NULL.
 * @param name         Method name. Must not be NULL.
 * @param desc         JNI method descriptor. Must not be NULL.
 * @param access_flags JVM access flags (e.g. @c ACC_PUBLIC | @c ACC_STATIC).
 * @param stream       Out parameter receiving the method's writable bytecode stream.
 *
 * @return @ref SU_OK                          on success.
 * @return @ref SU_MISSING_REQUIRED_PARAMETERS if any pointer parameter is NULL.
 * @return @ref SU_MEMORY_ALLOCATION_FAILURE   if the method or its stream cannot be allocated.
 */
enum su_error su_add_method(struct su_transform *transform, const char *name, const char *desc, u2 access_flags, struct su_stream **stream);

/**
 * @brief Serialises a completed transform back into a raw class file buffer.
 *
 * The returned @p buffer is heap-allocated and must be freed by the caller via @ref SU_FREE.
 *
 * @param transform     Completed transform context. Must not be NULL.
 * @param buffer        Out parameter receiving the serialised class file bytes. Must not be NULL.
 * @param buffer_length Out parameter receiving the length of @p buffer in bytes. Must not be NULL.
 *
 * @return @ref SU_OK                          on success.
 * @return @ref SU_MISSING_REQUIRED_PARAMETERS if any parameter is NULL.
 * @return @ref SU_MEMORY_ALLOCATION_FAILURE   if the output buffer cannot be allocated.
 */
enum su_error su_transform_build(const struct su_transform *transform, u1 **buffer, u2 *buffer_length);

/**
 * @brief Releases all resources owned by a transform context.
 *
 * Frees all chunks, methods, and constant pool entries. Does not free @p transform itself.
 *
 * @param transform Transform context to dispose. No-op if NULL.
 */
void su_transform_dispose(struct su_transform *transform);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_TRANSFORM_H
