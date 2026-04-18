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

  jmethodID* original;
  void *detour;
};

#endif // SUTURE_HOOK_H