# Linux Process Model & Container Execution Mechanics

This document explains the fundamental Linux kernel process model, system calls, memory architecture, and synchronization mechanisms used in **Phase 1** of **MyRun**.

---

## 1. What is a Linux Process?

In the Linux kernel, a process is represented internally by `struct task_struct` (defined in `<linux/sched.h>`). It encapsulates all execution state, including:
- **Identifier**: Process ID (`pid`), Thread Group ID (`tgid`).
- **Memory descriptor (`struct mm_struct *mm`)**: The virtual memory address space (page tables, virtual memory areas `vm_area_struct`, code, heap, stack).
- **File descriptor table (`struct files_struct *files`)**: Open file descriptors and flags.
- **Signal handling state (`struct sighand_struct *sighand`)**: Registered signal handlers and pending signals.
- **Credentials (`struct cred *cred`)**: Real/effective/saved UID/GID, capabilities (`cap_bset`, `cap_effective`, `cap_permitted`).
- **Namespaces (`struct nsproxy *nsproxy`)**: References to active PID, UTS, Mount, Net, IPC, Cgroup, and User namespaces.

In containers, isolation is achieved by decoupling and virtualizing these pointers in `task_struct`.

---

## 2. Process Creation: `fork()` vs `clone()`

### 2.1 `fork()`
`fork()` creates an exact duplicate of the calling process:
- Creates a new `task_struct`.
- Duplicates the parent's file descriptor table (with matching open file descriptions and offsets).
- Duplicates the parent's memory address space using **Copy-on-Write (COW)**.

### 2.2 Copy-On-Write (COW)
When `fork()` executes, the kernel does **not** copy physical memory pages. Instead:
1. It copies the parent's page tables to the child.
2. It marks all shared pages in both parent and child as **read-only**.
3. When either process attempts to write to a page, the CPU triggers a Page Fault (`#PF`).
4. The kernel's page fault handler allocates a new physical frame, copies the 4KB page content, updates the faulting process's page table to point to the new frame with write permissions, and resumes execution.

### 2.3 `clone()` (Upcoming in Phase 2+)
While `fork()` always duplicates everything, `clone()` allows granular control over what is shared and what is isolated via flags:
- `CLONE_NEWPID`: Create a new PID namespace.
- `CLONE_NEWNS`: Create a new Mount namespace.
- `CLONE_NEWUTS`: Create a new UTS namespace.
- `CLONE_NEWNET`: Create a new Network namespace.
- `CLONE_NEWUSER`: Create a new User namespace.
- `CLONE_VM`: Share memory (threads).
- `CLONE_FILES`: Share file descriptor table.

In Phase 1, MyRun uses standard `fork()`. In subsequent phases, `clone()` or `unshare()` is used to instantiate isolated container namespaces.

---

## 3. Binary Execution: `execve()`

`execve(const char *pathname, char *const argv[], char *const envp[])` replaces the current process image with a new executable.

### 3.1 Kernel Execution Steps:
1. **Validation & Permissions**: Checks `pathname` existence and execute permissions (`X_OK`) against credentials.
2. **Binary Header Inspection**:
   - If ELF binary (`\x7fELF`): The kernel ELF loader (`fs/binfmt_elf.c`) parses ELF program headers (`PT_LOAD`), sets up memory mappings (`.text`, `.rodata`, `.data`, `.bss`), and initializes the dynamic linker (`/lib64/ld-linux-x86-64.so.2` if dynamic).
   - If Shebang (`#!`): Parses interpreter line (e.g. `#!/bin/sh`) and invokes interpreter with the script path as argument.
3. **Memory Reset**: Unmaps the old address space (`mm_struct`) and creates a fresh virtual memory map.
4. **Stack Preparation**: Copies `argv`, `envp`, and the Auxiliary Vector (`auxv`) to the top of the new user stack.
5. **Descriptor Cleanup**: Closes any file descriptors marked with `FD_CLOEXEC` (or `O_CLOEXEC`).
6. **Instruction Pointer Set**: Sets the CPU instruction pointer (`RIP` on x86_64, `PC` on ARM64) to the binary entry point and transitions to User Mode (`Ring 3` / `EL0`).

`execve()` only returns if an error occurred.

---

## 4. Parent-Child Synchronization via `pipe(O_CLOEXEC)`

A critical challenge in container runtimes is detecting whether `execve()` succeeded in the child or failed (e.g., command not found or permission denied).

If `execve()` fails, the child returns, but the parent must know the exact `errno` to report it accurately.

### The MyRun Synchronization Protocol:
```
Parent                                       Child
  |                                            |
  +-- pipe(sync_pipe) with FD_CLOEXEC --------+
  |                                            |
  +-- fork() ----------------------------------+
  |                                            |
  | (closes write end)                         | (closes read end)
  |                                            |
  |                                            | executes execve()
  |                                            |
  |                                            +--- [If execve succeeds]:
  |                                            |    FD_CLOEXEC automatically closes
  |                                            |    write end upon image replacement.
  |                                            |
  |                                            +--- [If execve fails]:
  |                                                 writes errno into write end
  |                                                 and exits with 126/127.
  |
  +-- read(read_end, &child_errno, sizeof(int))
  |
  +--> n == 0: EOF received -> execve succeeded!
  +--> n > 0 : Error received -> parent receives child errno, reaps child, and logs error.
```

---

## 5. Process Lifecycle & Status Decoding

The parent waits for the child process using `waitpid(pid, &status, 0)`.

The 32-bit `status` integer encodes multiple termination and state-change conditions:

| Macro | Meaning | Action / Exit Code in MyRun |
|---|---|---|
| `WIFEXITED(status)` | Process terminated normally via `exit(n)` or return from `main()` | `exit_code = WEXITSTATUS(status)` (0-255) |
| `WIFSIGNALED(status)` | Process was killed by an unhandled signal | `signaled = true`, `term_signal = WTERMSIG(status)`, `exit_code = 128 + term_signal` |
| `WCOREDUMP(status)` | Process produced a core dump upon termination | `core_dumped = true` |
| `WIFSTOPPED(status)` | Process was stopped by a signal (e.g. `SIGSTOP`, `SIGTSTP`) | Handled when job control is enabled |
| `WIFCONTINUED(status)` | Process was resumed by `SIGCONT` | Handled when tracking live state |

### Standard Exit Status Conventions (POSIX / Shell):
- `0`: Success.
- `1 - 125`: Application-defined exit code.
- `126`: Command found but not executable (Permission Denied / `EACCES`).
- `127`: Command not found (`ENOENT`).
- `128 + N`: Fatal termination by signal `N` (e.g., 128 + 9 = 137 for `SIGKILL`, 128 + 15 = 143 for `SIGTERM`).

---

## 6. Signal Forwarding & Terminal Interactivity

When running interactively, the runtime supervisor must forward terminal control and interruption signals to the container child process:

- `SIGINT` (`Ctrl-C`): Intercepted by parent, forwarded to child via `kill(child_pid, SIGINT)`.
- `SIGTERM` (Termination request): Intercepted and forwarded to child so the container process can perform graceful shutdown.
- `SIGQUIT` (`Ctrl-\`): Intercepted and forwarded to child.
- `SIGWINCH` (Window size change): Forwarded to child so full-screen TUI apps (top, vim, htop) dynamically resize.

### Async-Signal Safety:
Signal handlers must only invoke functions that are reentrant and async-signal-safe according to POSIX.1-2008. In MyRun, `kill()` and volatile `sig_atomic_t` access are used exclusively inside signal handlers.
