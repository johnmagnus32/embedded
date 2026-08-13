#!/usr/bin/env bash
# run-recipe.sh — the ONE node runner (orchestrator). Builds ANY recipe with no branch on identity:
# prepare the env, source the recipe (binds tasks via `inherit <class>` + inline do_* overrides),
# gate on the content cache, then do_fetch -> do_build -> do_install BY NAME. In: RECIPE, LAYER, and
# the bootstrap seed PRODUCT_DIR; everything else (BOARD, layout, pins) from forge.conf + board.conf.
# Make is the only entry point (forge.conf must already exist).
set -euo pipefail

# Self-location (BASH_SOURCE is this file); inherit() uses it to find classes/.
FORGE_CORE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; export FORGE_CORE

# ---- shared helpers (in scope for every recipe / class this runner sources) ------------------
# The FETCH mechanism is NOT here — it's the default do_fetch in classes/base.sh (inherited first),
# with the shared fetch primitives host classes also call.

log()  { printf '\033[1;34m[%s]\033[0m %s\n' "${LAYER:-recipe}" "$*"; }
die()  { printf '\033[1;31m[%s] ERROR:\033[0m %s\n' "${LAYER:-recipe}" "$*" >&2; exit 1; }

# recipe_get <recipe> <KEY> [default] -> bare value of the last KEY=, ${VAR}-expanded. Normalization
# MUST match Make's reader (resolve.mk _field): strip inline `# comment`, trim, collapse runs, strip
# one quote layer, then eval.
recipe_get() {
  local file="$1" key="$2" def="${3:-}" raw
  raw="$(sed -n "s/^${key}=//p" "${file}" 2>/dev/null | tail -n1 | sed 's/[[:space:]]*#.*$//' | tr '\t' ' ' | tr -s ' ')"
  read -r raw <<<"${raw}"
  case "${raw}" in
    '"'*'"') raw="${raw#\"}"; raw="${raw%\"}" ;;
    "'"*"'") raw="${raw#\'}"; raw="${raw%\'}" ;;
  esac
  [ -n "${raw}" ] || { printf '%s' "${def}"; return 0; }
  eval "printf '%s' \"${raw}\""
}

# _prepend_path <dir> — put <dir> at the front of PATH if present and not already there.
_prepend_path() {
  [ -d "$1" ] || return 0
  case ":${PATH}:" in
    *":$1:"*) : ;;
    *) PATH="$1:${PATH}"; export PATH ;;
  esac
}

# ---- node lifecycle (in the order main runs it) ----------------------------------------------

# setup_build_env — build the node's environment before the recipe is sourced: the resolved env
# (forge.conf + board.conf), the provisioned tools on PATH, and the per-node scaffolding (which
# DEFINES inherit()). main then does `inherit base` + `source ${RECIPE}` to bind behaviour.
setup_build_env() {
  load_build_env
  setup_path
  set_node_env
}

# load_build_env — resolve the node's build environment as a pipeline: validate the one bootstrap
# input, source forge.conf (layout/pins/selection from resolve.mk), then the board BSP, then derive
# the cache + libc-staging paths. forge.conf FIRST: it defines BOARD_DIR, which the board step needs
# (order is fixed by data flow). Bad/unset inputs are caught by set -euo pipefail + resolve.mk's
# parse-time checks; only the board-dir guard (a silent case set -e wouldn't catch) is asserted here.
load_build_env() {
  # 1. bootstrap seed -> build tree (set -u aborts if Make didn't inject PRODUCT_DIR)
  BUILD_DIR="${PRODUCT_DIR}/build"

  # 2. resolved selection + layout + tool pins (resolve.mk -> forge.conf; set -e aborts if missing)
  # shellcheck disable=SC1091
  source "${BUILD_DIR}/forge.conf"
  export FORGE_ROOT CROSS_COMPILE ARCH    # sub-makes (kbuild) read CROSS_COMPILE/ARCH from the env

  # 3. board BSP (forge.conf just set BOARD_DIR)
  [ -d "${BOARD_DIR}" ] || die "board dir '${BOARD_DIR}' not found (BOARD_NAME=${BOARD_NAME})"
  [ -f "${BOARD_DIR}/board.conf" ] && source "${BOARD_DIR}/board.conf"

  # 4. paths derived from the resolved env (the `:-`/`:=` defaults also cover a standalone run)
  FORGE_STAMPS="${FORGE_STAMPS:-${BUILD_DIR}/.forge/stamps}"   # per-<LAYER> last-built taskhash
  FORGE_SIGS="${FORGE_SIGS:-${BUILD_DIR}/.forge/sigs}"         # .taskhash sig a dependent reads (ripple)
  # libc staging is keyed on link mode (unknown to resolve.mk) — computed here, where the producer
  # (libc class) + consumers (cc-profile, rootfs) agree.
  LIBC_BUILD_DIR="${BUILD_DIR}/libc"
  _libc_link="${LINKAGE:-${PKG_LINK:-static}}"
  : "${LIBC_STAGE_DIR:=${LIBC_BUILD_DIR}/stage-${LIBC:-custom}-${_libc_link}}"
  : "${STAGE_INC:=${LIBC_BUILD_DIR}/include}"
  export LIBC_STAGE_DIR STAGE_INC
}

# setup_path — forge's universal provisioned tools onto PATH: the locally-built make (if that dir
# exists) + both cross toolchains. Provider-specific tools (U-Boot's binman venv) are prepended by the recipe.
setup_path() {
  _prepend_path "${HOSTMAKE_DIR}/bin"
  _prepend_path "${TOOLCHAIN_DIR}/bin"
  _prepend_path "${ROOTFS_TOOLCHAIN_DIR}/bin"
}

# set_node_env — per-node env + class scaffolding: define inherit() (main applies `inherit base`,
# then sources the recipe, so the recipe's own inherit/do_* override the base defaults, last-wins).
set_node_env() {
  export PKG_LINK="${PKG_LINK:-${LINKAGE:-static}}"

  # RECIPE + LAYER are Make-injected + resolve.mk-validated; set -u aborts below if either is unset,
  # and `source "${RECIPE}"` (main) aborts if the file is missing.
  RECIPE_DIR="$(cd "$(dirname "${RECIPE}")" && pwd)"; export RECIPE_DIR   # recipe reads sibling data (cc-profile.sh, *.config)

  export STAGE="${BUILD_DIR}/rootfs/stage"                 # assembled rootfs (the rootfs step merges pkgstages here)
  export PKG_DEST="${BUILD_DIR}/rootfs/pkgstage/${LAYER}"  # this package's own install dir (Buildroot per-package model)
  [ -n "${LIBC_SRC:-}" ] && export LIBC LIBC_SRC
  export REPO_ROOT ROOTFS_CROSS_COMPILE ROOTFS_TARGET
  while IFS='=' read -r _v _; do export "${_v?}"; done < <(set | grep '^ROOTFS_ARCH_FLAGS' || true)   # cc-profile reads arch tuning

  NODE_SCRATCH="${BUILD_DIR}/nodes/${LAYER}"   # per-node scratch (tarball extract / compile obj)
  export PROVIDER_RECIPE="${RECIPE}"           # kconfig providers read their own facts via recipe_get

  # inherit() binds a class (sources classes/<name>.sh) — DEFINED here (chicken/egg: it's what sources
  # classes) and records each into _INHERITED_CLASSES so compute_taskhash hashes class bodies. main
  # applies `inherit base` then sources the recipe once this scaffolding exists.
  _INHERITED_CLASSES=""
  inherit() { _INHERITED_CLASSES="${_INHERITED_CLASSES} ${FORGE_CORE}/classes/$1.sh"; source "${FORGE_CORE}/classes/$1.sh"; }
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
# examples: host tools, kernel/U-Boot, packages (stage:), gv3libc (libcstage:). Empty => no standalone
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
      # The engine drives every build, so a run-recipe.sh code change must invalidate every node.
      # Hashed comment-stripped so a pure comment/whitespace edit doesn't rebuild the world.
      printf '=== engine ===\n'
      grep -vE '^[[:space:]]*#|^[[:space:]]*$' "${FORGE_CORE}/run-recipe.sh" 2>/dev/null
      printf '=== siblings ===\n'
      ( cd "${RECIPE_DIR}" && find . -type f ! -name recipe.sh -exec sha256sum {} + 2>/dev/null | sort )
      printf '=== source ===\n';  _hash_source
      # Fold link mode only where the output differs by it: libc (PKG_LINKSENS) + its linkers (PKG_DEPENDS).
      _linksens=0
      [ "${PKG_LINKSENS:-0}" = 1 ] && _linksens=1
      case " ${PKG_DEPENDS:-} " in *" libc "*) _linksens=1 ;; esac
      [ "${_linksens}" = 1 ] && printf 'link:%s\n' "${PKG_LINK:-static}"
      # Cross-toolchain + arch SELECTION (CROSS_COMPILE/TC_ARCH/ARCH — forge.conf config no dep edge
      # carries) + board dir: inputs to TARGET builds only. Host classes opt OUT via PKG_TARGET_INDEPENDENT.
      # An opt-out (small closed set) fails safe: forgetting it over-invalidates, never reuses stale.
      if [ "${PKG_TARGET_INDEPENDENT:-0}" != 1 ]; then
        printf '=== toolchain/arch ===\n'
        printf '%s|%s|%s|%s\n' "${CROSS_COMPILE:-}" "${ROOTFS_CROSS_COMPILE:-}" "${ARCH:-}" "${TC_ARCH:-}"
        if [ -d "${BOARD_DIR:-/nonexistent}" ]; then
          printf '=== board ===\n'
          ( cd "${BOARD_DIR}" && find . -type f -exec sha256sum {} + 2>/dev/null | sort )
        fi
      fi
    } | sha256sum | cut -d' ' -f1
  )"

  deps="${PKG_HOST_DEPENDS:-} ${PKG_DEPENDS:-}"
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
# the three tasks. Each task reads the generic node env (NODE_SCRATCH + PKG_SRC_DIR from do_fetch) and
# derives/defaults its own class-specific bits.
run_tasks() {
  rm -rf "${NODE_SCRATCH}"; mkdir -p "${NODE_SCRATCH}"
  do_fetch
  do_build
  do_install
}

# mark_built — record the taskhash just built (so the next run with an unchanged taskhash skips).
# Only if the recipe declares an output.
mark_built() {
  if [ -n "${_output}" ]; then mkdir -p "${FORGE_STAMPS}"; printf '%s' "${_taskhash}" > "${_stamp}"; fi
}

main() {
  setup_build_env       # resolved env + PATH + per-node scaffolding (defines inherit())
  inherit base          # default tasks (do_fetch); the recipe's inherit/do_* override, last-wins
  # shellcheck disable=SC1090
  source "${RECIPE}"    # recipe facts as vars + inherit(s) + inline do_* overrides
  skip_if_built         # resolve output + content taskhash; exit 0 if already built
  run_tasks             # do_fetch -> do_build -> do_install
  mark_built            # record the taskhash so the next run skips
}
main
