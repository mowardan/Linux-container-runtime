#ifndef MYRUN_CONTAINER_H
#define MYRUN_CONTAINER_H

#include <sys/types.h>
#include <stdbool.h>
#include "error.h"

/**
 * @file container.h
 * @brief OCI container lifecycle: state, create, start, kill, delete, and exec.
 */

typedef enum {
    CONTAINER_STATUS_CREATING = 0,
    CONTAINER_STATUS_CREATED,
    CONTAINER_STATUS_RUNNING,
    CONTAINER_STATUS_STOPPED
} container_status_t;

struct container_state {
    char id[128];
    pid_t init_process_pid;
    container_status_t status;
    char bundle_path[512];
    char rootfs[512];
};

/**
 * @brief Saves container state to /run/myrun/<id>/state.json
 */
int container_save_state(const struct container_state *state);

/**
 * @brief Loads container state from /run/myrun/<id>/state.json
 */
int container_load_state(const char *id, struct container_state *state);

/**
 * @brief Deletes container state directory from /run/myrun/<id>
 */
int container_delete_state(const char *id);

/**
 * @brief Enters an existing container's namespaces using setns() and executes cmd.
 */
int container_exec(const char *id, int argc, char *argv[]);

/**
 * @brief Sends a signal to the container init process.
 */
int container_kill(const char *id, int signal);

#endif /* MYRUN_CONTAINER_H */
