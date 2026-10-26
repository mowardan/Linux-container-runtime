#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "runtime.h"
#include "process.h"
#include "namespace.h"
#include "cgroup.h"
#include "network.h"
#include "container.h"
#include "log.h"
#include "error.h"

int runtime_run(const struct myrun_config *config) {
    if (!config || !config->command_argv || config->command_argc <= 0) {
        LOG_ERROR("Invalid configuration passed to runtime_run");
        return MYRUN_ERR_INVALID_ARG;
    }

    /* Generate container ID if not given */
    char cid[64];
    if (config->container_id) {
        strncpy(cid, config->container_id, sizeof(cid) - 1);
        cid[sizeof(cid) - 1] = '\0';
    } else {
        snprintf(cid, sizeof(cid), "box_%d", (int)getpid());
    }

    struct process_spec spec;
    memset(&spec, 0, sizeof(spec));
    spec.command = config->command_argv[0];
    spec.argv = config->command_argv;
    spec.envp = NULL;
    spec.cwd = config->cwd;
    spec.hostname = config->hostname ? config->hostname : MYRUN_DEFAULT_HOSTNAME;
    spec.rootfs = config->rootfs;
    spec.binds = config->binds;
    spec.bind_count = config->bind_count;
    spec.security = config->sec;

    LOG_INFO("Spawning container process '%s' (ID: %s, hostname: '%s') in isolated namespaces...",
             spec.command, cid, spec.hostname);

    /* Set up cgroups v2 limits before launching */
    cgroup_create_and_apply(cid, &config->cgroups);

    pid_t child_pid = 0;
    void *stack_base = NULL;
    int ns_flags = MYRUN_NS_PID | MYRUN_NS_UTS | MYRUN_NS_MOUNT | MYRUN_NS_IPC;
    if (config->user_ns.enabled) {
        ns_flags |= MYRUN_NS_USER;
    }
    if (config->net.enabled) {
        ns_flags |= MYRUN_NS_NET;
    }

    int err = namespace_spawn(&spec, ns_flags, &config->user_ns, &child_pid, &stack_base);
    if (err != MYRUN_SUCCESS) {
        cgroup_destroy(cid);
        if (err == MYRUN_ERR_NOT_FOUND) {
            return 127;
        } else if (err == MYRUN_ERR_PERMISSION) {
            return 126;
        }
        return 1;
    }

    /* Attach process to cgroup v2 */
    cgroup_attach_process(cid, child_pid);

    /* Setup networking if enabled */
    if (config->net.enabled) {
        network_setup_veth(cid, child_pid, &config->net);
    }

    /* Save container state */
    struct container_state cstate;
    memset(&cstate, 0, sizeof(cstate));
    strncpy(cstate.id, cid, sizeof(cstate.id) - 1);
    cstate.init_process_pid = child_pid;
    cstate.status = CONTAINER_STATUS_RUNNING;
    if (config->bundle_path) strncpy(cstate.bundle_path, config->bundle_path, sizeof(cstate.bundle_path) - 1);
    if (config->rootfs) strncpy(cstate.rootfs, config->rootfs, sizeof(cstate.rootfs) - 1);
    container_save_state(&cstate);

    /* Forward signals */
    if (process_init_signals(child_pid) != MYRUN_SUCCESS) {
        LOG_WARN("Could not initialize signal forwarding for PID %d", child_pid);
    }

    /* Wait for container termination */
    struct process_result result;
    memset(&result, 0, sizeof(result));
    err = process_wait(child_pid, &result);

    /* Free stack */
    if (stack_base) {
        namespace_free_stack(stack_base, MYRUN_CHILD_STACK_SIZE);
        stack_base = NULL;
    }

    process_restore_signals();

    /* Clean up cgroups, networking, and state */
    cgroup_destroy(cid);
    if (config->net.enabled) {
        network_cleanup(cid);
    }
    container_delete_state(cid);

    if (err != MYRUN_SUCCESS) {
        LOG_ERROR("Failed waiting for container process %d", child_pid);
        return 1;
    }

    LOG_DEBUG("Container process exited with status %d", result.exit_code);
    return result.exit_code;
}
