#include <suture/flag.h>

#include <jni.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  const char *typeName;
  const char *fieldName;
  const char *typeString;
  int32_t isStatic;
  uint64_t offset;
  void *address;
} VMStructEntry;

typedef struct {
  const char *typeName;
  const char *superclassName;
  int32_t isOopType;
  int32_t isIntegerType;
  int32_t isUnsigned;
  uint64_t size;
} VMTypeEntry;

extern JNIIMPORT VMStructEntry *gHotSpotVMStructs;
extern JNIIMPORT VMTypeEntry *gHotSpotVMTypes;

bool jflag_patch(const char *name, const bool value) {
  if (!name || !gHotSpotVMStructs || !gHotSpotVMTypes)
    return false;

  uint64_t flag_size = 0;
  for (const VMTypeEntry *type_entry = gHotSpotVMTypes; type_entry->typeName; type_entry++) {
    if (strcmp(type_entry->typeName, "JVMFlag") == 0 ||
        strcmp(type_entry->typeName, "Flag") == 0) {
      flag_size = type_entry->size;
      break;
    }
  }

  unsigned char *flags_array = NULL;
  size_t num_flags = 0;
  uint64_t name_offset = 0;
  uint64_t addr_offset = 0;

  for (const VMStructEntry *struct_entry = gHotSpotVMStructs; struct_entry->typeName; struct_entry++) {
    const bool is_flag_type = (strcmp(struct_entry->typeName, "JVMFlag") == 0 || strcmp(struct_entry->typeName, "Flag") == 0);
    if (is_flag_type) {
      if (strcmp(struct_entry->fieldName, "_name") == 0)
        name_offset = struct_entry->offset;
      else if (strcmp(struct_entry->fieldName, "_addr") == 0)
        addr_offset = struct_entry->offset;
    }

    if (struct_entry->isStatic) {
      if (strcmp(struct_entry->fieldName, "flags") == 0 ||
          strcmp(struct_entry->fieldName, "flagTable") == 0 ||
          strcmp(struct_entry->fieldName, "head") == 0) {
        flags_array = *(unsigned char **)struct_entry->address;
      } else if (strcmp(struct_entry->fieldName, "numFlags") == 0) {
        num_flags = *(size_t *)struct_entry->address;
      }
    }
  }

  if (!flags_array || !num_flags || !flag_size)
    return false;

  for (size_t i = 0; i < num_flags; i++) {
    unsigned char *entry = flags_array + (i * flag_size);
    const char *currentName = *(const char **)(entry + name_offset);

    if (currentName && strcmp(currentName, name) == 0) {
      *(*(bool **)(entry + addr_offset)) = value;
      return true;
    }
  }

  return false;
}