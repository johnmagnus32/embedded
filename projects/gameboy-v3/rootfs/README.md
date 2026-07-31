# gameboy-v3 rootfs — a from-scratch userspace (gv3libc + coreutils)

A minimal root filesystem built **entirely from scratch**: our own C library
(`gv3libc`), our own coreutils, and our own shell — no BusyBox, no external libc.
It's the userspace counterpart to the from-scratch `bootloader/` and `kernel/`,
and the third "drop-in replacement" in the project (replacing the reference
BusyBox rootfs that `forge/backends/rootfs.sh` builds).

The point is **understanding**: to see exactly what a libc, a crt0, a shell, and
a rootfs image are made of, by writing each one against a known syscall ABI.

> **Where the pieces live (forge refactor).** The reusable component sources
> **graduated to repo-root PROVIDERS**: the C library + dynamic linker to
> [`libc/`](../../../libc/) (`libc/`, `libc/ld/`, `libc/test/`), and the coreutils
> to [`coreutils/`](../../../coreutils/) (was `rootfs/bin/`). This `rootfs/` dir is
> now the **product ASSEMBLER** — its `Makefile` builds those providers' sources and
> stages them into an initramfs, layering the product's [`../overlay/`](../overlay/)
> (the PID-1 `init.sh`) on top. Paths below are relative to this dir; provider paths
> reach up (`../../../libc`, `../../../coreutils`, `../../../kernel`).

## Status

- **Static (`make`) — working.** `gv3libc` is a static archive (`libc.a`); the
  programs link it in and run on our custom `kernel/` (verified in QEMU `-M virt`).
- **Dynamic (`make LINK=dynamic`) — working.** PIC programs + a shared `libc.so`,
  loaded at runtime by our own from-scratch dynamic linker **`ld-gv3.so.1`** (see
  [`libc/ld/`](../../../libc/ld/)). Verified end-to-end on BOTH a mainline reference kernel AND our
  own `kernel/` (which now does `PT_INTERP` loading + file-backed `mmap`). The
  interactive shell + all coreutils run dynamically linked.
- Not yet run on real T113 silicon (same caveat as the whole project).

## The design principle: the kernel owns the ABI, the libc consumes it

Our programs make Linux syscalls (`svc 0`, number in `r7`) against **exactly the
ABI our kernel implements**. That ABI is single-sourced in the kernel's UAPI
headers and *consumed* by the libc — never re-typed:

```
  kernel/include/uapi/gv3_syscalls.h   syscall NUMBERS      ┐  the CONTRACT (kernel owns)
  kernel/include/uapi/gv3_abi.h        structs + constants  ┘
        │  `make headers` copies a SNAPSHOT into build/include/ (headers_install model)
        ▼
  gv3libc  wraps them in POSIX functions (open/read/malloc/…) that programs call
```

**Constraint that shapes everything:** the libc only issues syscalls the kernel
actually implements. If a function would need a syscall we don't have, we don't
build it (yet). This is what keeps the rootfs runnable on our own kernel.

## Layout

The C library, dynamic linker, and coreutils are **repo-root providers**; this
dir holds the **assembler** + the product's device table.

```
  libc/                   ← PROVIDER (repo root): the C library + dynamic linker
    include/              OUR public headers (stdio.h, unistd.h, sys/stat.h, …)
                          — pure source; the UAPI snapshot is staged in build/
    src/
      crt/crt0.S          _start: capture sp, call __libc_start_main, _exit
      start.c             __libc_start_main: unpack argc/argv/envp, call main
      syscall.c           the raw svc entry points (__syscall3/6)
      unistd.c stdio.c stdlib.c string.c        the C library proper
      sbrk.c malloc.c     heap: sbrk over brk(2), free-list malloc on top
      stat.c dirent.c wait.c tty.c              syscall wrappers
    user.ld               static link script (link low at 0x10000)
    ld/                   the from-scratch dynamic linker (ld-gv3.so.1)
    test/                 libc + linker host unit tests (dynamic.sh)
  coreutils/              ← PROVIDER (repo root): one .c per program — sh, echo,
                            cat, pwd, ls, wc, hello  (was rootfs/bin/)

  projects/gameboy-v3/
    overlay/init.sh       /init — a #!/bin/sh script (kernel's shebang loader runs it)
    rootfs/               ← THIS DIR: the assembler
      rootfs.devs         device table (/dev/console, /dev/null) — the only nodes
      Makefile            builds the providers' sources + overlay-merges + packs cpio
      build/              ALL generated output (gitignored, `make clean` wipes it)
```

## Build & run

```bash
# from rootfs/ (after `make toolchain` built the musl toolchain)
make                     # build gv3libc (static) + all programs into build/
make BOARD=virt          # same, VFP-free, for the QEMU -M virt kernel/test harness
make rootfs              # package build/rootfs/ into build/rootfs.cpio.gz
make BOARD=virt qemu     # boot it on the custom kernel (needs: cd ../kernel && make BOARD=virt)
make clean               # rm -rf build/
```

- **`BOARD`** — `t113` (default) or `virt`. `virt` adds `-mgeneral-regs-only`
  (no VFP) to match the kernel's own user programs and the `-M virt` test harness.
- Boots to an interactive `gv3$` shell: `cd`/`pwd`/`exit`/`exec` builtins plus
  fork+exec of the coreutils. `/init` is `../overlay/init.sh`, launched via the
  kernel's `#!`-shebang path, which `exec`s the interactive shell as PID 1.

## The build pattern (staging tree + walk + device table)

Packaging follows the same shape as Buildroot / Yocto / the kernel's own
initramfs builder (and mirrors `forge/backends/rootfs.sh`):

1. **Auto-discover** — every `../../../coreutils/*.c` is a program, every
   `../../../libc/src/*.c` is a libc unit (`$(wildcard)` + `vpath`). Adding a
   coreutil is a one-file drop into the provider, no Makefile edit.
2. **Staging tree** — install programs into `build/rootfs/` (mirrors the final
   `/`), then **overlay-merge** `../overlay/` on top (its `init.sh` → `/init`).
3. **Walk it** — generate the `gen_init_cpio` listing by `find`-walking the tree.
4. **Device table** — append `rootfs.devs` (the `/dev` nodes that can't exist as
   real host files). This is the *only* hand-maintained part of the image spec.

`hello` is a demo (`DEMO_PROGS`) — built by `make all` but excluded from the image.

## How the pieces fit (static)

```
  program (coreutils/foo.c)  →  #include <unistd.h> etc.  →  calls read()/malloc()/…
        │  link: crt0.o + libc.a (code copied in)
        ▼
  static ET_EXEC at 0x10000  →  kernel fs/binfmt_elf.c loads it, jumps to _start (crt0)
        │  crt0 → __libc_start_main → main(argc, argv, envp)
        ▼
  libc wrappers issue `svc 0` (r7 = SYS_*)  →  kernel syscall dispatch
```

Dynamic adds a layer: the kernel loads BOTH the program and the interpreter, and
jumps to the interpreter (ld-gv3) first, which stitches in libc before the program runs:

```
  dynamic program (PT_INTERP=/lib/ld-gv3.so.1, DT_NEEDED=libc.so)
        │  kernel maps program + maps ld-gv3 at INTERP_BASE, builds full auxv,
        │  jumps to ld-gv3._start (NOT the program)
        ▼
  ld-gv3._start → _dl_main:  read auxv → map libc.so (per DT_NEEDED) →
        │                    resolve JUMP_SLOT/GLOB_DAT/RELATIVE → jump to AT_ENTRY
        ▼
  program _start (crt0) → main …  library calls resolve through the filled GOT
```

- **crt0** bridges the kernel's raw handoff (sp → argc/argv/envp block) to the C
  `main(argc,argv,envp)` convention. It's tiny and unavoidable — only the
  capture-sp step must be assembly; the rest is `start.c`.
- **malloc** sits on `sbrk`, which sits on the kernel's `brk(2)`. `sbrk` converts
  relative growth to the absolute address `brk` wants and returns the old break.

## Dynamic linking

`make LINK=dynamic` (do `make clean` when toggling) builds:
- `libc.so` — a **PIC** shared library exporting our libc symbols.
- programs linked against it: `ET_EXEC` with `PT_INTERP=/lib/ld-gv3.so.1`,
  `DT_NEEDED=libc.so`, a GOT/PLT, and ~40% smaller (libc no longer copied in).
- the image gains `/lib/libc.so` + `/lib/ld-gv3.so.1` (our dynamic linker).

At boot the kernel sees the program's `PT_INTERP`, maps **our** linker
`ld-gv3.so.1` (from [`libc/ld/`](../../../libc/ld/)) at `INTERP_BASE` and jumps to *it* first; the
linker maps `libc.so` (read from the program's `DT_NEEDED`), resolves every
`JUMP_SLOT`/`GLOB_DAT`/`RELATIVE` relocation, then jumps to the program's entry.
See [`libc/ld/PLAN.md`](../../../libc/ld/PLAN.md) for the staged design and [`libc/ld/src/dl_main.c`](../../../libc/ld/src/dl_main.c)
for the linker loop. The linker is ~5 KB, has **zero relocations of its own**
(so it needs no self-bootstrap), and its pure reloc/parse logic is host-unit-tested
(`libc/ld/test/run.sh`).

Inspect the structure:
```bash
make BOARD=virt LINK=dynamic
readelf -l build/bin/sh | grep INTERP         # the requested interpreter
readelf -d build/bin/sh | grep NEEDED         # the recorded dependency
readelf --dyn-syms build/libc.so | grep FUNC  # our exported symbols
```

### Testing

Two automated paths, both green:
- **`libc/ld/test/run.sh`** — host (x86) unit tests of the linker's pure logic:
  relocation arithmetic + SysV-hash symbol lookup (`test_reloc.c`), and parsing a
  REAL `libc.so`'s `.dynamic` (`test_parse.c`). Fast, no target needed.
- **`libc/test/dynamic.sh --gv3`** — full-system boot test. Runs a known-good
  musl-dynamic binary (proves the harness) AND our dynamic rootfs under a mainline
  reference kernel (built once into `build/refkernel/`). To boot the same rootfs
  on our OWN kernel: `make BOARD=virt qemu` after `make LINK=dynamic`.

| Runner | Status | Notes |
|---|---|---|
| **our custom `kernel/`** | ✅ works | the kernel now does `PT_INTERP`/`ET_DYN` loading (`fs/binfmt_elf.c`) + file-backed `mmap` (`mm/mmap.c`); our linker runs on it |
| **mainline Linux** (`build/refkernel/`) | ✅ works | full dynamic-linking support; the `libc/test/dynamic.sh` reference. Also cross-checks our ABI |
| **`qemu-arm` (user-mode)** | n/a here | only aarch64 is packaged on this host; not used |

## Honest caveats

- **QEMU `-M virt` verified, not silicon.** Same standing caveat as `kernel/`.
- **The shell is minimal** — no quoting, pipes, redirection, or globbing yet.
  `init.sh` uses unquoted `echo` for that reason.
- **The dynamic linker is scoped**: a single `DT_NEEDED` (our libc), eager binding
  (no lazy PLT), no symbol versioning/TLS/`dlopen`. Enough to run our rootfs; not
  a general-purpose `ld.so`. Non-PIE `ET_EXEC` programs + PIE handled; see `libc/ld/PLAN.md`.
- **Signals** in our kernel are stored, not delivered — fine for reaching a
  prompt; a shell relying on real signal delivery (job control) would need more.
