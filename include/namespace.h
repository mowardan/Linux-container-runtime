#ifndef MYRUN_NAMESPACE_H
#define MYRUN_NAMESPACE_H

#include <sys/types.h>
#include <stddef.h>
#include "process.h"
#include "error.h"
#include "uts.h"

/**
 * @file namespace.h
 * @brief Linux namespace isolation and child container instantiation APIs.
 */

#define MYRUN_CHILD_STACK_SIZE (2 * 1024 * 1024) /* 2 MB */

/**
 * @brief Namespace isolation flags.
 */
typedef enum {
    MYRUN_NS_NONE  = 0,
    MYRUN_NS_PID   = (1 << 0), /**< Isolate PID namespace (CLONE_NEWPID) */
    MYRUN_NS_UTS   = (1 << 1), /**< Isolate hostname / UTS (CLONE_NEWUTS) */
    MYRUN_NS_MOUNT = (1 << 2), /**< Isolate mount namespace (CLONE_NEWNS) */
    MYRUN_NS_USER  = (1 << 3), /**< Isolate user namespace (CLONE_NEWUSER) */
    MYRUN_NS_NET   = (1 << 4), /**< Isolate network namespace (CLONE_NEWNET) */
    MYRUN_NS_IPC   = (1 << 5)  /**< Isolate IPC namespace (CLONE_NEWIPC) */
} myrun_ns_flags_t;

/**
 * @brief Spawns a container child process inside the specified Linux namespaces.
 *
 * Allocates a dedicated child stack and uses clone() with the specified namespace
 * flags (such as CLONE_NEWPID, CLONE_NEWUTS). An error synchronization pipe is used
 * to capture any child-side startup or exec failures.
 *
 * @param spec Target process specification (command, argv, envp, cwd, hostname).
 * @param ns_flags Bitmask of myrun_ns_flags_t namespaces to create.
 * @param out_pid Pointer to store the host PID of the spawned child.
 * @param out_stack_base Pointer to store the base address of the allocated stack.
 *                       Must be freed with namespace_free_stack() AFTER waitpid().
 * @return MYRUN_SUCCESS (0) on success, or an appropriate error code.
 */
int namespace_spawn(const struct process_spec *spec, int ns_flags, pid_t *out_pid, void **out_stack_base);

/**
 * @brief Frees the stack allocated for the container child process.
 * 
 * @param stack_base Base address of the stack allocated by namespace_spawn().
 * @param size Size in bytes of the stack (typically MYRUN_CHILD_STACK_SIZE).
 */
void namespace_free_stack(void *stack_base, size_t size);

#endif /* MYRUN_NAMESPACE_H */
