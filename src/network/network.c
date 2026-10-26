#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "network.h"
#include "log.h"
#include "error.h"

#ifdef __linux__
static void run_cmd(const char *cmd) {
    int res = system(cmd);
    (void)res;
}
#endif

int network_setup_veth(const char *container_id, pid_t child_pid, const struct net_config *net) {
#ifdef __linux__
    if (!net || !net->enabled || child_pid <= 0) {
        return MYRUN_SUCCESS;
    }

    char host_veth[64];
    char peer_veth[64];
    snprintf(host_veth, sizeof(host_veth), "veth_%s", container_id);
    snprintf(peer_veth, sizeof(peer_veth), "ceth_%s", container_id);

    char cmd[512];
    LOG_INFO("Configuring container veth pair: %s <-> %s for PID %d", host_veth, peer_veth, child_pid);

    /* 1. Create veth pair on host */
    snprintf(cmd, sizeof(cmd), "ip link add %s type veth peer name %s", host_veth, peer_veth);
    if (system(cmd) != 0) {
        LOG_WARN("Failed creating veth pair: %s", cmd);
        return MYRUN_ERR_GENERIC;
    }

    /* 2. Attach host_veth to bridge if configured */
    const char *bridge = net->bridge_name ? net->bridge_name : "myrun0";
    snprintf(cmd, sizeof(cmd), "ip link set %s master %s 2>/dev/null || true", host_veth, bridge);
    run_cmd(cmd);

    /* 3. Bring host_veth up */
    snprintf(cmd, sizeof(cmd), "ip link set %s up", host_veth);
    run_cmd(cmd);

    /* 4. Move peer_veth into container netns */
    snprintf(cmd, sizeof(cmd), "ip link set %s netns %d", peer_veth, (int)child_pid);
    if (system(cmd) != 0) {
        LOG_WARN("Failed moving %s into netns of PID %d", peer_veth, child_pid);
        return MYRUN_ERR_GENERIC;
    }

    /* 5. Rename to eth0, bring up, assign IP and default gateway inside container netns */
    const char *ip = net->ip_address ? net->ip_address : "10.0.0.2/24";
    snprintf(cmd, sizeof(cmd), "nsenter -t %d -n ip link set %s name eth0", (int)child_pid, peer_veth);
    run_cmd(cmd);

    snprintf(cmd, sizeof(cmd), "nsenter -t %d -n ip link set lo up", (int)child_pid);
    run_cmd(cmd);

    snprintf(cmd, sizeof(cmd), "nsenter -t %d -n ip link set eth0 up", (int)child_pid);
    run_cmd(cmd);

    snprintf(cmd, sizeof(cmd), "nsenter -t %d -n ip addr add %s dev eth0", (int)child_pid, ip);
    run_cmd(cmd);

    if (net->gateway) {
        snprintf(cmd, sizeof(cmd), "nsenter -t %d -n ip route add default via %s", (int)child_pid, net->gateway);
        run_cmd(cmd);
    }

    return MYRUN_SUCCESS;
#else
    (void)container_id;
    (void)child_pid;
    (void)net;
    return MYRUN_SUCCESS;
#endif
}

int network_setup_dns(const char *rootfs, const char *dns_server) {
    if (!dns_server) return MYRUN_SUCCESS;

    char path[512];
    if (rootfs && strcmp(rootfs, "/") != 0) {
        snprintf(path, sizeof(path), "%s/etc/resolv.conf", rootfs);
    } else {
        snprintf(path, sizeof(path), "/etc/resolv.conf");
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        LOG_WARN("Could not write DNS server to '%s': %s", path, strerror(errno));
        return MYRUN_ERR_GENERIC;
    }

    fprintf(f, "nameserver %s\n", dns_server);
    fclose(f);
    LOG_DEBUG("Configured container DNS: nameserver %s in %s", dns_server, path);
    return MYRUN_SUCCESS;
}

int network_cleanup(const char *container_id) {
#ifdef __linux__
    if (!container_id) return MYRUN_SUCCESS;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip link delete veth_%s 2>/dev/null || true", container_id);
    run_cmd(cmd);
    return MYRUN_SUCCESS;
#else
    (void)container_id;
    return MYRUN_SUCCESS;
#endif
}
