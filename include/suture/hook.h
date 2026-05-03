#ifndef SUTURE_HOOK_H
#define SUTURE_HOOK_H

#include "error.h"
#include "disasm.h"

#include <jni.h>

struct su_transform;
struct su_method;

enum su_hook_type {
  SU_HOOK_DETOUR,
  SU_HOOK_TRAMPOLINE
};

struct su_hook {
  enum su_hook_type type;

  char *name;
  char *signature;
  char *jump;

  jmethodID *native;
  void *function;
};

#ifdef __cplusplus
extern "C" {
#endif

enum su_error su_hook_detour(const struct su_hook *hook, struct su_transform *transform, struct su_method* method);

enum su_error su_hook_trampoline(const struct su_hook *hook, struct su_transform *transform, struct su_method* method);

char *su_hook_jump_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_HOOK_H