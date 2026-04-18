#include <suture/hook.h>

#include <stdlib.h>
#include <string.h>

char *su_hook_original_name(const char *name) {
  static const char *prefix = "__su_original__";
  if (name == NULL)
    return NULL;

  const size_t original_name_length = strlen(prefix) + strlen(name);
  char *original_name = malloc(original_name_length + 1);

  if (original_name == NULL)
    return NULL;

  snprintf(original_name, original_name_length + 1, "%s%s", prefix, name);
  return original_name;
}