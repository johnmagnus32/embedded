# Forge Plumbing Cleanup: forge.conf, backends/ split, libc-as-provider

**Status:** PROPOSED — not yet implemented (as of 2026-08-04). A design spec for
another engineer/agent to execute. Three related cleanups to the forge engine's
*plumbing* (not its structure — the provider/package/host models are done and
silicon-verified). They are grouped into ONE doc because they touch the SAME files
(`forge/core/providers.mk`, `forge/core/rules.mk`, the layer makefiles, and every
backend's input model), so doing them in isolation would mean rewiring the same
plumbing three times. Do them in the phase order of §5.

These came out of a readability pass on the engine. The essential logic is simple;
the pain is **accidental** complexity in how state crosses the Make↔bash boundary
(a hand-threaded `BACKEND_ENV` string with fussy quoting) and two remaining spots
that branch on a hardcoded name instead of a generic property. None of this changes
build *outputs* — it is pure legibility + genericness, verified byte-identical.

> **How to use this doc:** read §0 (current state + the hard constraints) FIRST —
> especially §0.3, a GNU Make 3.82 gotcha that kills the obvious implementation of #1.
> Then implement §5's phases in order, verifying (§6) after each. Every claim about
> the tree was true at writing; **verify each file still matches before editing.**

---

## 0. Current state + hard constraints

### 0.1 Where the engine lives (post-reorg)

The engine is `forge/core/` (see docs/LINUX_HOST_PACKAGE_MODEL.md for the reorg). Key files:
- `forge/core/rules.mk` — top-level targets + the dependency graph; builds the
  `BACKEND_ENV` string and threads it (plus ~15 more vars) into recursive `$(MAKE)`
  calls to the layer makefiles.
- `forge/core/providers.mk` — resolves the selection (reads recipes, computes
  `KERNEL_RECIPE`/`*_STYLE`/`*_SRC`/`CFG`/`ROOTFS_TAG`/…). Included by rules.mk.
- `forge/core/{kernel,bootloader,rootfs,image}.mk` + `layer.mk` — the layer drivers.
- `forge/core/` — the shell mechanism (see §3 for the full inventory).
- Two-root addressing: `FORGE_ROOT` = `forge/` (holds catalogs `packages/`,
  `providers/`, `hostpackages/`); `REPO_ROOT` = git root (holds custom source
  `kernel/ libc/ bootloader/ coreutils/`). lib.sh + providers.mk both compute these.

### 0.2 The `BACKEND_ENV` threading (#1's target) — verified current text

`rules.mk` builds one env string and re-passes it, with a 6-line comment agonizing
over single-vs-double quoting `PACKAGES` (which can contain spaces):
```make
BACKEND_ENV := PRODUCT_DIR=$(PRODUCT_DIR) BOARD_NAME=$(BOARD) REPO_ROOT=$(REPO_ROOT) \
               BOOTLOADER=$(BOOTLOADER) KERNEL=$(BUILD_KERNEL) \
               LIBC=$(LIBC) LIBC_SRC=$(LIBC_SRC) PACKAGES='$(PACKAGES)' ROOTFS_TAG=$(ROOTFS_TAG) \
               ROOTFS_TARGET=$(ROOTFS_TARGET) \
               MEDIA=$(MEDIA)
```
And the `kernel bootloader rootfs:` recipe threads `BACKEND_ENV="$(BACKEND_ENV)"`
PLUS ~15 more `VAR=$(VAR)` (KERNEL_STYLE, KERNEL_RECIPE, KERNEL_MAKE_GOALS,
KERNEL_BUILD, KERNEL_HOST_DEPENDS, KERNEL_TARGET, KERNEL_SRC, … and the BOOTLOADER
equivalents) into the sub-make. That ~15-line forwarding block is the "plumbing crap."

Backends receive their inputs as **ambient environment** — a script's real input
contract is "~15 env vars someone upstream set," discoverable only by reading the
whole script. (Most backends DO document it in a header block — "Inputs:"/"CONTRACT"
— but the values still arrive invisibly via the threaded env.)

### 0.3 ⚠️ HARD CONSTRAINT: the product runs under GNU Make **3.82**

`projects/gameboy-v3/Makefile` does `include $(REPO_ROOT)/forge/core/rules.mk` under
whatever `make` invoked it — which on this host is **GNU Make 3.82** (verified:
`make --version` → 3.82). Forge builds/uses a pinned Make ≥4 as a HOST TOOL, but
`providers.mk`/`rules.mk` parse under 3.82, before any host tool is on PATH.

**Consequence for #1:** the obvious "providers.mk writes the config with `$(file >…)`"
does NOT work — `$(file …)` was added in Make 4.0. The spec below uses a
**3.82-safe** mechanism instead (a `$(shell …printf…)` write, OR emitting the file
from a recipe step). Do not use `$(file)`.

Also note: `$(shell)`, `$(eval)`, `-include`, `override` all work on 3.82 (providers.mk
already uses them). The dual "Make-`include` + bash-`source` the same KEY=value file"
trick is already proven in the tree — `board.conf` is read BOTH ways today.

### 0.4 The two libc special-casing sites (#3's target) — verified

libc is the one provider NOT filed under `providers/` — it is special-cased in
providers.mk by a hardcoded enum + a hardcoded path map:
```make
# line ~44:
$(if $(filter-out custom musl,$(LIBC)),  $(error LIBC must be custom|musl (got '$(LIBC)')))
# line ~102:
LIBC_SRC := $(strip $(if $(filter custom,$(LIBC)), $(REPO_ROOT)/libc, $(BUILD)/musl))
```
(kernel/bootloader are validated generically by recipe existence and resolved from
`providers/<role>/<impl>/provider.mk`; only libc still hardcodes its impl names.)
The dead third copy of this map — a `case "$LIBC"` fallback in rootfs-assemble.sh —
was already removed; these two are what remain.

### 0.5 Files to read before starting
`forge/core/rules.mk`, `forge/core/providers.mk`, `forge/core/layer.mk`,
`forge/core/lib/core.sh` (self-location + the `recipe_get`/`_recipe` readers),
`forge/core/steps/rootfs-assemble.sh` (a backend that also has a standalone caller),
`projects/gameboy-v3/board/t113-gameboy/board.conf` (the dual-read precedent),
`projects/gameboy-v3/Makefile`, and the two standalone backend callers
`kernel/test/golden.sh` + `libc/test/dynamic.sh`.

### 0.6 Review findings folded into this spec (verified against the tree 2026-08-04)

A first-draft of this doc under-stated three risks; all were confirmed by grep and
are now designed-for below. Summary so an implementer knows these were deliberate:

- **The `${HERE}` intra-source hazard (drove the §2 rewrite).** Every backend sources
  its siblings by a RELATIVE `"${HERE}/x.sh"` — 11 such lines, NONE containing the
  string `backends/`. A directory split scatters siblings across `lib/`+`styles/`+
  `steps/`, so `${HERE}` stops resolving them and all 11 need hand-recomputed relative
  paths. The original doc's driving grep (`core/backends|BACKENDS`) does not surface
  these — it would look "done" while sources silently break at runtime. **Because the
  split's payoff is cosmetic and its cost is 11 fragile path recomputations, §2 is
  re-scoped to rename-IN-PLACE by filename convention (no dir boundaries, no `${HERE}`
  changes).** The dir-split option is kept but marked NOT recommended, with the grep
  it would actually require.
- **The forge.conf quoting hazard (§1).** `$(PACKAGES)` (space-bearing) is read on the
  MAKE side — but ONLY at providers.mk:45/:120/:138, which run BEFORE forge.conf is
  written (they read raw config.mk PACKAGES). The only POST-include Make read is
  rootfs.mk:17, a cosmetic `@echo`. So the blast radius of a bash-quoted value in a
  Make-included forge.conf is one ugly log line, NOT a corrupted `ROOTFS_TAG`/foreach —
  but it must still be designed out: layers consume the already-resolved `ROOTFS_TAG`,
  never raw `$(PACKAGES)`. §1 now says this instead of "board.conf ignores the quotes."
- **`PKG_FETCH=prebuilt` is a NEW fetch arm (§3).** The existing vocabulary is exactly
  `local` | `git` (providers.mk:99-100 branch on `filter local`, else the git checkout
  dir). musl-as-libc needs a THIRD arm (`prebuilt` → the toolchain sysroot). So §3 is
  "replace the hardcoded map with a recipe-driven resolver that GAINS a `prebuilt`
  arm," not a pure deletion. Reframed below.

---

## 1. Change #1 — one resolved `forge.conf` instead of the `BACKEND_ENV` string

**Goal.** Resolve the selection ONCE, write it to a generated file, and have both the
layer makefiles (`include`) and the shell backends (`source`) read that ONE file.
Kill the hand-threaded `BACKEND_ENV` string + the ~15-var recursive-make forwarding +
the quoting gymnastics. The data flow becomes inspectable: `cat build/forge.conf`
shows exactly what the engine resolved.

**Mechanism (the board.conf trick, generalized — with a caveat board.conf doesn't hit).**
A `KEY=value` file that is BOTH Make-`include`-able and bash-`source`-able (bare
`KEY=value`, no spaces around `=`). providers.mk resolves all the facts, then writes
them to `$(BUILD)/forge.conf`.

⚠️ **The quoting hazard board.conf does NOT cover (verified — read this before writing forge.conf).**
board.conf works dual-read only because the MAKE side reads nothing space-bearing
from it (just bare `KERNEL_TARGET`/`ROOTFS_TARGET`). forge.conf is different: it would
carry `PACKAGES` (space-bearing). If you write `PACKAGES="coreutils busybox"` (quoted
for bash) and a layer `.mk` `include`s it, Make sees the LITERAL quote characters —
`$(PACKAGES)` becomes `"coreutils busybox"`, so a `$(subst … +,$(PACKAGES))` yields
`"coreutils+busybox"`, a `$(foreach)` iterates `"coreutils` and `busybox"`, etc. So
you canNOT just "quote for bash and trust Make to ignore it."

**What saves you (verified):** the load-bearing MAKE reads of `$(PACKAGES)` all live in
`providers.mk` (`:45` empty-check, `:120` foreach surface-check, `:138` `ROOTFS_TAG`),
which runs BEFORE forge.conf is written and reads raw `PACKAGES` from `config.mk` —
they never read it back from forge.conf. The ONLY post-`include` Make read is
`rootfs.mk:17`, a cosmetic `@echo`. So the design rule is:
- **Make side: layers consume the already-RESOLVED values** (`ROOTFS_TAG`, `CFG`,
  `LIBC_SRC`, `*_STYLE`, …), never raw space-bearing `$(PACKAGES)`. Fix `rootfs.mk:17`
  to log `$(ROOTFS_TAG)` (already resolved, `+`-joined, space-free) instead of
  `$(PACKAGES)`. After that, NO Make consumer reads a space-bearing key from forge.conf.
- **Bash side:** backends that genuinely need the space-bearing list (`rootfs-assemble.sh`
  iterates `PACKAGES`) `source` forge.conf where the quotes ARE wanted.
- **If a key must be read space-bearing by BOTH:** don't — resolve it to a space-free
  form (like `ROOTFS_TAG`) for Make and keep the raw form for bash only. There is no
  single quoting that satisfies bare-Make and quoted-bash simultaneously.

> **AS-BUILT DEVIATION (2026-08-04) — option (A) is WRONG on Make 3.82; shipped (B).**
> The spec below preferred option (A), a parse-time `$(shell printf … "$$FORGE_CONF_BODY")`.
> Verified in isolation on the product's Make 3.82: **(A) writes an EMPTY file.** GNU Make
> 3.82 exports a `define` to RECIPE shells but NOT to a parse-time `$(shell …)`, so
> `$$FORGE_CONF_BODY` expands to nothing there. The implementation therefore uses **option
> (B)**: a real `$(BUILD)/forge.conf:` recipe target (which DOES see the exported define),
> made `.PHONY` (the config derives from make VARIABLES, not input files, so Make can't
> detect a selection change as a normal prereq — without `.PHONY`, `make image LIBC=musl`
> after `…LIBC=custom` would reuse a stale file) and a prerequisite of every layer. Also
> shipped: the forwarded build-dir var is named **`FORGE_BUILD`, not `BUILD`** — a
> command-line `BUILD=$(BUILD)` on the recursive `$(MAKE)` propagates via `MAKEFLAGS` into
> the providers' OWN `make -C kernel`/`make -C bootloader`, overriding their internal
> `BUILD` (`build/$(BOARD)`, `build`); the bootloader's `fel:` then `rm -rf $(BUILD)`'d the
> product build dir incl. `build/toolchain/`, losing the cross-gcc. Namespacing the
> forwarded var can't collide with a provider Makefile.
>
> **AS-BUILT: versions.env vs forge.conf precedence (the §"forge.conf vs versions.env" note
> below, resolved).** Shipped source order in lib.sh is `versions.env` → `board.conf` →
> `forge.conf`. forge.conf does NOT clobber an EXPLICIT caller env (snapshot/restore block:
> snapshot the caller-set values of forge.conf's key list, source the file, restore the
> snapshot — caller wins where set, forge.conf fills the rest). Note the subtlety: because
> `board.conf` sets `KERNEL_TARGET`/`ROOTFS_TARGET` *before* the snapshot, they read as
> "already set" and so board.conf wins over forge.conf for those two keys — harmless
> because forge.conf derives the SAME values from the board via providers.mk (identical
> for gameboy-v3). gameboy-v3 ships no versions.env, so the two-file overlap is moot today.

**3.82-safe write (do NOT use `$(file)` — see §0.3).** Two acceptable options:
- **(A, ~~preferred~~ BROKEN on 3.82 — see as-built note above) a `$(shell)` write at parse time**, guarded so it runs once:
  ```make
  # after all *_STYLE/*_RECIPE/CFG/... are computed:
  define FORGE_CONF_BODY
  PRODUCT_DIR=$(PRODUCT_DIR)
  BOARD_NAME=$(BOARD)
  ROOTFS_TAG=$(ROOTFS_TAG)            # resolved, +-joined, SPACE-FREE — Make-safe
  PACKAGES="$(PACKAGES)"             # quoted: bash-only consumer (rootfs-assemble); NO Make layer reads this back (see the quoting hazard above)
  ... (one KEY=value per line)
  endef
  export FORGE_CONF_BODY
  _ := $(shell mkdir -p $(BUILD); printf '%s\n' "$$FORGE_CONF_BODY" > $(BUILD)/forge.conf)
  ```
  (Exporting the define to the environment and having the shell echo `$FORGE_CONF_BODY`
  avoids escaping every value inline. Verify the quoting survives — this is the fussy
  part; test with a space-bearing `PACKAGES="coreutils busybox"`.)
- **(B) a real recipe prerequisite**: a `$(BUILD)/forge.conf:` target that writes the
  file, made a prerequisite of `kernel bootloader rootfs image`. More Make-idiomatic
  (no parse-time `$(shell)` side effect) but adds a node to the graph. Either is fine;
  pick one and document why.

**Consumers.**
- **Backends**: replace "read ambient env vars" with `source "$BUILD_DIR/forge.conf"`
  at the top (after lib.sh, which computes `BUILD_DIR`). Each backend's header
  "Inputs" block now names *forge.conf keys*, and the require-guards (`: "${VAR:?}"`)
  still apply — the file is the single input surface.
- **Layer makefiles**: `include $(BUILD)/forge.conf` instead of receiving the ~15
  `VAR=$(VAR)`. rules.mk's `kernel bootloader rootfs:` recipe shrinks to just
  `$(MAKE) -f $@.mk build` (plus the few genuinely per-call args — see below).
- **`BACKEND_ENV` is deleted.**

**What STAYS an explicit arg (not in forge.conf).** The handful of genuinely
per-invocation values that differ *within* one build: `LAYER`, `STYLE`, `SRC`,
`MAKE_GOALS`, `BUILD_SCRIPT`, `RECIPE`, `HOST_DEPENDS` (layer.mk sets these per layer),
and `LINKAGE`/`ROOTFS_TARGET`-override (dynamic.sh passes these to rootfs-assemble.sh
standalone). Global product/board/selection config → forge.conf; per-call args → the
call. This split is the whole point: config is a file you can inspect; args are few.

**Standalone-caller caveat (IMPORTANT).** `kernel/test/golden.sh` and
`libc/test/dynamic.sh` call backends DIRECTLY, not through rules.mk — so `forge.conf`
may not exist when they run. Two options: (a) they run `make print-config`/a tiny
`make forge.conf` target first to generate it, then source it; or (b) backends treat
forge.conf as optional (`[ -f "$BUILD_DIR/forge.conf" ] && source it`) and the
standalone callers keep setting the few vars they need via env (as they do today).
Prefer (b) — it keeps the test harnesses' explicit env-setting working unchanged and
makes forge.conf a convenience for the graph path, not a hard dependency. Verify both
harnesses still pass.

**⚠️ forge.conf vs the optional `versions.env` (source-order precedence).** `lib.sh`
still `source`s an optional `$PRODUCT_DIR/versions.env` if present (line ~110; gameboy-v3
ships none today, but the guard stays for future products). Once backends ALSO `source`
forge.conf, two files can set env keys and **whichever is sourced LAST wins silently.**
Decide + document the order deliberately: recommend `source versions.env` FIRST, then
`source forge.conf`, so the engine-resolved forge.conf values win over a stray product
override (or vice-versa if product override is intended — but pick one and comment it).
Also confirm the key sets don't overlap unintentionally (forge.conf carries resolved
selection/paths; versions.env historically carried component pins, now mostly moved
into recipes — so overlap should be near-zero, but verify no key is set by both).

**Risk.** This rewires the engine spine. It is the highest-risk of the three. Land it
FIRST but as its own phase, fully verified (§6), before touching #2/#3 — a broken
forge.conf write would fail every build, so it must be proven before piling on.

---

## 2. Change #2 — rename `backends/` files to reflect what they hold (NOT a dir split)

> **AS-BUILT DEVIATION (2026-08-04) — the FULL dir split was done (overriding this section's
> recommendation), and flattened one level further: no `backends/` dir at all.** The user
> asked for the cosmetic payoff of real directories. Final layout is three role dirs
> promoted directly under `forge/core/`:
> ```
> forge/core/
>   *.mk (orchestrators)   lib/{core.sh,resolve.sh,ccprofile.sh}
>   styles/{make-c,kconfig,compile-c,substrate}.sh + styles/host/{host-*}.sh
>   steps/{toolchain,image,rootfs-assemble,rootfs-pack,host}.sh   defaults/
> ```
> Renames: `lib.sh`→`lib/core.sh`, `cc-profile.sh`→`lib/ccprofile.sh`, `resolve.sh`→
> `lib/resolve.sh`; `build-make-c.sh`→`styles/make-c.sh` (and `build-{kconfig,compile-c,
> substrate}`→`styles/{kconfig,compile-c,substrate}.sh`); `host-styles/`→`styles/host/`;
> the five step scripts move to `steps/` unrenamed.
>
> The `${HERE}` hazard this section warned about was REAL and handled: all 11 cross-file
> `source` lines were rewired to the new relative paths (`"${HERE}/../lib/core.sh"`,
> `"${HERE}/../styles/make-c.sh"`, `"${HERE}/../steps/host.sh"`); `lib/core.sh`'s self-
> location `..`-climb was corrected (it recovers `forge/core` to compute FORGE_ROOT/
> REPO_ROOT); `steps/rootfs-assemble.sh`'s `../defaults/rootfs.devs` depth was fixed;
> `steps/host.sh`'s `host-styles/${PKG_TYPE}.sh` dispatch → `../styles/host/${PKG_TYPE}.sh`
> (PKG_TYPE in the hostpackages recipes is untouched — still the bare filename). External
> refs updated: the makefile var `BACKENDS`→`CORE` (= `$(FORGE_DIR)` = `forge/core`) with
> its recipe uses now `$(CORE)/steps/…` / `$(CORE)/styles/…`; both provider `build.sh`
> scripts source `core/lib/core.sh` + `core/steps/host.sh`; `libc/test/dynamic.sh`'s
> `ASSEMBLE=` + hint strings; README + this doc + the host-package doc. VERIFIED: golden
> 6/6, full-custom `make image` byte-identical (DTB `57551e34`, kernel `b102ab74`,
> bootloader `2a6ee685`), mainline kernel `.config`/DTB unchanged. The rest of this
> section (the original "rename-in-place / just document it" recommendation) is kept for
> the record but was NOT the path taken.

**Problem.** `backends/` implies "interchangeable implementations behind one
interface." Only ~1/3 of the dir is that. The dir holds THREE kinds of thing:

1. **Genuinely pluggable backends** (dispatched by `PKG_TYPE`/`STYLE`):
   `build-compile-c.sh`, `build-kconfig.sh`, `build-make-c.sh`, `build-substrate.sh`,
   and `host-styles/{host-autotools,host-cc,host-pyvenv,host-tarball-bin}.sh`.
2. **Shared mechanism / toolkit** (sourced by everything): `lib.sh`, `resolve.sh`,
   `cc-profile.sh`.
3. **Per-layer step scripts** (THE step for their layer, not an alternative):
   `toolchain.sh`, `image.sh`, `rootfs-assemble.sh`, `rootfs-pack.sh`, `host.sh`.

**⚠️ Why NOT a directory split (the `${HERE}` hazard — verified).** The tempting fix
is three dirs (`lib/`, `styles/`, `steps/`). DON'T — the payoff is cosmetic and the
cost is real: **every backend sources its siblings by a RELATIVE `"${HERE}/x.sh"`**
(11 lines, verified — see §0.6), and NONE contains the string `backends/`. A dir split
scatters siblings across three dirs, so `${HERE}` (which is the *script's own* dir) no
longer resolves any of them:
- `toolchain.sh`(steps) → sources `lib.sh`(lib) + `host.sh`(steps)
- `build-substrate.sh`/`build-compile-c.sh`(styles) → source `cc-profile.sh`(lib)
- `host.sh`(steps) → sources `resolve.sh`(lib) + dispatches `host-styles/${PKG_TYPE}.sh`(styles)
- `rootfs-assemble.sh`(steps) → sources `lib.sh`(lib), `host.sh`(steps), runs
  `build-substrate.sh`/`build-*.sh`(styles), `bash -c 'source rootfs-pack.sh'`(steps)
- `build-make-c.sh`(styles) → sources `lib.sh`(lib) + `host.sh`(steps)

All 11 would need hand-recomputed `../lib/`, `../steps/` relative paths, and it's
runtime breakage a path-string grep never surfaces (only golden + host_provision
catch it). Not worth it for a naming improvement.

**RECOMMENDED: rename IN PLACE by filename convention (zero `${HERE}` churn).** Keep
everything in `forge/core/` (one dir, so every `"${HERE}/sibling.sh"` keeps
working untouched), and make the three KINDS legible by name prefix:
```
forge/core/
  lib-*.sh     lib-core.sh (was lib.sh)  lib-resolve.sh  lib-ccprofile.sh   (kind 2, toolkit)
  style-*.sh   style-compile-c.sh  style-kconfig.sh  style-make-c.sh  style-substrate.sh
               host-styles/*  (kind 1, pluggable — already visibly a "style" family)
  step-*.sh    step-toolchain.sh  step-image.sh  step-rootfs-assemble.sh
               step-rootfs-pack.sh  step-host.sh                            (kind 3)
```
This still touches the `"${HERE}/x.sh"` lines (the basenames change) — but they stay
IN the same dir, so it's a pure find/replace of names, not relative-path recomputation
across boundaries, and a broken rename fails loudly at `source` (file-not-found) rather
than resolving to the wrong dir. Also lower-value than #1/#3, so it's fine to SKIP
entirely — the clearest cheap win is a one-line note in `forge/README.md` classifying
the existing names (toolkit vs style vs step) and leaving the files put.

**If you do the rename**, the driving grep is NOT `BACKENDS`-based — it's the source
lines + the dynamic dispatch + doc references:
```
grep -rn 'source\|\${HERE}\|_HOST_SH_DIR\|host-styles\|BACKENDS\|/backends/' \
  forge/ projects/ kernel/ libc/ --include='*.sh' --include='*.mk' --include='Makefile' --include='*.md'
```
Per renamed file, update: its own name at every `source "${HERE}/<oldname>"`,
`host.sh`'s `host-styles/${PKG_TYPE}.sh` dispatch (unaffected if `host-styles/` keeps
its name — recommended), lib.sh's self-location string if it greps its own basename,
`rules.mk`'s `$(BACKENDS)/<name>.sh` references, the layer makefiles' `BACKEND`/build
dispatch, and the two test harnesses. Update all four `docs/LINUX_*.md`.

**Bottom line on #2:** re-scoped from "split into dirs" (risky, cosmetic) to
"rename-in-place by convention" (cheap) — or **skip and just document the naming** in
the README. Do NOT do the dir split unless you accept rewiring all 11 `${HERE}` sources.

---

## 3. Change #3 — elevate libc to a `providers/libc/` catalog (kill the last hardcoded provider enum)

**Problem.** libc is the odd provider out. kernel/bootloader are
`providers/<role>/<impl>/provider.mk` recipes, validated generically by recipe
existence and resolved from the recipe. libc alone is hardcoded in providers.mk (the
`custom|musl` enum + the `custom→libc / else→musl` path map, §0.4). Adding a third
libc (uclibc, picolibc) today needs providers.mk edits; for kernel/bootloader it's a
drop-in recipe.

**Target.** Give libc the same catalog treatment:
```
forge/providers/libc/custom/provider.mk   PKG_ROLE=libc PKG_FETCH=local PKG_SOURCE=libc
    (+ the existing libc/build.sh + libc/libc-profile.sh stay in repo-root libc/, referenced by PKG_SOURCE)
forge/providers/libc/musl/provider.mk     PKG_ROLE=libc PKG_FETCH=prebuilt
    (musl is baked into the cross toolchain — thin recipe: no source, no build.sh)
```
Then in providers.mk:
- **Validate by existence** (like kernel/bootloader): the `custom|musl` enum on
  line ~44 is DELETED; a bad `LIBC` fails via `$(wildcard providers/libc/$(LIBC)/provider.mk)`.
- **Resolve `LIBC_SRC` from the recipe — and this ADDS a fetch arm, it is NOT a pure
  deletion (verified §0.6).** The existing fetch vocabulary is exactly `local` | `git`
  (providers.mk:99-100: `$(if $(filter local,$(FETCH)), $(REPO_ROOT)/$(PKG_SOURCE),
  $(BUILD)/<checkout>)` — a TWO-arm branch). musl-as-libc is neither: it's a `prebuilt`
  toolchain sysroot. So replacing the hardcoded line-102 map means a THREE-arm resolver:
  ```make
  # LIBC_SRC by the recipe's PKG_FETCH (local | git | prebuilt):
  #   local    -> $(REPO_ROOT)/$(PKG_SOURCE)     (gv3libc: repo-root libc/)
  #   git      -> $(BUILD)/<checkout>            (a hypothetical fetched-source libc)
  #   prebuilt -> the toolchain sysroot dir      (musl: baked into the cross toolchain)
  ```
  Introduce `prebuilt` as a first-class `PKG_FETCH` value (it's new to the vocabulary —
  today only `local`/`git` exist). Decide whether kernel/bootloader's 2-arm `*_SRC`
  resolution should ALSO gain the `prebuilt` arm for uniformity, or whether it stays
  libc-only for now (fine — no kernel/bootloader is prebuilt today). Either way, name
  it: #3 = "recipe-driven resolver that GAINS a `prebuilt` arm," not "delete the map."

**The honest wrinkle (must be in the recipe's comments).** musl-as-libc is really a
TOOLCHAIN property — the musl cross-compiler *is* the libc — so its recipe is thin and
slightly weird (`PKG_FETCH=prebuilt`, no `PKG_SOURCE`, no `build.sh`). That's fine and
mirrors how cc-profile.sh / build-substrate.sh ALREADY treat musl ("prebuilt sysroot,
no libc-profile.sh / build.sh to source" — property dispatch, not name dispatch). The
libc catalog won't be as clean as kernel/bootloader, but the payoff is: (a) generic
validation, (b) one uniform resolution path, (c) a third libc becomes a drop-in.

**Downstream consumers to check.** `cc-profile.sh` and `build-substrate.sh` already
dispatch on a PROPERTY (does `LIBC_SRC/libc-profile.sh` / `LIBC_SRC/build.sh` exist?),
NOT on the libc name — so they need NO change as long as `LIBC_SRC` still points at the
right dir. `rootfs-assemble.sh` requires `LIBC_SRC` (already a require-guard). The
`_pkg_surface_check` PKG_LIBC logic (musl vs custom) is unaffected. Verify the custom
(gv3libc) substrate still builds and the musl path still no-ops correctly.

**Risk.** Low-medium, contained to providers.mk resolution + two new recipe files.
Independent of #1/#2 in logic, but shares providers.mk with #1 — sequence after #1.

---

## 3b. Change #4 — rootfs layer provisions gen_init_cpio via declare-don't-call (like the other layers)

**Status:** IMPLEMENTED (2026-08-04, this window). Small, standalone, verified — did
not wait for #1/#2/#3.

**Problem.** The kernel and bootloader layers follow **declare-don't-call**: `layer.mk`
runs `lib/host.sh HOST_DEPENDS=…` to provision a layer's `PKG_HOST_DEPENDS` BEFORE the
build step, so the step assumes its tools are present (like a Buildroot package whose
host-deps are make prerequisites). The **rootfs layer did not**: `rootfs.mk` just ran
`steps/rootfs-assemble.sh`, and the assembler reached out and grabbed its own pack tool
mid-run (`host_provision gen_init_cpio` inline), guarded by a confusing comment that
rationalized the exception ("this driver is like layer.mk, so it may provision directly").

That comment was really apologizing for an asymmetry: **Buildroot/Yocto both make the
rootfs/image assembler a dependency-DECLARING recipe** — Yocto's image recipe lists
`cpio-native` etc. in `DEPENDS`; Buildroot's `fs/cpio/cpio.mk` lists `host-cpio` in
`ROOTFS_CPIO_DEPENDENCIES` — so the packing tool is a declared build-dep the infra
provisions ahead of the step, never grabbed imperatively by the assembler. forge's
rootfs layer alone lacked that.

**Fix (proportionate — no fabricated recipe).** `gen_init_cpio` is a FIXED host dep of
the rootfs layer (unlike the kernel's, it doesn't vary by provider selection). So rather
than invent a whole rootfs recipe for one fixed tool, `rootfs.mk` declares it the SAME
way `layer.mk` does — a `host.sh HOST_DEPENDS=gen_init_cpio` line BEFORE the assembler:
```make
build:
	@echo "[forge] rootfs: ..."
	@$(BOOTSTRAP) HOST_DEPENDS=gen_init_cpio $(CORE)/lib/host.sh   # declare-don't-call
	@$(BOOTSTRAP) $(CORE)/steps/rootfs-assemble.sh
```
and the inline `host_provision gen_init_cpio` (+ its rationalizing comment) leaves the
assembler. The assembler still `source`s `lib/host.sh` ONLY if it needs the function
elsewhere — check; if `gen_init_cpio` was its sole use, drop that source too and just
assume the tool is on PATH (lib/core.sh prepends the provisioned tool dir by presence,
exactly as the kernel/bootloader steps rely on).

**Why not a full rootfs recipe?** That's the "fabricate a recipe for one fixed dep"
over-engineering trap. If the rootfs layer ever grows provider-varying host deps (a
different packer per image format, say), promote it to a real recipe then — same
"second consumer forces the abstraction" rule the rest of forge follows. For one fixed
tool, matching layer.mk's provisioning line is the faithful minimum.

**Verify:** golden 6/6 (uses gen_init_cpio), `libc/test/dynamic.sh`, custom + musl
`make rootfs`, and confirm a full-custom `make image` still needs NO mainline-Linux
checkout (gen_init_cpio is a standalone host package, not taken from a kernel tree).

---

## 3c. Change #5 — one recipe filename: `package.mk`/`provider.mk` → `recipe.mk`

**Status:** IMPLEMENTED (2026-08-04, this window). Verified.

**Problem.** Three dirs, two filenames, ONE concept. `packages/*/package.mk`,
`hostpackages/*/package.mk`, and `providers/*/*/provider.mk` are all the SAME object —
a bare `KEY=value` "five facts" recipe, read by the same `recipe_get`/`_recipe_get`
machinery. The only real differences (`PKG_CLASS`/`PKG_ROLE`, and what build style
applies) live INSIDE the file or are implied by the DIRECTORY. So two filenames for one
concept is a small snowflake: it implies a structural difference at the file level that
doesn't exist. (Note `package.mk` is already used by TWO categories — `packages/` and
`hostpackages/` — so the name never meant "a package" specifically anyway.)

**Fix.** Rename every recipe to `recipe.mk`. The **directory** carries the category
(`packages/` = target, `hostpackages/` = host tool, `providers/<role>/` = boot artifact,
and — with #6 — `steps/<layer>/` = engine step); the **filename** uniformly answers "what
is this file?" → a recipe. After: `<dir>/recipe.mk` everywhere.

**Functional reference sites to update (verified — the paths the engine OPENS):**
- `core/providers.mk`: `_recipe = …/provider.mk` (→ recipe.mk); the `_pkg_surface_check`
  two lines reading `packages/$(1)/package.mk`.
- `core/lib/core.sh`: `recipe_path()` → `provider.mk`.
- `core/lib/host.sh`: `_hostpkg_recipe()` → `hostpackages/$1/package.mk`.
- `core/steps/rootfs-assemble.sh`: `mk="${PKGROOT}/$1/package.mk"`.
- `core/steps/toolchain.sh`: two `hostpackages/toolchain-*/package.mk` version reads.
- `core/steps/image.sh`: the two `KERNEL_RECIPE`/`BOOTLOADER_RECIPE` standalone defaults
  (`…/provider.mk`).
- both provider `build.sh`: `PROVIDER_RECIPE="${PROVIDER_RECIPE:-${HERE}/provider.mk}"`.
- Then `git mv` all 15 recipe files, and sweep comments + all four docs + `config.mk`'s
  descriptive comments.

**Mechanical, but wide** — do it as ONE pass with a full grep
(`grep -rn 'package\.mk\|provider\.mk' forge/ projects/ kernel/ libc/`) and verify
golden + both `make image`. A missed functional site fails loudly (recipe-not-found), a
missed comment is cosmetic.

---

## 3d. Change #6 — rootfs + image are engine-step layers with their OWN recipe

**Status:** IMPLEMENTED (2026-08-04, this window). Verified. Supersedes #4's interim
hardcoded-in-rootfs.mk fix.

**Problem (the remaining snowflake, and there are TWO instances).** kernel/bootloader/
libc declare their host tools in a recipe's `PKG_HOST_DEPENDS`, which providers.mk
resolves generically. But the two ENGINE-STEP layers grab their host tools imperatively:
- `rootfs-assemble.sh` / `rootfs.mk` — `gen_init_cpio` (the pack tool). #4 moved the
  timing to declare-don't-call but left the dep HARDCODED in `rootfs.mk` — still special.
- `image.sh` — `genimage` (the SD-image assembler), grabbed inline on the MEDIA=sd path.

**Key distinction (why NOT `providers/rootfs/`).** `providers/<role>/<impl>/` is a
catalog of INTERCHANGEABLE, SELECTED implementations (kernel custom|mainline, etc.).
rootfs and image have NO selection axis — one implementation each, a fixed engine step.
Cramming them into `providers/` would force a fake `<impl>` dimension. They are not
providers. But — post-#5, a `recipe.mk` no longer implies "selectable provider"; it just
means "a component's declared facts." rootfs and image ARE components with facts
(host-deps; for image, per-media host-deps). So they get a recipe, in their own tier.

**Fix.** A new engine-step tier with a recipe each:
```
forge/steps/rootfs/recipe.mk    PKG_CLASS=step  PKG_ROLE=rootfs  PKG_HOST_DEPENDS=gen_init_cpio
forge/steps/image/recipe.mk     PKG_CLASS=step  PKG_ROLE=image   PKG_HOST_DEPENDS=          (base: none)
                                                                  PKG_HOST_DEPENDS_sd=genimage  (MEDIA=sd only)
```
(Location: a top-level `forge/steps/` catalog — peer of `packages/`/`providers/`/
`hostpackages/` — NOT `forge/core/steps/` which holds the step SCRIPTS. Recipe is DATA
under the catalog; the script stays engine mechanism under core/. Pick + record the exact
dir; `forge/steps/` recommended for symmetry with the other catalogs.)

- **providers.mk** resolves `ROOTFS_HOST_DEPENDS`/`IMAGE_HOST_DEPENDS` with the SAME
  `_recipe_get` used for every layer — now uniform across provider AND engine-step layers.
  For image, resolve the base list plus `PKG_HOST_DEPENDS_$(MEDIA)` (so `genimage` is
  declared for `sd`, nothing for `nor`) — keeps the laziness (nor builds provision no
  genimage) as a DECLARED fact, not an inline `if`.
- **rules.mk** forwards `ROOTFS_HOST_DEPENDS`/`IMAGE_HOST_DEPENDS` exactly as it forwards
  `KERNEL_HOST_DEPENDS`.
- **rootfs.mk** provisions `$(ROOTFS_HOST_DEPENDS)` (the value now READ, not hardcoded);
  the `gen_init_cpio` literal leaves rootfs.mk. **image.mk/image.sh**: the inline
  `host_provision genimage` leaves; the engine provisions `$(IMAGE_HOST_DEPENDS)` ahead of
  the composer.
- Net: `<LAYER>_HOST_DEPENDS := $(call _recipe_get,…)` for EVERY layer, provider or
  engine-step; each layer's driver provisions its resolved deps; nothing hardcoded,
  nothing grabbed inline. Both snowflakes gone.

**Why this is proportionate, not over-built.** It does NOT invent a selection axis; the
step recipe is a one-fact declaration (its host-deps). It reuses the existing `_recipe_get`
reader and the existing declare-don't-call provisioning line. The payoff: the "how does a
layer get its host tools" rule is finally uniform — read from a recipe — with zero
exceptions.

**Verify:** golden 6/6; `libc/test/dynamic.sh`; custom + musl `make rootfs`; `make image`
MEDIA=nor (provisions NO genimage — assert its stamp absent) AND MEDIA=sd (provisions
genimage); a full-custom build still needs no mainline-Linux checkout.

---

## 3e. Change #7 — rename steps→layers + colocate the assembler with its recipe (as build.sh)

**Status:** IMPLEMENTED (2026-08-05, this window). Finishes #6: the engine-step layers
now match the provider layers exactly — recipe + build.sh in one folder; `core/` is pure
shared machinery.

**Two problems #6 left.**
1. **Name collision.** #6 created `forge/steps/<name>/recipe.mk` (the recipe catalog),
   but the assembler SCRIPTS still live in `forge/core/steps/` — two `steps/` dirs, same
   word, different roles, same components (rootfs/image) split across both. Confusing.
2. **The assembler is the recipe's build PROCEDURE — it should live WITH the recipe.**
   #6 kept `image.sh`/`rootfs-assemble.sh` in `core/` on a "generic engine mechanism"
   argument. That's the OLD (pre-colocation) framing and it's inconsistent with what
   forge already chose for the kernel: forge does NOT follow Yocto's shared-class model
   (`kernel.bbclass`), it COLOCATES each layer's build procedure with its recipe
   (`providers/kernel/mainline/build.sh`, named by `PKG_BUILD`). By that rule the rootfs
   and image assemblers are the rootfs/image layer's build.sh — same as the mainline
   kernel's — and belong in the recipe folder, not `core/`.

   What Yocto's image recipe OWNS is the answer: the recipe carries the image DATA/hooks
   (`IMAGE_INSTALL`≈PACKAGES, `IMAGE_FSTYPES`≈MEDIA, postprocess); the generic algorithm
   is a class. forge collapsed "class" into "the layer's own build.sh" for kernel — so
   for consistency the rootfs/image algorithm rides in *their* build.sh too, not core/.
   (The genuinely-shared bits — `styles/*`, `lib/*`, `defaults/*` — STAY in core/, because
   they ARE shared across layers, exactly like Buildroot's `pkg-*.mk` infra.)

**Fix.**
```
forge/layers/rootfs/  recipe.mk + build.sh   (build.sh = the old core/steps/rootfs-assemble.sh)
forge/layers/image/   recipe.mk + build.sh   (build.sh = the old core/steps/image.sh)
forge/core/           lib/ styles/ defaults/ + the .mk drivers + toolchain.sh — PURE shared
                      machinery, no per-layer procedure. core/steps/ is gone.
```
- Rename `forge/steps/` → `forge/layers/` (kills the collision; "layer" is the codebase's
  own word — `layer.mk`, `LAYER=`, "the rootfs layer"; and it parallels the other catalogs
  named by content). providers.mk's `_step_get` root becomes `$(FORGE_ROOT)/layers/…`.
- `git mv core/steps/rootfs-assemble.sh → layers/rootfs/build.sh`, `core/steps/image.sh →
  layers/image/build.sh`. Rewire their `${HERE}/../{lib,styles,defaults}` sources: the
  scripts now live at `forge/layers/<name>/`, so they resolve `core/` via `FORGE_ROOT`
  exactly as the provider build.sh scripts already do
  (`FORGE_ROOT="$(cd "${HERE}/../../.." && pwd)"; source "${FORGE_ROOT}/core/lib/core.sh"`;
  styles→`${FORGE_ROOT}/core/styles/…`; defaults→`${FORGE_ROOT}/core/defaults/…`). This is
  the PROVEN depth pattern, not a new hazard — but it IS the real work (recompute those
  ~6 source lines in the two scripts).
- The two layer recipes declare `PKG_BUILD=layers/<name>/build.sh`; providers.mk resolves
  `ROOTFS_BUILD`/`IMAGE_BUILD` from it (same as `KERNEL_BUILD`); rootfs.mk/image.mk run
  `$(ROOTFS_BUILD)`/`$(IMAGE_BUILD)` instead of a hardcoded `$(CORE)/steps/…` path. Now
  the layer's build procedure is recipe-declared, uniform with kernel/bootloader.
- `toolchain.sh` STAYS in `core/` (renamed dir, e.g. `core/toolchain.sh` or a small
  `core/steps/` kept only for it) — it's `make toolchain`, a convenience with no recipe,
  not a layer. Decide + record where it lands.

**Result:** every layer — kernel, bootloader, rootfs, image — is `recipe.mk` + `build.sh`
in one folder, dispatched via `PKG_BUILD`; `core/` holds only shared mechanism (styles,
lib, defaults, drivers). The recipe/mechanism split is uniform with zero exceptions.

**Verify:** print-config both stacks; `make rootfs` custom+musl; `make image` nor+sd;
golden 6/6; `libc/test/dynamic.sh` (a standalone caller of the rootfs build.sh — its
`ASSEMBLE=` path string moves to `layers/rootfs/build.sh`, update it); grep proves no
`core/steps` references remain.

---

## 3f. Change #8 — the rootfs assembler owns installing the substrate's runtime .so's (produce/install split)

**Status:** IMPLEMENTED (2026-08-06, this window).

**Problem — dynamic-loader staging is split across THREE places, two of which are the
wrong owner.** For a DYNAMIC rootfs, the libc's runtime files (the loader + `libc.so`)
must land in the image's `/lib`. Today WHO installs them depends on the libc, and
neither owner is the rootfs:
- **custom (gv3libc):** `libc/build.sh` itself installs `ld-gv3.so.1` + `libc.so` into
  `$STAGE/lib` — a PROVIDER reaching into the rootfs image (the libc equivalent of the
  old inline `host_provision` grabs).
- **musl:** the staging lives in `styles/kconfig.sh` — i.e. the **BusyBox package's build
  style** installs musl's `libc.so` (+ the `ld-musl-armhf.so.1` symlink) into `$STAGE/lib`.
  So "install the libc loader" is owned by whatever package happened to need it first.
- **rootfs assembler:** owns NEITHER — it just runs the substrate build.

That's the Buildroot staging-vs-target violation: a recipe/package writes directly into
the final rootfs. Buildroot separates `INSTALL_STAGING` (produce, into the sysroot) from
`INSTALL_TARGET` (into the image); Yocto separates `do_install` (into `${D}`) from
`do_rootfs` (copy packaged files into the image). The PRODUCER never writes the image.

**Key realization (why libc is NOT specially entangled).** libc/build.sh's real output
(crt0.o, libc.a, libc.so, ld-gv3.so.1, staged UAPI) already goes to `SUBSTRATE_DIR` —
a self-contained artifact. The ONLY thing making it look non-self-contained is that one
`$STAGE/lib` install block. Move that out and libc is as self-contained as a zImage.

**Fix — the rootfs assembler owns runtime-`.so` install into `/lib`, for WHICHEVER libc.**
- `libc/build.sh`: DELETE the `$STAGE/lib` install (lines ~56-66). It now produces
  `libc.so` + `ld-gv3.so.1` into `SUBSTRATE_DIR` and stops. Produce-only.
- `styles/kconfig.sh` (BusyBox style): DELETE the musl-loader `$STAGE/lib` staging
  (the `-print-file-name=libc.so` + `ln -sf … ld-musl` block). A package build style
  no longer installs a libc.
- **rootfs assembler** gains one step, run after the substrate build + package loop,
  only when `PKG_LINK=dynamic`, that installs the substrate's runtime files into
  `$STAGE/lib` by libc — reading from the right SOURCE per libc:
  - **custom:** from `SUBSTRATE_DIR` → `/lib/ld-gv3.so.1` + `/lib/libc.so`.
  - **musl:** from the toolchain sysroot (`${ROOTFS_CROSS_COMPILE}gcc -print-file-name=libc.so`)
    → `/lib/libc.so` + the `ld-musl-armhf.so.1 -> libc.so` symlink.
  The "which files, from where" table is the ONE libc-specific bit; keep it a small
  case in the assembler (the assembler is the rootfs layer's own build.sh, so a
  per-libc install table is legitimately its business — it's the `do_rootfs` analogue).

**Result:** producers (libc/build.sh, the busybox style) are produce-only; the rootfs
assembler is the single owner of "populate the image's /lib with the runtime libc",
unified across custom + musl. No recipe/package writes `$STAGE` for the libc anymore.
This also makes libc a clean, self-contained artifact — the prerequisite for
promoting it to its own sequenced layer, which #9 (below) then did.

**Verify:** custom static (no loader staged — static needs none) + custom dynamic
(`/lib/{ld-gv3.so.1,libc.so}` present); musl static + musl dynamic (`/lib/{libc.so,
ld-musl-armhf.so.1}` present); golden 6/6 (busybox + dynamic cases exercise musl-dynamic
staging — now done by the assembler, not the busybox style); `libc/test/dynamic.sh`
(custom-dynamic loader staged); byte-compare a dynamic initramfs pre/post (identical —
same files land in /lib, just installed by a different owner).

---

## 3g. Change #9 — libc is its OWN sequenced layer (built before rootfs, like kernel/bootloader)

**Status:** IMPLEMENTED (2026-08-06, this window). Depends on #8 (libc had to be a
self-contained artifact first — #8 removed the `$STAGE/lib` install; #9 removes the
`build`).

**Problem — the rootfs assembler still BUILT the substrate as a side effect.** After #8,
`layers/rootfs/build.sh` still opened with `build_substrate()` → `styles/substrate.sh`,
i.e. the rootfs layer compiled gv3libc (or no-op'd musl) before its package loop. That's
the genuine smell the whole thread kept circling: the libc is a distinct build product —
the analogue of the kernel and the bootloader — yet it was produced inside another layer's
script instead of being its own graph node. kernel/bootloader are sequenced layers
(`image: kernel bootloader rootfs`); libc was not.

**Fix — promote libc to a sequenced layer; rootfs only CONSUMES the substrate.**
- **Shared layout in `core.sh`.** `SUBSTRATE_DIR`/`STAGE_INC` moved OUT of the assembler
  into `core.sh` as build-tree layout: `build/libc/substrate-<libc>-<link>` + `build/libc/
  include` (the analogue of `build/linux`/`build/u-boot`). Computed in the ONE place both
  producer and consumers `source`, so they agree by construction — no duplicated formula.
  The link axis comes from `LINKAGE` (a command-line var Make re-exports to every nested
  recipe; the CC-profile's name for it is `PKG_LINK`).
- **`styles/substrate.sh` became the layer's build.sh** (self-sufficient): it now sources
  `core.sh` itself (like `make-c.sh`), captures the `ROOTFS_TARGET` override before the
  board.conf clobber (load-bearing for the `virt`/VFP-free arch profile), derives
  `PKG_LINK` from `LINKAGE`, then dispatches on the property "does the selected libc ship
  a `build.sh`?" — from-source runs it, prebuilt (musl) no-ops. Same one mechanism for
  both impls, so libc gets a thin driver, not a `layer.mk` STYLE dispatch.
- **New driver `forge/core/libc.mk`** (peer of `rootfs.mk`/`image.mk`): provisions
  `LIBC_HOST_DEPENDS` (declare-don't-call, empty today) then runs `substrate.sh`.
- **`providers.mk`** resolves `LIBC_HOST_DEPENDS` from the selected libc recipe, the same
  `_recipe_get` used for kernel/bootloader.
- **`rules.mk`** adds `libc` to the layer target list (`$@.mk` → `libc.mk`), forwards
  `LIBC_HOST_DEPENDS`, and sequences it with an accumulating prerequisite `rootfs: libc`
  (no recipe — Make merges it with the shared layer rule). `LINKAGE` is NOT forwarded
  explicitly: it's a command-line var, already re-exported to every nested recipe env.
- **`layers/rootfs/build.sh`**: `build_substrate()` DELETED. No existence-check replaces
  it — a recipe TRUSTS its declared deps, the way image.sh trusts the kernel/bootloader/
  rootfs layers it composes. `rootfs: libc` is a NORMAL prerequisite, so Make builds libc
  to successful completion before this recipe runs and a libc failure aborts the graph
  before us (holds under `-j` and `-k`); a check would only re-assert that. A *missing*
  substrate can't slip through silently either — cc-profile links gv3libc's `crt0.S.o`
  (and static `libc.a`) by FULL PATH, so the first package build fails loudly regardless
  (and an existence-check couldn't catch the one real silent-wrong, a stale-but-present
  artifact, so it earns nothing). A brief `require_substrate()` guard existed transiently
  in this window and was removed after an adversarial review confirmed it redundant. The
  CC-profile env exports that lived in `build_substrate()` (`LIBC`/`LIBC_SRC`/`PKG_LINK`/
  `BOARD`/`ROOTFS_CROSS_COMPILE` + the `ROOTFS_ARCH_FLAGS*` family) stayed — the PACKAGE
  build styles need them — but moved to top-level config; only the substrate *build* left.
- **`libc/test/dynamic.sh`** (the one standalone caller that invoked the assembler
  directly): now runs `substrate.sh` THEN `build.sh`, playing the `rootfs: libc` edge by
  hand — the assembler no longer checks, so skipping `substrate.sh` fails at the package
  link (missing `crt0.S.o`/`libc.so`), not with a friendly message.

**Result:** the build graph is `kernel bootloader libc rootfs` → `image`, four peer
component layers, none building another's product. The rootfs assembler is purely an
assembler (build packages against the substrate + pack), matching image.sh (compose
already-built components). `build_substrate()` is gone from the rootfs layer.

**Verify:** all four (libc × link) profiles through the real graph — custom static
(`libc.a`), custom dynamic (`libc.so`+`ld-gv3.so.1`, staged to `/lib`), musl static
(no-op layer), musl dynamic (`/lib/{libc.so,ld-musl-armhf.so.1}`); cold `make rootfs`
auto-runs the libc layer first (the graph edge, no in-assembler check);
golden 6/6; `libc/test/dynamic.sh --gv3` (ref + gv3, exercises the substrate→assemble
hand-off + the `virt` arch capture); `make image` full-custom composes with the new
graph order.

---

## 4. Explicitly NOT in scope (things considered and rejected)

- **Unifying the two recipe readers** (`_recipe_get` in Make vs `recipe_get` in bash).
  They INTENTIONALLY differ: the bash one `eval`s `${VAR}` in values (for
  `PKG_ARTIFACT`'s `${KERNEL_TARGET}`); the Make one must not. Merging drags `eval`
  into Make's parse or drops expansion the composer needs — a correctness change, not
  a cleanup. Leave both; the comments already note the split (keep them accurate).
- **Naming the `$(subst $(_space),+,$(PACKAGES))` / `$(VAR:$(REPO_ROOT)/%=%)` string
  ops as helpers.** Single-use, each already has an explaining comment; a helper adds
  indirection for one call site — worse, not better.
- **Deleting the whitespace-strip `override` block in providers.mk.** VERIFIED
  load-bearing: axes are interpolated into `providers/<role>/<impl>/` paths, where a
  stray space misroutes resolution (a trailing-space `KERNEL` silently fell through to
  the mainline path in a test). Keep it; `override` is required so it applies to CLI
  overrides too. (If touched at all: only fix the stale comment claiming the
  non-PACKAGES strips are "not load-bearing" — they are.)

---

## 5. Phasing (do in order; verify §6 after EACH; keep unstaged unless told otherwise)

1. **#3 libc-as-provider** FIRST — it's the smallest, lowest-risk, and independent in
   logic. Doing it first means #1 (which rewrites providers.mk's var-emission) sees the
   already-generic libc resolution and emits `LIBC_SRC` uniformly. Create the two
   recipes, delete the enum + path-map, verify.
2. **#1 forge.conf** — the spine rewrite. Land it alone, fully verified, because a
   broken write fails every build. Use the 3.82-safe mechanism (§0.3, §1). Keep the
   standalone test-harness path working (§1 caveat b).
3. **#2 backends/ rename** LAST — and OPTIONAL. Re-scoped to rename-in-place by
   convention (§2), not a dir split. Do it once #1 has already touched each backend's
   input model (one pass of name-fixing, not two), OR skip it and just add the README
   classification note. It's the lowest-value of the three; never do the dir-split
   variant unless you accept rewiring all 11 `${HERE}` sources.

Rationale for this order: risk-ascending for the structural pair (#3 < #1); #2 is
cosmetic and optional, sequenced last. If time-boxed, **#3 alone is a complete,
valuable, low-risk win** (it removes the last "edit the engine to add a provider"
burden) — and #2 can be reduced to a one-line README note at no risk.

---

## 6. Verification (after each phase — must be behavior-identical)

- `make -C projects/gameboy-v3 print-config` for BOTH stacks (full-custom and
  `KERNEL=mainline BOOTLOADER=uboot LIBC=musl PACKAGES=busybox`) — resolves, values
  unchanged.
- **golden 6/6** (`kernel/test/golden.sh`) and **`libc/test/dynamic.sh`** — both use
  standalone backend invocation, so they catch #1's standalone-caller caveat and #2's
  path moves.
- A real **`make image`** for full-custom AND all-OSS (both must still compose the
  bundle). Byte-compare the resulting kernel `.config`/DTB/initramfs against a
  pre-change build where feasible — the whole point is zero output change.
- **#1-specific:** `cat build/forge.conf` shows the resolved selection; build with a
  space-bearing `PACKAGES="coreutils busybox"` and confirm `ROOTFS_TAG`/`CFG` come out
  `musl-coreutils+busybox` (NOT `musl-"coreutils busybox"` — the quoting hazard, §1);
  the rootfs assembler still iterates both packages; `grep -rn BACKEND_ENV forge/`
  returns nothing.
- **#2-specific (only if the rename was done):** every `source "${HERE}/<name>"`
  resolves (a broken rename fails loudly at `source`, file-not-found); `host_provision`
  still dispatches (`host-styles/${PKG_TYPE}.sh`); golden + a real host_provision pass.
  If #2 was skipped, just confirm the README classification note landed.
- **#3-specific:** a bad `LIBC=bogus` fails via recipe-existence (not the deleted
  enum); the custom gv3libc substrate builds; the musl path no-ops. `providers/libc/`
  recipes parse.
- **The gold standard (do at the end):** a clean rebuild + flash-and-boot BOTH stacks
  on the T113 rig — the same validation done for the reorg (custom → `gv3$`, all-OSS →
  `~ #`). Only this proves the plumbing changes didn't perturb the hardware path.

---

## 7. Note for the implementer

The engine's *structure* is done and silicon-proven (both stacks boot from clean
builds). This doc is strictly about making the *plumbing* legible + fully generic —
the last two hardcoded-name branches (#3) and the string-threading (#1), plus a name
that no longer fits (#2). Nothing here should change a single build artifact; if a
byte differs, something regressed. When in doubt, prefer the smaller change: #3 alone
already removes the last "edit the engine to add a provider" burden, which is the
principle the whole forge has been converging on.
