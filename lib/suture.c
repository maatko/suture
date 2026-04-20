#include <suture.h>

#include <suture/flag.h>
#include <suture/transform.h>

#include <assert.h>
#include <stdbool.h>
#include <string.h>

static enum su_error su_attach_thread(JavaVM *jvm, void **env, bool *has_attached) {
  assert(jvm != NULL && "su_attach_thread: parameter `jvm` must be a valid pointer.");
  assert(env != NULL && "su_attach_thread: parameter `env` must be a valid pointer.");

  jint res = JVM_INVOKE(jvm, GetEnv, env, JNI_VERSION_1_6);

  const bool attach = res == JNI_EDETACHED;
  if (attach)
    res = JVM_INVOKE(jvm, AttachCurrentThreadAsDaemon, env, NULL);

  if (NULL != has_attached)
    (*has_attached) = attach;

  return res != JNI_OK ? SU_JVM_JNI_ATTACH_FAILURE : SU_OK;
}

enum su_error su_init(struct su_env *env) {
  assert(env != NULL && "su_init: `env` must be a valid pointer.");
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

  if (!su_flag_patch("AllowRedefinitionToAddDeleteMethods", true)) {
    return SU_JVM_GENERIC_FAILURE;
  }

  return SU_OK;
}

enum su_error su_detour(struct su_env *env, const char *class_name, const char *method_name, const char *method_signature, jmethodID *original_method, void *function) {
  assert(env != NULL && "su_detour: `env` must be a valid pointer.");
  assert(function != NULL && "su_detour: `function` must be a valid pointer.");

  env->hooks = (struct su_hook *)realloc(env->hooks, (++env->hooks_count) * sizeof(struct su_hook));
  assert(env->hooks != NULL && "su_detour: failed to reallocate memory for the hooks");

  env->hooks[env->hooks_count - 1] = (struct su_hook){
    .type = SU_HOOK_DETOUR,
    .name = strdup(method_name),
    .signature = strdup(method_signature),
    .class_name = strdup(class_name),
    .original_name = su_hook_original_name(class_name),
    .original = original_method,
    .detour = function
  };

  return SU_OK;
}

enum su_error su_mdetour(struct su_env *env, jmethodID method, jmethodID *original_method, void *function) {
  enum su_error status = SU_OK;

  jclass declaring_class = NULL;

  char *class_name = NULL;
  char *method_name = NULL;
  char *method_signature = NULL;

  JVM_TRY(JVM_INVOKE(env->jvmti, GetMethodDeclaringClass, method, &declaring_class), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE);
  JVM_TRY(JVM_INVOKE(env->jvmti, GetClassSignature, declaring_class, &class_name, NULL), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE);

  JVM_TRY_CATCH(status, JVM_INVOKE(env->jvmti, GetMethodName, method, &method_name, &method_signature, NULL), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE, exit);
  SU_TRY_CATCH(status, su_detour(env, class_name, method_name, method_signature, original_method, function), exit);

exit:

  JVM_FREE(env->jvmti, class_name);
  JVM_FREE(env->jvmti, method_name);
  JVM_FREE(env->jvmti, method_signature);

  return status;
}

enum su_error su_transform(const struct su_env *env) {
  enum su_error status = SU_OK;
  JNIEnv *jni = NULL;
  bool attached;

  SU_TRY(su_attach_thread(env->jvm, (void **)&jni, &attached), SU_JVM_JNI_ATTACH_FAILURE);

  JVM_TRY_CATCH(status, JVM_INVOKE(env->jvmti, SetEnvironmentLocalStorage, env), JVMTI_ERROR_NONE, SU_JVM_ENVIRONMENT_STORAGE_FAILURE, exit);

  jclass *classes = (jclass *)malloc(sizeof(jclass) * env->hooks_count);
  assert(classes != NULL && "su_transform: failed to allocate memory for classes");

  for (u2 i = 0; i < env->hooks_count; i++) {
    const struct su_hook *hook = &env->hooks[i];
    classes[i] = JVM_INVOKE(jni, FindClass, hook->class_name);
  }

  JVM_TRY_CATCH(status, JVM_INVOKE(env->jvmti, RetransformClasses, env->hooks_count, classes), JVMTI_ERROR_NONE, SU_JVM_RETRANSFORM_FAILURE, exit);

  if (env->error != SU_OK)
    return env->error;

  for (u2 i = 0; i < env->hooks_count; i++) {
    const struct su_hook *hook = &env->hooks[i];
     jclass klass = classes[i];

    JNINativeMethod method = {
      .name = hook->name,
      .signature = hook->signature,
      .fnPtr = hook->detour
    };

    JVM_TRY_CATCH(status, JVM_INVOKE(jni, RegisterNatives, klass, &method, 1), JVMTI_ERROR_NONE, SU_JVM_RETRANSFORM_FAILURE, exit);

    klass = JVM_INVOKE(jni, FindClass, hook->class_name);

    free(classes);

    if (hook->original != NULL)
      (*hook->original) = JVM_INVOKE(jni, GetMethodID, klass, hook->original_name, hook->signature);
  }

exit:

  if (attached)
    JVM_TRY(JVM_INVOKE(env->jvm, DetachCurrentThread), JNI_OK, SU_JVM_JVMTI_DETACH_FAILURE);

  return status;
}

void su_dispose(struct su_env *env) {
  const jvmtiEventCallbacks callbacks = { 0 };

  JVM_INVOKE(env->jvmti, SetEventCallbacks, &callbacks, sizeof(callbacks));
  JVM_INVOKE(env->jvmti, SetEventNotificationMode, JVMTI_DISABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL);
  JVM_INVOKE(env->jvm, DetachCurrentThread);

  if (env->hooks != NULL) {
    for (u2 i = 0; i < env->hooks_count; i++) {
      struct su_hook *hook = &env->hooks[i];
      if (hook == NULL)
        continue;

      SU_FREE(hook->name);
      SU_FREE(hook->signature);
      SU_FREE(hook->class_name);
      SU_FREE(hook->original_name);
    }

    SU_FREE(env->hooks);
  }
}