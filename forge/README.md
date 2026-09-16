# forge/ — the Linux build workshop (engine + catalogs)

`forge/` turns **a product's selected providers + board config** into a bootable image.
Same model as Buildroot/Yocto — a generic engine, per-board config, and component
recipes — rolled by hand because building it is the learning goal. Today's one product
is [`projects/gameboy-v3/`](../projects/gameboy-v3/).

The metaphor: **`forge/` is the whole workshop; `forge/engine/` is the machinery, `forge/meta/` the parts
catalog.** This mirrors Yocto's `poky/` split into `bitbake/` (the engine) + `meta/` (the core layer).

```
forge/
  engine/            the ENGINE — Make orchestrator (engine.mk) + the shell it drives (run-recipe.sh, recipe-scan.sh)
  meta/              the core LAYER (Yocto's poky/meta) — classes + recipe catalog:
    classes-global/    classes auto-inherited by EVERY node (base); à la Yocto's classes-global/
    classes-recipe/    classes a recipe opts into via `inherit <class>`; à la Yocto's classes-recipe/
    recipes-kernel/    linux, kernel-custom            (provide virtual/kernel)
    recipes-bsp/       u-boot, bootloader-custom        (provide virtual/bootloader)
    recipes-core/      musl, libc-custom (virtual/libc); init-* , runit (virtual/init);
                       busybox, coreutils (additive packages); rootfs, image (build phases)
    recipes-devtools/  toolchains (class=cross), make/genimage/gen_init_cpio/libconfuse/binman-venv (class=native)
```

The ENGINE (`forge/engine/`) is generic and product-agnostic; all metadata — classes AND recipes — lives
in the LAYER (`forge/meta/`), exactly as Yocto keeps `.bbclass` and `.bb` files out of `bitbake/`.

**Yocto-style flat catalog.** Recipes are grouped by DOMAIN (`recipes-<domain>/`, like Yocto's
`recipes-core`/`recipes-devtools`/`recipes-kernel`/`recipes-bsp`), not by role-directory. A recipe's
ROLE is METADATA, not its folder:
- `PKG_CLASS` = `target` (in the image) | `native` (host tool) | `cross` (host tool emitting target
  code) | `image` (a build phase). `native`/`cross` recipes become `host-<name>` graph nodes.
- A swappable axis declares `PKG_PROVIDES=virtual/<axis>`; the product's `config.mk` picks one with
  `PREFERRED_PROVIDER_virtual/<axis> = <recipe>` (Yocto's model exactly — the engine names no axis).
  Every buildable — provider, host tool, package, step — is a graph node named by its recipe; a
  dependency is either a recipe name or a `virtual/<x>` the engine resolves via `PREFERRED_PROVIDER`.
Adding an implementation = a new `recipes-<domain>/<name>/` dir with the right metadata; no engine edit.
The PRODUCT is its own layer (its `recipes-*/` + `packages/` are searched first, overriding forge's).

**Two addressing roots.** CATALOGS resolve under `FORGE_META` (= `forge/meta/`, the layer). Custom-provider
SOURCE (repo-root `kernel/ libc/ bootloader/ coreutils/` — software, not build-system)
resolves under `REPO_ROOT` (the git root). A recipe's `PKG_FETCH=local` + `PKG_SOURCE=kernel`
means `$REPO_ROOT/kernel`.

## How a product uses it

A product Makefile is thin:

```make
PRODUCT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT   := $(abspath $(PRODUCT_DIR)/../..)
include config.mk                        # the SELECTION (providers + board + media)
include $(REPO_ROOT)/forge/engine/engine.mk # the engine
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
`recipes-<domain>/<name>/`). A package just depends on `libc` (`PKG_DEPENDS`); libc compatibility
is not pre-checked, so `LIBC=custom PACKAGES=busybox` builds until it hits the real link
errors on symbols libc does not implement yet (which are the libc port worklist).

Peripherals (an LCD, etc.) are NOT axes — a peripheral is invariant board data (a DT node
+ driver kconfig), applied unconditionally by the board via `board/<board>/` fragments.

## The engine

Every buildable is a **recipe** run through **one runner**; **Make is the one dependency
walker**. There is no per-layer makefile and no dispatch on provider identity.

| file | role |
|------|------|
| `engine/engine.mk`     | the Make engine, naming no component: RESOLUTION (each `virtual/<x>` → its `PREFERRED_PROVIDER` recipe, verified to provide it; `forge.conf` gets a generic `PROVIDER_<x>` path per virtual) then TARGETS+GRAPH (one `_node_rule` per recipe, node = recipe name; a dep is a recipe name or a `virtual/<x>` resolved the same way; host + target deps are both Make prerequisites). |
| `engine/recipe-scan.sh`| the ONE place `recipe.sh` is parsed on the Make side — `field <recipe> <KEY>` reads a metadata key (last-wins, comment/quote-stripped, `${VAR}` left for Make to expand). |
| `engine/run-recipe.sh` | the ONE node runner (ORCHESTRATOR) + the shared build ENVIRONMENT. Loads the env (sources forge.conf + board.conf, scrubs PATH to the HOSTTOOLS allowlist), defines only the primitives it needs before a class is inherited (`recipe_get`, `apply_dtsi_overlay`, `log`/`die`, `inherit`), computes the content taskhash + skips up-to-date nodes, then sources a recipe and calls `do_fetch → do_build → do_install` BY NAME — no branch on kind or identity. The FETCH mechanism is NOT here — it's the default `do_fetch`, in `meta/classes-global/base.sh`. |
| `meta/classes-global/` | classes auto-inherited by EVERY node (Yocto's `classes-global/`): `base` — the implicit default tasks + the FETCH MECHANISM every node gets (the default `do_fetch` dispatch per `PKG_FETCH` plus the shared download/clone primitives `fetch_verify`/`git_clone_pinned`/`clone_or_reuse_pinned`/`forge_fetch_file` that host classes also call — à la `base.bbclass`). |
| `meta/classes-recipe/` | capabilities a recipe opts into via `inherit <class>` (Yocto's `classes-recipe/`, `.bbclass`): task DEFAULTS (`compile-c`, `make-c`, `libc`, host `host-cc`/`host-autotools`/`host-pyvenv`/`host-tarball-bin`/`host-toolchain-gcc`, `devicetree`) or a shared MECHANISM (`kconfig` = the defconfig→fixup→normalize functions, à la Yocto's `cml1`). The libc's CC/link contract lives with the libc (`meta/recipes-core/<libc>/cc-profile.sh`), sourced directly — the engine has no per-libc CC code. || `core/defaults/`     | engine defaults (`rootfs.devs`, host config fragments). |

### A recipe

A recipe is bare `KEY=value` facts + a class binding + optional inline task overrides. It
is read two ways: `engine.mk` scrapes keys with `awk`; `run-recipe.sh` and the classes
`source` it as bash. (Hence `recipe.sh`, not `.mk` — it is bash, never Make-included.)

```sh
# recipes-kernel/kernel-custom/recipe.sh — uses the class defaults (the common case)
PKG_NAME=kernel
PKG_CLASS=provider
PKG_ROLE=kernel
PKG_FETCH=local
PKG_SOURCE=kernel               # -> $REPO_ROOT/kernel
PKG_TYPE=make-c
inherit make-c                  # binds do_build (make -C) + do_install (kernel DTB)
PKG_HOST_DEPENDS=toolchain-glibc
PKG_ARTIFACT=src:build/${KERNEL_TARGET}/kernel.bin
```

```sh
# recipes-kernel/linux/recipe.sh — overrides a task (the bespoke case)
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

`engine.mk` resolves the whole selection and writes it to
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
  recipe (kernel/U-Boot in `recipes-<domain>/<name>/recipe.sh`, busybox in `recipes-core/busybox/recipe.sh`),
  Buildroot/Yocto style.

The HOST-constrained pins (cross toolchains, GNU make — chosen by the build host, not the
product) stay in `recipes-devtools/<tool>/recipe.sh`. A second product reuses `forge/` and writes
only its own `config.mk` + `board/`.

## Targets

```
make                          # default: image for MEDIA (nor bundle | sd .img)
make image                    # same, explicit
make flash                    # image (nor) + flash + FEL-boot on the rig
make <recipe>                 # build one node by its recipe name: linux, musl, busybox,
                              #   rootfs, toolchain-gcc, gen_init_cpio, … (see meta/recipes-*/)
make test                     # kernel golden QEMU tests
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
