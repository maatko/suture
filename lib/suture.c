#include <suture.h>

#include <assert.h>
#include <stdbool.h>
#include <string.h>

static enum su_error su_attach_thread(JavaVM *jvm, void **env, bool *has_attached) {
  assert(jvm != NULL && "su_attach_thread: parameter `jvm` must be a valid pointer.");
  assert(env != NULL && "su_attach_thread: parameter `env` must be a valid pointer.");

  jint res = JVM_INVOKE(jvm, GetEnv, env, JNI_VERSION_1_1);

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

  JVM_TRY(JVM_INVOKE(env->jvm, GetEnv, (void **)&env->jvmti, JVMTI_VERSION_1_1), JNI_OK, SU_JVM_JVMTI_ATTACH_FAILURE);

  if (attached)
    (*env->jvm)->DetachCurrentThread(env->jvm);

  return SU_OK;
}

enum su_error su_detour(struct su_env *env, const char *class_name, const char *method_name, const char *method_signature, void *function) {
  assert(env != NULL && "su_detour: `env` must be a valid pointer.");
  assert(function != NULL && "su_detour: `function` must be a valid pointer.");

  env->hooks = (struct su_hook *)realloc(env->hooks, (++env->hooks_count) * sizeof(struct su_hook));
  assert(env->hooks != NULL && "su_detour: failed to reallocate memory for the hooks");

  env->hooks[env->hooks_count - 1] = (struct su_hook){
      .type = SU_HOOK_DETOUR,
      .name = strdup(method_name),
      .signature = strdup(method_signature),
      .class_name = strdup(class_name),
      .function = function};

  return SU_OK;
}

enum su_error su_mdetour(struct su_env *env, jmethodID method, void *function) {
  enum su_error status = SU_OK;

  jclass declaring_class = NULL;

  char *class_name = NULL;
  char *method_name = NULL;
  char *method_signature = NULL;

  JVM_TRY(JVM_INVOKE(env->jvmti, GetMethodDeclaringClass, method, &declaring_class), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE);

  JVM_TRY(JVM_INVOKE(env->jvmti, GetClassSignature, declaring_class, &class_name, NULL), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE);

  JVM_TRY_CATCH(status, JVM_INVOKE(env->jvmti, GetMethodName, method, &method_name, &method_signature, NULL), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE, catch);

  SU_TRY_CATCH(status, su_detour(env, class_name, method_name, method_signature, function), catch);

catch:
  JVM_FREE(env->jvmti, class_name);
  JVM_FREE(env->jvmti, method_name);
  JVM_FREE(env->jvmti, method_signature);

  return status;
}

enum su_error su_transform(struct su_env *env) {
  return SU_OK;
}

void su_dispose(struct su_env *env) {
  if (env->hooks != NULL) {
    for (u2 i = 0; i < env->hooks_count; i++) {
      struct su_hook *hook = &env->hooks[i];
      if (hook == NULL)
        continue;

      SU_FREE(hook->name);
      SU_FREE(hook->signature);
      SU_FREE(hook->class_name);
    }

    SU_FREE(env->hooks);
  }
}