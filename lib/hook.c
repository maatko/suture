#include <suture/hook.h>

#include <stdlib.h>
#include <string.h>

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