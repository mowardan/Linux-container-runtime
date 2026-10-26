#ifndef MYRUN_NETWORK_H
#define MYRUN_NETWORK_H

#include <sys/types.h>
#include <stdbool.h>
#include "error.h"

/**
 * @file network.h
 * @brief Network namespace isolation (CLONE_NEWNET), virtual ethernet (veth), bridge, and DNS.
 */

struct net_config {
    bool enabled;
    char *bridge_name;      /**< e.g., "myrun0" */
    char *ip_address;       /**< e.g., "10.0.0.2/24" */
    char *gateway;          /**< e.g., "10.0.0.1" */
    char *dns_server;       /**< e.g., "8.8.8.8" */
};

/**
 * @brief Configures loopback and virtual ethernet link inside container netns.
 *
 * @param container_id Container identifier.
 * @param child_pid Container process PID in host namespace.
 * @param net Config options.
 * @return MYRUN_SUCCESS on success.
 */
int network_setup_veth(const char *container_id, pid_t child_pid, const struct net_config *net);

/**
 * @brief Configures container DNS (/etc/resolv.conf).
 *
 * @param rootfs Path to container rootfs (or "/" if already pivoted).
 * @param dns_server IP address of DNS server.
 * @return MYRUN_SUCCESS on success.
 */
int network_setup_dns(const char *rootfs, const char *dns_server);

/**
 * @brief Cleans up host veth endpoint when container exits.
 */
int network_cleanup(const char *container_id);

#endif /* MYRUN_NETWORK_H */
