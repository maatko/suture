#include <suture/asm.h>
#include <suture/code.h>
#include <suture/hook.h>
#include <suture/opcodes.h>
#include <suture/tracker.h>
#include <suture/transform.h>

#include "internal.h"

static struct su_attribute *su_hook_get_code_attribute(const struct su_method *method) {
  for (u2 i = 0; i < method->attributes_count; i++) {
    struct su_attribute *attribute = &method->attributes[i];
    if (strcmp(attribute->name, "Code") == 0)
      return attribute;
  }
  return NULL;
}

static enum su_error su_hook_get_flags(const struct su_method *method, u2 *flags) {
  struct su_stream stream = method->chunk->stream;

  stream.cursor = 0;
  return su_stream_r2(&stream, flags, 0);
}

static enum su_error su_hook_load(struct su_code *code, struct su_insn *position, const u1 opcode, const u2 var) {
  enum su_error status = SU_OK;

  struct su_insn *insn = NULL;
  SU_TRY(status, su_asm_insn(&insn, opcode));

  insn->var = var;
  return su_asm_insert_before(&code->instructions, position, insn);
}

enum su_error su_hook_detour(const struct su_hook *hook, struct su_transform *transform, struct su_method *method) {
  if (hook == NULL || transform == NULL || method == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  if (transform->access_flags & ACC_INTERFACE)
    return SU_DETOUR_INVALID_TARGET;

  const struct su_attribute *code_attribute = su_hook_get_code_attribute(method);
  if (code_attribute == NULL)
    return SU_CLASS_CODE_MISSING;

  enum su_error status = SU_OK;

  u2 flags;
  SU_TRY(status, su_hook_get_flags(method, &flags));

  struct su_chunk *code_chunk = code_attribute->chunk;

  const u2 method_index = (u2)(method - transform->methods);
  const u2 attribute_index = (u2)(code_attribute - method->attributes);

  struct su_attribute *jump_attributes = calloc(1, sizeof(struct su_attribute));
  if (jump_attributes == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  struct su_stream *jump_stream = NULL;
  SU_TRY_CATCH(status, su_add_method(transform, hook->jump, hook->signature, ACC_PRIVATE | (flags & ACC_STATIC ? ACC_STATIC : ACC_FINAL) | (flags & ACC_STRICT), &jump_stream), exit);
  SU_TRY_CATCH(status, su_stream_w2(jump_stream, 1, 0), exit);

  method = &transform->methods[method_index];

  struct su_method *jump = &transform->methods[transform->methods_count - 1];
  jump_attributes[0] = method->attributes[attribute_index];

  jump->attributes = jump_attributes;
  jump->attributes_count = 1;

  jump_attributes = NULL;

  code_chunk->prev->next = code_chunk->next;
  code_chunk->next->prev = code_chunk->prev;

  code_chunk->prev = jump->chunk;
  code_chunk->next = jump->chunk->next;

  jump->chunk->next->prev = code_chunk;
  jump->chunk->next = code_chunk;

  for (u2 i = attribute_index; i + 1 < method->attributes_count; i++)
    method->attributes[i] = method->attributes[i + 1];

  method->attributes_count--;

  struct su_stream *stream = &method->chunk->stream;
  stream->cursor = 0;

  SU_TRY_CATCH(status, su_stream_w2(stream, flags | ACC_NATIVE, 0), exit);
  SU_TRY_CATCH(status, su_stream_w2(stream, method->attributes_count, sizeof(u2) * 2), exit);

  stream->cursor = stream->length;

exit:
  SU_FREE(jump_attributes);
  return status;
}

enum su_error su_hook_trampoline(const struct su_hook *hook, struct su_transform *transform, struct su_method *method) {
  if (hook == NULL || transform == NULL || method == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  if (transform->access_flags & ACC_INTERFACE)
    return SU_DETOUR_INVALID_TARGET;

  const struct su_attribute *code_attribute = su_hook_get_code_attribute(method);
  if (code_attribute == NULL)
    return SU_CLASS_CODE_MISSING;

  enum su_error status = SU_OK;

  u2 flags;
  SU_TRY(status, su_hook_get_flags(method, &flags));

  const bool is_static = (flags & ACC_STATIC) != 0;

  struct su_chunk *code_chunk = code_attribute->chunk;
  struct su_stream code_stream = code_chunk->stream;

  code_stream.cursor = 0;

  struct su_code code = { 0 };
  SU_TRY(status, su_code_parse(&code, transform->constant_pool, transform->constant_pool_count, &code_stream));

  struct su_stream assembled = { 0 };

  struct su_stream *jump_stream = NULL;
  SU_TRY_CATCH(status, su_add_method(transform, hook->jump, hook->jump_signature, ACC_PRIVATE | ACC_NATIVE | (is_static ? ACC_STATIC : ACC_FINAL), &jump_stream), exit);
  SU_TRY_CATCH(status, su_stream_w2(jump_stream, 0, 0), exit);

  u2 name_index, desc_index, name_and_type_index, jump_index;
  SU_TRY_CATCH(status, su_const_add_utf8(transform, hook->jump, &name_index), exit);
  SU_TRY_CATCH(status, su_const_add_utf8(transform, hook->jump_signature, &desc_index), exit);
  SU_TRY_CATCH(status, su_const_add_name_and_type(transform, name_index, desc_index, &name_and_type_index), exit);
  SU_TRY_CATCH(status, su_const_add_methodref(transform, transform->this_class, name_and_type_index, &jump_index), exit);

  struct su_insn *first = code.instructions;
  u2 slot = 0;

  if (!is_static) {
    SU_TRY_CATCH(status, su_hook_load(&code, first, ALOAD, slot++), exit);
  }

  const char *type = hook->signature + 1;
  while (*type != ')' && *type != '\0') {
    u1 opcode = ILOAD;
    u2 size = 1;

    switch (*type) {
      case 'J':
        opcode = LLOAD;
        size = 2;
        break;
      case 'D':
        opcode = DLOAD;
        size = 2;
        break;
      case 'F':
        opcode = FLOAD;
        break;
      case 'L':
      case '[':
        opcode = ALOAD;
        break;
      default:
        break;
    }

    while (*type == '[')
      type++;

    if (*type == 'L') {
      while (*type != ';' && *type != '\0')
        type++;
    }

    if (*type != '\0')
      type++;

    SU_TRY_CATCH(status, su_hook_load(&code, first, opcode, slot), exit);
    slot += size;
  }

  struct su_insn *call = NULL;
  SU_TRY_CATCH(status, su_asm_insn(&call, is_static ? INVOKESTATIC : INVOKESPECIAL), exit);

  call->method.constant = jump_index;
  SU_TRY_CATCH(status, su_asm_insert_before(&code.instructions, first, call), exit);

  if (code.max_stack < slot)
    code.max_stack = slot;

  SU_TRY_CATCH(status, su_code_build(&code, &assembled), exit);

  SU_FREE(code_chunk->stream.buffer);
  code_chunk->stream = assembled;

exit:
  if (status != SU_OK)
    SU_FREE(assembled.buffer);

  su_code_dispose(&code);
  return status;
}

char *su_hook_jump_name(const char *name) {
  if (name == NULL)
    return NULL;

  const char *prefix = "__suture_jump__";
  const size_t length = strlen(prefix) + strlen(name);

  char *original_name = malloc(length + 1);
  if (original_name == NULL)
    return NULL;

  snprintf(original_name, length + 1, "%s%s", prefix, name);
  return original_name;
}

char *su_hook_jump_signature(const enum su_hook_type type, const char *signature) {
  if (signature == NULL || signature[0] != '(')
    return NULL;

  const char *end = strchr(signature, ')');
  if (end == NULL)
    return NULL;

  if (type == SU_HOOK_DETOUR)
    return strdup(signature);

  const size_t length = (size_t)(end - signature) + 1;

  char *jump_signature = malloc(length + 2);
  if (jump_signature == NULL)
    return NULL;

  memcpy(jump_signature, signature, length);

  jump_signature[length] = 'V';
  jump_signature[length + 1] = '\0';

  return jump_signature;
}
