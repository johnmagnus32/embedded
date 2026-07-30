# forge/ — the shared Linux-image build engine

`forge/` is the reusable build orchestration for the Linux-class products in this
repo (today: `projects/gameboy-v3/`). It turns **"a product's selected providers +
board config"** into a bootable image. Same model as Buildroot/Yocto (generic
engine + per-board config + component recipes); we roll our own because building
it is the learning goal.

## How a product uses it

A product Makefile is thin:

```make
PRODUCT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT   := $(abspath $(PRODUCT_DIR)/../..)
include config.mk                       # the SELECTION (providers + board + media)
include $(REPO_ROOT)/forge/rules.mk     # the engine
```

`config.mk` picks an implementation for each layer along **independent axes**:

```make
KERNEL     ?= custom      # custom -> repo-root kernel/      | mainline -> fetch Linux
BOOTLOADER ?= custom      # custom -> repo-root bootloader/  | uboot    -> fetch U-Boot
LIBC       ?= gv3         # gv3    -> repo-root libc/         | musl     -> fetch musl
COREUTILS  ?= gv3         # gv3    -> repo-root coreutils/    | busybox  -> fetch BusyBox
BOARD      ?= t113-gameboy
MEDIA      ?= nor         # nor -> flash bundle (FEL loop)   | sd -> dd-able .img
LCD        ?=             # empty | ili9341
```

Override any axis on the CLI: `make KERNEL=mainline` boots our rootfs on a mainline
kernel (the known-good-reference discipline as a build switch — swap one provider
to localize whether a bug is ours or upstream's).

## The files

| file            | role |
|-----------------|------|
| `rules.mk`      | top-level targets + the dependency GRAPH (image ← kernel+bootloader+rootfs ← toolchain). Replaces the old `NN-` filename ordering with real Make deps. |
| `providers.mk`  | normalize + validate the selection; resolve each axis → a source path; `-include` the board's `board.mk` for `KERNEL_TARGET`/`ROOTFS_TARGET`; translate to the current shell backends' arg vocabulary. |
| `toolchain.mk`* | (targets in rules.mk) fetch/locate the cross toolchains — shells `00-toolchain.sh`. |
| `kernel.mk`     | build the selected kernel provider (custom: `make -C kernel/ BOARD=t113`; mainline: `02-kernel.sh`). |
| `bootloader.mk` | build the selected bootloader (custom: `make -C bootloader/` + `fel`; uboot: `01-uboot.sh`). |
| `rootfs.mk`     | build the selected rootfs (scratch: the product rootfs assembler; busybox: `03-rootfs.sh`). |
| `image.mk`      | assemble components → the MEDIA output (nor bundle / sd .img). |

## Targets

```
make image        # default: build for MEDIA (nor bundle | sd .img)
make flash        # image (nor) + flash + FEL-boot on the rig
make kernel|bootloader|rootfs|toolchain   # one layer
make print-config # resolved selection, no build
make test         # kernel golden QEMU tests
make clean
```

## Deliberate scope + honest state

- **The engine ORCHESTRATES; the reproducible backends still do the heavy lifting.**
  The layer `.mk` files delegate to the proven `0N-*.sh` scripts, the provider
  Makefiles, and `build.sh`'s media assembly. Reimplementing the fetch/verify/
  pyenv/host-make logic in Make would be a large rewrite for zero behavior gain;
  forge's win is *reusable engine + Make dependency graph + thin product Makefile*.
- **Mixed rootfs (e.g. gv3 libc + BusyBox utils)** is the point of splitting
  coreutils into its own provider, but it needs an overlay-staging assembler that
  isn't wired yet — `providers.mk` errors clearly rather than build the wrong thing.
- **The engine is board-agnostic; the board is an input.** forge/*.mk no longer
  bakes in "t113" — each provider's build-target comes from
  `board/<board>/board.mk` (`KERNEL_TARGET`/`ROOTFS_TARGET`), resolved in
  `providers.mk` and passed down. The board's memory/storage layout is
  `board/<board>/layout.env`; its DT overlays + `image.cfg` live alongside.
- **The kernel source still carries the SoC address sets** (GIC/UART/timer bases
  in `kernel/include/board.h`, selected by `BOARD=t113|virt`). Those are
  chip-level, not board-level — a second T113 board wouldn't change them — so they
  stay put until a `soc/` tier is justified (below), rather than being pushed into
  per-board config prematurely.
- **No `soc/` tier yet** — deliberately deferred until a second board reveals which
  facts are genuinely chip-wide vs board-wide (you can't extract a shared tier from
  a sample size of one).

_The migration plan + rationale: `docs/LINUX_BUILD_SYSTEM_REFACTOR.md`._
