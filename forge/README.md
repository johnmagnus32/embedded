# forge/ — the Linux build workshop (engine + catalogs)

`forge/` turns **a product's selected providers + board config** into a bootable image.
Same model as Buildroot/Yocto — a generic engine, per-board config, and component
recipes — rolled by hand because building it is the learning goal. Today's one product
is [`projects/gameboy-v3/`](../projects/gameboy-v3/).

The metaphor: **`forge/` is the whole workshop; `forge/core/` is the machinery inside it.**

```
forge/
  core/          the ENGINE — Make orchestrator + the shell mechanism it drives
  providers/     BOOT-ARTIFACT catalog   — kernel/ bootloader/ libc/ (single-select axes)
  packages/      ROOTFS-CONTENT catalog  — busybox, coreutils (additive install set)
  hostpackages/  HOST-TOOL catalog       — toolchains, make, genimage, gen_init_cpio, binman-venv
  steps/         PHASE catalog           — rootfs, image (non-selectable build phases)
```

Buildroot parallel: `core/` ≈ `support/`; `providers/` ≈ `linux/`+`boot/`; `packages/`
≈ `package/`; `hostpackages/` ≈ the `host-*` packages; `steps/` ≈ `fs/` + the post-image
script. The catalogs are PEERS of the engine in one tree.

**Two addressing roots.** CATALOGS resolve under `FORGE_ROOT` (= `forge/`). Custom-provider
SOURCE (repo-root `kernel/ libc/ bootloader/ coreutils/` — software, not build-system)
resolves under `REPO_ROOT` (the git root). A recipe's `PKG_FETCH=local` + `PKG_SOURCE=kernel`
means `$REPO_ROOT/kernel`.

## How a product uses it

A product Makefile is thin:

```make
PRODUCT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT   := $(abspath $(PRODUCT_DIR)/../..)
include config.mk                        # the SELECTION (providers + board + media)
include $(REPO_ROOT)/forge/core/rules.mk # the engine
```

`config.mk` picks an implementation for each layer along **independent axes**:

```make
KERNEL     ?= custom       # custom -> repo-root kernel/      | mainline -> fetch Linux
BOOTLOADER ?= custom       # custom -> repo-root bootloader/  | uboot    -> fetch U-Boot
LIBC       ?= custom       # rootfs C LIBRARY: custom (repo-root libc/) | musl (fetch musl)
PACKAGES   ?= coreutils    # rootfs install set: coreutils busybox …
BOARD      ?= t113-gameboy
MEDIA      ?= nor          # nor -> flash bundle (FEL loop)   | sd -> dd-able .img
```

Override any axis on the CLI: `make KERNEL=mainline` boots our rootfs on a mainline
kernel — the known-good-reference discipline as a build switch, to localize whether a
bug is ours or upstream's.

The rootfs is a **package model**: `LIBC` is the C library everything links; `PACKAGES`
is the additive install set (our coreutils and BusyBox are both packages under
`packages/<name>/`). Each package declares the libc surface it needs (`PKG_LIBC`), so
`LIBC=custom PACKAGES=busybox` is rejected up front (BusyBox needs a complete libc).

Peripherals (an LCD, etc.) are NOT axes — a peripheral is invariant board data (a DT node
+ driver kconfig), applied unconditionally by the board via `board/<board>/` fragments.

## The engine

Every buildable is a **recipe** run through **one runner**; **Make is the one dependency
walker**. There is no per-layer makefile and no dispatch on provider identity.

| file | role |
|------|------|
| `core/rules.mk`      | top-level targets + the dependency GRAPH. Host deps and target deps are both Make prerequisites (`host-<dep>` edges come from each recipe's `PKG_HOST_DEPENDS`). |
| `core/resolve.mk`    | resolve the SELECTION → recipe paths, source paths, `CFG`, and `forge.conf`. Validates by recipe existence (adding a provider is a new dir, no edit here). One field reader (`_field`) for every recipe. |
| `core/run-recipe.sh` | the ONE node runner (ORCHESTRATOR) + the shared build ENVIRONMENT. Loads the env (sources forge.conf + board.conf, sets PATH), defines only the primitives it needs before a class is inherited (`recipe_get`, `apply_dtsi_overlay`, `log`/`die`, `_prepend_path`, `inherit`), computes the content taskhash + skips up-to-date nodes, then sources a recipe and calls `do_fetch → do_build → do_install` BY NAME — no branch on kind or identity. The FETCH mechanism is NOT here — it's the default `do_fetch`, in `classes/base.sh`. |
| `core/rules.mk` `toolchain:` | `make toolchain`: provision the base host-tool set (make + both cross toolchains + gen_init_cpio) as pure Make prerequisites over the base `host-<name>` nodes. |
| `core/classes/`      | capabilities a recipe binds via `inherit <class>` (Yocto's `.bbclass`): `base` (the implicit default tasks + the FETCH MECHANISM every node gets — the default `do_fetch` dispatch per `PKG_FETCH` plus the shared download/clone primitives `fetch_verify`/`git_clone_pinned`/`clone_or_reuse_pinned`/`forge_fetch_file` that host classes also call — à la `base.bbclass`) + task DEFAULTS (`compile-c`, `make-c`, `libc`, host `host-cc`/`host-autotools`/`host-pyvenv`/`host-tarball-bin`) or a shared MECHANISM (`kconfig` = the defconfig→fixup→normalize functions, à la Yocto's `cml1`). The libc's CC/link contract lives with the libc (`providers/libc/<impl>/cc-profile.sh`), sourced directly — the engine has no per-libc CC code. |
| `core/defaults/`     | engine defaults (`rootfs.devs`, host config fragments). |

### A recipe

A recipe is bare `KEY=value` facts + a class binding + optional inline task overrides. It
is read two ways: `resolve.mk` scrapes keys with `awk`; `run-recipe.sh` and the classes
`source` it as bash. (Hence `recipe.sh`, not `.mk` — it is bash, never Make-included.)

```sh
# providers/kernel/custom/recipe.sh — uses the class defaults (the common case)
PKG_NAME=gv3kernel
PKG_CLASS=provider
PKG_ROLE=kernel
PKG_FETCH=local
PKG_SOURCE=kernel               # -> $REPO_ROOT/kernel
PKG_TYPE=make-c
inherit make-c                  # binds do_build (make -C) + do_install (kernel DTB)
PKG_HOST_DEPENDS=toolchain-glibc
PKG_ARTIFACT=src:build/${KERNEL_TARGET}/gv3kernel.bin
```

```sh
# providers/kernel/mainline/recipe.sh — overrides a task (the bespoke case)
inherit kconfig                 # the shared configure MECHANISM (kconfig_configure/…)
do_fetch()   { :; }             # do_build self-contains fetch+build+install …
do_install() { :; }
do_build() {                    # … the provider's own procedure, inline
  ...clone pinned Linux -> defconfig + fragments -> DT overlays -> make zImage -> copy...
}
```

A `class` only DEFINES `do_*` functions; a recipe's own `do_*` after the `inherit` line
overrides it (bash last-definition-wins). Bespoke build procedures live INLINE in the recipe
(it's bash), the way Buildroot puts `FOO_BUILD_CMDS` in the package `.mk` — one file per recipe,
no sibling `build.sh`.

### forge.conf — the resolved config the backends read

`resolve.mk` resolves the whole selection once and `rules.mk` writes it to
`$(BUILD)/forge.conf` (regenerated every build; `cat` it to see exactly what was
resolved). `run-recipe.sh` `source`s it at the top of every node — instead of threading
~15 vars through recursive `$(MAKE)` calls. (Make itself doesn't read it back; it's a
prerequisite of the build targets, not an `include`.) Two seeds still pass as explicit
args because the runner needs them *before* it can find forge.conf: `PRODUCT_DIR` +
`BOARD_NAME`. Make is the only entry point — `run-recipe.sh` requires forge.conf to exist
(a bootstrap-vs-forge.conf `BOARD` mismatch is a hard error, catching a stale forge.conf).

### How the runner finds the product's data

`run-recipe.sh` requires `PRODUCT_DIR` + `BOARD_NAME` and sources, from the product:

- `board/<board>/board.conf` — the single board config (BSP): provider build targets,
  OSS build facts (defconfigs, board DT, console), and memory/storage layout. Dual-read
  (bash `source`s all keys; Make `-include`s and reads only the two target keys).
- `versions.env` — OPTIONAL; gameboy-v3 ships none. Component version pins live in each
  recipe (kernel/U-Boot in `providers/*/recipe.sh`, busybox in `packages/busybox/recipe.sh`),
  Buildroot/Yocto style.

The HOST-constrained pins (cross toolchains, GNU make — chosen by the build host, not the
product) stay in `hostpackages/*/recipe.sh`. A second product reuses `forge/` and writes
only its own `config.mk` + `board/`.

## Targets

```
make image        # default: build for MEDIA (nor bundle | sd .img)
make flash        # image (nor) + flash + FEL-boot on the rig
make kernel|bootloader|libc|rootfs|toolchain   # one layer
make print-config # resolved selection, no build
make test         # kernel golden QEMU tests
make clean
```

Sibling to `forge/`, the repo-root [`tools/`](../tools/) holds the rig/dev tooling
(`flash.sh`, `t113power.sh`) — deliver + debug, distinct from build. `make flash` shells
`tools/flash.sh`.

## Deliberate scope

- **The engine ORCHESTRATES; the reproducible backends do the heavy lifting.** Recipes
  delegate to proven build procedures (`build.sh`, the classes). Forge's win is *reusable
  engine + Make dependency graph + thin product Makefile*, not reimplementing fetch/build
  in Make.
- **The engine is board-agnostic; the board is an input.** No `forge/` file bakes in
  "t113". Provider build targets come from `board/<board>/board.conf`; DT overlays +
  `genimage.cfg` live alongside.
- **The kernel source still carries the SoC address sets** (GIC/UART/timer bases in
  `kernel/include/board.h`, selected by `BOARD=t113|virt`). Those are chip-level, not
  board-level, so they stay put until a second board justifies a `soc/` tier — you can't
  extract a shared tier from a sample size of one.

_Design docs: [`docs/`](../docs/) (start with `LINUX_FORGE_RECIPE_CONTRACT.md`)._
