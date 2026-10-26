#ifndef MYRUN_CGROUP_H
#define MYRUN_CGROUP_H

#include <sys/types.h>
#include <stdbool.h>
#include <stddef.h>
#include "error.h"

/**
 * @file cgroup.h
 * @brief Linux cgroups v2 resource limit enforcement (CPU, Memory, PIDs) and statistics.
 */

struct cgroup_limits {
    long long memory_max;      /**< Memory limit in bytes (-1 for max) */
    long long cpu_quota_us;    /**< CPU quota in microseconds (-1 for max) */
    long long cpu_period_us;   /**< CPU period in microseconds (usually 100000) */
    long pids_max;             /**< Max processes count (-1 for max) */
};

struct cgroup_stats {
    unsigned long long memory_current;
    unsigned long long cpu_usage_usec;
    unsigned long pids_current;
};

/**
 * @brief Checks if cgroups v2 is mounted at /sys/fs/cgroup.
 */
bool cgroup_v2_available(void);

/**
 * @brief Creates a cgroup v2 subtree for the container and applies limits.
 *
 * Path: /sys/fs/cgroup/myrun/<container_id>/
 *
 * @param container_id Identifier for the container.
 * @param limits Limits to apply (memory, cpu, pids).
 * @return MYRUN_SUCCESS on success, or an error code.
 */
int cgroup_create_and_apply(const char *container_id, const struct cgroup_limits *limits);

/**
 * @brief Moves process pid into the container cgroup.
 *
 * @param container_id Container identifier.
 * @param pid Process ID to move.
 * @return MYRUN_SUCCESS on success, or an error code.
 */
int cgroup_attach_process(const char *container_id, pid_t pid);

/**
 * @brief Collects usage statistics from the container cgroup.
 *
 * @param container_id Container identifier.
 * @param stats Output stats structure.
 * @return MYRUN_SUCCESS on success, or an error code.
 */
int cgroup_read_stats(const char *container_id, struct cgroup_stats *stats);

/**
 * @brief Destroys container cgroup directory after processes terminate.
 *
 * @param container_id Container identifier.
 * @return MYRUN_SUCCESS on success.
 */
int cgroup_destroy(const char *container_id);

/**
 * @brief Parses a human-readable memory string (e.g., "512M", "1G", "64k") into bytes.
 */
long long cgroup_parse_memory_bytes(const char *str);

#endif /* MYRUN_CGROUP_H */
