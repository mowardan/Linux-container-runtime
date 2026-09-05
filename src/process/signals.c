#include <signal.h>
#include <unistd.h>
#include <string.h>
#include "process.h"
#include "log.h"

static volatile sig_atomic_t g_active_child_pid = 0;
static struct sigaction g_old_sigint;
static struct sigaction g_old_sigterm;
static struct sigaction g_old_sigquit;
#ifdef SIGWINCH
static struct sigaction g_old_sigwinch;
#endif

static void forward_signal_handler(int signo) {
    pid_t pid = (pid_t)g_active_child_pid;
    if (pid > 0) {
        /* kill() is async-signal-safe */
        kill(pid, signo);
    }
}

int process_init_signals(pid_t child_pid) {
    g_active_child_pid = (sig_atomic_t)child_pid;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = forward_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, &g_old_sigint) < 0) {
        LOG_ERROR("Failed to register SIGINT handler: %s", strerror(errno));
        return MYRUN_ERR_SIGNAL;
    }
    if (sigaction(SIGTERM, &sa, &g_old_sigterm) < 0) {
        LOG_ERROR("Failed to register SIGTERM handler: %s", strerror(errno));
        return MYRUN_ERR_SIGNAL;
    }
    if (sigaction(SIGQUIT, &sa, &g_old_sigquit) < 0) {
        LOG_ERROR("Failed to register SIGQUIT handler: %s", strerror(errno));
        return MYRUN_ERR_SIGNAL;
    }
#ifdef SIGWINCH
    if (sigaction(SIGWINCH, &sa, &g_old_sigwinch) < 0) {
        LOG_ERROR("Failed to register SIGWINCH handler: %s", strerror(errno));
        return MYRUN_ERR_SIGNAL;
    }
#endif

    return MYRUN_SUCCESS;
}

void process_restore_signals(void) {
    g_active_child_pid = 0;
    sigaction(SIGINT, &g_old_sigint, NULL);
    sigaction(SIGTERM, &g_old_sigterm, NULL);
    sigaction(SIGQUIT, &g_old_sigquit, NULL);
#ifdef SIGWINCH
    sigaction(SIGWINCH, &g_old_sigwinch, NULL);
#endif
}
