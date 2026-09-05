#ifndef MYRUN_UTILS_H
#define MYRUN_UTILS_H

#include <stddef.h>
#include <stdbool.h>

/**
 * @file utils.h
 * @brief Memory, string, and file descriptor helper utilities.
 */

/**
 * @brief Safely duplicates a string, aborting or returning NULL on allocation failure.
 */
char *myrun_strdup(const char *s);

/**
 * @brief Safely duplicates an argument vector (NULL-terminated array of strings).
 */
char **myrun_dup_argv(char *const *argv, int count);

/**
 * @brief Frees a NULL-terminated array of dynamically allocated strings.
 */
void myrun_free_argv(char **argv);

/**
 * @brief Checks whether str starts with prefix.
 */
bool myrun_str_starts_with(const char *str, const char *prefix);

#endif /* MYRUN_UTILS_H */
