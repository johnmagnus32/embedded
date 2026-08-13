# Linux Providers as Recipes: Unifying Kernel/U-Boot with the Package Model

> **Later note:** the `LCD=ili9341` build knob mentioned below was subsequently REMOVED
> — a peripheral is board data (DT + kernel-config fragments), not a product axis. The
> `make image` matrix no longer takes an `LCD=` selector; the ILI9341 panel is now a
> board-applied fragment pair. References to `LCD=ili9341` below are the accurate record
> of *this* refactor's verification (when the knob still existed), left as-is.

**Status:** IMPLEMENTED (2026-08-01). The kernel + bootloader layers now build
through the uniform (fetch-method × build-style) contract described here, driven by
declarative recipes at `providers/<role>/<impl>/provider.mk`. What shipped, and the
one deliberate deviation from the letter of §4/§7:

- **Recipes (§3.2–3.3):** `providers/{kernel,bootloader}/{custom,mainline|uboot}/provider.mk`
  — bare `KEY=value` (`PKG_CLASS`/`PKG_ROLE`/`PKG_FETCH`/`PKG_TYPE`/`PKG_SOURCE`|`PKG_VERSION`+URLs/`PKG_ARTIFACT`).
- **Engine reads them (§3.4):** `forge/providers.mk` derives `*_STYLE`/`*_FETCH`/`*_SRC`/
  `*_MAKE_GOALS` from the selected recipe (helper `_recipe_get`); the `$(if $(filter
  custom…))` identity branches are gone. `forge/kernel.mk`/`bootloader.mk` are thin
  callers of the shared driver `forge/layer.mk`, which dispatches on `STYLE`
  (`make-c` | `kconfig`) — never on provider name.
- **Pins relocated (§1.4, §5):** `KERNEL_TAG`/`UBOOT_TAG` + URLs moved out of
  `versions.env` into the recipes (`kernel.sh`/`uboot.sh` read them from
  `PROVIDER_RECIPE` via `recipe_get`, with a `versions.env` fallback for a bare
  standalone `./kernel.sh`). The dead `BUSYBOX_*` block is deleted; `versions.env`
  now holds only the two artifact filenames.
- **Composer reads `PKG_ARTIFACT` (§4.5):** `resolve_artifacts` (lib.sh) decodes each
  recipe's `PKG_ARTIFACT` (`src:`/`out:` base scheme) instead of hardcoding paths.

- **DEVIATION from §4.3/§7.3 — kept THREE kconfig backends, not one.** §4.3 proposed
  collapsing mainline kernel + U-Boot *onto* `build-kconfig.sh`. That is actually
  *less* faithful to Buildroot/Yocto, which keep `linux.mk`, `uboot.mk`, `busybox.mk`
  as SEPARATE recipes that share only the **configure step** (`pkg-kconfig.mk`), not
  one build script — kernel/U-Boot/busybox have sharply different fetch/build/install
  bodies. So the shared unit extracted is `lib.sh:kconfig_configure` (make defconfig →
  provider fixup hook → normalize), which `kernel.sh`, `uboot.sh`, AND
  `build-kconfig.sh` all call; each keeps its own build/install. Same de-duplication
  goal, structure that matches the upstreams. Verified: `.config` (kernel + U-Boot)
  and the board DTB are byte-identical pre/post; golden 6/6; dynamic.sh 2/2; the full
  `make image` selector matrix (incl. `LCD=ili9341`, `MEDIA=sd`) composes unchanged.

Original proposal follows (kept for the rationale; the boxed status above is the
as-built record).

---

This reconciles the two existing
build-system docs, which today sit on opposite sides of an unstated seam:
- `docs/LINUX_BUILD_SYSTEM_REFACTOR.md` — the **provider** model
  (`KERNEL=custom|mainline`, `BOOTLOADER=custom|uboot`), where the kernel and
  bootloader are selected via an `ifeq` in `forge/kernel.mk` / `forge/bootloader.mk`.
- `docs/LINUX_ROOTFS_PACKAGE_MODEL.md` — the **package** model, where each rootfs
  component is a declarative `packages/<name>/package.mk` (five facts) built by a
  `PKG_TYPE` build-style backend (`forge/backends/build-*.sh`).

Neither doc observes that these two ideas should **converge**: a mainline Linux
kernel and mainline U-Boot are *also* fetch + kconfig builds, structurally
identical to `packages/busybox/package.mk`. This doc proposes giving the kernel
and bootloader providers a `package.mk`-style recipe and flowing all fetch/build
logic through **one contract** — while preserving the category distinction that
Buildroot and Yocto both enforce (providers are single-select boot-artifact
producers; packages are additive rootfs content).

Intended to be executable by another Claude window with no prior context. Verify
every claim against the tree before acting — some of it motivates a change and may
already have drifted.

---

## 1. The problem

### 1.1 Two dispatch shapes for what is really one operation

The kernel/bootloader layers dispatch on **provider identity**:

```makefile
# forge/kernel.mk (and bootloader.mk is the same shape)
ifeq ($(KERNEL),custom)
build:  @$(MAKE) -C $(KERNEL_SRC) BOARD=$(KERNEL_TARGET)   # local dir: just make it
else
build:  @$(BACKEND_ENV) $(BACKENDS)/kernel.sh              # mainline: fetch+configure+build
endif
```

The `ifeq` exists because the two providers have different **lifecycles**: the
custom kernel is a source dir already in the repo (`make -C`), while mainline must
be *fetched, verified, defconfig'd, tweaked, built, and have its DTB overlaid*.
But branching on "which provider is this" is exactly what Buildroot and Yocto
refuse to do — the engine ends up knowing each provider's internals.

### 1.2 The two OSS backends are ~60–70% hand-copied

`forge/backends/kernel.sh` and `forge/backends/uboot.sh` share nearly the same
skeleton: pinned clone-or-reuse (via the shared `clone_or_reuse_pinned` helper),
pristine-DTS restore, `make <defconfig>`, `scripts/config` tweak + `olddefconfig`
+ grep-verify, DTS `#include` overlay, `make -j`, assert artifact, copy to
`OUTPUT_DIR`, report heredoc. Only the provider-unique payload differs:

- **kernel:** disable `CONFIG_GCC_PLUGINS` (host lacks `gmp.h`); optional ILI9341
  DRM/fbdev + LCD DT overlay (`LCD=ili9341`).
- **U-Boot:** binman/pylibfdt python venv bootstrap; `CONS_INDEX=1` UART0 console;
  SPI-NOR MTD-first Kconfig; control-DTB console + NOR overlays.

### 1.3 The unifying contract already exists — but only for the rootfs layer

The package-model migration extracted the "kconfig build style" into
`forge/backends/build-kconfig.sh` (defconfig → sed `.config` tweaks →
`olddefconfig` + verify → cross build → install). `packages/busybox/package.mk`
declares `PKG_TYPE=kconfig` and rides it. **A mainline kernel and mainline U-Boot
are both kconfig-style builds** — they should ride the same style, with their
unique config edits expressed as fragments/hooks (exactly as `build-kconfig.sh`
already applies `PKG_KCONFIG_DISABLE=CONFIG_TC` for BusyBox). Today they don't:
the migration reached the rootfs layer only.

### 1.4 The version-pin smell that motivated this

`projects/gameboy-v3/versions.env` currently holds:
- `KERNEL_TAG` / `KERNEL_GIT_URL`, `UBOOT_TAG` / `UBOOT_GIT_URL` — **LIVE**, read
  by `kernel.sh` / `uboot.sh`.
- `BUSYBOX_VERSION` / `_URL` / `_SHA256` / `_TARBALL` — **DEAD**. Grep confirms no
  consumer outside `versions.env`; BusyBox's real pin lives in
  `packages/busybox/package.mk` (`PKG_VERSION` / `PKG_SITE` / `PKG_SHA256`), which
  `rootfs-assemble.sh` reads.

So BusyBox already carries its pin *in its recipe* (Buildroot/Yocto style), while
the kernel/U-Boot pins sit in a product-level env file *because they have no
recipe to carry them*. The dead BusyBox block is the visible symptom of the seam:
one component migrated, two did not.

---

## 2. How Buildroot and Yocto do it (the target these mirror)

Both tools express kernel, U-Boot, and BusyBox as recipes in **the same build
machinery** — but keep the kernel/bootloader in a **separate category** from
rootfs packages.

**Yocto:**
- `meta/recipes-kernel/linux/linux-yocto_*.bb` — `inherit kernel`
- `meta/recipes-bsp/u-boot/u-boot_*.bb` — `inherit uboot-config`
- `meta/recipes-core/busybox/busybox_*.bb` — a normal recipe
- The kernel is selected once via `PREFERRED_PROVIDER_virtual/kernel`; you do
  *not* list kernels in `IMAGE_INSTALL`. Its version is pinned per-product via the
  recipe / `PREFERRED_VERSION`.

**Buildroot:**
- `linux/linux.mk` — kernel, on the generic-package infra
- `boot/uboot/uboot.mk` — U-Boot, on the generic-package infra
- `package/busybox/busybox.mk` — BusyBox, same infra
- Kernel and U-Boot are **not** under `package/` (which means "populates the
  rootfs `target/`"). They get their own top-level dirs, and their versions are a
  **mandatory per-product choice** in the defconfig
  (`BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE`,
  `BR2_TARGET_UBOOT_CUSTOM_VERSION_VALUE`) — there is no universal default.

**The invariant both enforce:** same recipe *format* and same build *machinery*
for all three, but the kernel/bootloader are a distinct *category* — single-select
producers of a **boot artifact** (composed into the image outside the rootfs),
not additive **rootfs content**.

---

## 3. The proposed model: providers as recipes

### 3.1 The category distinction (keep it — it is load-bearing)

| | rootfs package (`packages/`) | boot-artifact provider (kernel / bootloader) |
|---|---|---|
| Output goes | *into* the rootfs (`/bin/busybox`) | *outside* the rootfs (`zImage`, `u-boot-…-with-spl.bin`) |
| Consumed by | the rootfs assembler (`rootfs-assemble.sh`) | the image composer (`image.sh`) |
| Selection | **additive** set (`PACKAGES=busybox coreutils …`) | **single-select** axis (`KERNEL=`, `BOOTLOADER=`) |
| Buildroot home | `package/` | `linux/`, `boot/` |
| Yocto home | `recipes-core` etc. | `recipes-kernel`, `recipes-bsp` |

Because of this, the provider recipes must **not** go in `packages/` (that dir is
the additive rootfs catalog). They get a sibling category.

### 3.2 Recipe format — the same five facts, plus a class marker

A provider recipe is the same `KEY=value` five-facts file as a package, with two
additions: a class marker (so the engine knows it is a single-select boot-artifact
provider, not additive rootfs content) and an explicit fetch method (see §3.4).

```makefile
# providers/kernel/mainline/provider.mk  (illustrative)
PKG_NAME=linux
PKG_CLASS=provider              # single-select boot-artifact; NOT additive rootfs content
PKG_ROLE=kernel                 # which single-select axis this fills (kernel|bootloader)

# SOURCE — fetch method + pin (see §3.4). Was versions.env KERNEL_TAG/KERNEL_GIT_URL.
PKG_FETCH=git
PKG_GIT_URL=https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git
PKG_GIT_URL_MIRROR=https://github.com/gregkh/linux.git
PKG_VERSION=v6.12.95

# BUILD — the kconfig style, shared with busybox.
PKG_TYPE=kconfig
PKG_KCONFIG_DEFCONFIG=sunxi_defconfig
PKG_KCONFIG_DISABLE=CONFIG_GCC_PLUGINS      # host lacks gmp.h
# board defconfig / DTB / console come from board/<board>/board.conf, not here.

# ARTIFACT — what the composer picks up (board fact: zImage vs Image).
PKG_ARTIFACT=zImage
```

```makefile
# providers/kernel/custom/provider.mk  (the from-scratch kernel)
PKG_NAME=gv3kernel
PKG_CLASS=provider
PKG_ROLE=kernel
PKG_FETCH=local                 # source is the repo-root kernel/ provider; no fetch
PKG_SOURCE=kernel
PKG_TYPE=make-c                 # a self-contained provider: `make -C` it
PKG_ARTIFACT=build/$(KERNEL_TARGET)/gv3kernel.bin
```

The provider-unique payload that does **not** fit the generic kconfig style
(kernel: ILI9341 LCD hook; U-Boot: binman venv, SPI-NOR MTD-first ordering,
control-DTB overlays) is expressed as **hooks/config fragments** the build style
applies — the same mechanism `build-kconfig.sh` already uses for BusyBox's
`PKG_KCONFIG_DISABLE`. Keep these as named optional fragments, not new `ifeq`s.

### 3.3 Directory layout

Mirror Buildroot's `linux/` + `boot/` (separate from `package/`) and Yocto's
`recipes-kernel` + `recipes-bsp` (separate from `recipes-core`):

```
packages/                     ← additive rootfs catalog (unchanged)
  busybox/package.mk
  coreutils/package.mk
providers/                    ← NEW: single-select boot-artifact provider recipes
  kernel/
    custom/provider.mk        ← PKG_FETCH=local  PKG_TYPE=make-c   -> repo-root kernel/
    mainline/provider.mk      ← PKG_FETCH=git    PKG_TYPE=kconfig  (was versions.env + kernel.sh)
  bootloader/
    custom/provider.mk        ← PKG_FETCH=local  PKG_TYPE=make-c   -> repo-root bootloader/
    uboot/provider.mk         ← PKG_FETCH=git    PKG_TYPE=kconfig  (was versions.env + uboot.sh)
```

The repo-root component dirs (`kernel/`, `bootloader/`, `libc/`, `coreutils/`)
stay exactly where they are — they are the **source** the `local` recipes point
at (Buildroot `SITE_METHOD=local` / Yocto `externalsrc`). The `providers/` recipes
are thin metadata, like `packages/*/package.mk`.

### 3.4 The two orthogonal axes (fetch method × build style)

This is the generalization of the package model up to the kernel/bootloader layer.
The `ifeq custom/mainline` collapses into two orthogonal, one-place dispatchers:

- **Fetch method** (`PKG_FETCH`): `local` (no-op; the source is a repo-root dir)
  vs `git` / `tarball` (the shared `clone_or_reuse_pinned` / `fetch_verify`
  helpers in `lib.sh`). This is the *only* legitimate "custom vs upstream" branch,
  and it lives in ONE place, not per-layer.
- **Build style** (`PKG_TYPE`): `kconfig` (mainline kernel, mainline U-Boot,
  BusyBox — one shared `build-kconfig.sh`), `make-c` (self-contained custom
  providers — `make -C`), plus the existing `compile-c` / `substrate` styles.

`forge/kernel.mk` and `forge/bootloader.mk` lose their `ifeq` and become a single
recipe that delegates to a generic provider driver, which reads the selected
recipe and runs `fetch(method) → configure/build(style) → emit-artifact`. The
driver never branches on provider *name*.

---

## 4. Concrete changes

1. **Create `providers/`** with the four recipes in §3.3. Move the live
   `KERNEL_TAG`/`KERNEL_GIT_URL`/`UBOOT_TAG`/`UBOOT_GIT_URL` pins **out of**
   `versions.env` and **into** the `mainline`/`uboot` recipes (BusyBox already
   does this; this makes kernel/U-Boot consistent).
2. **Add `PKG_FETCH` + `PKG_TYPE=make-c`** handling to the build-style dispatcher
   (the `local`/`make-c` path is trivial — it is today's `ifeq custom` branch).
3. **Move mainline kernel + U-Boot onto `build-kconfig.sh`**, expressing their
   unique config edits as fragments/hooks. Retire the bodies of `kernel.sh` /
   `uboot.sh` into: (a) the shared kconfig style, (b) per-provider fragment scripts
   for the genuinely unique bits (ILI9341 hook; binman venv + SPI-NOR + overlays).
4. **Replace the `ifeq`** in `forge/kernel.mk` / `forge/bootloader.mk` with the
   uniform (fetch-method × build-style) delegation to the generic driver.
5. **Teach `image.sh` (the composer)** to read `PKG_ARTIFACT` from the selected
   provider recipe instead of hardcoding artifact paths — the composer asks the
   provider where its output landed (see the composer cleanup in
   `LINUX_BUILD_SYSTEM_REFACTOR.md` Phase-3-of-the-audit direction).

---

## 5. What this deletes / resolves

- **Deletes the dead `BUSYBOX_*` block** from `versions.env` (already unread).
- **Empties `versions.env` of provider pins** — `KERNEL_TAG`/`UBOOT_TAG` move into
  their recipes. After this, `versions.env` may disappear entirely (its remaining
  keys `UBOOT_IMAGE` / `INITRAMFS_IMAGE` are artifact *filenames*, not versions —
  relocate them to `lib.sh` as engine defaults or to `board.conf` as board facts).
- **Removes the `ifeq`** dispatch from `kernel.mk` / `bootloader.mk`.
- **Removes ~60–70% duplication** between `kernel.sh` and `uboot.sh` (both collapse
  onto `build-kconfig.sh` + small fragment scripts).
- **Fixes the doc/code drift** at `LINUX_ROOTFS_PACKAGE_MODEL.md:466`, which still
  claims `versions.env` holds the busybox tag.

---

## 6. Non-goals and scope discipline

- **Do NOT build an N-provider plugin registry.** There are exactly two providers
  per layer (custom/mainline, custom/uboot). Build the fixed
  (fetch-method × build-style) contract Buildroot uses, not a speculative
  framework. This is the same "don't extract the abstraction until a second
  consumer forces it" discipline the rest of the build system follows — here the
  second consumer (kernel/U-Boot needing what BusyBox already has) *does* exist,
  which is what justifies the change; a third, hypothetical provider does not.
- **Do NOT put provider recipes in `packages/`.** That dir is the additive rootfs
  catalog; boot-artifact providers are single-select and go in `providers/`
  (Buildroot `linux/`+`boot/`; Yocto `recipes-kernel`+`recipes-bsp`).
- **Do NOT change any build outputs.** This is a pure restructuring: identical
  `zImage`, DTB, U-Boot image, and initramfs before and after.
- **Do NOT resurrect the deleted `forge/backends/rootfs.sh`** — it is superseded by
  the package-model assembler. (Context for whoever implements this.)
- The custom kernel/bootloader's board facts stay **provider-internal** (in
  `kernel/include/board.h`, `bootloader/`), exactly as
  `LINUX_BUILD_SYSTEM_REFACTOR.md` already states — only the *mainline* path reads
  `board.conf`'s defconfig/DTB/console keys.

---

## 7. Phasing (behavior-preserving; verify after each)

1. **Recipes without behavior change.** Create `providers/` recipes that *encode*
   the current selection but leave `kernel.mk`/`bootloader.mk` calling the existing
   backends. Prove the recipes parse and carry the right pins.
2. **Fetch-method + `make-c` style.** Route the `PKG_FETCH=local` / `make-c` path
   (today's `ifeq custom` branch) through the generic driver. Custom kernel +
   custom bootloader now build via the contract; mainline still via `kernel.sh` /
   `uboot.sh`.
3. **Mainline onto `build-kconfig.sh`.** Move mainline kernel + U-Boot onto the
   shared kconfig style with fragment hooks. Delete the `ifeq`. This is the
   largest phase — land it only once 1–2 are green.
4. **Cleanup.** Delete the dead `versions.env` block, relocate the artifact-name
   keys, and update `LINUX_ROOTFS_PACKAGE_MODEL.md:466` + this doc's status.

---

## 8. Verification (must match pre-change outputs exactly)

- `make -C projects/gameboy-v3 print-config` — resolved selection unchanged.
- `kernel/test/golden.sh` — all 6 cases pass (as before).
- `libc/test/dynamic.sh` — passes (it drives the rootfs assembler directly).
- `make image` across the selector matrix, byte-comparing artifacts before/after:
  - `KERNEL=custom BOOTLOADER=custom` (full custom stack)
  - `KERNEL=mainline BOOTLOADER=uboot` (all-OSS reference)
  - `LCD=ili9341` (mainline kernel LCD hook still fires)
  - `MEDIA=sd` (genimage SD path)

If any artifact differs, a config fragment/hook was dropped or reordered — diff the
generated `.config` against the pre-change build to find it.
