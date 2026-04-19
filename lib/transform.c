#include <suture/transform.h>

#include <suture.h>
#include <suture/stream.h>
#include <suture/types.h>

#include <assert.h>
#include <string.h>

#define CONSTANT_POOL_INDEX 1

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
  if (JVM_INVOKE(jvmti, GetEnvironmentLocalStorage, (void **)&env) != JVMTI_ERROR_NONE)
    return;

  enum su_error status = SU_OK;
  for (u2 i = 0; i < env->hooks_count; i++) {
    const struct su_hook *hook = &env->hooks[i];
    if (strcmp(name, hook->class_name) != 0)
      continue;

    struct su_transform transform = { 0 };
    SU_TRY_CATCH(status, su_transform_init(&transform, (u1 *)class_data, (u2)class_data_len), exit);

    su_const_add_utf8(&transform, "Hello, World!");

    u1 *t_buffer;
    u2 t_length;

    su_transform_build(&transform, &t_buffer, &t_length);

    FILE *file = fopen("transformed.class", "wb");
    {
      fwrite((void *)t_buffer, t_length, 1, file);
    }
    fclose(file);

    {
      unsigned char *jvmti_buffer = NULL;
      JVM_TRY_CATCH(status, JVM_INVOKE(jvmti, Allocate, t_length, &jvmti_buffer), JVMTI_ERROR_NONE, SU_JVM_GENERIC_FAILURE, exit);

      memcpy(jvmti_buffer, t_buffer, t_length);

      (*new_class_data) = jvmti_buffer;
      (*new_class_data_len) = t_length;
    }

    free(t_buffer);

    su_transform_dispose(&transform);
  }

exit:
  env->error = status;
}

enum su_error su_transform_init(struct su_transform *transform, u1 *buffer, u2 buffer_length) {
  enum su_error status = SU_OK;

  memset((void *)transform, 0, sizeof(struct su_transform));
  transform->chunks = NULL;

  struct su_stream stream = {
    .buffer = buffer,
    .length = buffer_length,
    .cursor = 0,
    .chunk = 0
  };

  u4 magic;
  SU_TRY_CATCH(status, su_stream_read(&stream, u4, &magic, 0), exit);

  if (magic != 0xCAFEBABE)
    return SU_JVM_INVALID_CLASS_MAGIC;

  stream.cursor += sizeof(u2) * 2;

  SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, &transform->chunks_count), exit);
  SU_TRY_CATCH(status, su_stream_read(&stream, u2, &transform->constant_pool_count, 0), exit);

  transform->constant_pool = calloc(transform->constant_pool_count, sizeof(void *));
  assert(transform->constant_pool != NULL && "failed to allocate memory for strings");

  for (u2 i = 1; i < transform->constant_pool_count; i++) {
    u1 tag;
    SU_TRY_CATCH(status, su_stream_read(&stream, u1, &tag, 0), exit);

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
      SU_TRY_CATCH(status, su_stream_read(&stream, u2, &length, 0), exit);

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

  SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, &transform->chunks_count), exit);

  u2 interfaces_count;
  SU_TRY_CATCH(status, su_stream_read(&stream, u2, &interfaces_count, sizeof(u2) * 3), exit);

  stream.cursor += sizeof(u2) * interfaces_count;

  u2 fields_count;
  SU_TRY_CATCH(status, su_stream_read(&stream, u2, &fields_count, 0), exit);

  for (u2 i = 0; i < fields_count; i++) {
    u2 attributes_count;
    SU_TRY_CATCH(status, su_stream_read(&stream, u2, &attributes_count, sizeof(u2) * 3), exit);

    for (u2 j = 0; j < attributes_count; j++) {
      u4 attributes_length;
      SU_TRY_CATCH(status, su_stream_read(&stream, u4, &attributes_length, sizeof(u2)), exit);

      stream.cursor += attributes_length;
    }
  }

  SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, &transform->chunks_count), exit);

  u2 methods_count;
  SU_TRY_CATCH(status, su_stream_read(&stream, u2, &methods_count, 0), exit);

  for (u2 i = 0; i < methods_count; i++) {
    u2 name_index;
    SU_TRY_CATCH(status, su_stream_read(&stream, u2, &name_index, sizeof(u2)), exit);

    u2 desc_index;
    SU_TRY_CATCH(status, su_stream_read(&stream, u2, &desc_index, 0), exit);

    u2 attributes_count;
    SU_TRY_CATCH(status, su_stream_read(&stream, u2, &attributes_count, 0), exit);

    for (u2 j = 0; j < attributes_count; j++) {
      u4 attributes_length;
      SU_TRY_CATCH(status, su_stream_read(&stream, u4, &attributes_length, sizeof(u2)), exit);

      stream.cursor += attributes_length;
    }

    SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, &transform->chunks_count), exit);
  }

  u2 attributes_count;
  SU_TRY_CATCH(status, su_stream_read(&stream, u2, &attributes_count, 0), exit);

  for (u2 j = 0; j < attributes_count; j++) {
    u4 attributes_length;
    SU_TRY_CATCH(status, su_stream_read(&stream, u4, &attributes_length, sizeof(u2)), exit);

    stream.cursor += attributes_length;
  }

  SU_TRY_CATCH(status, su_stream_chunk(&stream, &transform->chunks, &transform->chunks_count), exit);

  if (stream.cursor < stream.length) {
    return SU_STREAM_UNFINISHED_READ;
  }

exit:
  return status;
}

u2 su_const_add_utf8(struct su_transform *transform, const char *utf8) {
  struct su_stream *chunk = &transform->chunks[CONSTANT_POOL_INDEX];
  su_stream_w1(chunk, CONSTANT_Utf8, 0);

  const u2 length = (u2)strlen(utf8);
  su_stream_w2(chunk, length, 0);
  su_stream_wn(chunk, (void *)utf8, length, 0);

  transform->constant_pool = realloc(transform->constant_pool, sizeof(void *) * (transform->constant_pool_count + 1));
  transform->constant_pool[transform->constant_pool_count] = NULL;

  return transform->constant_pool_count++;
}

enum su_error su_transform_build(const struct su_transform *transform, u1 **buffer, u2 *buffer_length) {
  u2 sz = 0;
  u1 *buff = NULL;

  for (u2 i = 0; i < transform->chunks_count; i++) {
    struct su_stream *chunk = &transform->chunks[i];

    if (i == CONSTANT_POOL_INDEX) {
      chunk->cursor = 0;
      su_stream_w2(chunk, transform->constant_pool_count, 0);
    }

    buff = realloc(buff, sz + chunk->length);
    memcpy(buff + sz, chunk->buffer, chunk->length);

    sz += chunk->length;
  }

  (*buffer) = buff;
  (*buffer_length) = sz;
  return SU_OK;
}

void su_transform_dispose(struct su_transform *transform) {
  for (u2 i = 0; i < transform->constant_pool_count; i++)
    SU_FREE(transform->constant_pool[i]);
  SU_FREE(transform->constant_pool);

  for (u2 i = 0; i < transform->chunks_count; i++)
    SU_FREE(transform->chunks[i].buffer);
  SU_FREE(transform->chunks);
}