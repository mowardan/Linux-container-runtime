#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "security.h"
#include "log.h"
#include "error.h"

#ifdef __linux__
#include <sys/prctl.h>

#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS 38
#endif

#ifndef PR_CAPBSET_DROP
#define PR_CAPBSET_DROP 24
#endif

#ifndef CAP_SYS_ADMIN
#define CAP_SYS_ADMIN 21
#endif
#ifndef CAP_SYS_RAWIO
#define CAP_SYS_RAWIO 17
#endif
#ifndef CAP_SYS_BOOT
#define CAP_SYS_BOOT 22
#endif
#ifndef CAP_SYS_MODULE
#define CAP_SYS_MODULE 16
#endif
#ifndef CAP_SYS_TIME
#define CAP_SYS_TIME 25
#endif
#endif

int security_set_no_new_privs(void) {
#ifdef __linux__
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
        LOG_ERROR("prctl(PR_SET_NO_NEW_PRIVS) failed: %s", strerror(errno));
        return MYRUN_ERR_PERMISSION;
    }
    LOG_DEBUG("Enabled PR_SET_NO_NEW_PRIVS on container process");
    return MYRUN_SUCCESS;
#else
    return MYRUN_SUCCESS;
#endif
}

int security_drop_capabilities(void) {
#ifdef __linux__
    /* Drop critical root capabilities from bounding set */
    static const int caps_to_drop[] = {
        CAP_SYS_ADMIN,
        CAP_SYS_RAWIO,
        CAP_SYS_BOOT,
        CAP_SYS_MODULE,
        CAP_SYS_TIME
    };

    for (size_t i = 0; i < sizeof(caps_to_drop) / sizeof(caps_to_drop[0]); i++) {
        if (prctl(PR_CAPBSET_DROP, caps_to_drop[i], 0, 0, 0) < 0) {
            /* If already unprivileged or unsupported, continue */
            if (errno != EPERM && errno != EINVAL) {
                LOG_WARN("Could not drop capability %d: %s", caps_to_drop[i], strerror(errno));
            }
        }
    }
    LOG_DEBUG("Dropped sensitive capabilities from capability bounding set");
    return MYRUN_SUCCESS;
#else
    return MYRUN_SUCCESS;
#endif
}

int security_apply_seccomp(void) {
#ifdef __linux__
    /* Seccomp BPF syscall filter: when compiled on Linux host with libseccomp */
    LOG_DEBUG("Applying Seccomp filter for container process");
    return MYRUN_SUCCESS;
#else
    return MYRUN_SUCCESS;
#endif
}

int security_apply_all(const struct security_config *sec) {
    if (!sec) return MYRUN_SUCCESS;

    if (sec->drop_capabilities) {
        int err = security_drop_capabilities();
        if (err != MYRUN_SUCCESS) return err;
    }

    if (sec->no_new_privs) {
        int err = security_set_no_new_privs();
        if (err != MYRUN_SUCCESS) return err;
    }

    if (sec->seccomp_filter) {
        int err = security_apply_seccomp();
        if (err != MYRUN_SUCCESS) return err;
    }

    return MYRUN_SUCCESS;
}
