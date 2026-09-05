#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "runtime.h"
#include "process.h"
#include "log.h"
#include "error.h"

int runtime_run(const struct myrun_config *config) {
    if (!config || !config->command_argv || config->command_argc <= 0) {
        LOG_ERROR("Invalid configuration passed to runtime_run");
        return MYRUN_ERR_INVALID_ARG;
    }

    struct process_spec spec;
    memset(&spec, 0, sizeof(spec));
    spec.command = config->command_argv[0];
    spec.argv = config->command_argv;
    spec.envp = NULL; /* Inherit parent environment for Phase 1 */
    spec.cwd = config->cwd;

    LOG_INFO("Starting process '%s'...", spec.command);

    pid_t child_pid = 0;
    int err = process_spawn(&spec, &child_pid);
    if (err != MYRUN_SUCCESS) {
        if (err == MYRUN_ERR_NOT_FOUND) {
            return 127;
        } else if (err == MYRUN_ERR_PERMISSION) {
            return 126;
        }
        return 1;
    }

    /* Initialize signal forwarding for active child process */
    if (process_init_signals(child_pid) != MYRUN_SUCCESS) {
        LOG_WARN("Could not initialize signal forwarding for PID %d", child_pid);
    }

    /* Wait for child process completion */
    struct process_result result;
    memset(&result, 0, sizeof(result));
    err = process_wait(child_pid, &result);

    /* Restore parent signal handlers */
    process_restore_signals();

    if (err != MYRUN_SUCCESS) {
        LOG_ERROR("Failed waiting for child process %d", child_pid);
        return 1;
    }

    LOG_DEBUG("Process exited with code %d", result.exit_code);
    return result.exit_code;
}
