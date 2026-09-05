#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <sys/stat.h>
#include "process.h"
#include "runtime.h"
#include "config.h"
#include "utils.h"
#include "log.h"
#include "error.h"

static int g_tests_passed = 0;
static int g_tests_failed = 0;

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

static void test_process_spawn_success(void) {
    char *argv[] = {"echo", "MyRun Test", NULL};
    struct process_spec spec = {
        .command = "echo",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL
    };

    pid_t pid = 0;
    int err = process_spawn(&spec, &pid);
    TEST_ASSERT(err == MYRUN_SUCCESS, "process_spawn should succeed for 'echo'");
    TEST_ASSERT(pid > 0, "PID must be positive");

    struct process_result result;
    err = process_wait(pid, &result);
    TEST_ASSERT(err == MYRUN_SUCCESS, "process_wait should succeed");
    TEST_ASSERT(result.exited_normally == true, "Process should exit normally");
    TEST_ASSERT(result.exit_code == 0, "Exit code should be 0");
    TEST_ASSERT(result.signaled == false, "Process should not be signaled");

    TEST_PASS("test_process_spawn_success");
}

static void test_process_exit_code_propagation(void) {
    char *argv[] = {"sh", "-c", "exit 42", NULL};
    struct process_spec spec = {
        .command = "sh",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL
    };

    pid_t pid = 0;
    int err = process_spawn(&spec, &pid);
    TEST_ASSERT(err == MYRUN_SUCCESS, "process_spawn should succeed for 'sh -c exit 42'");

    struct process_result result;
    err = process_wait(pid, &result);
    TEST_ASSERT(err == MYRUN_SUCCESS, "process_wait should succeed");
    TEST_ASSERT(result.exited_normally == true, "Process should exit normally");
    TEST_ASSERT(result.exit_code == 42, "Exit code should be 42");

    TEST_PASS("test_process_exit_code_propagation");
}

static void test_process_not_found(void) {
    char *argv[] = {"/nonexistent/binary_xyz_12345", NULL};
    struct process_spec spec = {
        .command = "/nonexistent/binary_xyz_12345",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL
    };

    pid_t pid = 0;
    int err = process_spawn(&spec, &pid);
    TEST_ASSERT(err == MYRUN_ERR_NOT_FOUND, "process_spawn should return MYRUN_ERR_NOT_FOUND");

    TEST_PASS("test_process_not_found");
}

static void test_process_arguments_handling(void) {
    char *argv[] = {
        "sh",
        "-c",
        "test \"$1\" = \"first argument with spaces\" && test \"$2\" = \"second_arg\"",
        "sh",
        "first argument with spaces",
        "second_arg",
        NULL
    };
    struct process_spec spec = {
        .command = "sh",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL
    };

    pid_t pid = 0;
    int err = process_spawn(&spec, &pid);
    TEST_ASSERT(err == MYRUN_SUCCESS, "process_spawn should succeed");

    struct process_result result;
    err = process_wait(pid, &result);
    TEST_ASSERT(err == MYRUN_SUCCESS, "process_wait should succeed");
    TEST_ASSERT(result.exit_code == 0, "Arguments check should succeed with exit code 0");

    TEST_PASS("test_process_arguments_handling");
}

static void test_process_working_directory(void) {
    char *argv[] = {
        "sh",
        "-c",
        "test \"$(pwd -P 2>/dev/null || pwd)\" = \"$(cd /tmp && (pwd -P 2>/dev/null || pwd))\"",
        NULL
    };
    struct process_spec spec = {
        .command = "sh",
        .argv = argv,
        .envp = NULL,
        .cwd = "/tmp"
    };

    pid_t pid = 0;
    int err = process_spawn(&spec, &pid);
    TEST_ASSERT(err == MYRUN_SUCCESS, "process_spawn should succeed with cwd=/tmp");

    struct process_result result;
    err = process_wait(pid, &result);
    TEST_ASSERT(err == MYRUN_SUCCESS, "process_wait should succeed");
    TEST_ASSERT(result.exit_code == 0, "Working directory match check should succeed");

    TEST_PASS("test_process_working_directory");
}

static void test_process_custom_environment(void) {
    char *custom_env[] = {
        "MYRUN_ENV_TEST=CONTAINER_ENGINE_SUCCESS",
        "PATH=/usr/bin:/bin",
        NULL
    };
    char *argv[] = {
        "sh",
        "-c",
        "test \"$MYRUN_ENV_TEST\" = \"CONTAINER_ENGINE_SUCCESS\"",
        NULL
    };
    struct process_spec spec = {
        .command = "sh",
        .argv = argv,
        .envp = custom_env,
        .cwd = NULL
    };

    pid_t pid = 0;
    int err = process_spawn(&spec, &pid);
    TEST_ASSERT(err == MYRUN_SUCCESS, "process_spawn should succeed with custom envp");

    struct process_result result;
    err = process_wait(pid, &result);
    TEST_ASSERT(err == MYRUN_SUCCESS, "process_wait should succeed");
    TEST_ASSERT(result.exit_code == 0, "Environment variable check should succeed");

    TEST_PASS("test_process_custom_environment");
}

static void test_process_signal_termination(void) {
    char *argv[] = {"sleep", "5", NULL};
    struct process_spec spec = {
        .command = "sleep",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL
    };

    pid_t pid = 0;
    int err = process_spawn(&spec, &pid);
    TEST_ASSERT(err == MYRUN_SUCCESS, "process_spawn should succeed for sleep");

    /* Send SIGTERM to child */
    usleep(50000); /* 50ms */
    kill(pid, SIGTERM);

    struct process_result result;
    err = process_wait(pid, &result);
    TEST_ASSERT(err == MYRUN_SUCCESS, "process_wait should succeed");
    TEST_ASSERT(result.signaled == true, "Process should be marked as signaled");
    TEST_ASSERT(result.term_signal == SIGTERM, "Terminating signal should be SIGTERM");
    TEST_ASSERT(result.exit_code == 128 + SIGTERM, "Exit code should be 128 + SIGTERM");

    TEST_PASS("test_process_signal_termination");
}

static void test_config_parsing(void) {
    char *args[] = {"myrun", "run", "--cwd", "/tmp", "--debug", "echo", "hello", NULL};
    struct myrun_config config;
    int ret = config_parse_args(7, args, &config);
    TEST_ASSERT(ret == 0, "config_parse_args should succeed");
    TEST_ASSERT(config.subcommand && strcmp(config.subcommand, "run") == 0, "Subcommand must be 'run'");
    TEST_ASSERT(config.debug == true, "Debug flag must be true");
    TEST_ASSERT(config.cwd && strcmp(config.cwd, "/tmp") == 0, "CWD must be '/tmp'");
    TEST_ASSERT(config.command_argc == 2, "Command argc must be 2");
    TEST_ASSERT(strcmp(config.command_argv[0], "echo") == 0, "Command must be 'echo'");
    TEST_ASSERT(strcmp(config.command_argv[1], "hello") == 0, "Arg must be 'hello'");
    config_free(&config);

    TEST_PASS("test_config_parsing");
}

int main(void) {
    log_set_level(LOG_LEVEL_WARN); /* Suppress verbose debug logs during test run */

    printf("=========================================\n");
    printf("     MyRun Phase 1 Test Suite            \n");
    printf("=========================================\n\n");

    test_process_spawn_success();
    test_process_exit_code_propagation();
    test_process_not_found();
    test_process_arguments_handling();
    test_process_working_directory();
    test_process_custom_environment();
    test_process_signal_termination();
    test_config_parsing();

    printf("\n-----------------------------------------\n");
    printf("Tests Passed: %d\n", g_tests_passed);
    printf("Tests Failed: %d\n", g_tests_failed);
    printf("-----------------------------------------\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
