#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "runtime.h"
#include "process.h"
#include "namespace.h"
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
    spec.envp = NULL; /* Inherit parent environment for now */
    spec.cwd = config->cwd;

    LOG_INFO("Spawning container process '%s' in new PID namespace...", spec.command);

    pid_t child_pid = 0;
    void *stack_base = NULL;
    int ns_flags = MYRUN_NS_PID; /* Phase 2: Isolate PID namespace */

    int err = namespace_spawn(&spec, ns_flags, &child_pid, &stack_base);
    if (err != MYRUN_SUCCESS) {
        if (err == MYRUN_ERR_NOT_FOUND) {
            return 127;
        } else if (err == MYRUN_ERR_PERMISSION) {
            return 126;
        } else if (err == MYRUN_ERR_CLONE) {
            return 1;
        }
        return 1;
    }

    /* Initialize signal forwarding for the container child process */
    if (process_init_signals(child_pid) != MYRUN_SUCCESS) {
        LOG_WARN("Could not initialize signal forwarding for PID %d", child_pid);
    }

    /* Wait for container process termination */
    struct process_result result;
    memset(&result, 0, sizeof(result));
    err = process_wait(child_pid, &result);

    /* Free child stack only AFTER child has completely terminated and been reaped */
    if (stack_base) {
        namespace_free_stack(stack_base, MYRUN_CHILD_STACK_SIZE);
        stack_base = NULL;
    }

    /* Restore parent signal handlers */
    process_restore_signals();

    if (err != MYRUN_SUCCESS) {
        LOG_ERROR("Failed waiting for container process %d", child_pid);
        return 1;
    }

    LOG_DEBUG("Container process exited with status %d", result.exit_code);
    return result.exit_code;
}
