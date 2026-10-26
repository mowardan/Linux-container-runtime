#ifndef MYRUN_PROCESS_H
#define MYRUN_PROCESS_H

#include <sys/types.h>
#include <stdbool.h>
#include "error.h"
#include "filesystem.h"
#include "security.h"

/**
 * @file process.h
 * @brief Process creation, lifecycle execution, waiting, and signal handling.
 */

/**
 * @brief Specification for a process to be spawned.
 */
struct process_spec {
    const char *command;               /**< Binary path or command name to execute */
    char *const *argv;                 /**< Null-terminated argument vector */
    char *const *envp;                 /**< Null-terminated environment variable vector */
    const char *cwd;                   /**< Working directory (optional, NULL for current) */
    const char *hostname;              /**< Container hostname for UTS namespace (optional) */
    const char *rootfs;                /**< Container root filesystem path (optional) */
    const struct bind_mount *binds;    /**< Bind mounts list */
    size_t bind_count;                 /**< Number of bind mounts */
    struct security_config security;   /**< Security parameters (capabilities, no_new_privs) */
};

/**
 * @brief Execution result gathered upon child process termination.
 */
struct process_result {
    int exit_code;            /**< Exit code if process exited normally (0-255) */
    int term_signal;          /**< Signal number if process was terminated by signal */
    bool exited_normally;     /**< True if process exited via exit() or return from main */
    bool signaled;            /**< True if process was terminated by an unhandled signal */
    bool core_dumped;         /**< True if process produced a core dump */
};

/**
 * @brief Forks and executes a command described by spec in the child.
 */
int process_spawn(const struct process_spec *spec, pid_t *out_pid);

/**
 * @brief Directly executes the command replacing current process image.
 */
int process_exec(const struct process_spec *spec);

/**
 * @brief Waits for the child process to terminate and decodes its status.
 */
int process_wait(pid_t pid, struct process_result *result);

/**
 * @brief Sets up parent signal handlers to forward interactive signals to child.
 */
int process_init_signals(pid_t child_pid);

/**
 * @brief Restores default signal handlers.
 */
void process_restore_signals(void);

#endif /* MYRUN_PROCESS_H */
