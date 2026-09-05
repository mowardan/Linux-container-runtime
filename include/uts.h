#ifndef MYRUN_UTS_H
#define MYRUN_UTS_H

#include <stddef.h>
#include <stdbool.h>
#include "error.h"

/**
 * @file uts.h
 * @brief UTS namespace isolation, hostname configuration and validation APIs.
 */

#define MYRUN_DEFAULT_HOSTNAME "myrun"
#define MYRUN_MAX_HOSTNAME_LEN 64

/**
 * @brief Validates a container hostname string.
 *
 * Enforces hostname constraints:
 * - Non-null and non-empty.
 * - Maximum 64 characters (matching Linux HOST_NAME_MAX and struct utsname).
 * - No slashes ('/').
 * - No control characters (ASCII < 32 or == 127).
 * - No whitespace.
 * - Only permitted hostname characters (alphanumeric, hyphens, dots, underscores).
 *
 * @param hostname String to validate.
 * @return MYRUN_SUCCESS (0) if valid, MYRUN_ERR_INVALID_ARG if invalid.
 */
int hostname_validate(const char *hostname);

/**
 * @brief Sets up UTS namespace by validating and applying the container hostname.
 *
 * Calls direct sethostname() syscall in the child container process.
 *
 * @param hostname Target hostname to configure.
 * @return MYRUN_SUCCESS (0) on success, or an appropriate error code.
 */
int uts_namespace_setup(const char *hostname);

#endif /* MYRUN_UTS_H */
