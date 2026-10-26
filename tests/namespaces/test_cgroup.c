#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgroup.h"
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

static void test_cgroup_parsing(void) {
    TEST_ASSERT(cgroup_parse_memory_bytes("1024") == 1024, "Raw bytes parse");
    TEST_ASSERT(cgroup_parse_memory_bytes("64k") == 64 * 1024, "Kilobytes parse");
    TEST_ASSERT(cgroup_parse_memory_bytes("128M") == 128LL * 1024 * 1024, "Megabytes parse");
    TEST_ASSERT(cgroup_parse_memory_bytes("2G") == 2LL * 1024 * 1024 * 1024, "Gigabytes parse");
    TEST_ASSERT(cgroup_parse_memory_bytes("invalid") == -1, "Invalid string returns -1");
    TEST_PASS("test_cgroup_parsing");
}

int main(void) {
    printf("=========================================\n");
    printf("      MyRun cgroups v2 Test Suite        \n");
    printf("=========================================\n\n");

    test_cgroup_parsing();

    printf("\n-----------------------------------------\n");
    printf("Tests Passed: %d\n", g_tests_passed);
    printf("Tests Failed: %d\n", g_tests_failed);
    printf("-----------------------------------------\n");

    return g_tests_failed ? 1 : 0;
}
