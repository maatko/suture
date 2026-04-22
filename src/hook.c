#include <suture/hook.h>
#include <suture/opcodes.h>

#include <stdlib.h>
#include <string.h>

enum su_error su_hook_detour(const struct su_hook *hook, struct su_transform *transform, struct su_stream *stream) {
  u1 *attributes = NULL;

  const u2 attributes_offset = sizeof(u2) * 3;
  const u2 attributes_length = stream->length - attributes_offset;

  attributes = (u1 *)malloc(sizeof(u1) * attributes_length);
  if (attributes == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  memcpy(attributes, stream->buffer + attributes_offset, attributes_length);

  stream->cursor = 0;
  u2 flags, name_index, desc_index;

  enum su_error status = SU_OK;
  SU_TRY_CATCH(status, su_stream_r2(stream, &flags, 0), exit);
  SU_TRY_CATCH(status, su_stream_r2(stream, &name_index, 0), exit);
  SU_TRY_CATCH(status, su_stream_r2(stream, &desc_index, 0), exit);

  SU_FREE(stream->buffer);

  stream->length = 0;
  stream->cursor = 0;

  SU_TRY_CATCH(status, su_stream_w2(stream, flags | ACC_NATIVE, 0), exit);
  SU_TRY_CATCH(status, su_stream_w2(stream, name_index, 0), exit);
  SU_TRY_CATCH(status, su_stream_w2(stream, desc_index, 0), exit);
  SU_TRY_CATCH(status, su_stream_w2(stream, 0, 0), exit);

  struct su_stream *new_stream;
  SU_TRY_CATCH(status, su_add_method(transform, hook->original_name, hook->signature, ACC_PRIVATE | (flags & ACC_STATIC ? ACC_STATIC : ACC_FINAL), &new_stream), exit);
  SU_TRY_CATCH(status, su_stream_wn(new_stream, attributes, attributes_length, 0), exit);

exit:
  SU_FREE(attributes);
  return status;
}

char *su_hook_original_name(const char *name) {
  if (name == NULL)
    return NULL;

  const char *prefix = "__su_original__";
  const size_t length = strlen(prefix) + strlen(name);

  char *original_name = malloc(length + 1);
  if (original_name == NULL)
    return NULL;

  snprintf(original_name, length + 1, "%s%s", prefix, name);
  return original_name;
}