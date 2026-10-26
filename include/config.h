#ifndef MYRUN_CONFIG_H
#define MYRUN_CONFIG_H

#include <stdbool.h>
#include "cgroup.h"
#include "user.h"
#include "network.h"
#include "filesystem.h"
#include "security.h"

/**
 * @file config.h
 * @brief Command-line configuration parsing and container settings.
 */

struct myrun_config {
    char *subcommand;            /**< Subcommand: "run", "create", "start", "kill", "delete", "state", "exec", "stats", "version", "help" */
    char *container_id;          /**< Container ID */
    char *bundle_path;           /**< Path to OCI bundle */
    char **command_argv;         /**< Target command and its argument vector */
    int command_argc;            /**< Argument count of the target command */
    char *cwd;                   /**< Initial working directory */
    char *hostname;              /**< Container hostname (UTS namespace) */
    char *rootfs;                /**< Container rootfs path */
    struct bind_mount binds[16]; /**< Bind mounts */
    size_t bind_count;           /**< Number of bind mounts */
    struct cgroup_limits cgroups;/**< Resource constraints */
    struct user_ns_config user_ns;/**< User namespace mapping */
    struct net_config net;       /**< Network namespace configuration */
    struct security_config sec;  /**< Security constraints */
    bool debug;                  /**< Enable debug logging */
    bool verbose;                /**< Enable verbose info logging */
    int kill_signal;             /**< Signal to send for kill subcommand */
};

/**
 * @brief Parses command-line arguments into a struct myrun_config.
 */
int config_parse_args(int argc, char *argv[], struct myrun_config *config);

/**
 * @brief Frees resources allocated in myrun_config.
 */
void config_free(struct myrun_config *config);

/**
 * @brief Prints CLI usage and help message to stdout.
 */
void config_print_usage(const char *prog_name);

/**
 * @brief Prints version information to stdout.
 */
void config_print_version(void);

#endif /* MYRUN_CONFIG_H */
