/**
 * @file flag.h
 * @brief JVM flag manipulation interface.
 */

#ifndef SUTURE_FLAG_H
#define SUTURE_FLAG_H

#include "error.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Patches a JVM boolean flag by name.
 *
 * Walks the HotSpot VM structs to locate the named flag in the JVM flag table
 * and overwrites its value. Supports both legacy (@c Flag) and modern (@c JVMFlag) layouts.
 *
 * @param name     Name of the JVM flag to patch. Must not be NULL.
 * @param original If non-NULL, receives the flag's original value before patching.
 * @param value    New boolean value to write to the flag.
 *
 * @return @ref SU_OK                        on success.
 * @return @ref SU_MISSING_REQUIRED_PARAMETERS if @p name is NULL.
 * @return @ref SU_DETOUR_NOT_SUPPORTED      if the flag table or target flag could not be located.
 */
enum su_error su_flag_patchb(const char *name, bool *original, bool value);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_FLAG_H
