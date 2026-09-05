#include <stdlib.h>
#include <string.h>
#include "utils.h"

char *myrun_strdup(const char *s) {
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s);
    char *dup = malloc(len + 1);
    if (!dup) {
        return NULL;
    }
    memcpy(dup, s, len + 1);
    return dup;
}

char **myrun_dup_argv(char *const *argv, int count) {
    if (!argv || count < 0) {
        return NULL;
    }
    char **new_argv = calloc((size_t)count + 1, sizeof(char *));
    if (!new_argv) {
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        if (argv[i]) {
            new_argv[i] = myrun_strdup(argv[i]);
            if (!new_argv[i]) {
                myrun_free_argv(new_argv);
                return NULL;
            }
        }
    }
    new_argv[count] = NULL;
    return new_argv;
}

void myrun_free_argv(char **argv) {
    if (!argv) {
        return;
    }
    for (int i = 0; argv[i] != NULL; i++) {
        free(argv[i]);
    }
    free(argv);
}

bool myrun_str_starts_with(const char *str, const char *prefix) {
    if (!str || !prefix) {
        return false;
    }
    size_t prefix_len = strlen(prefix);
    return strncmp(str, prefix, prefix_len) == 0;
}
