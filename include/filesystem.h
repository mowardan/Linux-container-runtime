#ifndef MYRUN_FILESYSTEM_H
#define MYRUN_FILESYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include "error.h"

/**
 * @file filesystem.h
 * @brief Root filesystem isolation (pivot_root, chroot) and pseudo-filesystems mounting.
 */

struct bind_mount {
    char *source;
    char *target;
    bool readonly;
};

struct filesystem_spec {
    const char *rootfs;                /**< Path to rootfs directory */
    bool readonly_root;                /**< Remount rootfs read-only */
    const struct bind_mount *binds;    /**< Array of bind mounts */
    size_t bind_count;                 /**< Number of bind mounts */
};

/**
 * @brief Validates that a rootfs path exists and looks like a valid root directory.
 *
 * @param rootfs Path to check.
 * @return MYRUN_SUCCESS on success, or MYRUN_ERR_INVALID_ARG / MYRUN_ERR_NOT_FOUND.
 */
int filesystem_validate_rootfs(const char *rootfs);

/**
 * @brief Sets up pseudo-filesystems inside the container rootfs (/proc, /sys, /dev, etc.).
 *
 * Must be called after pivot_root / chroot has switched the root to the container rootfs.
 *
 * @return MYRUN_SUCCESS on success, or an error code.
 */
int filesystem_mount_pseudo(void);

/**
 * @brief Performs pivot_root transition to isolate container filesystem.
 *
 * 1. Ensures rootfs is a mount point (bind mount to itself).
 * 2. Creates a temporary old_root directory.
 * 3. Calls pivot_root(new_root, old_root).
 * 4. Chdirs to "/".
 * 5. Unmounts and removes old_root.
 *
 * @param rootfs Path to container new root directory.
 * @return MYRUN_SUCCESS on success, or MYRUN_ERR_MOUNT on failure.
 */
int filesystem_pivot_root(const char *rootfs);

/**
 * @brief Fallback filesystem isolation using chroot when pivot_root is unavailable.
 *
 * @param rootfs Path to container root directory.
 * @return MYRUN_SUCCESS on success, or MYRUN_ERR_MOUNT on failure.
 */
int filesystem_chroot(const char *rootfs);

/**
 * @brief Applies bind mounts into the container rootfs.
 *
 * @param rootfs Base path to container rootfs (if before pivot) or "/" (if after pivot).
 * @param binds Array of bind mounts.
 * @param bind_count Number of bind mounts.
 * @return MYRUN_SUCCESS on success, or MYRUN_ERR_MOUNT on failure.
 */
int filesystem_apply_bind_mounts(const char *rootfs, const struct bind_mount *binds, size_t bind_count);

#endif /* MYRUN_FILESYSTEM_H */
