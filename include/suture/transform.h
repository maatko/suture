#ifndef SUTURE_TRANSFORM_H
#define SUTURE_TRANSFORM_H

#include "error.h"
#include "stream.h"
#include "types.h"

#include <jni.h>
#include <jvmti.h>

struct su_method {
  char *name;
  char *desc;
  struct su_chunk *chunk;
};

struct su_transform {
  struct su_chunk *chunks;

  struct su_method *methods;
  u2 methods_count;

  void **constant_pool;
  u2 constant_pool_count;
};

#ifdef __cplusplus
extern "C" {
#endif

extern void JNICALL su_transform_class_file_load_hook(
    jvmtiEnv *jvmti, JNIEnv *jni,
    jclass class_being_redefined, jobject loader,
    const char *name, jobject protection_domain,
    jint class_data_len, const unsigned char *class_data,
    jint *new_class_data_len, unsigned char **new_class_data);

enum su_error su_transform_init(struct su_transform *transform, u1 *buffer, u2 buffer_length);

enum su_error su_const_add_utf8(struct su_transform *transform, const char *utf8, u2 *cp_index);

enum su_error su_add_method(struct su_transform *transform, const char *name, const char *desc, u2 access_flags, struct su_stream **stream);

enum su_error su_transform_build(const struct su_transform *transform, u1 **buffer, u2 *buffer_length);

void su_transform_dispose(struct su_transform *transform);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_TRANSFORM_H
