#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "namespace.h"
#include "process.h"
#include "log.h"
#include "error.h"

static int g_tests_passed = 0;
static int g_tests_failed = 0;
static int g_tests_skipped = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        g_tests_failed++; \
        return; \
    } \
} while (0)

#define TEST_PASS(name) do { \
    printf("  [PASS] %s\n", name); \
    g_tests_passed++; \
} while (0)

#define TEST_SKIP(name, reason) do { \
    printf("  [SKIPPED] %s (%s)\n", name, reason); \
    g_tests_skipped++; \
} while (0)

static void test_pid_namespace_is_pid_1(void) {
#ifndef __linux__
    TEST_SKIP("test_pid_namespace_is_pid_1", "Requires Linux CLONE_NEWPID");
    return;
#else
    char *argv[] = {"sh", "-c", "test \"$$\" = \"1\"", NULL};
    struct process_spec spec = {
        .command = "sh",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_SUCCESS, "namespace_spawn with MYRUN_NS_PID should succeed");
    TEST_ASSERT(host_pid > 0, "Host PID should be positive");

    struct process_result result;
    err = process_wait(host_pid, &result);
    if (stack) {
        namespace_free_stack(stack, MYRUN_CHILD_STACK_SIZE);
    }

    TEST_ASSERT(err == MYRUN_SUCCESS, "process_wait should succeed");
    TEST_ASSERT(result.exited_normally == true, "Child should exit normally");
    TEST_ASSERT(result.exit_code == 0, "Inside container, $$ must equal 1 (exit code 0)");

    TEST_PASS("test_pid_namespace_is_pid_1");
#endif
}

static void test_pid_namespace_nspid_inspection(void) {
#ifndef __linux__
    TEST_SKIP("test_pid_namespace_nspid_inspection", "Requires Linux /proc/<pid>/status NSpid");
    return;
#else
    char *argv[] = {"sleep", "1", NULL};
    struct process_spec spec = {
        .command = "sleep",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_SUCCESS, "namespace_spawn should succeed");
    TEST_ASSERT(host_pid > 0, "Host PID should be positive");

    /* Inspect /proc/<host_pid>/status for NSpid */
    char status_path[128];
    snprintf(status_path, sizeof(status_path), "/proc/%d/status", host_pid);

    FILE *f = fopen(status_path, "r");
    bool found_nspid = false;
    int outer_pid = 0;
    int inner_pid = 0;

    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "NSpid:", 6) == 0) {
                /* Format: NSpid:\t<outer_pid>\t<inner_pid> */
                int matched = sscanf(line + 6, "%d %d", &outer_pid, &inner_pid);
                if (matched == 2) {
                    found_nspid = true;
                }
                break;
            }
        }
        fclose(f);
    }

    struct process_result result;
    process_wait(host_pid, &result);
    if (stack) {
        namespace_free_stack(stack, MYRUN_CHILD_STACK_SIZE);
    }

    TEST_ASSERT(found_nspid == true, "Kernel must provide NSpid entry in /proc/<pid>/status");
    TEST_ASSERT(outer_pid == host_pid, "Outer NSpid must equal host PID");
    TEST_ASSERT(inner_pid == 1, "Inner NSpid must equal 1 (container PID 1)");
    TEST_ASSERT(outer_pid != inner_pid || host_pid == 1, "Host PID must differ from Container PID");

    TEST_PASS("test_pid_namespace_nspid_inspection");
#endif
}

static void test_pid_namespace_exit_code_propagation(void) {
    char *argv[] = {"sh", "-c", "exit 42", NULL};
    struct process_spec spec = {
        .command = "sh",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_SUCCESS, "namespace_spawn should succeed");

    struct process_result result;
    err = process_wait(host_pid, &result);
    if (stack) {
        namespace_free_stack(stack, MYRUN_CHILD_STACK_SIZE);
    }

    TEST_ASSERT(err == MYRUN_SUCCESS, "process_wait should succeed");
    TEST_ASSERT(result.exit_code == 42, "Exit code 42 must be correctly propagated");

    TEST_PASS("test_pid_namespace_exit_code_propagation");
}

static void test_pid_namespace_missing_executable(void) {
    char *argv[] = {"/nonexistent/binary_xyz_98765", NULL};
    struct process_spec spec = {
        .command = "/nonexistent/binary_xyz_98765",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_ERR_NOT_FOUND, "Missing executable should return MYRUN_ERR_NOT_FOUND");
    TEST_ASSERT(stack == NULL, "Stack should be cleaned up on failure");

    TEST_PASS("test_pid_namespace_missing_executable");
}

static void test_pid_namespace_normal_execution(void) {
    char *argv[] = {"echo", "Hello from PID namespace", NULL};
    struct process_spec spec = {
        .command = "echo",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_SUCCESS, "namespace_spawn for echo should succeed");

    struct process_result result;
    err = process_wait(host_pid, &result);
    if (stack) {
        namespace_free_stack(stack, MYRUN_CHILD_STACK_SIZE);
    }

    TEST_ASSERT(err == MYRUN_SUCCESS, "process_wait should succeed");
    TEST_ASSERT(result.exit_code == 0, "echo should exit with 0");

    TEST_PASS("test_pid_namespace_normal_execution");
}

static void test_pid_namespace_zombie_cleanup(void) {
    char *argv[] = {"sh", "-c", "exit 0", NULL};
    struct process_spec spec = {
        .command = "sh",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_SUCCESS, "namespace_spawn should succeed");

    struct process_result result;
    process_wait(host_pid, &result);
    if (stack) {
        namespace_free_stack(stack, MYRUN_CHILD_STACK_SIZE);
    }

    /* Verify child process no longer exists (no zombie left in OS table) */
    int kill_res = kill(host_pid, 0);
    int kill_err = errno;
    TEST_ASSERT(kill_res == -1 && kill_err == ESRCH, "Reaped child PID must not exist in process table");

    TEST_PASS("test_pid_namespace_zombie_cleanup");
}

int main(void) {
    log_set_level(LOG_LEVEL_WARN);

    printf("=========================================\n");
    printf("     MyRun Phase 2 PID Namespace Tests   \n");
    printf("=========================================\n\n");

    test_pid_namespace_is_pid_1();
    test_pid_namespace_nspid_inspection();
    test_pid_namespace_exit_code_propagation();
    test_pid_namespace_missing_executable();
    test_pid_namespace_normal_execution();
    test_pid_namespace_zombie_cleanup();

    printf("\n-----------------------------------------\n");
    printf("Tests Passed:  %d\n", g_tests_passed);
    printf("Tests Skipped: %d\n", g_tests_skipped);
    printf("Tests Failed:  %d\n", g_tests_failed);
    printf("-----------------------------------------\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
