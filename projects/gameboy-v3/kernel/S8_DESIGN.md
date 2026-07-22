# S8 design — processes, fork, execve, ELF loading

Scope for S8 (minimal, enough for a shell that forks+execs): a small fixed
process table, per-process address spaces (own L1 page table), eager-copy fork
(no COW yet), execve of a static ARM ET_EXEC ELF, wait4, and real exit teardown.
Deliberately NOT in S8: COW fork, demand paging, threads, signals delivery,
scheduling fairness (a simple runnable-list round-robin is enough).

## Process model (PCB)

```
enum pstate { UNUSED, RUNNABLE, RUNNING, ZOMBIE };

struct proc {
    int          pid;
    enum pstate  state;
    uint32_t     l1_pa;        // this process's L1 page-table (TTBR0 value)
    struct proc *parent;
    int          exit_code;    // valid when ZOMBIE, harvested by wait4
    struct regs  ctx;          // saved user register frame (see below)
};
#define NPROC 16
static struct proc proctab[NPROC];
```

`struct regs` = the full user context captured at a trap: r0-r12, user sp, user
lr, the return pc, and the spsr (user cpsr). This is the generalization of S7's
single `kern_ctx` into "N processes, each with its own saved frame."

## Address-space rule (THE load-bearing invariant)

Every process's L1 table must contain, in addition to its user mappings, **the
kernel's own mappings** (kernel .text/.data/stacks in DRAM + all MMIO), so that
when a trap switches into the kernel while TTBR0 points at that process, the
kernel code/stack/UART/GIC are still reachable. Concretely: start each new L1 as
a COPY of the kernel's base identity map (S6's table), then add user pages on top.

Split of the address space (simple, avoids kernel/user VA overlap):
- kernel + MMIO + DRAM identity: as S6 maps it (low addresses + 0x40000000..)
- user program: loaded at its ELF p_vaddr (BusyBox static is typically ~0x10000)
- user stack: high user VA, e.g. top of a chosen user region, grows down

Because BusyBox links low (~0x10000) and our kernel is at 0x41000000, user and
kernel VAs don't collide — good, no need for a TTBR0/TTBR1 split yet.

## pmm ↔ page-table consistency (the discipline from earlier)

Every physical page a process owns is tracked by BOTH pmm (allocated) and the
page table (mapped). To prevent the free-while-mapped / map-while-free bugs:
- **one path** allocates+maps (`uvm_map_page`) and one path unmaps+frees
  (`uvm_free_all` on exit walks the L2 tables, frees every user page + the L2s).
- exit/execve teardown frees the OLD address space fully before/without reusing.
- (No sharing yet, so no refcounts needed in S8; COW would add them later.)

## fork()

1. Find a free PCB. Alloc a new L1 (copy kernel base mappings in).
2. For each USER page mapped in the parent: alloc a new physical page, copy 4 KB,
   map it in the child's L1 at the same VA with the same perms. (eager copy)
3. Copy the parent's saved register frame into the child's `ctx`, then set the
   child's `ctx.r0 = 0` (child sees fork() return 0; parent gets the child pid).
4. Mark child RUNNABLE; parent's syscall returns child pid.

## execve(path, argv, envp)  [S8: path→our ramfs/embedded blob]

1. Load the ELF into a FRESH address space (build new L1, don't touch the old
   one's user pages until the new one is ready — so a failed exec can bail).
2. For each PT_LOAD: map pages at p_vaddr, copy p_filesz, zero to p_memsz (bss).
3. Build the initial user stack: argc, argv[], NULL, envp[], NULL, auxv[]
   (AT_PAGESZ, AT_RANDOM, AT_NULL at minimum — TBD by research).
4. Free the OLD address space, switch to the new L1, set the saved frame's
   pc=e_entry, sp=user_stack_top, cpsr=USR. Return into it.

## wait4() / exit()

- exit(code): mark ZOMBIE, store code, free the address space (user pages + L1),
  reparent/notify parent, switch to another RUNNABLE process (or idle).
- wait4(): parent finds a ZOMBIE child, harvests its exit code + pid, frees the
  PCB slot. (Blocking wait needs a scheduler; S8 can start with "child runs to
  completion then parent continues" if we keep it cooperative.)

## Scheduler (minimal)

A round-robin over RUNNABLE procs, entered from the syscall return path and the
timer tick. S8 can begin with the simplest thing that runs the fork+exec test:
run child to exit, then resume parent. Full preemptive scheduling is a refinement.

## S8 test (the milestone)

An initial user program (loaded like S7) calls fork(); the child calls
execve() on a SECOND embedded ELF ("hello from child"); the parent wait4()s and
prints the child's exit code. Seeing both programs' output = fork+exec+wait +
two independent address spaces all working — the machinery a shell needs.

## Open items resolved by the running verification
- ELF ET_EXEC header/phdr offsets + load procedure.
- Exact initial user stack layout + minimal auxv for static musl.
- fork return-frame details + TTBR0 switch/TLB sequence.
- L2 4KB small-page descriptor bits (separate focused workflow).
