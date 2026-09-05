#ifndef MYRUN_CONFIG_H
#define MYRUN_CONFIG_H

#include <stdbool.h>

/**
 * @file config.h
 * @brief Command-line configuration parsing and container settings.
 */

struct myrun_config {
    char *subcommand;       /**< Subcommand: "run", "help", "version", etc. */
    char **command_argv;    /**< Target command and its argument vector */
    int command_argc;       /**< Argument count of the target command */
    char *cwd;              /**< Initial working directory */
    char *hostname;         /**< Container hostname (UTS namespace) */
    bool debug;             /**< Enable debug logging */
    bool verbose;           /**< Enable verbose info logging */
};

/**
 * @brief Parses command-line arguments into a struct myrun_config.
 *
 * @param argc Argument count.
 * @param argv Argument array.
 * @param config Output configuration structure.
 * @return 0 on success, non-zero on error or if help/version handled.
 */
int config_parse_args(int argc, char *argv[], struct myrun_config *config);

/**
 * @brief Frees resources allocated in myrun_config.
 *
 * @param config Configuration structure.
 */
void config_free(struct myrun_config *config);

/**
 * @brief Prints CLI usage and help message to stdout.
 *
 * @param prog_name Name of the running executable.
 */
void config_print_usage(const char *prog_name);

/**
 * @brief Prints version information to stdout.
 */
void config_print_version(void);

#endif /* MYRUN_CONFIG_H */
