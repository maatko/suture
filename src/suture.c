#include <suture.h>

#include <suture/flag.h>
#include <suture/tracker.h>
#include <suture/transform.h>

#include <stdbool.h>

#include "internal.h"

static enum su_error su_attach_thread(JavaVM *jvm, void **env, bool *has_attached) {
  if (jvm == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  jint res = JVM_INVOKE(jvm, GetEnv, env, JNI_VERSION_1_6);

  const bool attach = res == JNI_EDETACHED;
  if (attach)
    res = JVM_INVOKE(jvm, AttachCurrentThreadAsDaemon, env, NULL);

  if (NULL != has_attached)
    *has_attached = attach;

  return res != JNI_OK ? SU_JVM_JNI_ATTACH_FAILURE : SU_OK;
}

static enum su_error su_get_class(struct su_env *env, const char *class_name, struct su_class **klass) {
  if (env == NULL || class_name == NULL || klass == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  for (u2 i = 0; i < env->classes_count; i++) {
    struct su_class *current_class = &env->classes[i];
    if (strcmp(current_class->name, class_name) == 0) {
      *klass = current_class;
      return SU_OK;
    }
  }

  enum su_error status = SU_OK;

  JNIEnv *jni = NULL;
  SU_TRY(status, su_attach_thread(env->jvm, (void **)&jni, NULL));

  const jclass handle = JVM_INVOKE(jni, FindClass, class_name);
  if (handle == NULL) {
    JVM_INVOKE(jni, ExceptionClear);
    return SU_JVM_CLASS_NOT_FOUND;
  }

  void *buffer = realloc(env->classes, sizeof(struct su_class) * (env->classes_count + 1));
  if (buffer == NULL) {
    JVM_INVOKE(jni, DeleteLocalRef, handle);
    return SU_MEMORY_ALLOCATION_FAILURE;
  }

  env->classes = (struct su_class *)buffer;

  struct su_class *current_class = &env->classes[env->classes_count];
  memset(current_class, 0, sizeof(struct su_class));

  current_class->name = strdup(class_name);
  current_class->handle = JVM_INVOKE(jni, NewGlobalRef, (jobject)handle);

  JVM_INVOKE(jni, DeleteLocalRef, handle);

  if (current_class->name == NULL || current_class->handle == NULL) {
    if (current_class->handle != NULL)
      JVM_INVOKE(jni, DeleteGlobalRef, current_class->handle);

    SU_FREE(current_class->name);
    return SU_MEMORY_ALLOCATION_FAILURE;
  }

  *klass = current_class;
  env->classes_count++;
  return SU_OK;
}

static jmethodID su_get_method(JNIEnv *jni, const jclass klass, const char *name, const char *signature) {
  jmethodID method = JVM_INVOKE(jni, GetMethodID, klass, name, signature);
  if (method != NULL)
    return method;

  JVM_INVOKE(jni, ExceptionClear);

  method = JVM_INVOKE(jni, GetStaticMethodID, klass, name, signature);
  if (method == NULL)
    JVM_INVOKE(jni, ExceptionClear);

  return method;
}

enum su_error su_init(struct su_env *env) {
  if (env == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  memset(env, 0, sizeof(struct su_env));
  env->classes = NULL;
  env->classes_count = 0;

  jsize count;
  JNIEnv *jni;
  bool attached;

  enum su_error status = SU_OK;
  if (JNI_GetCreatedJavaVMs(&env->jvm, 1, &count) != JNI_OK || count == 0)
    return SU_JVM_NO_VIRTUAL_MACHINES;

  SU_TRY(status, su_attach_thread(env->jvm, (void **)&jni, &attached));
  JVM_TRY(JVM_INVOKE(env->jvm, GetEnv, (void **)&env->jvmti, JVMTI_VERSION_1_2), JNI_OK, SU_JVM_JVMTI_ATTACH_FAILURE);

  jvmtiCapabilities capabilities = { 0 };
  capabilities.can_redefine_classes = 1;
  capabilities.can_redefine_any_class = 1;
  capabilities.can_retransform_classes = 1;
  capabilities.can_retransform_any_class = 1;

  JVM_TRY(JVM_INVOKE(env->jvmti, AddCapabilities, &capabilities), JVMTI_ERROR_NONE, SU_JVM_CAPABILITIES_FAILURE);

  jvmtiEventCallbacks callbacks = { 0 };
  callbacks.ClassFileLoadHook = &su_transform_class_file_load_hook;

  JVM_TRY(JVM_INVOKE(env->jvmti, SetEventCallbacks, &callbacks, sizeof(callbacks)), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE);
  JVM_TRY(JVM_INVOKE(env->jvmti, SetEventNotificationMode, JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE);

  // the flag was added in java 13, the versions before it always allow methods to be added
  status = su_flag_patchb("AllowRedefinitionToAddDeleteMethods", &env->allow_redefinition, true);
  if (status == SU_JVM_FLAG_NOT_FOUND)
    status = SU_OK;

  return status;
}

static enum su_error su_create_hook(struct su_env *env, enum su_hook_type type, const char *class_name, const char *method_name, const char *method_signature, jmethodID *original, void *function) {
  if (method_name[0] == '<')
    return SU_DETOUR_INVALID_TARGET;
  if (method_signature[0] != '(' || strchr(method_signature, ')') == NULL)
    return SU_DETOUR_INVALID_TARGET;

  enum su_error status = SU_OK;
  struct su_class *klass = NULL;

  SU_TRY(status, su_get_class(env, class_name, &klass));

  void *hooks_data = realloc(klass->hooks, (klass->hooks_count + 1) * sizeof(struct su_hook));
  if (hooks_data == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  klass->hooks = (struct su_hook *)hooks_data;

  struct su_hook *hook = &klass->hooks[klass->hooks_count];
  hook->type = type;
  hook->name = strdup(method_name);
  hook->signature = strdup(method_signature);
  hook->jump = su_hook_jump_name(method_name);
  hook->jump_signature = su_hook_jump_signature(type, method_signature);
  hook->native = original;
  hook->function = function;

  if (hook->name == NULL || hook->signature == NULL || hook->jump == NULL || hook->jump_signature == NULL) {
    SU_FREE(hook->name);
    SU_FREE(hook->signature);
    SU_FREE(hook->jump);
    SU_FREE(hook->jump_signature);
    return SU_MEMORY_ALLOCATION_FAILURE;
  }

  klass->hooks_count++;
  return SU_OK;
}

enum su_error su_detour(struct su_env *env, const char *class_name, const char *method_name, const char *method_signature, jmethodID *original_method, void *function) {
  if (env == NULL || class_name == NULL || method_name == NULL || method_signature == NULL || function == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  return su_create_hook(env, SU_HOOK_DETOUR, class_name, method_name, method_signature, original_method, function);
}

enum su_error su_mdetour(struct su_env *env, jmethodID method, jmethodID *original_method, void *function) {
  if (env == NULL || function == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  enum su_error status = SU_OK;
  jclass declaring_class = NULL;

  char *class_name = NULL;
  char *method_name = NULL;
  char *method_signature = NULL;

  JVM_TRY(JVM_INVOKE(env->jvmti, GetMethodDeclaringClass, method, &declaring_class), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE);
  JVM_TRY(JVM_INVOKE(env->jvmti, GetClassSignature, declaring_class, &class_name, NULL), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE);

  class_name[strlen(class_name) - 1] = '\0';

  JVM_TRY_CATCH(status, JVM_INVOKE(env->jvmti, GetMethodName, method, &method_name, &method_signature, NULL), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE, exit);
  SU_TRY_CATCH(status, su_detour(env, class_name + 1, method_name, method_signature, original_method, function), exit);

exit:
  JVM_FREE(env->jvmti, class_name);
  JVM_FREE(env->jvmti, method_name);
  JVM_FREE(env->jvmti, method_signature);

  return status;
}

enum su_error su_trampoline(struct su_env *env, const char *class_name, const char *method_name, const char *method_signature, void *function) {
  if (env == NULL || class_name == NULL || method_name == NULL || method_signature == NULL || function == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  return su_create_hook(env, SU_HOOK_TRAMPOLINE, class_name, method_name, method_signature, NULL, function);
}

enum su_error su_mtrampoline(struct su_env *env, jmethodID method, void *function) {
  if (env == NULL || function == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  enum su_error status = SU_OK;
  jclass declaring_class = NULL;

  char *class_name = NULL;
  char *method_name = NULL;
  char *method_signature = NULL;

  JVM_TRY(JVM_INVOKE(env->jvmti, GetMethodDeclaringClass, method, &declaring_class), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE);
  JVM_TRY(JVM_INVOKE(env->jvmti, GetClassSignature, declaring_class, &class_name, NULL), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE);

  class_name[strlen(class_name) - 1] = '\0';

  JVM_TRY_CATCH(status, JVM_INVOKE(env->jvmti, GetMethodName, method, &method_name, &method_signature, NULL), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE, exit);
  SU_TRY_CATCH(status, su_trampoline(env, class_name + 1, method_name, method_signature, function), exit);

exit:
  JVM_FREE(env->jvmti, class_name);
  JVM_FREE(env->jvmti, method_name);
  JVM_FREE(env->jvmti, method_signature);

  return status;
}

enum su_error su_transform(struct su_env *env) {
  enum su_error status = SU_OK;
  if (env == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  if (env->classes_count == 0)
    return SU_OK;

  JNIEnv *jni = NULL;
  SU_TRY(status, su_attach_thread(env->jvm, (void **)&jni, NULL));

  jclass *targets = malloc(sizeof(jclass) * env->classes_count);
  if (targets == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  for (u2 i = 0; i < env->classes_count; i++)
    targets[i] = env->classes[i].handle;

  env->error = SU_OK;

  JVM_TRY_CATCH(status, JVM_INVOKE(env->jvmti, SetEnvironmentLocalStorage, env), JVMTI_ERROR_NONE, SU_JVM_ENVIRONMENT_STORAGE_FAILURE, exit);
  JVM_TRY_CATCH(status, JVM_INVOKE(env->jvmti, RetransformClasses, env->classes_count, targets), JVMTI_ERROR_NONE, SU_JVM_RETRANSFORM_FAILURE, exit);

  status = env->error;

  for (u2 i = 0; i < env->classes_count; i++) {
    const struct su_class *klass = &env->classes[i];
    if (klass->bytes == NULL || klass->hooks_count == 0)
      continue;

    JNINativeMethod *methods = malloc(sizeof(JNINativeMethod) * klass->hooks_count);
    if (methods == NULL) {
      status = SU_MEMORY_ALLOCATION_FAILURE;
      goto exit;
    }

    for (u2 j = 0; j < klass->hooks_count; j++) {
      const struct su_hook *hook = &klass->hooks[j];

      JNINativeMethod *method = &methods[j];
      method->fnPtr = hook->function;

      if (hook->type == SU_HOOK_TRAMPOLINE) {
        method->name = hook->jump;
        method->signature = hook->jump_signature;
        continue;
      }

      method->name = hook->name;
      method->signature = hook->signature;

      if (hook->native != NULL)
        *hook->native = su_get_method(jni, klass->handle, hook->jump, hook->signature);
    }

    if (JVM_INVOKE(jni, RegisterNatives, klass->handle, methods, klass->hooks_count) != JNI_OK) {
      JVM_INVOKE(jni, ExceptionClear);

      if (status == SU_OK)
        status = SU_JVM_NATIVE_REGISTRATION_FAILURE;
    }

    SU_FREE(methods);
  }

exit:
  SU_FREE(targets);
  return status;
}

enum su_error su_dispose(struct su_env *env) {
  if (env == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  if (env->jvm == NULL || env->jvmti == NULL)
    return SU_OK;

  enum su_error status = SU_OK;

  const jvmtiEventCallbacks callbacks = { 0 };
  JVM_INVOKE(env->jvmti, SetEventCallbacks, &callbacks, sizeof(callbacks));
  JVM_INVOKE(env->jvmti, SetEventNotificationMode, JVMTI_DISABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL);
  JVM_INVOKE(env->jvmti, SetEnvironmentLocalStorage, NULL);

  JNIEnv *jni = NULL;
  if (su_attach_thread(env->jvm, (void **)&jni, NULL) != SU_OK)
    jni = NULL;

  if (env->classes != NULL) {
    jvmtiClassDefinition *definitions = malloc(sizeof(jvmtiClassDefinition) * env->classes_count);
    if (definitions == NULL)
      status = SU_MEMORY_ALLOCATION_FAILURE;

    jint definitions_count = 0;
    for (u2 i = 0; i < env->classes_count && definitions != NULL; i++) {
      const struct su_class *klass = &env->classes[i];
      if (klass->bytes == NULL)
        continue;

      definitions[definitions_count].klass = klass->handle;
      definitions[definitions_count].class_byte_count = klass->bytes_length;
      definitions[definitions_count].class_bytes = klass->bytes;

      definitions_count++;
    }

    if (definitions_count > 0 && JVM_INVOKE(env->jvmti, RedefineClasses, definitions_count, definitions) != JVMTI_ERROR_NONE)
      status = SU_JVM_RETRANSFORM_FAILURE;

    for (u2 i = 0; i < env->classes_count; i++) {
      struct su_class *klass = &env->classes[i];

      for (u2 j = 0; j < klass->hooks_count; j++) {
        struct su_hook *hook = &klass->hooks[j];

        SU_FREE(hook->name);
        SU_FREE(hook->signature);
        SU_FREE(hook->jump);
        SU_FREE(hook->jump_signature);
      }
      SU_FREE(klass->hooks);

      SU_FREE(klass->name);
      JVM_FREE(env->jvmti, klass->bytes);

      if (jni != NULL)
        JVM_INVOKE(jni, DeleteGlobalRef, klass->handle);
    }

    SU_FREE(definitions);
    SU_FREE(env->classes);
  }
  env->classes_count = 0;

  JVM_INVOKE(env->jvmti, ForceGarbageCollection);
  JVM_INVOKE(env->jvm, DetachCurrentThread);

  const enum su_error flag_status = su_flag_patchb("AllowRedefinitionToAddDeleteMethods", NULL, env->allow_redefinition);
  if (flag_status != SU_OK && flag_status != SU_JVM_FLAG_NOT_FOUND && status == SU_OK)
    status = flag_status;

  env->jvmti = NULL;
  env->jvm = NULL;

  return status;
}
