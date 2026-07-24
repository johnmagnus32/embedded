# gameboy-v3 custom kernel

Our own kernel for the T113-S3, built from scratch. The end goal (see
[PLAN.md](PLAN.md)) is to run the existing musl BusyBox initramfs unchanged and
reach a shell. This is a long climb (stages **S5–S10**); the doc is the map.

## Status: Stage 10 ✅ (running) — real BusyBox, interactive shell, in QEMU

**The goal is reached: the unchanged musl BusyBox initramfs boots, the stock
`/init` runs, and we get an interactive `/bin/sh` prompt over the UART** — first
proven on QEMU (`-M virt -cpu cortex-a7`). This is the first time the kernel
*executes* end to end rather than just building; every layer S5–S10 is now
proven by running, not only by structural inspection. It has **not** yet run on
the physical T113.

```
===================================================
  gameboy-v3 initramfs — T113-S3 is alive.
  Linux 0.9-gv3 armv7l        <- $(uname -srm)  (command substitution over a pipe)
  cores online: 1             <- $(nproc)
===================================================

/ # echo HELLO-FROM-SHELL
HELLO-FROM-SHELL
/ # uname -a
Linux gameboy-v3 0.9-gv3 gv3kernel S10 armv7l GNU/Linux
/ # echo sub: $(echo hi | tr a-z A-Z)
sub: HI
/ # exit
[kernel] last process exited; halting.
```

### Running it in QEMU (`make BOARD=virt`)

The kernel now has a **board-abstraction seam** so the same source targets both
the T113 and QEMU's `virt` machine. `board.h` selects the GIC/timer/UART
addresses at compile time; `board_16550.c` (T113) and `board_pl011.c` (QEMU) are
the two UART back-ends behind a common `uart_hw_*` interface. `BOARD=virt` also
links at `0x40010000` (QEMU RAM base + the Linux boot-stub offset) and enables
the VFP/FPU in `_start` (hardfloat BusyBox emits `vpush`/`vmov`).

```bash
make BOARD=virt          # → build/gv3kernel.bin + build/initramfs.cpio.gz (test)
make BOARD=virt qemu      # boot the test initramfs under QEMU
# or boot the real musl-BusyBox rootfs:
qemu-system-arm -M virt -cpu cortex-a7 -m 128M -nographic -net none \
  -kernel build/gv3kernel.bin -initrd ../build/output/initramfs.cpio.gz
```

We boot the **raw `.bin`** (not the ELF): that makes QEMU run its Linux boot
stub, which loads the image at `0x40010000`, passes the DTB in `r2`, and fills
`/chosen/linux,initrd-start|end` so the kernel finds the `-initrd`. (An ELF
`-kernel` does *not* set `r2`.) The default `BOARD=t113` build is unchanged and
still targets our own bootloader on silicon.

### What it took to reach the prompt (beyond S10 step 1)

Booting the *real* BusyBox surfaced a series of ABI/behaviour gaps, each fixed
and verified by re-running:
- **`__ARM_NR_set_tls` (0xf0005)** — musl's crt sets the thread pointer via this
  ARM-private syscall; we write `TPIDRURO` (CP15 c13,c0,3).
- **128 KB user stack** — a single page wasn't enough once the shell put real
  buffers (e.g. `getcwd`) near the stack top; `map_user_stack` now maps 32 pages.
- **`#!` shebang exec** — the stock `/init` *is* a `#!/bin/sh` script, so
  `proc_spawn_elf`/`proc_execve` peek for `#!` and run the interpreter.
- **missing syscalls** found empirically (`-ENOSYS` loop): `open`(5),
  `pipe`/`pipe2`, `statx`, `sched_getaffinity`, `poll`, plus `set_tls`.
- **blocking pipe reads** — `$(...)` command substitution deadlocked-then-EOF'd
  under cooperative scheduling: the parent read the empty pipe before the writer
  child ran. A read of an empty pipe *whose write end is still open* now yields
  to a runnable process and re-runs the `svc` (same rewind trick as `wait4`);
  writer/reader open-counts are tracked at the open-file layer so EOF is correct
  across `fork`/`dup`. This is what makes `$(uname -srm)`/`$(nproc)` populate.
- **`mount`(21)/`umount2`(52) no-op stubs** — the stock `/init` mounts
  `/proc`,`/sys`,`/dev` and treats failure as noisy; we accept them as no-ops
  (the mount-point dirs already exist, empty — honest: nothing populates them).
- **`open("")` → `-ENOENT`** — POSIX says an empty path is `-ENOENT`, not the
  cwd/root dir. `cttyhack` relies on this: when it can't find a controlling tty
  it `open("")`s and, on the failure, keeps the inherited console fds instead of
  `dup2`ing a directory over stdin/out/err (which had made the shell read EOF and
  exit immediately).
- **termios `ECHO` tracking (no more double-echo)** — the console is one shared
  device with one `termios`; `TCSETS` records it and the console read path echoes
  only when `ECHO` is set. A raw-mode line editor (BusyBox) clears `ECHO` and
  echoes itself, so the kernel must not also echo.

### S10 step 1 (the foundation): memory syscalls + tty ioctls

The memory syscalls static musl needs for its allocator, and the tty `ioctl`s a
shell needs to believe it's on a terminal.

- **`mm_syscall.c`** — `mmap2` (anonymous private: `MAP_PRIVATE|MAP_ANONYMOUS`,
  `fd=-1`; the 6th arg is a **page** offset), `munmap`, `mprotect` (no-op success
  — our L2 pages are already user-RW+exec), and a real **`brk`** (returns the
  *resulting* break, never `-errno` — the contract musl relies on). Backed by a
  per-process memory map: heap grows up from the ELF top (`proc->brk`), anon
  mmap grows down from `MMAP_TOP = 0x38000000` (`proc->mmap_top`). `elf_load` now
  reports the initial break; `vm.c` gained `vm_unmap_page`.
- **tty `ioctl`s** (`fs_syscall.c`) — the load-bearing correction from the ABI
  research: musl's **`isatty()` calls `TIOCGWINSZ`, not `TCGETS`**, so that must
  succeed on the console fd (fills 24×80) for the shell to take its interactive
  path. `TCGETS` now returns a real 36-byte `struct termios` (`NCCS=19`, cooked
  mode `ICANON|ECHO`). Plus identity/pgrp stubs (`getuid`→0, `setsid`, …).

The syscall set was **measured, not guessed**: we statically disassembled the
real `busybox` and decoded every constant `r7` load (see PLAN.md → "S10
measurement"), classifying the shell-critical vs applet vs stub vs out-of-scope
groups. This step covers group A's memory piece.

**Boot handoff (also built this step).** The kernel now *consumes* the ARM-Linux
boot protocol the bootloader already emits, instead of only ever using the
embedded cpio:
- `start.S` saves `r2` (DTB physical address) + `r1` (machine type) into `.data`
  words at the very top of `_start`, before anything clobbers them; exposes
  `boot_dtb()`/`boot_machine()`.
- `kfdt.c/.h` — a from-scratch **FDT reader** (mirror of the bootloader's FDT
  *writer*): validates the blob, walks the structure block by node depth
  (matching `/chosen`, `/memory` — ignoring `@unit` suffixes), and extracts the
  initramfs location from `/chosen/bootargs` (`initrd=<hexaddr>,<decsize>`) plus
  RAM size from `/memory`. **This reader was compiled natively and run against
  the real 18 KB T113 DTB** — the first genuine *execution* in the project:
  it returned `/memory = 0x40000000 + 128 MiB` and `initrd = 0x41c00000,1155695`
  correctly, with missing nodes/props returning NULL.
- `kmain` reads the DTB, `pmm_reserve`s the DTB + initrd regions (so allocation
  can't clobber them), clamps `ram_top` to what the MMU maps, and unpacks the
  **bootloader-provided initramfs** (`-initrd`/`linux,initrd-*` on QEMU, or the
  `initrd=` bootargs region on the T113). There is **no embedded fallback**: if
  the boot handoff doesn't provide an initrd, the kernel halts. (This mirrors
  real Linux, which either uses `CONFIG_INITRAMFS_SOURCE` built-in *or* the
  bootloader's initrd — but never silently falls back to a baked-in one. A tiny
  test cpio is still built by `make` as a `-initrd` you can point at explicitly;
  it is no longer linked into the kernel.)
- `pmm.c` gained `pmm_reserve(start,end)` + an `end<=start` guard on `pmm_init`.

**In-kernel gunzip** (`inflate.c/.h`) — a puff-style DEFLATE decoder + gzip
wrapper (CRC32 + `ISIZE` checked). `load_archive()` detects the `1f 8b` magic,
sizes a pmm scratch buffer from `ISIZE`, inflates, then `cpio_load`s — so the
bootloader's real `initramfs.cpio.gz` works directly. The embedded test cpio is
itself gzipped (`FLG=0x08`, FNAME) so the default path exercises it too. Proven
by real execution: byte-identical on the actual 1.4 MB image + stored/empty/
back-ref/incompressible/FNAME cases, and fuzzed under ASan/UBSan (no OOB/hang).

**argv/env stack + `#!` shebang exec** — the last exec-side pieces so BusyBox can
actually run (it selects its applet from `argv[0]`, and `/init` is a `#!/bin/sh`
script). Faithful reimplementations of Linux's `create_elf_tables` +
`binfmt_script`:
- `setup_user_stack(l1, top, argv, envp)` builds the exact initial stack musl's
  crt reads: strings at the page top, then `AT_RANDOM`, then
  `argc | argv[] | NULL | envp[] | NULL | auxv{AT_PAGESZ,AT_RANDOM,AT_NULL}`,
  16-byte-aligned at `argc`. (Verified vs musl 1.2.2 `__libc_start_main`: only
  `AT_PAGESZ=4096` really matters; `AT_RANDOM`/`AT_PHDR`/`set_tls` unneeded for
  this no-PT_TLS static binary.)
- `proc_execve(path, argv, envp)` snapshots the caller's argv/envp into a kernel
  `bprm` buffer **before** touching the address space (they're only valid in the
  caller's AS), then loops on `#!`: `parse_shebang` → splice
  `[interp, (arg), scriptpath, orig argv[1..]]` → load the interpreter ELF
  (bounded to 4 levels). A real ELF loads directly via `vfs_load_elf` + a fresh
  argv/env stack. The shebang parser + stack layout are host-tested; `uhello`
  now has a crt-style `_start` that reads and prints its argv/env to validate
  the layout end to end.

The test `/init` exercises all of it: `brk`/`mmap2`/`munmap`/`TIOCGWINSZ`, then
`fork` + `execve("/bin/hello", ["/bin/hello","arg1"], ["HOME=/"])` — and
`/bin/hello` prints back `argc=2, argv[0]=/bin/hello, argv[1]=arg1, env HOME=/`.

**Signals** are stored but not yet delivered: `rt_sigaction`/`rt_sigprocmask`
record the disposition + blocked mask in the PCB (so the shell's startup setup
succeeds), but a handler is not yet invoked and `rt_sigreturn` is not yet
reached. The async source that delivery needs now exists (the timer IRQ preempts
user mode — see the scheduling note below), so delivery is unblocked as a next
step: on the return-to-user path, if a pending unblocked signal has a handler,
push a signal frame and redirect `tf.pc`; `rt_sigreturn` restores it.

**Done (post-S10):** the real musl-BusyBox rootfs boots to an interactive prompt
in QEMU, now under a **user-mode-preemptive** scheduler (the timer time-slices
user processes; the kernel itself stays non-preemptible). **Still ahead:** run
the same image on the physical T113 (`BOARD=t113`, loaded by our bootloader);
signal *delivery* (the machinery above); a real `/proc`,`/sys` rather than the
empty mount-point no-ops; and — only if ever needed — in-kernel preemption or a
second core (both require kernel-wide locking).

## Status: Stage 9 ✅ (built) — filesystem: cpio → ramfs + VFS + file syscalls

S9 gives the kernel a **filesystem**. At boot it unpacks a `newc` cpio archive
into an in-memory read-write filesystem (**ramfs**), and adds the file syscalls a
shell needs — `openat`, `read`, `write`, `getdents64`, `fstat64`, `lseek`,
`dup`/`dup2`, `fcntl64`, `chdir`, … — over a per-process file-descriptor table.
`execve` now loads programs **from the filesystem** instead of an embedded table.

The S9 test (`/init`) opens+reads `/etc/motd`, lists `/` and `/bin` via
`getdents64`, then forks and `execve`s `/bin/hello` — a *separate ELF read out of
the ramfs*. On hardware you'll see:

```
Stage 9: unpacking initramfs cpio into ramfs ...
cpio: unpacked 7 entries (12288 bytes archive), heap used N KiB
proc: spawned '/init' pid 1 entry 0x100dc l1 0x41xxx000
init: hello from the filesystem, I am pid 1
init: contents of /etc/motd:
    Welcome to gameboy-v3 (custom kernel, Stage 9: the filesystem is alive).
init: contents of /:
    ./ ../ bin/ etc/ dev/ init
init: contents of /bin:
    ./ ../ hello
init: [child] execve("/bin/hello") from the fs
hello: I am /bin/hello, a separate ELF loaded from the ramfs!
hello: /etc/motd says: Welcome to gameboy-v3 ...
[kernel] pid 2 exit(7)
init: child pid 2 exited with code 7
init: S9 filesystem test complete, exiting 0
```

That proves the whole path: a cpio parsed into inodes, files read through fds,
directories enumerated with the exact `getdents64` record layout, and a program
loaded + run *from the filesystem*. This is what `execve("/bin/sh")` will use.

### How the pieces fit (S9)

- **`kmalloc.c/.h`** — a small kernel heap (first-fit free list over pmm pages,
  forward-coalescing). The fs needs variable-sized objects (inodes, dirents,
  page-pointer arrays); this is the seed of a slab allocator.
- **`ramfs.c/.h`** — the in-memory RW filesystem: inodes (`RF_REG`/`RF_DIR`/
  `RF_LNK`/`RF_CHR`), directory child lists, **page-backed growable file data**,
  and path resolution (`ramfs_lookup`, following symlinks with a loop budget).
- **`cpio.c/.h`** — a `newc` (magic `070701`) parser. Walks the 110-byte ASCII
  headers, honours the 4-byte name/data padding, recreates dirs/files/symlinks/
  device nodes as inodes. Format verified against `gen_init_cpio` + the kernel
  parser (see the facts table).
- **`file.c/.h`** — `struct file` (inode + offset + flags + refcount) and the
  per-process **fd table** (`NOFILE=16`). `dup`/`dup2`/`fork` share one open-file
  description (shared offset, refcount); `exit` closes all.
- **`vfs.c/.h`** — the thin glue: resolve a path (relative to a process cwd) and
  load an ELF *from a file inode* (linearize the file into a kmalloc buffer, hand
  it to the existing `elf_load`). `execve` and the syscalls talk to this, so
  adding a real filesystem later doesn't touch them.
- **`fs_syscall.c/.h`** — the file syscalls, over ramfs + the fd table. The
  console (char device `5,1`) is special-cased: reads/writes go to the UART, so a
  program's stdin/stdout/stderr work. `kstat.h`/`dirent.h`/`fs_abi.h` hold the
  ABI-exact structs and constants (compile-time `_Static_assert`ed).
- **`proc.c`** — `struct proc` gains an `fdtable` + `cwd`; new processes get
  fd 0/1/2 → `/dev/console`; `fork` copies the table, `execve` preserves it,
  `exit` closes it.
- **`progs_blob.S` + `initramfs.c`** — bake the built test cpio into the kernel
  (`.incbin`); `initramfs_base()/size()` hand it to `cpio_load()`. This is the
  seam that later points at the bootloader-loaded musl-BusyBox image instead.

The S8 embedded-ELF table (`progs.c`) is **removed** — `execve` is fully VFS-based
now.

## Status: Stage 8 ✅ (built) — processes: fork + execve + wait4 + exit

S8 adds the **process model**: per-process virtual address spaces (4 KB paging),
and the four syscalls a shell is built on — `fork`, `execve`, `wait4`, `exit`.
The test is the real thing in miniature: an embedded `/init` program forks; the
child `execve`s a **completely separate** embedded ELF `/hello` into a fresh
address space; the parent `wait4`s and prints the child's exit code. On hardware
you'll see:

```
Stage 8: spawning /init ...
proc: spawned '/init' pid 1 entry 0x100dc l1 0x41xxx000
init: hello, I am pid 1
init: forked child pid 2, waiting...
init: [child] calling execve("/hello")
elf: PT_LOAD vaddr=0x10000 filesz=142 memsz=142
hello: I am a SEPARATE ELF, execve'd into the child's address space!
[kernel] pid 2 exit(7)
init: wait4 returned pid 2, child exit code 7
init: done, exiting 0
[kernel] pid 1 exit(0)
[kernel] last process exited; halting.
```

That sequence proves the whole machinery: two processes with independent page
tables at overlapping user VAs (both link at `0x10000`), a second program image
loaded and run via the ELF loader, and exit status flowing parent←child through
`wait4`. This is exactly what a shell does to run a command.

### How the pieces fit (S8)

- **`vm.c/.h`** — per-process address spaces. Each is a 16 KB L1 table that
  *copies the kernel's S6 identity map* (so kernel code/stack/MMIO stay reachable
  after a `TTBR0` switch) and adds **4 KB pages** for user VAs via L2 tables.
  `vm_create`/`vm_map_page`/`vm_walk`/`vm_copy` (eager-copy fork)/`vm_destroy`
  (the single teardown path — frees every user page + L2 + the L1) / `vm_switch`.
- **`elf.c/.h`** — minimal static `ET_EXEC` loader: map + copy each `PT_LOAD`
  segment at its `p_vaddr`, zero bss (pmm hands out zeroed frames), return
  `e_entry`. No relocation, no interpreter (BusyBox static is `ET_EXEC`).
- **`proc.c/.h`** — the PCB table + `fork`/`execve`/`wait4`/`exit` + a minimal
  round-robin. **Uniform switch model:** `syscall_trap` snapshots the caller's
  frame into its PCB (`cur->tf`); each process syscall operates on `cur->tf` and
  may change `cur` to context-switch; the trap always resumes `proc_current_tf()`.
  One kernel stack, cooperative switching at syscall boundaries (preemptive
  scheduling on the timer IRQ is a later refinement).
- **`progs.c/.h` + `progs_blob.S`** — the embedded-program registry. The two user
  ELFs (`user/uinit.c`, `user/uhello.c`, built standalone via `user/user.ld`) are
  `.incbin`'d into the kernel; `prog_lookup("/init")` etc. hands their bytes to
  the loader. This is the seam S9 replaces with a real ramfs lookup — `execve`
  itself won't change.
- **`start.S`** — `svc_entry` now builds a **full `struct trapframe`** (user
  sp/lr, r0-r12, pc, cpsr) and calls `syscall_trap(&frame)`, which may return a
  *different* process's frame; `user_return` restores it. (It resets the kernel
  SP to a fixed top on entry, since the previous return repointed it into a PCB.)
- **`libk.c`** — `memcpy`/`memset`/`memmove`; GCC emits calls to these for struct
  assignment (`p->tf = *tf`) and array init even under `-ffreestanding`.

The S7 `enter_user`/`kernel_resume`/`.user`-section path is **removed** — S8's
`proc_run_first` + `user_return` supersede it.

## Status: Stage 6 ✅ (built) — MMU on top of S5's vectors/GIC/timer

S5 gave the bare-metal foundation (vectors + GIC-400 + a timer heartbeat). S6
adds **virtual memory**: a physical page allocator and an ARMv7 short-descriptor
page table, with the MMU enabled. On hardware you'll see a `pmm:`/`mmu:` init
log, an `mmu selftest: … MATCH` line proving translation works, then the S5
timer heartbeat — now running with the MMU on.

| Stage | Goal | Status |
|-------|------|--------|
| **S5** | vectors + GIC-400 + generic-timer tick | ✅ built |
| **S6** | MMU (identity map + phys page allocator) | ✅ built |
| **S7** | user mode + syscall dispatch | ✅ built |
| **S8** | per-process paging + fork/execve/wait4/exit + ELF loader | ✅ built |
| **S9** | cpio → ramfs (RW) + kmalloc + VFS + fd tables + file syscalls | ✅ built |
| **S10** | memory syscalls + tty + boot handoff + gunzip → real BusyBox `sh` | ✅ running in QEMU (interactive prompt) |

## Source tree

Organized after the Linux kernel: the **arch-specific** half (anything that would
change on a port to another CPU) lives under `arch/arm/`; everything else is
portable C grouped by role (`kernel/` core, `mm/` memory, `fs/` filesystem +
program loading, `drivers/` devices, `lib/` helpers). All headers are centralized
under `include/` (with `include/uapi/` for the kernel↔user ABI), and the build
adds `-Iinclude`.

```
arch/arm/          ARMv7-A / Cortex-A7 / T113 — the non-portable half
  start.S          entry, vector table, banked fault stacks, svc_entry +
                   switchable irq_entry (full trapframe) + user_return, irq_save/restore
  vfp.S            VFP/NEON save/restore (d0-d31 + FPSCR); `.fpu neon-vfpv4`
  mmu_asm.S        CP15 accessors (TTBR0/TTBCR/DACR/SCTLR) + the enable sequence
  mmu.c            ARMv7 short-descriptor page table (1 MB sections), identity map
  vm.c             per-process address spaces: 4 KB paging (L2), create/map/walk/copy/destroy/switch
  syscall.c        `svc 0` trap dispatch (process + file syscalls, ARM EABI; else -ENOSYS)
  kernel.ld        link at LOAD_ADDR (default 0x41000000; 0x40010000 for QEMU via --defsym)
kernel/            portable core
  main.c           C entry: pmm + MMU init, selftest, GIC/timer, irq_dispatch
                   (tick + user-mode preemption seam), spawn /init, enter userspace
  proc.c           PCB + fork/execve/wait4/exit + preempt/sleep/wakeup + ELF-load handoff
mm/                memory management
  pmm.c            physical page allocator (4 KB frames, bitmap over DRAM)
  kmalloc.c        small kernel heap (first-fit free list over pmm pages, coalescing)
  mmap.c           S10 memory syscalls: brk + mmap2 (anon + file-backed/MAP_FIXED) / munmap / mprotect
fs/                filesystem + program loading
  vfs.c            path resolve + load an ELF from a file inode (the execve seam)
  ramfs.c          in-memory RW filesystem: inodes, dir lists, page-backed files, path resolution
  cpio.c           `newc` (070701) cpio parser → ramfs
  file.c           struct file + per-process fd table (fork copies, exit closes)
  fs_syscall.c     file syscalls over ramfs + fds; console (char 5,1) → UART; tty ioctls
  binfmt_elf.c     ELF loader: ET_EXEC + ET_DYN (load bias, PT_INTERP, AT_PHDR) — runs musl ld.so
drivers/           devices
  gic.c            GIC-400 (enable PPI, ack/EOI)
  timer.c          ARMv7 CP15 generic timer, periodic tick
  of/kfdt.c        boot-handoff FDT reader (initrd + /memory)
  tty/uart.c       console wrapper (\n→\r\n) over the board's uart_hw_* back-end
  tty/board_16550.c / board_pl011.c  the two UART back-ends (T113 16550 / QEMU PL011)
lib/               portable helpers
  libk.c           memcpy/memset/memmove/memcmp + str* (GCC emits some under -ffreestanding)
  printf.c         tiny printf → UART
  inflate.c        in-kernel gunzip: puff-style DEFLATE (RFC1951) + gzip (RFC1952) + CRC32
include/           all headers (-Iinclude); board.h is the GIC/timer/UART board seam
  uapi/            the kernel↔user ABI: gv3_syscalls.h (numbers) + gv3_abi.h (types/structs/flags)
user/              standalone static test programs (uinit, uhello, ufault*, uorphan*, upreempt*) + user.ld
test/              golden.sh regression harness (smoke/fault/orphan/preempt/busybox/dynamic)
Documentation/     this README, PLAN.md, S8/S10_DESIGN.md
Makefile           BOARD=t113(default)/virt; builds build/<board>/; `make test` runs golden.sh
motd.txt           data file packed into the test cpio at /etc/motd
```

## Stage 6 — the MMU, how it works

- **Physical allocator** (`pmm.c`): a bitmap over DRAM from `_kernel_end` to the
  top of 128 MB, handing out 4 KB frames (`pmm_alloc_page`) or aligned runs
  (`pmm_alloc_aligned`, used for the 16 KB-aligned page table).
- **Page table** (`mmu.c`): one 16 KB L1 table = 4096 × 1 MB *section*
  descriptors, TTBR0-only (`TTBCR.N=0` → TTBR0 covers all 4 GB). We
  **identity-map** the whole address space (VA == PA) with region-appropriate
  attributes, so the kernel keeps running the instant translation turns on:
    - DRAM `0x40000000..0x48000000` → **Normal, write-back cacheable, RW**
      (`descriptor = (pa & 0xFFF00000) | 0x1C0E`)
    - everything else (incl. MMIO: UART `0x02500000`, GIC `0x03021000`, CCU) →
      **Device, uncached, execute-never** (`| 0x0C16`)
- **Enable** (`mmu_asm.S`): build table → `TLBIALL` → `DACR=0x55555555` (all
  domains client) → `TTBCR=0` → `TTBR0=table` (non-cacheable walks, simplest
  correct) → `DSB; ISB` → set `SCTLR.M` → `ISB`. Caches (SCTLR.C/I) are left
  **off** for S6 (running cacheable would need cleaning the freshly-built table
  from the D-cache first; off is simplest and correct).
- **Selftest** (`kmain.c`): map a fresh physical page at an unused VA
  (`0x50000000`), write through the VA, read back through the (identity-mapped)
  PA — if they match, translation demonstrably works. VA≠PA here, so it truly
  exercises the MMU (unlike identity regions where VA==PA proves nothing).

## Build

```bash
../scripts/00-toolchain.sh      # once (shared toolchain)
make                            # → build/gv3kernel.bin
```

## How it runs (loaded by our bootloader)

Our kernel is linked to run from **DRAM at 0x41000000**, so — unlike the
bootloader — it needs something to have already initialized DRAM and copied it
there. That something is **our bootloader** (`../bootloader`), which already:
inits DRAM, reads a file off the SD, loads it to 0x41000000, and jumps to it.

The bootloader currently loads a file named **`zImage`** to 0x41000000 and jumps
there (its Linux path). Our kernel binary is a raw ARM image that wants exactly
that: a jump to its first instruction at 0x41000000. It ignores the r0/r1/r2 the
bootloader passes (those are the Linux DTB handoff — harmless to us). So the
quickest way to boot S5 on hardware:

1. Build the bootloader (`../bootloader`) and this kernel.
2. Put `build/gv3kernel.bin` on the SD's FAT partition **named `zImage`**
   (replacing the Linux zImage). The DTB/initramfs the bootloader also loads are
   simply unused by our kernel.
3. Boot. After the bootloader's "Jumping to kernel", you should see the S6
   init log (pmm/mmu/selftest), the S5 GIC/timer bring-up, then the S9
   filesystem test:

```
===================================================
  gameboy-v3 custom kernel — Stage 9
  ramfs from cpio + VFS + file syscalls
===================================================
pmm: NNNN pages (~127 MiB) over 0x41xxxxxx..0x48000000
mmu: L1 table @ 0x41xxx000 (identity map, DRAM normal / MMIO device)
mmu: enabled (translation on)
mmu selftest: VA 0x50000000 -> PA 0x41xxx000, readback MATCH (translation works)
gic: distributor + cpu interface enabled
IRQs enabled.

Stage 9: unpacking initramfs cpio into ramfs ...
cpio: unpacked 7 entries (12288 bytes archive), heap used N KiB
proc: spawned '/init' pid 1 entry 0x100dc l1 0x41xxx000
init: hello from the filesystem, I am pid 1
init: contents of /etc/motd:
    Welcome to gameboy-v3 (custom kernel, Stage 9: the filesystem is alive).
init: contents of /:
    ./ ../ bin/ etc/ dev/ init
init: contents of /bin:
    ./ ../ hello
init: [child] execve("/bin/hello") from the fs
hello: I am /bin/hello, a separate ELF loaded from the ramfs!
hello: /etc/motd says: Welcome to gameboy-v3 ...
[kernel] pid 2 exit(7)
init: child pid 2 exited with code 7
init: S9 filesystem test complete, exiting 0
[kernel] pid 1 exit(0)
[kernel] last process exited; halting.
```

The proofs, in order: a cpio parsed into a filesystem, a file read through an fd
(`/etc/motd`), directories enumerated with the exact `getdents64` record layout,
and a second program loaded + run **from the ramfs** (`/bin/hello`) — the exact
path `execve("/bin/sh")` will take. (Later, a dedicated bootloader mode or a
distinct filename can replace the `zImage` rename; for a first bring-up this is
the least-friction path.)

## Verified facts behind the code (sun20i / Cortex-A7)

Confirmed against the ARM ARMv7-A ARM, GIC-400 TRM, and Cortex-A7 TRM
(adversarially, like the bootloader stages):

| Thing | Value |
|-------|-------|
| Vector table | 8× `ldr pc`, 32-byte aligned, order reset/undef/svc/pabt/dabt/rsvd/irq/fiq |
| VBAR | `mcr p15,0,Rn,c12,c0,0`; SCTLR.V (bit 13) must be 0 |
| IRQ return | `subs pc, lr, #4` (lr_irq is +4) |
| CPSR modes | irq=0x12, svc=0x13; I-bit = bit 7 |
| GIC-400 GICD base | `0x03021000` |
| GIC-400 GICC base | `0x03022000` |
| GICD_CTLR / ISENABLER / IPRIORITYR | `0x000` / `0x100+4n` / `0x400+n` |
| GICC_CTLR / PMR / IAR / EOIR | `0x000` / `0x004` / `0x00C` / `0x010` |
| Spurious INTID | 1023 (0x3FF) |
| Timer | ARMv7 generic, **secure physical** (CNTP_*) |
| Timer INTID | **29** (PPI 13) |
| CNTFRQ / CNTP_TVAL / CNTP_CTL | `p15,0,Rt,c14,c0,0` / `c14,c2,0` / `c14,c2,1` |
| Counter frequency (T113) | 24 MHz (DT `clock-frequency = 0x16e3600`) |
| **MMU** L1 table | 4096 × 1 MB sections = 16 KB, 16 KB-aligned; TTBR0-only, `TTBCR.N=0` |
| Section descriptor (Normal RAM) | `(pa & 0xFFF00000) \| 0x1C0E` (WBWA, RW, XN=0, Dom0) |
| Section descriptor (Device/MMIO) | `(pa & 0xFFF00000) \| 0x0C16` (device, RW, XN=1, Dom0) |
| TTBR0 / TTBCR / DACR / SCTLR | `c2,c0,0` / `c2,c0,2` / `c3,c0,0` / `c1,c0,0` |
| TLBIALL / TLBIMVA | `c8,c7,0` / `c8,c7,1` |
| DACR (all domains client) | `0x55555555` |
| SCTLR.M enable | bit 0; enable order: TLBIALL→DACR→TTBCR→TTBR0→DSB;ISB→set M→ISB |
| **L1 coarse** descriptor (points to L2) | `(l2_pa & 0xFFFFFC00) \| 0x01` (domain 0) |
| **L2 small page** (4 KB, Normal, user RW, exec) | `(pa & 0xFFFFF000) \| 0x7E` (AP=0b011, TEX=0b001, B=C=1, XN=0) |
| L1 / L2 index | `va[31:20]` / `va[19:12]`; L1 = 16 KB, L2 = 1 KB (256 entries) |
| trapframe (17 words, low→high) | sp_usr, lr_usr, r0-r12, pc(=lr_svc), cpsr(=spsr_svc) |
| user mode CPSR | `0x10` (USR, I/F clear) |
| ELF: ET_EXEC / EM_ARM | `e_type=2` / `e_machine=40`; entry `+24`, phoff `+28`, phnum `+44`; PT_LOAD=1 |
| initial user stack | argc=0, argv=NULL, envp=NULL, auxv{AT_PAGESZ=6→4096, AT_NULL=0}, 16-byte aligned |
| EABI syscalls used (S8) | fork=2, execve=11, write=4, exit=1, getpid=20, wait4=114, exit_group=248 |
| **cpio newc** header | 110 bytes: 6-char magic `070701` + 13 × 8-char ASCII-hex fields; name follows (incl NUL); name & data each padded to 4 |
| cpio symlink / trailer | link target = file DATA (may lack NUL — we add one); archive ends at name `TRAILER!!!` (namesize 11) |
| **file syscalls** (ARM EABI) | openat=322, close=6, read=3, write=4, writev=146, _llseek=140, dup=41, dup2=63, fcntl64=221, getdents64=217, fstat64=197, stat64=195, fstatat64=327, faccessat=334, readlinkat=332, chdir=12, getcwd=183, ioctl=54, uname=122 |
| musl uses the *at/*64 forms | not open(5)/lseek(19)/stat(106)/getdents(141) — those are legacy; musl issues openat/_llseek/*stat64/getdents64 |
| **`O_DIRECTORY` (ARM!)** | `040000` (16384) — ARM override, NOT the asm-generic `0200000`. O_CREAT=0100, O_TRUNC=01000, O_APPEND=02000, AT_FDCWD=−100 |
| ARM `struct stat64` | **104 bytes**; `st_mode`@16, `st_size`(s64)@48, `st_blocks`@64, real `st_ino`(u64)@96 **and** legacy `__st_ino`@12; EABI holes @44/@60 |
| `linux_dirent64` | `d_ino`(u64)@0, `d_off`(s64)@8, `d_reclen`(u16)@16, `d_type`(u8)@18, `d_name`@19; `d_reclen = round_up(19+namelen+1, 8)` |
| `d_type` / iovec | DT_DIR=4, DT_REG=8, DT_LNK=10, DT_CHR=2; `struct iovec` = {ptr@0, len@4} (8-byte stride on arm32) |
| **mem syscalls** (ARM EABI) | brk=45, mmap2=192, munmap=91, mprotect=125, madvise=220 |
| **mmap2 6th arg = PAGES** | `mmap2(addr,len,prot,flags,fd,pgoff)`: pgoff is in 4 KB units, not bytes. anon private = `MAP_PRIVATE\|MAP_ANONYMOUS = 0x22`, fd=-1, ignore fd for anon |
| MAP_/PROT_ (ARM=generic) | PROT_READ=1/WRITE=2/EXEC=4/NONE=0; MAP_SHARED=1, MAP_PRIVATE=2, MAP_TYPE=0xf, MAP_FIXED=0x10, MAP_ANONYMOUS=0x20 |
| **brk contract** | returns the RESULTING break, NEVER -errno: `brk(0)`=query current; grow→new; failure→OLD unchanged (musl detects failure by "returned < requested") |
| mmap error convention | success = mapped VA; error = value in `-4095..-1` (musl reads as MAP_FAILED). Never return a mapping in the top 4 KB |
| **isatty uses TIOCGWINSZ** | musl `isatty()` = `ioctl(fd, TIOCGWINSZ)` (NOT TCGETS) — must succeed on console for the interactive shell path |
| tty ioctl consts (ARM=generic) | TCGETS=0x5401, TCSETS=0x5402, TIOCGWINSZ=0x5413, TIOCGPGRP=0x540F, TIOCSCTTY=0x540E, FIONREAD=0x541B |
| `struct termios` | NCCS=19 → 36 bytes: `c_iflag/oflag/cflag/lflag`(4×u32) + `c_line`(u8) + `c_cc[19]`. `struct winsize`={row,col,xp,yp} u16×4 |
| **boot handoff** | `r0`=0, `r1`=machine (~0 for DT), `r2`=DTB phys addr; SVC, IRQ/FIQ off, MMU+D$ off. DTB detected by magic `0xd00dfeed` (BE) at r2 |
| **FDT header** (BE u32) | magic@0, totalsize@4, off_dt_struct@8, off_dt_strings@12, off_mem_rsvmap@16, version@20 |
| FDT tokens (BE, 4-aligned) | BEGIN_NODE=1 (+name NUL, pad4), END_NODE=2, PROP=3 (+{len,nameoff} BE, +value pad4), NOP=4, END=9; `nameoff`=byte offset into strings block |
| FDT paths | root = empty name = depth 1; `/chosen`,`/memory` = depth 2; match ignores `@unit` suffix. `/memory reg` (root cells 1/1) = `<u32 base><u32 size>` BE |
| initrd via bootargs | `/chosen/bootargs` string carries `initrd=<hexaddr>,<decsize>` (addr hex w/ 0x, size decimal) — kernel must reserve DTB+initrd regions before allocating |

## What's verifiable here vs. on hardware

As of S10 the kernel **runs end to end on QEMU** (`BOARD=virt`): the timer fires,
the GIC routes it, the MMU translates, processes fork/exec/wait, and real
BusyBox reaches an interactive prompt — so the logic is proven by execution, not
just structure. What QEMU's `virt` machine can **not** prove is the **T113
hardware specifics**: it uses a PL011 UART and GICv2 at different addresses than
the T113's 16550 + GIC-400 (hence the `board.h` seam), and it doesn't model the
T113's secure-physical-timer routing or the exact clock. Those still need the
physical T113 + serial console. The `BOARD=t113` build is structurally correct
(vector table 32-byte aligned at the load address, VBAR written, SCTLR.V cleared,
IRQ path with the `subs …#4` correction and clean `srs`/`rfe` — checked via
objdump) but has not yet been run on silicon.

Things most likely to need on-silicon iteration (flagged honestly):
1. **Timer/GIC group routing.** GIC-400 has security extensions; we deliver the
   timer as a Group-0 IRQ (GICC_CTLR=1, FIQEn=0). If the secure timer arrives as
   an FIQ instead on this part, the fix is small (route/handle FIQ) but is a
   real possibility to check first.
2. **CNTFRQ.** We read it and fall back to 24 MHz + program it if it reads 0. If
   ticks come at the wrong rate, CNTFRQ is the first suspect.
3. **MMU caches off.** We enable translation but leave SCTLR.C/I (D/I caches)
   OFF for S6 — correct and simplest, just not fast. Turning caches on later
   needs cleaning the page table out of the D-cache first; deferred deliberately.
   If the MMU selftest reads back MISMATCH, suspect the section-descriptor
   attribute bits or the L1 table alignment (must be 16 KB-aligned).

S8-specific things to watch on hardware:
4. **L2 descriptor / AP bits.** User pages use AP=0b011 (PL0+PL1 RW). If a user
   read/write faults (data abort) or the kernel can't touch user memory during a
   syscall, the `0x7E` small-page attributes or the `0x01` coarse-L1 entry are
   the first suspects. (These were verified against the ARM ARM but only the
   board proves them.)
5. **TLB coherence on switch.** `vm_switch` does `TTBR0 ← l1; TLBIALL`. We flush
   the *whole* TLB on every address-space switch (simplest, correct); if stale
   translations ever appear, this is where ASIDs/selective invalidation would go.
6. **Scheduling: USER-MODE preemptive (post-S10).** Originally cooperative
   (switch only at syscall boundaries) — a CPU-bound user program that never
   trapped would run forever. Now the timer IRQ **preempts user mode**: the
   switchable `irq_entry` builds a full trapframe and `proc_preempt` (proc.c)
   round-robins to another runnable process — but *only when the interrupt hit
   USER mode*. A syscall still runs to completion un-preempted (SVC entry masks
   IRQs), so the kernel stays non-reentrant and needs **no in-kernel locking** on
   a single core. VFP/NEON (d0-d31 + FPSCR) is saved/restored per switch
   (`vfp.S`) so time-slicing FP-using processes is safe. Proven by the `preempt`
   golden case: two CPU-bound children (no syscalls in their loops) interleave.
   *Not* done: in-kernel preemption (would need per-process kernel stacks +
   kernel-wide locking) and SMP (second core) — both explicitly out of scope.
7. **`wait4` re-run trick.** When the parent waits before the child has run, we
   rewind the parent's saved `pc` by 4 so the `svc` re-executes after the child
   exits and re-selects the parent. Verify the compiler emitted a 4-byte `svc`
   (ARM, not Thumb) — it does here (`-marm`), but it's an assumption worth noting.

S9-specific things to watch on hardware:
8. **Struct ABI to musl.** The ARM `struct stat64` (104 B) and `linux_dirent64`
   layouts are `_Static_assert`ed at compile time, but only real BusyBox proves
   musl reads them correctly. If `ls -l` shows garbage sizes/types, suspect the
   stat64 field offsets or the `getdents64` `d_reclen` rounding.
9. **No user-pointer validation (but faults are now contained).** Syscalls
   dereference user VAs directly (the caller's address space is active), so a bad
   pointer passed INTO a syscall still faults in kernel mode — which the fault
   handler treats as a kernel panic (halt), since there's no clean `-EFAULT`
   unwinding of a half-done syscall. What IS handled now: a fault in USER code
   (bad load/store, undefined instruction) is caught by real handlers
   (`start.S` undef/pabt/dabt → `fault_trap` in `proc.c`), which kill just that
   process with SIGSEGV/SIGILL and reschedule — the rest of the system keeps
   running (proven by the `fault` golden case: a child writes through NULL, the
   parent survives). `access_ok`-style validation so a bad *syscall argument*
   pointer returns `-EFAULT` instead of panicking is still a hardening item.
10. **Console input is polled + blocking.** `sys_read` on the console spins in
    `uart0_getc` until a byte arrives (no interrupt-driven input, no line editing
    beyond CR→LF + echo). With cooperative scheduling that means a `read` on the
    console blocks *the whole kernel* until a key is pressed — acceptable for a
    single-shell bring-up, but it pairs with preemptive scheduling later.
11. **kmalloc is simple.** First-fit + forward-only coalescing. Heavy fs churn
    could fragment it; there's no compaction. Fine for the test and a small
    initramfs; revisit if BusyBox's allocations stress it.
    (Adversarial review of S9 caught and fixed four latent bugs that don't
    manifest on the embedded test cpio but would on the real BusyBox rootfs:
    a `dup2` fd-leak, a symlink-loop guard that recursed instead of bounding —
    now threads one shared budget across the whole resolution so a cyclic
    symlink returns `-ELOOP` rather than overflowing the kernel stack — a 32-bit
    `off+n` overflow in `ramfs_read/write`, and silent mis-splitting of path
    components ≥64 bytes — now `-ENAMETOOLONG`.)
12. **`brk` returns `-ENOSYS` on purpose** so musl falls back to `mmap2` — but
    `mmap2` isn't implemented yet either (S10). Static BusyBox reaching `main`
    will need one of them; that's the first thing S10 adds.
