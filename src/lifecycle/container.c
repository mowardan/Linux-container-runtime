#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "container.h"
#include "log.h"
#include "error.h"

#ifdef __linux__
#include <sched.h>
#endif

#ifdef __linux__
#define STATE_BASE_DIR "/run/myrun"
#else
#define STATE_BASE_DIR "/tmp/myrun"
#endif

static const char *status_str(container_status_t s) {
    switch (s) {
        case CONTAINER_STATUS_CREATING: return "creating";
        case CONTAINER_STATUS_CREATED:  return "created";
        case CONTAINER_STATUS_RUNNING:  return "running";
        case CONTAINER_STATUS_STOPPED:  return "stopped";
        default: return "unknown";
    }
}

int container_save_state(const struct container_state *state) {
    if (!state || !state->id[0]) return MYRUN_ERR_INVALID_ARG;

    mkdir(STATE_BASE_DIR, 0755);
    char dir[256];
    snprintf(dir, sizeof(dir), "%s/%s", STATE_BASE_DIR, state->id);
    mkdir(dir, 0755);

    char path[512];
    snprintf(path, sizeof(path), "%s/state.json", dir);

    FILE *f = fopen(path, "w");
    if (!f) {
        LOG_ERROR("Failed opening state file '%s': %s", path, strerror(errno));
        return MYRUN_ERR_GENERIC;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"ociVersion\": \"1.0.2\",\n");
    fprintf(f, "  \"id\": \"%s\",\n", state->id);
    fprintf(f, "  \"status\": \"%s\",\n", status_str(state->status));
    fprintf(f, "  \"pid\": %d,\n", (int)state->init_process_pid);
    fprintf(f, "  \"bundle\": \"%s\",\n", state->bundle_path);
    fprintf(f, "  \"rootfs\": \"%s\"\n", state->rootfs);
    fprintf(f, "}\n");

    fclose(f);
    LOG_DEBUG("Saved state for container '%s' (PID %d, status %s)",
              state->id, (int)state->init_process_pid, status_str(state->status));
    return MYRUN_SUCCESS;
}

int container_load_state(const char *id, struct container_state *state) {
    if (!id || !state) return MYRUN_ERR_INVALID_ARG;
    memset(state, 0, sizeof(*state));

    char path[512];
    snprintf(path, sizeof(path), "%s/%s/state.json", STATE_BASE_DIR, id);

    FILE *f = fopen(path, "r");
    if (!f) {
        LOG_ERROR("Container '%s' not found at '%s'", id, path);
        return MYRUN_ERR_NOT_FOUND;
    }

    strncpy(state->id, id, sizeof(state->id) - 1);

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        int pid_val;
        char s_val[64];
        if (sscanf(line, " \"pid\": %d,", &pid_val) == 1) {
            state->init_process_pid = pid_val;
        } else if (sscanf(line, " \"status\": \"%63[^\"]\",", s_val) == 1) {
            if (strcmp(s_val, "running") == 0) state->status = CONTAINER_STATUS_RUNNING;
            else if (strcmp(s_val, "created") == 0) state->status = CONTAINER_STATUS_CREATED;
            else state->status = CONTAINER_STATUS_STOPPED;
        }
    }
    fclose(f);
    return MYRUN_SUCCESS;
}

int container_delete_state(const char *id) {
    if (!id) return MYRUN_ERR_INVALID_ARG;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s/state.json", STATE_BASE_DIR, id);
    unlink(path);
    snprintf(path, sizeof(path), "%s/%s", STATE_BASE_DIR, id);
    rmdir(path);
    return MYRUN_SUCCESS;
}

int container_kill(const char *id, int signal) {
    struct container_state state;
    int err = container_load_state(id, &state);
    if (err != MYRUN_SUCCESS) return err;

    if (state.init_process_pid <= 0) {
        LOG_ERROR("Invalid container init PID");
        return MYRUN_ERR_GENERIC;
    }

    if (kill(state.init_process_pid, signal) < 0) {
        LOG_ERROR("Failed to send signal %d to PID %d: %s", signal, state.init_process_pid, strerror(errno));
        return MYRUN_ERR_SIGNAL;
    }

    LOG_INFO("Sent signal %d to container '%s' (PID %d)", signal, id, state.init_process_pid);
    return MYRUN_SUCCESS;
}

int container_exec(const char *id, int argc, char *argv[]) {
#ifdef __linux__
    (void)argc;
    struct container_state state;
    int err = container_load_state(id, &state);
    if (err != MYRUN_SUCCESS) return err;

    pid_t target_pid = state.init_process_pid;
    if (target_pid <= 0) {
        LOG_ERROR("Container init process PID is invalid");
        return MYRUN_ERR_GENERIC;
    }

    /* Join namespaces using setns */
    const char *ns_types[] = {"ipc", "uts", "net", "pid", "mnt"};
    for (size_t i = 0; i < sizeof(ns_types)/sizeof(ns_types[0]); i++) {
        char ns_path[256];
        snprintf(ns_path, sizeof(ns_path), "/proc/%d/ns/%s", target_pid, ns_types[i]);
        int fd = open(ns_path, O_RDONLY);
        if (fd >= 0) {
            if (setns(fd, 0) < 0) {
                LOG_WARN("setns failed for %s: %s", ns_path, strerror(errno));
            }
            close(fd);
        }
    }

    /* Fork into the target PID namespace to get a child inside it */
    pid_t child = fork();
    if (child == 0) {
        execvp(argv[0], argv);
        LOG_ERROR("execvp failed: %s", strerror(errno));
        _exit(127);
    }

    int status;
    waitpid(child, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#else
    (void)id;
    (void)argc;
    (void)argv;
    LOG_WARN("container_exec (setns) is only supported on Linux");
    return MYRUN_SUCCESS;
#endif
}
