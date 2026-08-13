# Linux Rootfs: Toward a Package Model

**Status:** design doc for UNBUILT work. It proposes evolving the rootfs from
today's coarse "pick one of two whole rootfs types" into a Buildroot/Yocto-style
**package set**, so a rootfs is composed from independently selected pieces (and a
"common component" can appear regardless of which libc / coreutils is chosen).
The work still to do is the "Remaining work" section; the "Implementation
contract" specifies its mechanics.

**Scope.** This concerns ONLY the Linux-class rootfs assembly in this repo (the
T113 build path: `forge/rootfs.mk`, the `forge/backends/rootfs-*` assemblers, and
the `LIBC`/`COREUTILS` provider axes). It builds on
[`LINUX_BUILD_SYSTEM_REFACTOR.md`](LINUX_BUILD_SYSTEM_REFACTOR.md) — read that
first for the provider/product/forge model and the vocabulary. The
microcontroller / FPGA parts of the repo (`rtos/`, `sim/`, `dbg/`, `cad/`) are
unaffected.

**Status: IMPLEMENTED.** The target model below is now the shipping design. The knobs
are `LIBC=custom|musl` (the substrate) and `PACKAGES=<name>…` (the install set); the
old `COREUTILS` axis is gone — the base userland is a plain package (`packages/coreutils/`,
`packages/busybox/`). The engine is ONE assembler, `forge/backends/rootfs-assemble.sh`:
it builds the libc substrate (`build-substrate.sh` for `LIBC=custom`; a no-op for musl)
then each package in `PACKAGES` against it via its build style (`build-compile-c.sh`,
`build-kconfig.sh`), and finishes through the shared tail `forge/backends/rootfs-pack.sh`
(overlay-merge → device-table → walk → pack cpio). `forge/providers.mk` no longer collapses
into a `BUILD_ROOTFS` flavor; it resolves `PACKAGES` and does a per-package libc-surface
pre-check. The sections below are kept as the DESIGN RATIONALE that produced this; where
they say "today does X" / "will do Y", read them as the reasoning, not the current state.

## The problem this solves

Today the rootfs is a **coarse, monolithic choice**. `forge/providers.mk`
collapses the two independent axes `LIBC` and `COREUTILS` into a single
`BUILD_ROOTFS` that must be one of two "pure" combos:

```
  LIBC=custom + COREUTILS=custom   -> BUILD_ROOTFS=scratch   (our libc + our coreutils)
  LIBC=musl   + COREUTILS=busybox  -> BUILD_ROOTFS=busybox   (musl + BusyBox)
  anything else                    -> $(error MIXED rootfs, not yet buildable)
```

and `forge/rootfs.mk` **dispatched to one of two flavor populators** (the product's
own rootfs Makefile for scratch, `forge/backends/rootfs.sh` for busybox) — fixed
provider pairings. Two consequences:

1. **You cannot mix providers** — e.g. "musl libc + our coreutils," or "our libc +
   BusyBox," are hard errors. The libc and coreutils axes are advertised as
   independent but aren't actually composable (the `BUILD_ROOTFS` collapse forces
   the two pure combos).
2. **A component you want in every rootfs can only be a *prebuilt* file.** A
   prebuilt file/binary goes in the `overlay/` (which the shared tail already
   applies to both flavors). But a component that must be *built* — and linked
   against the selected libc — has no home: the two populators are fixed pairings,
   not an install set you can add to.

## How Buildroot and Yocto avoid this (the model to emulate)

The key realization: **in Buildroot/Yocto there is no "busybox rootfs" vs "custom
rootfs" as monolithic alternatives.** A rootfs is a **set of packages**, and
BusyBox is just *one* package among many. The libc is a *separate* package every
other package links against.

- **Buildroot:** `package/<name>/` is a generic recipe per component
  (`package/busybox/`, `package/dropbear/`, …). An image is "install this set of
  packages" (`BR2_PACKAGE_*`). The libc (`toolchain`) is chosen once; every
  package links against it. Prebuilt files go in a verbatim **rootfs overlay**
  (`BR2_ROOTFS_OVERLAY`) copied on top after packages install.
- **Yocto:** each component is a recipe (`*.bb`); an image recipe lists
  `IMAGE_INSTALL = "busybox mytool dropbear"`. The libc is a separate recipe.
  Board/product tweaks are `.bbappend` files layered onto the generic recipe.
  Extra files come via an image recipe or a small file-install recipe.

So the "common component in every rootfs" question **does not even arise as a
special case**: it's just "a package in the install set" (if it must be built) or
"a file in the overlay" (if it's prebuilt) — both orthogonal to whether BusyBox is
also installed. There is no monolithic rootfs to be "inside" or "outside" of.

Two mechanisms, two jobs:
- **overlay** = prebuilt files copied verbatim onto the assembled tree (config,
  scripts, data, a static binary that doesn't care about the libc).
- **package** = something the build system *builds* (and links against the
  selected libc), installed into the tree.

## Target model for this repo

Replace the coarse `BUILD_ROOTFS ∈ {scratch,busybox}` with a **composed rootfs**.

### Three kinds of thing (the key mental model)

A rootfs isn't one axis — it's a few *categories*, and Buildroot/Yocto treat each
differently. The test that sorts a component into its category: **"can you have two
of them INSTALLED at once?"**

```
  1. SUBSTRATE          chosen ONCE; everything links/depends on it.   → a hard axis
                        e.g. libc.  Buildroot: the toolchain/libc choice
                        (BR2_TOOLCHAIN_*) / Yocto: TCLIBC.

  2. PLAIN PACKAGE      additive; install as many as you like, no      → the PACKAGES set
                        install-time exclusivity.  This is MOST things —
                        INCLUDING the base userland: busybox AND coreutils
                        can both be installed in one tree (PATH just shadows
                        the duplicates), so "busybox vs our coreutils" is NOT
                        exclusive; it's set membership.  e.g. busybox,
                        coreutils, dropbear, mtd-utils.

  3. RUNTIME ROLE       a slot only ONE package can fill AT RUNTIME even   → a role selector
                        if several are installed. The clean example is init
                        (only one process is PID 1: systemd vs busybox-init
                        vs sysvinit). Buildroot: BR2_INIT_* / Yocto:
                        VIRTUAL-RUNTIME_init_manager. A role selector picks
                        WHICH installed package fills the slot; it does not
                        make the others un-installable.
```

The sorting test is strict, and it moves the base userland into PLAIN: `libc` is the
substrate (a true top-level axis); `busybox`/`coreutils`/`dropbear`/`mtd-utils` are
all **plain packages** you compose into an install set — *including the base
userland*, because busybox and coreutils can physically coexist (Buildroot lets you
select both; the applet-vs-binary shadowing is a PATH detail, not exclusivity). Two
consequences worth stating plainly:

- **The base userland is NOT a role.** An earlier peer `COREUTILS` axis was wrong
  (coreutils isn't the substrate) — but so is treating "busybox vs coreutils" as a
  mutually-exclusive role. It's just which package(s) you put in `PACKAGES`, which
  is exactly Buildroot's model and precisely what the "BusyBox is just a package"
  payoff below means.
- **Init IS a genuine role, but today it isn't a package at all.** Our PID 1 is
  `overlay/init.sh` — a prebuilt shell script in the `overlay/` (the prebuilt-file
  case), not a built package. So an `INIT=` role selector is premature: there is one
  init and it's an overlay file. Add `INIT` as a role only when a second init
  (busybox-init/systemd) actually exists to pick — same "sample size of one"
  discipline as the `soc/` tier. Until then the model is just two axes: **libc
  (substrate) + PACKAGES (everything else, base userland included).**

### The product config

Two axes — `LIBC` (substrate) and `PACKAGES` (everything else). There is ONE schema,
not a role-heavy variant and a lean variant; the base userland is just a package in
the set.

```makefile
# in the product's config.mk (illustrative)
LIBC     ?= custom       # SUBSTRATE (custom | musl) — the C library ALL packages link against
PACKAGES ?= coreutils    # the install set (plain, additive): the base userland lives
                         #   HERE (coreutils | busybox | coreutils busybox), plus adds
                         #   like "dropbear mtd-utils". Built against the selected libc.
# overlay/  is applied unconditionally (already exists in the product) — prebuilt files
#           (incl. today's PID-1 overlay/init.sh) that need no build.
```

The generic assembler iterates `$(PACKAGES)`: for each, build its source against the
selected libc → install into the staging tree; then overlay-merge → device table →
walk → pack. There is no separate role loop — the base userland is not special, it's
just the package(s) you listed. (A genuine RUNTIME role like `INIT=` is deferred
until a second init exists — see the taxonomy above; today PID 1 is `overlay/init.sh`.)

- **`LIBC` (substrate)** — every package is built *against the selected libc*, not
  shipped as a fixed pairing (`LIBC=musl` means "build the set against musl"). A
  package builds only if the chosen libc covers its *surface*: musl covers anything;
  the custom libc (gv3libc) covers what we've implemented so far, and grows
  demand-driven (see "`PKG_DEPENDS := libc` is a SURFACE requirement" below). So
  `LIBC=custom` + `PACKAGES=busybox` ("build BusyBox against gv3libc") builds only
  once gv3libc covers BusyBox's surface — a worklist, not a given (and a large one:
  BusyBox needs buffered stdio/getopt_long/termios/regex, so in practice BusyBox
  runs on `LIBC=musl` — the demand-driven loop's realistic targets are smaller
  packages). Today's custom rootfs is `LIBC=custom` + `PACKAGES=coreutils`.
- **`PACKAGES` (plain, additive)** — the install set; the "common component" answer
  for anything that must be *built* (base userland included), orthogonal to nothing
  — you compose the set freely (subject only to the per-package libc-surface check).
- **`overlay/`** — the answer for **prebuilt** common files (config, scripts, the
  PID-1 `init.sh`); applied on every path, no build.

### What a "package" is (the design)

Strip away the framework machinery and a package in *any* system (Buildroot,
Yocto, Debian, Arch) is the same thing: **a component described declaratively
enough that a generic engine can build + install it without knowing what it is.**
Every one expresses the same FIVE facts:

```
  1. IDENTITY      name (+ version)
  2. SOURCE        where the code is (a local dir, or a URL + hash)
  3. DEPENDENCIES  what must exist first — esp. the libc it links against
  4. BUILD         how to turn source -> artifacts (configure/compile)
  5. INSTALL       which artifacts land in the rootfs, at what paths
```

Buildroot's `foo.mk` is these five as Make vars (`FOO_VERSION`, `FOO_SITE`,
`FOO_DEPENDENCIES`, `FOO_BUILD_CMDS`, `FOO_INSTALL_TARGET_CMDS`); Yocto's `foo.bb`
is the same in bitbake (`SRC_URI`, `DEPENDS`, `do_compile`, `do_install`). Same
skeleton, different syntax.

For forge, a package is **a directory with a `package.mk`** (mirroring Buildroot's
`foo.mk`), living in a `packages/` index alongside the special `libc/` provider:

```
  embedded/
    packages/                 ← the package index (like Buildroot's package/)
      coreutils/package.mk      our from-scratch utils, now a PACKAGE
      busybox/package.mk        the OSS alternative, ALSO just a package
      dropbear/package.mk       a future add (SSH), same shape
    libc/                     ← NOT a package: the SUBSTRATE every package links (see below)
```

```makefile
# packages/coreutils/package.mk  (illustrative — NOT built yet)
PKG_NAME    := coreutils
PKG_SOURCE  := $(REPO_ROOT)/coreutils              # local; an OSS pkg uses fetch URL + hash
PKG_DEPENDS := libc                                # links the SELECTED libc provider
PKG_BUILD   := $(FORGE)/backends/build-c-pkg.sh    # generic recipe: compile *.c vs $(LIBC_SRC)
PKG_INSTALL := /bin                                # where its binaries go in the rootfs
```

The product's `config.mk` gains the **install set** (Buildroot's `IMAGE_INSTALL`):

```makefile
LIBC     ?= custom         # the libc EVERY package links against (the substrate; custom|musl)
PACKAGES ?= coreutils      # the set to build + install (add: busybox, dropbear, mytool…)
```

and `forge/rootfs.mk` becomes a generic loop (replacing the scratch/busybox
dispatch): for each pkg in `$(PACKAGES)` → build its source against `$(LIBC_SRC)`
→ install its artifacts into the staging tree → then overlay-merge → device table
→ walk → pack. This is the from-scratch analogue of the existing `lib.sh`-reads-
`versions.env` pattern, extended from "the product" to "each package."

### Two structural ideas that matter most

- **BusyBox stops being a "mode."** Today `BUILD_ROOTFS=busybox` selects a whole
  different assembler. In the package model, `busybox` is just a package you can
  put in the set: `PACKAGES=busybox`, or `PACKAGES=coreutils`, or even
  `PACKAGES=coreutils dropbear`. The coarse either/or dissolves into set
  membership — the single biggest conceptual payoff, and exactly Buildroot's model.
- **`libc` is deliberately NOT a package — it's the substrate.** Every package
  links it, so it's chosen once (`LIBC=`) and threaded into every package's build
  via `PKG_DEPENDS := libc`. This mirrors Buildroot (the toolchain/libc is selected
  once, not an `IMAGE_INSTALL` entry) and it's precisely WHY the MIXED-rootfs
  problem exists today: coreutils/busybox are currently pre-*paired* with a libc
  instead of built against the *selected* one. "Build me against whatever libc is
  selected" makes every `LIBC × package` combination *expressible* — the engine
  stops FORBIDDING mixes. Whether a given mix *links* is then a separate question
  answered by the libc's surface (below): musl covers anything; gv3libc covers a
  package only once we've grown its surface to meet it. So the guard isn't removed,
  it's REFINED — from "these two combos only" to "build it, and if the selected libc
  can't satisfy the package's surface, fail with a clear surface-gap message" (a
  worklist, not a hard pairing). "Valid by construction" applies to the wiring, not
  to every link succeeding.

Note: your providers are already ~80% packages — `coreutils/` is a dir of source
that gets built + installed, i.e. a package missing only its `package.mk`
declaration. The work is adding that thin declarative wrapper + the generic
build-install loop, not inventing a package system from nothing.

### `PKG_DEPENDS := libc` is a SURFACE requirement — and gv3libc grows to meet it

The `libc` dependency isn't binary ("has a libc / doesn't"). A package needs a
particular *surface* of the C library — the specific functions and headers it
references. Two libcs satisfy surfaces very differently:

- **`LIBC=musl`** — a complete libc. It covers essentially any upstream package's
  surface out of the box. This is why real OSS packages (`mtd-utils`, `dropbear`,
  …) "just work" on the musl axis, exactly as they do in Buildroot/Yocto (which
  only ever pair packages with complete libcs).
- **`LIBC=custom`** (gv3libc) — our from-scratch teaching libc, deliberately
  minimal (today: `stdio` [UNBUFFERED — no `FILE*`, a small `printf` conversion
  set], `stdlib` [incl. a working `malloc`/`free`/`calloc`/`realloc` over `sbrk`],
  `string`, `unistd`, `fcntl`, `dirent`, `sys/{stat,wait,mount}`, `tty`, `errno`
  + a small syscall set). It does NOT yet cover a full package's surface — the gaps
  are less "no allocator" (we have one) and more buffered stdio / `getopt_long` /
  locales / the long tail of string+stdlib helpers.

The key point: **a package that won't build against gv3libc is not a dead end — it
is a WORKLIST.** This is the same demand-driven loop the project already ran twice:

```
  kernel bring-up:  boot unmodified BusyBox → hit -ENOSYS → implement that syscall → repeat
  libc bring-up:    build a package         → hit undefined-symbol → implement it   → repeat
```

The *unmodified package is the specification*: the compiler/linker errors
(`undefined reference to 'getopt_long'`, `implicit declaration of 'ioctl'`) are a
precise, finite TODO list of exactly the surface that package needs — you don't
guess or implement all of POSIX speculatively. Each function you add has a proven
consumer (the same "don't build the abstraction until a real consumer defines it"
discipline applied to libc symbols). Grow gv3libc package-by-package.

**But the gaps are not uniform — and knowing which to fill is the real skill:**

- **Shallow (good targets):** pure, self-contained, no kernel dependency —
  `getopt`/`getopt_long`, `strtol`, `qsort`, `memmove`, more `printf` conversions,
  `atexit`. An instructive afternoon each; high learning-per-function.
- **`ioctl`/UAPI structs — copy, don't invent:** when a package needs
  `struct mtd_info_user` + `MEMGETINFO`, copy them verbatim from the kernel UAPI
  (`<mtd/mtd-abi.h>`) into our UAPI headers — exactly as we copied syscall numbers
  and `struct stat` into `gv3_abi.h`. Bounded and mechanical; grows the UAPI too.
- **Deep (icebergs — a decision, not an auto-fill):** buffered `stdio` (`FILE`,
  `fopen`/`fread`/`ungetc`/seeking) is a real subsystem; **locales**, **wchar**,
  **dlopen**, **threads** drag in large machinery; and some deps aren't libc at all
  (`mtd-utils` needs **zlib** — a whole separate library). When a package demands a
  deep subsystem, that's a decision point, not an automatic "implement the gap."

**The graduation point (be honest about it):** gv3libc exists to *learn*, not to
be a production libc. Grow it for the functions that are *instructive* (the core
C/POSIX surface: more `string`/`stdlib` helpers, `getopt`, buffered stdio, the
`ioctl`/`mmap` glue — note the allocator already exists, so it's stdio and the
string/stdlib tail that are the real next targets). When a package's demanded
surface exceeds what's worth hand-implementing
(locales, threads, a bundled zlib), that's the signal to either skip that package
on the `LIBC=custom` axis or run it on **`LIBC=musl`** instead. The dual
`LIBC=custom|musl` axis exists precisely to let you make that call *per package*:
custom (gv3libc) for the ones whose gaps teach you something, musl as the escape
hatch for the rest. The engine can
even make this explicit — a package may declare a minimum libc surface and the
resolver refuses `LIBC=custom` with a clear message (same spirit as the MIXED-rootfs
guard) rather than emitting a wall of link errors.

### What to deliberately OMIT (understanding the model = knowing its edges)

Real package systems carry a lot you should NOT build here — naming it is part of
knowing the model:

- **Dependency resolution / topological build order** — Buildroot computes order
  from `FOO_DEPENDENCIES`. With ~3 components, a flat `PACKAGES` list + "libc
  first" suffices. Don't write a solver.
- **Multiple concurrent versions of one package** — Buildroot/Yocto can't really
  either; skip. (Each package pins ONE version — see the fetch contract below.)
- **Removal, reverse-deps, `Config.in` menus, per-package patches/hooks** — all
  Buildroot machinery with no payoff at this scale.

The minimal version is exactly: **a five-fact `package.mk` + a `PACKAGES` install
list + a generic build-install loop + libc-as-substrate.** That is the model made
concrete without the industrial-scale bits.

## Implementation contract (Buildroot-derived — the concrete mechanics)

The sections above are decision-complete but leave the *mechanics* as `# illustrative`.
This section specifies them, each grounded in how Buildroot solves the same problem
(Buildroot is the closer model: Make-based like forge, and its `pkg-<style>.mk` +
`TARGET_DIR`/`STAGING_DIR` + toolchain-wrapper trio maps almost 1:1 onto forge).
Four of the five below reduce to **two load-bearing mechanisms**: a per-libc
**CC profile** (Buildroot's toolchain wrapper) and a shared **STAGE + staging-sysroot**
(Buildroot's `TARGET_DIR` + `STAGING_DIR`).

### C1. Build styles — `PKG_TYPE` + one backend per style

Mirrors Buildroot's package *infrastructures* (`$(eval $(autotools-package))`) /
Yocto's `inherit`. A package declares HOW it builds; the engine has one backend per
style. Do NOT put build logic in a package — only the style + its deltas.

```makefile
PKG_TYPE := autotools    # compile-c | autotools | kconfig | cargo
```

- `forge/backends/build-<type>.sh` (or `.mk`) implements each style's
  configure→build→install for the standard case:
  - **compile-c** — our coreutils shape: `$(CC) *.c` → binaries. (The product's
    old rootfs-Makefile logic, generalized to one package dir.)
  - **autotools** — `./configure --host=$(TARGET) --prefix=/usr && make && make install DESTDIR=$(STAGE)`. (mtd-utils, dropbear.)
  - **kconfig** — `make <defconfig> && make && make install`. (busybox.)
  - **cargo** — `cargo build --release --target <triple>`, then install the binary.
- A package supplies only deltas (`PKG_CONF_OPTS`, extra `make` args), like
  Buildroot's `FOO_CONF_OPTS`.

Start with just **compile-c** (covers our coreutils today) and add styles as real
packages demand them — same demand-driven discipline as libc growth. Don't
pre-build cargo/kconfig backends speculatively.

### C2. The build/install contract — per-package build dir + install into `STAGE`

Mirrors Buildroot's `$(@D)` (per-package build dir) → install into `$(TARGET_DIR)`
(the staging rootfs). **Install is arbitrary commands, not a single dest path** —
this is what lets a package place binaries in `/usr/sbin` AND a config in `/etc`
(a flat `PKG_INSTALL := /bin` can't).

The engine invokes a build-style backend with this **environment contract**:

```
  INPUTS (engine → backend):
    PKG_DIR        the package's source dir (resolved from PKG_SOURCE; see C5)
    PKG_BUILD      a scratch build dir for this package's objects  (= Buildroot $(@D))
    STAGE          the staging rootfs tree, mirrors "/"            (= Buildroot $(TARGET_DIR))
    SYSROOT        staging sysroot for build-time headers/libs     (= Buildroot $(STAGING_DIR))
    CC, CFLAGS, LDFLAGS, TARGET   the per-libc CC PROFILE (see C3) — already carries
                                  the selected libc; backend NEVER hardcodes libc flags
  OUTPUT (backend → engine):
    installs its target files under $(STAGE) at their final paths; exit 0 = success.
    (build-time-only artifacts — headers/libs a later package needs — go to $(SYSROOT).)
```

`PKG_INSTALL` in `package.mk` is then either a simple `dest-dir` (for compile-c:
"put my binaries here") OR, for anything non-trivial, an explicit install-commands
hook the backend runs (`PKG_INSTALL_CMDS`), exactly like Buildroot's
`FOO_INSTALL_TARGET_CMDS`. The generic loop just: resolve source → build into
`PKG_BUILD` → install into `STAGE`/`SYSROOT` → next package → then the shared
`rootfs-pack.sh` tail (overlay-merge → device-table → walk → pack).

### C3. Linking the selected libc — a per-libc CC profile (the toolchain wrapper)

Mirrors Buildroot's **toolchain wrapper** (a `cc` with sysroot + target flags +
libc baked in) / Yocto's `TCLIBC`. The libc choice is made TRANSPARENT to the
package's *build recipe*: it calls `$(CC)`; the profile already links the right
libc. One knob (`LIBC=`) swaps the profile; a package's recipe needs no per-libc
edit — it rebuilds against whichever libc is selected.

SCOPE OF "transparent" (read with the caveat below): this holds cleanly for the
**compile-c** style (our coreutils — just `$(CC) *.c`). It does NOT make a package
*link successfully* on any libc: the `custom` profile is `-nostdlib`/`user.ld`,
which is incompatible with autotools feature-probing and only exposes gv3libc's
(small, growing) surface — so `PKG_TYPE=autotools`/`kconfig` packages in practice
run on `LIBC=musl`, and the resolver rejects `custom` for them (C4/surface check).
"Transparent" = the recipe doesn't hardcode libc flags; it does NOT = "every
package builds on every libc." That link question is the surface worklist (below).

Two profiles (the engine selects one from `LIBC`), grounded in today's code:

```
  LIBC=musl    (normal, complete-libc link):
    CC      = arm-buildroot-linux-musleabihf-gcc
    CFLAGS  = --sysroot=<musl sysroot> $(ARCH_FLAGS)
    LDFLAGS = (normal; musl crt + libc via the sysroot)
    → autotools ./configure works; standard startup.

  LIBC=custom  (gv3libc: -nostdlib + our crt0 + our linker script):
    CC      = arm-buildroot-linux-musleabihf-gcc   (driver only — links NONE of musl)
    CFLAGS  = -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-stack-protector \
              $(ARCH_FLAGS) -I<libc>/include -I<staged UAPI>
    LDFLAGS = -nostdlib -T <libc>/user.ld <crt0.o> <libc.a>  (static; crt0 built
              from <libc>/src/crt/crt0.S, libc.a from <libc>/src/*.c)
              (or -fPIC + libc.so for LINK=dynamic)
    → this is exactly the old product rootfs-Makefile logic, now lifted into the
      reusable cc-profile.sh + build-substrate.sh.
```

A build-style backend (C1) uses `$(CC)`/`$(CFLAGS)`/`$(LDFLAGS)` and is thus
libc-agnostic. IMPORTANT: the `custom` profile's `-nostdlib`/`user.ld` model is
incompatible with autotools' feature-probing (and gv3libc's surface is too small
anyway — see the SURFACE section). So in practice `PKG_TYPE=autotools` packages
run on `LIBC=musl`; the resolver should reject `custom` for them (C4/surface check).

### C4. Dependencies — `PKG_DEPENDS`, build-first, discover via the staging sysroot

Mirrors Buildroot's `FOO_DEPENDENCIES` + `$(STAGING_DIR)` / Yocto's `DEPENDS`. Two
parts, both minimal:

- **Ordering:** a package's non-libc `PKG_DEPENDS` build BEFORE it. Minimal version:
  no solver — build `PACKAGES` (and their listed deps) in a simple pass assuming the
  human ordered them into a DAG (Buildroot *does* topo-sort, but from the same flat
  per-package lists; you can defer the sort until a real diamond dependency appears).
- **Discovery:** a dep installs its build-time headers/libs into the shared
  **`$(SYSROOT)`** (staging sysroot, distinct from `$(STAGE)` which is what ships),
  and the CC profile's `CFLAGS`/`LDFLAGS` include `-I$(SYSROOT)/include -L$(SYSROOT)/lib`.
  So `mtd-utils`' `./configure` finds `zlib` automatically — no per-package wiring.

`libc` in `PKG_DEPENDS` is special (the substrate, C3) — it's not built as a
package; it selects the CC profile. Non-libc deps (zlib) ARE packages built into
`$(SYSROOT)`. (Yocto's `DEPENDS` vs `RDEPENDS` build/runtime split is more than
needed now; note it as the "if this grows" direction.)

### C5. Fetch + verify — reuse the engine fetch; version/hash live IN the package

Mirrors Buildroot (`FOO_VERSION`/`FOO_SITE` + a `.hash` file) / Yocto (`SRC_URI` +
checksums, version in the recipe filename). **DECISION: per-package, community
style** — a package owns its version + hash, NOT the product's `versions.env`.
This is now uniform across ALL components: busybox's pin lives in
`packages/busybox/package.mk`, and the kernel/U-Boot pins moved the same way into
their provider recipes (`providers/<role>/<impl>/provider.mk` — see
`docs/LINUX_PROVIDER_RECIPE_MODEL.md`). `versions.env` no longer holds any version
pins; it retains only two artifact *filenames* (`UBOOT_IMAGE`, `INITRAMFS_IMAGE`).

```makefile
# a LOCAL-source package (our coreutils): no fetch
PKG_SOURCE  := $(REPO_ROOT)/coreutils

# an OSS package (mtd-utils): URL + pinned version + hash, IN the package.mk
PKG_VERSION := 2.1.6
PKG_SITE    := ftp://ftp.infradead.org/pub/mtd-utils
PKG_SOURCE  := mtd-utils-$(PKG_VERSION).tar.bz2
PKG_SHA256  := <sha256>
```

- The engine's generic fetch step: if `PKG_SITE` is set → download `PKG_SOURCE`
  from `PKG_SITE`, verify `PKG_SHA256`, extract → `PKG_DIR`. If `PKG_SOURCE` is a
  local dir → use it directly, skip fetch.
- **Reuse existing machinery, don't reinvent:** `forge/backends/rootfs.sh` already
  has working curl + `sha256sum --check` tarball fetch (and `lib.sh` has
  `git_clone_pinned` for git). Lift the tarball fetch/verify/extract out of
  `rootfs.sh` into a shared `lib.sh` helper (e.g. `fetch_verify <url> <sha> <dest>`)
  that both busybox-as-a-package and any OSS package call. This is a small
  extraction, not new logic.

### Contract summary (the two mechanisms behind four gaps)

```
  per-libc CC PROFILE (C3)  → makes libc transparent; solves linking, enables dep discovery
  STAGE + SYSROOT (C2,C4)   → where builds install (→ image) AND where deps expose headers
  PKG_TYPE build styles (C1)→ named backend per build style (compile-c first; add on demand)
  per-package fetch (C5)    → version+hash in package.mk; reuse lib.sh fetch_verify
```

Build these only when the remaining work below is actually justified — not
speculatively. Start with the `compile-c` style + local `PKG_SOURCE` (covers our
coreutils becoming a package with zero new fetch/style work), then add
autotools/fetch when the first real OSS package (e.g. mtd-utils) needs it.

## Remaining work (each step builds green)

Verify with the QEMU golden tests (`kernel/test/golden.sh`, whose `busybox`/`dynamic`
cases boot the real initramfs) after each. Both steps use the mechanics in the
"Implementation contract" above.

- **Step 1 — make libc the substrate; build the base userland against it.** Build
  the base userland against the *selected* libc (via the per-libc CC profile, C3)
  instead of the fixed `scratch`/`busybox` pairing. This REFINES the
  `MIXED rootfs not yet buildable` guard in `providers.mk` from "these two combos
  only" into a per-package **libc-surface check**: the engine stops forbidding
  mixes; a combo the selected libc can't cover (e.g. `LIBC=custom` + BusyBox) fails
  with a clear surface-gap message instead of a blanket refusal. This is the
  "become composable" core, and it needs ONLY C3 (the CC profile) — not the full
  C1–C5 machinery. Scope note: renaming today's `COREUTILS` knob is a cross-cutting
  edit (it's a command-line var threaded through `providers.mk` + both backends), so
  Step 1 may keep the `COREUTILS` name and just change its *meaning* (built against
  the selected libc), deferring any rename to Step 2.

- **Step 2 — the `package.mk` convention + an install set (`PACKAGES`).** Introduce
  the `package.mk` convention (see "What a package is" + the Implementation
  contract), and add a product-level `PACKAGES` list the assembler iterates —
  building each against the selected libc and staging it. This is the full
  Buildroot-style "rootfs = set of packages," which dissolves the two flavor
  populators (busybox and coreutils become plain packages in the set). Defer until
  there's a real second built component to install (same "sample size of one"
  discipline as the `soc/` tier). A genuine `INIT=` runtime-role selector is a
  further, separate deferral — only when a second init exists to pick.

## Honest cautions

- **Don't build the package machinery speculatively.** The remaining steps (libc
  substrate; then the `package.mk` + `PACKAGES` convention) are only justified once
  you actually want a non-pure libc × userland combo or a third installable
  component — until then the coarse model is fine, and the `providers.mk` guard
  *correctly* errors instead of silently building something wrong. Note Step 2's
  full machinery (C1–C5) is deliberately over-specified relative to the plan: it's
  a reference for WHEN that day comes, not a licence to build it now. Step 1 needs
  only C3.
- **The overlay/package split is the durable idea, not the implementation.** Even
  if the Make details change, keep the two jobs distinct: *overlay = prebuilt files
  copied verbatim; package = something built (against the chosen libc) and
  installed.* Conflating them is what makes rootfs build systems confusing.
- **A component that links libc is NOT "the same component" across libcs.** "Our
  tool in both the busybox and custom rootfs" means *rebuilding* it against
  whichever libc the base uses — it's one source, two builds. Only a truly
  static/independent binary (or a non-executable file) is literally identical in
  both, and that's the overlay case (already handled — the overlay merge works on
  both flavors). This is why making libc the substrate that packages build against
  (Step 1 of the remaining work) is the prerequisite for a *built* "common component."
- **This is Buildroot's job (again).** A full package system with dependency
  resolution, versioning, and hundreds of recipes is exactly Buildroot/Yocto. We
  build a minimal version because *building it* is the learning goal; if the goal
  were shipping many products, adopting Buildroot and packaging our providers as
  Buildroot packages is the pragmatic path.

## One-paragraph summary

Today's rootfs is a coarse pick of one of two monolithic, pre-paired assemblers
(`scratch` = custom+custom, `busybox` = musl+busybox), with mixing disallowed and no
clean way to add a *built* component to *every* rootfs (only prebuilt overlay files).
Buildroot/Yocto avoid this by making a rootfs a **set of packages** (BusyBox is just
one) plus a verbatim **overlay** for prebuilt files — so "a common component in
both" is simply "a package in the install set" or "a file in the overlay,"
orthogonal to the userland choice. Model a rootfs as two axes (plus one deferred
role): the **libc substrate** (chosen once, everything links it — a hard axis) and
**plain packages** (the additive install set — the base userland, busybox and
coreutils alike, lives here; they can coexist). A genuine RUNTIME role (init: one
PID 1) exists in principle but is deferred — today's init is an `overlay/` file, not
a package, so there's nothing to select yet. The remaining work: (1) build the base
userland against the *selected* libc so the substrate composes with any userland —
this REFINES the MIXED guard into a surface check (build it; fail clearly if the
selected libc can't cover the package's surface) rather than removing it; (2) add a
product `PACKAGES` install set + the `package.mk` convention (busybox and coreutils
become packages) — deferred until a real second built component needs it. Keep the
durable distinction throughout: overlay = prebuilt files; package =
built-against-the-chosen-libc and installed.
