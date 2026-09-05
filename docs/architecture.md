# MyRun Architecture & System Specification

## 1. Executive Summary

**MyRun** is an educational, production-grade Linux container runtime implemented in C. Its purpose is to demonstrate the concrete mechanics of container isolation, resource containment, and security primitives as implemented in the Linux kernel.

Rather than treating containers as black-box abstractions managed by Docker or containerd, MyRun breaks down each component into explicit system calls, kernel interfaces, and POSIX semantics.

---

## 2. Layered Architecture

```
+---------------------------------------------------------------+
|                      CLI & Entry Point                        |
|                         (src/main.c)                          |
+-------------------------------+-------------------------------+
                                |
                                v
+---------------------------------------------------------------+
|                      Runtime Orchestration                    |
|                    (src/runtime/runtime.c)                    |
+-------------------------------+-------------------------------+
                                |
         +----------------------+----------------------+
         |                      |                      |
         v                      v                      v
+------------------+  +-------------------+  +------------------+
|  Process Engine  |  | Namespaces Engine |  |  Security Layer  |
| (src/process/)   |  | (src/namespaces/) |  | (src/security/)  |
+------------------+  +-------------------+  +------------------+
         |                      |                      |
         +----------------------+----------------------+
                                |
         +----------------------+----------------------+
         |                      |                      |
         v                      v                      v
+------------------+  +-------------------+  +------------------+
| Filesystem / Root|  | cgroups v2 Engine |  | Network Engine   |
| (src/filesystem/)|  | (src/cgroups/)    |  | (src/network/)   |
+------------------+  +-------------------+  +------------------+
```

---

## 3. Subsystem Breakdown

### 3.1 Process Subsystem (`src/process/`)
- Manages process creation (`fork()`, later `clone()`).
- Coordinates binary execution (`execve()`).
- Implements synchronized error reporting via `pipe(O_CLOEXEC)`.
- Handles parent-child signal forwarding (`SIGINT`, `SIGTERM`, `SIGQUIT`, `SIGWINCH`).
- Collects and decodes termination status via `waitpid()`.

### 3.2 Namespaces Subsystem (`src/namespaces/`)
- **PID Namespace (`CLONE_NEWPID`)** *(Implemented)*: Isolates the process ID space. The container's primary process becomes PID 1.
- **UTS Namespace (`CLONE_NEWUTS`)** *(Implemented)*: Isolates hostname and NIS domain name.
- **Mount Namespace (`CLONE_NEWNS`)** *(Planned)*: Isolates filesystem mount tables and mount propagation trees.
- **User Namespace (`CLONE_NEWUSER`)**: Maps unprivileged host UIDs to container UID 0 (root).
- **Network Namespace (`CLONE_NEWNET`)**: Provides isolated network stack, routing tables, and interface lists.
- **IPC Namespace (`CLONE_NEWIPC`)**: Isolates System V IPC and POSIX message queues.

### 3.3 Filesystem Subsystem (`src/filesystem/`) - *Planned*
- Mounts and remounts root filesystem with `pivot_root()`.
- Creates synthetic and pseudo-filesystems: `/proc`, `/sys`, `/dev`, `/dev/pts`, `/dev/shm`.
- Applies bind mounts and enforces read-only root filesystems where configured.

### 3.4 Resource Subsystem (`src/cgroups/`) - *Planned*
- Interacts with Linux cgroups v2 (`/sys/fs/cgroup/`).
- Enforces memory constraints (`memory.max`, `memory.high`), CPU shares/quotas (`cpu.max`), and maximum process counts (`pids.max`).
- Reads usage metrics for `myrun stats`.

### 3.5 Security Subsystem (`src/security/`) - *Planned*
- Drops unnecessary POSIX capabilities (`cap_set_proc()`).
- Enforces `PR_SET_NO_NEW_PRIVS` via `prctl()`.
- Applies Seccomp-BPF filters to prevent unauthorized syscall execution.

---

## 4. Container Startup Lifecycle

```
[CLI Invocation] (myrun run ...)
      |
      v
1. Parse arguments & validate configuration
      |
      v
2. Create sync pipe (pipe with O_CLOEXEC)
      |
      v
3. Fork / Clone child with requested namespace flags
      |
      +-----------------------------------------+
      | (Parent Branch)                         | (Child Branch)
      v                                         v
4. Wait on sync pipe for child status     4. (In Phase 2+) Setup namespace internals
                                          5. (In Phase 5+) pivot_root to new rootfs
                                          6. (In Phase 9+) Join cgroups
                                          7. (In Phase 16+) Drop capabilities & no_new_privs
                                          8. Call execve()
                                                |
                                                v
                                          (If execve succeeds, pipe auto-closes via CLOEXEC)
                                          (If execve fails, child writes errno to pipe & exits)
      |                                         |
      +-----------------------------------------+
      |
      v
5. Parent detects pipe EOF (success) or reads errno (failure)
      |
      v
6. Parent registers signal handlers (SIGINT, SIGTERM forwarding)
      |
      v
7. Parent waits via waitpid()
      |
      v
8. Parent restores signal handlers, unlinks resources, and propagates child exit code
```
