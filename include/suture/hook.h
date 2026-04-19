#ifndef SUTURE_HOOK_H
#define SUTURE_HOOK_H

#include <jni.h>

enum su_hook_type {
  SU_HOOK_DETOUR
};

struct su_hook {
  enum su_hook_type type;

  char *name;
  char *signature;
  char *class_name;
  char *original_name;

  jmethodID* original;
  void *detour;

  const unsigned char* original_bytes;
  jint original_length;
};

#ifdef __cplusplus
extern "C" {
#endif

char* su_hook_original_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_HOOK_H