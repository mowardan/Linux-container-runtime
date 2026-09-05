#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "namespace.h"
#include "process.h"
#include "config.h"
#include "mount.h"
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

static void test_mount_namespace_clone_newns(void) {
#ifndef __linux__
    TEST_SKIP("test_mount_namespace_clone_newns", "Requires Linux CLONE_NEWNS");
    return;
#else
    char *argv[] = {"sh", "-c", "exit 0", NULL};
    struct process_spec spec = {
        .command = "sh",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL,
        .hostname = "mount-test"
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID | MYRUN_NS_UTS | MYRUN_NS_MOUNT, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_SUCCESS, "namespace_spawn with MYRUN_NS_MOUNT should succeed");
    TEST_ASSERT(host_pid > 0, "Host PID must be positive");

    struct process_result result;
    err = process_wait(host_pid, &result);
    if (stack) {
        namespace_free_stack(stack, MYRUN_CHILD_STACK_SIZE);
    }

    TEST_ASSERT(err == MYRUN_SUCCESS, "process_wait should succeed");
    TEST_ASSERT(result.exited_normally == true, "Child should exit normally");
    TEST_ASSERT(result.exit_code == 0, "Exit code must be 0");

    TEST_PASS("test_mount_namespace_clone_newns");
#endif
}

static void test_mount_namespace_tmpfs_isolation(void) {
#ifndef __linux__
    TEST_SKIP("test_mount_namespace_tmpfs_isolation", "Requires Linux CLONE_NEWNS");
    return;
#else
    /* Mount a private tmpfs inside the container and verify files in it */
    char *argv[] = {
        "sh",
        "-c",
        "mkdir -p /tmp/myrun_mnt_test && "
        "mount -t tmpfs tmpfs /tmp/myrun_mnt_test && "
        "touch /tmp/myrun_mnt_test/private_container_file.txt && "
        "test -f /tmp/myrun_mnt_test/private_container_file.txt",
        NULL
    };
    struct process_spec spec = {
        .command = "sh",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL,
        .hostname = "tmpfs-test"
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID | MYRUN_NS_UTS | MYRUN_NS_MOUNT, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_SUCCESS, "namespace_spawn should succeed");

    struct process_result result;
    err = process_wait(host_pid, &result);
    if (stack) {
        namespace_free_stack(stack, MYRUN_CHILD_STACK_SIZE);
    }

    TEST_ASSERT(err == MYRUN_SUCCESS, "process_wait should succeed");
    TEST_ASSERT(result.exit_code == 0, "Container tmpfs creation and file check should succeed");

    /* Host verifies that the container's tmpfs mount does NOT exist on the host */
    FILE *f = fopen("/proc/mounts", "r");
    bool found_on_host = false;
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "/tmp/myrun_mnt_test")) {
                found_on_host = true;
                break;
            }
        }
        fclose(f);
    }

    TEST_ASSERT(found_on_host == false, "Container tmpfs mount must NOT be present in host /proc/mounts");

    TEST_PASS("test_mount_namespace_tmpfs_isolation");
#endif
}

static void test_mount_namespace_host_non_interference(void) {
#ifndef __linux__
    TEST_SKIP("test_mount_namespace_host_non_interference", "Requires Linux CLONE_NEWNS");
    return;
#else
    /* Count mounts on host before */
    int mounts_before = 0;
    FILE *f1 = fopen("/proc/mounts", "r");
    if (f1) {
        char line[512];
        while (fgets(line, sizeof(line), f1)) {
            mounts_before++;
        }
        fclose(f1);
    }

    /* Spawn container that performs multiple mount / unmount actions */
    char *argv[] = {
        "sh",
        "-c",
        "mkdir -p /tmp/myrun_transient_mnt && "
        "mount -t tmpfs tmpfs /tmp/myrun_transient_mnt && "
        "umount /tmp/myrun_transient_mnt && "
        "rmdir /tmp/myrun_transient_mnt",
        NULL
    };
    struct process_spec spec = {
        .command = "sh",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL,
        .hostname = "non-interference"
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID | MYRUN_NS_UTS | MYRUN_NS_MOUNT, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_SUCCESS, "namespace_spawn should succeed");

    struct process_result result;
    process_wait(host_pid, &result);
    if (stack) {
        namespace_free_stack(stack, MYRUN_CHILD_STACK_SIZE);
    }

    /* Count mounts on host after */
    int mounts_after = 0;
    FILE *f2 = fopen("/proc/mounts", "r");
    if (f2) {
        char line[512];
        while (fgets(line, sizeof(line), f2)) {
            mounts_after++;
        }
        fclose(f2);
    }

    TEST_ASSERT(mounts_before == mounts_after, "Host mount count must remain unchanged");

    TEST_PASS("test_mount_namespace_host_non_interference");
#endif
}

static void test_mount_namespace_proc_ns_inode(void) {
#ifndef __linux__
    TEST_SKIP("test_mount_namespace_proc_ns_inode", "Requires Linux /proc/self/ns/mnt");
    return;
#else
    char *argv[] = {"sleep", "0.2", NULL};
    struct process_spec spec = {
        .command = "sleep",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL,
        .hostname = "ns-inode-check"
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID | MYRUN_NS_UTS | MYRUN_NS_MOUNT, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_SUCCESS, "namespace_spawn should succeed");
    TEST_ASSERT(host_pid > 0, "Host PID should be positive");

    char host_ns[256] = {0};
    char child_ns[256] = {0};

    ssize_t host_len = readlink("/proc/self/ns/mnt", host_ns, sizeof(host_ns) - 1);
    char child_path[128];
    snprintf(child_path, sizeof(child_path), "/proc/%d/ns/mnt", host_pid);
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
                    "Host and container must point to distinct Mount namespace inodes in /proc");
    }

    TEST_PASS("test_mount_namespace_proc_ns_inode");
#endif
}

static void test_mount_pid_uts_mount_coexistence(void) {
#ifndef __linux__
    TEST_SKIP("test_mount_pid_uts_mount_coexistence", "Requires Linux namespaces");
    return;
#else
    char *argv[] = {
        "sh",
        "-c",
        "test \"$$\" = \"1\" && test \"$(uname -n)\" = \"tri-namespace-box\"",
        NULL
    };
    struct process_spec spec = {
        .command = "sh",
        .argv = argv,
        .envp = NULL,
        .cwd = NULL,
        .hostname = "tri-namespace-box"
    };

    pid_t host_pid = 0;
    void *stack = NULL;
    int err = namespace_spawn(&spec, MYRUN_NS_PID | MYRUN_NS_UTS | MYRUN_NS_MOUNT, &host_pid, &stack);
    TEST_ASSERT(err == MYRUN_SUCCESS, "namespace_spawn should succeed");

    struct process_result result;
    err = process_wait(host_pid, &result);
    if (stack) {
        namespace_free_stack(stack, MYRUN_CHILD_STACK_SIZE);
    }

    TEST_ASSERT(err == MYRUN_SUCCESS, "process_wait should succeed");
    TEST_ASSERT(result.exit_code == 0,
                "Container must simultaneously have PID 1, private UTS hostname, and private Mount namespace");

    TEST_PASS("test_mount_pid_uts_mount_coexistence");
#endif
}

int main(void) {
    log_set_level(LOG_LEVEL_WARN);

    printf("=========================================\n");
    printf("    MyRun Phase 4 Mount Namespace Tests  \n");
    printf("=========================================\n\n");

    test_mount_namespace_clone_newns();
    test_mount_namespace_tmpfs_isolation();
    test_mount_namespace_host_non_interference();
    test_mount_namespace_proc_ns_inode();
    test_mount_pid_uts_mount_coexistence();

    printf("\n-----------------------------------------\n");
    printf("Tests Passed:  %d\n", g_tests_passed);
    printf("Tests Skipped: %d\n", g_tests_skipped);
    printf("Tests Failed:  %d\n", g_tests_failed);
    printf("-----------------------------------------\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
