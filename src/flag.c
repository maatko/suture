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

enum su_error su_flag_patchb(const char *name, bool *original, const bool value) {
  if (name == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  uint64_t flag_size = 0;
  for (const struct vm_type_entry *type_entry = gHotSpotVMTypes; type_entry->type_name; type_entry++) {
    if (strcmp(type_entry->type_name, "JVMFlag") == 0 || strcmp(type_entry->type_name, "Flag") == 0) {
      flag_size = type_entry->size;
      break;
    }
  }

  unsigned char *flags = NULL;
  uint64_t name_offset = 0;
  uint64_t address_offset = 0;
  size_t num_flags = 0;

  for (const struct vm_struct_entry *struct_entry = gHotSpotVMStructs; struct_entry->type_name; struct_entry++) {
    const bool isFlagType = (strcmp(struct_entry->type_name, "JVMFlag") == 0 || strcmp(struct_entry->type_name, "Flag") == 0);
    if (isFlagType && strcmp(struct_entry->field_name, "_name") == 0)
      name_offset = struct_entry->offset;
    if (isFlagType && strcmp(struct_entry->field_name, "_addr") == 0)
      address_offset = struct_entry->offset;

    if (!struct_entry->is_static)
      continue;

    if (strcmp(struct_entry->field_name, "numFlags") == 0) {
      num_flags = *(size_t *)struct_entry->address;
    } else if ((strcmp(struct_entry->field_name, "flags") == 0 || strcmp(struct_entry->field_name, "flagTable") == 0 || strcmp(struct_entry->field_name, "head") == 0) && flags == NULL) {
      flags = *(unsigned char **)struct_entry->address;
    }
  }

  if (!flags || !num_flags || !flag_size)
    return SU_DETOUR_NOT_SUPPORTED;

  for (size_t i = 0; i < num_flags; i++) {
    unsigned char *current = flags + (i * flag_size);
    const char *current_name = *(const char **)(current + name_offset);

    if (current_name != NULL && strcmp(current_name, name) == 0) {
      bool *offset = *(bool **)(current + address_offset);

      if (original != NULL)
        (*original) = *offset;

      memset(offset, value, sizeof(bool));
      return SU_OK;
    }
  }

  return SU_DETOUR_NOT_SUPPORTED;
}