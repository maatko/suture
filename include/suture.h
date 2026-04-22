/**
 * @file suture.h
 * @brief Top-level Suture API — environment lifecycle and method detouring.
 */

#ifndef SUTURE_H
#define SUTURE_H

#include <suture/error.h>
#include <suture/hook.h>
#include <suture/types.h>

#include <jni.h>
#include <jvmti.h>

#include <stdbool.h>

/**
 * @brief Global Suture environment holding all JVM state and registered hooks.
 *
 * Must be initialised with @ref su_init before use and released with @ref su_dispose.
 */
struct su_env {
  enum su_error error; /**< Last error recorded during an async or hook operation. */

  JavaVM *jvm;     /**< Active JVM instance obtained at initialisation. */
  jvmtiEnv *jvmti; /**< JVMTI environment attached to @c jvm. */

  struct su_hook *hooks; /**< Array of registered method hooks. */
  u2 hooks_count;        /**< Number of entries in @c hooks. */

  jclass *targets;  /**< Array of classes pending retransformation. */
  u2 targets_count; /**< Number of entries in @c targets. */

  bool allow_redefinition; /**< Used for storing the original value of allow_redefinition when patching the jvm */
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialises a Suture environment, attaching to the running JVM and acquiring JVMTI capabilities.
 *
 * Must be called before any other Suture function. Locates the active JVM,
 * attaches a JVMTI environment, and registers the class file load hook.
 *
 * @param env Environment to initialise. Must not be NULL.
 *
 * @return @ref SU_OK                          on success.
 * @return @ref SU_MISSING_REQUIRED_PARAMETERS if @p env is NULL.
 * @return @ref SU_JVM_NO_VIRTUAL_MACHINES     if no running JVM could be found.
 * @return @ref SU_JVM_JVMTI_ATTACH_FAILURE    if the JVMTI environment could not be attached.
 * @return @ref SU_JVM_CAPABILITIES_FAILURE    if required JVMTI capabilities could not be acquired.
 */
enum su_error su_init(struct su_env *env);

/**
 * @brief Registers a detour on a method identified by class name, method name, and signature.
 *
 * Resolves the target method via JNI and schedules a bytecode detour to redirect
 * execution to @p function. The original method remains accessible via @p original_method.
 *
 * @param env               Initialised Suture environment. Must not be NULL.
 * @param class_name        Fully qualified class name (e.g. @c "java/lang/Object"). Must not be NULL.
 * @param method_name       Name of the method to detour. Must not be NULL.
 * @param method_signature  JNI method descriptor (e.g. @c "(I)V"). Must not be NULL.
 * @param original_method   Out parameter receiving the original method ID. May be NULL.
 * @param function          Detour function pointer to redirect execution to. Must not be NULL.
 *
 * @return @ref SU_OK                          on success.
 * @return @ref SU_MISSING_REQUIRED_PARAMETERS if any required parameter is NULL.
 * @return @ref SU_DETOUR_INVALID_TARGET       if the class or method could not be resolved.
 * @return @ref SU_MEMORY_ALLOCATION_FAILURE   if the hook could not be registered.
 */
enum su_error su_detour(struct su_env *env, const char *class_name, const char *method_name, const char *method_signature, jmethodID *original_method, void *function);

/**
 * @brief Registers a detour on a method identified by an existing @c jmethodID.
 *
 * Equivalent to @ref su_detour but takes a pre-resolved @c jmethodID directly,
 * bypassing JNI class and method lookup.
 *
 * @param env             Initialised Suture environment. Must not be NULL.
 * @param method          Resolved method ID to detour. Must not be NULL.
 * @param original_method Out parameter receiving the original method ID. May be NULL.
 * @param function        Detour function pointer to redirect execution to. Must not be NULL.
 *
 * @return @ref SU_OK                          on success.
 * @return @ref SU_MISSING_REQUIRED_PARAMETERS if @p env or @p function is NULL.
 * @return @ref SU_DETOUR_INVALID_TARGET       if @p method could not be resolved to a class.
 * @return @ref SU_MEMORY_ALLOCATION_FAILURE   if the hook could not be registered.
 */
enum su_error su_mdetour(struct su_env *env, jmethodID method, jmethodID *original_method, void *function);

/**
 * @brief Applies all registered detours by triggering a JVMTI class retransformation.
 *
 * Retransforms every class in @c env->targets, causing the JVMTI class file load
 * hook to rewrite their bytecode with the registered detours applied.
 *
 * @param env Initialised Suture environment with at least one registered hook. Must not be NULL.
 *
 * @return @ref SU_OK                       on success.
 * @return @ref SU_MISSING_REQUIRED_PARAMETERS if @p env is NULL.
 * @return @ref SU_JVM_RETRANSFORM_FAILURE  if JVMTI retransformation fails.
 */
enum su_error su_transform(const struct su_env *env);

/**
 * @brief Detaches from JVMTI and releases all resources owned by the environment.
 *
 * Frees all hooks, targets, and JVMTI state. Does not free @p env itself.
 * After this call, @p env must not be used without re-initialising via @ref su_init.
 *
 * @param env Environment to dispose. Must not be NULL.
 *
 * @return @ref SU_OK                          on success.
 * @return @ref SU_MISSING_REQUIRED_PARAMETERS if @p env is NULL.
 * @return @ref SU_JVM_JVMTI_DETACH_FAILURE    if the JVMTI environment could not be cleanly detached.
 */
enum su_error su_dispose(struct su_env *env);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_H