# Linux PID Namespaces & Container Process Isolation

This document details the theory, Linux kernel mechanics, system calls, and implementation specifics of **PID Namespace Isolation** implemented in **Phase 2** of **MyRun**.

---

## 1. What is a PID Namespace?

A **PID Namespace** isolates the Process ID number space. Processes in different PID namespaces can have the same PID.

In Linux:
- The first PID namespace created at kernel boot is the **root/initial PID namespace** (`init_pid_ns`).
- When a process calls `clone()` with `CLONE_NEWPID` or `unshare(CLONE_NEWPID)`, a child PID namespace is created.
- The first process created inside a new PID namespace is assigned **PID 1** within that namespace, while retaining a regular, distinct PID in parent (ancestor) namespaces.

```text
Host (Root PID Namespace)
│
├── PID 1 (systemd / host init)
│
└── PID 18472 (myrun runtime supervisor)
      │
      │ clone(CLONE_NEWPID | SIGCHLD)
      ▼
   Container Process
      ├── Host PID:      18473
      └── Container PID: 1 (in new PID namespace)
```

---

## 2. Kernel Primitives: `clone()`, `unshare()`, and `setns()`

### 2.1 `clone()` with `CLONE_NEWPID`
```c
int clone(int (*fn)(void *), void *stack, int flags, void *arg);
```
- `CLONE_NEWPID`: Instructs the kernel to spawn the child process in a new PID namespace.
- `SIGCHLD`: Tells the kernel to send a `SIGCHLD` signal to the parent when the child terminates so `waitpid()` can reap it.
- `stack`: Dedicated memory buffer allocated by the parent where the child's stack pointer begins.

> [!NOTE]
> `clone()` requires `CAP_SYS_ADMIN` in the parent namespace (or an unprivileged user namespace, introduced in Phase 7).

### 2.2 `unshare(CLONE_NEWPID)` vs `clone(CLONE_NEWPID)`
- When a process calls `unshare(CLONE_NEWPID)`, the calling process does **not** move into the new PID namespace. Instead, subsequent child processes spawned by `fork()` or `clone()` will be placed in the new PID namespace.
- `clone(CLONE_NEWPID)` creates the child directly inside the new PID namespace in a single atomic syscall.

### 2.3 `setns(int fd, int nstype)`
- Allows a process to attach itself to an existing namespace via an open file descriptor to `/proc/<pid>/ns/pid`. (Used in Phase 20 for `myrun exec`).

---

## 3. Why PID 1 is Special (Init Process Semantics)

The first process inside a PID namespace becomes **PID 1** (the init process of that namespace). PID 1 has unique responsibilities and kernel behaviors:

### 3.1 Orphan Adoption
When any process inside a PID namespace becomes orphaned (its parent terminates before it), the Linux kernel automatically reparents the orphan to the namespace's **PID 1**, not host PID 1.

### 3.2 Signal Immunity
By default, the Linux kernel ignores fatal signals (like `SIGINT` or `SIGTERM`) sent to PID 1 unless PID 1 explicitly registers a signal handler for them. `SIGKILL` and `SIGSTOP` sent from *within* the same namespace are ignored by PID 1. However, `SIGKILL` sent from an *ancestor* (parent) namespace will terminate PID 1.

### 3.3 Namespace Termination
If PID 1 inside a PID namespace terminates for any reason, the kernel automatically sends `SIGKILL` to all remaining processes in that PID namespace and destroys the namespace.

---

## 4. Host PID vs Container PID: Kernel `/proc` Verification

The Linux kernel tracks the nested hierarchy of PIDs in `struct pid` and exposes them in `/proc/<pid>/status` under the `NSpid` field:

```text
$ cat /proc/18473/status | grep NSpid
NSpid:  18473   1
```
- The first number (`18473`) is the PID in the host root namespace.
- The second number (`1`) is the PID in the child container namespace.

---

## 5. Memory & Stack Management for `clone()`

Unlike `fork()` which duplicates the parent's stack via Copy-On-Write, `clone()` requires an explicit stack memory region allocated in the parent:

```c
void *stack_base = mmap(NULL, MYRUN_CHILD_STACK_SIZE,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                        -1, 0);
```

### Critical Rules:
1. **Stack Direction**: On x86_64 and ARM64, the call stack grows downward. Therefore, `clone()` must be passed the top of the allocated memory block:
   ```c
   void *stack_top = (char *)stack_base + MYRUN_CHILD_STACK_SIZE;
   ```
2. **Stack Lifetime**: The allocated stack must **never** be freed (`munmap`) while the child process is executing. MyRun frees the stack only after `process_wait()` confirms the child has terminated and been reaped.

---

## 6. Security Limitations of Phase 2

> [!WARNING]
> **PID Isolation is NOT a Complete Container Security Boundary.**
> 
> In Phase 2:
> - The container process has its own PID space, but still shares the host mount table and root filesystem.
> - Tools like `ps` read `/proc`. Because `/proc` has not yet been remounted in an isolated mount namespace, `ps` inside the container will still see the host process table.
> - Complete isolation requires Mount Namespaces (Phase 4), isolated Rootfs (Phase 5), and mounting a new `/proc` filesystem (Phase 6).
