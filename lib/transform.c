#include <suture/transform.h>

#include <jni.h>
#include <jvmti.h>

static void JNICALL ClassFileLoadHook(
    jvmtiEnv *jvmti, JNIEnv *jni,
    jclass class_being_redefined, jobject loader,
    const char *name, jobject protection_domain,
    jint class_data_len, const unsigned char *class_data,
    jint *new_class_data_len, unsigned char **new_class_data) {
  printf("transforming: %s\n", name);
}

enum su_error su_transform_init(const struct su_env *env) {
  jvmtiEventCallbacks callbacks = {0};
  callbacks.ClassFileLoadHook = &ClassFileLoadHook;

  JVM_TRY(JVM_INVOKE(env->jvmti, SetEventCallbacks, &callbacks, sizeof(callbacks)), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE);
  JVM_TRY(JVM_INVOKE(env->jvmti, SetEventNotificationMode, JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE);

  return SU_OK;
}

enum su_error su_transform_dispose(struct su_env *env) {
  const jvmtiEventCallbacks callbacks = {0};

  JVM_TRY(JVM_INVOKE(env->jvmti, SetEventCallbacks, &callbacks, sizeof(callbacks)), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE);
  JVM_TRY(JVM_INVOKE(env->jvmti, SetEventNotificationMode, JVMTI_DISABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE);

  return SU_OK;
}