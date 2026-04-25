#include <suture.h>

#include <suture/flag.h>
#include <suture/transform.h>

#include <stdbool.h>
#include <string.h>

static enum su_error su_attach_thread(JavaVM *jvm, void **env, bool *has_attached) {
  if (jvm == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  jint res = JVM_INVOKE(jvm, GetEnv, env, JNI_VERSION_1_6);

  const bool attach = res == JNI_EDETACHED;
  if (attach)
    res = JVM_INVOKE(jvm, AttachCurrentThreadAsDaemon, env, NULL);

  if (NULL != has_attached)
    (*has_attached) = attach;

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

  JNIEnv *jni = NULL;
  su_attach_thread(env->jvm, (void **)&jni, NULL); // todo: handle the return response

  void *buffer = realloc(env->classes, sizeof(struct su_class) * (env->classes_count + 1));
  if (buffer == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  env->classes = (struct su_class *)buffer;

  struct su_class *current_class = &env->classes[env->classes_count];
  memset(current_class, 0, sizeof(struct su_class));

  current_class->name = strdup(class_name);

  const jclass handle = JVM_INVOKE(jni, FindClass, class_name);
  current_class->handle = JVM_INVOKE(jni, NewGlobalRef, (jobject)handle);

  *klass = current_class;
  env->classes_count++;
  return SU_OK;
}

enum su_error su_init(struct su_env *env) {
  if (env == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  memset(env, 0, sizeof(struct su_env));

  jsize count;
  JNIEnv *jni;
  bool attached;

  if (JNI_GetCreatedJavaVMs(&env->jvm, 1, &count) != JNI_OK || count == 0)
    return SU_JVM_NO_VIRTUAL_MACHINES;

  SU_TRY(su_attach_thread(env->jvm, (void **)&jni, &attached), SU_JVM_JNI_ATTACH_FAILURE);
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

  return su_flag_patchb("AllowRedefinitionToAddDeleteMethods", &env->allow_redefinition, true);
}

enum su_error su_detour(struct su_env *env, const char *class_name, const char *method_name, const char *method_signature, jmethodID *original_method, void *function) {
  if (env == NULL || class_name == NULL || method_signature == NULL || function == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  if (class_name[0] == '<')
    return SU_DETOUR_INVALID_TARGET;

  struct su_class *klass = NULL;
  su_get_class(env, class_name, &klass); // todo: check for error return response code.

  void *hooks_data = realloc(klass->hooks, (klass->hooks_count + 1) * sizeof(struct su_hook));
  if (hooks_data == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  klass->hooks = (struct su_hook *)hooks_data;

  struct su_hook *hook = &klass->hooks[klass->hooks_count];
  hook->type = SU_HOOK_DETOUR;
  hook->name = strdup(method_name);
  hook->signature = strdup(method_signature);
  hook->jump = su_hook_original_name(class_name);
  hook->original = original_method;
  hook->function = function;

  klass->hooks_count++;
  return SU_OK;
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

enum su_error su_transform(const struct su_env *env) {
  if (env == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  JNIEnv *jni = NULL;
  SU_TRY(su_attach_thread(env->jvm, (void **)&jni, NULL), SU_JVM_JNI_ATTACH_FAILURE);

  jclass *targets = malloc(sizeof(jclass) * env->classes_count);
  if (targets == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  for (u2 i = 0; i < env->classes_count; i++)
    targets[i] = env->classes[i].handle;

  enum su_error status = SU_OK;
  JVM_TRY_CATCH(status, JVM_INVOKE(env->jvmti, SetEnvironmentLocalStorage, env), JVMTI_ERROR_NONE, SU_JVM_ENVIRONMENT_STORAGE_FAILURE, exit);
  JVM_TRY_CATCH(status, JVM_INVOKE(env->jvmti, RetransformClasses, env->classes_count, targets), JVMTI_ERROR_NONE, SU_JVM_RETRANSFORM_FAILURE, exit);

  SU_FREE(targets);

  if (env->error != SU_OK)
    return env->error;

  for (u2 i = 0; i < env->classes_count; i++) {
    const struct su_class *klass = &env->classes[i];

    JNINativeMethod *methods = malloc(sizeof(struct su_class) * klass->hooks_count);
    if (methods == NULL)
      return SU_MEMORY_ALLOCATION_FAILURE;

    for (u2 j = 0; j < klass->hooks_count; j++) {
      const struct su_hook *hook = &klass->hooks[j];

      JNINativeMethod *method = &methods[j];
      method->name = hook->name;
      method->signature = hook->signature;
      method->fnPtr = hook->function;

      const jclass transformed_class = JVM_INVOKE(jni, FindClass, klass->name);
      if (hook->original != NULL)
        *hook->original = JVM_INVOKE(jni, GetMethodID, transformed_class, hook->jump, hook->signature); // todo: in trampoline hooks, signature is not the same as original method signature
    }

    JVM_TRY_CATCH(status, JVM_INVOKE(jni, RegisterNatives, klass->handle, methods, klass->hooks_count), JVMTI_ERROR_NONE, SU_JVM_RETRANSFORM_FAILURE, exit);
    SU_FREE(methods);
  }

exit:
  return status;
}

enum su_error su_dispose(struct su_env *env) {
  if (env == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  if (env->classes != NULL) {
    jvmtiClassDefinition *definitions = malloc(sizeof(jvmtiClassDefinition) * env->classes_count);
    if (definitions == NULL)
      return SU_MEMORY_ALLOCATION_FAILURE;

    const jvmtiEventCallbacks callbacks = { 0 };
    JVM_INVOKE(env->jvmti, SetEventCallbacks, &callbacks, sizeof(callbacks));
    JVM_INVOKE(env->jvmti, SetEventNotificationMode, JVMTI_DISABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL);

    JNIEnv *jni;
    su_attach_thread(env->jvm, (void **)&jni, NULL);

    for (u2 i = 0; i < env->classes_count; i++) {
      struct su_class *klass = &env->classes[i];

      definitions[i].klass = JVM_INVOKE(jni, FindClass, klass->name);
      definitions[i].class_byte_count = klass->bytes_length;
      definitions[i].class_bytes = klass->bytes;

      for (u2 j = 0; j < klass->hooks_count; j++) {
        struct su_hook *hook = &klass->hooks[j];

        SU_FREE(hook->name);
        SU_FREE(hook->signature);
        SU_FREE(hook->jump);
      }
      SU_FREE(klass->hooks);

      SU_FREE(klass->name);
      JVM_INVOKE(jni, DeleteGlobalRef, klass->handle);
    }

    JVM_INVOKE(env->jvmti, RedefineClasses, env->classes_count, definitions);

    SU_FREE(definitions);
    SU_FREE(env->classes);
  }

  JVM_INVOKE(env->jvm, DetachCurrentThread);
  return su_flag_patchb("AllowRedefinitionToAddDeleteMethods", NULL, env->allow_redefinition);
}