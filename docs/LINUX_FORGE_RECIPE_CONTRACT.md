# Forge Recipe Contract: a generic engine + `inherit`-based classes (kill the type ladders)

**Status:** IMPLEMENTED (2026-08-06). All phases done + golden 6/6 green (rig boot pending a
hardware session). End state: `forge/core/` has NO per-layer makefiles — only `providers.mk`
(resolve) + `rules.mk` (graph). Every buildable (kernel, bootloader, libc, each package,
rootfs, image) runs through ONE generic runner (`forge/core/run-recipe.sh`) via `inherit
<class>` + `do_*`; zero engine-side type dispatch. Deleted: `layer.mk`, `kernel.mk`,
`bootloader.mk`, `libc.mk`, `rootfs.mk`, `image.mk`, `styles/{make-c,substrate,compile-c,
kconfig}.sh` (→ `classes/`). Two deviations from the plan below, both agreed with the user:
(1) **packages are real graph nodes** (`pkg-<name>`, generated per PACKAGES; each installs
into a shared STAGE that `stage-init` preps; rootfs `PKG_DEPENDS=${PACKAGES}` packs only) —
the fuller "packages as graph nodes" reading, not the minimal one; (2) the **incremental
stamps were RIPPED** (targets are plain `.PHONY`, always rebuild) — the user valued a clean
model over fast rebuilds, so the #B stamp machinery in rules.mk was removed.

**The one-line goal:** the engine stops enumerating build *kinds*. A recipe declares its
behaviour by `inherit`ing a class and optionally overriding named tasks (`do_build`/
`do_install`); the engine just sources the recipe and calls the tasks by name. This deletes
the two engine-side `if/else`/`case` ladders (`layer.mk`, `build_package`) and makes
kernel/bootloader/libc/rootfs/packages all uniform graph nodes built through one contract.

This is the companion refactor to `LINUX_FORGE_STEP_CONTRACT.md` (#A unified fetch + #B
per-layer stamps, both landed) and `LINUX_FORGE_PLUMBING_CLEANUP.md` (#1–#9, landed). It uses
the same discipline: exact current-state citations, phased plan, verify-each-step.

---

## 0. Motivation + honest scope

### 0.1 The smell (what the user flagged)

Two places in the engine branch on a closed set of build kinds:

- **`forge/core/layer.mk:50-77`** — `ifeq ($(STYLE),make-c) … else ifeq ($(STYLE),kconfig) …
  else <error>`. The kernel/bootloader driver picks a build path by string.
- **`forge/layers/rootfs/build.sh:121-125`** — `case "${PKG_TYPE}" in compile-c) …; kconfig) …;
  *) die`. The rootfs assembler picks each package's build path by string.

Both ENUMERATE kinds inside the engine. Adding a build kind = editing the engine. That is the
antithesis of the Yocto/Buildroot model, where the engine is generic and "kind" is a class the
*recipe* binds to.

Two further consequences of the same root cause:
- **kernel/bootloader/libc are special-cased layers** — each has its own driver makefile
  (`kernel.mk`/`bootloader.mk`/`libc.mk`/`rootfs.mk`) feeding `layer.mk`. Five near-identical
  drivers where there should be one generic runner.
- **`build_package` makes the rootfs assembler an engine that builds OTHER packages** — it
  conflates "build a package" with "pack a cpio." The rootfs should only pack; packages should
  be graph nodes built by the same generic runner as everything else.

### 0.2 We already built the right pattern once

`forge/core/lib/host.sh:104-111` (the host-package subsystem) already does convention dispatch
with NO `if/else`:

```sh
local style=".../styles/host/${PKG_TYPE}.sh"
[ -f "${style}" ] || die "unknown PKG_TYPE '${PKG_TYPE}'"
source "${style}"; host_style_build
```

and `forge/core/lib/resolve.sh` (`walk_deps`) is a generic, class-agnostic topological closure
walker. So the target side just never got migrated to match the host side. **This refactor
finishes an architecture we already proved — it does not invent one.**

### 0.3 Honest scope — what this DOES and DOESN'T make us

Assessed per-dimension against Yocto (six-lens review, 2026-08-06). This refactor:

- **DOES** reach Yocto's *dispatch* property on two axes → **strong** alignment:
  - task/contract model: uniform-named tasks, engine never branches on kind;
  - recipe-as-data + no engine branching (for build type).
- **DOES** land us at **Buildroot's** model for typed-defaults-plus-per-recipe-override
  (see §1.4 — this is the `inherit`/`do_*` mechanism), which is a genuine improvement over
  today and consistent with a real build system (Buildroot recipes are Make+shell, not pure
  data; forge recipes become facts + inline shell tasks — same idea).
- **DOES NOT** make us Yocto on the granularity axes (deliberate NON-GOALS, §4):
  - no `addtask` intra-recipe task DAG — `do_*` is a fixed short sequence, not a schedulable
    per-task graph;
  - no per-task stamping/sstate/hash-equivalence — stamps stay per-layer (from #B), mtime not
    signature;
  - no per-recipe `${D}` / `do_populate_sysroot` — packages install into a shared staging tree;
  - no metadata layers (`bblayers.conf`/`BBFILE_PRIORITY`) — catalogs stay flat dirs.

Framing to hold onto: **"eliminate engine-side type branching; become Buildroot-with-shell-
tasks," NOT "become Yocto."** At forge's scale (~5 layers + a few packages) the granularity
Yocto adds is exactly the part that would not pay for itself. If a byte of build output differs
after this refactor, something regressed — it is a structural change, not a behaviour change.

### 0.4 Files to read before starting

- `forge/core/layer.mk` (the make-c|kconfig ladder — DELETED)
- `forge/layers/rootfs/build.sh` (`build_package` case — DELETED; the pack tail — KEPT as `do_rootfs`)
- `forge/core/lib/host.sh` (the convention-dispatch + stamp/witness PRECEDENT to generalize)
- `forge/core/lib/resolve.sh` (`walk_deps` — the generic graph walker, reusable for package deps)
- `forge/core/styles/{make-c,kconfig,compile-c,substrate}.sh` (become CLASSES: default `do_*`)
- `forge/core/rules.mk` (the #B stamp graph — KEPT; the per-layer driver forwarding — SIMPLIFIED)
- `forge/core/{kernel,bootloader,libc,rootfs,image}.mk` (the five drivers — collapse toward one)
- `forge/core/lib/fetch.sh` (`forge_fetch` from #A — becomes the default `do_fetch`)
- `forge/core/lib/ccprofile.sh` (the CC profile — stays a build-CONFIG input, NOT per-pkg branching)

### 0.5 Hard constraints (carried forward)

- **GNU Make 3.82** on the product-parse host (no `$(file >)`; recipe-write for forge.conf).
- **`FORGE_BUILD`, never `BUILD`** on recursive `$(MAKE)` lines (the toolchain-wipe collision).
- **Dual-read recipes** (verified §1.2): a recipe must stay awk-readable for bare `PKG_*` facts
  (Make builds the graph at parse time) AND bash-sourceable for the `do_*` functions. The test
  in §1.2 confirms both coexist in one file.
- **Keep #A (`forge_fetch`) and #B (per-layer stamps)** — this refactor builds ON them, does not
  undo them. Make stays the graph + incremental engine.

---

## 1. The contract

### 1.1 A recipe = facts + `inherit` + optional task overrides

```sh
# packages/coreutils/recipe.mk — uses the class defaults (the common case)
PKG_NAME=coreutils
PKG_FETCH=local
PKG_SOURCE=coreutils
PKG_DEPENDS=libc
PKG_INSTALL=/bin
inherit compile-c          # binds default do_build/do_install from classes/compile-c.sh
```

```sh
# providers/kernel/mainline/recipe.mk — overrides do_build/do_install (the generic-package case)
PKG_NAME=linux
PKG_FETCH=git
PKG_VERSION=v6.12.95
PKG_DEPENDS=toolchain-glibc
inherit kconfig            # defaults for whatever it doesn't override
do_build()   { kconfig_configure "${KERNEL_DEFCONFIG}" kfixup; make -j"$(nproc)" ...; }
do_install() { cp -f "${BUILT_ZIMAGE}" "${OUTPUT_DIR}/zImage"; ... }
```

Three facts make this work:
- **`inherit <class>`** is the ONLY name→file binding, and it lives IN THE RECIPE (visible), not
  in engine string-interpolation. It's Yocto's `inherit autotools` / Buildroot's
  `$(eval $(generic-package))`, in forge's native bash.
- **Override falls out of bash scoping for free**: `inherit` sources the class (defining default
  `do_*`) FIRST; a `do_build` appearing AFTER the `inherit` line redefines it. Last definition
  wins. No conditional, no hook registry. (Verified: overriding `do_build` while inheriting
  `do_install` works.)
- **`PKG_TYPE` goes away as an engine dispatch key.** Today `PKG_TYPE=kconfig` is a string the
  engine switches on (the smell). With `inherit kconfig` the recipe binds its own behaviour;
  the engine never sees a "type." (`PKG_TYPE` may survive as pure metadata/label if useful, but
  nothing branches on it.)

### 1.2 The dual-read invariant (verified)

The recipe is read two ways and BOTH must keep working:
- **Make/awk (parse time):** extracts bare `PKG_*` facts to build the dependency graph. It reads
  line-anchored `^PKG_KEY=` and ignores the `do_*` function bodies entirely.
- **bash `source` (build time):** gets the facts as vars AND the `do_*` as callable functions,
  AND executes `inherit`.

Verified on this tree: a file holding `PKG_NAME=busybox … PKG_TYPE=kconfig` + `do_build(){…}` +
`do_install(){…}` is (a) awk-readable for every fact and (b) bash-sourceable to callable
functions. The function bodies do not perturb the awk fact reader. So the invariant holds — the
same dual-read that already governs `board.conf`/recipes today, now with functions added.

### 1.3 The classes (formerly "styles")

`forge/core/styles/*.sh` → `forge/core/classes/*.sh` (rename to match the borrowed vocabulary).
Each class DEFINES default `do_*` functions — the typed-infra / `.bbclass` analogue:

- `classes/compile-c.sh` — `do_build`: compile each `.c` in `PKG_DIR` against the CC profile;
  `do_install`: install the ELFs into `${STAGE}${PKG_INSTALL}`. (today's compile-c.sh body,
  wrapped in `do_build`/`do_install`.)
- `classes/kconfig.sh` — `do_build`: `make <defconfig>` → fixup → `make` → `make install` into
  `${STAGE}`. (today's kconfig.sh body.) Providers that need bespoke logic (kernel/u-boot)
  `inherit kconfig` then override `do_build`/`do_install` inline — the current provider
  `build.sh` bodies MOVE INTO the recipe as those functions (deleting `PKG_BUILD=<script>`, the
  out-of-line pointer — inline is more Buildroot-faithful).
- `classes/make-c.sh` — `do_build`: `make -C ${PKG_DIR} ${goals}`. (today's make-c.sh; the
  custom kernel/bootloader `inherit make-c`.)
- `classes/substrate.sh` — the libc substrate build. libc `inherit substrate`. (unchanged body.)
- Host classes (`classes/host/*.sh`) — ALREADY this shape (`host_style_build`); rename the
  function to the uniform `do_build`/`do_install` set for consistency, or keep as-is under a
  host-specific contract. (Low priority — host side already generic. See §3 phasing.)

`do_fetch` default = `forge_fetch` (from #A): the engine's default fetch task. A recipe almost
never overrides it (fetch is fully generic already).

### 1.4 Consistency with Buildroot (why this is the right target)

Buildroot recipe = a Make fragment: declarative `FOO_VERSION`/`FOO_SITE` + inline
`define FOO_BUILD_CMDS … endef` / `define FOO_INSTALL_TARGET_CMDS … endef`, ending in
`$(eval $(generic-package))` (write your own CMDS) or `$(eval $(autotools-package))` (inherit
defaults, override via `FOO_CONF_OPTS` / `FOO_POST_INSTALL_TARGET_HOOKS`).

forge's `inherit <class>` + inline `do_build()`/`do_install()` is the SAME model in bash:
- `inherit compile-c` with no overrides ≙ `$(eval $(autotools-package))` (typed defaults).
- `inherit kconfig` + `do_build(){…}` ≙ `$(eval $(generic-package))` + `define FOO_BUILD_CMDS`.

So forge lands AT Buildroot's altitude on this axis — typed default + per-recipe inline override
— not the stricter pure-data model, and not the full Yocto task-DAG. `do_build()` in bash is
ergonomically nicer than `define FOO_BUILD_CMDS` in Make (real functions, `${PKG_NAME}` in
scope, no `$(@D)` macro dance).

### 1.5 The generic runner (replaces layer.mk + build_package)

ONE runner, the generalization of `host.sh`'s `_host_build_one`. Pseudocode:

```sh
# build_one <recipe-file> — the generic node builder (target side)
build_one() {
  ( # subshell so each node's vars/functions don't leak into the next
    unset -f do_fetch do_build do_install do_rootfs 2>/dev/null
    do_fetch() { forge_fetch "${recipe}" "${scratch}"; }   # engine default (from #A)
    inherit()  { source "${FORGE_CLASSES}/$1.sh"; }        # the ONLY name->file binding
    source "${recipe}"          # facts as vars + inherit(s) + any do_* overrides
    : "${PKG_NAME:?}"
    do_fetch                    # -> PKG_SRC_DIR (or the CC/substrate inputs)
    do_build                    # class default or recipe override
    do_install                  # into the shared STAGE (or OUTPUT_DIR for boot artifacts)
  )
}
```

- No `if/else` on kind. `inherit` + last-definition-wins does the dispatch.
- The subshell + `unset -f` is the isolation `build_package` did with `unset PKG_*` — extended to
  the functions, so one recipe's `do_build` can't leak to the next.
- `do_install` for a boot artifact (kernel/u-boot) writes `OUTPUT_DIR`; for a rootfs package
  writes the shared `STAGE`. That destination is the class's/recipe's business, not the engine's.

### 1.6 rootfs becomes a recipe with `do_rootfs` = pack-only

```sh
# layers/rootfs/recipe.mk
PKG_NAME=rootfs
PKG_DEPENDS=${PACKAGES}        # the install set — coreutils busybox … (the graph edges)
PKG_HOST_DEPENDS=gen_init_cpio
inherit rootfs-image           # provides do_rootfs (pack STAGE -> cpio) + runtime-.so staging
```

- The engine builds `PKG_DEPENDS` (each package's `do_build`/`do_install` into `STAGE`) BEFORE
  rootfs's `do_rootfs` — via the SAME graph mechanism that already sequences `rootfs: libc`
  (#9) and `image: kernel bootloader rootfs`. `walk_deps` / the Make graph does the ordering.
- `do_rootfs` = today's `rootfs_pack` + `rootfs_overlay_merge` + `stage_substrate_runtime`
  (§3f). It NO LONGER contains `build_package` — packages are their own nodes now.
- This is Yocto's image `do_rootfs` almost exactly: "assemble the final rootfs from the
  already-built packages."

### 1.7 What stays exactly as-is

- **#A `forge_fetch`** — becomes the default `do_fetch`; the primitive is unchanged.
- **#B per-layer stamps** — unchanged; the runner is invoked from the same stamp recipes in
  `rules.mk`. (Nodes are still stamped per-layer, not per-task — the deliberate non-goal.)
- **The libc substrate as a CC-profile build-CONFIG input** — `ccprofile.sh`/`libc-profile.sh`
  thread the selected libc into every package's compile flags. This is NOT per-package
  branching; it's Buildroot's fixed-per-build toolchain. Untouched.
- **`resolve.sh walk_deps`** — reused to resolve `PKG_DEPENDS` for the package graph (it already
  resolves host deps); no new resolver.
- **The Make dependency graph** (`rules.mk`) — stays the inter-node engine. `inherit`/`do_*` is
  purely how a node BINDS ITS BUILD; Make still decides node ORDER + incrementality.

---

## 2. What gets deleted / transformed (the concrete diff shape)

| Today | After |
|---|---|
| `forge/core/layer.mk` (make-c\|kconfig `ifeq` ladder) | **DELETED** — generic runner + `inherit` |
| `forge/core/{kernel,bootloader,libc,rootfs}.mk` (5 drivers) | collapse to ONE generic runner invoked from the stamp recipes |
| `build_package` case in `layers/rootfs/build.sh` | **DELETED** — packages are graph nodes |
| `PKG_BUILD=<script>` (out-of-line build pointer) | **DELETED** — build logic inline as `do_build` in the recipe |
| `PKG_TYPE=<kind>` as engine dispatch key | replaced by `inherit <class>` (PKG_TYPE optional metadata) |
| `forge/core/styles/*.sh` (executed top-to-bottom) | `forge/core/classes/*.sh` (define default `do_*`) |
| provider `build.sh` bodies (kernel/u-boot) | move INTO their recipes as `do_build`/`do_install` |
| rootfs assembler = build-packages + pack | rootfs recipe = `do_rootfs` (pack only) |
| `image.mk`/image `build.sh` (compose) | image recipe `inherit`s an image class; `do_install`=compose (or keep image as-is initially — see §3) |

---

## 3. Phasing (do in order; verify §5 after EACH; keep unstaged unless told)

Risk-ascending, each phase independently green:

1. **Classes + the generic runner, PROVEN on packages first.** Rename `styles/`→`classes/`; wrap
   each style body in `do_build`/`do_install`. Add the `build_one` runner (with `inherit`). Point
   `build_package`'s guts at `build_one` for the two package types (coreutils/busybox) — deleting
   the `case`. Verify: `make rootfs` both stacks, golden 6/6. (Smallest blast radius: packages
   only; providers still use layer.mk.)
2. **Migrate the providers off `layer.mk`.** kernel/bootloader/libc recipes gain `inherit` +
   inline `do_build`/`do_install` (moving each provider `build.sh` body into its recipe). Route
   `kernel.mk`/`bootloader.mk`/`libc.mk`'s stamp recipes through `build_one`. DELETE `layer.mk`
   and `PKG_BUILD`. Verify: `make kernel/bootloader/libc` both stacks, golden 6/6, `make image`.
3. **rootfs → `do_rootfs` recipe.** `PKG_DEPENDS=${PACKAGES}`; the assembler keeps only pack +
   overlay + runtime-.so staging as `do_rootfs`; packages build as their own nodes ahead of it
   (graph edge). Verify: all four (libc×link) profiles, golden 6/6.
4. **Collapse the drivers.** With every layer routing through `build_one`, fold
   `kernel.mk`/`bootloader.mk`/`libc.mk`/`rootfs.mk` into ONE generic stamp+run rule in
   `rules.mk` parameterized by recipe path. Verify: full matrix, golden 6/6.
5. **(Optional) image + host unification.** Give `image` a class + `do_install`=compose; rename
   host `host_style_build`→`do_build` for one contract across host+target. Lowest value (image is
   a pure composer already; host is already generic) — do only if it removes real duplication.

If time-boxed, **phases 1–3 are the complete win** (both ladders gone, rootfs is pack-only);
4–5 are consolidation.

---

## 4. Explicitly NOT in scope (Yocto features we deliberately skip)

- **`addtask` / intra-recipe task DAG.** `do_*` stays a fixed short sequence run by `build_one`,
  not a per-task before/after graph. No inserting `do_patch` between tasks via metadata.
- **Per-task stamping / sstate / hash-equivalence.** Stamps stay per-layer (mtime, from #B). No
  content-signature cache. (Deepest divergence; not worth it at this scale.)
- **Per-recipe `${D}` + `do_package` + `do_populate_sysroot`.** Packages install into a shared
  `STAGE` in place; no private destdir aggregation, no packaging split.
- **Metadata layers** (`bblayers.conf`, `BBFILE_PRIORITY`, `.bbappend`). Catalogs stay flat dirs.
- **Task append/prepend hooks** (`do_install:append`). Bash `source` gives OVERRIDE (redefine
  `do_build`), not "run mine after the default." Start override-only. IF a real
  need appears, add a `do_install_append()` convention the runner calls after `do_install` — but
  YAGNI until then.
- **Composed multi-`inherit`.** The mechanism allows it (each `inherit` just sources a file), but
  we don't design for class-composition until a second class genuinely needs mixing.

---

## 5. Verification (after each phase — must be behaviour-identical)

- `make -C projects/gameboy-v3 print-config` both stacks — resolves unchanged.
- **golden 6/6** (`kernel/test/golden.sh`) after every phase.
- `make rootfs` custom + musl, static + dynamic (all four libc×link profiles); the #B incremental
  checks still hold (no-op skips; touch a coreutils `.c` → only rootfs rebuilds).
- `make image` full-custom composes; all-OSS composes up to the pre-existing uboot
  `DM_SPI_FLASH` fragment bug (unrelated).
- **Dual-read proof per phase:** the Make graph still resolves `PKG_DEPENDS`/`PKG_*` via awk with
  `do_*` functions present in the recipe (no parse breakage).
- **The `if/else` is gone:** `grep -n 'ifeq.*STYLE\|case "${PKG_TYPE}"' forge/` returns nothing;
  the only name→file binding is `inherit` in recipes.
- Byte-compare a rebuilt zImage / initramfs against a pre-refactor build — identical (structural
  change, zero output change).
- **Gold standard (end):** clean rebuild + flash-and-boot BOTH stacks on the T113 rig (custom →
  `gv3$`, all-OSS → `~ #`).

---

## 6. Note for the implementer

The engine already has the generic half (`host.sh` convention dispatch + `resolve.sh` graph
walker); this refactor promotes that shape to the target side and deletes the two `if/else`
ladders that are the last engine-side enumeration of build kinds. The mechanism is proven:
`inherit`-then-override via bash last-definition-wins (tested), and the dual-read recipe
(facts+functions in one file, tested). Keep the frame honest — this makes forge
**Buildroot-with-shell-tasks + convention dispatch**, closing the gap the user can see and feel
(the `if/else` smell); it does NOT make forge Yocto (no task DAG, no sstate, no `${D}`), and
those are deliberate non-goals at this scale. Prefer the smaller change: phases 1–3 alone remove
both ladders and make rootfs pack-only, which is the whole point.

---

## Addendum — ONE dependency walker: Make (2026-08-06)

**Trigger (user):** "why are host dependencies treated in a special way? where do we resolve
target deps and run them in order — should we unify, isn't that how Yocto/Buildroot do it?"

**Finding:** host deps used a real closure-walker (`resolve.sh` `walk_deps`, via `host.sh`);
target deps (`PKG_DEPENDS`) were **declared but IGNORED** — the graph edges were hand-wired in
`rules.mk`. And `walk_deps` had exactly ONE caller left (`host.sh`) — the Phase-3 rewrite had
orphaned its "target" clients. So forge had TWO dependency mechanisms, and `PKG_DEPENDS` lied.

**Decision (user):** unify on ONE engine — and since **Make is already a topological walker**
(it orders the target graph), the clean-by-subtraction move is to make **Make the one walker**
for host deps too, and retire the shell walker. (Not "route target deps through walk_deps" —
that would reinvent Make in bash and lose `-j`/ordering.)

**Change:**
- **`resolve.sh` DELETED** (the shell `walk_deps`; no callers left).
- **`host.sh`**: the `host_provision` closure-walker → `host_build <name>...` (builds exactly
  the named node(s); NO walk). `_host_build_one` (stamp+witness + host-style dispatch) is
  unchanged — that's the real per-node work.
- **`rules.mk`**: emits a `host-<name>` `.PHONY` node per hostpackage, with `host-<name>:
  host-<dep>` edges from each recipe's `PKG_HOST_DEPENDS` (via `_hostpkg_get_host`). Each build
  node lists its host deps as `$(call _hostdeps,<deps>)` = `host-<dep>` PREREQUISITES.
  `_build_recipe` dropped its `host.sh` line — host provisioning is now Make prerequisites.
- Make gives the three things `walk_deps` gave: transitive **closure**, **topological order**
  (`host-genimage: host-libconfuse` builds libconfuse first — verified), and **build-once**
  (a `.PHONY` node depended on by kernel+bootloader+libc runs ONCE per invocation — verified).

**Result:** ONE dependency engine (Make) for host AND target. `PKG_DEPENDS`/`PKG_HOST_DEPENDS`
are both real Make edges; nothing is decorative; `resolve.sh` + the shell walker are gone.
This is the Buildroot/Yocto "one DEPENDS mechanism" property, achieved by subtraction.

**Verified:** dry-run shows host deps as ordered prereqs (libconfuse before genimage; toolchain-
glibc+binman-venv before uboot); build-once (host-toolchain-glibc runs 1×/`make image`); cold
rebuild (wiped gen_init_cpio → Make graph rebuilt it); `make image` custom composes; golden 6/6.
Rig boot pending a hardware session.
