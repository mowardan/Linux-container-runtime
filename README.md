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
|           | fork() + clone() + synchronization pipe                     |
|           v                                                             |
|  +-------------------------------------------------------------------+  |
|  |                        CONTAINER PROCESS                          |  |
|  |                                                                   |  |
|  |  +---------------------+  +--------------------+                  |  |
|  |  |   PID Namespace     |  |  Mount Namespace   |                  |  |
|  |  |  (Isolated PID 1)   |  | (pivot_root/mount) |                  |  |
|  |  +---------------------+  +--------------------+                  |  |
|  |  +---------------------+  +--------------------+                  |  |
|  |  |   UTS Namespace     |  |   User Namespace   |                  |  |
|  |  | (Hostname/NIS domain|  | (UID/GID Mapping)  |                  |  |
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
| Phase 2 | PID Namespace | Planned | `clone(CLONE_NEWPID)`, `/proc` isolation, PID 1 init semantics |
| Phase 3 | UTS Namespace | Planned | `clone(CLONE_NEWUTS)`, `sethostname()` |
| Phase 4 | Mount Namespace | Planned | `clone(CLONE_NEWNS)`, `mount(MS_PRIVATE \| MS_REC)` |
| Phase 5 | Root Filesystem | Planned | `pivot_root()`, `chroot()`, bind mounts |
| Phase 6 | Special Filesystems | Planned | `/proc`, `/sys`, `/dev`, `/dev/pts`, `/dev/shm` |
| Phase 7-8 | User Namespaces & Sync | Planned | `CLONE_NEWUSER`, `uid_map`, `gid_map`, sync protocol |
| Phase 9-10 | cgroups v2 & Stats | Planned | `/sys/fs/cgroup/` (`cpu`, `memory`, `pids`) |
| Phase 11-15 | Virtual Networking & DNS | Planned | `CLONE_NEWNET`, `veth`, bridge `myrun0`, NAT, `/etc/resolv.conf` |
| Phase 16-18 | Security Hardening | Planned | Capabilities (`libcap`), `PR_SET_NO_NEW_PRIVS`, `seccomp` |
| Phase 19-21 | Lifecycle & Exec/Inspect | Planned | `create`, `start`, `kill`, `delete`, `exec` (`setns`), `state` |
| Phase 22-25 | Images & OCI Bundles | Planned | Rootfs tar import, OverlayFS layers, OCI `config.json` |

---

## Phase 1 Implementation Details

Phase 1 provides the foundational process execution engine:
- **Fork / Exec / Wait Lifecycle**: Creates child processes via `fork()`, resolves executables across `$PATH` or direct path, and executes via `execve()`.
- **Atomic Error Synchronization**: Uses a pipe with `O_CLOEXEC` to propagate child execution failures (`ENOENT`, `EACCES`) back to the parent before `_exit()`.
- **Exit Status Decoding**: Correctly decodes `WIFEXITED`, `WEXITSTATUS`, `WIFSIGNALED`, `WTERMSIG`, and `WCOREDUMP`.
- **Signal Forwarding**: Hooks `SIGINT`, `SIGTERM`, `SIGQUIT`, and `SIGWINCH` using `sigaction` with `SA_RESTART` and forwards signals to the spawned child.
- **Safety**: Built with strict compiler flags (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2`) and validated under **AddressSanitizer** and **UndefinedBehaviorSanitizer**.

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

# Run test suite
make test

# Run test suite in clean Linux container environment (Ubuntu 24.04 + GCC + ASan)
make test-linux

# Clean build artifacts
make clean
```

---

## CLI Usage

```bash
# Show help and usage
./bin/myrun --help

# Show version
./bin/myrun --version

# Run a command
./bin/myrun run /bin/echo "Hello from MyRun!"

# Run a command with working directory override
./bin/myrun run --cwd /tmp /bin/sh -c "echo 'Working directory:' \$(pwd)"

# Run in debug mode
./bin/myrun run --debug /bin/sh -c "exit 42"
```

---

## Testing

The project includes unit and integration tests under `tests/`:

```bash
make asan test
```

Expected output:
```
=========================================
     MyRun Phase 1 Test Suite            
=========================================

  [PASS] test_process_spawn_success
  [PASS] test_process_exit_code_propagation
  [PASS] test_process_not_found
  [PASS] test_process_arguments_handling
  [PASS] test_process_working_directory
  [PASS] test_process_custom_environment
  [PASS] test_process_signal_termination
  [PASS] test_config_parsing

-----------------------------------------
Tests Passed: 8
Tests Failed: 0
-----------------------------------------
```

---

## Documentation

- [docs/architecture.md](docs/architecture.md) — Architectural overview, subsystem interaction, and OCI alignment.
- [docs/process-model.md](docs/process-model.md) — Deep-dive into Linux process creation, `fork()` vs `clone()`, `execve()`, Copy-on-Write (COW), status decoding, and signal propagation.
