# Forge Reorg + Host-Package Model + the Dependency Resolver

**Status:** IMPLEMENTED (2026-08-02). All phases (§8 0–6) landed and verified; phase 7
(target-side resolver) is intentionally deferred until target packages gain real edges
(§3.4/§8.7) — `resolve.sh` already exists for it. What shipped, and the deviations from
the letter of the spec:

- **Phase 0 reorg (§2):** engine → `forge/core/`; `packages/` + `providers/` moved to
  `forge/{packages,providers}/`; two-root addressing (`FORGE_ROOT` for catalogs,
  `REPO_ROOT` for custom-provider source) wired through lib.sh/providers.mk/the provider
  build.sh scripts/the gameboy Makefile/the test harnesses. Custom SOURCE (`kernel/ libc/
  bootloader/ coreutils/`) stayed at the repo root as specified.
- **Phase 1 resolver (§3):** `forge/core/lib/resolve.sh` — `walk_deps`, a neutral
  3-color-DFS closure-walker (topo order, cycle detection, dedup), with a `--selftest`.
- **Phase 2–6 host subsystem (§4):** 7 recipes in `forge/hostpackages/*` + 4 host-styles
  (`host-tarball-bin`/`host-autotools`/`host-cc`/`host-pyvenv`) + `host.sh` (host_provision,
  a thin resolve.sh client). `make toolchain`, the SD/genimage path, the rootfs
  gen_init_cpio, and the U-Boot binman-venv all cut over to `host_provision`; the ~40-line
  venv block left the U-Boot build.sh (§6). Providers DECLARE `PKG_HOST_DEPENDS`; the layer
  driver provisions the declared closure (declare-don't-call, §4.4). `setup_*` retired from
  toolchain.sh; host-tool pins moved from lib.sh into the recipes; `make host-clean` added.

- **DEVIATIONS (deliberate, all behavior-preserving):**
  1. **Host prefix not physically consolidated** (§4.2 COMPATIBILITY note honored): each
     host tool still installs into TODAY's dir (`build/{toolchain,toolchain-musl,hostmake,
     hosttools,pyenv}`), named by the recipe's `PKG_HOST_DEST`; `HOST_PREFIX`/`.stamps` are
     added and the stamps live under `build/host/`. Physically moving every tool under
     `build/host/{bin,lib}` was NOT done — it would change cc-profile's `CROSS_COMPILE`
     resolution and buys nothing functional. The var-name aliases the doc suggested are
     unneeded because the dirs are unchanged.
  2. **Idempotency = stamp AND artifact-witness**, not stamp alone (§4.5 strengthened): a
     current stamp with a missing artifact (partial `build/` wipe) correctly re-provisions.
     Found + fixed during Phase 4.
  3. **Host recipe values that are multi-word are QUOTED** (`PKG_HOST_CONFIGURE_FLAGS`,
     `PKG_HOST_CONFIGURE_ENV`, `PKG_PYMODULES`): host recipes are bash-`source`d by host.sh
     (never Make-parsed, unlike package.mk), so quoting is correct + necessary. `PKG_HOST_SKIP_IF`
     is a simple command (a lib.sh helper `host_have_make_ge4`), not an inline shell `case`.

Verified: `make print-config` (both stacks) byte-identical to pre-reorg; golden 6/6;
dynamic.sh 2/2; full-custom + all-OSS + MEDIA=sd `make image`; U-Boot `.config`
byte-identical; the LAZINESS HEADLINE (§4.6) — a full-custom build provisions NO genimage
+ NO binman-venv + NO libconfuse (stamp set asserted); `make host-clean` + re-provision.

- **FOLLOW-UP (gen_init_cpio: vendored → FETCHED).** The original cut vendored the kernel's
  `usr/gen_init_cpio.c` into `forge/core/hosttools/` and compiled the copy (`host-cc` reading
  `PKG_HOST_SRC`). That copy-paste is now removed: `gen_init_cpio` is FETCHED like every other
  host tool — the recipe pins `PKG_SITE` (kernel.org cgit plain) + `PKG_SITE_MIRROR` (gregkh
  GitHub) + `PKG_SOURCE_QUERY=?h=<tag>` + `PKG_SHA256` (309cab96…); `host-cc.sh` fetch_verifies
  the single .c (primary→mirror) then `cc`-compiles it. It has no standalone release, but it IS
  fetchable as a subset of the kernel source — the Buildroot host-uboot-tools shape (fetch the
  upstream, build one small part). The single-file fetch keeps a full-custom build from dragging
  in the whole kernel. `forge/core/hosttools/` is deleted; kernel/Makefile + golden.sh + dynamic.sh
  no longer compile a vendored copy — they take the engine-provisioned tool (golden.sh/dynamic.sh
  run the product `make toolchain` to provision it, then pass `GEN_INIT_CPIO=`). Verified: fetched
  file byte-identical to the old vendored body; compiled tool produces identical cpios; golden 6/6.

Original proposal follows (kept for the rationale; the boxed status above is the
as-built record).

---

A design spec for
another engineer/agent to execute. It covers THREE changes that are one coherent
piece of work, in this order:

0. **Reorg** — move all OS-building material under one umbrella `forge/`: the engine
   mechanism into `forge/core/`, and the three catalogs (`packages/`, `providers/`,
   `hostpackages/`) in as peers of the engine. De-clutters the repo root and matches
   Buildroot's single-tree model. **Do this FIRST** so everything below targets final
   paths and nothing moves twice.
1. **Dependency resolver** (`forge/core/lib/resolve.sh`) — a neutral,
   class-agnostic closure-walker — plus the **three dependency relations** every
   recipe declares.
2. **Host-package subsystem** (`forge/hostpackages/` + `forge/core/lib/host.sh`)
   built on that resolver, turning today's ad-hoc host-tool provisioning into
   declared-dependency, graph-derived provisioning (like Buildroot `host-*` / Yocto
   `-native`).

The resolver is shared by BOTH host provisioning AND (when target packages grow real
inter-dependencies) target rootfs assembly — built generically from day one so the
target side adopts it without a rewrite.

> **How to use this doc:** it is self-contained. Read §0 (current state — what exists,
> which files to read) FIRST. Implement in the phase order of §8: Phase 0 is the reorg,
> Phases 1–7 the resolver + host packages. Every path below is the FINAL (post-reorg)
> path. Every claim about the current tree was true at writing — **verify each file
> still matches before editing**; the codebase moves.

---

## 0. Current state (read these first — no prior context assumed)

Forge is a from-scratch embedded-Linux build engine. A product
(`projects/gameboy-v3/`) sets selectors in `config.mk` and `include`s the engine's
top makefile. There are already THREE component models:

- **`packages/<name>/package.mk`** — additive **target** rootfs content (busybox,
  coreutils). Bare `KEY=value` "five facts" recipes. Built by a `PKG_TYPE` build-style
  backend, assembled by the rootfs assembler.
- **`providers/<role>/<impl>/provider.mk`** — single-select **boot artifacts**:
  `kernel/{custom,mainline}`, `bootloader/{custom,uboot}`. Same recipe shape +
  `PKG_CLASS=provider`. A kconfig provider (mainline kernel, uboot) ships its own
  `build.sh` beside the recipe; a make-c provider (custom kernel/bootloader) is
  `make -C`'d by `build-make-c.sh`.
- **`libc/`** — the substrate provider (from-source gv3libc) or musl (prebuilt).
  Selected by the `LIBC` axis, NOT a package. Ships `libc/build.sh` +
  `libc/libc-profile.sh` (its compile/link contract). This is the "libc is special —
  an axis, not a graph node" pattern (see §7).

### 0.1 Repo root TODAY (what the reorg changes)

```
bootloader/  cad/  coreutils/  dbg/  docs/  forge/  kernel/  libc/
packages/  projects/  providers/  rtos/  sim/  tools/
```

`forge/` currently means "the engine" and holds: `rules.mk providers.mk layer.mk
kernel.mk bootloader.mk rootfs.mk image.mk`, plus `backends/` (shell mechanism +
build styles), `defaults/`, `hosttools/` (vendored `gen_init_cpio.c`), `README.md`.
The catalogs `packages/`, `providers/` and the custom-provider SOURCE dirs
(`kernel/`, `bootloader/`, `libc/`, `coreutils/`) sit at the root beside it.

### 0.2 Files to read before editing (current paths)

- `forge/backends/lib.sh` — shared shell mechanism: build-tree layout, host-tool pins
  + dirs (§0.4), sources the product's `board.conf`, defines helpers (`fetch_verify`,
  `clone_or_reuse_pinned`, `recipe_get`, `apply_config_fragment`, …). Computes
  `FORGE_DIR` + `REPO_ROOT` by self-location (lines ~41–47): `FORGE_DIR` = its own dir
  up one (`forge/`), `REPO_ROOT` = up two.
- `forge/providers.mk` — resolves the selected recipe. Computes `FORGE_DIR` =
  `$(dir $(lastword MAKEFILE_LIST))`, `REPO_ROOT` = `$(FORGE_DIR)/..`. Recipe path
  helper `_recipe = $(REPO_ROOT)/providers/$(1)/$(2)/provider.mk` (line ~46). Package
  surface-check reads `$(REPO_ROOT)/packages/$(1)/package.mk` (lines ~97–98).
- `forge/backends/toolchain.sh` — TODAY's host-tool provisioner (function library;
  the thing the subsystem replaces). Main runs `setup_host_make`,
  `setup_cross_toolchain`, `setup_gen_init_cpio` eagerly; `setup_genimage` is lazy.
- `forge/backends/rootfs-assemble.sh` — the target package loop. `PKGROOT=
  ${REPO_ROOT}/packages` (line ~52); `for pkg in $PACKAGES`; **flat list, user order,
  NO dep resolution** (§0.3). Local package source resolved as `${REPO_ROOT}/${PKG_SOURCE}`.
- `providers/kernel/mainline/build.sh`, `providers/bootloader/uboot/build.sh` — each
  self-locates: `HERE=<dirname>`, `REPO_ROOT="${HERE}/../../.."`, then `source
  "${REPO_ROOT}/forge/backends/lib.sh"`. The uboot one has the **binman venv inline**
  (~lines 53–90) — the thing §6 relocates.
- `projects/gameboy-v3/Makefile` — `REPO_ROOT := $(PRODUCT_DIR)/../..`; then
  `include $(REPO_ROOT)/forge/rules.mk` (line 18).
- `kernel/test/golden.sh`, `libc/test/dynamic.sh` — standalone harnesses that
  reference `forge/backends/rootfs-assemble.sh` etc. by path
  (`ASSEMBLE="${REPO_ROOT}/forge/backends/rootfs-assemble.sh"`, dynamic.sh line ~40).
- Sibling docs: `docs/LINUX_ROOTFS_PACKAGE_MODEL.md`, `docs/LINUX_PROVIDER_RECIPE_MODEL.md`.

### 0.3 The target dep model today (the gap §1–§2 close)

The assembler loops `for pkg in ${PACKAGES}` in the human-written order, with NO
inter-package resolution. The only "dependency" is `PKG_DEPENDS=libc` in
`packages/{busybox,coreutils}/package.mk` — a **sentinel meaning "the substrate,"**
not a real edge (libc is the axis, §7). Fine while every package depends only on libc,
but the user states target packages are about to multiply, so real edges are imminent.
The resolver (§2) is built now so those edges are expressible with no recipe rewrite.

### 0.4 The six host tools today, and where their pins/dirs live

All provisioned by `toolchain.sh` functions; all pins in `lib.sh`. Current values:

| Tool | becomes `PKG_TYPE` | current provisioner | pins in `lib.sh` | install dir in `lib.sh` |
|---|---|---|---|---|
| GNU Make ≥4 | `host-autotools` (conditional) | `setup_host_make` (eager) | `MAKE_VERSION=4.4.1`, `MAKE_TARBALL`, `MAKE_URL` (no SHA today) | `HOSTMAKE_DIR=$BUILD/hostmake` |
| glibc cross toolchain | `host-tarball-bin` | `setup_cross_toolchain` (eager) | `TOOLCHAIN_{LIBC,CHANNEL,VERSION,EXT,TARBALL,URL,SHA256}` (`2021.11-1`) | `TOOLCHAIN_DIR=$BUILD/toolchain` |
| musl cross toolchain | `host-tarball-bin` | `setup_cross_toolchain` (eager) | `ROOTFS_TC_{VERSION,EXT,TARBALL,URL,SHA256}` (`2021.11-1`) | `ROOTFS_TOOLCHAIN_DIR=$BUILD/toolchain-musl` |
| gen_init_cpio | `host-cc` | `setup_gen_init_cpio` (**eager**) | (none — vendored source) | `HOSTTOOLS_DIR/bin`; source `forge/hosttools/gen_init_cpio.c` (present, untracked) |
| libconfuse | `host-autotools` | inside `setup_genimage` | `CONFUSE_{VERSION,TARBALL,URL,SHA256}` (`3.3`) | `HOSTTOOLS_DIR=$BUILD/hosttools` |
| genimage | `host-autotools` (dep: libconfuse) | `setup_genimage` (**lazy**, sourced by `image.sh` MEDIA=sd only) | `GENIMAGE_{VERSION,TARBALL,URL,SHA256}` (`18`) | `HOSTTOOLS_DIR=$BUILD/hosttools` |
| binman python venv | `host-pyvenv` | **inline in `providers/bootloader/uboot/build.sh`** | (none — module list) | `PYENV_DIR=$BUILD/pyenv` |

Note the inconsistency the subsystem fixes: three different laziness mechanisms
(eager / lazy-call-site / inline-in-a-provider) for one category. (`$BUILD` =
`$PRODUCT_DIR/build`; these are all under the product build tree, unaffected by the
reorg — the reorg moves the *source/recipe* tree, not the *build* tree.)

---

## 1. The problem (why this is worth doing now)

Two problems, tackled together because they touch the same files:

**A. Root-directory sprawl.** `forge/` (engine) + `packages/` + `providers/` +
`hostpackages/` (coming) + the four custom source dirs all sit at the root. The
OS-building material has no single home; "what is the Linux build system?" has no one
answer. **Fix: one umbrella `forge/` — engine in `forge/core/`, catalogs as peers**
(§0 reorg, this is Phase 0).

**B. Host tools are ad-hoc.** A function library with three different hand-wired
laziness mechanisms; nothing *declares* "U-Boot needs binman" or "the SD path needs
genimage" — a human wired each call site. Add a tool → hunt and edit its consumers.
Provisioning logic is smeared into consumer scripts (binman venv inside the U-Boot
build). No uniform recipe. **Fix: host tools become packages resolved by a dependency
graph** (§2–§3).

Directive from the user: **build the interfaces right up front** (the reorg's final
paths, the recipe schema, the resolver) — those are what every future recipe and
consumer is written against; retrofitting them is the expensive migration.
Implementation *depth* can grow behind stable interfaces.

---

## 2. Phase 0 — the reorg (do this first)

### 2.1 Target layout

```
forge/                         ← the umbrella: ALL OS-building material (the "workshop")
  core/                        ← the ENGINE mechanism  (was forge/*.mk + backends/ + defaults/ + hosttools/)
    rules.mk providers.mk layer.mk kernel.mk bootloader.mk rootfs.mk image.mk
    backends/                  lib.sh, toolchain.sh, build-*.sh, cc-profile.sh, rootfs-*.sh, image.sh,
                               + NEW resolve.sh, host.sh, host-styles/  (§3–§4)
    defaults/                  rootfs.devs, *-host.config fragments
    hosttools/                 gen_init_cpio.c (vendored source)
  packages/                    ← target catalog        (moved from repo-root packages/)
  providers/                   ← boot-artifact catalog (moved from repo-root providers/)
  hostpackages/                ← host-tool catalog     (NEW — §3)
  README.md                    ← "what forge is + how to add a package/provider/host-tool"
```

This mirrors Buildroot precisely: `buildroot/support/` ≈ `forge/core/`;
`buildroot/package/` ≈ `forge/packages/`; `buildroot/{boot,linux}/` ≈
`forge/providers/`; host packages ≈ `forge/hostpackages/`. The catalogs are PEERS of
the engine inside one tree — not nested in the engine's internals, not scattered at
the root.

**Name:** `core/` (not `engine/`). "Forge" is a workshop metaphor; a forge contains a
`core`, not an "engine" (that double-metaphors). `core/` is short, unambiguous, and
collides with nothing. (Buildroot uses `support/`; `core/` is clearer for a reader who
doesn't know Buildroot.) The metaphor shifts deliberately: **`forge/` now means "the
whole workshop," `core/` is the machinery inside** — say this in `forge/README.md`.

### 2.2 What MOVES vs what STAYS

**Moves INTO `forge/`:**
- `forge/*.mk` + `forge/backends/` + `forge/defaults/` + `forge/hosttools/` → `forge/core/…`
- repo-root `packages/` → `forge/packages/`
- repo-root `providers/` → `forge/providers/`

**STAYS at the repo root (important — do NOT move these):**
- `kernel/`, `bootloader/`, `libc/`, `coreutils/` — these are **source code**, not
  build-system. Buildroot never vendors the source of what it builds into the tool;
  our custom kernel/libc are *local source a recipe points at* (Buildroot
  `SITE_METHOD=local`). `kernel/` is a whole OS with its own `golden.sh`/`PLAN.md`;
  `libc/` has `ld/` + tests. The `providers/kernel/custom/provider.mk` recipe (which
  moves into forge) references them via `PKG_SOURCE=kernel` — a **repo-root-relative**
  path. Recipe-in-forge, source-at-root is the clean split.
- `projects/`, `tools/`, `docs/`, and the non-Linux domains `rtos/ sim/ cad/ dbg/`.

Result root: `forge/` (all build-system) + the 4 custom source dirs + `projects/
tools/ docs/ rtos/ sim/ cad/ dbg/`. The sprawl (the catalogs) is now inside `forge/`.

### 2.3 The addressing rule the reorg introduces (the crux)

The move splits addressing into **two roots**, and today's code conflates them under
one `REPO_ROOT`. After the reorg:

- **Catalogs** (packages, providers, hostpackages) are addressed relative to the
  **forge umbrella**: `FORGE_ROOT/{packages,providers,hostpackages}`.
- **Custom provider SOURCE** (kernel, libc, …) stays relative to the **repo root**:
  `REPO_ROOT/kernel`, `REPO_ROOT/libc` (what `PKG_SOURCE=kernel` resolves against).

So introduce a `FORGE_ROOT` (the umbrella `forge/`) distinct from `REPO_ROOT` (the
git root). Engine now lives at `forge/core/`, i.e. `FORGE_ROOT = <core>/..`,
`REPO_ROOT = FORGE_ROOT/..`.

### 2.4 Exact reference updates (grep-driven checklist)

Path/へ depth changes — engine is now THREE dirs deep (`forge/core/`), not two:

1. **`forge/core/lib/core.sh`** self-location: today `FORGE_DIR = <backends>/..`
   (= `forge/`), `REPO_ROOT = FORGE_DIR/..`. After: from `forge/core/`,
   `CORE_DIR = <backends>/..` (= `forge/core`), `FORGE_ROOT = CORE_DIR/..` (= `forge/`),
   `REPO_ROOT = FORGE_ROOT/..` (git root). Export `FORGE_ROOT`. Keep `FORGE_DIR` as an
   alias for `CORE_DIR` if other code reads it (grep first).
2. **`forge/core/providers.mk`**: `FORGE_DIR := $(dir …)` now = `forge/core/`. Add
   `FORGE_ROOT := $(abspath $(FORGE_DIR)/..)` (= `forge/`) and
   `REPO_ROOT := $(abspath $(FORGE_ROOT)/..)`. Change the recipe helper
   `_recipe = $(REPO_ROOT)/providers/…` → `$(FORGE_ROOT)/providers/…`; and the
   package surface-check `$(REPO_ROOT)/packages/…` → `$(FORGE_ROOT)/packages/…`.
3. **`forge/core/steps/rootfs-assemble.sh`**: `PKGROOT="${REPO_ROOT}/packages"` →
   `"${FORGE_ROOT}/packages"`. LOCAL package source stays
   `PKG_SRC_DIR="${REPO_ROOT}/${PKG_SOURCE}"` (repo-root — source, not catalog).
4. **`providers/*/build.sh`** (now `forge/providers/*/build.sh`): they self-locate as
   `REPO_ROOT="${HERE}/../../.."` and `source "${REPO_ROOT}/forge/backends/lib.sh"`.
   After the move the build.sh sits at `forge/providers/<role>/<impl>/build.sh`
   (FOUR dirs under repo root) and lib.sh is at `forge/core/lib/core.sh`. Recompute:
   `FORGE_ROOT="${HERE}/../../.."` (= `forge/`), source
   `"${FORGE_ROOT}/core/lib/core.sh"`. Update the recipe self-default
   (`PROVIDER_RECIPE:-${HERE}/provider.mk` is unaffected — it's beside build.sh).
5. **`projects/gameboy-v3/Makefile`**: `include $(REPO_ROOT)/forge/rules.mk` →
   `$(REPO_ROOT)/forge/core/rules.mk`.
6. **`kernel/test/golden.sh`, `libc/test/dynamic.sh`**: update every
   `${REPO_ROOT}/forge/backends/…` → `${REPO_ROOT}/forge/core/…`
   (`ASSEMBLE=`, the rootfs-pack ref, the toolchain hints, comments). These live under
   `kernel/`/`libc/` which STAY at root — only their forge-path strings change.
7. **Any `include $(dir …)/layer.mk`** or sibling `.mk` includes inside `forge/core/`
   are relative and unaffected; verify with a dry `make print-config`.
8. **Docs**: update the two sibling docs' path references + `forge/README.md`.

Grep to drive it (run BEFORE and AFTER):
```
grep -rn 'forge/backends\|forge/rules.mk\|forge/providers.mk\|/providers/\|/packages/' \
  --include='*.sh' --include='*.mk' --include='Makefile' --include='*.md' \
  forge/ projects/ kernel/ libc/ coreutils/ bootloader/ docs/
```

### 2.5 How to perform the move (git-history-preserving)

Use `git mv` so history follows (even though changes stay unstaged per the working
rule — `git mv` then leaves them staged; if the working rule forbids staging, do plain
`mv` and let the user stage later). Suggested order:
```
mkdir forge/core
git mv forge/*.mk forge/backends forge/defaults forge/hosttools forge/core/
git mv packages forge/packages
git mv providers forge/providers
# then apply §2.4 reference edits
```
Verify §9 (esp. `print-config` for both stacks) BEFORE moving on to Phase 1.

---

## 3. The dependency resolver (`forge/core/lib/resolve.sh`)

A neutral, class-agnostic closure-walker. Knows nothing about host vs target, what a
node *is*, or what building it *does*. It only walks edges.

### 3.1 The three dependency relations (the durable schema — commit to all three now)

Every recipe may declare up to three relations. Distinct because different
destinations + different machinery — conflating them forces a later rewrite:

| Relation | Meaning | Destination | Drives |
|---|---|---|---|
| `PKG_HOST_DEPENDS` | host tools needed to **build** me | `HOST_PREFIX` (on PATH) | host provisioning closure |
| `PKG_DEPENDS` | **target** packages needed to **build** me (headers, `.a`) | `SYSROOT` | build **order** (topological) |
| `PKG_RDEPENDS` | **target** packages needed to **run** me (`.so`, `dlopen` plugins, data) | `STAGE` (image) | image **install-set** (runtime closure) |

Example (`gv3metrics` daemon): a `.proto` compiler `protoc` is build-only, ships
nothing → `PKG_HOST_DEPENDS`; a `dlopen`'d plugin nothing links at build → runtime-only
→ `PKG_RDEPENDS`; `zlib` is both (headers to link, `.so` to run) → both `PKG_DEPENDS`
and `PKG_RDEPENDS`. One list can't express these; three fields can. Declare all three
in the schema from package #1 even though most are empty today (§4.1) — so the first
package with a real dep just fills one in.

### 3.2 `walk_deps` — the one algorithm

```
walk_deps <edge_reader_cmd> <root>...
  # edge_reader_cmd <node>  ->  prints <node>'s DIRECT deps, one per line (may be empty)
  # emits: ALL reachable nodes in TOPOLOGICAL order (every dep BEFORE its dependents),
  #        deduplicated.  Exits nonzero on a CYCLE, naming it.
```

Implementation: DFS with three-color marking (white=unseen, gray=on stack, black=done).
Gray-visited-while-gray ⇒ cycle. Post-order emission ⇒ topological order. A `seen` set
⇒ dedup. ~40 lines of pure shell, zero class knowledge — the reusable core. The
`edge_reader_cmd` indirection is what lets the SAME walker serve host deps, build-deps,
and runtime-deps: the caller picks which recipe key to read.

### 3.3 Three callers, one walker

| Caller | Reads | Node action |
|---|---|---|
| `host.sh` (§4) | `PKG_HOST_DEPENDS` from `forge/hostpackages/<n>/package.mk` | provision into `HOST_PREFIX` via its host `PKG_TYPE` style |
| rootfs — build set | `PKG_DEPENDS` from `forge/packages/<n>/package.mk` | compile; headers/libs → `SYSROOT`; shipped files → `STAGE` iff in image set |
| rootfs — image set | `PKG_RDEPENDS` from `forge/packages/<n>/package.mk` | closure only — what must physically be in the rootfs |

Buildroot/Yocto use ONE dep mechanism for host + target; class is a node attribute.
`resolve.sh` is that mechanism; `host.sh` and the assembler are thin clients. **Do NOT
put target resolution inside `host.sh`** — both call `resolve.sh`.

### 3.4 What the target assembler becomes (the three-phase loop)

Replaces the flat `for pkg in $PACKAGES` (§0.3). Wire the HOST caller now (§4, real
edges exist); wire the target build/image callers when target packages gain real edges
— but build `resolve.sh` generic enough to serve all three from day one.

```
1. image_set = walk_deps(read PKG_RDEPENDS, $PACKAGES)     # transitive RUNTIME closure
2. build_set = walk_deps(read PKG_DEPENDS, image_set)      # + build-only deps, topo-sorted
                                                           # image_set ⊆ build_set
3. for node in build_set (topo order):
     host_provision  node.PKG_HOST_DEPENDS                 # deduped globally, built once
     build node against CC profile
       install headers/libs  -> SYSROOT
       install shipped files -> STAGE   iff node ∈ image_set    # the DEPENDS≠RDEPENDS payoff
```

Until target packages have real edges, image_set = `$PACKAGES` and build_set = same in
list order ⇒ behavior identical to today; introduce without changing output. Value
switches on automatically at the first real edge.

---

## 4. The host-package subsystem

A host tool is **a package that installs into a host prefix instead of the target
rootfs**. Extend the existing package model; don't invent a parallel one.

### 4.1 Recipe format — `forge/hostpackages/<name>/package.mk`

```makefile
# forge/hostpackages/genimage/package.mk
PKG_NAME=genimage
PKG_CLASS=host                 # installs into HOST_PREFIX, ships NOTHING to target
PKG_VERSION=18                 # (matches lib.sh GENIMAGE_VERSION today)
PKG_SITE=https://github.com/pengutronix/genimage/releases/download/v18
PKG_SOURCE=genimage-18.tar.xz
PKG_SHA256=<from lib.sh GENIMAGE_SHA256>
PKG_TYPE=host-autotools
PKG_HOST_DEPENDS=libconfuse    # host→host edge
```

```makefile
# forge/hostpackages/binman-venv/package.mk
PKG_NAME=binman-venv
PKG_CLASS=host
PKG_TYPE=host-pyvenv
PKG_PYMODULES=setuptools pyelftools pyyaml importlib_resources
# no PKG_VERSION/SITE/SHA — a pyvenv pins via its module list.
```

Empty relation fields are omitted; the schema still admits them. Host recipes use
`PKG_HOST_DEPENDS` for host→host edges; `PKG_DEPENDS`/`PKG_RDEPENDS` are target
relations and normally absent here.

### 4.2 The host prefix — unify today's five scattered dirs into one

```
build/host/
  bin/      genimage, gen_init_cpio, make (if built), python (venv), arm-*-gcc (toolchains)
  lib/      libconfuse.a, pkgconfig/
  include/  confuse.h
  .stamps/  <name>-<version>          (idempotency markers — §4.5)
```

Replaces `build/{toolchain,toolchain-musl,hostmake,hosttools,pyenv}`. `lib.sh` defines
`HOST_PREFIX=$BUILD_DIR/host` and prepends `build/host/bin` to PATH (it already
prepends the pieces piecewise — consolidate). Host build styles install
`--prefix=$HOST_PREFIX`; the prebuilt cross toolchains extract into
`$HOST_PREFIX/<triple>/` with their `bin/` on PATH (preserve today's exact extracted
layout so `cc-profile.sh`/`CROSS_COMPILE` resolution is unchanged).

> COMPATIBILITY: many places read `TOOLCHAIN_DIR`, `ROOTFS_TOOLCHAIN_DIR`,
> `HOSTTOOLS_DIR`, `HOSTMAKE_DIR`, `PYENV_DIR`, `GEN_INIT_CPIO`. Do NOT break them in
> one step — keep the var names as aliases into `build/host/` during migration, or
> grep every consumer first
> (`grep -rn 'TOOLCHAIN_DIR\|HOSTTOOLS_DIR\|PYENV_DIR\|GEN_INIT_CPIO\|HOSTMAKE_DIR' forge/ kernel/ libc/`).

### 4.3 Host build styles (one backend per `PKG_TYPE`) — under `forge/core/styles/host/`

Lift bodies almost verbatim from the current `toolchain.sh` functions — RELOCATION
into a uniform shape, not new logic:

| `PKG_TYPE` | For | Backend does | Lifted from |
|---|---|---|---|
| `host-tarball-bin` | cross toolchains | fetch+verify+extract prebuilt tarball; sanity-check compiler runs & emits ARM | `setup_cross_toolchain`/`fetch_toolchain` |
| `host-autotools` | genimage, libconfuse, make | fetch+verify+extract; `./configure --prefix=$HOST_PREFIX && make && make install` | `setup_genimage`, `setup_host_make` |
| `host-cc` | gen_init_cpio | `cc -O2 <vendored.c> -o $HOST_PREFIX/bin/<name>` | `setup_gen_init_cpio` |
| `host-pyvenv` | binman-venv | probe a dev-header python NOT the toolchain's; `python -m venv`; `pip install --index-url https://pypi.org/simple/ <PKG_PYMODULES>` | the U-Boot `build.sh` venv block |

Special cases to carry faithfully:
- **make is conditional** — only built if the host's own `make` is <4.0 (today's
  `setup_host_make` short-circuit). Model as: the recipe's style no-ops when the host
  tool already satisfies the requirement.
- **host-pyvenv** keeps the three traps: never the toolchain's bundled python; require
  `Python.h` (pylibfdt compiles a C extension); install from real PyPI (`--index-url`,
  because the host default index is a creds-gated mirror).

### 4.4 The resolver (`forge/core/lib/host.sh`)

`host_provision <name>...`:
1. Closure: `walk_deps(read PKG_HOST_DEPENDS from forge/hostpackages/<n>/package.mk, <name>...)`.
2. Each node in topo order: skip if stamp current (§4.5); else dispatch to its
   `PKG_TYPE` host-style backend → install into `HOST_PREFIX` → drop stamp.
3. Idempotent; safe to call repeatedly.

Who declares:
- **Provider/package recipes** name build-time host tools:
  `forge/providers/bootloader/uboot/provider.mk` → `PKG_HOST_DEPENDS=binman-venv`;
  `forge/providers/{kernel/mainline,bootloader/uboot}` → `toolchain-glibc make`;
  `forge/packages/*` + `libc` (musl-built) → `toolchain-musl`; the rootfs assembler →
  `gen_init_cpio`.
- **Non-recipe steps** (SD image) declare at the call point: `image.mk` MEDIA=sd →
  `host_provision genimage`.

**DONE (§8 phase 5, 2026-08-05): declare-don't-call is now the real flow.** The layer
driver reads `PKG_HOST_DEPENDS` (→ `providers.mk` → `HOST_DEPENDS`) and satisfies it BEFORE
invoking the provider build step: `layer.mk` RUNS `forge/core/lib/host.sh` (its
executed-path guard: source toolkit + define log/die, `host_provision $HOST_DEPENDS`) ahead
of BOTH the make-c style and the kconfig `build.sh`. (`host.sh` is a LIBRARY when the drivers
source it, a COMMAND when layer.mk runs it — one file, dual role, like `toolchain.sh`.) The build steps no longer provision — they
`source lib/core.sh` (which puts the just-built tools on PATH by PRESENCE) and PREFLIGHT that
their compiler/venv is present, dying with a "run make toolchain" hint if invoked standalone
without prior provisioning. So the trigger lives in the engine, matching Buildroot's
host-dep-as-prerequisite model. Removed: the `host_provision` calls + `source host.sh` from
`styles/make-c.sh`, `providers/kernel/mainline/build.sh`, `providers/bootloader/uboot/build.sh`
(the uboot PATH/`$PYTHON` export went too — `lib/core.sh` already does it by presence, which
now fires because the engine provisioned binman-venv first). NOT changed: `steps/rootfs-assemble.sh`
still provisions `gen_init_cpio` and `image.sh` still provisions `genimage` — those are DRIVERS
(they resolve+loop+dispatch), not leaf build steps, so provisioning their own tool is the same
role `layer.mk` plays, not a declare-don't-call violation.

### 4.5 Idempotency / clean
- Each node writes `build/host/.stamps/<name>-<version>` (or a module-list hash for
  pyvenv). `host_provision` skips a current stamp. Mirrors today's `.deps-ok` /
  "already extracted" checks.
- `make host-clean` removes `build/host/`; `make clean` includes it.
- Pins live in each recipe (moved out of `lib.sh`, §8 phase 6). `lib.sh` keeps only
  `HOST_PREFIX` + host-arch defaults (`TC_ARCH`, `CROSS_COMPILE`,
  `ROOTFS_CROSS_COMPILE` — board/host facts, not host-tool pins).

### 4.6 Laziness = the declared closure (the property to PRESERVE)

The user's explicit requirement: **a full-custom build downloads no Python and no
genimage.**

- `make image KERNEL=custom BOOTLOADER=custom LIBC=custom` provisions only: `make`
  (if host's <4) + `toolchain-glibc` + `toolchain-musl` + `gen_init_cpio`.
  **NOT `genimage`** (no SD), **NOT `binman-venv`** (no mainline U-Boot).
- `MEDIA=sd` adds `genimage` (+ `libconfuse`).
- `BOOTLOADER=uboot` adds `binman-venv`.

---

## 5. Directory layout (end state, post everything)

```
forge/
  core/
    rules.mk providers.mk layer.mk kernel.mk bootloader.mk rootfs.mk image.mk
    lib/       core.sh (was lib.sh)  ccprofile.sh (was cc-profile.sh)
      resolve.sh                      walk_deps (neutral closure-walker), §3
      host.sh                         host_provision, §4.4 (uses lib/resolve.sh); dual role:
                                      LIBRARY when sourced by the steps, COMMAND when layer.mk
                                      RUNS it to satisfy declared HOST_DEPENDS (§4.4 phase 5)
    styles/    make-c.sh kconfig.sh compile-c.sh substrate.sh   (was build-*.sh)
      host/    host-tarball-bin.sh host-autotools.sh host-cc.sh host-pyvenv.sh   §4.3
    steps/     toolchain.sh  rootfs-assemble.sh (inlines its pack tail)  image.sh
    defaults/    rootfs.devs, *-host.config
    hosttools/   gen_init_cpio.c
  packages/      busybox/  coreutils/                     (moved in, Phase 0)
  providers/     kernel/{custom,mainline}/  bootloader/{custom,uboot}/  (moved in, Phase 0)
  hostpackages/  make/ toolchain-glibc/ toolchain-musl/ libconfuse/       NEW
                 genimage/ gen_init_cpio/ binman-venv/
  README.md
kernel/  bootloader/  libc/  coreutils/     ← custom provider SOURCE, STAYS at root
projects/  tools/  docs/  rtos/  sim/  cad/  dbg/
```

`forge/core/toolchain.sh` doesn't vanish: `make toolchain` stays as the convenience
that provisions the base set (`host_provision make toolchain-glibc toolchain-musl
gen_init_cpio`), by calling `host_provision` instead of holding the bodies.

---

## 6. The binman-venv relocation (the question that started this)

The ~40-line venv block inline in `providers/bootloader/uboot/build.sh` (post-reorg:
`forge/providers/bootloader/uboot/build.sh`) moves out in §8 phase 4:
- → `forge/hostpackages/binman-venv/package.mk` (`PKG_TYPE=host-pyvenv`, module list),
- venv body → `forge/core/styles/host/host-pyvenv.sh`,
- `forge/providers/bootloader/uboot/provider.mk` gains `PKG_HOST_DEPENDS=binman-venv`,
- `build.sh` deletes the block; the layer driver (or interim, top of build.sh) calls
  `host_provision binman-venv`.

Result: the U-Boot build script is only about building U-Boot; the venv is provisioned
iff U-Boot is in the build; a full-custom build never touches Python (§4.6).

---

## 7. Special components — what the resolver does NOT walk (context, do not "fix")

Not everything is a graph node. Some components are **selected (an axis)** or sit
**below the graph** as foundations. The resolver must not resolve these.

- **Tier 0 — below the graph.** The cross toolchain (compiler+binutils) and the
  **libc** it targets. Circular with the compiler; chosen once globally. Forge:
  `LIBC` axis + `cc-profile.sh`; the toolchain is a host-tarball-bin package but a
  *foundation*, not resolved via `PKG_DEPENDS`. Buildroot `TCLIBC`; Yocto `TCLIBC`.
  **`PKG_DEPENDS=libc` today is a SENTINEL, not an edge** — when §3.4's target build
  caller is wired, `libc` DROPS OUT of `PKG_DEPENDS` (substrate is implicit in the CC
  profile), leaving `PKG_DEPENDS` meaning only "other target packages."
- **Tier 1 — single-slot roles (axes / virtual-providers).** kernel, bootloader
  (already axes), and — as packages land — **init/PID 1**, **/bin/sh**, **device
  manager (mdev/udev)**, **getty**, **syslog**. Buildroot `BR2_INIT_*` / Yocto
  `VIRTUAL-RUNTIME_*`. Picked via a selector, NOT added to a package set.
- **Tier 2 — in-graph but privileged.** Kernel UAPI headers (libc compiles against
  them; `build-substrate.sh` stages `gv3_syscalls.h`/`gv3_abi.h`); the host-tool class
  (`PKG_CLASS=host`: special class, ordinary resolution).
- **Tier 3 — the actual graph.** busybox, coreutils, zlib, future daemons/plugins —
  what the resolver walks.

Rule: resolver walks Tier 3 (+ host graph); Tiers 0–1 chosen before the walk; Tier 2
privileged inside it.

---

## 8. Phasing (behavior-preserving; verify §9 after each)

0. **Reorg (§2).** `git mv` engine → `forge/core/`, catalogs → `forge/{packages,
   providers}/`; apply the §2.4 reference edits (two-root addressing: `FORGE_ROOT` for
   catalogs, `REPO_ROOT` for custom source). Prove `make print-config` (both stacks) +
   golden 6/6 + dynamic.sh unchanged. NOTHING else in this phase.
1. **Build the resolver.** `forge/core/lib/resolve.sh` (`walk_deps`, §3.2) with a
   standalone unit check (synthetic edges → topo order; synthetic cycle → nonzero). No
   consumers yet.
2. **Scaffold host packages, no behavior change.** `forge/hostpackages/*` recipes +
   `forge/core/styles/host/*` (bodies lifted from `toolchain.sh` `setup_*`) +
   `forge/core/lib/host.sh`. Add `HOST_PREFIX` to `lib.sh` as aliases over
   existing dirs (don't move files yet). Don't rewire consumers.
3. **Cut over `make toolchain`.** Replace eager
   `setup_host_make`/`setup_cross_toolchain`/`setup_gen_init_cpio` with
   `host_provision make toolchain-glibc toolchain-musl gen_init_cpio`. Prove identical
   provisioned prefix.
4. **Cut over lazy/inline tools.** `image.sh` SD path → `host_provision genimage`;
   rootfs assembler inline gen_init_cpio → `host_provision gen_init_cpio`; U-Boot
   `build.sh` venv → delete + `host_provision binman-venv` (§6).
5. **Declare deps in recipes + drive from the layer. [DONE 2026-08-05]** `PKG_HOST_DEPENDS`
   is in the provider recipes; the layer driver satisfies it before the build step by RUNNING
   `steps/host.sh` (its executed-path guard — one file, library-when-sourced /
   command-when-run) (the leaf build steps stopped self-provisioning — see §4.4's DONE note).
   Host laziness is now fully graph-derived + engine-triggered.
6. **Move pins into recipes; slim `lib.sh`; retire `setup_*`.** Relocate host-tool
   version/SHA pins from `lib.sh` into `forge/hostpackages/*/package.mk`; consolidate
   `build/host/`; delete the emptied `setup_*` from `toolchain.sh`.
7. **(LATER, when target packages gain real edges) Target-side resolver.** Replace
   `rootfs-assemble.sh`'s flat loop with the 3-phase `walk_deps` loop (§3.4); drop the
   `libc` sentinel from `PKG_DEPENDS`; add `PKG_DEPENDS`/`PKG_RDEPENDS` to the first
   packages with real inter-deps. `resolve.sh` already exists (phase 1) — wiring only.

Phase 0 is a pure move+rewire (identical behavior). 1–4 relocation + resolver
(identical behavior). 5–6 the faithful finish. 7 activates at the first real target
edge.

---

## 9. Non-goals / scope discipline

- **Not a Buildroot clone.** No Kconfig, no Make-metaprogramming dep engine.
  `resolve.sh` is a ~40-line shell closure-walker.
- **Build the SCHEMA, RESOLVER, and final PATHS fully now; grow node-action depth
  later.** The reorg layout, the three relations, `walk_deps`, and the
  SYSROOT/STAGE-by-relation rule are the durable interfaces. Version-constrained deps
  (`foo>=1.2`), optional/`select` deps, parallel scheduling are node-action extensions
  that DON'T change the schema — defer until needed.
- **Host packages never ship to target** (`PKG_CLASS=host` → `HOST_PREFIX`, never
  `STAGE`).
- **Custom provider SOURCE stays at repo root** (§2.2) — it's software, not
  build-system; recipes reference it by repo-root path.
- **Cross toolchain stays EXTERNAL** (`host-tarball-bin`). "Internal from-source
  toolchain" is a separate deferred axis the model merely gives a home.
- **Don't over-fit the resolver.** Only host→host edge today is genimage→libconfuse.

---

## 10. Verification (must be behavior-identical to today)

Run after each phase; all hold at the end:

- **Phase 0:** `make -C projects/gameboy-v3 print-config` for BOTH the full-custom and
  the `KERNEL=mainline BOOTLOADER=uboot LIBC=musl PACKAGES=busybox` stacks resolves to
  the same values as before; golden 6/6; dynamic.sh; a `make image` for both stacks
  composes. (Catches every broken path from the move.)
- `resolve.sh` unit check: synthetic edge set → correct topo order; synthetic cycle →
  nonzero, named.
- `make toolchain` provisions the same tools — compare `make`/`arm-*-gcc --version`;
  cross toolchains extract byte-identical trees (same pinned tarball + SHA).
- **Laziness (headline):** `make image KERNEL=custom BOOTLOADER=custom LIBC=custom`
  provisions NO `genimage` and NO `binman-venv` — assert `build/host/.stamps/` has
  neither and no fetch for them occurs.
- `MEDIA=sd` provisions `genimage` (+`libconfuse`); the SD `.img` is unchanged.
- `BOOTLOADER=uboot` provisions `binman-venv`; the U-Boot image AND `.config` are
  byte-identical to a pre-change build.
- **Golden 6/6** (`kernel/test/golden.sh`) and **`libc/test/dynamic.sh`** pass — both
  use `gen_init_cpio`, now a host package, and both had their forge-path strings
  updated in Phase 0.
- `make clean` / `make host-clean` removes `build/host/`; a fresh `make image`
  re-provisions exactly the declared closure.
- Post-phase-6 grep returns only retired definitions or nothing:
  `grep -rn 'setup_host_make\|setup_cross_toolchain\|setup_genimage\|setup_gen_init_cpio' forge/`
- Post-phase-0 grep finds no stale two-dir forge paths:
  `grep -rn 'forge/backends\|forge/rules.mk\|forge/providers.mk' forge/ projects/ kernel/ libc/`
  should return only `forge/core/...` paths (and doc prose).
```
