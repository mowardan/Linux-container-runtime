# Linux Mount Namespaces & Propagation Isolation

This document details the kernel mechanics, system calls, mount propagation semantics, and implementation architecture of **Mount Namespace Isolation** implemented in **Phase 4** of **MyRun**.

---

## 1. What is a Mount Namespace?

The **Mount Namespace** was the very first namespace introduced in the Linux kernel (Linux 2.4.19 in 2002, hence the flag name `CLONE_NEWNS` meaning "New Namespace").

It isolates the list of filesystem mount points seen by a process:
- Each process belongs to a `struct mnt_namespace` kernel object.
- System calls that modify mount points—such as `mount()`, `umount()`, `pivot_root()`, and `umount2()`—only modify the caller's private mount table once properly isolated.

```text
Host System (Root Mount Namespace)
├── / (rootfs, ext4/xfs, MS_SHARED)
├── /proc (procfs)
└── /sys (sysfs)

MyRun Container (Isolated Mount Namespace via CLONE_NEWNS + MS_PRIVATE)
├── / (recursively MS_PRIVATE)
├── /tmp/myrun_tmpfs (private tmpfs mount, completely invisible to host)
└── ... (future pivot_root in Phase 5)
```

---

## 2. Mount Propagation Mechanics

In modern Linux distributions (systemd, Debian, Ubuntu, RHEL), the root mount (`/`) and all storage mounts are configured as **Shared (`MS_SHARED`)**.

### The Shared Propagation Problem
When a process invokes `clone(..., CLONE_NEWNS, ...)`:
- The child process receives a copy of the mount table.
- **However**, if mounts remain in a shared peer group (`MS_SHARED`), mount and unmount events performed inside the container will **propagate back to the host system**!

### The Solution: `MS_REC | MS_PRIVATE`
To ensure that container mounts never bleed into the host or other containers, MyRun immediately remounts the root mount point hierarchy as **Private**:

```c
mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);
```

- `MS_REC`: Applies the flag recursively to `/` and every submount currently attached.
- `MS_PRIVATE`: Converts the mounts into private mounts, detaching them from any shared peer group.

Any subsequent mount (e.g. `tmpfs`, bind mounts, or remounting `/proc`) remains strictly isolated within this container.

---

## 3. Mount Propagation Types Summary

| Propagation Type | Flag | Behavior |
|---|---|---|
| **Shared** | `MS_SHARED` | Mount/unmount events propagate to and from other peer mounts in the group. |
| **Private** | `MS_PRIVATE` | Mount/unmount events do **not** propagate to or from any other mount. *(MyRun standard)* |
| **Slave** | `MS_SLAVE` | Receives propagation from master mounts, but changes do not propagate back. |
| **Unbindable**| `MS_UNBINDABLE` | Private mount that cannot be duplicated using bind mounts. |

---

## 4. Kernel Architecture & Lifecycle

```text
[CLI Invocation] (myrun run ...)
       │
       ▼
[Parent: myrun]
       │
       ├─ clone(child_trampoline, stack_top, CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD)
       │
       └───> [Child Process]
               │
               ├─ 1. Enter PID Namespace (Child = PID 1)
               ├─ 2. Enter UTS Namespace (Child nodename = configured hostname)
               ├─ 3. Enter Mount Namespace (Attached to private mnt_namespace)
               ├─ 4. mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL)
               ├─ 5. sethostname(hostname, len)
               └─ 6. execve(command, argv, envp)
```

---

## 5. Kernel Namespace Inspection via `/proc`

The mount namespace can be verified through `/proc`:

```bash
# Check container's mount namespace inode
readlink /proc/self/ns/mnt
# Example: mnt:[4026532585]

# Compare with host
readlink /proc/1/ns/mnt
# Example: mnt:[4026531840]
```

Differing inode numbers confirm that the process operates inside a distinct mount namespace.

---

## 6. Security Limitations of Phase 4

> [!WARNING]
> **Mount Namespace Isolation is NOT a Complete Root Filesystem Sandbox.**
>
> While Phase 4 isolates the mount table:
> - The container process currently still sees the host filesystem hierarchy (`/usr`, `/etc`, `/home`, etc.) because `pivot_root` has not yet swapped the root mount.
> - **Phase 5 (Root Filesystem)** will implement `pivot_root()` and bind mounts to restrict the container to a dedicated rootfs.
> - **Phase 6 (Special Filesystems)** will remount private `/proc`, `/sys`, `/dev`, and `/dev/pts`.
