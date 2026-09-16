# os/ — the Linux build workshop (engine + catalogs)

`os/` turns **a product's selected providers + board config** into a bootable image.
Same model as Buildroot/Yocto — a generic engine, per-board config, and component
recipes — rolled by hand because building it is the learning goal. Today's one product
is [`projects/gameboy-v3/`](../projects/gameboy-v3/).

The metaphor: **`os/` is the whole workshop; `os/engine/` is the machinery, `os/meta/` the parts
catalog.** This mirrors Yocto's `poky/` split into `bitbake/` (the engine) + `meta/` (the core layer).

```
os/
  engine/            the ENGINE — a Make graph-walker (engine.mk) + the bash it drives (run-recipe.sh + os-env.sh)
  meta/              the core LAYER (Yocto's poky/meta) — classes + recipe catalog:
    classes-global/    classes auto-inherited by EVERY node (base); à la Yocto's classes-global/
    classes-recipe/    classes a recipe opts into via `inherit <class>`; à la Yocto's classes-recipe/
    recipes-kernel/    linux, kernel-custom            (provide virtual/kernel)
    recipes-bsp/       u-boot, bootloader-custom        (provide virtual/bootloader)
    recipes-core/      musl, libc-custom (virtual/libc); init-* , runit (virtual/init);
                       busybox, coreutils (additive packages); rootfs, image (build phases)
    recipes-devtools/  toolchains (class=cross), make/genimage/gen_init_cpio/libconfuse/binman-venv (class=native)
```

The ENGINE (`os/engine/`) is generic and product-agnostic; all metadata — classes AND recipes — lives
in the LAYER (`os/meta/`), exactly as Yocto keeps `.bbclass` and `.bb` files out of `bitbake/`.

**Yocto-style flat catalog.** Recipes are grouped by DOMAIN (`recipes-<domain>/`, like Yocto's
`recipes-core`/`recipes-devtools`/`recipes-kernel`/`recipes-bsp`), not by role-directory. A recipe's
ROLE is METADATA, not its folder:
- `PKG_CLASS` = `target` (in the image) | `native` (host tool) | `cross` (host tool emitting target
  code) | `image` (a build phase). Every buildable — provider, host tool, package, step — is a graph
  node named by its recipe (`make linux`, `make musl`, `make busybox`, `make toolchain-gcc`, `make rootfs`).
- A swappable axis declares `PKG_PROVIDES=virtual/<axis>`; the product's `local.conf` picks one via its
  `os_preferred_provider` (Yocto's DISTRO/MACHINE role — the engine names no axis). A dependency is
  either a recipe name or a `virtual/<x>` the scripts resolve to its preferred provider.
Adding an implementation = a new `recipes-<domain>/<name>/` dir with the right metadata; no engine edit.
The PRODUCT is its own layer (its `recipes-*/` + `packages/` are searched first, overriding os's).

**Two addressing roots.** CATALOGS resolve under `OS_META` (= `os/meta/`, the layer). Custom-provider
SOURCE (repo-root `kernel/ libc/ bootloader/ coreutils/` — software, not build-system)
resolves under `REPO_ROOT` (the git root). A recipe's `PKG_FETCH=local` + `PKG_SOURCE=kernel`
means `$REPO_ROOT/kernel`.

## How a product uses it

A product Makefile is thin — it only includes the engine:

```make
PRODUCT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT   := $(abspath $(PRODUCT_DIR)/../..)
include $(REPO_ROOT)/os/engine/engine.mk
```

The SELECTION lives in the product's `local.conf` (plain bash — Yocto's `conf/local.conf`; the os
scripts read it, never Make), picking an implementation for each layer along **independent axes**:

```sh
: "${KERNEL:=mainline}"          # mainline -> fetch Linux           | custom -> repo-root kernel/
: "${BOOTLOADER:=custom}"        # custom   -> repo-root bootloader/ | uboot  -> fetch U-Boot
: "${LIBC:=musl}"                # musl     -> fetch + build musl    | custom -> repo-root libc/
: "${PACKAGES:=busybox console}" # rootfs install set (space-separated)
: "${BOARD:=t113-gameboy}"
: "${MEDIA:=nor}"                # nor -> flash bundle (FEL loop)    | sd -> dd-able .img
```
plus a `os_preferred_provider` case mapping each knob to a recipe (`mainline`→`linux`, `custom`→`libc-custom`, …).

Override any axis on the CLI as an env var: `KERNEL=mainline make` boots our rootfs on a mainline
kernel — the known-good-reference discipline as a build switch, to localize whether a bug is ours or
upstream's. (The `:=` defaults keep an env override.)

The rootfs is a **package model**: `LIBC` is the C library everything links; `PACKAGES`
is the additive install set (our coreutils and BusyBox are both packages under
`recipes-<domain>/<name>/`). A package just depends on `virtual/libc` (`PKG_DEPENDS`); libc compatibility
is not pre-checked, so `LIBC=custom PACKAGES=busybox` builds until it hits the real link
errors on symbols libc does not implement yet (which are the libc port worklist).

Peripherals (an LCD, etc.) are NOT axes — a peripheral is invariant board data (a DT node
+ driver kconfig), applied unconditionally by the board via `board/<board>/` fragments.

## The engine

Every buildable is a **recipe** run through **one runner**; **Make is the one dependency
walker**. There is no per-layer makefile and no dispatch on provider identity.

| file | role |
|------|------|
| `engine/engine.mk`     | a pure dependency-graph walker — nothing else. Globs the recipes, asks os-env for each node's resolved prerequisites (`$(shell os-env.sh deps <node>)`), runs `run-recipe.sh <node>` per node, and lets Make walk the graph. No config, no resolution, no state — all in bash. |
| `engine/os-env.sh`  | the config + resolution BRAIN (bash). Reads the product's `local.conf`; resolves a `virtual/<x>` to its preferred provider (`os_resolve` / `os_byname`, verified against `PKG_PROVIDES`); derives the whole build environment (`os_load_env`: `PROVIDER_<x>` paths, `CROSS_COMPILE` + toolchain dirs, the path layout, the artifact tags). SOURCED by `run-recipe.sh`; EXECUTED by Make for `deps` (graph edges) + the Makefile's `print` (flash). |
| `engine/run-recipe.sh` | the ONE node runner (ORCHESTRATOR). Takes a node NAME; sources `os-env.sh` for the resolved env, scrubs PATH to the HOSTTOOLS allowlist, defines the pre-class primitives (`apply_dtsi_overlay`, `inherit`), computes the content taskhash + skips up-to-date nodes, then sources the recipe and calls `do_fetch → do_build → do_install` BY NAME — no branch on kind or identity. The FETCH mechanism is NOT here — it's the default `do_fetch`, in `meta/classes-global/base.sh`. |
| `meta/classes-global/` | classes auto-inherited by EVERY node (Yocto's `classes-global/`): `base` — the implicit default tasks + the FETCH MECHANISM every node gets (the default `do_fetch` dispatch per `PKG_FETCH` plus the shared download/clone primitives `fetch_verify`/`git_clone_pinned`/`clone_or_reuse_pinned`/`os_fetch_file` that host classes also call — à la `base.bbclass`). |
| `meta/classes-recipe/` | capabilities a recipe opts into via `inherit <class>` (Yocto's `classes-recipe/`, `.bbclass`): task DEFAULTS (`compile-c`, `make-c`, `libc`, host `host-cc`/`host-autotools`/`host-pyvenv`/`host-tarball-bin`/`host-toolchain-gcc`, `devicetree`) or a shared MECHANISM (`kconfig` = the defconfig→fixup→normalize functions, à la Yocto's `cml1`). The libc's CC/link contract lives with the libc (`meta/recipes-core/<libc>/cc-profile.sh`), sourced directly — the engine has no per-libc CC code. || `core/defaults/`     | engine defaults (`rootfs.devs`, host config fragments). |

### A recipe

A recipe is bare `KEY=value` facts + a class binding + optional inline task overrides. It
is read two ways: `os-env.sh` scrapes keys (`recipe_get`, for the deps + the env); `run-recipe.sh`
and the classes `source` it as bash. (Hence `recipe.sh`, not `.mk` — it is bash, never Make-included.)

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

### local.conf + os-env — no generated config file

There is no generated config file. The product's `local.conf` (bash) is the whole selection; `os-env.sh`
reads it and DERIVES the entire build environment on demand — the resolved `PROVIDER_<x>` paths, the
toolchain scalars (`CROSS_COMPILE`, `TOOLCHAIN_DIR`, …), the path layout (all a fixed function of
`PRODUCT_DIR`), and the artifact tags (`CFG`, `INITRAMFS_IMAGE`). `run-recipe.sh` sources `os-env.sh`
at the top of every node; Make calls it (`os-env.sh deps <node>`) to resolve the graph edges. Nothing
is threaded through `$(MAKE)` and nothing is written to disk. The one seed both need is `PRODUCT_DIR`
(passed by the engine on every call); everything else is `local.conf` + `board.conf` + the catalog.

### How the runner finds the product's data

`os-env.sh` needs only `PRODUCT_DIR` (the engine passes it) and reads, from the product:

- `local.conf` — the SELECTION (knobs + `os_preferred_provider` + `OS_VIRTUALS`), env-overridable.
- `boards/<board>/board.conf` — the single board config (BSP): provider build targets, OSS build facts
  (defconfigs, board DT, console), and memory/storage layout. Plain bash, `source`d whole.
- `versions.env` — OPTIONAL; gameboy-v3 ships none. Component version pins live in each recipe
  (kernel/U-Boot in `recipes-<domain>/<name>/recipe.sh`, busybox in `recipes-core/busybox/recipe.sh`),
  Buildroot/Yocto style.

The HOST-constrained pins (cross toolchains, GNU make — chosen by the build host, not the
product) stay in `recipes-devtools/<tool>/recipe.sh`. A second product reuses `os/` and writes
only its own `local.conf` + `boards/`.

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

Sibling to `os/`, the repo-root [`tools/`](../tools/) holds the rig/dev tooling
(`flash.sh`, `t113power.sh`) — deliver + debug, distinct from build. `make flash` shells
`tools/flash.sh`.

## Deliberate scope

- **The engine ORCHESTRATES; the reproducible backends do the heavy lifting.** Recipes
  delegate to proven build procedures (`build.sh`, the classes). Forge's win is *reusable
  engine + Make dependency graph + thin product Makefile*, not reimplementing fetch/build
  in Make.
- **The engine is board-agnostic; the board is an input.** No `os/` file bakes in
  "t113". Provider build targets come from `board/<board>/board.conf`; DT overlays +
  `genimage.cfg` live alongside.
- **The kernel source still carries the SoC address sets** (GIC/UART/timer bases in
  `kernel/include/board.h`, selected by `BOARD=t113|virt`). Those are chip-level, not
  board-level, so they stay put until a second board justifies a `soc/` tier — you can't
  extract a shared tier from a sample size of one.

_Design docs: [`docs/`](../docs/) (start with `LINUX_FORGE_RECIPE_CONTRACT.md`)._
