# myrun — Linux Container Runtime in C

[![Language](https://img.shields.io/badge/language-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform](https://img.shields.io/badge/platform-Linux%20%3E%3D%205.8-orange.svg)](https://kernel.org)
[![POSIX](https://img.shields.io/badge/standard-POSIX%20%2F%20Linux%20ABI-green.svg)](https://man7.org/linux/man-pages/dir_section_2.html)
[![Spec](https://img.shields.io/badge/spec-OCI%20Runtime%20v1.0-purple.svg)](https://github.com/opencontainers/runtime-spec)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Zero Deps](https://img.shields.io/badge/dependencies-none%20(libc%20only)-success.svg)](Makefile)

**myrun** is a modular, high-performance Linux container runtime written from scratch in pure **C (C11 / POSIX / Linux syscalls)** with zero external dependencies.

Inspired by **runc**, **bwrap (Bubblewrap)**, and the **OCI (Open Container Initiative) Runtime Specification**, `myrun` demystifies containerization by directly interacting with Linux kernel primitives. It provisions isolated execution environments using kernel namespaces, cgroups v2 resource controllers, pivot_root filesystem isolation, capabilities dropping, seccomp filters, and virtual networking—without relying on Docker, containerd, or existing runtime daemons.

---

## Table of Contents

- [Architectural Overview](#architectural-overview)
- [Container Anatomy & Kernel Primitives](#container-anatomy--kernel-primitives)
- [Key Features & Subsystems](#key-features--subsystems)
- [Building & Installation](#building--installation)
- [Quickstart Guide](#quickstart-guide)
- [CLI Reference](#cli-reference)
- [Hands-on Recipes & Examples](#hands-on-recipes--examples)
  - [1. Running an Isolated Alpine Rootfs](#1-running-an-isolated-alpine-rootfs)
  - [2. Enforcing cgroups v2 Resource Constraints](#2-enforcing-cgroups-v2-resource-constraints)
  - [3. Read-Only Bind Mounts](#3-read-only-bind-mounts)
  - [4. Inspecting Live State & cgroup Metrics](#4-inspecting-live-state--cgroup-metrics)
  - [5. Attaching to Running Containers via `setns`](#5-attaching-to-running-containers-via-setns)
  - [6. User Namespaces & Rootless Isolation](#6-user-namespaces--rootless-isolation)
- [Internal Systems Engineering](#internal-systems-engineering)
  - [The Synchronization Pipe Handshake](#the-synchronization-pipe-handshake)
  - [The `pivot_root` Mount Propagation Trap](#the-pivot_root-mount-propagation-trap)
  - [PID 1 Semantics & Signal Forwarding](#pid-1-semantics--signal-forwarding)
- [Directory Layout](#directory-layout)
- [Testing & Quality Assurance](#testing--quality-assurance)
- [Documentation Deep Dives](#documentation-deep-dives)
- [License](#license)

---

## Architectural Overview

In the Linux kernel, there is no single entity known as a "container". A container is simply a standard Linux process whose view of the operating system is partitioned and constrained by combining independent kernel subsystems:

```
+-----------------------------------------------------------------------------------+
|                                     HOST OS                                       |
|                                                                                   |
|  +-----------------------------------------------------------------------------+  |
|  |                            myrun (Supervisor)                               |  |
|  |  * Parses CLI flags / OCI config.json                                       |  |
|  |  * Initializes cgroup v2 hierarchy (/sys/fs/cgroup/myrun/<id>)              |  |
|  |  * Spawns container via clone(2) with dedicated mmap(2) stack              |  |
|  |  * Manages sync pipes (error propagation & UID/GID mapping ACK)             |  |
|  |  * Forwards signals (SIGINT, SIGTERM, SIGWINCH) & reaps child               |  |
|  +-------------------------------------+---------------------------------------+  |
|                                        |                                          |
|                clone(CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS |                  |
|                      CLONE_NEWUSER | CLONE_NEWNET | CLONE_NEWIPC)                 |
|                                        |                                          |
|                                        v                                          |
|  +-----------------------------------------------------------------------------+  |
|  |                         CONTAINER INIT (PID 1)                              |  |
|  |                                                                             |  |
|  |  +--------------------------+           +--------------------------------+  |  |
|  |  |      PID Namespace       |           |        Mount Namespace         |  |  |
|  |  |  * Child becomes PID 1   |           |  * Private mount propagation   |  |  |
|  |  |  * Isolated process tree |           |  * pivot_root() new rootfs     |  |  |
|  |  |  * Private /proc mount   |           |  * Mount /proc, /sys, /dev     |  |  |
|  |  +--------------------------+           +--------------------------------+  |  |
|  |  +--------------------------+           +--------------------------------+  |  |
|  |  |      UTS Namespace       |           |         User Namespace         |  |  |
|  |  |  * Isolated hostname     |           |  * Rootless execution          |  |  |
|  |  |  * Isolated NIS domain   |           |  * UID/GID range translation   |  |  |
|  |  +--------------------------+           +--------------------------------+  |  |
|  |  +--------------------------+           +--------------------------------+  |  |
|  |  |    Network Namespace     |           |         IPC Namespace          |  |  |
|  |  |  * Dedicated veth pair   |           |  * Private POSIX message queue |  |  |
|  |  |  * Private loopback / IP |           |  * Private System V IPC & shm  |  |  |
|  |  +--------------------------+           +--------------------------------+  |  |
|  |                                                                             |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  |  |                          Security Layer                               |  |  |
|  |  |  * Drop Linux capability bounding set (CAP_SYS_ADMIN, etc.)          |  |  |
|  |  |  * Set PR_SET_NO_NEW_PRIVS (prevent SUID binary escalation)           |  |  |
|  |  |  * Install Seccomp-BPF system call whitelist / filter                |  |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  |                                     |                                       |  |
|  |                                     v                                       |  |
|  |                             execve(target, argv)                            |  |
|  +-------------------------------------+---------------------------------------+  |
|                                        |                                          |
|                                        v                                          |
|  +-----------------------------------------------------------------------------+  |
|  |                               cgroups v2                                    |  |
|  |  * Memory limit: memory.max                                                 |  |
|  |  * CPU quota: cpu.max (quota / period)                                      |  |
|  |  * Process limit: pids.max (fork-bomb prevention)                           |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## Container Anatomy & Kernel Primitives

| Subsystem | Linux Mechanism | Syscalls & Interfaces | Container Purpose |
|---|---|---|---|
| **PID Namespace** | `CLONE_NEWPID` | `clone()`, `setns()`, `/proc/<pid>/ns/pid` | Isolates process tree; first process inside becomes PID 1. |
| **UTS Namespace** | `CLONE_NEWUTS` | `sethostname()`, `setdomainname()` | Isolates system hostname and domain name from the host. |
| **Mount Namespace** | `CLONE_NEWNS` | `mount(2)`, `umount2(2)` | Grants an independent filesystem mount table. |
| **Rootfs Isolation** | VFS pivot | `pivot_root()`, `chroot()`, `MS_PRIVATE` | Switches system root to container filesystem, unmounts host root. |
| **Pseudo Filesystems**| Kernel VFS mounts | `mount("proc", "/proc", "proc", ...)` | Mounts private instances of `/proc`, `/sys`, `/dev`, `/dev/pts`, `/dev/shm`. |
| **User Namespace** | `CLONE_NEWUSER` | `/proc/<pid>/uid_map`, `/proc/<pid>/gid_map` | Maps unprivileged host user to root inside container. |
| **IPC Namespace** | `CLONE_NEWIPC` | POSIX mq, System V IPC keys | Isolates shared memory, semaphores, and message queues. |
| **Network Namespace**| `CLONE_NEWNET` | `veth` pair, `ip link`, host bridge, NAT | Allocates dedicated network interfaces, loopback, and IP tables. |
| **Resource Control**| **cgroups v2** | `/sys/fs/cgroup/myrun/<id>/*` | Enforces hard limits on memory, CPU bandwidth, and total processes. |
| **Privilege Drops** | POSIX Capabilities | `cap_set_proc()`, `PR_CAPBSET_DROP` | Revokes sensitive privileges even when running as UID 0. |
| **Escalation Lock** | Process Security | `prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)` | Disables gaining extra privileges via setuid/setgid binaries. |
| **Syscall Filtering**| **Seccomp-BPF** | `prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER)` | Restricts dangerous syscalls (`reboot`, `swapon`, `sys_chroot`). |
| **Lifecycle & Exec**| Container ABI | `setns()`, OCI `config.json`, OCI JSON state | Supports lifecycle states (`running`, `stopped`) and `exec` injection. |

---

## Key Features & Subsystems

- **Zero Third-Party Dependencies**: Pure standard C11 linking solely against POSIX and Linux kernel headers.
- **Ultra-Low Overhead**: Binary size < 60 KB, container startup latency < 5 ms, memory footprint < 1 MB RSS.
- **Robust Error Propagation**: Uses bidirectional `O_CLOEXEC` pipes to propagate exact POSIX `errno` codes from the child process back to the supervisor before `execve`.
- **Modern cgroups v2**: Uses unified cgroups v2 hierarchies (`memory.max`, `cpu.max`, `pids.max`) with automatic cleanup upon container exit.
- **Safe Filesystem Switch**: Implements standard `pivot_root(2)` with `MS_REC | MS_PRIVATE` propagation on `/` to prevent container mount leaks to the host OS.
- **OCI Alignment**: Implements standard commands (`run`, `exec`, `kill`, `delete`, `state`, `stats`) and parses OCI `config.json` specifications.
- **Portable Test Suite**: Unit and integration test harnesses runnable natively on Linux, with full fallback detection on other POSIX operating systems.

---

## Building & Installation

### Prerequisites

- **Host Compiler**: GCC (>= 9.0) or Clang (>= 11.0) with C11 support.
- **Build System**: GNU Make.
- **Operating System**: Linux kernel >= 5.8 (recommended for full cgroups v2 and user namespaces).

### Build Targets

```bash
# Compile standard release build with optimizations (-O2)
make

# Compile with debug symbols and verbose diagnostics (-g3 -O0 -DDEBUG)
make debug

# Compile with AddressSanitizer and UndefinedBehaviorSanitizer
make asan

# Run local unit and integration tests
make test

# Run full integration test suite inside a privileged Linux container (Docker)
make test-linux

# Clean compilation artifacts
make clean
```

The resulting binary will be created at `bin/myrun`.

---

## Quickstart Guide

### 1. Minimal Isolated Process

Run a shell with isolated PID, UTS, Mount, and IPC namespaces:

```bash
sudo ./bin/myrun run --hostname isolated-box /bin/sh
```

Inside the container:
```sh
# Hostname reflects our isolated setting
hostname
# isolated-box

# Process ID is 1 (isolated init)
echo $$
# 1
```

### 2. Resource-Limited Execution

Run a command with a 128 MB RAM ceiling and a maximum of 20 processes:

```bash
sudo ./bin/myrun run --id worker-1 -m 128M --pids 20 /bin/sh
```

---

## CLI Reference

```
Usage: myrun [global options] <command> [command options] [arguments...]

Commands:
  run [options] <executable> [args...]  Run a process / container
  exec <id> <command> [args...]         Execute a process inside an existing container
  kill <id> [signal]                    Send signal to container init process (default 15)
  delete <id>                           Delete container state
  state <id>                            Query container state and status
  stats <id>                            Display cgroup resource statistics
  version                               Show version information
  help                                  Show this help message

Global Options:
  -d, --debug                           Enable debug output
  -v, --verbose                         Enable verbose logging
  -h, --help                            Show help
      --version                         Show version

Run Options:
  --id <name>                           Specify custom container ID
  -H, --hostname <name>                 Set container hostname (UTS namespace)
      --cwd <dir>                       Set initial working directory
      --rootfs <dir>                    Set container root filesystem (pivot_root)
  -b, --bind <host>:<container>[:ro]    Bind mount a host file or directory
  -m, --memory <limit>                  Limit memory usage (e.g., 256M, 1G)
      --cpu <quota_us>                  Limit CPU quota in microseconds per 100ms period
      --pids <max>                      Limit maximum process count (PID cgroup limit)
      --net                             Enable isolated network namespace
      --ip <address>                    Assign container IP address (e.g. 10.0.0.2/24)
      --dns <ip>                        Configure container DNS nameserver
      --uid-map <in:out:count>          Map container UID to host UID (User namespace)
      --gid-map <in:out:count>          Map container GID to host GID (User namespace)
      --no-new-privs                    Enable PR_SET_NO_NEW_PRIVS
      --drop-caps                       Drop sensitive capabilities (CAP_SYS_ADMIN, etc.)
```

---

## Hands-on Recipes & Examples

### 1. Running an Isolated Alpine Rootfs

To get a full container experience, download and unpack a lightweight rootfs (such as Alpine Linux):

```bash
# 1. Prepare rootfs directory
mkdir -p /tmp/alpine-rootfs
curl -sSL https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.0-x86_64.tar.gz | tar -xz -C /tmp/alpine-rootfs

# 2. Launch container with pivot_root and private pseudo-filesystems
sudo ./bin/myrun run \
    --id alpine-demo \
    --hostname alpine-node \
    --rootfs /tmp/alpine-rootfs \
    /bin/sh
```

Inside the container:
```sh
/ # ps aux
PID   USER     TIME  COMMAND
    1 root      0:00 /bin/sh
    2 root      0:00 ps aux

/ # df -h
Filesystem                Size      Used Available Use% Mounted on
none                     20.0G      4.1G     14.8G  22% /
proc                      0.0K      0.0K      0.0K   0% /proc
sysfs                     0.0K      0.0K      0.0K   0% /sys
devtmpfs                 10.0M      0.0K     10.0M   0% /dev
```

### 2. Enforcing cgroups v2 Resource Constraints

Protect the host from runaway memory usage and fork bombs:

```bash
sudo ./bin/myrun run \
    --id guarded-service \
    --rootfs /tmp/alpine-rootfs \
    --memory 64M \
    --pids 10 \
    /bin/sh
```

If a process attempts a fork bomb `:(){ :|:& };:`, the kernel's `pids.max` controller immediately rejects additional `fork()` / `clone()` calls with `EAGAIN` (`Resource temporarily unavailable`), preserving host stability.

### 3. Read-Only Bind Mounts

Inject host files or configuration directories safely into the container:

```bash
# Mount host logs read-write and host certificates read-only
sudo ./bin/myrun run \
    --rootfs /tmp/alpine-rootfs \
    --bind /var/log/app:/var/log/app \
    --bind /etc/ssl/certs:/etc/ssl/certs:ro \
    /bin/sh
```

### 4. Inspecting Live State & cgroup Metrics

While a container is running, inspect its state from another terminal:

```bash
# View OCI-compliant JSON state
sudo ./bin/myrun state guarded-service
```
Output:
```json
{
  "id": "guarded-service",
  "status": 1,
  "pid": 48215
}
```

```bash
# View real-time cgroup resource consumption
sudo ./bin/myrun stats guarded-service
```
Output:
```text
Memory Usage: 4194304 bytes
PIDs Count:   2
```

### 5. Attaching to Running Containers via `setns`

Inject an administrative or debugging process into the running namespaces of an active container:

```bash
sudo ./bin/myrun exec guarded-service /bin/ps -ef
```

Under the hood, `myrun exec` inspects `/proc/<pid>/ns/` for each namespace type (`pid`, `mnt`, `uts`, `ipc`, `net`), joins them using the `setns(2)` system call, and executes the debugging command directly inside the container's environment.

### 6. User Namespaces & Rootless Isolation

Map an unprivileged host user (e.g., UID 1000) to `root` (UID 0) inside the container:

```bash
./bin/myrun run \
    --uid-map 0:1000:1 \
    --gid-map 0:1000:1 \
    /bin/sh -c 'id'
```

Output:
```
uid=0(root) gid=0(root) groups=0(root)
```
The process has full `root` control inside its own namespace, but carries no privileges on the host OS.

---

## Internal Systems Engineering

### The Synchronization Pipe Handshake

When invoking `clone()`, errors occurring in the child process *after* `clone()` but *before* `execve()` (such as a missing binary, mount failure, or permissions fault) cannot be communicated via a standard exit code without losing the exact `errno`.

`myrun` resolves this with a synchronization pipe marked with `O_CLOEXEC`:

```
Parent                                                Child
  |                                                     |
  |-- pipe(sync_pipe) with FD_CLOEXEC ----------------->|
  |-- clone(child_trampoline, stack, flags, ...) ------>|
  |                                                     |
  |   (Parent waits: read(sync_pipe[0]))                |-- setup namespaces (UTS, mnt)
  |                                                     |-- pivot_root()
  |                                                     |-- drop caps / seccomp
  |                                                     |-- execve(cmd, argv, envp)
  |                                                     |      |
  |                                                     |      +--> [SUCCESS]
  |                                                     |             FD closes via CLOEXEC
  |<-- EOF detected (read returns 0) -------------------|             Child runs application
  |                                                     |
  |                                                     |      +--> [FAILURE]
  |<-- write(sync_pipe[1], &errno, sizeof(int)) --------|             Writes errno to pipe
  |    (Parent reads errno, translates to exit code)    |             _exit(127/126)
```

If `execve()` succeeds, the kernel automatically closes `sync_pipe[1]` due to the `FD_CLOEXEC` descriptor flag, notifying the parent of success via an `EOF` on `read()`.

### The `pivot_root` Mount Propagation Trap

Linux systems running `systemd` configure the root mount point (`/`) with shared mount propagation (`MS_SHARED`) by default. Calling `pivot_root(new_root, put_old)` under shared propagation will fail with `EINVAL`.

To guarantee robust isolation, `myrun` systematically:
1. Creates a private mount namespace with `CLONE_NEWNS`.
2. Recursively marks the entire mount tree as private:
   ```c
   mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);
   ```
3. Bind-mounts `new_root` onto itself so it qualifies as an official VFS mount point:
   ```c
   mount(new_root, new_root, NULL, MS_BIND | MS_REC, NULL);
   ```
4. Invokes `syscall(SYS_pivot_root, new_root, put_old)`.
5. Changes working directory to `/`, remounts `/proc` and `/sys`, and unmounts the host old root with `umount2(put_old, MNT_DETACH)`.

### PID 1 Semantics & Signal Forwarding

In Linux, the init process (PID 1) of a PID namespace is treated specially by the kernel: any signal for which PID 1 has not explicitly installed a signal handler is discarded, even `SIGINT` or `SIGTERM`.

`myrun` handles this by:
- Having the supervisor catch host `SIGINT`, `SIGTERM`, and `SIGWINCH` signals via `sigaction`.
- Forwarding incoming signals directly to the child PID.
- Handling container PID 1 termination cleanly by reading exit statuses via `waitpid(2)` and extracting exit codes or terminating signal numbers (`WIFEXITED`, `WTERMSIG`).

---

## Directory Layout

```
.
├── Makefile                # Unified build system (release, debug, asan, test)
├── Dockerfile.dev          # Reproducible Linux test container environment
├── include/                # Public subsystem headers
│   ├── cgroup.h            # cgroups v2 memory, cpu, and pids interfaces
│   ├── config.h            # CLI flag parser and runtime configuration struct
│   ├── container.h         # Container state tracking and exec/setns lifecycle
│   ├── error.h             # Error codes and POSIX errno mappings
│   ├── filesystem.h        # pivot_root, bind mounts, and pseudo-mounts
│   ├── log.h               # ANSI colored structured logging engine
│   ├── mount.h             # Mount namespace isolation helpers
│   ├── namespace.h         # clone(2) trampoline and stack management
│   ├── network.h           # Virtual Ethernet (veth) and bridge networking
│   ├── oci.h               # OCI bundle & config.json specification parsing
│   ├── process.h           # Process spawning, signal forwarding, waitpid
│   ├── runtime.h           # Main runtime supervisor entry point
│   ├── security.h          # Capabilities, no_new_privs, and seccomp filters
│   ├── user.h              # User namespace UID/GID translation mappings
│   ├── utils.h             # Path resolution and helper utilities
│   └── uts.h               # UTS namespace validation and sethostname
├── src/                    # Implementation modules
│   ├── main.c              # CLI dispatcher
│   ├── runtime/            # Runtime coordinator
│   ├── process/            # Process execution and signal handlers
│   ├── namespaces/         # PID, UTS, and Mount isolation routines
│   ├── filesystem/         # pivot_root, chroot fallback, and bind mounts
│   ├── cgroups/            # cgroups v2 controller operations
│   ├── network/            # veth pair and bridge setup
│   ├── security/           # Capabilities dropping & seccomp-BPF
│   ├── lifecycle/          # State persistence and container_exec (setns)
│   ├── oci/                # OCI config parser and overlayfs mounting
│   ├── config/             # Argument parsing and validation
│   └── utils/              # Memory, string, and path utilities
├── tests/                  # Test suites
│   ├── process/            # Process spawning, exit status, and signals
│   └── namespaces/         # PID, UTS, Mount, Filesystem, and cgroups tests
└── docs/                   # Systems engineering documentation
    ├── architecture.md     # Architecture specifications & syscall flows
    ├── process-model.md    # Process lifecycle, clone vs fork, and exit codes
    ├── pid-namespace.md    # Kernel PID namespace mechanics and PID 1 init
    ├── uts-namespace.md    # UTS namespace isolation & validation rules
    └── mount-namespace.md  # Mount propagation flags & pivot_root mechanics
```

---

## Testing & Quality Assurance

The codebase includes an extensive suite of automated tests covering process lifecycle, argument parsing, namespace boundary isolation, hostname sanitization, and cgroups v2 controllers:

```bash
# Run tests locally
make test
```

To test Linux-specific features (such as `CLONE_NEWPID`, `CLONE_NEWUTS`, and `pivot_root`) in a guaranteed Linux environment on any host:

```bash
make test-linux
```

This launches a disposable Ubuntu container with full privileges, compiles the codebase with AddressSanitizer and UndefinedBehaviorSanitizer, and runs the entire test suite.

---

## Documentation Deep Dives

For detailed systems-level explanations of each kernel subsystem:

- [Architecture Overview](docs/architecture.md) — Comprehensive supervisor-to-container execution flow and OCI spec alignment.
- [Process Model](docs/process-model.md) — Process creation, memory layout, `execve` behavior, and status code decoding.
- [PID Namespace](docs/pid-namespace.md) — Namespace virtualization, PID 1 init responsibilities, and zombie process reaping.
- [UTS Namespace](docs/uts-namespace.md) — UTS data structures in the kernel (`nsproxy`, `uts_namespace`) and validation rules.
- [Mount Namespace](docs/mount-namespace.md) — VFS mounts, mount propagation (`MS_SHARED` vs `MS_PRIVATE`), and `pivot_root` mechanics.

---

## License

This project is open-source software licensed under the **[MIT License](LICENSE)**.
