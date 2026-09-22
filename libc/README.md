# libc/ — a from-scratch C library + dynamic linker (the userspace C library)

This is our own C library, written **entirely from scratch** — no external
libc — plus a from-scratch dynamic linker ([`ld/`](ld/)). It's the C library
the from-scratch userland links against: with the sibling
[`coreutils/`](../coreutils/) provider (our `sh`, `cat`, `ls`, …) it forms a
minimal rootfs that is the third "drop-in replacement" in the project (alongside
the from-scratch [`bootloader/`](../bootloader) and [`kernel/`](../kernel)),
standing in for the reference musl+BusyBox rootfs.

The point is **understanding**: to see exactly what a libc, a crt0, a shell, and
a rootfs image are made of, by writing each one against a known syscall ABI.

> **How it's built into a rootfs (the package model).** libc + coreutils are repo-root
> sources built by the os build (`os/`) as graph nodes. `MACHINE=<m> make rootfs` walks the
> dependency graph: the selected libc (libc here, via the `libc` class) builds first, then each
> package in `PACKAGES` links against it (coreutils via the `compile-c` class), installing
> into a shared staging tree; the `rootfs` recipe (`os/meta/recipes-core/rootfs/`) then merges
> each package's staged files + the selected INIT provider's `/init` + the device table and packs
> the cpio — the SAME pack tail every package set uses.
> This doc describes the library itself; paths below are relative to this `libc/` dir.

## Status

- **Static (`make`) — working.** `libc` is a static archive (`libc.a`); the
  programs link it in and run on our custom `kernel/` (verified in QEMU `-M virt`).
- **Dynamic (`make LINK=dynamic`) — working.** PIC programs + a shared `libc.so`,
  loaded at runtime by our own from-scratch dynamic linker **`ld.so.1`** (see
  [`libc/ld/`](ld/)). Verified end-to-end on BOTH a mainline reference kernel AND our
  own `kernel/` (which now does `PT_INTERP` loading + file-backed `mmap`). The
  interactive shell + all coreutils run dynamically linked.
- Not yet run on real T113 silicon (same caveat as the whole project).

## The design principle: the kernel owns the ABI, the libc consumes it

Our programs make Linux syscalls (`svc 0`, number in `r7`) against **exactly the
ABI our kernel implements**. That ABI is single-sourced in the kernel's UAPI
headers and *consumed* by the libc — never re-typed:

```
  kernel/include/uapi/syscalls.h   syscall NUMBERS      ┐  the CONTRACT (kernel owns)
  kernel/include/uapi/abi.h        structs + constants  ┘
        │  `make headers` copies a SNAPSHOT into build/include/ (headers_install model)
        ▼
  libc  wraps them in POSIX functions (open/read/malloc/…) that programs call
```

**Constraint that shapes everything:** the libc only issues syscalls the kernel
actually implements. If a function would need a syscall we don't have, we don't
build it (yet). This is what keeps the rootfs runnable on our own kernel.

## Layout

This dir is the **libc source**; `coreutils/` is a sibling source; the os build engine
(`os/engine/`) builds them into a rootfs from the product's selection.

```
  libc/                   ← THIS PROVIDER: the C library + dynamic linker
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
    ld/                   the from-scratch dynamic linker (ld.so.1)
    test/                 libc + linker host unit tests (dynamic.sh)
  ../coreutils/           ← SIBLING SOURCE: one .c per program — sh, echo,
                            cat, pwd, ls, wc, mount
  ../os/meta/recipes-core/rootfs/
    recipe.sh             the rootfs recipe's pack step: merge staged files + device table +
                          walk + pack (runs after the package nodes populate the staging tree)
    rootfs.devs           the base device table (proc/sys/dev + console/null)
  ../os/meta/classes-recipe/
    libc.sh               the `libc` class: builds libc (libc.a/.so + crt0) for the custom libc
    compile-c.sh          the `compile-c` class (one .c -> one ELF); our coreutils
  ../projects/gameboy-v3/image/  ← the PRODUCT (selection): conf/local.conf picks providers +
                            PACKAGES; a product rootfs.devs is OPTIONAL (augments the base table;
                            gameboy-v3 ships none). /init comes from the selected INIT provider.
```

## Build & run

The rootfs is built through the os build, not from this dir. The selection lives in the
product's `image/conf/local.conf` — set `PROVIDER_libc=libc-custom`, `PACKAGES="coreutils"`,
and `LINKAGE` (static or dynamic). Then build from the product's `image/` dir, naming a
`MACHINE` (`t113` or `qemu`; toolchains build on demand — no separate prereq step):

```bash
cd projects/gameboy-v3/image
MACHINE=qemu make image        # → build/qemu/bundle/   (our libc + coreutils)
```

To build + boot JUST this rootfs under QEMU (the libc/linker dev loop), build the rootfs
node, then boot it with the `libc/test/dynamic.sh` harness. That harness is self-contained
(it knows nothing about the os build): it takes a mainline reference kernel and our rootfs
as paths and boots them under QEMU `-M virt`:

```bash
# build the VFP-free dynamic rootfs (ships ld.so.1) — LINKAGE=dynamic in local.conf + MACHINE=qemu (virt)
cd projects/gameboy-v3/image
MACHINE=qemu make rootfs       # → build/qemu/output/initramfs-*-dynamic.cpio.gz
```

- **`MACHINE`** — `t113` (the handheld) or `qemu`. `qemu` builds for QEMU `-M virt` with
  `-mgeneral-regs-only` (no VFP), matching the kernel's own user programs and the `-M virt`
  test harness. Required — no default.
- **`LINKAGE`** — `static` (default) or `dynamic` (PIC programs + shared `libc.so`,
  loaded by `ld.so.1`); set in `local.conf`.
- Boots to an interactive `gv3$` shell: `cd`/`pwd`/`exit`/`exec` builtins plus
  fork+exec of the coreutils. `/init` comes from the selected INIT provider — for this
  dev loop the `init-shell` recipe (`os/meta/recipes-core/init-shell/init`), a `#!/bin/sh`
  script that mounts the API filesystems and `exec`s the interactive shell as PID 1.

## The build pattern (staging tree + walk + device table)

Packaging follows the same shape as Buildroot / Yocto / the kernel's own initramfs
builder — and is UNIFIED in the os build: every package set flows through the same
graph (libc → packages → `rootfs` recipe) and its shared pack tail:

1. **Auto-discover** — every `../coreutils/*.c` is a program, every `src/*.c` is a
   libc unit. Adding a coreutil is a one-file drop into the source, no recipe edit.
2. **Staging tree** — the compile-c class installs programs into a staging dir
   (mirrors the final `/`).
3. **Merge** — the shared tail merges each selected package's staged files plus the
   selected INIT provider's `/init` into the staging tree.
4. **Device table + walk + pack** — the shared tail emits the base
   `os/meta/recipes-core/rootfs/rootfs.devs` (the `/dev` nodes + proc/sys/dev mountpoints
   that can't exist as real host files) plus any OPTIONAL product `rootfs.devs`
   appended, walks the tree, and packs the `gen_init_cpio` cpio.gz.

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
jumps to the interpreter (ld.so) first, which stitches in libc before the program runs:

```
  dynamic program (PT_INTERP=/lib/ld.so.1, DT_NEEDED=libc.so)
        │  kernel maps program + maps ld.so at INTERP_BASE, builds full auxv,
        │  jumps to ld.so._start (NOT the program)
        ▼
  ld.so._start → _dl_main:  read auxv → map libc.so (per DT_NEEDED) →
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
- programs linked against it: `ET_EXEC` with `PT_INTERP=/lib/ld.so.1`,
  `DT_NEEDED=libc.so`, a GOT/PLT, and ~40% smaller (libc no longer copied in).
- the image gains `/lib/libc.so` + `/lib/ld.so.1` (our dynamic linker).

At boot the kernel sees the program's `PT_INTERP`, maps **our** linker
`ld.so.1` (from [`libc/ld/`](ld/)) at `INTERP_BASE` and jumps to *it* first; the
linker maps `libc.so` (read from the program's `DT_NEEDED`), resolves every
`JUMP_SLOT`/`GLOB_DAT`/`RELATIVE` relocation, then jumps to the program's entry.
See [`libc/ld/PLAN.md`](ld/PLAN.md) for the staged design and [`libc/ld/src/dl_main.c`](ld/src/dl_main.c)
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
- **`libc/test/dynamic.sh`** — full-system boot test. Self-contained (knows nothing
  about the os build): give it `REFKERNEL` (a mainline `-M virt` zImage) and
  `DYNAMIC_INITRD` (our dynamically-linked rootfs cpio.gz) as paths, and it boots them
  under QEMU `-M virt`. PASS = our `ld.so.1` maps `/lib/libc.so`, relocates, and reaches
  the interactive `gv3$` prompt. To boot the same rootfs on our OWN kernel, point that
  kernel's `make BOARD=virt qemu` at the cpio (see [`kernel/`](../kernel)).

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
