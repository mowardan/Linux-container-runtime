#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "runtime.h"
#include "log.h"
#include "error.h"

int main(int argc, char *argv[]) {
    struct myrun_config config;
    int parse_res = config_parse_args(argc, argv, &config);

    if (parse_res != 0) {
        config_free(&config);
        /* If 1, it handled help or version; if negative, syntax/parse error */
        return (parse_res > 0) ? 0 : 1;
    }

    int exit_code = 0;

    if (config.subcommand && strcmp(config.subcommand, "run") == 0) {
        exit_code = runtime_run(&config);
    } else {
        fprintf(stderr, "Error: Unknown or missing subcommand\n\n");
        config_print_usage(argv[0]);
        exit_code = 1;
    }

    config_free(&config);
    return exit_code;
}
