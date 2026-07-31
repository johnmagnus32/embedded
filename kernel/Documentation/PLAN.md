# gameboy-v3 custom kernel — plan

Writing our own minimal kernel for the T113-S3, as the deepest "from scratch"
step of the project. **Status: S5–S10 all built; the goal is reached — the
unchanged musl BusyBox initramfs boots to an interactive `/bin/sh` prompt, first
proven running in QEMU (`-M virt`). Not yet run on the physical T113.** This doc
is now part design record, part map of what's left (silicon bring-up +
preemption + signal delivery).

## The goal (stated precisely)

Boot **our own kernel** (not Linux), have it run our **existing musl BusyBox
initramfs unchanged**, and reach an interactive shell where `ls` / `cat` /
`echo` / `cd` work. Minimal features — just enough for the shell to run.

The unchanged-initramfs constraint is what defines the project: BusyBox is a
**statically-linked ARM Linux binary**, so "run it unchanged" means our kernel
must present the **Linux syscall ABI** (svc 0, number in r7). We are not writing
"a kernel" in the abstract — we are writing a **minimal Linux-ABI-compatible
kernel**. That is a much bigger target than a bare-metal RTOS-style kernel, and
it is the largest single sub-project in the whole gameboy-v3 effort (realistically
months of evenings). Worth it for understanding, not for shipping the handheld.

## What we can reuse (the launch pad is already built)

Everything below the kernel already exists from the bootloader work:
- **Toolchain** — the musl cross toolchain (`arm-buildroot-linux-musleabihf-`),
  Step 0. Freestanding builds (`-nostdlib`), same as the bootloader.
- **The bootloader itself** — becomes our kernel's loader. Today `boot_kernel()`
  jumps to Linux; it would instead jump to *our* kernel image. DRAM is already
  initialized, the SD/FAT reader already loads the initramfs into RAM, and the
  eGON/SD boot path is done.
- **UART0 driver + printf** — our `dram_shim.c` printf and `uart.c` port directly
  as the kernel's early console.
- **The initramfs** — musl BusyBox, `build/output/initramfs.cpio.gz` (~792 KB).

So the kernel starts from: CPU in a known state, DRAM working, a console, and the
initramfs sitting in RAM. We build *up* from there.

## The stages

Each stage boots on real hardware and proves exactly one thing (same discipline
as the bootloader). S5 continues the numbering after the bootloader's Stages 1–4.

> **S9 scope note (the gzip decision).** The existing initramfs artifact is
> `initramfs.cpio.gz`. For S9 we consume an **uncompressed** `.cpio` (the same
> file contents, no gzip wrapper) so the stage stays focused on the fs/VFS —
> the kernel-teaching part. An in-kernel DEFLATE/gunzip is a cleanly-scoped
> optional add-on (S9.5) and does not change any of the fs code. The S9 build
> bakes a small **test** cpio (built from our two user programs + a data file +
> a `/dev/console` node) into the kernel; pointing `cpio_load()` at the real
> bootloader-loaded musl-BusyBox image later is a one-line change (the bytes'
> source), because the parser takes a `(base, size)`.

| Stage | Adds | Proven by | Confidence |
|-------|------|-----------|------------|
| **S5 ✅ built** | Exception vector table + GIC-400 + ARMv7 generic-timer tick | a timer IRQ prints "tick" on UART0 | **high** |
| **S6 ✅ built** | MMU: ARMv7 short-descriptor page table (1 MB sections) + physical page allocator | identity map + enable; VA→PA selftest reads back MATCH | **high** |
| **S7 ✅ built** | User mode + `svc` syscall entry + dispatch table | hand-written user program does write()+getpid()+exit() via svc | **high** |
| **S8 ✅ built** | Per-process address spaces (4 KB paging) + `fork`/`execve`/`wait4`/`exit` + ELF loader | `/init` forks; child `execve`s a 2nd embedded ELF `/hello`; parent `wait4`s + prints exit code | **medium (structural)** |
| **S9 ✅ built** | cpio → ramfs (RW) + a small heap (kmalloc) + VFS + fd tables + file syscalls (`openat`/`read`/`write`/`getdents64`/`fstat64`/`lseek`/`dup`/…); `execve` loads from the FS | `/init` reads `/etc/motd`, `getdents64`'s `/` and `/bin`, forks + `execve`s `/bin/hello` **from the ramfs** (not an embedded blob) | **medium (structural)** |
| **S10 ✅ running (QEMU)** | mem syscalls (`mmap2`/`brk`) + tty `ioctl`s (termios `ECHO` tracked) + identity stubs + **boot handoff** (`r2`/DTB→initrd) + **in-kernel gunzip** + **argv/env stack + `#!` shebang exec** + **signals/clone/time** + **blocking pipe reads** + `set_tls`/`poll`/`statx`/`mount`-stub → real BusyBox `sh` | The **unchanged musl BusyBox** boots in QEMU (`-M virt -cpu cortex-a7`): the stock `#!/bin/sh` `/init` runs, `$(uname -srm)`/`$(nproc)` (command substitution over pipes) populate, `cttyhack`→`/bin/sh` reaches an interactive `/ #` prompt, and `echo`/`uname`/`ls`/`$(… | …)` run as forked applets. Board seam (`board.h` + PL011/16550) targets both QEMU and T113. | **high (executed) on QEMU; T113 unproven** |

**Start with S5 as a standalone milestone.** It is the foundation everything else
needs (interrupts + a timer), it is the part we are most confident about (GIC-400
and the ARMv7 timer are architected ARM IP, not Allwinner black boxes), and it is
a finishable, satisfying thing on its own: a heartbeat printed from a real
interrupt handler. Decide whether to climb toward S10 after S5 works.

## Why the MMU is non-negotiable (the wall at S6)

The shell runs commands by `fork()`+`execve()`. After `fork`, two processes exist
at the **same virtual addresses** (BusyBox is `ET_EXEC`, linked to a fixed
address) backed by **different physical pages**. Only the MMU (per-process page
tables) makes that possible. So the MMU is not an optimization here — it is the
mechanism the ABI requires. (The escape hatch, nommu/uClinux, needs BusyBox
rebuilt with vfork — which violates "unchanged initramfs", so: MMU it is.)

## Hardware facts (verified from the T113 kernel DTB on disk)

- **Interrupt controller: GIC-400** (`arm,gic-400`). Register banks:
  distributor `0x03021000`, CPU interface `0x03022000` (also `0x03024000`,
  `0x03026000` for the virt/hyp views). `#interrupt-cells = <3>`.
- **Timer: ARMv7 architected/generic timer** (`arm,armv7-timer`), per-core PPIs
  (13/14/11/10 = secure/non-secure/virt/hyp physical timer). This is the tick
  source a scheduler runs on — read/program via CP15 (CNTFRQ, CNTP_TVAL,
  CNTP_CTL), no MMIO base needed.
- **CPUs:** 2× Cortex-A7 (`cpu@0`, `cpu@1`). SMP is out of scope for the minimal
  kernel — CPU0 only, exactly as our bootloader already runs.

> Before writing S5 code, verify the GIC-400 register offsets (GICD_CTLR,
> GICD_ISENABLER, GICC_CTLR, GICC_PMR, GICC_IAR, GICC_EOIR) against the ARM
> GIC-400 TRM — adversarially, like every bootloader stage.

## The syscall set — what we measured, and the honest gap

The load-bearing input to S10 is the exact syscall set our musl BusyBox uses.
We investigated statically (no `strace`/`qemu-arm` on the build host):

- **Confirmed:** the musl static binary makes syscalls via `svc 0` with the
  number in `r7`; it references **≥ 73** distinct syscalls.
- **CAVEAT — the static extraction is biased.** musl issues most syscalls through
  a shared `__syscall` trampoline that loads r7 *dynamically*, so grepping for
  `mov r7,#N` immediates **misses the hot core** (`write`, `read`, `openat`,
  `mmap2`, `clone`, `brk`, `fstat64`, …) and over-represents rarely-used
  constant-numbered ones (`swapon`, `acct`, `pivot_root`, `capset`, …). So the
  "73" list is real but NOT the priority list.
- **Realistic estimate for an interactive shell + `ls`/`cat`/`echo`/`cd`:
  ~40–60 syscalls.** The precise runtime set genuinely needs `strace` under
  QEMU-user, which this host lacks.

**Consequence for the plan:** S10 is **empirical**. Implement the obvious core
(below), boot, and let `-ENOSYS` failures on the UART tell you the next syscall
to add. You do not need the full list up front. To get the real list ahead of
time, install `qemu-arm` + `strace` and trace the initramfs once.

### S10 measurement (done — static extraction of the real binary)

`qemu-arm` isn't on this host, so instead of dynamic tracing we disassembled the
actual `build/rootfs/bin/busybox` (musl static, ARM) and decoded every distinct
constant `mov r7,#N` → syscall number (264 `svc` sites; the dynamic-`r7`
trampoline core is already covered by S8/S9). That's the whole *applet* surface;
the **boot-to-`sh`-prompt** path is a subset. Classified:

- **(A) shell can't reach a prompt without these — ALL IMPLEMENTED:**
  `mprotect`(125), `rt_sigaction`(174)/`rt_sigprocmask`(175), `munmap`(91),
  `clone`(120, fork-flags form), `gettimeofday`(78), `sysinfo`(116),
  `clock_gettime`(263)/`clock_gettime64`(403), `nanosleep`(162). `mmap2`/`brk`/
  `set_tid_address`/`ioctl` also present. Signals STORE dispositions + mask (no
  async delivery in the cooperative model — `rt_sigreturn` is a can't-happen
  guard); `clone` supports the fork-equivalent (no `CLONE_VM`), else `-ENOSYS`.
- **(B) needed by `ls`/`cat`/`echo`/`cd`/`mkdir` applets:** `mkdir`(39),
  `rmdir`(40), `unlink`(10), `rename`(38), `symlink`(83), `chmod`(15),
  `ftruncate64`(194), `umask`(60), `poll`(168). (musl already routes stat/read
  via the *at/*64 forms we have.)
- **(C) trivial stubs (return 0 or a constant):** the id/pgrp/rlimit family —
  `getuid[32]`/`getgid32`/`geteuid32`/`getegid32`→0, `getppid`/`getpgid`/`getsid`,
  `setpgid`/`setsid`/`setuid32`/`setgid32`→0, `ugetrlimit`/`setrlimit`,
  `prctl`, `sched_yield`, `times`, `restart_syscall`.
- **(D) out of S10 scope → `-ENOSYS` (BusyBox/musl degrade):** `mount`,
  `umount2`, `reboot`, `swapon/off`, `init_module`, xattrs, `capget/set`,
  `futex`, `sendfile`, `pivot_root`, `personality`, `adjtimex`, `keyctl`,
  `unshare`, `pselect6`, `madvise`(benign no-op stub).

So the list is now **known, not guessed**. The `-ENOSYS`-on-UART loop is a
fallback for anything the static pass under-counted (the dynamic-r7 core), not
the primary method.

**This step (start of S10):** group (A)'s memory piece — `mmap2` (anonymous
private), `munmap`, `mprotect`, real `brk` — plus the tty `ioctl`s, then switch
to the real BusyBox initramfs and read the first `-ENOSYS`. Signals (`rt_sig*`)
and `clone` follow.

### The core set to implement first (S7–S10)
- **Process:** `clone` (fork flags), `execve`, `wait4`, `exit`, `exit_group`,
  `getpid`, `getppid`, `setpgid`, `setsid`
- **File:** `openat`, `close`, `read`, `write`, `readv`/`writev`, `lseek`,
  `dup`, `dup2`, `fcntl64`, `getdents64`
- **Stat:** `fstat64`, `stat64`/`fstatat64`, `faccessat`/`access`
- **Memory:** `brk`, `mmap2`, `munmap`, `mprotect`
- **Dir/cwd:** `chdir`, `getcwd`
- **Terminal:** `ioctl` (termios `TCGETS`/`TCSETS` — start as no-op stubs)
- **Signals:** `rt_sigaction`, `rt_sigprocmask`, `rt_sigreturn`
- **Identity/misc:** `uname`, `getuid32`/`geteuid32`/`getgid32`/`getegid32`
  (return 0 = root), `set_tid_address`, `clock_gettime`, `nanosleep`
- **Everything else:** return `-ENOSYS`. musl degrades gracefully — its startup
  probes for optional features and copes when they fail.

### The musl payoff (why we switched the rootfs to musl)
musl's `_start`→`main` path does far less than glibc's (no NSS, lighter
rseq/robust-list/tunables probing), so the shell reaches its prompt through a
**smaller, more predictable syscall path**. That directly shrinks and de-risks
S10 — the whole reason the rootfs was moved to musl. See
`forge/backends/rootfs.sh` and the toolchain memory.

## Open items to resolve before the hard stages

1. **GIC-400 register offsets** — verify vs the ARM TRM before S5 code.
2. **Exact syscall set** — get `qemu-arm`+`strace` and trace the real initramfs,
   ideally before S10 (S5–S9 don't depend on it).
3. **Kernel link address** — pick a DRAM load address for our kernel (distinct
   from where we load the initramfs); update the bootloader's `boot_kernel`
   target. Our sunxi load-address facts: kernel region ~0x41000000, initramfs
   ~0x41C00000 (see `../bootloader` / boot memory).
4. **ELF loader scope** — BusyBox is `ET_EXEC` static (fixed load address, no
   relocation, no interpreter) — the simplest possible ELF to load. Confirm no
   PIE.

## Honest bottom line

- **S5** (bare-metal interrupts + timer) is the clean, finishable next step and
  the part we're most sure of. Do it first, standalone.
- **S6→S10** is a real journey — writing a minimal Unix (MMU, processes, a VFS,
  a tty). Worth it to *understand* how Linux works; not on the path to shipping
  the Game Boy. Reuses the entire bootloader as its loader.
- The one measurement we still owe ourselves is the true syscall set via
  strace/QEMU — deferred safely to S10.

## References
- Bootloader (the loader + reusable drivers): `../bootloader/README.md`
- Rootfs / musl toolchain: `forge/backends/rootfs.sh`, project memories
- T113 boot facts + load addresses: project memory `gameboy-v3-t113-boot-facts`
