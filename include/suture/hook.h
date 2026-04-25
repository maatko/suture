#ifndef SUTURE_HOOK_H
#define SUTURE_HOOK_H

#include "error.h"
#include "stream.h"
#include "transform.h"

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

  jclass klass;
  jmethodID *original;
  void *detour;

  unsigned char *original_bytes;
  jint original_length;
};

#ifdef __cplusplus
extern "C" {
#endif

enum su_error su_hook_detour(const struct su_hook *hook, struct su_transform *transform, struct su_stream *stream);

char *su_hook_original_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_HOOK_H