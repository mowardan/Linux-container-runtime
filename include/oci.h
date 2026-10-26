#ifndef MYRUN_OCI_H
#define MYRUN_OCI_H

#include <stdbool.h>
#include <stddef.h>
#include "error.h"

/**
 * @file oci.h
 * @brief OCI bundle config.json parser and OverlayFS container rootfs setup.
 */

struct oci_bundle_spec {
    char rootfs_path[512];
    char cwd[512];
    char **argv;
    int argc;
    char **envp;
    int envc;
    char hostname[256];
    bool readonly_rootfs;
};

/**
 * @brief Parses an OCI bundle's config.json at bundle_path/config.json.
 */
int oci_load_bundle(const char *bundle_path, struct oci_bundle_spec *spec);

/**
 * @brief Frees resources in an oci_bundle_spec.
 */
void oci_free_spec(struct oci_bundle_spec *spec);

/**
 * @brief Sets up an OverlayFS mount combining lowerdir, upperdir, and workdir onto target.
 */
int oci_setup_overlayfs(const char *lowerdir, const char *upperdir, const char *workdir, const char *target);

#endif /* MYRUN_OCI_H */
