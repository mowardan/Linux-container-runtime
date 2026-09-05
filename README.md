# MyRun: Educational Linux Container Runtime in C

**MyRun** is a modular, educational Linux container runtime built from scratch in **C (C11 / POSIX / Linux syscalls)**.

Inspired by the **OCI (Open Container Initiative) Runtime Specification**, **runc**, and **libcontainer**, MyRun is designed to demystify containerization by directly implementing Linux kernel primitives without relying on Docker, containerd, or existing runtime libraries.

---

## Architecture & Container Anatomy

A "container" is not an actual Linux kernel object; rather, it is a standard Linux process isolated and constrained using a combination of kernel subsystems:

```
+-------------------------------------------------------------------------+
|                                  HOST                                   |
|                                                                         |
|  +-------------------+                                                  |
|  |       myrun       |  (CLI / Runtime Supervisor)                      |
|  +--------+----------+                                                  |
|           |                                                             |
|           | clone(CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD) + sync pipe    |
|           v                                                             |
|  +-------------------------------------------------------------------+  |
|  |                        CONTAINER PROCESS                          |  |
|  |                                                                   |  |
|  |  +---------------------+  +--------------------+                  |  |
|  |  |   PID Namespace     |  |  Mount Namespace   |                  |  |
|  |  |  (Isolated PID 1)   |  | (pivot_root/mount) |                  |  |
|  |  |    [IMPLEMENTED]    |  |     [PLANNED]      |                  |  |
|  |  +---------------------+  +--------------------+                  |  |
|  |  +---------------------+  +--------------------+                  |  |
|  |  |   UTS Namespace     |  |   User Namespace   |                  |  |
|  |  | (Hostname/NIS domain|  | (UID/GID Mapping)  |                  |  |
|  |  |    [IMPLEMENTED]    |  |     [PLANNED]      |                  |  |
|  |  +---------------------+  +--------------------+                  |  |
|  |  +---------------------+  +--------------------+                  |  |
|  |  |  Network Namespace  |  |   IPC Namespace    |                  |  |
|  |  |   (veth / bridge)   |  |   (System V/POSIX) |                  |  |
|  |  +---------------------+  +--------------------+                  |  |
|  |                                                                   |  |
|  |  +-------------------------------------------------------------+  |  |
|  |  |                      Security Layer                         |  |  |
|  |  |   Capabilities Dropping | no_new_privs | Seccomp-BPF        |  |  |
|  |  +-------------------------------------------------------------+  |  |
|  +---------------------------------+---------------------------------+  |
|                                    |                                    |
|                                    v                                    |
|  +-------------------------------------------------------------------+  |
|  |                         cgroups v2                                |  |
|  |        (/sys/fs/cgroup: memory.max, cpu.max, pids.max)            |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

---

## Development Phases & Roadmap

| Phase | Subsystem | Status | Linux Primitives / Syscalls |
|---|---|---|---|
| **Phase 1** | **Process Runtime** | **Complete** | `fork()`, `execve()`, `waitpid()`, `sigaction()`, `pipe(O_CLOEXEC)` |
| **Phase 2** | **PID Namespace** | **Complete** | `clone(CLONE_NEWPID)`, dedicated `mmap` stack, PID 1 init semantics |
| **Phase 3** | **UTS Namespace** | **Complete** | `clone(CLONE_NEWUTS)`, `sethostname()`, `--hostname <name>` CLI option |
| **Phase 4** | **Mount Namespace** | **Complete** | `clone(CLONE_NEWNS)`, `mount(MS_PRIVATE \| MS_REC)` |
| Phase 5 | Root Filesystem | Planned | `pivot_root()`, `chroot()`, bind mounts |
| Phase 6 | Special Filesystems | Planned | `/proc`, `/sys`, `/dev`, `/dev/pts`, `/dev/shm` |
| Phase 7-8 | User Namespaces & Sync | Planned | `CLONE_NEWUSER`, `uid_map`, `gid_map`, sync protocol |
| Phase 9-10 | cgroups v2 & Stats | Planned | `/sys/fs/cgroup/` (`cpu`, `memory`, `pids`) |
| Phase 11-15 | Virtual Networking & DNS | Planned | `CLONE_NEWNET`, `veth`, bridge `myrun0`, NAT, `/etc/resolv.conf` |
| Phase 16-18 | Security Hardening | Planned | Capabilities (`libcap`), `PR_SET_NO_NEW_PRIVS`, `seccomp` |
| Phase 19-21 | Lifecycle & Exec/Inspect | Planned | `create`, `start`, `kill`, `delete`, `exec` (`setns`), `state` |
| Phase 22-25 | Images & OCI Bundles | Planned | Rootfs tar import, OverlayFS layers, OCI `config.json` |

---

## Building and Running

### Requirements
- GCC (>= 9) or Clang (>= 11)
- GNU Make
- Linux Kernel (>= 5.8 recommended for cgroups v2) or Docker / VM

### Build Targets

```bash
# Build standard release binary
make

# Build debug binary (-g3 -O0 -DDEBUG)
make debug

# Build with AddressSanitizer and UndefinedBehaviorSanitizer
make asan

# Run host test suites
make test

# Run all test suites inside privileged Linux container environment (Ubuntu 24.04 + GCC + ASan)
make test-linux

# Clean build artifacts
make clean
```

---

## CLI Usage

```bash
# Run a container with isolated PID and custom hostname
sudo ./bin/myrun run --hostname web /bin/sh -c 'hostname; echo "PID: $$"'
# Output:
# web
# PID: 1

# Run with custom working directory
sudo ./bin/myrun run --hostname db-box --cwd /tmp /bin/sh -c 'hostname; pwd'

# Run in debug mode
sudo ./bin/myrun run --debug --hostname app-01 /bin/echo "Hello from isolated container"
```

---

## Documentation

- [docs/architecture.md](docs/architecture.md) — Architectural overview, subsystem interaction, and OCI alignment.
- [docs/process-model.md](docs/process-model.md) — Deep-dive into Linux process creation, `fork()` vs `clone()`, `execve()`, and status decoding.
- [docs/pid-namespace.md](docs/pid-namespace.md) — Linux PID namespaces, `CLONE_NEWPID`, PID 1 init semantics, orphan reparenting, and `/proc/<pid>/status` `NSpid`.
- [docs/uts-namespace.md](docs/uts-namespace.md) — Linux UTS namespaces, `CLONE_NEWUTS`, `sethostname()`, and hostname isolation.
- [docs/mount-namespace.md](docs/mount-namespace.md) — Linux Mount namespaces, `CLONE_NEWNS`, `MS_REC | MS_PRIVATE`, and mount propagation.
