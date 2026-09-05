#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include "process.h"
#include "log.h"

extern char **environ;

static int set_cloexec(int fd) {
    int flags = fcntl(fd, F_GETFD);
    if (flags < 0) {
        return -1;
    }
    return fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

int process_exec(const struct process_spec *spec) {
    if (!spec || !spec->command || !spec->argv) {
        errno = EINVAL;
        return MYRUN_ERR_INVALID_ARG;
    }

    char *const *envp = spec->envp ? spec->envp : environ;

    if (spec->cwd && chdir(spec->cwd) < 0) {
        LOG_ERROR("Failed to change working directory to '%s': %s", spec->cwd, strerror(errno));
        return MYRUN_ERR_EXEC;
    }

    /* If command contains a slash, execute directly with execve */
    if (strchr(spec->command, '/')) {
        execve(spec->command, spec->argv, envp);
        return MYRUN_ERR_EXEC;
    }

    /* Otherwise, resolve command across PATH */
    const char *path_env = getenv("PATH");
    if (!path_env) {
        path_env = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    }

    char path_buf[1024];
    const char *p = path_env;
    while (*p) {
        const char *next = strchr(p, ':');
        size_t len = next ? (size_t)(next - p) : strlen(p);

        if (len > 0 && len + 1 + strlen(spec->command) < sizeof(path_buf)) {
            memcpy(path_buf, p, len);
            path_buf[len] = '/';
            strcpy(path_buf + len + 1, spec->command);

            execve(path_buf, spec->argv, envp);
            /* If error is anything other than ENOENT or ENOTDIR (e.g., EACCES), stop or continue search */
            if (errno == EACCES) {
                /* File exists but is not executable */
                break;
            }
        }

        if (!next) {
            break;
        }
        p = next + 1;
    }

    /* If PATH search exhausted without finding executable */
    if (errno == 0) {
        errno = ENOENT;
    }
    return MYRUN_ERR_EXEC;
}

int process_spawn(const struct process_spec *spec, pid_t *out_pid) {
    if (!spec || !out_pid) {
        return MYRUN_ERR_INVALID_ARG;
    }

    /* Create an error pipe for child -> parent error notification */
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

    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("fork() failed: %s", strerror(errno));
        close(sync_pipe[0]);
        close(sync_pipe[1]);
        return MYRUN_ERR_FORK;
    }

    if (pid == 0) {
        /* === CHILD PROCESS === */
        close(sync_pipe[0]); /* Close read end */

        process_exec(spec);

        /* If exec returns, an error occurred */
        int err = errno;
        ssize_t written = write(sync_pipe[1], &err, sizeof(err));
        (void)written;
        close(sync_pipe[1]);

        /* Standard exit code: 127 if command not found, 126 if permission denied, 1 otherwise */
        int exit_code = (err == ENOENT) ? 127 : ((err == EACCES) ? 126 : 1);
        _exit(exit_code);
    }

    /* === PARENT PROCESS === */
    close(sync_pipe[1]); /* Close write end */

    int child_errno = 0;
    ssize_t n = read(sync_pipe[0], &child_errno, sizeof(child_errno));
    close(sync_pipe[0]);

    if (n > 0) {
        /* Child failed during exec */
        LOG_ERROR("Child process failed to execute '%s': %s (errno %d)",
                  spec->command, strerror(child_errno), child_errno);

        /* Wait for the child to clean up zombie */
        int status;
        waitpid(pid, &status, 0);

        if (child_errno == ENOENT) {
            return MYRUN_ERR_NOT_FOUND;
        } else if (child_errno == EACCES) {
            return MYRUN_ERR_PERMISSION;
        } else {
            return MYRUN_ERR_EXEC;
        }
    }

    /* Child exec succeeded (pipe reached EOF on CLOEXEC) */
    *out_pid = pid;
    LOG_DEBUG("Spawned child process with PID %d", pid);
    return MYRUN_SUCCESS;
}

int process_wait(pid_t pid, struct process_result *result) {
    if (pid <= 0 || !result) {
        return MYRUN_ERR_INVALID_ARG;
    }
    memset(result, 0, sizeof(*result));

    int status = 0;
    pid_t wpid;

    do {
        wpid = waitpid(pid, &status, 0);
    } while (wpid < 0 && errno == EINTR);

    if (wpid < 0) {
        LOG_ERROR("waitpid() failed for PID %d: %s", pid, strerror(errno));
        return MYRUN_ERR_WAIT;
    }

    if (WIFEXITED(status)) {
        result->exited_normally = true;
        result->exit_code = WEXITSTATUS(status);
        LOG_DEBUG("Child process %d exited normally with status %d", pid, result->exit_code);
    } else if (WIFSIGNALED(status)) {
        result->signaled = true;
        result->term_signal = WTERMSIG(status);
        result->exit_code = 128 + result->term_signal;
#ifdef WCOREDUMP
        result->core_dumped = WCOREDUMP(status);
#endif
        LOG_WARN("Child process %d terminated by signal %d (%s)%s",
                 pid, result->term_signal, strsignal(result->term_signal),
                 result->core_dumped ? " (core dumped)" : "");
    } else {
        result->exit_code = 1;
        LOG_WARN("Child process %d terminated with unexpected status 0x%x", pid, status);
    }

    return MYRUN_SUCCESS;
}
