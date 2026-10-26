#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "user.h"
#include "log.h"
#include "error.h"

int user_parse_id_map(const char *map_str, struct id_map *map) {
    if (!map_str || !map) {
        return MYRUN_ERR_INVALID_ARG;
    }

    unsigned int inside = 0, outside = 0, count = 1;
    int parsed = sscanf(map_str, "%u:%u:%u", &inside, &outside, &count);
    if (parsed < 2) {
        LOG_ERROR("Invalid ID map format '%s' (expected inside:outside[:count])", map_str);
        return MYRUN_ERR_INVALID_ARG;
    }

    map->inside_id = inside;
    map->outside_id = outside;
    map->count = (parsed >= 3) ? count : 1;
    return MYRUN_SUCCESS;
}

#ifdef __linux__
static int write_file_string(const char *path, const char *str) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        LOG_ERROR("Failed to open '%s': %s", path, strerror(errno));
        return -1;
    }

    size_t len = strlen(str);
    ssize_t written = write(fd, str, len);
    close(fd);

    if (written < 0 || (size_t)written != len) {
        LOG_ERROR("Failed writing '%s' to '%s': %s", str, path, strerror(errno));
        return -1;
    }
    return 0;
}
#endif

int user_namespace_write_mappings(pid_t child_pid, const struct id_map *uid_map, const struct id_map *gid_map) {
#ifdef __linux__
    if (child_pid <= 0) {
        return MYRUN_ERR_INVALID_ARG;
    }

    char path[256];
    char content[256];

    /* Disable setgroups before writing gid_map (required for unprivileged user namespaces) */
    snprintf(path, sizeof(path), "/proc/%d/setgroups", child_pid);
    if (write_file_string(path, "deny\n") < 0) {
        LOG_WARN("Could not write 'deny' to '%s' (may require root or newer kernel)", path);
    }

    /* Write UID mapping */
    if (uid_map && uid_map->count > 0) {
        snprintf(path, sizeof(path), "/proc/%d/uid_map", child_pid);
        snprintf(content, sizeof(content), "%u %u %zu\n",
                 (unsigned int)uid_map->inside_id,
                 (unsigned int)uid_map->outside_id,
                 uid_map->count);
        if (write_file_string(path, content) < 0) {
            LOG_ERROR("Failed to write uid_map for PID %d: %s", child_pid, strerror(errno));
            return MYRUN_ERR_NAMESPACE;
        }
        LOG_DEBUG("Configured UID map for PID %d: %u -> %u (count %zu)",
                  child_pid, uid_map->inside_id, uid_map->outside_id, uid_map->count);
    }

    /* Write GID mapping */
    if (gid_map && gid_map->count > 0) {
        snprintf(path, sizeof(path), "/proc/%d/gid_map", child_pid);
        snprintf(content, sizeof(content), "%u %u %zu\n",
                 (unsigned int)gid_map->inside_id,
                 (unsigned int)gid_map->outside_id,
                 gid_map->count);
        if (write_file_string(path, content) < 0) {
            LOG_ERROR("Failed to write gid_map for PID %d: %s", child_pid, strerror(errno));
            return MYRUN_ERR_NAMESPACE;
        }
        LOG_DEBUG("Configured GID map for PID %d: %u -> %u (count %zu)",
                  child_pid, gid_map->inside_id, gid_map->outside_id, gid_map->count);
    }

    return MYRUN_SUCCESS;
#else
    (void)child_pid;
    (void)uid_map;
    (void)gid_map;
    return MYRUN_SUCCESS;
#endif
}
