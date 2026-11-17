#ifndef SUTURE_FLAG_H
#define SUTURE_FLAG_H

#include "error.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum su_error su_flag_patchb(const char *name, bool *original, bool value);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_FLAG_H
