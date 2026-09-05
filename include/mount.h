#ifndef MYRUN_MOUNT_H
#define MYRUN_MOUNT_H

#include "error.h"

/**
 * @file mount.h
 * @brief Linux Mount Namespace isolation and mount propagation configuration APIs.
 */

/**
 * @brief Configures the container mount namespace.
 *
 * Remounts the root filesystem tree with MS_REC | MS_PRIVATE so that any mounts
 * or unmounts performed inside this container do not propagate back to the host
 * or to other containers.
 *
 * @return MYRUN_SUCCESS (0) on success, or MYRUN_ERR_MOUNT on failure.
 */
int mount_namespace_setup(void);

#endif /* MYRUN_MOUNT_H */
