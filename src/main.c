#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "runtime.h"
#include "container.h"
#include "cgroup.h"
#include "log.h"
#include "error.h"

int main(int argc, char *argv[]) {
    struct myrun_config config;
    int parse_res = config_parse_args(argc, argv, &config);

    if (parse_res != 0) {
        config_free(&config);
        return (parse_res > 0) ? 0 : 1;
    }

    int exit_code = 0;

    if (config.subcommand && strcmp(config.subcommand, "run") == 0) {
        exit_code = runtime_run(&config);
    } else if (config.subcommand && strcmp(config.subcommand, "exec") == 0) {
        exit_code = container_exec(config.container_id, config.command_argc, config.command_argv);
    } else if (config.subcommand && strcmp(config.subcommand, "kill") == 0) {
        exit_code = container_kill(config.container_id, config.kill_signal);
    } else if (config.subcommand && strcmp(config.subcommand, "delete") == 0) {
        exit_code = container_delete_state(config.container_id);
    } else if (config.subcommand && strcmp(config.subcommand, "state") == 0) {
        struct container_state state;
        if (container_load_state(config.container_id, &state) == MYRUN_SUCCESS) {
            printf("{\n  \"id\": \"%s\",\n  \"status\": %d,\n  \"pid\": %d\n}\n",
                   state.id, state.status, (int)state.init_process_pid);
            exit_code = 0;
        } else {
            fprintf(stderr, "Error: Container '%s' not found\n", config.container_id);
            exit_code = 1;
        }
    } else if (config.subcommand && strcmp(config.subcommand, "stats") == 0) {
        struct cgroup_stats stats;
        if (cgroup_read_stats(config.container_id, &stats) == MYRUN_SUCCESS) {
            printf("Memory Usage: %llu bytes\n", stats.memory_current);
            printf("PIDs Count:   %lu\n", stats.pids_current);
            exit_code = 0;
        } else {
            fprintf(stderr, "Error: Could not read stats for container '%s'\n", config.container_id);
            exit_code = 1;
        }
    } else {
        fprintf(stderr, "Error: Unknown or missing subcommand\n\n");
        config_print_usage(argv[0]);
        exit_code = 1;
    }

    config_free(&config);
    return exit_code;
}
