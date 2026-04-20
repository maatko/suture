#include <suture/flag.h>

#include <jni.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct vm_struct_entry {
  const char *type_name;
  const char *field_name;
  const char *type_string;

  int32_t is_static;
  uint64_t offset;

  void *address;
};

struct vm_type_entry {
  const char *type_name;
  const char *super_class_name;

  int32_t is_oop_type;
  int32_t is_integer_type;
  int32_t is_unsigned;

  uint64_t size;
};

extern JNIIMPORT struct vm_struct_entry *gHotSpotVMStructs;
extern JNIIMPORT struct vm_type_entry *gHotSpotVMTypes;

bool su_flag_patch(const char *name, const bool value) {
  unsigned char *flags_array = NULL;
  size_t numFlags = 0;
  uint64_t flagSize = 0;
  uint64_t nameOffset = 0, addrOffset = 0;

  for (struct vm_type_entry *t = gHotSpotVMTypes; t->type_name; t++) {
    if (strcmp(t->type_name, "JVMFlag") == 0 || strcmp(t->type_name, "Flag") == 0) {
      flagSize = t->size;
      break;
    }
  }

  for (struct vm_struct_entry *s = gHotSpotVMStructs; s->type_name; s++) {
    bool isFlagType = (strcmp(s->type_name, "JVMFlag") == 0 || strcmp(s->type_name, "Flag") == 0);
    if (isFlagType && strcmp(s->field_name, "_name") == 0)
      nameOffset = s->offset;
    if (isFlagType && strcmp(s->field_name, "_addr") == 0)
      addrOffset = s->offset;


    if (s->is_static && (strcmp(s->field_name, "flags") == 0 ||
                         strcmp(s->field_name, "flagTable") == 0 ||
                         strcmp(s->field_name, "head") == 0) && flags_array == NULL) {
      flags_array = *(unsigned char **)s->address;
    }

    if (s->is_static && strcmp(s->field_name, "numFlags") == 0) {
      numFlags = *(size_t *)s->address;
    }
  }

  if (!flags_array || !numFlags || !flagSize) {
    return false;
  }

  for (size_t i = 0; i < numFlags; i++) {
    unsigned char *current = flags_array + (i * flagSize);
    const char *current_name = *(const char **)(current + nameOffset);

    if (current_name != NULL && strcmp(current_name, name) == 0) {
      bool *offset = *(bool **)(current + addrOffset);
      memset(offset, value, sizeof(bool));
      return true;
    }
  }

  return false;
}