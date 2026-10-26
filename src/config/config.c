#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "namespace.h"
#include "cgroup.h"
#include "user.h"
#include "utils.h"
#include "log.h"

#define MYRUN_VERSION "1.0.0 (Full OCI Container Runtime)"

void config_print_version(void) {
    printf("myrun version %s\n", MYRUN_VERSION);
    printf("A comprehensive educational Linux container runtime from scratch in C\n");
}

void config_print_usage(const char *prog_name) {
    printf("Usage: %s [global options] <command> [command options] [arguments...]\n\n", prog_name);
    printf("Commands:\n");
    printf("  run [options] <executable> [args...]  Run a process / container\n");
    printf("  exec <id> <command> [args...]         Execute a process inside an existing container\n");
    printf("  kill <id> [signal]                    Send signal to container init process (default 15)\n");
    printf("  delete <id>                           Delete container state\n");
    printf("  state <id>                            Query container state and status\n");
    printf("  stats <id>                            Display cgroup resource statistics\n");
    printf("  version                               Show version information\n");
    printf("  help                                  Show this help message\n\n");
    printf("Global Options:\n");
    printf("  -d, --debug                           Enable debug output\n");
    printf("  -v, --verbose                         Enable verbose logging\n");
    printf("  -h, --help                            Show help\n");
    printf("      --version                         Show version\n\n");
    printf("Run Options:\n");
    printf("  --id <name>                           Specify custom container ID\n");
    printf("  -H, --hostname <name>                 Set container hostname (UTS namespace)\n");
    printf("      --cwd <dir>                       Set initial working directory\n");
    printf("      --rootfs <dir>                    Set container root filesystem (pivot_root)\n");
    printf("  -b, --bind <host>:<container>[:ro]    Bind mount a host file or directory\n");
    printf("  -m, --memory <limit>                  Limit memory usage (e.g., 256M, 1G)\n");
    printf("      --cpu <quota_us>                  Limit CPU quota in microseconds per 100ms period\n");
    printf("      --pids <max>                      Limit maximum process count (PID cgroup limit)\n");
    printf("      --net                             Enable isolated network namespace\n");
    printf("      --ip <address>                    Assign container IP address (e.g. 10.0.0.2/24)\n");
    printf("      --dns <ip>                        Configure container DNS nameserver\n");
    printf("      --uid-map <in:out:count>          Map container UID to host UID (User namespace)\n");
    printf("      --gid-map <in:out:count>          Map container GID to host GID (User namespace)\n");
    printf("      --no-new-privs                    Enable PR_SET_NO_NEW_PRIVS\n");
    printf("      --drop-caps                       Drop sensitive capabilities (CAP_SYS_ADMIN, etc.)\n\n");
    printf("Examples:\n");
    printf("  %s run --hostname web /bin/sh\n", prog_name);
    printf("  %s run --rootfs /var/rootfs /bin/sh\n", prog_name);
    printf("  %s run -m 512M --pids 50 /bin/echo \"Constrained container\"\n", prog_name);
    printf("  %s stats box_12345\n", prog_name);
}

int config_parse_args(int argc, char *argv[], struct myrun_config *config) {
    if (!config) {
        return -1;
    }
    memset(config, 0, sizeof(*config));
    config->kill_signal = 15; /* SIGTERM by default */

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
        } else if (strcmp(arg, "run") == 0 || strcmp(arg, "exec") == 0 ||
                   strcmp(arg, "kill") == 0 || strcmp(arg, "delete") == 0 ||
                   strcmp(arg, "state") == 0 || strcmp(arg, "stats") == 0) {
            config->subcommand = myrun_strdup(arg);
            i++;
            break;
        } else if (arg[0] == '-') {
            fprintf(stderr, "Error: unrecognized global option '%s'\n\n", arg);
            config_print_usage(argv[0]);
            return -1;
        } else {
            config->subcommand = myrun_strdup("run");
            break;
        }
    }

    if (!config->subcommand) {
        return 0;
    }

    /* Subcommands: kill, delete, state, stats */
    if (strcmp(config->subcommand, "kill") == 0 || strcmp(config->subcommand, "delete") == 0 ||
        strcmp(config->subcommand, "state") == 0 || strcmp(config->subcommand, "stats") == 0) {
        if (i >= argc) {
            fprintf(stderr, "Error: '%s' requires a container ID argument\n", config->subcommand);
            return -1;
        }
        config->container_id = myrun_strdup(argv[i++]);
        if (strcmp(config->subcommand, "kill") == 0 && i < argc) {
            config->kill_signal = atoi(argv[i++]);
            if (config->kill_signal <= 0) config->kill_signal = 15;
        }
        return 0;
    }

    /* Subcommand: exec */
    if (strcmp(config->subcommand, "exec") == 0) {
        if (i >= argc) {
            fprintf(stderr, "Error: 'exec' requires container ID and command\n");
            return -1;
        }
        config->container_id = myrun_strdup(argv[i++]);
        if (i >= argc) {
            fprintf(stderr, "Error: 'exec' requires a command to run\n");
            return -1;
        }
        config->command_argc = argc - i;
        config->command_argv = myrun_dup_argv(&argv[i], config->command_argc);
        return 0;
    }

    /* Subcommand: run options */
    while (i < argc) {
        const char *arg = argv[i];

        if (strcmp(arg, "--id") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --id requires a container ID\n");
                return -1;
            }
            config->container_id = myrun_strdup(argv[++i]);
            i++;
        } else if (strcmp(arg, "--cwd") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --cwd requires a directory path\n");
                return -1;
            }
            config->cwd = myrun_strdup(argv[++i]);
            i++;
        } else if (strcmp(arg, "-H") == 0 || strcmp(arg, "--hostname") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: %s requires a hostname argument\n", arg);
                return -1;
            }
            const char *hostname_arg = argv[++i];
            if (hostname_validate(hostname_arg) != MYRUN_SUCCESS) {
                fprintf(stderr, "myrun: invalid hostname '%s'\n", hostname_arg);
                return -1;
            }
            config->hostname = myrun_strdup(hostname_arg);
            i++;
        } else if (strcmp(arg, "--rootfs") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --rootfs requires a directory path\n");
                return -1;
            }
            config->rootfs = myrun_strdup(argv[++i]);
            i++;
        } else if (strcmp(arg, "-b") == 0 || strcmp(arg, "--bind") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --bind requires a mount spec\n");
                return -1;
            }
            char *b_str = argv[++i];
            char *colon = strchr(b_str, ':');
            if (colon && config->bind_count < 16) {
                *colon = '\0';
                config->binds[config->bind_count].source = myrun_strdup(b_str);
                char *target = colon + 1;
                char *ro_opt = strchr(target, ':');
                if (ro_opt) {
                    *ro_opt = '\0';
                    config->binds[config->bind_count].readonly = (strcmp(ro_opt + 1, "ro") == 0);
                }
                config->binds[config->bind_count].target = myrun_strdup(target);
                config->bind_count++;
            }
            i++;
        } else if (strcmp(arg, "-m") == 0 || strcmp(arg, "--memory") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --memory requires a limit\n");
                return -1;
            }
            config->cgroups.memory_max = cgroup_parse_memory_bytes(argv[++i]);
            i++;
        } else if (strcmp(arg, "--cpu") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --cpu requires quota\n");
                return -1;
            }
            config->cgroups.cpu_quota_us = atoll(argv[++i]);
            config->cgroups.cpu_period_us = 100000;
            i++;
        } else if (strcmp(arg, "--pids") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --pids requires max limit\n");
                return -1;
            }
            config->cgroups.pids_max = atol(argv[++i]);
            i++;
        } else if (strcmp(arg, "--net") == 0) {
            config->net.enabled = true;
            i++;
        } else if (strcmp(arg, "--ip") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --ip requires an address\n");
                return -1;
            }
            config->net.enabled = true;
            config->net.ip_address = myrun_strdup(argv[++i]);
            i++;
        } else if (strcmp(arg, "--dns") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --dns requires a server IP\n");
                return -1;
            }
            config->net.dns_server = myrun_strdup(argv[++i]);
            i++;
        } else if (strcmp(arg, "--uid-map") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --uid-map requires a mapping\n");
                return -1;
            }
            config->user_ns.enabled = true;
            user_parse_id_map(argv[++i], &config->user_ns.uid_map);
            i++;
        } else if (strcmp(arg, "--gid-map") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --gid-map requires a mapping\n");
                return -1;
            }
            config->user_ns.enabled = true;
            user_parse_id_map(argv[++i], &config->user_ns.gid_map);
            i++;
        } else if (strcmp(arg, "--no-new-privs") == 0) {
            config->sec.no_new_privs = true;
            i++;
        } else if (strcmp(arg, "--drop-caps") == 0) {
            config->sec.drop_capabilities = true;
            i++;
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
    if (!config) return;
    if (config->subcommand) {
        free(config->subcommand);
        config->subcommand = NULL;
    }
    if (config->container_id) {
        free(config->container_id);
        config->container_id = NULL;
    }
    if (config->bundle_path) {
        free(config->bundle_path);
        config->bundle_path = NULL;
    }
    if (config->cwd) {
        free(config->cwd);
        config->cwd = NULL;
    }
    if (config->hostname) {
        free(config->hostname);
        config->hostname = NULL;
    }
    if (config->rootfs) {
        free(config->rootfs);
        config->rootfs = NULL;
    }
    for (size_t i = 0; i < config->bind_count; i++) {
        if (config->binds[i].source) free(config->binds[i].source);
        if (config->binds[i].target) free(config->binds[i].target);
    }
    if (config->net.bridge_name) free(config->net.bridge_name);
    if (config->net.ip_address) free(config->net.ip_address);
    if (config->net.gateway) free(config->net.gateway);
    if (config->net.dns_server) free(config->net.dns_server);
    if (config->command_argv) {
        myrun_free_argv(config->command_argv);
        config->command_argv = NULL;
    }
}
