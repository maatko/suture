#ifndef SUTURE_TRACKER_H
#define SUTURE_TRACKER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef NDEBUG
#  include <stdio.h>
#  include <string.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

bool su_memory_print();

void *su_memory_alloc(size_t size, const char *file, size_t line_number);

void *su_memory_realloc(void *ptr, size_t size, const char *file, size_t line_number);

void *su_memory_calloc(size_t num_elements, size_t size_elements, const char *file, size_t line_number);

void *su_memory_memset(void *ptr, int value, size_t size);

void *su_memory_memcpy(void *dest, const void *src, size_t size);

int su_memory_strcmp(const char *first, const char *second);

char *su_memory_strdup(const char *str, const char *file, size_t line_number);

size_t su_memory_strlen(const char *str);

void su_memory_free(void *ptr);

#ifdef __cplusplus
}
#endif

#ifndef NDEBUG
#  define print_leaks() su_memory_print()
#  define malloc(size) su_memory_alloc(size, __FILE__, __LINE__)
#  define realloc(ptr, size) su_memory_realloc(ptr, size, __FILE__, __LINE__)
#  define calloc(num_elements, size) su_memory_calloc(num_elements, size, __FILE__, __LINE__)
#  define memset(ptr, value, size) su_memory_memset(ptr, value, size)
#  define memcpy(dest, src, size) su_memory_memcpy(dest, src, size)
#  define strcmp(first, second) su_memory_strcmp(first, second)
#  define strdup(str) su_memory_strdup(str, __FILE__, __LINE__)
#  define strlen(str) su_memory_strlen(str)
#  define free(ptr) su_memory_free(ptr)
#else
#  define print_leaks
#endif

#endif // SUTURE_TRACKER_H
