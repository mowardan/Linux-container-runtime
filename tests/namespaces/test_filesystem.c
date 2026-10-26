#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "filesystem.h"
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

static void test_filesystem_validate(void) {
    TEST_ASSERT(filesystem_validate_rootfs(NULL) == MYRUN_ERR_INVALID_ARG, "NULL rootfs should return INVALID_ARG");
    TEST_ASSERT(filesystem_validate_rootfs("/nonexistent/directory/xyz_99") != MYRUN_SUCCESS, "Nonexistent path should fail");
    TEST_ASSERT(filesystem_validate_rootfs("/tmp") == MYRUN_SUCCESS, "/tmp should be valid rootfs path");
    TEST_PASS("test_filesystem_validate");
}

int main(void) {
    printf("=========================================\n");
    printf("    MyRun Filesystem Isolation Tests     \n");
    printf("=========================================\n\n");

    test_filesystem_validate();

    printf("\n-----------------------------------------\n");
    printf("Tests Passed: %d\n", g_tests_passed);
    printf("Tests Failed: %d\n", g_tests_failed);
    printf("-----------------------------------------\n");

    return g_tests_failed ? 1 : 0;
}
