#include <suture/hook.h>
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

enum su_error su_hook_detour(const struct su_hook *hook, struct su_transform *transform, struct su_method *method) {
  return SU_OK;
}

enum su_error su_hook_trampoline(const struct su_hook *hook, struct su_transform *transform, struct su_method *method) {
  const struct su_attribute *code_attribute = su_hook_get_code_attribute(method);
  if (code_attribute == NULL)
    return SU_CLASS_CODE_MISSING;

  enum su_error status = SU_OK;
  struct su_stream code_stream = code_attribute->chunk->stream;

  code_stream.cursor = 0;

  u2 max_stack, max_local;
  SU_TRY(status, su_stream_r2(&code_stream, &max_stack, sizeof(u2) + sizeof(u4)));
  SU_TRY(status, su_stream_r2(&code_stream, &max_local, 0));

  printf("max_stack: %d, max_local: %d\n", max_stack, max_local);

  u4 code_length;
  SU_TRY(status, su_stream_r4(&code_stream, &code_length, 0));

  struct su_insn *instructions = NULL;
  SU_TRY(status, su_disasm_parse(&instructions, transform->constant_pool, transform->constant_pool_count, &code_stream, code_length));

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