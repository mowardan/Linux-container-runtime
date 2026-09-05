# Linux UTS Namespaces & Hostname Isolation

This document details the kernel theory, system calls, data structures, implementation architecture, and security considerations of **UTS Namespace Isolation** implemented in **Phase 3** of **MyRun**.

---

## 1. What is a UTS Namespace?

The **UTS Namespace** (named after the historic UNIX Timesharing System) is a Linux kernel isolation primitive that encapsulates two system identifiers:
1. **Host Name** (`nodename` in `struct utsname`)
2. **NIS / YP Domain Name** (`domainname` in `struct utsname`)

### Host Impact Without UTS Isolation
In standard Linux without container isolation:
- The system hostname is global to the entire operating system kernel.
- Calling `sethostname()` changes the hostname for every process, service, daemon, logger, and network socket on the host machine.

### Isolation With `CLONE_NEWUTS`
When a process is spawned with the `CLONE_NEWUTS` flag:
- The Linux kernel instantiates a new, independent `struct uts_namespace` object.
- The new namespace inherits a snapshot copy of the parent's hostname and domain name.
- Any subsequent invocation of `sethostname()` or `setdomainname()` modifies only the calling process's private UTS namespace.
- The host system hostname and other containers' hostnames remain completely untouched.

---

## 2. Kernel Primitives & Data Structures

### 2.1 The `utsname` Structure
Defined in `<sys/utsname.h>` (and `<linux/utsname.h>` in kernel space):

```c
struct utsname {
    char sysname[65];    /* Operating system name (e.g., "Linux") */
    char nodename[65];   /* Network node / host name (e.g., "myrun", "web") */
    char release[65];    /* OS release version (e.g., "6.8.0-45-generic") */
    char version[65];    /* OS build version */
    char machine[65];    /* Hardware architecture (e.g., "x86_64", "aarch64") */
    char domainname[65]; /* NIS / YP domain name */
};
```

### 2.2 Kernel Namespace Hierarchy & Syscalls

```text
                       Kernel Task (struct task_struct)
                                     │
                             nsproxy pointer
                                     │
                        ┌────────────┴────────────┐
                        │                         │
                 pid_namespace              uts_namespace
                 (PID mapping)           (nodename/domainname)
```

1. **`clone(..., CLONE_NEWUTS | CLONE_NEWPID | SIGCHLD, ...)`**: Creates a new process and attaches it to a new `uts_namespace` and `pid_namespace`.
2. **`sethostname(const char *name, size_t len)`**: Configures the `nodename` inside the caller's active UTS namespace. Requires `CAP_SYS_ADMIN` in the namespace.
3. **`gethostname(char *name, size_t len)`**: Queries the `nodename` of the caller's active UTS namespace.
4. **`uname(struct utsname *buf)`**: Returns the complete system identity structure for the caller's UTS namespace.

---

## 3. Conceptual Architecture Diagram

```text
                         HOST
                          │
                     Host Hostname
                          │
                   "production-host"
                          │
                    ┌─────┴─────┐
                    │   MyRun   │
                    └─────┬─────┘
                          │
         clone(CLONE_NEWPID | CLONE_NEWUTS)
                          │
             ┌────────────┴────────────┐
             │                         │
       PID Namespace             UTS Namespace
             │                         │
           PID 1                 hostname="web"
             │                         │
             └────────────┬────────────┘
                          │
                   sethostname("web")
                          │
                       execve()
                          │
                       /bin/sh
```

---

## 4. Execution & Initialization Lifecycle

The child container process executes in a strictly deterministic sequence before handing control over to the user binary:

```text
[Parent: myrun]
  │
  ├─ 1. Parse & validate CLI options (--hostname <name>)
  ├─ 2. Create synchronization pipe (pipe with O_CLOEXEC)
  ├─ 3. Allocate 2MB stack via mmap()
  ├─ 4. Call clone(child_trampoline, stack_top, CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD, &args)
  │
  │───> [Child Process]
  │       │
  │       ├─ 5. Enter PID Namespace (process becomes inner PID 1)
  │       ├─ 6. Enter UTS Namespace (attached to private uts_namespace)
  │       ├─ 7. Execute direct sethostname(hostname, len) syscall
  │       │     (If failed, write errno to sync pipe & _exit(1))
  │       ├─ 8. Execute process_exec() -> execve(command, argv, envp)
  │       │     (If execve succeeds, sync pipe closes automatically via O_CLOEXEC)
  │       └─ 9. Target container program runs as PID 1 with custom hostname
  │
  ├─ 10. Parent reads sync pipe (detects EOF on success, or receives error)
  ├─ 11. Parent registers interactive signal forwarders (SIGINT, SIGTERM, SIGQUIT, SIGWINCH)
  ├─ 12. Parent blocks on waitpid(child_pid)
  └─ 13. Parent reaps child, frees stack via munmap(), and returns container exit code
```

---

## 5. Hostname Validation & Security Handling

User-supplied hostnames are treated as untrusted input:
- **No Shell Invocation**: MyRun never uses `system()`, `popen()`, or `/bin/hostname` to configure the UTS namespace. It directly issues the `sethostname()` kernel system call.
- **Validation Rules (`hostname_validate`)**:
  - Rejects `NULL` or empty strings (`""`).
  - Rejects hostnames exceeding `MYRUN_MAX_HOSTNAME_LEN` (64 characters, matching `sizeof(((struct utsname *)0)->nodename) - 1`).
  - Rejects slashes (`/`).
  - Rejects ASCII control characters (`< 32` or `127`).
  - Rejects whitespace (spaces, tabs, newlines).
  - Rejects shell metacharacters and invalid symbols (only `[a-zA-Z0-9.-_]` are allowed).

---

## 6. Kernel Namespace Inspection via `/proc`

Every Linux process exposes its associated namespaces as symlinks under `/proc/<pid>/ns/`.

Inside the container:
```bash
readlink /proc/self/ns/uts
# Example: uts:[4026532580]
```

On the host:
```bash
readlink /proc/self/ns/uts
# Example: uts:[4026531838]
```

The differing inode numbers confirm that the container process is attached to an independent kernel namespace object rather than the host's root UTS namespace.

---

## 7. Security Model & Limitations of Phase 3

> [!WARNING]
> **UTS + PID Isolation is NOT a Complete Security Boundary.**
>
> While Phase 3 achieves full hostname and PID isolation:
> - **Mount Namespace (Phase 4)**: The filesystem mount table is currently shared with the host.
> - **Root Filesystem (Phase 5)**: The container still has access to the host rootfs (`/`).
> - **Special Filesystems (Phase 6)**: `/proc` and `/sys` still reflect the host state until remounted in an isolated mount namespace.
> - **User Namespaces (Phase 7-8)**: Root in container is still root on host (requires `sudo` / `CAP_SYS_ADMIN`).
> - **Capabilities & Seccomp (Phase 16-18)**: Linux capabilities and syscall filtering have not yet been restricted.

Do not deploy untrusted workloads without subsequent isolation phases (Mount, Rootfs, User, Cgroups, Seccomp).
