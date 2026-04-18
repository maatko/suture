#ifndef SUTURE_H
#define SUTURE_H

#include <suture/error.h>
#include <suture/hook.h>
#include <suture/types.h>

#include <jni.h>
#include <jvmti.h>

struct su_env {
  JavaVM *jvm;
  jvmtiEnv *jvmti;

  struct su_hook *hooks;
  u2 hooks_count;
};

#ifdef __cplusplus
extern "C" {
#endif

enum su_error su_init(struct su_env *env);

enum su_error su_detour(struct su_env *env, const char *class_name, const char *method_name, const char *method_signature, jmethodID *original_method, void *function);

enum su_error su_mdetour(struct su_env *env, jmethodID method, jmethodID *original_method, void *function);

enum su_error su_transform(const struct su_env *env);

void su_dispose(struct su_env *env);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_H