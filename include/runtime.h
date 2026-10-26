#ifndef MYRUN_RUNTIME_H
#define MYRUN_RUNTIME_H

#include "config.h"

/**
 * @file runtime.h
 * @brief Top-level container runtime entry points and lifecycle management.
 */

/**
 * @brief Executes a container process according to the provided configuration.
 *
 * Configures process specifications, spawns the container inside isolated namespaces,
 * manages signal forwarding, and monitors lifecycle.
 *
 * @param config Validated runtime configuration.
 * @return Exit code representing child exit status, or shell exit code (126, 127, 128+N).
 */
int runtime_run(const struct myrun_config *config);

#endif /* MYRUN_RUNTIME_H */
