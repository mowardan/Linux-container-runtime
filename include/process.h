#ifndef MYRUN_PROCESS_H
#define MYRUN_PROCESS_H

#include <sys/types.h>
#include <stdbool.h>
#include "error.h"

/**
 * @file process.h
 * @brief Process creation, lifecycle execution, waiting, and signal handling.
 */

/**
 * @brief Specification for a process to be spawned.
 */
struct process_spec {
    const char *command;      /**< Binary path or command name to execute */
    char *const *argv;        /**< Null-terminated argument vector */
    char *const *envp;        /**< Null-terminated environment variable vector */
    const char *cwd;          /**< Working directory (optional, NULL for current) */
    const char *hostname;     /**< Container hostname for UTS namespace (optional) */
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
 * 
 * Uses fork() and execvpe()/execve(). In case of exec failure in the child,
 * the error status is communicated back to the parent.
 *
 * @param spec Process configuration specification.
 * @param out_pid Pointer to store the spawned child PID.
 * @return MYRUN_SUCCESS (0) on success, or a myrun_err_t error code.
 */
int process_spawn(const struct process_spec *spec, pid_t *out_pid);

/**
 * @brief Directly executes the command replacing current process image.
 * 
 * @param spec Process configuration specification.
 * @return Does not return on success; returns myrun_err_t on failure.
 */
int process_exec(const struct process_spec *spec);

/**
 * @brief Waits for the child process to terminate and decodes its status.
 *
 * Handles EINTR interrupts transparently. Decodes wait status macros:
 * WIFEXITED, WEXITSTATUS, WIFSIGNALED, WTERMSIG, WCOREDUMP.
 *
 * @param pid Process ID to wait for.
 * @param result Pointer to store the decoded process result.
 * @return MYRUN_SUCCESS (0) on success, or MYRUN_ERR_WAIT on failure.
 */
int process_wait(pid_t pid, struct process_result *result);

/**
 * @brief Sets up parent signal handlers to forward interactive signals to child.
 *
 * Intercepts SIGINT, SIGTERM, SIGQUIT, and SIGWINCH, forwarding them to child_pid.
 *
 * @param child_pid PID of the child process.
 * @return MYRUN_SUCCESS (0) on success.
 */
int process_init_signals(pid_t child_pid);

/**
 * @brief Restores default signal handlers.
 */
void process_restore_signals(void);

#endif /* MYRUN_PROCESS_H */
