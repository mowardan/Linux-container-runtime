#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/utsname.h>

#include "namespace.h"
#include "process.h"
#include "config.h"
#include "uts.h"
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

static void test_uts_namespace_hostname_isolation(void) {
#ifndef __linux__
    TEST_SKIP("test_uts_namespace_hostname_isolation", "Requires Linux CLONE_NEWUTS");
    return;
#else
    char *argv[] = {"sh", "-c", "test \"$(hostname 2>/dev/null || uname -n)\" = \"web\"", NULL};
    struct process_spec spec = {
        .command = "sh",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL,
        .hostname = "web"
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID | MYRUN_NS_UTS, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_SUCCESS, "namespace_spawn with MYRUN_NS_UTS should succeed");
    TEST_ASSERT(host_pid > 0, "Host PID should be positive");

    struct process_result result;
    err = process_wait(host_pid, &result);
    if (stack) {
        namespace_free_stack(stack, MYRUN_CHILD_STACK_SIZE);
    }

    TEST_ASSERT(err == MYRUN_SUCCESS, "process_wait should succeed");
    TEST_ASSERT(result.exited_normally == true, "Child should exit normally");
    TEST_ASSERT(result.exit_code == 0, "Inside container, hostname must equal 'web'");

    TEST_PASS("test_uts_namespace_hostname_isolation");
#endif
}

static void test_uts_namespace_host_non_interference(void) {
#ifndef __linux__
    TEST_SKIP("test_uts_namespace_host_non_interference", "Requires Linux CLONE_NEWUTS");
    return;
#else
    char host_before[256];
    memset(host_before, 0, sizeof(host_before));
    if (gethostname(host_before, sizeof(host_before) - 1) < 0) {
        strncpy(host_before, "unknown", sizeof(host_before));
    }

    char *argv[] = {"echo", "testing non-interference", NULL};
    struct process_spec spec = {
        .command = "echo",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL,
        .hostname = "isolated-test-hostname-12345"
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID | MYRUN_NS_UTS, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_SUCCESS, "namespace_spawn should succeed");

    struct process_result result;
    process_wait(host_pid, &result);
    if (stack) {
        namespace_free_stack(stack, MYRUN_CHILD_STACK_SIZE);
    }

    char host_after[256];
    memset(host_after, 0, sizeof(host_after));
    if (gethostname(host_after, sizeof(host_after) - 1) < 0) {
        strncpy(host_after, "unknown", sizeof(host_after));
    }

    TEST_ASSERT(strcmp(host_before, host_after) == 0,
                "Host system hostname must remain unmodified after container execution");

    TEST_PASS("test_uts_namespace_host_non_interference");
#endif
}

static void test_uts_namespace_uname_nodename(void) {
#ifndef __linux__
    TEST_SKIP("test_uts_namespace_uname_nodename", "Requires Linux CLONE_NEWUTS");
    return;
#else
    char *argv[] = {"sh", "-c", "test \"$(uname -n)\" = \"box-alpha\"", NULL};
    struct process_spec spec = {
        .command = "sh",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL,
        .hostname = "box-alpha"
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID | MYRUN_NS_UTS, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_SUCCESS, "namespace_spawn should succeed");

    struct process_result result;
    err = process_wait(host_pid, &result);
    if (stack) {
        namespace_free_stack(stack, MYRUN_CHILD_STACK_SIZE);
    }

    TEST_ASSERT(err == MYRUN_SUCCESS, "process_wait should succeed");
    TEST_ASSERT(result.exit_code == 0, "uname -n must match container hostname 'box-alpha'");

    TEST_PASS("test_uts_namespace_uname_nodename");
#endif
}

static void test_uts_namespace_default_hostname(void) {
#ifndef __linux__
    TEST_SKIP("test_uts_namespace_default_hostname", "Requires Linux CLONE_NEWUTS");
    return;
#else
    char *argv[] = {"sh", "-c", "test \"$(hostname 2>/dev/null || uname -n)\" = \"myrun\"", NULL};
    struct process_spec spec = {
        .command = "sh",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL,
        .hostname = MYRUN_DEFAULT_HOSTNAME
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID | MYRUN_NS_UTS, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_SUCCESS, "namespace_spawn should succeed");

    struct process_result result;
    err = process_wait(host_pid, &result);
    if (stack) {
        namespace_free_stack(stack, MYRUN_CHILD_STACK_SIZE);
    }

    TEST_ASSERT(err == MYRUN_SUCCESS, "process_wait should succeed");
    TEST_ASSERT(result.exit_code == 0, "Default container hostname must be 'myrun'");

    TEST_PASS("test_uts_namespace_default_hostname");
#endif
}

static void test_uts_namespace_pid_and_uts_coexistence(void) {
#ifndef __linux__
    TEST_SKIP("test_uts_namespace_pid_and_uts_coexistence", "Requires Linux CLONE_NEWUTS");
    return;
#else
    char *argv[] = {
        "sh",
        "-c",
        "test \"$$\" = \"1\" && test \"$(hostname 2>/dev/null || uname -n)\" = \"integration-demo\"",
        NULL
    };
    struct process_spec spec = {
        .command = "sh",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL,
        .hostname = "integration-demo"
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID | MYRUN_NS_UTS, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_SUCCESS, "namespace_spawn should succeed");

    struct process_result result;
    err = process_wait(host_pid, &result);
    if (stack) {
        namespace_free_stack(stack, MYRUN_CHILD_STACK_SIZE);
    }

    TEST_ASSERT(err == MYRUN_SUCCESS, "process_wait should succeed");
    TEST_ASSERT(result.exit_code == 0, "Container must simultaneously be PID 1 and have hostname 'integration-demo'");

    TEST_PASS("test_uts_namespace_pid_and_uts_coexistence");
#endif
}

static void test_uts_namespace_multiple_containers(void) {
#ifndef __linux__
    TEST_SKIP("test_uts_namespace_multiple_containers", "Requires Linux CLONE_NEWUTS");
    return;
#else
    char host_before[256];
    memset(host_before, 0, sizeof(host_before));
    if (gethostname(host_before, sizeof(host_before) - 1) < 0) {
        strncpy(host_before, "unknown", sizeof(host_before));
    }

    /* Run container A */
    char *argv_a[] = {"sh", "-c", "test \"$(uname -n)\" = \"container-a\"", NULL};
    struct process_spec spec_a = {
        .command = "sh",
        .argv = argv_a,
        .envp = NULL,
        .cwd = NULL,
        .hostname = "container-a"
    };
    pid_t pid_a = 0;
    void *stack_a = NULL;
    int err = namespace_spawn(&spec_a, MYRUN_NS_PID | MYRUN_NS_UTS, &pid_a, &stack_a);
    TEST_ASSERT(err == MYRUN_SUCCESS, "spawn container A should succeed");
    struct process_result res_a;
    process_wait(pid_a, &res_a);
    if (stack_a) {
        namespace_free_stack(stack_a, MYRUN_CHILD_STACK_SIZE);
    }
    TEST_ASSERT(res_a.exit_code == 0, "Container A hostname should be container-a");

    /* Run container B */
    char *argv_b[] = {"sh", "-c", "test \"$(uname -n)\" = \"container-b\"", NULL};
    struct process_spec spec_b = {
        .command = "sh",
        .argv = argv_b,
        .envp = NULL,
        .cwd = NULL,
        .hostname = "container-b"
    };
    pid_t pid_b = 0;
    void *stack_b = NULL;
    err = namespace_spawn(&spec_b, MYRUN_NS_PID | MYRUN_NS_UTS, &pid_b, &stack_b);
    TEST_ASSERT(err == MYRUN_SUCCESS, "spawn container B should succeed");
    struct process_result res_b;
    process_wait(pid_b, &res_b);
    if (stack_b) {
        namespace_free_stack(stack_b, MYRUN_CHILD_STACK_SIZE);
    }
    TEST_ASSERT(res_b.exit_code == 0, "Container B hostname should be container-b");

    char host_after[256];
    memset(host_after, 0, sizeof(host_after));
    if (gethostname(host_after, sizeof(host_after) - 1) < 0) {
        strncpy(host_after, "unknown", sizeof(host_after));
    }
    TEST_ASSERT(strcmp(host_before, host_after) == 0, "Host hostname must remain unchanged");

    TEST_PASS("test_uts_namespace_multiple_containers");
#endif
}

static void test_uts_namespace_concurrent_isolation(void) {
#ifndef __linux__
    TEST_SKIP("test_uts_namespace_concurrent_isolation", "Requires Linux CLONE_NEWUTS");
    return;
#else
    char host_before[256];
    memset(host_before, 0, sizeof(host_before));
    if (gethostname(host_before, sizeof(host_before) - 1) < 0) {
        strncpy(host_before, "unknown", sizeof(host_before));
    }

    /* Container Alpha runs a small script checking its nodename after a small sleep */
    char *argv_alpha[] = {"sh", "-c", "sleep 0.1 && test \"$(uname -n)\" = \"alpha\"", NULL};
    struct process_spec spec_alpha = {
        .command = "sh",
        .argv = argv_alpha,
        .envp = NULL,
        .cwd = NULL,
        .hostname = "alpha"
    };

    /* Container Beta runs a small script checking its nodename after a small sleep */
    char *argv_beta[] = {"sh", "-c", "sleep 0.1 && test \"$(uname -n)\" = \"beta\"", NULL};
    struct process_spec spec_beta = {
        .command = "sh",
        .argv = argv_beta,
        .envp = NULL,
        .cwd = NULL,
        .hostname = "beta"
    };

    pid_t pid_alpha = 0;
    void *stack_alpha = NULL;
    int err_alpha = namespace_spawn(&spec_alpha, MYRUN_NS_PID | MYRUN_NS_UTS, &pid_alpha, &stack_alpha);
    TEST_ASSERT(err_alpha == MYRUN_SUCCESS, "Spawning concurrent container Alpha should succeed");

    pid_t pid_beta = 0;
    void *stack_beta = NULL;
    int err_beta = namespace_spawn(&spec_beta, MYRUN_NS_PID | MYRUN_NS_UTS, &pid_beta, &stack_beta);
    TEST_ASSERT(err_beta == MYRUN_SUCCESS, "Spawning concurrent container Beta should succeed");

    /* Host checks its hostname while both containers are actively running */
    char host_mid[256];
    memset(host_mid, 0, sizeof(host_mid));
    if (gethostname(host_mid, sizeof(host_mid) - 1) < 0) {
        strncpy(host_mid, "unknown", sizeof(host_mid));
    }
    TEST_ASSERT(strcmp(host_before, host_mid) == 0, "Host hostname must be unaffected while containers run");

    /* Wait for both containers */
    struct process_result res_alpha, res_beta;
    process_wait(pid_alpha, &res_alpha);
    process_wait(pid_beta, &res_beta);

    if (stack_alpha) {
        namespace_free_stack(stack_alpha, MYRUN_CHILD_STACK_SIZE);
    }
    if (stack_beta) {
        namespace_free_stack(stack_beta, MYRUN_CHILD_STACK_SIZE);
    }

    TEST_ASSERT(res_alpha.exit_code == 0, "Container Alpha must observe hostname 'alpha'");
    TEST_ASSERT(res_beta.exit_code == 0, "Container Beta must observe hostname 'beta'");

    TEST_PASS("test_uts_namespace_concurrent_isolation");
#endif
}

static void test_uts_namespace_ns_inode_inspection(void) {
#ifndef __linux__
    TEST_SKIP("test_uts_namespace_ns_inode_inspection", "Requires Linux /proc/self/ns/uts");
    return;
#else
    char *argv[] = {"sleep", "0.2", NULL};
    struct process_spec spec = {
        .command = "sleep",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL,
        .hostname = "inode-check"
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID | MYRUN_NS_UTS, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_SUCCESS, "namespace_spawn should succeed");
    TEST_ASSERT(host_pid > 0, "Host PID should be positive");

    char host_ns[256] = {0};
    char child_ns[256] = {0};

    ssize_t host_len = readlink("/proc/self/ns/uts", host_ns, sizeof(host_ns) - 1);
    char child_path[128];
    snprintf(child_path, sizeof(child_path), "/proc/%d/ns/uts", host_pid);
    ssize_t child_len = readlink(child_path, child_ns, sizeof(child_ns) - 1);

    struct process_result result;
    process_wait(host_pid, &result);
    if (stack) {
        namespace_free_stack(stack, MYRUN_CHILD_STACK_SIZE);
    }

    if (host_len > 0 && child_len > 0) {
        host_ns[host_len] = '\0';
        child_ns[child_len] = '\0';
        TEST_ASSERT(strcmp(host_ns, child_ns) != 0,
                    "Host and container must point to distinct UTS namespace inodes in /proc");
    }

    TEST_PASS("test_uts_namespace_ns_inode_inspection");
#endif
}

static void test_uts_namespace_validation(void) {
    /* Null or empty */
    TEST_ASSERT(hostname_validate(NULL) == MYRUN_ERR_INVALID_ARG, "NULL hostname must be rejected");
    TEST_ASSERT(hostname_validate("") == MYRUN_ERR_INVALID_ARG, "Empty hostname must be rejected");

    /* Length check */
    char long_name[128];
    memset(long_name, 'a', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    TEST_ASSERT(hostname_validate(long_name) == MYRUN_ERR_INVALID_ARG, "Overly long hostname must be rejected");

    /* Exactly 64 chars -> valid */
    char max_valid[65];
    memset(max_valid, 'x', 64);
    max_valid[64] = '\0';
    TEST_ASSERT(hostname_validate(max_valid) == MYRUN_SUCCESS, "64-char valid hostname must be accepted");

    /* Slashes */
    TEST_ASSERT(hostname_validate("host/name") == MYRUN_ERR_INVALID_ARG, "Hostname with '/' must be rejected");

    /* Spaces and tabs */
    TEST_ASSERT(hostname_validate("host name") == MYRUN_ERR_INVALID_ARG, "Hostname with space must be rejected");
    TEST_ASSERT(hostname_validate("host\tname") == MYRUN_ERR_INVALID_ARG, "Hostname with tab must be rejected");

    /* Control characters */
    TEST_ASSERT(hostname_validate("host\x01name") == MYRUN_ERR_INVALID_ARG, "Hostname with control char must be rejected");

    /* Disallowed metacharacters */
    TEST_ASSERT(hostname_validate("host;rm") == MYRUN_ERR_INVALID_ARG, "Hostname with ';' must be rejected");
    TEST_ASSERT(hostname_validate("host$var") == MYRUN_ERR_INVALID_ARG, "Hostname with '$' must be rejected");
    TEST_ASSERT(hostname_validate("host>out") == MYRUN_ERR_INVALID_ARG, "Hostname with '>' must be rejected");

    /* Valid hostnames */
    TEST_ASSERT(hostname_validate("myrun") == MYRUN_SUCCESS, "'myrun' must be valid");
    TEST_ASSERT(hostname_validate("web-node-01") == MYRUN_SUCCESS, "'web-node-01' must be valid");
    TEST_ASSERT(hostname_validate("db_service.internal") == MYRUN_SUCCESS, "'db_service.internal' must be valid");

    TEST_PASS("test_uts_namespace_validation");
}

static void test_config_hostname_parsing(void) {
    /* Valid --hostname */
    char *args1[] = {"myrun", "run", "--hostname", "web-prod-01", "echo", "hi", NULL};
    struct myrun_config config1;
    int ret1 = config_parse_args(6, args1, &config1);
    TEST_ASSERT(ret1 == 0, "config_parse_args with --hostname should succeed");
    TEST_ASSERT(config1.hostname && strcmp(config1.hostname, "web-prod-01") == 0, "Hostname must be 'web-prod-01'");
    config_free(&config1);

    /* Valid -H */
    char *args2[] = {"myrun", "run", "-H", "db-node", "echo", "hi", NULL};
    struct myrun_config config2;
    int ret2 = config_parse_args(6, args2, &config2);
    TEST_ASSERT(ret2 == 0, "config_parse_args with -H should succeed");
    TEST_ASSERT(config2.hostname && strcmp(config2.hostname, "db-node") == 0, "Hostname must be 'db-node'");
    config_free(&config2);

    /* Invalid: empty hostname */
    char *args3[] = {"myrun", "run", "--hostname", "", "echo", "hi", NULL};
    struct myrun_config config3;
    int ret3 = config_parse_args(6, args3, &config3);
    TEST_ASSERT(ret3 != 0, "config_parse_args with empty hostname must fail");
    config_free(&config3);

    /* Invalid: hostname with slash */
    char *args4[] = {"myrun", "run", "--hostname", "bad/name", "echo", "hi", NULL};
    struct myrun_config config4;
    int ret4 = config_parse_args(6, args4, &config4);
    TEST_ASSERT(ret4 != 0, "config_parse_args with bad/name must fail");
    config_free(&config4);

    /* Invalid: missing hostname argument */
    char *args5[] = {"myrun", "run", "--hostname", NULL};
    struct myrun_config config5;
    int ret5 = config_parse_args(3, args5, &config5);
    TEST_ASSERT(ret5 != 0, "config_parse_args with missing hostname parameter must fail");
    config_free(&config5);

    /* Invalid: missing command */
    char *args6[] = {"myrun", "run", "--hostname", "test", NULL};
    struct myrun_config config6;
    int ret6 = config_parse_args(4, args6, &config6);
    TEST_ASSERT(ret6 != 0, "config_parse_args with missing command must fail");
    config_free(&config6);

    TEST_PASS("test_config_hostname_parsing");
}

static void test_uts_namespace_execve_preservation(void) {
    char *argv[] = {"echo", "arg1", "arg2 with spaces", "arg3", NULL};
    struct process_spec spec = {
        .command = "echo",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL,
        .hostname = "echo-box"
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID | MYRUN_NS_UTS, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_SUCCESS, "namespace_spawn should succeed");

    struct process_result result;
    err = process_wait(host_pid, &result);
    if (stack) {
        namespace_free_stack(stack, MYRUN_CHILD_STACK_SIZE);
    }

    TEST_ASSERT(err == MYRUN_SUCCESS, "process_wait should succeed");
    TEST_ASSERT(result.exit_code == 0, "echo with preserved args must succeed");

    TEST_PASS("test_uts_namespace_execve_preservation");
}

int main(void) {
    log_set_level(LOG_LEVEL_WARN);

    printf("=========================================\n");
    printf("     MyRun Phase 3 UTS Namespace Tests   \n");
    printf("=========================================\n\n");

    test_uts_namespace_hostname_isolation();
    test_uts_namespace_host_non_interference();
    test_uts_namespace_uname_nodename();
    test_uts_namespace_default_hostname();
    test_uts_namespace_pid_and_uts_coexistence();
    test_uts_namespace_multiple_containers();
    test_uts_namespace_concurrent_isolation();
    test_uts_namespace_ns_inode_inspection();
    test_uts_namespace_validation();
    test_config_hostname_parsing();
    test_uts_namespace_execve_preservation();

    printf("\n-----------------------------------------\n");
    printf("Tests Passed:  %d\n", g_tests_passed);
    printf("Tests Skipped: %d\n", g_tests_skipped);
    printf("Tests Failed:  %d\n", g_tests_failed);
    printf("-----------------------------------------\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
