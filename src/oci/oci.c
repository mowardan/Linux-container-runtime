#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "oci.h"
#include "utils.h"
#include "log.h"
#include "error.h"

#ifdef __linux__
#include <sys/mount.h>
#endif

int oci_setup_overlayfs(const char *lowerdir, const char *upperdir, const char *workdir, const char *target) {
#ifdef __linux__
    if (!lowerdir || !upperdir || !workdir || !target) {
        return MYRUN_ERR_INVALID_ARG;
    }

    char data[1024];
    snprintf(data, sizeof(data), "lowerdir=%s,upperdir=%s,workdir=%s", lowerdir, upperdir, workdir);

    LOG_DEBUG("Mounting OverlayFS onto '%s' with options: %s", target, data);
    if (mount("overlay", target, "overlay", 0, data) < 0) {
        LOG_ERROR("OverlayFS mount failed: %s (errno %d)", strerror(errno), errno);
        return MYRUN_ERR_MOUNT;
    }

    LOG_INFO("Successfully mounted OverlayFS rootfs at '%s'", target);
    return MYRUN_SUCCESS;
#else
    (void)lowerdir;
    (void)upperdir;
    (void)workdir;
    (void)target;
    return MYRUN_SUCCESS;
#endif
}

int oci_load_bundle(const char *bundle_path, struct oci_bundle_spec *spec) {
    if (!bundle_path || !spec) return MYRUN_ERR_INVALID_ARG;
    memset(spec, 0, sizeof(*spec));

    char config_file[512];
    snprintf(config_file, sizeof(config_file), "%s/config.json", bundle_path);

    FILE *f = fopen(config_file, "r");
    if (!f) {
        LOG_WARN("Could not open OCI config.json at '%s', assuming default rootfs layout", config_file);
        snprintf(spec->rootfs_path, sizeof(spec->rootfs_path), "%s/rootfs", bundle_path);
        strcpy(spec->cwd, "/");
        spec->argc = 1;
        spec->argv = calloc(2, sizeof(char *));
        spec->argv[0] = myrun_strdup("/bin/sh");
        return MYRUN_SUCCESS;
    }

    /* Basic JSON scanning for root.path, process.cwd, process.args, hostname */
    snprintf(spec->rootfs_path, sizeof(spec->rootfs_path), "%s/rootfs", bundle_path);
    strcpy(spec->cwd, "/");
    strcpy(spec->hostname, "oci-container");

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char val[256];
        if (strstr(line, "\"path\":") && sscanf(line, " \"path\": \"%255[^\"]\"", val) == 1) {
            if (val[0] == '/') {
                snprintf(spec->rootfs_path, sizeof(spec->rootfs_path), "%s", val);
            } else {
                snprintf(spec->rootfs_path, sizeof(spec->rootfs_path), "%s/%s", bundle_path, val);
            }
        } else if (strstr(line, "\"cwd\":") && sscanf(line, " \"cwd\": \"%255[^\"]\"", val) == 1) {
            snprintf(spec->cwd, sizeof(spec->cwd), "%s", val);
        } else if (strstr(line, "\"hostname\":") && sscanf(line, " \"hostname\": \"%63[^\"]\"", val) == 1) {
            snprintf(spec->hostname, sizeof(spec->hostname), "%.63s", val);
        }
    }
    fclose(f);

    /* Default fallback argv if none extracted */
    if (!spec->argv) {
        spec->argc = 1;
        spec->argv = calloc(2, sizeof(char *));
        spec->argv[0] = myrun_strdup("/bin/sh");
    }

    return MYRUN_SUCCESS;
}

void oci_free_spec(struct oci_bundle_spec *spec) {
    if (!spec) return;
    if (spec->argv) {
        for (int i = 0; i < spec->argc; i++) {
            free(spec->argv[i]);
        }
        free(spec->argv);
        spec->argv = NULL;
    }
    if (spec->envp) {
        for (int i = 0; i < spec->envc; i++) {
            free(spec->envp[i]);
        }
        free(spec->envp);
        spec->envp = NULL;
    }
}
