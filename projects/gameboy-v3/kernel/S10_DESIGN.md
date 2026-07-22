# S10 design — memory syscalls + tty, toward an interactive shell

Goal of S10 overall: boot the **unchanged musl BusyBox initramfs** and reach an
interactive `/bin/sh` prompt over UART0. This doc covers the FIRST step (per the
plan's measured group A): the memory syscalls (`mmap2`/`munmap`/`mprotect`/`brk`)
and the tty `ioctl`s — the actual first blocker (static musl can't reach `main()`
without a working `brk`-or-`mmap2` for its malloc, and `isatty()`/job-control
probes drive the shell's interactive path).

## User memory map (per process, VA space 0x00001000 .. 0x40000000)

Measured from the real binary (readelf): BusyBox is `ET_EXEC`, PT_LOAD R-E at
`0x00010000..~0x15e000`, RW data/bss to `~0x172000`, entry `0x20e98`. Stack page
at the top (`0x3ffff000`). That leaves a large middle gap. We split it:

```
  0x00000000  (page 0 unmapped — null-deref traps)
  0x00010000  ELF text/rodata/data/bss        (loaded by elf_load)
  ~0x00172000 program break START (page-rounded highest PT_LOAD end)
      |         brk heap — grows UP via brk()          (proc->brk)
      v
      ...          (gap)
      ^
      |         mmap area — grows DOWN from the top     (proc->mmap_top)
  0x38000000  MMAP_TOP  (mmap base; anon mappings placed just below, downward)
      ...
  0x3ffff000  initial user stack page (grows down)
  0x40000000  USER_VA_MAX (kernel is above, at 0x41000000)
```

Heap (brk) and mmap grow toward each other but start ~0x28 MB apart; for the
shell's modest use they won't meet. No fancy VMA tree — two bump pointers is
enough for S10 (honest scope; a real kernel tracks VMAs).

## brk (nr 45) — THE contract to get right

`brk(addr)` **returns the resulting break**, NOT 0/-errno:
- `brk(0)` (or any addr below the start): return the current break unchanged.
- `brk(new)` grows/shrinks: on success return `new` (page work done); on failure
  (OOM) return the **unchanged old** break. musl compares "returned < requested"
  to detect failure and fall back to mmap2. Returning -errno here breaks musl.

Implementation: `proc->brk` holds the current break (initialized by execve to the
page-rounded top of the ELF's PT_LOADs). Growing maps zeroed pages up to the new
page-rounded break via `vm_map_page`; shrinking unmaps + frees. Clamp to below
the mmap area.

## mmap2 (nr 192) — anonymous private only for S10

`mmap2(addr, len, prot, flags, fd, pgoffset)` — 6th arg is offset in **PAGES**.
S10 supports the case musl's malloc + thread setup use: `MAP_PRIVATE|MAP_ANONYMOUS`
(`0x22`), `fd == -1`. We ignore `addr` hint unless `MAP_FIXED`, allocate `len`
(page-rounded) zeroed frames, map them at a fresh region descending from
`proc->mmap_top`, and return the base VA. File-backed mmap and shared mappings
return `-ENOSYS` (musl's malloc doesn't need them; a file mmap of the binary
isn't used since we exec via read()).

Return convention: success = the mapped VA; failure = a value in `-4095..-1`
(negated errno), which musl treats as `MAP_FAILED`. So on error return `-K_ENOMEM`
etc. and the dispatcher passes it back in r0 unchanged.

- **munmap(addr, len)**: unmap + free the pages in the range (best-effort; we
  don't split partial regions — fine for whole-allocation frees).
- **mprotect(addr, len, prot)**: S10 maps everything RWX already (our L2 small
  pages are user-RW + executable), so mprotect is effectively a **no-op that
  returns 0** — enough for musl's RELRO/guard-page calls to succeed. (A real
  per-page permission change is a later refinement.)

## tty ioctls — make the console look like a terminal

`isatty(fd)` in musl = `ioctl(fd, TCGETS, &t)` succeeds → 1. The shell takes its
interactive path only if stdin/stdout are ttys. So for the console fd (char dev
5,1): `TCGETS` must return 0 and fill a plausible `struct termios` (NCCS=19,
sane c_lflag with ICANON|ECHO). `TIOCGWINSZ` → 24x80. `TCSETS*`/`TIOCSCTTY`/
`TIOCSPGRP` → 0. `TIOCGPGRP` → a pgrp. Non-tty fds → `-ENOTTY`. (S9 already stubs
several; S10 makes the termios payload real so line handling behaves.)

## What this step deliberately defers
- Signals (`rt_sigaction`/`rt_sigprocmask`/`rt_sigreturn`) — group A, next step.
- `clone` (fork-flags) — musl threads; the shell itself is single-threaded, so
  fork() suffices to start. Add if the `-ENOSYS` log demands it.
- Per-page mprotect, file-backed/shared mmap, VMA tracking — later refinements.
- Preemptive scheduling — still cooperative; a blocking console read still
  stalls the kernel (acceptable for a single interactive shell).
