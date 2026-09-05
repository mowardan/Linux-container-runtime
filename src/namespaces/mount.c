#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "mount.h"
#include "log.h"
#include "error.h"

#ifdef __linux__
#include <sys/mount.h>
#endif

int mount_namespace_setup(void) {
#ifdef __linux__
    /*
     * When a process creates a new mount namespace (CLONE_NEWNS), by default
     * systemd sets the root mount propagation to MS_SHARED.
     * To ensure that all container mounts, unmounts, or pivot_root actions remain
     * strictly private to this container, remount the entire hierarchy recursively
     * as MS_PRIVATE.
     */
    LOG_DEBUG("Configuring container mount namespace: remounting '/' recursively as MS_PRIVATE...");

    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) {
        int err = errno;
        LOG_ERROR("Failed to remount '/' with MS_REC | MS_PRIVATE: %s (errno %d)",
                  strerror(err), err);
        return MYRUN_ERR_MOUNT;
    }

    LOG_DEBUG("Successfully configured private mount propagation (MS_REC | MS_PRIVATE) on '/'");
    return MYRUN_SUCCESS;
#else
    LOG_WARN("Mount namespace isolation is only supported on Linux; skipped on host");
    return MYRUN_SUCCESS;
#endif
}
