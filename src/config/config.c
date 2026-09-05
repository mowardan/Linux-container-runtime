#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "namespace.h"
#include "utils.h"
#include "log.h"

#define MYRUN_VERSION "0.3.0-alpha (Phase 3: UTS Namespace)"

void config_print_version(void) {
    printf("myrun version %s\n", MYRUN_VERSION);
    printf("An educational Linux container runtime from scratch in C\n");
}

void config_print_usage(const char *prog_name) {
    printf("Usage: %s [global options] <command> [command options] [arguments...]\n\n", prog_name);
    printf("Commands:\n");
    printf("  run [options] <executable> [args...]  Run a process / container\n");
    printf("  version                               Show version information\n");
    printf("  help                                  Show this help message\n\n");
    printf("Global Options:\n");
    printf("  -d, --debug                           Enable debug output\n");
    printf("  -v, --verbose                         Enable verbose logging\n");
    printf("  -h, --help                            Show help\n");
    printf("      --version                         Show version\n\n");
    printf("Run Options:\n");
    printf("  -H, --hostname <name>                 Set container hostname (UTS namespace)\n");
    printf("      --cwd <dir>                       Set initial working directory\n\n");
    printf("Examples:\n");
    printf("  %s run --hostname web /bin/sh\n", prog_name);
    printf("  %s run --hostname mybox /bin/hostname\n", prog_name);
    printf("  %s run /bin/echo \"Hello, World!\"\n", prog_name);
    printf("  %s run --debug /bin/sh -c \"echo PID: $$\"\n", prog_name);
}

int config_parse_args(int argc, char *argv[], struct myrun_config *config) {
    if (!config) {
        return -1;
    }
    memset(config, 0, sizeof(*config));

    if (argc < 2) {
        config_print_usage(argv[0]);
        return 1;
    }

    int i = 1;
    while (i < argc) {
        const char *arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0 || strcmp(arg, "help") == 0) {
            config_print_usage(argv[0]);
            return 1;
        } else if (strcmp(arg, "--version") == 0 || strcmp(arg, "version") == 0) {
            config_print_version();
            return 1;
        } else if (strcmp(arg, "-d") == 0 || strcmp(arg, "--debug") == 0) {
            config->debug = true;
            log_set_level(LOG_LEVEL_DEBUG);
            i++;
        } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            config->verbose = true;
            log_set_level(LOG_LEVEL_INFO);
            i++;
        } else if (strcmp(arg, "run") == 0) {
            config->subcommand = myrun_strdup("run");
            i++;
            break;
        } else if (arg[0] == '-') {
            fprintf(stderr, "Error: unrecognized global option '%s'\n\n", arg);
            config_print_usage(argv[0]);
            return -1;
        } else {
            /* If no explicit subcommand given, treat as 'run' */
            config->subcommand = myrun_strdup("run");
            break;
        }
    }

    if (!config->subcommand || strcmp(config->subcommand, "run") != 0) {
        return 0;
    }

    /* Parse 'run' specific options */
    while (i < argc) {
        const char *arg = argv[i];

        if (strcmp(arg, "--cwd") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --cwd requires a directory path\n");
                return -1;
            }
            config->cwd = myrun_strdup(argv[i + 1]);
            i += 2;
        } else if (strcmp(arg, "-H") == 0 || strcmp(arg, "--hostname") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: %s requires a hostname argument\n", arg);
                return -1;
            }
            const char *hostname_arg = argv[i + 1];
            if (hostname_validate(hostname_arg) != MYRUN_SUCCESS) {
                fprintf(stderr, "myrun: invalid hostname '%s'\n", hostname_arg);
                return -1;
            }
            config->hostname = myrun_strdup(hostname_arg);
            i += 2;
        } else if (strcmp(arg, "-d") == 0 || strcmp(arg, "--debug") == 0) {
            config->debug = true;
            log_set_level(LOG_LEVEL_DEBUG);
            i++;
        } else if (strcmp(arg, "--") == 0) {
            i++;
            break;
        } else if (arg[0] == '-') {
            fprintf(stderr, "Error: unrecognized run option '%s'\n", arg);
            return -1;
        } else {
            /* Beginning of target command */
            break;
        }
    }

    if (i >= argc) {
        fprintf(stderr, "Error: 'run' requires a command to execute\n\n");
        config_print_usage(argv[0]);
        return -1;
    }

    config->command_argc = argc - i;
    config->command_argv = myrun_dup_argv(&argv[i], config->command_argc);
    if (!config->command_argv) {
        LOG_ERROR("Failed to allocate memory for command arguments");
        return -1;
    }

    return 0;
}

void config_free(struct myrun_config *config) {
    if (!config) {
        return;
    }
    if (config->subcommand) {
        free(config->subcommand);
        config->subcommand = NULL;
    }
    if (config->cwd) {
        free(config->cwd);
        config->cwd = NULL;
    }
    if (config->hostname) {
        free(config->hostname);
        config->hostname = NULL;
    }
    if (config->command_argv) {
        myrun_free_argv(config->command_argv);
        config->command_argv = NULL;
    }
}
