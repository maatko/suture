#ifndef SUTURE_H
#define SUTURE_H

#include <suture/error.h>
#include <suture/types.h>

#include <jni.h>
#include <jvmti.h>

struct su_env {
  JavaVM *jvm;
  JNIEnv *jni;
  jvmtiEnv *jvmti;
};

#ifdef __cplusplus
extern "C" {
#endif

enum su_error su_init(struct su_env *env);

enum su_error su_transform(struct su_env *env);

void su_dispose(struct su_env *env);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_H