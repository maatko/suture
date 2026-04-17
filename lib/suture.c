#include <suture.h>

#include <assert.h>
#include <jni.h>
#include <string.h>

#define INVOKE(ENV, WHAT, ...) (*(ENV))->WHAT((ENV), ##__VA_ARGS__)

enum su_error su_init(struct su_env *env) {
  assert(env != NULL && "su_init: env must be a valid pointer.");
  memset(env, 0, sizeof(struct su_env));

  jsize count;
  if (JNI_GetCreatedJavaVMs(&env->jvm, 1, &count) != JNI_OK || count == 0)
    return SU_JVM_NO_VIRTUAL_MACHINES;

  jint res = INVOKE(env->jvm, GetEnv, (void **)&env->jni, JNI_VERSION_1_6);
  if (res == JNI_EDETACHED)
    res = INVOKE(env->jvm, AttachCurrentThread, (void **)&env->jni, NULL);

  if (res != JNI_OK)
    return SU_JVM_ATTACH_JNI_FAILURE;

  res = INVOKE(env->jvm, GetEnv, (void **)&env->jvmti, JVMTI_VERSION_1_2);
  if (res == JNI_EDETACHED)
    res = INVOKE(env->jvm, AttachCurrentThread, (void **)&env->jvmti, NULL);

  return res == JNI_OK ? SU_OK : SU_JVM_ATTACH_JVMTI_FAILURE;
}

enum su_error su_transform(struct su_env *env) {
  printf("jvm: %p\n", env->jvm);
  printf("jni: %p\n", env->jni);
  printf("jvmti: %p\n", env->jvmti);

  return SU_OK;
}

void su_dispose(struct su_env *env) {
  INVOKE(env->jvm, DetachCurrentThread);
}

#undef INVOKE