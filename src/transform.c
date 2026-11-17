#include <suture/transform.h>

#include <suture.h>
#include <suture/opcodes.h>
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
#define CONSTANT_Dynamic 17
#define CONSTANT_InvokeDynamic 18
#define CONSTANT_Module 19
#define CONSTANT_Package 20

void JNICALL su_transform_class_file_load_hook(jvmtiEnv *jvmti, JNIEnv *jni, jclass class_being_redefined, jobject loader, const char *name, jobject protection_domain, jint class_data_len, const unsigned char *class_data, jint *new_class_data_len, unsigned char **new_class_data) {
  struct su_env *env = NULL;
  enum su_error status = SU_OK;

  if (JVM_INVOKE(jvmti, GetEnvironmentLocalStorage, (void **)&env) != JVMTI_ERROR_NONE || env == NULL || name == NULL)
    return;

  struct su_class *klass = NULL;
  for (u2 i = 0; i < env->classes_count; i++) {
    if (strcmp(name, env->classes[i].name) == 0) {
      klass = &env->classes[i];
      break;
    }
  }

  if (klass == NULL)
    return;

  struct su_transform transform = { 0 };

  u1 *transformed_buffer = NULL;
  u4 transformed_buffer_len = 0;

  unsigned char *old_bytes = NULL;
  unsigned char *jvmti_buffer = NULL;

  if (class_data_len <= 0) {
    status = SU_MISSING_REQUIRED_PARAMETERS;
    goto exit;
  }

  JVM_TRY_CATCH(status, JVM_INVOKE(jvmti, Allocate, class_data_len, &old_bytes), JVMTI_ERROR_NONE, SU_MEMORY_ALLOCATION_FAILURE, exit);
  memcpy(old_bytes, class_data, class_data_len);
  SU_TRY_CATCH(status, su_transform_init(&transform, (u1 *)class_data, (u4)class_data_len), exit);

  for (u2 j = 0; j < klass->hooks_count; j++) {
    const struct su_hook *hook = &klass->hooks[j];

    struct su_method *method = NULL;
    for (u2 k = 0; k < transform.methods_count; k++) {
      if (strcmp(hook->name, transform.methods[k].name) == 0 && strcmp(hook->signature, transform.methods[k].desc) == 0) {
        method = &transform.methods[k];
        break;
      }
    }

    if (method == NULL) {
      status = SU_DETOUR_INVALID_TARGET;
      goto exit;
    }

    switch (hook->type) {
      case SU_HOOK_DETOUR:
        SU_TRY_CATCH(status, su_hook_detour(hook, &transform, method), exit);
        break;
      case SU_HOOK_TRAMPOLINE:
        SU_TRY_CATCH(status, su_hook_trampoline(hook, &transform, method), exit);
        break;
    }
  }

  SU_TRY_CATCH(status, su_transform_build(&transform, &transformed_buffer, &transformed_buffer_len), exit);
  JVM_TRY_CATCH(status, JVM_INVOKE(jvmti, Allocate, (jlong)transformed_buffer_len, &jvmti_buffer), JVMTI_ERROR_NONE, SU_MEMORY_ALLOCATION_FAILURE, exit);

  memcpy(jvmti_buffer, transformed_buffer, transformed_buffer_len);

  *new_class_data = jvmti_buffer;
  *new_class_data_len = (jint)transformed_buffer_len;

  if (klass->bytes == NULL) {
    klass->bytes = old_bytes;
    klass->bytes_length = class_data_len;

    old_bytes = NULL;
  }

exit:
  SU_FREE(transformed_buffer);
  su_transform_dispose(&transform);

  JVM_FREE(jvmti, old_bytes);

  if (status != SU_OK)
    env->error = status;
}

static enum su_error su_transform_skip(struct su_stream *stream, const u4 size) {
  if (size > (u4)(stream->length - stream->cursor))
    return SU_STREAM_AT_END;

  stream->cursor += size;
  return SU_OK;
}

static char *su_transform_utf8(const struct su_transform *transform, const u2 index) {
  return index < transform->constant_pool_count ? transform->constant_pool[index] : NULL;
}

enum su_error su_transform_init(struct su_transform *transform, u1 *buffer, const u4 buffer_length) {
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

  SU_TRY_CATCH(status, su_transform_skip(&stream, sizeof(u2) * 2), exit);

  SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, NULL), exit);
  SU_TRY_CATCH(status, su_stream_r2(&stream, &transform->constant_pool_count, 0), exit);

  transform->constant_pool = calloc(transform->constant_pool_count, sizeof(void *));
  assert((transform->constant_pool_count == 0 || transform->constant_pool != NULL) && "failed to allocate memory for constant pool");

  for (u2 i = 1; i < transform->constant_pool_count; i++) {
    u1 tag;
    SU_TRY_CATCH(status, su_stream_r1(&stream, &tag, 0), exit);

    switch (tag) {
      case CONSTANT_Class:
      case CONSTANT_String:
      case CONSTANT_MethodType:
      case CONSTANT_Module:
      case CONSTANT_Package:
        SU_TRY_CATCH(status, su_transform_skip(&stream, sizeof(u2)), exit);
        break;
      case CONSTANT_Fieldref:
      case CONSTANT_Methodref:
      case CONSTANT_InterfaceMethodref:
      case CONSTANT_NameAndType:
      case CONSTANT_Dynamic:
      case CONSTANT_InvokeDynamic:
        SU_TRY_CATCH(status, su_transform_skip(&stream, sizeof(u2) * 2), exit);
        break;
      case CONSTANT_Integer:
      case CONSTANT_Float:
        SU_TRY_CATCH(status, su_transform_skip(&stream, sizeof(u4)), exit);
        break;
      case CONSTANT_Long:
      case CONSTANT_Double:
        SU_TRY_CATCH(status, su_transform_skip(&stream, sizeof(u4) * 2), exit);
        i++;
        break;
      case CONSTANT_Utf8: {
        u2 length;
        SU_TRY_CATCH(status, su_stream_r2(&stream, &length, 0), exit);

        char *string = malloc(sizeof(char) * (length + 1));
        assert(string != NULL && "failed to allocate memory for string");

        string[length] = '\0';
        transform->constant_pool[i] = string;

        SU_TRY_CATCH(status, su_stream_rn(&stream, (u1 *)string, length, 0), exit);
      } break;
      case CONSTANT_MethodHandle:
        SU_TRY_CATCH(status, su_transform_skip(&stream, sizeof(u1) + sizeof(u2)), exit);
        break;
      default:
        status = SU_JVM_INVALID_CONSTANT_POOL;
        goto exit;
    }
  }

  SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, NULL), exit);

  SU_TRY_CATCH(status, su_stream_r2(&stream, &transform->access_flags, 0), exit);
  SU_TRY_CATCH(status, su_stream_r2(&stream, &transform->this_class, 0), exit);

  u2 interfaces_count;
  SU_TRY_CATCH(status, su_stream_r2(&stream, &interfaces_count, sizeof(u2)), exit);
  SU_TRY_CATCH(status, su_transform_skip(&stream, sizeof(u2) * interfaces_count), exit);

  u2 fields_count;
  SU_TRY_CATCH(status, su_stream_r2(&stream, &fields_count, 0), exit);

  for (u2 i = 0; i < fields_count; i++) {
    u2 attributes_count;
    SU_TRY_CATCH(status, su_stream_r2(&stream, &attributes_count, sizeof(u2) * 3), exit);

    for (u2 j = 0; j < attributes_count; j++) {
      u4 attributes_length;
      SU_TRY_CATCH(status, su_stream_r4(&stream, &attributes_length, sizeof(u2)), exit);
      SU_TRY_CATCH(status, su_transform_skip(&stream, attributes_length), exit);
    }
  }

  SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, NULL), exit);

  u2 methods_count;
  SU_TRY_CATCH(status, su_stream_r2(&stream, &methods_count, 0), exit);
  SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, &transform->methods_chunk), exit);

  transform->methods = calloc(methods_count, sizeof(struct su_method));
  assert((methods_count == 0 || transform->methods != NULL) && "failed to allocate memory for methods");

  transform->methods_count = methods_count;

  for (u2 i = 0; i < transform->methods_count; i++) {
    u2 name_index;
    SU_TRY_CATCH(status, su_stream_r2(&stream, &name_index, sizeof(u2)), exit);

    u2 desc_index;
    SU_TRY_CATCH(status, su_stream_r2(&stream, &desc_index, 0), exit);

    struct su_method *method = &transform->methods[i];

    u2 attributes_count;
    SU_TRY_CATCH(status, su_stream_r2(&stream, &attributes_count, 0), exit);
    SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, &method->chunk), exit);

    const char *name = su_transform_utf8(transform, name_index);
    const char *desc = su_transform_utf8(transform, desc_index);

    if (name == NULL || desc == NULL) {
      status = SU_JVM_INVALID_CONSTANT_POOL;
      goto exit;
    }

    method->name = strdup(name);
    method->desc = strdup(desc);

    method->attributes = calloc(attributes_count, sizeof(struct su_attribute));
    assert((attributes_count == 0 || method->attributes != NULL) && "failed to allocate memory for attributes");

    for (u2 j = 0; j < attributes_count; j++) {
      u2 attribute_name;
      SU_TRY_CATCH(status, su_stream_r2(&stream, &attribute_name, 0), exit);

      u4 attributes_length;
      SU_TRY_CATCH(status, su_stream_r4(&stream, &attributes_length, 0), exit);
      SU_TRY_CATCH(status, su_transform_skip(&stream, attributes_length), exit);

      struct su_attribute *attribute = &method->attributes[j];
      attribute->name = su_transform_utf8(transform, attribute_name);

      if (attribute->name == NULL) {
        status = SU_JVM_INVALID_CONSTANT_POOL;
        goto exit;
      }

      SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, &attribute->chunk), exit);
      method->attributes_count++;
    }
  }

  u2 attributes_count;
  SU_TRY_CATCH(status, su_stream_r2(&stream, &attributes_count, 0), exit);

  for (u2 j = 0; j < attributes_count; j++) {
    u4 attributes_length;
    SU_TRY_CATCH(status, su_stream_r4(&stream, &attributes_length, sizeof(u2)), exit);
    SU_TRY_CATCH(status, su_transform_skip(&stream, attributes_length), exit);
  }

  SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, NULL), exit);

  if (stream.cursor < stream.length)
    return SU_STREAM_UNFINISHED_READ;

exit:
  return status;
}

static enum su_error su_const_add(struct su_transform *transform, const u1 tag, const u1 *data, const u4 data_length, char *utf8, u2 *cp_index) {
  if (transform->chunks == NULL || transform->chunks->next == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  if (transform->constant_pool_count == UINT16_MAX)
    return SU_JVM_INVALID_CONSTANT_POOL;

  void *larger_buffer = realloc(transform->constant_pool, sizeof(void *) * (transform->constant_pool_count + 1));
  if (larger_buffer == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  transform->constant_pool = larger_buffer;

  enum su_error status = SU_OK;
  struct su_stream *const_pool_stream = &transform->chunks->next->stream;

  const u4 length = const_pool_stream->length;
  const_pool_stream->cursor = length;

  SU_TRY_CATCH(status, su_stream_w1(const_pool_stream, tag, 0), exit);
  SU_TRY_CATCH(status, su_stream_wn(const_pool_stream, data, data_length, 0), exit);

  transform->constant_pool[transform->constant_pool_count] = utf8;

  if (cp_index != NULL)
    *cp_index = transform->constant_pool_count;

  transform->constant_pool_count++;
exit:
  if (status != SU_OK) {
    const_pool_stream->length = length;
    const_pool_stream->cursor = length;
  }

  return status;
}

static enum su_error su_const_add_pair(struct su_transform *transform, const u1 tag, const u2 first, const u2 second, u2 *cp_index) {
  const u1 data[] = { (u1)(first >> 8), (u1)first, (u1)(second >> 8), (u1)second };
  return su_const_add(transform, tag, data, sizeof(data), NULL, cp_index);
}

enum su_error su_const_add_utf8(struct su_transform *transform, const char *utf8, u2 *cp_index) {
  if (transform == NULL || utf8 == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  for (u2 i = 1; i < transform->constant_pool_count; i++) {
    if (transform->constant_pool[i] == NULL || strcmp(transform->constant_pool[i], utf8) != 0)
      continue;

    if (cp_index != NULL)
      *cp_index = i;

    return SU_OK;
  }

  const size_t length = strlen(utf8);
  if (length > UINT16_MAX)
    return SU_STREAM_TOO_LARGE;

  u1 *data = malloc(sizeof(u2) + length);
  char *string = strdup(utf8);

  if (data == NULL || string == NULL) {
    SU_FREE(data);
    SU_FREE(string);
    return SU_MEMORY_ALLOCATION_FAILURE;
  }

  data[0] = (u1)(length >> 8);
  data[1] = (u1)length;
  memcpy(data + sizeof(u2), utf8, length);

  const enum su_error status = su_const_add(transform, CONSTANT_Utf8, data, (u4)(sizeof(u2) + length), string, cp_index);
  if (status != SU_OK)
    SU_FREE(string);

  SU_FREE(data);
  return status;
}

enum su_error su_const_add_name_and_type(struct su_transform *transform, const u2 name_index, const u2 desc_index, u2 *cp_index) {
  if (transform == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  return su_const_add_pair(transform, CONSTANT_NameAndType, name_index, desc_index, cp_index);
}

enum su_error su_const_add_methodref(struct su_transform *transform, const u2 class_index, const u2 name_and_type_index, u2 *cp_index) {
  if (transform == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  return su_const_add_pair(transform, CONSTANT_Methodref, class_index, name_and_type_index, cp_index);
}

enum su_error su_add_method(struct su_transform *transform, const char *name, const char *desc, const u2 access_flags, struct su_stream **stream) {
  if (transform == NULL || name == NULL || desc == NULL || transform->methods_chunk == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  if (transform->methods_count == UINT16_MAX)
    return SU_STREAM_TOO_LARGE;

  enum su_error status = SU_OK;

  u2 name_index, desc_index;
  SU_TRY(status, su_const_add_utf8(transform, name, &name_index));
  SU_TRY(status, su_const_add_utf8(transform, desc, &desc_index));

  void *larger_buffer = realloc(transform->methods, sizeof(struct su_method) * (transform->methods_count + 1));
  if (larger_buffer == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  transform->methods = (struct su_method *)larger_buffer;

  struct su_method *method = &transform->methods[transform->methods_count];
  memset(method, 0, sizeof(struct su_method));

  struct su_chunk *method_chunk = malloc(sizeof(struct su_chunk));
  if (method_chunk == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  memset(method_chunk, 0, sizeof(*method_chunk));

  method->name = strdup(name);
  method->desc = strdup(desc);

  if (method->name == NULL || method->desc == NULL) {
    status = SU_MEMORY_ALLOCATION_FAILURE;
    goto exit;
  }

  SU_TRY_CATCH(status, su_stream_w2(&method_chunk->stream, access_flags, 0), exit);
  SU_TRY_CATCH(status, su_stream_w2(&method_chunk->stream, name_index, 0), exit);
  SU_TRY_CATCH(status, su_stream_w2(&method_chunk->stream, desc_index, 0), exit);

  struct su_chunk *last_chunk = transform->methods_chunk;
  while (last_chunk->next != NULL)
    last_chunk = last_chunk->next;

  if (last_chunk == transform->methods_chunk) {
    status = SU_STREAM_INVALID_CHUNK;
    goto exit;
  }

  method_chunk->next = last_chunk;
  method_chunk->prev = last_chunk->prev;

  last_chunk->prev->next = method_chunk;
  last_chunk->prev = method_chunk;

  method->chunk = method_chunk;
  transform->methods_count++;

  if (stream != NULL)
    (*stream) = &method_chunk->stream;

exit:
  if (status != SU_OK) {
    SU_FREE(method->name);
    SU_FREE(method->desc);
    SU_FREE(method_chunk->stream.buffer);
    SU_FREE(method_chunk);
  }

  return status;
}

enum su_error su_transform_build(const struct su_transform *transform, u1 **buffer, u4 *buffer_length) {
  enum su_error status = SU_OK;
  if (transform == NULL || buffer == NULL || buffer_length == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  if (transform->chunks == NULL || transform->chunks->next == NULL || transform->methods_chunk == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  struct su_stream *const_pool_stream = &transform->chunks->next->stream;
  const_pool_stream->cursor = 0;

  SU_TRY(status, su_stream_w2(const_pool_stream, transform->constant_pool_count, 0));
  const_pool_stream->cursor = const_pool_stream->length;

  struct su_stream *methods_stream = &transform->methods_chunk->stream;
  methods_stream->cursor = 0;

  SU_TRY(status, su_stream_w2(methods_stream, transform->methods_count, 0));
  methods_stream->cursor = methods_stream->length;

  u1 *chunks_buffer = NULL;
  u4 chunks_buffer_size = 0;

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
  transform->constant_pool_count = 0;

  struct su_chunk *chunk = transform->chunks;
  while (chunk != NULL) {
    struct su_chunk *next = chunk->next;

    SU_FREE(chunk->stream.buffer);
    SU_FREE(chunk);

    chunk = next;
  }
  transform->chunks = NULL;
  transform->methods_chunk = NULL;

  if (transform->methods != NULL) {
    for (u2 i = 0; i < transform->methods_count; i++) {
      struct su_method *method = &transform->methods[i];
      SU_FREE(method->name);
      SU_FREE(method->desc);
      SU_FREE(method->attributes);
    }
    SU_FREE(transform->methods);
  }
  transform->methods_count = 0;
}
