#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "cgroup.h"
#include "log.h"
#include "error.h"

#define CGROUP_V2_BASE "/sys/fs/cgroup"
#define CGROUP_MYRUN_BASE "/sys/fs/cgroup/myrun"

long long cgroup_parse_memory_bytes(const char *str) {
    if (!str || !*str) return -1;
    char *endptr = NULL;
    long long val = strtoll(str, &endptr, 10);
    if (val <= 0) return -1;

    if (*endptr == '\0') {
        return val;
    } else if (strcasecmp(endptr, "k") == 0 || strcasecmp(endptr, "kb") == 0) {
        return val * 1024LL;
    } else if (strcasecmp(endptr, "m") == 0 || strcasecmp(endptr, "mb") == 0) {
        return val * 1024LL * 1024LL;
    } else if (strcasecmp(endptr, "g") == 0 || strcasecmp(endptr, "gb") == 0) {
        return val * 1024LL * 1024LL * 1024LL;
    }
    return val;
}

bool cgroup_v2_available(void) {
#ifdef __linux__
    struct stat st;
    if (stat(CGROUP_V2_BASE "/cgroup.controllers", &st) == 0) {
        return true;
    }
    return false;
#else
    return false;
#endif
}

#ifdef __linux__
static int write_cgroup_file(const char *path, const char *val) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        LOG_WARN("Could not open cgroup file '%s': %s", path, strerror(errno));
        return -1;
    }

    size_t len = strlen(val);
    ssize_t written = write(fd, val, len);
    close(fd);
    return (written == (ssize_t)len) ? 0 : -1;
}
#endif

int cgroup_create_and_apply(const char *container_id, const struct cgroup_limits *limits) {
#ifdef __linux__
    if (!cgroup_v2_available()) {
        LOG_WARN("cgroups v2 not available or not mounted; resource constraints skipped");
        return MYRUN_SUCCESS;
    }

    /* Ensure /sys/fs/cgroup/myrun exists */
    mkdir(CGROUP_MYRUN_BASE, 0755);

    /* Enable controllers in parent if possible */
    write_cgroup_file(CGROUP_MYRUN_BASE "/cgroup.subtree_control", "+cpu +memory +pids");

    /* Create container cgroup directory */
    char cg_path[512];
    snprintf(cg_path, sizeof(cg_path), "%s/%s", CGROUP_MYRUN_BASE, container_id);
    if (mkdir(cg_path, 0755) < 0 && errno != EEXIST) {
        LOG_WARN("Failed to create cgroup directory '%s': %s", cg_path, strerror(errno));
        return MYRUN_SUCCESS;
    }

    if (!limits) {
        return MYRUN_SUCCESS;
    }

    char file_path[1024];
    char buf[128];

    /* Apply Memory Limit */
    if (limits->memory_max > 0) {
        snprintf(file_path, sizeof(file_path), "%s/memory.max", cg_path);
        snprintf(buf, sizeof(buf), "%lld\n", limits->memory_max);
        if (write_cgroup_file(file_path, buf) == 0) {
            LOG_DEBUG("Configured memory.max = %lld bytes", limits->memory_max);
        }
    }

    /* Apply CPU Limit */
    if (limits->cpu_quota_us > 0) {
        snprintf(file_path, sizeof(file_path), "%s/cpu.max", cg_path);
        long long period = (limits->cpu_period_us > 0) ? limits->cpu_period_us : 100000LL;
        snprintf(buf, sizeof(buf), "%lld %lld\n", limits->cpu_quota_us, period);
        if (write_cgroup_file(file_path, buf) == 0) {
            LOG_DEBUG("Configured cpu.max = %lld %lld", limits->cpu_quota_us, period);
        }
    }

    /* Apply PIDs Limit */
    if (limits->pids_max > 0) {
        snprintf(file_path, sizeof(file_path), "%s/pids.max", cg_path);
        snprintf(buf, sizeof(buf), "%ld\n", limits->pids_max);
        if (write_cgroup_file(file_path, buf) == 0) {
            LOG_DEBUG("Configured pids.max = %ld", limits->pids_max);
        }
    }

    return MYRUN_SUCCESS;
#else
    (void)container_id;
    (void)limits;
    return MYRUN_SUCCESS;
#endif
}

int cgroup_attach_process(const char *container_id, pid_t pid) {
#ifdef __linux__
    if (!cgroup_v2_available()) {
        return MYRUN_SUCCESS;
    }

    char procs_path[512];
    snprintf(procs_path, sizeof(procs_path), "%s/%s/cgroup.procs", CGROUP_MYRUN_BASE, container_id);

    char pid_str[64];
    snprintf(pid_str, sizeof(pid_str), "%d\n", (int)pid);

    if (write_cgroup_file(procs_path, pid_str) < 0) {
        LOG_WARN("Could not attach PID %d to cgroup '%s'", pid, procs_path);
        return MYRUN_ERR_GENERIC;
    }

    LOG_DEBUG("Attached PID %d to cgroup '%s'", pid, procs_path);
    return MYRUN_SUCCESS;
#else
    (void)container_id;
    (void)pid;
    return MYRUN_SUCCESS;
#endif
}

int cgroup_read_stats(const char *container_id, struct cgroup_stats *stats) {
#ifdef __linux__
    if (!stats || !container_id) return MYRUN_ERR_INVALID_ARG;
    memset(stats, 0, sizeof(*stats));

    char path[512];
    FILE *f;

    snprintf(path, sizeof(path), "%s/%s/memory.current", CGROUP_MYRUN_BASE, container_id);
    f = fopen(path, "r");
    if (f) {
        unsigned long long mem = 0;
        if (fscanf(f, "%llu", &mem) == 1) stats->memory_current = mem;
        fclose(f);
    }

    snprintf(path, sizeof(path), "%s/%s/pids.current", CGROUP_MYRUN_BASE, container_id);
    f = fopen(path, "r");
    if (f) {
        unsigned long pids = 0;
        if (fscanf(f, "%lu", &pids) == 1) stats->pids_current = pids;
        fclose(f);
    }

    return MYRUN_SUCCESS;
#else
    (void)container_id;
    (void)stats;
    return MYRUN_SUCCESS;
#endif
}

int cgroup_destroy(const char *container_id) {
#ifdef __linux__
    if (!container_id) return MYRUN_SUCCESS;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", CGROUP_MYRUN_BASE, container_id);
    rmdir(path);
    return MYRUN_SUCCESS;
#else
    (void)container_id;
    return MYRUN_SUCCESS;
#endif
}
