#ifndef MYRUN_USER_H
#define MYRUN_USER_H

#include <sys/types.h>
#include <stdbool.h>
#include "error.h"

/**
 * @file user.h
 * @brief User Namespace (CLONE_NEWUSER) UID/GID mapping and unprivileged isolation.
 */

struct id_map {
    uid_t inside_id;
    uid_t outside_id;
    size_t count;
};

struct user_ns_config {
    bool enabled;
    struct id_map uid_map;
    struct id_map gid_map;
};

/**
 * @brief Writes UID and GID mapping files for the child process in user namespace.
 *
 * @param child_pid Host PID of the container process.
 * @param uid_map UID mapping structure.
 * @param gid_map GID mapping structure.
 * @return MYRUN_SUCCESS on success, or error code.
 */
int user_namespace_write_mappings(pid_t child_pid, const struct id_map *uid_map, const struct id_map *gid_map);

/**
 * @brief Parses an ID map string of the format "inside:outside:count".
 *
 * @param map_str Input string.
 * @param map Output struct id_map.
 * @return MYRUN_SUCCESS on success, or MYRUN_ERR_INVALID_ARG.
 */
int user_parse_id_map(const char *map_str, struct id_map *map);

#endif /* MYRUN_USER_H */
