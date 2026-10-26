#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "filesystem.h"
#include "log.h"
#include "error.h"

#ifdef __linux__
#include <sys/mount.h>
#include <sys/syscall.h>

#ifndef SYS_pivot_root
#if defined(__x86_64__)
#define SYS_pivot_root 155
#elif defined(__aarch64__)
#define SYS_pivot_root 41
#endif
#endif

static int sys_pivot_root(const char *new_root, const char *put_old) {
#ifdef SYS_pivot_root
    return syscall(SYS_pivot_root, new_root, put_old);
#else
    errno = ENOSYS;
    return -1;
#endif
}
#endif

int filesystem_validate_rootfs(const char *rootfs) {
    if (!rootfs || strlen(rootfs) == 0) {
        LOG_ERROR("rootfs path is NULL or empty");
        return MYRUN_ERR_INVALID_ARG;
    }

    struct stat st;
    if (stat(rootfs, &st) < 0) {
        LOG_ERROR("Cannot access rootfs '%s': %s", rootfs, strerror(errno));
        return (errno == ENOENT) ? MYRUN_ERR_NOT_FOUND : MYRUN_ERR_PERMISSION;
    }

    if (!S_ISDIR(st.st_mode)) {
        LOG_ERROR("rootfs '%s' is not a directory", rootfs);
        return MYRUN_ERR_INVALID_ARG;
    }

    return MYRUN_SUCCESS;
}

int filesystem_mount_pseudo(void) {
#ifdef __linux__
    LOG_DEBUG("Mounting container pseudo-filesystems (/proc, /sys, /dev, /dev/pts, /dev/shm)...");

    /* Ensure mount points exist */
    mkdir("/proc", 0755);
    mkdir("/sys", 0755);
    mkdir("/dev", 0755);
    mkdir("/dev/pts", 0755);
    mkdir("/dev/shm", 1777);

    /* 1. Mount /proc */
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL) < 0) {
        /* If already mounted or EBUSY, log and continue, otherwise report error */
        if (errno != EBUSY) {
            LOG_ERROR("Failed to mount /proc: %s (errno %d)", strerror(errno), errno);
            return MYRUN_ERR_MOUNT;
        }
    }

    /* 2. Mount /sys */
    if (mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RDONLY, NULL) < 0) {
        if (errno != EBUSY) {
            LOG_WARN("Failed to mount /sys: %s (errno %d)", strerror(errno), errno);
        }
    }

    /* 3. Mount /dev as tmpfs */
    if (mount("tmpfs", "/dev", "tmpfs", MS_NOSUID | MS_STRICTATIME, "mode=755") < 0) {
        if (errno != EBUSY) {
            LOG_WARN("Failed to mount /dev tmpfs: %s", strerror(errno));
        }
    }

    /* 4. Re-create /dev/pts and mount devpts */
    mkdir("/dev/pts", 0755);
    if (mount("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC, "newinstance,ptmxmode=0666,mode=620") < 0) {
        if (errno != EBUSY) {
            LOG_WARN("Failed to mount /dev/pts: %s", strerror(errno));
        }
    }

    /* 5. Re-create /dev/shm and mount tmpfs */
    mkdir("/dev/shm", 1777);
    if (mount("shm", "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, "mode=1777,size=65536k") < 0) {
        if (errno != EBUSY) {
            LOG_WARN("Failed to mount /dev/shm: %s", strerror(errno));
        }
    }

    /* Standard symlinks in /dev */
    symlink("/proc/self/fd", "/dev/fd");
    symlink("/proc/self/fd/0", "/dev/stdin");
    symlink("/proc/self/fd/1", "/dev/stdout");
    symlink("/proc/self/fd/2", "/dev/stderr");
    symlink("/dev/pts/ptmx", "/dev/ptmx");

    LOG_DEBUG("Successfully configured container pseudo-filesystems");
    return MYRUN_SUCCESS;
#else
    LOG_WARN("Pseudo-filesystem mounting is only supported on Linux; skipped on host");
    return MYRUN_SUCCESS;
#endif
}

int filesystem_pivot_root(const char *rootfs) {
#ifdef __linux__
    if (!rootfs) {
        return MYRUN_ERR_INVALID_ARG;
    }

    int val = filesystem_validate_rootfs(rootfs);
    if (val != MYRUN_SUCCESS) {
        return val;
    }

    LOG_DEBUG("Preparing pivot_root with new rootfs '%s'", rootfs);

    /* Step 1: Ensure new_root is a mount point by bind mounting it onto itself */
    if (mount(rootfs, rootfs, "bind", MS_BIND | MS_REC, NULL) < 0) {
        LOG_ERROR("Failed to bind mount rootfs '%s' onto itself: %s", rootfs, strerror(errno));
        return MYRUN_ERR_MOUNT;
    }

    /* Step 2: Create put_old directory inside rootfs */
    char put_old[1024];
    snprintf(put_old, sizeof(put_old), "%s/.old_root_%d", rootfs, (int)getpid());
    if (mkdir(put_old, 0700) < 0 && errno != EEXIST) {
        LOG_ERROR("Failed to create temporary put_old directory '%s': %s", put_old, strerror(errno));
        return MYRUN_ERR_MOUNT;
    }

    /* Step 3: Call pivot_root */
    if (sys_pivot_root(rootfs, put_old) < 0) {
        LOG_WARN("pivot_root failed: %s; attempting chroot fallback...", strerror(errno));
        rmdir(put_old);
        return filesystem_chroot(rootfs);
    }

    /* Step 4: Chdir to the new root directory */
    if (chdir("/") < 0) {
        LOG_ERROR("Failed to chdir to '/' after pivot_root: %s", strerror(errno));
        return MYRUN_ERR_MOUNT;
    }

    /* Step 5: Unmount and clean up old_root */
    char old_root_rel[256];
    snprintf(old_root_rel, sizeof(old_root_rel), "/.old_root_%d", (int)getpid());

    if (umount2(old_root_rel, MNT_DETACH) < 0) {
        LOG_WARN("Failed to unmount old root '%s': %s", old_root_rel, strerror(errno));
    }
    if (rmdir(old_root_rel) < 0) {
        LOG_WARN("Failed to remove old root directory '%s': %s", old_root_rel, strerror(errno));
    }

    LOG_DEBUG("Successfully performed pivot_root to new rootfs");
    return MYRUN_SUCCESS;
#else
    (void)rootfs;
    LOG_WARN("pivot_root is only supported on Linux; skipped on host");
    return MYRUN_SUCCESS;
#endif
}

int filesystem_chroot(const char *rootfs) {
#ifdef __linux__
    if (!rootfs) {
        return MYRUN_ERR_INVALID_ARG;
    }

    if (chroot(rootfs) < 0) {
        LOG_ERROR("chroot to '%s' failed: %s", rootfs, strerror(errno));
        return MYRUN_ERR_MOUNT;
    }

    if (chdir("/") < 0) {
        LOG_ERROR("chdir to '/' after chroot failed: %s", strerror(errno));
        return MYRUN_ERR_MOUNT;
    }

    LOG_DEBUG("Successfully changed root to '%s' using chroot", rootfs);
    return MYRUN_SUCCESS;
#else
    (void)rootfs;
    return MYRUN_SUCCESS;
#endif
}

int filesystem_apply_bind_mounts(const char *rootfs, const struct bind_mount *binds, size_t bind_count) {
#ifdef __linux__
    if (!binds || bind_count == 0) {
        return MYRUN_SUCCESS;
    }

    for (size_t i = 0; i < bind_count; i++) {
        const struct bind_mount *bm = &binds[i];
        if (!bm->source || !bm->target) {
            continue;
        }

        char full_target[1024];
        if (rootfs && strcmp(rootfs, "/") != 0) {
            snprintf(full_target, sizeof(full_target), "%s/%s", rootfs,
                     (bm->target[0] == '/') ? bm->target + 1 : bm->target);
        } else {
            snprintf(full_target, sizeof(full_target), "%s", bm->target);
        }

        /* Ensure destination directory or file exists */
        struct stat src_st;
        if (stat(bm->source, &src_st) < 0) {
            LOG_ERROR("Bind mount source '%s' not accessible: %s", bm->source, strerror(errno));
            return MYRUN_ERR_NOT_FOUND;
        }

        if (S_ISDIR(src_st.st_mode)) {
            mkdir(full_target, 0755);
        } else {
            FILE *f = fopen(full_target, "a");
            if (f) fclose(f);
        }

        unsigned long flags = MS_BIND | MS_REC;
        if (mount(bm->source, full_target, NULL, flags, NULL) < 0) {
            LOG_ERROR("Failed to bind mount '%s' -> '%s': %s",
                      bm->source, full_target, strerror(errno));
            return MYRUN_ERR_MOUNT;
        }

        if (bm->readonly) {
            flags |= MS_REMOUNT | MS_RDONLY;
            if (mount(NULL, full_target, NULL, flags, NULL) < 0) {
                LOG_WARN("Failed to remount '%s' read-only: %s", full_target, strerror(errno));
            }
        }

        LOG_DEBUG("Applied bind mount: %s -> %s %s",
                  bm->source, full_target, bm->readonly ? "(ro)" : "(rw)");
    }

    return MYRUN_SUCCESS;
#else
    (void)rootfs;
    (void)binds;
    (void)bind_count;
    return MYRUN_SUCCESS;
#endif
}
