#ifndef MYRUN_SECURITY_H
#define MYRUN_SECURITY_H

#include <stdbool.h>
#include "error.h"

/**
 * @file security.h
 * @brief Security isolation primitives: PR_SET_NO_NEW_PRIVS, capability dropping, and seccomp.
 */

struct security_config {
    bool no_new_privs;
    bool drop_capabilities;
    bool seccomp_filter;
};

/**
 * @brief Enforces PR_SET_NO_NEW_PRIVS preventing child processes from acquiring privileges via setuid/setgid.
 */
int security_set_no_new_privs(void);

/**
 * @brief Drops dangerous Linux capabilities from bounding and effective sets.
 */
int security_drop_capabilities(void);

/**
 * @brief Installs default container Seccomp BPF filter.
 */
int security_apply_seccomp(void);

/**
 * @brief Applies all requested security constraints inside child before exec.
 */
int security_apply_all(const struct security_config *sec);

#endif /* MYRUN_SECURITY_H */
