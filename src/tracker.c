#include <suture/tracker.h>

#undef malloc
#undef realloc
#undef calloc
#undef memset
#undef memcpy
#undef strcmp
#undef strdup
#undef strlen
#undef free

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct su_memory_tracker {
  void *ptr;
  size_t size;
  bool freed;

  const char *file;
  size_t line_number;

  struct su_memory_tracker *next;
};

struct su_memory_tracker *start = NULL;
struct su_memory_tracker *end = NULL;

static void su_memory_track(void *ptr, const size_t size, const char *file, const size_t line_number) {
  struct su_memory_tracker *tracker = malloc(sizeof(struct su_memory_tracker));
  assert(tracker != NULL && "failed to allocate memory for a tracker");

  tracker->next = NULL;
  tracker->ptr = ptr;
  tracker->size = size;
  tracker->freed = false;
  tracker->file = file;
  tracker->line_number = line_number;

  if (start == NULL) {
    start = end = tracker;
    return;
  }

  end->next = tracker;
  end = tracker;
}

static void su_memory_untrack(const void *ptr) {
  if (ptr == NULL || start == NULL)
    return;

  struct su_memory_tracker *latest = NULL;
  for (struct su_memory_tracker *it = start; it != NULL; it = it->next) {
    if (it->ptr == ptr && !it->freed) {
      latest = it;
    }
  }

  if (latest) {
    latest->freed = true;
  }
}

bool su_memory_print() {
  size_t leaked_bytes = 0;
  struct su_memory_tracker *it = start;

  while (it != NULL) {
    if (!it->freed) {
      printf("LEAK: %zu bytes (Allocated in '%s' at line %zu)\n", it->size, it->file, it->line_number);
      leaked_bytes += it->size;
    }

    struct su_memory_tracker *next = it->next;
    free(it);
    it = next;
  }

  if (leaked_bytes == 0)
    return false;

  printf("---------------------------\n");
  printf("> Total Leaked: %zu bytes\n", leaked_bytes);
  printf("---------------------------\n");

  start = end = NULL;
  return true;
}

void *su_memory_alloc(const size_t size, const char *file, const size_t line_number) {
  void *memory = malloc(size);
  su_memory_track(memory, size, file, line_number);

  return memory;
}

void *su_memory_realloc(void *ptr, const size_t size, const char *file, const size_t line_number) {
  if (size == 0) {
    su_memory_untrack(ptr);
    free(ptr);
    return NULL;
  }

  if (ptr != NULL)
    su_memory_untrack(ptr);

  void *new_ptr = realloc(ptr, size);
  if (new_ptr != NULL)
    su_memory_track(new_ptr, size, file, line_number);

  return new_ptr;
}

void *su_memory_calloc(const size_t num_elements, const size_t size_elements, const char *file, const size_t line_number) {
  void *memory = calloc(num_elements, size_elements);
  su_memory_track(memory, num_elements * size_elements, file, line_number);
  return memory;
}

void *su_memory_memset(void *ptr, const int value, const size_t size) {
  return memset(ptr, value, size);
}

void *su_memory_memcpy(void *dest, const void *src, const size_t size) {
  return memcpy(dest, src, size);
}

int su_memory_strcmp(const char *first, const char *second) {
  return strcmp(first, second);
}

char *su_memory_strdup(const char *str, const char *file, const size_t line_number) {
  char *memory = strdup(str);
  su_memory_track(memory, strlen(str) * sizeof(char), file, line_number);
  return memory;
}

size_t su_memory_strlen(const char *str) {
  return strlen(str);
}

void su_memory_free(void *ptr) {
  su_memory_untrack(ptr);
  free(ptr);
}