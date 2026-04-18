#ifndef SUTURE_TRANSFORM_H
#define SUTURE_TRANSFORM_H

#include <suture.h>

#ifdef __cplusplus
extern "C" {
#endif

enum su_error su_transform_init(const struct su_env *env);

enum su_error su_transform_dispose(struct su_env *env);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_TRANSFORM_H
