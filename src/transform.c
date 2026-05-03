#include <suture/transform.h>

#include <suture.h>
#include <suture/stream.h>
#include <suture/tracker.h>
#include <suture/types.h>

#include <assert.h>

#include "internal.h"

#define CONSTANT_Class 7
#define CONSTANT_Fieldref 9
#define CONSTANT_Methodref 10
#define CONSTANT_InterfaceMethodref 11
#define CONSTANT_String 8
#define CONSTANT_Integer 3
#define CONSTANT_Float 4
#define CONSTANT_Long 5
#define CONSTANT_Double 6
#define CONSTANT_NameAndType 12
#define CONSTANT_Utf8 1
#define CONSTANT_MethodHandle 15
#define CONSTANT_MethodType 16
#define CONSTANT_InvokeDynamic 18

void JNICALL su_transform_class_file_load_hook(jvmtiEnv *jvmti, JNIEnv *jni, jclass class_being_redefined, jobject loader, const char *name, jobject protection_domain, jint class_data_len, const unsigned char *class_data, jint *new_class_data_len, unsigned char **new_class_data) {
  struct su_env *env = NULL;
  enum su_error status = SU_OK;

  u1 *transformed_buffer;
  u2 transformed_buffer_len;

  if (JVM_INVOKE(jvmti, GetEnvironmentLocalStorage, (void **)&env) != JVMTI_ERROR_NONE)
    return;

  unsigned char *old_bytes = NULL;
  for (u2 i = 0; i < env->classes_count; i++) {
    struct su_class *klass = &env->classes[i];
    if (strcmp(name, klass->name) != 0)
      continue;

    JVM_INVOKE(jvmti, Allocate, class_data_len, &old_bytes);

    if (old_bytes == NULL) {
      env->error = SU_MEMORY_ALLOCATION_FAILURE;
      return;
    }

    memcpy(old_bytes, class_data, class_data_len);

    struct su_transform transform = { 0 };
    SU_TRY_CATCH(status, su_transform_init(&transform, (u1 *)class_data, (u2)class_data_len), exit);

    klass->bytes = old_bytes;
    klass->bytes_length = class_data_len;

    for (u2 j = 0; j < klass->hooks_count; j++) {
      const struct su_hook *hook = &klass->hooks[j];
      for (u2 k = 0; k < transform.methods_count; k++) {
        struct su_method *method = &transform.methods[k];
        if (strcmp(hook->name, method->name) != 0 || strcmp(hook->signature, method->desc) != 0)
          continue;

        switch (hook->type) {
          case SU_HOOK_DETOUR:
            SU_TRY_CATCH(status, su_hook_detour(hook, &transform, method), exit);
            break;
          case SU_HOOK_TRAMPOLINE:
            SU_TRY_CATCH(status, su_hook_trampoline(hook, &transform, method), exit);
            break;
        }
        break;
      }
    }

    SU_TRY_CATCH(status, su_transform_build(&transform, &transformed_buffer, &transformed_buffer_len), exit);

    unsigned char *jvmti_buffer = NULL;
    JVM_TRY_CATCH(status, JVM_INVOKE(jvmti, Allocate, transformed_buffer_len, &jvmti_buffer), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE, exit);

    memcpy(jvmti_buffer, transformed_buffer, transformed_buffer_len);

    *new_class_data = jvmti_buffer;
    *new_class_data_len = transformed_buffer_len;

    free(transformed_buffer);
    su_transform_dispose(&transform);
  }

exit:
  if (status != SU_OK)
    JVM_FREE(jvmti, old_bytes);

  env->error = status;
}

enum su_error su_transform_init(struct su_transform *transform, u1 *buffer, const u2 buffer_length) {
  if (transform == NULL || buffer == NULL || buffer_length == 0)
    return SU_MISSING_REQUIRED_PARAMETERS;

  enum su_error status = SU_OK;

  memset(transform, 0, sizeof(struct su_transform));
  transform->chunks = NULL;

  struct su_stream stream = {
    .buffer = buffer,
    .length = buffer_length,
    .cursor = 0,
    .chunk = 0
  };

  u4 magic;
  SU_TRY_CATCH(status, su_stream_r4(&stream, &magic, 0), exit);

  if (magic != 0xCAFEBABE)
    return SU_JVM_INVALID_CLASS_MAGIC;

  stream.cursor += sizeof(u2) * 2;

  SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, NULL), exit);
  SU_TRY_CATCH(status, su_stream_r2(&stream, &transform->constant_pool_count, 0), exit);

  transform->constant_pool = calloc(transform->constant_pool_count, sizeof(void *));
  assert(transform->constant_pool != NULL && "failed to allocate memory for constant pool");

  for (u2 i = 1; i < transform->constant_pool_count; i++) {
    u1 tag;
    SU_TRY_CATCH(status, su_stream_r1(&stream, &tag, 0), exit);

    switch (tag) {
      case CONSTANT_Class:
      case CONSTANT_String:
      case CONSTANT_MethodType:
        stream.cursor += sizeof(u2);
        break;
      case CONSTANT_Fieldref:
      case CONSTANT_Methodref:
      case CONSTANT_InterfaceMethodref:
      case CONSTANT_NameAndType:
      case CONSTANT_InvokeDynamic:
        stream.cursor += sizeof(u2) * 2;
        break;
      case CONSTANT_Integer:
      case CONSTANT_Float:
        stream.cursor += sizeof(u4);
        break;
      case CONSTANT_Long:
      case CONSTANT_Double:
        stream.cursor += sizeof(u4) * 2;
        i++;
        break;
      case CONSTANT_Utf8: {
        u2 length;
        SU_TRY_CATCH(status, su_stream_r2(&stream, &length, 0), exit);

        char *string = malloc(sizeof(char) * (length + 1));
        assert(string != NULL && "failed to allocate memory for string");

        SU_TRY_CATCH(status, su_stream_rn(&stream, (u1 *)string, length, 0), exit);
        string[length] = '\0';

        transform->constant_pool[i] = string;
      } break;
      case CONSTANT_MethodHandle:
        stream.cursor += sizeof(u1) + sizeof(u2);
        break;
      default:
        status = SU_JVM_INVALID_CONSTANT_POOL;
        goto exit;
    }
  }

  SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, NULL), exit);

  u2 interfaces_count;
  SU_TRY_CATCH(status, su_stream_r2(&stream, &interfaces_count, sizeof(u2) * 3), exit);

  stream.cursor += sizeof(u2) * interfaces_count;

  u2 fields_count;
  SU_TRY_CATCH(status, su_stream_r2(&stream, &fields_count, 0), exit);

  for (u2 i = 0; i < fields_count; i++) {
    u2 attributes_count;
    SU_TRY_CATCH(status, su_stream_r2(&stream, &attributes_count, sizeof(u2) * 3), exit);

    for (u2 j = 0; j < attributes_count; j++) {
      u4 attributes_length;
      SU_TRY_CATCH(status, su_stream_r4(&stream, &attributes_length, sizeof(u2)), exit);

      stream.cursor += attributes_length;
    }
  }

  SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, NULL), exit);
  SU_TRY_CATCH(status, su_stream_r2(&stream, &transform->methods_count, 0), exit);

  transform->methods = calloc(transform->methods_count, sizeof(struct su_method));
  assert(transform->methods != NULL && "failed to allocate memory for methods");

  for (u2 i = 0; i < transform->methods_count; i++) {
    u2 name_index;
    SU_TRY_CATCH(status, su_stream_r2(&stream, &name_index, sizeof(u2)), exit);

    u2 desc_index;
    SU_TRY_CATCH(status, su_stream_r2(&stream, &desc_index, 0), exit);

    struct su_method *method = &transform->methods[i];

    u2 attributes_count;
    SU_TRY_CATCH(status, su_stream_r2(&stream, &attributes_count, 0), exit);

    struct su_chunk *current_chunk = NULL;
    SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, &current_chunk), exit);

    struct su_attribute *attributes = calloc(attributes_count, sizeof(struct su_attribute));
    method->attributes_count = attributes_count;

    for (u2 j = 0; j < attributes_count; j++) {
      u2 attribute_name;
      SU_TRY_CATCH(status, su_stream_r2(&stream, &attribute_name, 0), exit);

      u4 attributes_length;
      SU_TRY_CATCH(status, su_stream_r4(&stream, &attributes_length, 0), exit);

      stream.cursor += attributes_length;

      struct su_attribute *attribute = &attributes[j];
      attribute->name = transform->constant_pool[attribute_name];

      SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, &attribute->chunk), exit);
    }

    method->name = strdup(transform->constant_pool[name_index]);
    method->desc = strdup(transform->constant_pool[desc_index]);
    method->chunk = current_chunk;
    method->attributes = attributes;
  }

  u2 attributes_count;
  SU_TRY_CATCH(status, su_stream_r2(&stream, &attributes_count, 0), exit);

  for (u2 j = 0; j < attributes_count; j++) {
    u4 attributes_length;
    SU_TRY_CATCH(status, su_stream_r4(&stream, &attributes_length, sizeof(u2)), exit);

    stream.cursor += attributes_length;
  }

  SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, NULL), exit);

  if (stream.cursor < stream.length)
    return SU_STREAM_UNFINISHED_READ;

exit:
  return status;
}

enum su_error su_const_add_utf8(struct su_transform *transform, const char *utf8, u2 *cp_index) {
  void *larger_buffer = realloc(transform->constant_pool, sizeof(void *) * (transform->constant_pool_count + 1));
  if (larger_buffer == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  enum su_error status = SU_OK;
  struct su_stream *const_pool_stream = &transform->chunks->next->stream;
  SU_TRY_CATCH(status, su_stream_w1(const_pool_stream, CONSTANT_Utf8, 0), exit);

  const u2 length = (u2)strlen(utf8);
  SU_TRY_CATCH(status, su_stream_w2(const_pool_stream, length, 0), exit);
  SU_TRY_CATCH(status, su_stream_wn(const_pool_stream, (void *)utf8, length, 0), exit);

  transform->constant_pool = larger_buffer;
  transform->constant_pool[transform->constant_pool_count] = NULL;

  if (cp_index != NULL)
    *cp_index = transform->constant_pool_count;

  transform->constant_pool_count++;
exit:
  return status;
}

enum su_error su_add_method(struct su_transform *transform, const char *name, const char *desc, const u2 access_flags, struct su_stream **stream) {
  enum su_error status = SU_OK;

  void *larger_buffer = realloc(transform->methods, sizeof(struct su_method) * (transform->methods_count + 1));
  if (larger_buffer == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  struct su_chunk *method_chunk = malloc(sizeof(struct su_chunk));
  if (method_chunk == NULL) {
    free(larger_buffer);
    return SU_MEMORY_ALLOCATION_FAILURE;
  }
  memset(method_chunk, 0, sizeof(*method_chunk));

  transform->methods = (struct su_method *)larger_buffer;

  u2 name_index, desc_index;
  SU_TRY_CATCH(status, su_const_add_utf8(transform, name, &name_index), exit);
  SU_TRY_CATCH(status, su_const_add_utf8(transform, desc, &desc_index), exit);

  SU_TRY_CATCH(status, su_stream_w2(&method_chunk->stream, access_flags, 0), exit);
  SU_TRY_CATCH(status, su_stream_w2(&method_chunk->stream, name_index, 0), exit);
  SU_TRY_CATCH(status, su_stream_w2(&method_chunk->stream, desc_index, 0), exit);

  struct su_chunk *last_chunk = transform->methods[transform->methods_count - 1].chunk;
  method_chunk->prev = last_chunk;
  method_chunk->next = last_chunk->next;

  if (last_chunk->next != NULL)
    last_chunk->next->prev = method_chunk;
  last_chunk->next = method_chunk;

  struct su_method *method = &transform->methods[transform->methods_count];
  method->name = strdup(name);
  method->desc = strdup(desc);
  method->chunk = method_chunk;

  transform->methods_count++;

  if (stream != NULL)
    (*stream) = &method_chunk->stream;

exit:
  return status;
}

enum su_error su_transform_build(const struct su_transform *transform, u1 **buffer, u2 *buffer_length) {
  enum su_error status = SU_OK;
  if (transform == NULL || buffer == NULL || buffer_length == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  struct su_chunk *const_pool_chunk = transform->chunks->next;
  const_pool_chunk->stream.cursor = 0;

  SU_TRY(status, su_stream_w2(&const_pool_chunk->stream, transform->constant_pool_count, 0));

  struct su_chunk *first_method_chunk = transform->methods->chunk;
  first_method_chunk->stream.cursor = 0;

  SU_TRY(status, su_stream_w2(&first_method_chunk->stream, transform->methods_count, 0));

  u1 *chunks_buffer = NULL;
  u2 chunks_buffer_size = 0;

  SU_TRY(status, su_stream_chunk_build(transform->chunks, &chunks_buffer, &chunks_buffer_size));

  *buffer = chunks_buffer;
  *buffer_length = chunks_buffer_size;
  return SU_OK;
}

void su_transform_dispose(struct su_transform *transform) {
  if (transform == NULL)
    return;

  if (transform->constant_pool != NULL) {
    for (u2 i = 0; i < transform->constant_pool_count; i++) {
      if (transform->constant_pool[i] != NULL)
        SU_FREE(transform->constant_pool[i]);
    }
    SU_FREE(transform->constant_pool);
  }

  struct su_chunk *chunk = transform->chunks;
  while (chunk != NULL) {
    struct su_chunk *next = chunk->next;

    SU_FREE(chunk->stream.buffer);
    SU_FREE(chunk);

    chunk = next;
  }
  transform->chunks = NULL;

  for (u2 i = 0; i < transform->methods_count; i++) {
    struct su_method *method = &transform->methods[i];
    SU_FREE(method->name);
    SU_FREE(method->desc);
    SU_FREE(method->attributes);
  }
  SU_FREE(transform->methods);
}