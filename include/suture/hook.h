#ifndef SUTURE_HOOK_H
#define SUTURE_HOOK_H

#include "disasm.h"
#include "error.h"

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
  char *jump_signature;

  jmethodID *native;
  void *function;
};

#ifdef __cplusplus
extern "C" {
#endif

enum su_error su_hook_detour(const struct su_hook *hook, struct su_transform *transform, struct su_method *method);

enum su_error su_hook_trampoline(const struct su_hook *hook, struct su_transform *transform, struct su_method *method);

char *su_hook_jump_name(const char *name);

char *su_hook_jump_signature(enum su_hook_type type, const char *signature);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_HOOK_H