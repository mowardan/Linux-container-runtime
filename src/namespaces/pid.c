#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>

#include "namespace.h"
#include "process.h"
#include "log.h"
#include "error.h"

#ifdef __linux__
#include <sched.h>
#endif

struct child_trampoline_args {
    const struct process_spec *spec;
    int sync_pipe_write_fd;
    int ns_flags;
};

static int set_cloexec(int fd) {
    int flags = fcntl(fd, F_GETFD);
    if (flags < 0) {
        return -1;
    }
    return fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

#ifdef __linux__
static int child_trampoline(void *arg) {
    struct child_trampoline_args *args = (struct child_trampoline_args *)arg;
    if (!args || !args->spec) {
        _exit(1);
    }

    /* Configure Mount namespace isolation if requested */
    if (args->ns_flags & MYRUN_NS_MOUNT) {
        int mount_err = mount_namespace_setup();
        if (mount_err != MYRUN_SUCCESS) {
            int err = errno ? errno : EINVAL;
            ssize_t written = write(args->sync_pipe_write_fd, &err, sizeof(err));
            (void)written;
            close(args->sync_pipe_write_fd);
            _exit(1);
        }
    }

    /* Configure UTS namespace hostname if requested */
    if ((args->ns_flags & MYRUN_NS_UTS) && args->spec->hostname) {
        int uts_err = uts_namespace_setup(args->spec->hostname);
        if (uts_err != MYRUN_SUCCESS) {
            int err = errno ? errno : EINVAL;
            ssize_t written = write(args->sync_pipe_write_fd, &err, sizeof(err));
            (void)written;
            close(args->sync_pipe_write_fd);
            _exit(1);
        }
    }

    /* Execute the container command */
    process_exec(args->spec);

    /* If process_exec returns, execve failed */
    int err = errno;
    ssize_t written = write(args->sync_pipe_write_fd, &err, sizeof(err));
    (void)written;
    close(args->sync_pipe_write_fd);

    int exit_code = (err == ENOENT) ? 127 : ((err == EACCES) ? 126 : 1);
    _exit(exit_code);
}
#endif

int namespace_spawn(const struct process_spec *spec, int ns_flags, pid_t *out_pid, void **out_stack_base) {
    if (!spec || !out_pid || !out_stack_base) {
        return MYRUN_ERR_INVALID_ARG;
    }

    *out_pid = 0;
    *out_stack_base = NULL;

    /* Create synchronization pipe for child error propagation */
    int sync_pipe[2];
    if (pipe(sync_pipe) < 0) {
        LOG_ERROR("Failed to create synchronization pipe: %s", strerror(errno));
        return MYRUN_ERR_FORK;
    }

    if (set_cloexec(sync_pipe[0]) < 0 || set_cloexec(sync_pipe[1]) < 0) {
        LOG_ERROR("Failed to set FD_CLOEXEC on sync pipe: %s", strerror(errno));
        close(sync_pipe[0]);
        close(sync_pipe[1]);
        return MYRUN_ERR_FORK;
    }

#ifdef __linux__
    /* Allocate child stack with MAP_STACK on Linux */
    void *stack_base = mmap(NULL,
                            MYRUN_CHILD_STACK_SIZE,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                            -1,
                            0);
    if (stack_base == MAP_FAILED) {
        LOG_ERROR("Failed to allocate child stack of size %d: %s",
                  MYRUN_CHILD_STACK_SIZE, strerror(errno));
        close(sync_pipe[0]);
        close(sync_pipe[1]);
        return MYRUN_ERR_NOMEM;
    }

    /* Stack grows downward on x86_64, aarch64 */
    void *stack_top = (char *)stack_base + MYRUN_CHILD_STACK_SIZE;

    struct child_trampoline_args args = {
        .spec = spec,
        .sync_pipe_write_fd = sync_pipe[1],
        .ns_flags = ns_flags
    };

    int clone_flags = SIGCHLD;
    if (ns_flags & MYRUN_NS_PID) {
        clone_flags |= CLONE_NEWPID;
    }
    if (ns_flags & MYRUN_NS_UTS) {
        clone_flags |= CLONE_NEWUTS;
    }
    if (ns_flags & MYRUN_NS_MOUNT) {
        clone_flags |= CLONE_NEWNS;
    }

    LOG_DEBUG("Calling clone() with flags 0x%x (CLONE_NEWPID=%s, CLONE_NEWUTS=%s, CLONE_NEWNS=%s)...",
              clone_flags,
              (ns_flags & MYRUN_NS_PID) ? "yes" : "no",
              (ns_flags & MYRUN_NS_UTS) ? "yes" : "no",
              (ns_flags & MYRUN_NS_MOUNT) ? "yes" : "no");

    pid_t pid = clone(child_trampoline, stack_top, clone_flags, &args);
    if (pid < 0) {
        int clone_err = errno;
        LOG_ERROR("clone() failed: %s (errno %d)", strerror(clone_err), clone_err);
        if (clone_err == EPERM) {
            LOG_ERROR("Root privileges (CAP_SYS_ADMIN) are required for namespace creation.");
        }
        close(sync_pipe[0]);
        close(sync_pipe[1]);
        munmap(stack_base, MYRUN_CHILD_STACK_SIZE);
        return MYRUN_ERR_CLONE;
    }

    /* === PARENT PROCESS === */
    close(sync_pipe[1]); /* Close write end */

    int child_errno = 0;
    ssize_t n = read(sync_pipe[0], &child_errno, sizeof(child_errno));
    close(sync_pipe[0]);

    if (n > 0) {
        /* Child reported execve or setup failure */
        LOG_ERROR("Child process failed during setup/exec of '%s': %s (errno %d)",
                  spec->command, strerror(child_errno), child_errno);

        /* Wait for child to exit so it doesn't become a zombie */
        int status;
        waitpid(pid, &status, 0);

        munmap(stack_base, MYRUN_CHILD_STACK_SIZE);

        if (child_errno == ENOENT) {
            return MYRUN_ERR_NOT_FOUND;
        } else if (child_errno == EACCES) {
            return MYRUN_ERR_PERMISSION;
        }
        return MYRUN_ERR_EXEC;
    }

    *out_pid = pid;
    *out_stack_base = stack_base;
    LOG_DEBUG("Spawned container child with host PID %d inside isolated namespaces", pid);
    return MYRUN_SUCCESS;

#else
    /* Non-Linux fallback for development host compilation */
    (void)ns_flags;
    LOG_WARN("Linux namespaces are not supported on this host platform; falling back to fork()");

    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("fork() failed: %s", strerror(errno));
        close(sync_pipe[0]);
        close(sync_pipe[1]);
        return MYRUN_ERR_FORK;
    }

    if (pid == 0) {
        close(sync_pipe[0]);
        process_exec(spec);
        int err = errno;
        ssize_t written = write(sync_pipe[1], &err, sizeof(err));
        (void)written;
        close(sync_pipe[1]);
        _exit((err == ENOENT) ? 127 : ((err == EACCES) ? 126 : 1));
    }

    close(sync_pipe[1]);
    int child_errno = 0;
    ssize_t n = read(sync_pipe[0], &child_errno, sizeof(child_errno));
    close(sync_pipe[0]);

    if (n > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (child_errno == ENOENT) {
            return MYRUN_ERR_NOT_FOUND;
        } else if (child_errno == EACCES) {
            return MYRUN_ERR_PERMISSION;
        }
        return MYRUN_ERR_EXEC;
    }

    *out_pid = pid;
    *out_stack_base = NULL;
    return MYRUN_SUCCESS;
#endif
}

void namespace_free_stack(void *stack_base, size_t size) {
    if (stack_base && size > 0) {
#ifdef __linux__
        munmap(stack_base, size);
#else
        (void)stack_base;
        (void)size;
#endif
    }
}
