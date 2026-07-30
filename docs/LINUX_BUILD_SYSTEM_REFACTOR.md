# Linux Build-System Refactor: Providers, Products, and a Shared Forge

**Status:** proposal / design doc. Nothing has been moved yet. This describes a
target directory structure and a phased plan to reach it safely.

**Scope — READ THIS FIRST.** This refactor concerns *only* the **Linux-class
build path** in this repo: the parts that build a bootable Linux(-ABI) SD image
for the Allwinner **T113** SoC (a Cortex-A7 application processor) — i.e. a
kernel, a bootloader, a C library, userland utilities, a root filesystem, and an
SD-card image. The repo *also* contains microcontroller and FPGA work
(bare-metal RTOS, simulators, FPGA/PPU cores, hardware/CAD). **Those are NOT
affected by anything here** and must not be moved or restructured by this plan.
If a directory is not named in the "What moves where" inventory below, leave it
alone.

## 0. Orientation for a reader with no prior context

Enough background to execute this doc cold:

- **The repo** is a single git monorepo rooted at `embedded/`. Today its
  top-level dirs include: `projects/` (individual builds), `rtos/`, `sim/`,
  `dbg/`, `cad/`, `docs/`. `rtos/`, `sim/`, `dbg/`, `cad/` are microcontroller /
  FPGA / tooling work — **out of scope**.
- **The subject of this refactor** is `projects/gameboy-v3/` — a handheld built
  on the T113. Today it is one directory containing *everything*: a from-scratch
  bootloader (`bootloader/`), a from-scratch Linux-ABI kernel (`kernel/`), a
  from-scratch userspace (`rootfs/` — a C library, a dynamic linker, coreutils,
  a shell), and a flat `scripts/` dir of numbered shell scripts
  (`00-toolchain.sh` … `04-image.sh`) that orchestrate fetching/building/imaging,
  plus loose config (`*.dtsi`, `boot.cmd`, `init`) and dev tools (`flash.sh`,
  `t113power.sh`).
- **Key property already achieved:** the from-scratch kernel, bootloader, and
  libc are **drop-in ABI-compatible** with their open-source equivalents
  (mainline Linux, U-Boot, musl). The project has already demonstrated running
  its custom rootfs on mainline Linux AND running a BusyBox/musl rootfs on the
  custom kernel — the components are interchangeable. This refactor turns that
  interchangeability into an explicit build-time *provider selection*.
- **"Provider vs. product vs. engine":**
  - a **provider** is a reusable software component (a kernel, a libc, …) — the
    *implementation* of a layer;
  - a **product** is a specific device build (`gameboy-v3`) that *selects* which
    provider implements each layer and adds its board-specific config;
  - the **engine** (called `forge/` here) is the reusable build orchestration
    that turns "selected providers + board config" into a bootable image.
  This is the same model as Buildroot/Yocto (a generic build engine + per-board
  config + component recipes). We roll our own because *building it* is a
  learning goal, but we follow the industry pattern.

## Goal

Turn `projects/gameboy-v3/` from "one bespoke project with a flat `scripts/`
dir" into a **provider / product / engine** model, so that:

1. Generic components (the from-scratch kernel, bootloader, libc, coreutils) live
   **once** at the top level of `embedded/`, reusable across projects — alongside
   the existing `rtos/`, `sim/`, `dbg/`.
2. A **product** (`gameboy-v3`, and future boards) is mostly *configuration*:
   which providers to use, the board's specifics, and an overlay of product files.
3. The **build orchestration is reusable** — a new product reuses the engine
   (`forge/`) without copying scripts. (A shared SoC/BSP tier is deferred until a
   second board exists — see "Why there is no `soc/` tier yet".)
4. A product can **select the custom providers OR the open-source ones**
   (`KERNEL=custom|mainline`, `LIBC=gv3|musl`, `COREUTILS=gv3|busybox`,
   `BOOTLOADER=custom|uboot`) — the payoff of drop-in ABI compatibility, and the
   project's known-good-reference testing story promoted to a build switch.

## Naming decisions

- **`forge/`** — the shared build system (engine). Chosen over `build/` (which
  reads as gitignored artifacts) and `buildroot/` (confusingly close to the real
  Buildroot tool). Mnemonic: "the forge assembles providers + board config into
  an image." It's a single rename if a different name is later preferred — the
  name appears only as a path.
- **`build/`** keeps its current meaning: gitignored build *artifacts*,
  per-project. `forge/` is checked-in infrastructure; `build/` is output.
- Providers keep descriptive names: `kernel/`, `bootloader/`, `libc/`,
  `coreutils/`.

## Community convention this follows (so the structure isn't arbitrary)

- **libc and userland utilities are SEPARATE components.** musl ships only a C
  library (no `ls`/`cat`); the utilities are separate projects (GNU coreutils,
  BusyBox, sbase, toybox) that *link against* a libc. Buildroot/Yocto treat them
  as sibling packages, never nested. → therefore our coreutils are their **own
  provider** (`coreutils/`), a sibling of `libc/`, NOT nested inside it.
- **The dynamic linker rides WITH the libc.** In musl, `libc.so` and the dynamic
  linker (`ld-musl.so`) are literally the same binary. → therefore our linker
  (`ld/`) lives *inside* `libc/` — that co-location is correct and matches reality.
- **The shell is conventionally its own component too** (dash/bash/busybox-ash are
  distinct from coreutils). At our scale, `sh` stays in `coreutils/` for now; flag
  it as an eventual separate provider if it grows. Do NOT split it now.
- **Engine vs. board config** mirrors coreboot's `src/` (generic) vs.
  `src/mainboard/` (board), and the Linux kernel's Kbuild (generic) vs. `arch/` +
  `arch/*/mach-*` (SoC/board data the build consumes). The build engine stays
  chip-agnostic; chip/board facts are *inputs* it consumes.

## Target structure

```
  embedded/
    forge/                    ← THE SHARED BUILD ENGINE (checked in, reused by all products)
      rules.mk                  top-level targets; `make image` deps kernel+rootfs+bootloader+toolchain
      providers.mk              resolves KERNEL/LIBC/COREUTILS/BOOTLOADER selection -> source paths
      toolchain.mk              fetch/locate the cross toolchains (was scripts/00-toolchain.sh)
      kernel.mk                 generic "build $(KERNEL_SRC) with $(BOARD) config" (was 02-kernel.sh)
      bootloader.mk             generic "build the selected bootloader" (was 01-uboot.sh + custom)
      rootfs.mk                 staging-tree + walk + overlay-merge + device-table assembler (was 03-rootfs.sh)
      image.mk                  genimage wrapper: $(BOARD)/image.cfg -> .img (was 04-image*.sh)
      README.md                 how the engine works + how to add a provider/product

    kernel/                   ← PROVIDER: the from-scratch Linux-ABI kernel   (moved from gameboy-v3/kernel)
    bootloader/               ← PROVIDER: the from-scratch T113 bootloader     (moved from gameboy-v3/bootloader)
    libc/                     ← PROVIDER: the C library + dynamic linker       (moved from gameboy-v3/rootfs/{libc,ld})
      include/ src/ user.ld     the C library (gv3libc)
      ld/                       the dynamic linker (ld-gv3) — rides WITH libc, like musl
      test/                     libc + linker host unit tests
    coreutils/                ← PROVIDER: the userland utility suite (like sbase/busybox)  (moved from gameboy-v3/rootfs/bin)
      sh.c echo.c cat.c ls.c pwd.c wc.c ...   depends on (links against) a libc provider

    tools/                    ← host/dev scripts (NEW, gathered from scripts/)
      flash.sh                  SD/NOR flashing              (was scripts/flash.sh)
      t113power.sh              remote power-cycle rig         (was scripts/t113power.sh)

    rtos/  sim/  dbg/  cad/  docs/     ← existing top-level siblings — UNCHANGED, OUT OF SCOPE

    projects/
      gameboy-v3/             ← A PRODUCT: config + board data + overlay + (for now) SoC facts
        config.mk               THE SELECTION: providers + board
        Makefile                thin: `include ../../forge/rules.mk`
        board/t113-gameboy/     board-specific BUILD INPUTS:
          pinmux.dtsi             this board's pin muxing / peripherals
          lcd-ili9341.dtsi
          nor-spi.dtsi
          uart0-console.dtsi
          image.cfg               genimage layout (partition table + BROM 8KiB offset)
          load-addrs              kernel/DTB/initramfs addresses (LAYOUT choice, not a SoC fact)
          defconfig-fragment      sunxi/T113 bits for the MAINLINE kernel path
        overlay/                SHIPPED into the rootfs, layered on the generic one:
          init.sh                 this product's PID-1 policy
          etc/... boot.cmd
        rootfs.devs             device nodes for THIS product's image
        build/                  artifacts (gitignored, as today)
```

### The two reuse tiers we build NOW (SoC tier deferred)

```
  TIER 1  forge/        generic engine        reused by EVERY project, any chip
  TIER 2  projects/X/   product + board       written fresh per device
                        (also holds T113/SoC facts FOR NOW — see below)
```

- A **new project** reuses tier 1 (`forge/`) and the providers, writing only
  tier 2: copy `projects/gameboy-v3/`, edit `config.mk` / `board/` / `overlay/`.

### Why there is no `soc/` tier yet (deliberate)

An earlier draft had a middle `soc/t113/` tier for "T113 facts reused across
boards." **We are deliberately NOT building it yet**, because that would extract a
shared tier from a sample size of ONE board — you can't tell which facts are
truly SoC-level vs. board-level until a *second* board disagrees. Every candidate
examined turned out NOT to belong in a shared SoC tier anyway:

- **DRAM timings** (`CONFIG_DRAM_*`) → **provider-internal**: they live in the
  bootloader's `dram.c` on the custom path, and in U-Boot's defconfig on the
  mainline path. Not shared, and different per bootloader.
- **Kernel/DTB/initramfs load addresses** → **board-level layout choice**, not a
  chip fact: a different T113 board could place them differently. The only
  silicon-pinned address is `DRAM_BASE = 0x40000000` (the memory canvas); *where*
  you place things on it is the board's decision. → lives in `board/`.
- **`sunxi_defconfig` fragment** → consumed only by the *mainline* kernel build;
  keep it with that build path (in `board/`) for now.
- **BROM 8KiB SD offset, FEL USB id, DRAM_BASE** → genuinely silicon-pinned, but
  there is little of it and only one consumer today; keep in `board/`/`tools/`.

**The test for later:** when a second board exists, ask of each item *"could a
different board with the same chip choose differently?"* — if no, it's a real SoC
fact and graduates to a `soc/t113/` (or `soc/sunxi/`) tier then; if yes, it stays
board-level. Extract `soc/` only when the second board defines its real shape
(coreboot's `src/soc/` + `src/mainboard/` split is the model to follow then).

## The provider-selection mechanism

`projects/gameboy-v3/config.mk` — the product declares which implementation
provides each layer:

```makefile
# providers: our custom implementation, or the open-source reference
KERNEL     ?= custom      # custom -> embedded/kernel     | mainline -> fetch Linux
BOOTLOADER ?= custom      # custom -> embedded/bootloader | uboot    -> fetch U-Boot
LIBC       ?= gv3         # gv3    -> embedded/libc        | musl     -> fetch musl
COREUTILS  ?= gv3         # gv3    -> embedded/coreutils   | busybox  -> fetch BusyBox

BOARD      := t113-gameboy
```

Note `LIBC` and `COREUTILS` are **independent axes** (the whole reason coreutils
is its own provider): you can build "our libc + BusyBox utilities" or "musl + our
utilities" — any combination.

`forge/providers.mk` resolves each selection to a source path:

```makefile
EMBEDDED  := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
KERNEL_SRC     := $(if $(filter custom,$(KERNEL)),     $(EMBEDDED)/kernel,     $(BUILD)/linux)
BOOTLDR_SRC    := $(if $(filter custom,$(BOOTLOADER)), $(EMBEDDED)/bootloader, $(BUILD)/u-boot)
LIBC_SRC       := $(if $(filter gv3,$(LIBC)),          $(EMBEDDED)/libc,       $(BUILD)/musl)
COREUTILS_SRC  := $(if $(filter gv3,$(COREUTILS)),     $(EMBEDDED)/coreutils,  $(BUILD)/busybox)
```

Then, from `projects/gameboy-v3/`:
```
  make                       # full custom stack (custom kernel + gv3 libc + gv3 coreutils + custom bootloader)
  make KERNEL=mainline LIBC=musl COREUTILS=busybox BOOTLOADER=uboot   # all-open-source reference build
  make KERNEL=mainline       # our rootfs on a mainline kernel (isolate: is a bug ours or the kernel's?)
```

That last line is the **known-good-reference discipline** turned into a
build-wide switch: swap one provider to the reference to localize a failure.

## What moves where (the migration inventory)

Every path below is under `projects/gameboy-v3/` unless it starts with
`embedded/`. Use `git mv` to preserve history. **Anything not listed here — in
particular `rtos/`, `sim/`, `dbg/`, `cad/`, and all microcontroller/FPGA work —
is out of scope and must not move.**

Graduate the generic providers:
```
  projects/gameboy-v3/kernel/       -> embedded/kernel/
  projects/gameboy-v3/bootloader/   -> embedded/bootloader/    (incl. its DRAM timings — provider-internal)
  projects/gameboy-v3/rootfs/libc/  -> embedded/libc/          (the C library)
  projects/gameboy-v3/rootfs/ld/    -> embedded/libc/ld/       (linker rides WITH libc, like musl)
  projects/gameboy-v3/rootfs/bin/   -> embedded/coreutils/     (the utility suite — its OWN provider)
  projects/gameboy-v3/rootfs/test/  -> embedded/libc/test/ and/or embedded/coreutils/test/ (split by what each tests)
```

The current flat `scripts/` splits three ways:
```
  ENGINE (generalize -> embedded/forge/):
    00-toolchain.sh   -> forge/toolchain.mk
    01-uboot.sh       -> forge/bootloader.mk    (the uboot provider path)
    02-kernel.sh      -> forge/kernel.mk        (parameterized by BOARD)
    03-rootfs.sh      -> forge/rootfs.mk        (merged with rootfs/'s staging pattern)
    04-image*.sh      -> forge/image.mk + board/*/image.cfg (genimage)
    build.sh, env.sh  -> forge/rules.mk

  BOARD BUILD INPUTS (-> projects/gameboy-v3/board/t113-gameboy/):
    lcd-ili9341.dtsi, nor-spi.dtsi, uart0-console.dtsi
    boot.cmd
    load addresses (kernel/DTB/initramfs — layout choice, not a SoC fact)
    image layout -> image.cfg
    NB: T113/SoC facts (BROM 8KiB offset, DRAM_BASE, FEL id) also live here FOR
    NOW — no separate soc/ tier until a second board exists. DRAM *timings* stay
    provider-internal (in the bootloader / U-Boot).

  OVERLAY (shipped into the rootfs -> projects/gameboy-v3/overlay/):
    init / init.sh  (the PID 1)   -> overlay/init.sh

  HOST/DEV TOOLING (-> embedded/tools/):
    flash.sh          -> tools/ (SD/NOR flashing)
    t113power.sh      -> tools/ (the remote power rig)
```

### The rootfs split (the fiddly part)

`rootfs/` today is **two kinds of thing** glued together: the reusable component
sources (the C library, the dynamic linker, the coreutils, and the
staging/walk/device-table build *pattern*) and product specifics (`init.sh`,
`rootfs.devs`, which programs ship). The split:

- **Graduates to providers:** `libc/` (+ `ld/`) → `embedded/libc/`; the coreutils
  (`bin/`) → `embedded/coreutils/` (its own provider). Each builds to
  objects/binaries the forge can stage.
- **Assembly logic → the forge:** the staging-tree walk + `gen_init_cpio`
  packaging becomes `forge/rootfs.mk` (generic). Its *inputs* (`overlay/`,
  `rootfs.devs`, which programs ship) stay per-product.
- **New step — overlay merge:** the rootfs assembler stages the generic files
  (from the libc/coreutils builds), then copies `projects/X/overlay/*` on top,
  then packs. This is how "generic rootfs + this product's init" compose without
  forking the rootfs. The current `rootfs/Makefile` already does staging + walk +
  device-table; it gains only the overlay-copy step, then graduates into
  `forge/rootfs.mk`.

## Board config must flow INTO the generic providers

Today `kernel/` bakes in T113 addresses (`board.h`) and the rootfs bakes in
`INIT_PATH`, etc. For a provider to be truly generic, those become **inputs**:

- `kernel.mk` builds `$(KERNEL_SRC)` with the board's defconfig fragment +
  `$(BOARD)/*.dtsi` — the kernel source stops hardcoding the board.
- Load addresses + BROM offset come from `$(BOARD)/` (board config). DRAM timings
  stay inside the chosen bootloader provider.

This is real decoupling (parameterizing the providers on board config), more than
moving folders. **It is staged separately** — see phasing. (If common facts later
prove genuinely SoC-wide across a second board, THAT is when a `soc/` tier is
extracted — not now.)

## Phasing (safe, incremental — each phase must build + boot green)

The refactor is large; do it in order, verifying at each step. **The safety net
is the existing test harness:** the kernel's golden tests (`kernel/test/`) and
`rootfs/test/dynamic.sh` boot the stack under QEMU `-M virt` and check output.
Run them after each phase; a phase is done only when they pass as before.

- **Phase 0 — provider selection in place, nothing moved.**
  Add `config.mk` + a `providers.mk`-style resolver *inside* `gameboy-v3`, with
  paths still pointing at current in-project locations. Prove `make KERNEL=custom`
  and `make KERNEL=mainline` both build. Establishes the switch with zero moves.

- **Phase 1 — graduate the providers (git mv, paths hardcoded).**
  Move `kernel/`, `bootloader/`, `libc/` (+`ld`), `coreutils/` (was `rootfs/bin`)
  to `embedded/`. Update resolver paths. Do NOT parameterize board config yet —
  the kernel can still hardcode T113. Prove the full stack builds + boots in QEMU.

- **Phase 2 — extract the engine into `forge/`.**
  Generalize the numbered scripts into `forge/*.mk`, replacing implicit ordering
  (encoded in the `NN-` filename prefixes) with real Make dependencies.
  `gameboy-v3/Makefile` becomes a thin `include`. Prove parity with the
  pre-refactor build.

- **Phase 3 — split board/overlay out of the product.**
  Move `*.dtsi` / `boot.cmd` / `image.cfg` / load-addresses to `board/`, `init` to
  `overlay/`. Add the overlay-merge step. Replace `04-image*.sh` with `genimage` +
  `image.cfg`. (T113/SoC facts stay in `board/` — no `soc/` tier yet.)

- **Phase 4 — parameterize providers on board config.**
  Remove hardcoded T113 from `kernel/`/`bootloader/`; feed them from `board/`.
  This is the deepest change and the true test that the providers are generic.
  Deferrable until a second board actually needs it.

- **Phase 5 (on project #2) — validate reuse, THEN consider `soc/`.**
  Create the second product; confirm it reuses `forge/` + the providers and writes
  only its own config/board/overlay. This is where the design proves itself — and
  where, with two boards to compare, you can finally see which facts are genuinely
  SoC-wide and extract a `soc/<chip>/` tier for them (coreboot `src/soc/` +
  `src/mainboard/` model). Not before.

## Honest cautions

- **Out-of-scope dirs.** `rtos/`, `sim/`, `dbg/`, `cad/` (microcontroller/FPGA
  work) are untouched by this plan. Only the T113 Linux path
  (`projects/gameboy-v3/` + the new top-level providers/forge) is in scope.
- **Legibility cost.** The numbered `NN-*.sh` scripts are readable top-to-bottom;
  a parameterized `forge/*.mk` engine is more powerful but harder to follow. Keep
  `forge/README.md` strong and the `.mk` files commented, or the reuse win is paid
  for in onboarding cost.
- **This IS Buildroot's job.** If the goal were purely "ship many products
  efficiently," adopting Buildroot and packaging the custom kernel/libc/coreutils
  as Buildroot packages would give the engine + SoC layers + genimage for free. We
  roll our own because the *building* is the learning goal — a deliberate choice.
- **Phase 4 is the hard one; don't front-load it.** Moving folders (Phases 1–3) is
  low-risk `git mv` + path edits. Parameterizing providers on board config
  (Phase 4) is real decoupling — defer until reuse is concrete.
- **Genimage dependency.** Phase 3's `image.cfg` approach adds a host tool
  (`genimage`); confirm it's available/installable before committing, or keep a
  thin `image.mk` that shells the current `dd`-based logic in the interim.

## One-paragraph summary

Graduate the generic Linux-path components — `kernel`, `bootloader`, `libc`
(with its dynamic linker), and `coreutils` (its own provider, sibling to `libc`
per community convention) — to the top level of `embedded/` next to `rtos/`,
`sim/`; make `projects/gameboy-v3/` a thin product (`config.mk` + `board/` +
`overlay/`) that *selects* providers along independent axes (`KERNEL`,
`BOOTLOADER`, `LIBC`, `COREUTILS` = custom or open-source); and factor the
orchestration (today's numbered `scripts/`) into a shared `forge/` engine — so
the next project reuses `forge/` and the providers and writes only its own
config/board/overlay. Two tiers now (`forge/` + product); the SoC/BSP tier is
deliberately deferred until a second board reveals which facts are genuinely
chip-wide. Phase it, verifying with the QEMU golden tests after each step:
selection switch → folder moves → engine extraction → (later) parameterize
providers on board config → (on board #2) consider extracting `soc/`. The
microcontroller and FPGA parts of the repo are unaffected throughout.
