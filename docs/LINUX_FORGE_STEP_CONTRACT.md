# Forge Step Contract: unified fetch (#A) + stamp-based layer steps (#B)

**Status:** #A IMPLEMENTED (2026-08-06, this window); #B pending. Two changes, sequenced:
- **#A — one fetch step** keyed on `PKG_FETCH` (local | git | tarball | prebuilt), so no
  layer re-implements fetching. High-value, low-risk. **DONE** — `lib/fetch.sh` (`forge_fetch`
  + the shared download primitive `forge_fetch_file`); all four fetch kinds + all three host
  styles (host-tarball-bin/host-cc/host-autotools) migrated; `fetch_verify` and
  `clone_or_reuse_pinned` now have exactly ONE live caller each (both inside `fetch.sh`). The
  host-style fold was NOT deferred (done in the same pass). Verified: local+tarball+git through
  the graph, a real genimage download via host-autotools, golden 6/6. See §1.7.
- **#B — a fixed step contract** for the main layers (the Buildroot shape:
  `.stamp_{fetched,configured,built,installed}` targets where each layer supplies `*_CMDS`),
  replacing the three ad-hoc dispatch shapes and buying incremental rebuilds. Bigger lift;
  depends on #A having reified `do_fetch` first. PENDING.

This doc is a companion to `docs/LINUX_FORGE_PLUMBING_CLEANUP.md` (#1–#9, all landed) and
uses the same conventions: exact current-state citations, a phased plan, and a verification
section. The guiding principle is unchanged — **mirror Buildroot/Yocto faithfully; the engine
holds mechanism, the recipe holds data; no layer re-implements what the engine can own once.**

---

## 0. Current state (verified against the tree)

### 0.1 The build graph today

`forge/core/rules.mk` expresses the layers as a real Make dependency graph:

```
toolchain
  └─ kernel  bootloader  libc  ──┐   (kernel bootloader libc rootfs: toolchain forge.conf)
        rootfs: libc              │   (rootfs also depends on libc — #9)
  image: kernel bootloader rootfs ┘
```

Each layer target's recipe is a single recursive `$(MAKE) -f <layer>.mk … build` (or
`assemble`). The per-layer makefiles (`kernel.mk`/`bootloader.mk`/`libc.mk`/`rootfs.mk`/
`image.mk`) `include $(FORGE_BUILD)/forge.conf` for the resolved config, then run the layer.

### 0.2 The two inconsistencies this doc targets

**(A) "fetch" is implemented in FOUR unrelated places** — there is no `do_fetch`:

| Fetch kind | Where it lives today | Mechanism |
|---|---|---|
| **git** (mainline kernel, u-boot) | inside each kconfig provider's OWN `build.sh` | `clone_or_reuse_pinned` (lib/core.sh), reading `PKG_VERSION`/`PKG_GIT_URL[_MIRROR]` from the recipe |
| **tarball** (busybox package) | `forge/layers/rootfs/build.sh` (`build_package`) | `fetch_verify` (lib/core.sh), reading `PKG_SITE`/`PKG_SOURCE`/`PKG_SHA256` |
| **tarball** (host tools) | `forge/core/styles/host/*.sh` | `fetch_verify`, same keys |
| **local** (custom kernel/bootloader/libc/coreutils) | `forge/core/providers.mk` (`$(if $(filter local,…))`) | no fetch — the path is just resolved to `$(REPO_ROOT)/<PKG_SOURCE>` |
| **prebuilt** (musl) | `forge/core/providers.mk` (`$(if $(filter prebuilt,…))`) | no fetch — a marker dir `$(BUILD)/musl` |

So the *same concept* (get a component's source on disk) has four call sites and two helpers,
and whether it's a "step" at all depends on the fetch method. `PKG_FETCH` already exists as
the discriminator (`local`/`git`/`tarball`*/`prebuilt`) — it is read in `providers.mk` for
path resolution, but no single step ACTS on it. (*busybox declares no `PKG_FETCH`; it's
implicitly tarball via `PKG_SITE`. #A makes this explicit.)

**(B) There are THREE layer-execution shapes** — no uniform step contract:

| Layers | Driver | Build dispatch |
|---|---|---|
| kernel, bootloader | `kernel.mk`/`bootloader.mk` → **`layer.mk`** | style-dispatch on `PKG_TYPE`: `make-c` \| `kconfig` |
| libc | **`libc.mk`** (thin) | one procedure: `styles/substrate.sh` (adapts internally on "ships a build.sh?") |
| rootfs, image | `rootfs.mk`/`image.mk` (thin) | bespoke script: `layers/{rootfs,image}/build.sh` |

Each script FUSES fetch + configure + build + install internally. None expose those phases as
named, orderable, addressable targets. Consequence: **every layer target is `.PHONY`**
(`rules.mk` line ~57), so `make image` unconditionally re-runs every layer — no incremental
rebuild. `make kernel` reclones/reconfigures/recompiles from the top even if nothing changed.

### 0.3 The precedent we already have — the host-package subsystem

Critically, **#B is not inventing a step engine — the host side already has one.**
`forge/core/lib/host.sh` + `resolve.sh` implement, for host tools, exactly the model #B wants
for target layers:

- **Stamp + witness idempotency** (`host.sh` `_host_build_one`, lines ~70–109): skip the build
  iff BOTH a current `<name>-<version>` stamp exists AND the declared artifact WITNESS is
  present. Stamp-without-witness (a wiped `build/`) ⇒ re-provision. This is precisely
  Buildroot's stamp-file model, already working and battle-tested this codebase.
- **A command/contract split**: each host style (`styles/host/<PKG_TYPE>.sh`) reads typed
  recipe keys (`PKG_HOST_CONFIGURE_FLAGS`, `PKG_HOST_BIN`, …) — the analogue of Buildroot's
  `FOO_CONFIGURE_CMDS`.
- **A neutral resolver** (`resolve.sh` `walk_deps`): one class-agnostic topological walker,
  already documented as "the ONE mechanism Buildroot/Yocto use for host+target."

So the honest framing: forge has been converging on Buildroot's model but stopped at the host
boundary. #B carries the *same* stamp/witness/CMDS pattern across to the target layers. That
lowers #B's risk — it's an extension of proven code, not a greenfield subsystem.

### 0.4 Files to read before starting

- `forge/core/rules.mk` (the graph + `.PHONY` + `BUILD`/`FORGE_BUILD` collision note)
- `forge/core/providers.mk` (`PKG_FETCH`→path resolution; the `_recipe_get` reader)
- `forge/core/lib/core.sh` (`fetch_verify`, `clone_or_reuse_pinned`, `clone_or_reuse` skeleton,
  `recipe_get`; the build-tree layout vars incl `KERNEL_SRC_DIR`/`UBOOT_SRC_DIR`/`SUBSTRATE_DIR`)
- `forge/core/lib/host.sh` + `resolve.sh` (the stamp/witness precedent for #B)
- `forge/core/layer.mk` (the make-c/kconfig dispatch #B subsumes)
- `forge/providers/kernel/mainline/build.sh` + `forge/providers/bootloader/uboot/build.sh`
  (where git-fetch is fused into build today — #A extracts it)
- `forge/layers/rootfs/build.sh` (`build_package`'s inline `fetch_verify` — #A extracts it)

### 0.5 Hard constraints (carried from the plumbing-cleanup doc)

- **GNU Make 3.82** on the product-parse host: no `$(file >)`, no `.ONESHELL` assumptions;
  order-only prereqs (`|`) work but the config write must stay a recipe step (§1 of the
  plumbing doc). Any new `.stamp` targets must parse + build under 3.82.
- **`FORGE_BUILD`, never `BUILD`** on recursive `$(MAKE)` lines — a command-line `BUILD=` leaks
  through `MAKEFLAGS` into provider Makefiles that define their own `BUILD` (the toolchain-wipe
  regression). New forwarded vars must be namespaced.
- **`.PHONY` on `forge.conf`** is deliberate (config derives from make VARIABLES, not files).
  #B's stamps are the OPPOSITE case (they DO derive from files) — do not make them phony.
- **Two-root addressing**: catalog paths (`packages/`/`providers/`/`hostpackages/`) are
  `FORGE_ROOT`-relative; custom source (`PKG_SOURCE`) is `REPO_ROOT`-relative.
- **Keep changes byte-identical in output** where possible; a differing artifact = a regression
  to explain.

---

## 1. Change #A — one `do_fetch` step keyed on `PKG_FETCH`

### 1.1 Goal

Exactly one place resolves "get this component's source on disk," dispatching on the recipe's
`PKG_FETCH` fact. Every layer/package/host-tool that needs source calls it; none re-implements
git-clone or tarball-fetch. This is the `do_fetch` task, distilled.

### 1.2 The contract

A new helper in `forge/core/lib/fetch.sh` (a lib/ toolkit sibling of `core.sh`/`host.sh`):

```sh
# forge_fetch <recipe-file>  ->  prints the resolved SOURCE DIR on stdout (absolute).
#   Dispatches on PKG_FETCH; idempotent (a present, verified source is a no-op).
#   local    -> $REPO_ROOT/$PKG_SOURCE      (assert it exists; never fetch)
#   prebuilt -> the marker dir              (no-op; source is baked into a host pkg)
#   git      -> clone_or_reuse_pinned into the layer's checkout dir (PKG_VERSION tag,
#               PKG_GIT_URL[_MIRROR]); assert tag matches on reuse
#   tarball  -> fetch_verify PKG_SITE/PKG_SOURCE @ PKG_SHA256 into DOWNLOAD_DIR, then
#               extract (--strip-components=1) into the layer's src dir
```

Key points that make this a drop-in, not a rewrite:

- It **reuses the two existing helpers unchanged** — `clone_or_reuse_pinned` and `fetch_verify`
  already live in `core.sh` and are correct. #A does not touch fetch *mechanism*; it removes
  the DUPLICATION of who calls them and on what keys.
- The **destination dir** per fetch kind is already defined once in `core.sh`
  (`KERNEL_SRC_DIR`/`UBOOT_SRC_DIR` for git; `SUBSTRATE_DIR` is downstream of libc's source;
  `DOWNLOAD_DIR` for tarballs). `forge_fetch` keys the git dest off `PKG_ROLE`
  (kernel→`KERNEL_SRC_DIR`, bootloader→`UBOOT_SRC_DIR`) exactly as `_recipe_src_dir` already
  does in `core.sh` — reuse that switch, don't duplicate it.
- **Recipe keys already exist**: `PKG_FETCH`, `PKG_VERSION`, `PKG_GIT_URL[_MIRROR]`, `PKG_SITE`,
  `PKG_SOURCE`, `PKG_SHA256` are all present in today's recipes (verified: kernel/mainline,
  bootloader/uboot, packages/busybox). #A reads them from ONE place instead of three. The only
  recipe change is adding an explicit `PKG_FETCH=tarball` to busybox (today implicit).

### 1.3 Call-site migration (what stops re-implementing fetch)

- **`providers/kernel/mainline/build.sh`** and **`providers/bootloader/uboot/build.sh`**:
  DELETE the `clone_or_reuse_pinned "${KERNEL_SRC_DIR}" "${KERNEL_TAG}" …` block (and the
  `recipe_get … PKG_GIT_URL` reads that feed it). The script now assumes its source is on disk
  (fetched by the `do_fetch` step that ran before it) and starts at `cd $SRC` + configure. This
  is the SAME "trust your declared inputs" move as the `require_substrate` removal — the fetch
  step is a declared prerequisite.
- **`layers/rootfs/build.sh`** (`build_package`): DELETE the inline
  `fetch_verify … ; tar -xf …` block; call `forge_fetch` on the package recipe instead (or, in
  the #B end-state, the package's own fetch step ran first). Packages are a slightly different
  case — they're fetched inside the rootfs loop, not as a top-level layer — see §1.5.
- **`styles/host/*.sh`**: OPTIONAL. The host styles already centralize their fetch via
  `fetch_verify` behind `host_provision`; folding them onto `forge_fetch` is a nice-to-have,
  not required for #A's value. Recommend deferring to keep #A small.
- **`providers.mk`**: the `local`/`prebuilt`/`git` PATH resolution stays (Make still needs the
  path to name artifacts) — but it now names the SAME dirs `forge_fetch` writes, so keep the
  `_recipe_src_dir`/`KERNEL_SRC` logic as the single source of the path formula and have
  `forge_fetch` import it, not fork it.

### 1.4 Where the step runs in the graph

Two options; recommend (a) for #A-alone, (b) as the natural bridge to #B:

- **(a) Inline, first thing in each layer's build.** Each layer driver runs `forge_fetch` (like
  it runs `host.sh` for host-deps) before the build step. Minimal graph change; #A lands alone.
- **(b) A real `<layer>-fetch` prerequisite.** Reify fetch as its own target that the build
  target depends on. This is the #B shape and is where #A and #B meet — do this when #B lands.

### 1.5 The one subtlety — packages vs layers

Layers (kernel/bootloader/libc) are single-component and top-level; packages are a *set*
iterated inside the rootfs assembler. `forge_fetch` works for both (it takes a recipe file), but
in #A-alone the package fetch stays INSIDE the rootfs loop (just calling `forge_fetch` instead
of inline `fetch_verify`). Only #B (if it models each package as its own stamped sub-step) would
lift package fetch out of the loop. Keep that boundary explicit so #A doesn't grow.

### 1.6 Verify #A

- `make kernel` (mainline) still clones once + reuses on re-run (tag-match assertion intact).
- `make rootfs LIBC=musl PACKAGES=busybox` still fetches+verifies the busybox tarball (SHA
  check intact); a corrupted download still fails the SHA.
- `make` full custom stack (all `local`) does no network fetch — `forge_fetch` no-ops to the
  repo dirs.
- `grep -rn "clone_or_reuse_pinned\|fetch_verify" forge/providers forge/layers` returns
  NOTHING (all migrated to `forge_fetch`); the only definitions/callers left are in
  `lib/fetch.sh` (+ optionally the host styles if deferred).
- Golden 6/6; `make image` custom composes; byte-compare a rebuilt zImage/initramfs (identical).

### 1.7 As-built (what actually landed)

- **`forge/core/lib/fetch.sh`** — two functions:
  - `forge_fetch <recipe> [scratch]` → echoes the resolved SOURCE DIR, dispatching on
    `PKG_FETCH`: `local` (assert `$REPO_ROOT/$PKG_SOURCE`), `prebuilt` (empty — musl), `git`
    (`clone_or_reuse_pinned` into the role's checkout dir), `tarball` (download+extract into
    the caller's scratch dir). git dest keyed off `PKG_ROLE`, reusing `_recipe_src_dir`'s switch.
  - `forge_fetch_file <name> <site> <sha> [mirror] [query]` → the ONE download primitive:
    download+verify one artifact into `DOWNLOAD_DIR` with primary→mirror fallback and the
    `?query` suffix (append to URL, not saved name); echoes the cached path; does NOT extract.
    This is where the primary/mirror/query logic lives — it used to be reimplemented inline in
    `host-cc`. `forge_fetch`'s tarball arm calls it too, so there is one download path total.
- **Migrated call sites** (all now call `forge_fetch`/`forge_fetch_file`, none reimplement fetch):
  - `providers/kernel/mainline/build.sh`, `providers/bootloader/uboot/build.sh` — deleted the
    inline `clone_or_reuse_pinned` + `PKG_GIT_URL[_MIRROR]` reads; call `forge_fetch` (git arm).
  - `layers/rootfs/build.sh` (`build_package`) — deleted the inline `PKG_SITE`-vs-local branch
    + `fetch_verify`/`tar`; calls `forge_fetch <recipe> <scratch>`.
  - `styles/host/host-cc.sh` — deleted the inline primary/mirror/query fetch; calls
    `forge_fetch_file` (the biggest de-dup — that logic now lives once in the primitive).
  - `styles/host/host-autotools.sh`, `styles/host/host-tarball-bin.sh` — replaced their direct
    `fetch_verify` with `forge_fetch_file` (both gain mirror support for free). **NOT deferred.**
  - `host.sh` now sources `fetch.sh` so the styles see `forge_fetch_file`.
- **Recipes**: `packages/busybox` gained explicit `PKG_FETCH=tarball` (was implied by
  `PKG_SITE`); `packages/coreutils` gained explicit `PKG_FETCH=local`.
- **Invariant achieved**: `grep` shows `fetch_verify` and `clone_or_reuse_pinned` each have
  exactly ONE live caller — both inside `fetch.sh`. Every other reference is a comment.
- **Verified**: local (custom coreutils, no network) + tarball (busybox `download → SHA256 OK`)
  + git (kernel `reusing … at v6.12.95`) through the graph; a REAL `genimage-18.tar.xz`
  download via host-autotools (not just cache-verify) + `built -> genimage`; gen_init_cpio
  re-provisioned via host-cc's mirror+query path; golden 6/6.

---

## 2. Change #B — a fixed step contract for the main layers

**Status:** IMPLEMENTED (2026-08-06, this window) — as the PER-LAYER-STAMP form (one stamp per
layer), NOT the per-step (four-stamp) form the design below sketched. The per-layer granularity
was chosen deliberately (see §2.8): it delivers the incremental-rebuild win with low churn and
low stale-artifact risk, and lets each layer's own tool (kbuild / our Makefiles / the package
loop) keep doing intra-layer incrementality. The `.stamp_{fetched,configured,built,installed}`
per-step decomposition (§2.2) was NOT done — it's redundant with those tools and higher-risk.

### 2.1 Goal

Every main layer (kernel, bootloader, libc, rootfs) is a real STAMP FILE whose prerequisites are
that layer's declarative inputs; `image` depends on the layer stamps and composes. Make skips a
layer whose inputs are unchanged — replacing the blanket `.PHONY` that always rebuilt everything.

### 2.2 The step set (Buildroot-distilled, minimal)

```
.stamp_fetched     -> forge_fetch (from #A)
.stamp_configured  -> ${LAYER}_CONFIGURE_CMDS   (kconfig defconfig+fragments; make-c: no-op; …)
.stamp_built       -> ${LAYER}_BUILD_CMDS       (make -j / compile loop / substrate build)
.stamp_installed   -> ${LAYER}_INSTALL_CMDS     (cp artifacts to OUTPUT_DIR / stage into image)
```

- **Four steps, not eight.** Buildroot has ~8 (download/extract/patch/configure/build/
  staging-install/target-install/images); Yocto more. At forge's scale, collapse extract+patch
  into fetch and staging+target into install. Do not port the full chain — that's the
  over-engineering trap. Add a step only when a real second consumer appears (e.g. a `patch`
  step iff a layer ever needs to patch upstream source).
- **`*_CMDS` are the layer's data**; the engine owns the stamp orchestration. This is exactly
  the host-style contract (`PKG_HOST_CONFIGURE_FLAGS` etc.) generalized. Reuse the pattern.

### 2.3 Mechanism — reuse the host subsystem's stamp+witness, don't reinvent

The stamp discipline is ALREADY WRITTEN in `host.sh` (`_host_build_one`): stamp file +
artifact witness, skip iff both current. #B factors that into a shared helper both host and
target use (the `resolve.sh` philosophy: one mechanism, class-agnostic). Two viable forms:

- **Make-native stamps (recommended).** Each step is a real file target
  `$(BUILD)/<layer>/.stamp_<step>` with the previous step as its prerequisite, plus the layer's
  actual INPUTS (recipe file, config fragments, source tree marker) as prerequisites so Make's
  own timestamp logic decides freshness. This is the most Make-idiomatic and gives incremental
  rebuild "for free." Under Make 3.82 this parses fine (plain file targets + prereqs).
  - Caveat: capturing "the config SELECTION changed" (a make-variable, not a file) needs the
    same trick `forge.conf` uses — depend on `forge.conf` (already rewritten every run). So a
    changed `LIBC=`/`LINKAGE=` re-triggers the dependent stamps via the `forge.conf` prereq.
- **Shell stamp+witness (host.sh parity).** If Make-native prereq tracking proves fiddly for a
  layer whose "input" is a whole git tree, fall back to the host model: a shell step checks
  `stamp newer than inputs && witness present`, else runs `*_CMDS`. Less elegant but proven.

Recommend Make-native for kernel/bootloader/libc/image (clear file inputs) and allow the
shell-witness form for rootfs (its input is a dir tree + a package set).

### 2.4 Collapsing the three shapes into one

- **`layer.mk`'s make-c/kconfig dispatch** becomes the DEFAULT `*_CMDS` for those `PKG_TYPE`s —
  i.e. a `make-c` layer's `_BUILD_CMDS` is `make -C $SRC $(MAKE_GOALS)`; a `kconfig` layer's
  `_CONFIGURE_CMDS` is `defconfig + apply_config_fragment`, `_BUILD_CMDS` is `make -j <image>`.
  These are the typed defaults (Buildroot's `autotools-package` analogue). A layer overrides a
  step only where it genuinely differs.
- **libc's `substrate.sh`** becomes libc's `_BUILD_CMDS` (property-dispatch stays inside it).
- **rootfs/image bespoke scripts** decompose: rootfs's package loop is `_BUILD_CMDS`, its pack
  is `_INSTALL_CMDS`; image's resolve+check is gone (done in #A/§require_substrate work), its
  compose is `_INSTALL_CMDS` (image has no fetch/configure/build — declare those steps no-op,
  which is legitimate: image is a pure `do_rootfs`-style composer).
- Result: ONE driver shape (`step.mk`, the generalized `layer.mk`), five layers routing through
  it, each supplying only the `*_CMDS` that differ from its `PKG_TYPE` defaults.

### 2.5 What #B buys

- **Incremental rebuilds**: `make image` after touching only a coreutils `.c` rebuilds the
  rootfs `_built`/`_installed` steps and re-composes — but does NOT reclone the kernel or
  rebuild u-boot. Today (all `.PHONY`) it rebuilds everything. This is the headline win.
- **Addressable steps**: `make kernel-configure` / `kernel-rebuild` (the `busybox-rebuild`
  analogue), useful for iterating on a defconfig fragment without a full clone+build.
- **One interface**: reading any layer, the reader sees the same four steps + `*_CMDS`, the
  consistency you noted Yocto/Buildroot have and forge lacks.

### 2.6 Risks + mitigations (be honest about the lift)

- **Correctly capturing inputs.** The hardest part is enumerating each step's real
  prerequisites so incrementality is CORRECT (not stale). A missed input ⇒ a stale artifact ⇒
  worse than `.PHONY`. Mitigation: land it layer-by-layer, and for each, verify a
  touch-an-input rebuild AND a touch-nothing no-op, byte-comparing artifacts. Start with libc
  (smallest, clearest inputs), end with rootfs (murkiest).
- **`.PHONY` removal is behavior-changing.** A cold `make image` must still produce identical
  output; only the WARM (unchanged) path changes (skip vs rebuild). Keep a `make clean` +
  `FORCE=1` escape (host.sh already honors `FORCE`) so a full rebuild is always one flag away.
- **Make 3.82.** Verify stamp targets + prereqs parse and rebuild correctly under 3.82 before
  spreading the pattern; the `forge.conf`-as-prereq trick for selection changes must be tested.
- **Don't over-decompose.** Resist adding patch/staging/images steps "for symmetry." Four steps
  cover every current layer; more is speculative.

### 2.7 Verify #B

- Per layer, in order libc → kernel → bootloader → image → rootfs:
  - cold build (no `build/`) produces byte-identical artifacts to pre-#B.
  - warm re-run with NO input change: the layer's steps are SKIPPED (log says "up to date");
    `make image` re-runs nothing below a green layer.
  - touch ONE real input (a `.c`, a config fragment, bump `PKG_VERSION`): exactly the dependent
    steps rebuild, upstream-unaffected layers stay skipped; artifact updates correctly.
  - `FORCE=1 make <layer>` rebuilds unconditionally.
- Golden 6/6 after each layer's migration.
- `grep` shows one driver shape: no layer has a bespoke fetch/build fusion; all route through
  `step.mk` with `*_CMDS`.
- The gold standard: clean rebuild + flash-and-boot BOTH stacks on the T113 rig (custom →
  `gv3$`, all-OSS → `~ #`), same as every prior spine change. (NOT yet done for #B — the rig
  step is deferred to the next hardware session; all QEMU/host verification below passed.)

### 2.8 As-built (what actually landed)

All in `forge/core/rules.mk`. Per-layer stamps, not per-step.

- **forge.conf → compare-and-write.** `$(BUILD)/forge.conf` is no longer `.PHONY`. Its recipe
  runs every invocation (via a `.PHONY: force` prereq) but renders to `$@.tmp` and only
  `mv`s over `$@` when the content DIFFERS (`cmp -s`), so the file's MTIME bumps ONLY when the
  selection changed. This is the linchpin: the layer stamps depend on this real file, so an
  unchanged selection leaves them fresh, while a changed selection (`LIBC=`/`PACKAGES=`/
  `LINKAGE=` / a version-pin bump — everything is in `FORGE_CONF_BODY`) re-triggers them.
- **Per-layer stamp targets** `$(BUILD)/.stamps/<layer>`: prereqs = `forge.conf` + the layer's
  recipe + its LOCAL source tree (`$(shell find <SRC> -type f)` for a local-fetch provider;
  a git/prebuilt provider has no repo tree, and its pin is already in forge.conf) + (rootfs)
  the local package sources + package recipes. Recipe = `$(call _run_layer,<layer>,$@)` (the
  existing recursive `$(MAKE) -f <layer>.mk build`, then `touch $@`). The layer NAME
  (`kernel`/`bootloader`/`libc`/`rootfs`) is a thin `.PHONY` alias onto its stamp, so
  `make kernel` still works.
- **Link-scoping:** `libc` and `rootfs` stamps are suffixed by `LINKAGE`
  (`.stamps/rootfs-static` vs `.stamps/rootfs-dynamic`), so `make rootfs LINKAGE=dynamic` is
  never skipped because the static stamp is fresh. Verified: both stamp pairs coexist.
- **Order-only toolchain:** each stamp has `| toolchain` (order-only) — toolchain runs first
  (it's idempotent + itself stamp-guarded in host.sh, so near-free) but does NOT force the
  stamp stale. A NORMAL phony prereq WOULD force always-rebuild (the trap); order-only avoids it.
- **`image`** depends on the three layer stamps + forge.conf and composes; it has no stamp of
  its own (its output is the bundle; re-composing from fresh artifacts is cheap + always wanted).
- **`FORCE=1`** → `$(shell rm -rf $(STAMP))` at parse time wipes all stamps (unconditional
  rebuild); `clean` also removes `$(STAMP)`.
- **Four Make-3.82 mechanisms verified in isolation before wiring** (all confirmed on the
  product's GNU Make 3.82): (1) compare-and-write → dependent rebuilds only on content change;
  (2) `$(shell find)` source prereqs → rebuild on edit-existing OR add-file; (3) multi-line
  canned recipe via `$(call)`; (4) order-only phony prereq → ordering without forced-stale.
- **Verified end-to-end:** cold `make image` builds all 4 layers + composes; a no-op re-run
  rebuilds ZERO layers (composes only); touching a coreutils `.c` rebuilds ONLY rootfs
  (kernel/bootloader/libc = 0); a selection change (custom→musl/busybox) re-triggers via
  forge.conf; `LINKAGE=dynamic` rebuilds despite fresh static; touching a kernel source
  rebuilds kernel; `FORCE=1` rebuilds a fresh layer; golden 6/6.

---

## 3. Sequencing

1. **#A first, alone.** It's self-contained, removes the four-fetch-sites smell, and REIFIES
   `do_fetch` — which #B's `.stamp_fetched` then wraps. Landing #A first means #B builds on a
   real fetch step instead of inventing one. Verify §1.6, keep unstaged until told.
2. **#B second, layer-by-layer.** Do NOT convert all five layers at once. Order libc → kernel →
   bootloader → image → rootfs (simplest inputs first, murkiest last). Verify §2.7 after EACH
   layer; a regression is contained to one layer's step wiring.

Rationale: risk-ascending. #A is a pure de-duplication (behavior-identical, easy to prove). #B
changes rebuild BEHAVIOR (incremental vs always), so it must be proven per layer with
byte-compares. If time-boxed, **#A alone is a complete, valuable win** (kills the fetch
inconsistency); #B can follow when incremental-rebuild pain justifies it.

---

## 4. Explicitly NOT in scope

- **A full Yocto-style task engine** (addtask/signatures/sstate-cache, hash-equivalence). Forge
  has ~5 layers; sstate's payoff is a thousands-of-recipes phenomenon. #B's four-stamp chain is
  the right altitude; a content-hash cache is over-engineering here.
- **Patch/staging/target-images as separate steps.** Collapsed into fetch/install until a real
  second consumer exists. Add on demand, not speculatively.
- **Folding the host styles onto `forge_fetch`** in #A. They already centralize fetch behind
  `host_provision`; migrating them is a nice-to-have deferred to keep #A minimal.
- **Unifying the two recipe readers** (`_recipe_get` Make vs `recipe_get` bash) — intentionally
  distinct (the bash one `eval`s `${VAR}`; the Make one must not), as documented in the
  plumbing-cleanup doc §4.

---

## 5. Note for the implementer

The engine's structure + the declarative interface (recipes, host-deps) are already uniform and
silicon-proven. This doc closes the two remaining EXECUTION-interface gaps: fetch is
implemented four times (#A), and the main layers speak three different step dialects with no
incrementality (#B). Both are "finish the Buildroot migration forge already started" — not new
paradigms. The host-package subsystem (`host.sh`/`resolve.sh`) is the working proof that the
stamp/witness/CMDS model fits this codebase; #B is that model, carried across the host→target
boundary. Prefer the smaller change when in doubt: #A alone already removes the inconsistency
you can SEE; #B removes the one you FEEL on every `make image`.
