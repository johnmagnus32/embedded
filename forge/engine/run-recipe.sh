#!/usr/bin/env bash
# run-recipe.sh — the ONE node runner (orchestrator). Builds ANY recipe with no branch on identity:
# prepare the env, source the recipe (binds tasks via `inherit <class>` + inline do_* overrides),
# gate on the content cache, then do_fetch -> do_build -> do_install BY NAME. In: the node NAME ($1)
# + the seed PRODUCT_DIR (env). forge-env.sh resolves everything else from the product's local.conf —
# no forge.conf, no shared state; Make just says which node to build.
set -euo pipefail

# forge-env.sh (sourced): the config + resolution brain — recipe_get, log/die, forge_load_env (the
# whole build environment from local.conf + board.conf), forge_byname, FORGE_ENGINE/FORGE_META. The
# FETCH mechanism is NOT here — it's the default do_fetch in classes-global/base.sh (inherited first).
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/forge-env.sh"
LAYER="${1:?run-recipe.sh: need a node name}"; export LAYER

# ---- node lifecycle (in the order main runs it) ----------------------------------------------

# setup_build_env — build the node's environment before the recipe is sourced: the resolved env
# (from local.conf + board.conf via forge-env), the provisioned tools on PATH, and the scaffolding (which
# DEFINES inherit()). main then does `inherit base` + `source ${RECIPE}` to bind behaviour.
setup_build_env() {
  load_build_env
  setup_host_env        # build the HOSTTOOLS farm + sanity-check the host — while PATH is still the host's
  setup_path            # THEN scrub PATH to {forge's built tools}:{farm}
  set_recipe_env
}

# load_build_env — the resolved build environment: forge-env.sh derives it from local.conf + board.conf
# (layout, providers, toolchain scalars, tags — everything the old forge.conf carried), then find this
# node's recipe by name. A node name matching no recipe fails clearly here (not on a later `source ""`).
load_build_env() {
  forge_load_env
  RECIPE="$(forge_byname "${LAYER}" || true)"; export RECIPE
  [ -n "${RECIPE}" ] && [ -f "${RECIPE}" ] \
    || die "no recipe for node '${LAYER}' — no recipes-*/ or packages/ dir by that name (typo in PACKAGES or a selection?)"
}

# --- host build environment (Yocto HOSTTOOLS / ASSUME_PROVIDED / sanity) ----------------------
# These run in setup_build_env BEFORE setup_path scrubs PATH, so they see the real host PATH. Both are
# idempotent + content-stamped: the barrier makes host-make the first node, so the farm/sanity are
# built there once and every later node early-returns (no per-node cost, no -j race).

# build_hosttools_farm — realise the HOSTTOOLS allowlist as a symlink farm; setup_path then scrubs PATH
# to ONLY this farm + forge's built-tool dirs, so a recipe reaches a host binary IFF it is listed.
# HOSTTOOLS are fatal (missing -> abort with an apt hint); HOSTTOOLS_NONFATAL + ASSUME_PROVIDED (forge
# rebuilds those) are linked only when present. Stamped on the list contents (rebuild on any change).
build_hosttools_farm() {
  local key keyfile t p missing=""
  # `type -P` (NOT `command -v`): a tool that is also a shell builtin (true/false/pwd/printf) makes
  # `command -v` print the bare word, not a path -> `ln -s true true` = a self-loop. `type -P` forces a
  # filesystem-executable lookup. The `farmv2` tag busts the stamp when THIS logic changes (list is same).
  key="$(printf 'farmv2|%s|%s|%s' "${HOSTTOOLS}" "${HOSTTOOLS_NONFATAL}" "${ASSUME_PROVIDED}" | sha256sum | cut -d' ' -f1)"
  keyfile="${HOSTTOOLS_FARM}/.key"
  [ "$(cat "${keyfile}" 2>/dev/null || true)" = "${key}" ] && return 0
  rm -rf "${HOSTTOOLS_FARM}"; mkdir -p "${HOSTTOOLS_FARM}"
  for t in ${HOSTTOOLS}; do
    if p="$(type -P "$t" 2>/dev/null)"; then ln -s "$p" "${HOSTTOOLS_FARM}/$t"; else missing="${missing} $t"; fi
  done
  [ -z "${missing}" ] || die "host is missing required tool(s):${missing}
  (Debian/Ubuntu: apt install build-essential binutils git curl xz-utils)"
  for t in ${HOSTTOOLS_NONFATAL} ${ASSUME_PROVIDED}; do
    if p="$(type -P "$t" 2>/dev/null)"; then ln -s "$p" "${HOSTTOOLS_FARM}/$t"; fi
  done
  printf '%s' "${key}" > "${keyfile}"
}

# sanity_check — enforce SANITY_REQUIRED (<tool>:<minver>) version floors once (Yocto's sanity). Generic
# version read: first dotted number on the tool's --version first line, compared via `sort -V`. make's
# floor is the BOOTSTRAP one (>=3.81 to build make-native); the >=4.0 use-vs-build call is the recipe's.
sanity_check() {
  local stamp key spec tool min have
  key="$(printf '%s' "${SANITY_REQUIRED}" | sha256sum | cut -d' ' -f1)"
  stamp="${BUILD_DIR}/.forge/.sanity-ok"
  [ "$(cat "${stamp}" 2>/dev/null || true)" = "${key}" ] && return 0
  for spec in ${SANITY_REQUIRED}; do
    tool="${spec%%:*}"; min="${spec#*:}"
    command -v "${tool}" >/dev/null 2>&1 || die "sanity: required host tool '${tool}' not found (need >= ${min})"
    have="$("${tool}" --version 2>&1 | head -n1 | grep -oE '[0-9]+(\.[0-9]+)+' | head -n1 || true)"
    [ -n "${have}" ] || { log "sanity: could not read ${tool} version; assuming OK"; continue; }
    [ "$(printf '%s\n%s\n' "${min}" "${have}" | sort -V | head -n1)" = "${min}" ] \
      || die "sanity: ${tool} ${have} is older than the required ${min}"
  done
  mkdir -p "${BUILD_DIR}/.forge"; printf '%s' "${key}" > "${stamp}"
}

setup_host_env() { build_hosttools_farm; sanity_check; }

# setup_path — SCRUB PATH to forge's provisioned tool dirs + the HOSTTOOLS farm, and NOTHING else. Built
# dirs first so a forge-built tool (make-native, the cross gcc) always shadows any host copy. After this
# a recipe can reach a host binary IFF it is in the farm (the allowlist). A recipe may still prepend its
# own provisioned dir afterward (e.g. U-Boot's binman venv) — that stacks on top of the scrubbed PATH.
setup_path() {
  # LIBC_TC_DIR/bin (the stage-1 toolchain) is on PATH too, AFTER the stage-2 dirs: the libc node builds
  # before stage-2 exists and its Makefile invokes the cross ar/ranlib by BARE name, so they must resolve
  # from stage-1 — but once stage-2 is built it comes first and wins. (For TOOLCHAIN=custom, LIBC_TC_DIR
  # == TOOLCHAIN_DIR, so the dedup below collapses it.)
  local d p=""
  for d in "${HOSTMAKE_DIR}/bin" "${TOOLCHAIN_DIR}/bin" "${LIBC_TC_DIR:+${LIBC_TC_DIR}/bin}" "${HOSTTOOLS_FARM}"; do
    [ -n "$d" ] && [ -d "$d" ] || continue
    case ":$p:" in *":$d:"*) ;; *) p="${p:+$p:}$d" ;; esac
  done
  PATH="$p"; export PATH
}

# set_recipe_env — per-node env + class scaffolding: define inherit() (main applies `inherit base`,
# then sources the recipe, so the recipe's own inherit/do_* override the base defaults, last-wins).
set_recipe_env() {
  export PKG_LINK="${PKG_LINK:-${LINKAGE:-static}}"

  # RECIPE + LAYER are Make-injected + engine.mk-validated; set -u aborts below if either is unset,
  # and `source "${RECIPE}"` (main) aborts if the file is missing.
  RECIPE_DIR="$(cd "$(dirname "${RECIPE}")" && pwd)"; export RECIPE_DIR   # recipe reads sibling data (cc-profile.sh, *.config)

  export STAGE="${BUILD_DIR}/rootfs/stage"                 # assembled rootfs (the rootfs step merges pkgstages here)
  export PKG_DEST="${BUILD_DIR}/rootfs/pkgstage/${LAYER}"  # this package's own install dir (Buildroot per-package model)
  export LIBC
  export REPO_ROOT ROOTFS_TARGET
  while IFS='=' read -r _v _; do export "${_v?}"; done < <(set | grep '^ROOTFS_ARCH_FLAGS' || true)   # cc-profile reads arch tuning

  RECIPE_SCRATCH="${BUILD_DIR}/scratch/${LAYER}"   # per-node scratch (tarball extract / compile obj)
  export PROVIDER_RECIPE="${RECIPE}"           # kconfig providers read their own facts via recipe_get

  # inherit() binds a class — DEFINED here (chicken/egg: it's what sources classes) and records each into
  # _INHERITED_CLASSES so compute_taskhash hashes class bodies. main applies `inherit base` then sources
  # the recipe once this scaffolding exists. Search order = classes-global/ then classes-recipe/ (Yocto's
  # classes*/ search), both under FORGE_META (forge/meta/, the layer), set by forge-env when it loaded.
  _INHERITED_CLASSES=""
  inherit() {
    local _c
    for _c in "${FORGE_META}/classes-global/$1.sh" "${FORGE_META}/classes-recipe/$1.sh"; do
      [ -f "${_c}" ] || continue
      _INHERITED_CLASSES="${_INHERITED_CLASSES} ${_c}"
      source "${_c}"; return 0
    done
    die "inherit: class '$1' not found in classes-global/ or classes-recipe/"
  }
  # require() binds a shared INCLUDE by path (Yocto's `require`): recipe DATA two recipes share — e.g.
  # SRC_URI/checksum pins — lives in a .inc, NOT a class (which is shared LOGIC/tasks). Recorded into
  # _REQUIRED_INCS so compute_taskhash hashes it too, so a pin bump ripples like a recipe edit.
  _REQUIRED_INCS=""
  require() { _REQUIRED_INCS="${_REQUIRED_INCS} $1"; source "$1"; }
}

# skip_if_built — resolve what this node outputs + its content taskhash (both kept for mark_built),
# then exit 0 if that taskhash is already built: the stamp (last-built taskhash) matches AND the
# declared output is present (the present-check backstops a wiped artifact). A recipe with no output
# never matches (always rebuilds). The PKG_HOST_SKIP_IF arm skips when the host already satisfies it
# (e.g. make>=4). Editing the recipe/class/source/board or bumping a dep changes the taskhash => miss.
skip_if_built() {
  resolve_output
  compute_taskhash
  if [ -n "${_output}" ] && [ "${FORCE:-0}" != 1 ] && [ -f "${_stamp}" ] \
       && [ "$(cat "${_stamp}" 2>/dev/null)" = "${_taskhash}" ] && [ -e "${_output}" ]; then
    log "cached — up to date (taskhash ${_taskhash:0:12})"; exit 0
  fi
  [ -n "${_output}" ] && [ -f "${_stamp}" ] && [ ! -e "${_output}" ] \
    && log "stamp present but artifact missing (${_output}) — rebuilding"
  if [ -n "${PKG_HOST_SKIP_IF:-}" ] && eval "${PKG_HOST_SKIP_IF}" >/dev/null 2>&1; then
    log "satisfied by the host already (PKG_HOST_SKIP_IF) — skipping build"
    mkdir -p "${FORGE_STAMPS}"; printf '%s' "${_taskhash}" > "${_stamp}"; exit 0
  fi
}

# resolve_output — set _output: the durable artifact this recipe declares (host: PKG_HOST_BIN |
# PKG_HOST_DEST[/bin/PKG_HOST_VERIFY_BIN]; else PKG_ARTIFACT out:/src:/stage:/libcstage:). Cacheable
# examples: host tools, kernel/U-Boot, packages (stage:), libc (libcstage:). Empty => no standalone
# artifact (the rootfs/image compose steps, the prebuilt-musl marker) => never cacheable, always
# rebuilds. Whether a recipe is cacheable is a PROPERTY it declares, not a type the engine branches on.
resolve_output() {
  _output=""
  if   [ -n "${PKG_HOST_BIN:-}" ];        then _output="${PKG_HOST_BIN}"
  elif [ -n "${PKG_HOST_VERIFY_BIN:-}" ]; then _output="${PKG_HOST_DEST:-}/bin/${PKG_HOST_VERIFY_BIN}"
  elif [ -n "${PKG_HOST_DEST:-}" ];       then _output="${PKG_HOST_DEST}"
  elif [ -n "${PKG_ARTIFACT:-}" ];        then _output="$(_artifact_path "${RECIPE}" PKG_ARTIFACT)"
  fi
}

# _recipe_src_dir <recipe> — the dir a recipe's `src:` artifacts are relative to: local ->
# $REPO_ROOT/$PKG_SOURCE, git -> $BUILD_DIR/$PKG_GIT_CHECKOUT, else "". Reads the recipe's own fetch
# facts, so it's role-agnostic and works for the current node OR a sibling recipe.
_recipe_src_dir() {
  local recipe="$1"
  case "$(recipe_get "${recipe}" PKG_FETCH local)" in
    local) printf '%s' "${REPO_ROOT}/$(recipe_get "${recipe}" PKG_SOURCE)" ;;
    git)   printf '%s' "${BUILD_DIR}/$(recipe_get "${recipe}" PKG_GIT_CHECKOUT)" ;;
    *)     printf '' ;;
  esac
}

# _artifact_path <recipe> <key> — resolve a recipe's <key> artifact spec (base:path) to an absolute
# path. base = out (build/output) | src (recipe source dir, via _recipe_src_dir) | stage (the node's
# own pkgstage) | libcstage (selected libc stage) | absent (verbatim); empty spec -> "". ONE resolver,
# reading facts via recipe_get, so both resolve_output (this node) and the image composer (sibling
# recipes) share it. stage/libcstage only apply to the current node (its own PKG_DEST/LIBC_STAGE_DIR).
_artifact_path() {
  local recipe="$1" key="$2" spec base path
  spec="$(recipe_get "${recipe}" "${key}")"
  [ -n "${spec}" ] || { printf ''; return 0; }
  base="${spec%%:*}"; path="${spec#*:}"
  case "${base}" in
    out)       printf '%s' "${OUTPUT_DIR}/${path}" ;;
    src)       printf '%s' "$(_recipe_src_dir "${recipe}")/${path}" ;;
    stage)     printf '%s' "${PKG_DEST}${path:+/${path}}" ;;
    libcstage) printf '%s' "${LIBC_STAGE_DIR}${path:+/${path}}" ;;
    *)         printf '%s' "${spec}" ;;
  esac
}

# compute_taskhash — set _taskhash + _stamp (Yocto's basehash + taskhash). basehash hashes everything
# the output depends on (recipe, classes, engine, siblings, source, link mode, target toolchain/board);
# taskhash then folds each dep's recorded taskhash so a bump ripples. Host cc/dtc/python are trusted,
# not hashed (Buildroot-style; binman-venv's embedded python is guarded by its output check instead).
compute_taskhash() {
  local base dep deps _linksens
  base="$(
    {
      printf '=== recipe ===\n';  cat "${RECIPE}"
      printf '=== classes ===\n'
      for dep in ${_INHERITED_CLASSES}; do [ -f "${dep}" ] && { printf '# %s\n' "${dep##*/}"; cat "${dep}"; }; done
      printf '=== includes ===\n'
      for dep in ${_REQUIRED_INCS}; do [ -f "${dep}" ] && { printf '# %s\n' "${dep##*/}"; cat "${dep}"; }; done
      # The engine drives every build, so an engine code change must invalidate every node — hash both
      # run-recipe.sh + forge-env.sh, comment-stripped so a pure comment/whitespace edit doesn't rebuild.
      printf '=== engine ===\n'
      for _e in run-recipe.sh forge-env.sh; do grep -vE '^[[:space:]]*#|^[[:space:]]*$' "${FORGE_ENGINE}/${_e}" 2>/dev/null; done
      printf '=== siblings ===\n'
      ( cd "${RECIPE_DIR}" && find . -type f ! -name recipe.sh -exec sha256sum {} + 2>/dev/null | sort )
      printf '=== source ===\n';  _hash_source
      # Fold link mode only where the output differs by it: libc (PKG_LINKSENS) + its linkers (PKG_DEPENDS).
      _linksens=0
      [ "${PKG_LINKSENS:-0}" = 1 ] && _linksens=1
      case " ${PKG_DEPENDS:-} " in *" virtual/libc "*) _linksens=1 ;; esac
      [ "${_linksens}" = 1 ] && printf 'link:%s\n' "${PKG_LINK:-static}"
      # Cross-toolchain + arch SELECTION (CROSS_COMPILE/TC_ARCH/ARCH — forge.conf config no dep edge
      # carries) + board dir: inputs to TARGET builds only. Host classes opt OUT via PKG_TARGET_INDEPENDENT.
      # An opt-out (small closed set) fails safe: forgetting it over-invalidates, never reuses stale.
      if [ "${PKG_TARGET_INDEPENDENT:-0}" != 1 ]; then
        printf '=== toolchain/arch ===\n'
        printf '%s|%s|%s\n' "${CROSS_COMPILE:-}" "${ARCH:-}" "${TC_ARCH:-}"
        if [ -d "${BOARD_DIR:-/nonexistent}" ]; then
          printf '=== board ===\n'
          ( cd "${BOARD_DIR}" && find . -type f -exec sha256sum {} + 2>/dev/null | sort )
        fi
      fi
    } | sha256sum | cut -d' ' -f1
  )"

  deps="${PKG_HOST_DEPENDS:-} ${PKG_DEPENDS:-}"
  # A TARGET recipe that links libc is built by the rootfs toolchain — the implied compiler edge
  # (matches engine.mk _ndeps), so a toolchain change folds into its taskhash even though it declares
  # no TC. Gated on class=target: a cross/native host tool (e.g. toolchain-gcc, which depends on libc
  # for its --with-sysroot) is NOT built by virtual/cross-cc, and injecting it would self-cycle.
  if [ "${PKG_CLASS:-target}" = target ]; then
    case " ${PKG_DEPENDS:-} " in *" virtual/libc "*) deps="${deps} virtual/cross-cc" ;; esac
  fi
  # Resolve every virtual/<x> dep to its provider recipe NAME (forge_resolve — the SAME resolver the
  # graph edges use), so a provider change folds into this node's taskhash.
  _rdeps=""; for dep in ${deps}; do _rdeps="${_rdeps} $(forge_resolve "${dep}")"; done
  deps="${_rdeps}"
  _taskhash="$(
    {
      printf '%s\n' "${base}"
      for dep in ${deps}; do
        [ -f "${FORGE_SIGS}/${dep}.taskhash" ] && printf 'dep:%s=%s\n' "${dep}" "$(cat "${FORGE_SIGS}/${dep}.taskhash")"
      done
    } | sha256sum | cut -d' ' -f1
  )"

  mkdir -p "${FORGE_SIGS}"
  printf '%s' "${_taskhash}" > "${FORGE_SIGS}/${LAYER}.taskhash"   # for dependents (written every run)
  _stamp="${FORGE_STAMPS}/${LAYER}"
}

# _hash_source — stable digest of THIS recipe's source, by PKG_FETCH: local = git ls-files content
# (git is the source-vs-generated truth; excludes build/); git = pinned tag; tarball/prebuilt = ver+sha.
_hash_source() {
  case "${PKG_FETCH:-local}" in
    local)
      if git -C "${REPO_ROOT}" rev-parse --git-dir >/dev/null 2>&1; then
        ( cd "${REPO_ROOT}" \
          && { git ls-files -z -- "${PKG_SOURCE}"; git ls-files -z --others --exclude-standard -- "${PKG_SOURCE}"; } \
          | sort -z | xargs -0 -r sha256sum 2>/dev/null )
      else   # not a git checkout: fall back to find, skipping the conventional build/ output dir
        ( cd "${REPO_ROOT}" && find "${PKG_SOURCE}" -type f -not -path '*/build/*' -exec sha256sum {} + 2>/dev/null | sort )
      fi ;;
    git)      printf 'git:%s@%s\n' "${PKG_GIT_URL:-}" "${PKG_VERSION:-}" ;;
    tarball)  printf 'tar:%s#%s\n'  "${PKG_VERSION:-}" "${PKG_SHA256:-}" ;;
    prebuilt) printf 'pre:%s#%s\n'  "${PKG_VERSION:-}" "${PKG_SHA256:-}" ;;
    none|*)   : ;;
  esac
}

# run_tasks — the uniform sequence (no branch on kind). Pure orchestration: a clean scratch dir, then
# the tasks in Yocto's order. do_fetch downloads; do_unpack extracts + sets PKG_SRC_DIR; do_patch edits
# the unpacked source; do_build compiles; do_install stages. Each task reads the generic node env
# (RECIPE_SCRATCH + PKG_SRC_DIR from do_fetch/do_unpack) and derives/defaults its own class-specific bits.
# base.sh supplies working defaults for fetch/unpack (git|local|tarball) + no-op patch, so a recipe
# binds only what differs (usually do_build/do_install; a source-patching or multi-tarball recipe also
# overrides do_patch/do_unpack).
run_tasks() {
  rm -rf "${RECIPE_SCRATCH}"; mkdir -p "${RECIPE_SCRATCH}"
  do_fetch
  do_unpack
  do_patch
  do_build
  do_install
}

# mark_built — record the taskhash just built (so the next run with an unchanged taskhash skips).
# Only if the recipe declares an output.
mark_built() {
  if [ -n "${_output}" ]; then mkdir -p "${FORGE_STAMPS}"; printf '%s' "${_taskhash}" > "${_stamp}"; fi
}

main() {
  setup_build_env       # resolved env (forge-env from local.conf) + PATH + scaffolding (RECIPE + inherit())
  inherit base          # default tasks (do_fetch); the recipe's inherit/do_* override, last-wins
  # shellcheck disable=SC1090
  source "${RECIPE}"    # recipe facts as vars + inherit(s) + inline do_* overrides
  skip_if_built         # resolve output + content taskhash; exit 0 if already built
  run_tasks             # do_fetch -> do_build -> do_install
  mark_built            # record the taskhash so the next run skips
}
main
