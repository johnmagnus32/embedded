#!/usr/bin/env bash
# engine.sh — the OS build engine (bash half; engine.mk is the Make graph-walker). Two subcommands,
# each taking a recipe NAME + the seed PRODUCT_DIR (env); all config/resolution reads the product's
# local.conf, so Make never sees a config value:
#   engine.sh resolve-dependencies <recipe>  -> its resolved prerequisite recipe names (the Make prereqs)
#   engine.sh execute-recipe        <recipe>  -> build it: resolve the env, source the recipe, cache-gate,
#                                                then do_fetch -> do_build -> do_install by name.
set -euo pipefail

log() { printf '\033[1;34m[%s]\033[0m %s\n' "${LAYER:-os}" "$*"; }
die() { printf '\033[1;31m[%s] ERROR:\033[0m %s\n' "${LAYER:-os}" "$*" >&2; exit 1; }

# recipe_get <recipe> <KEY> [default] -> bare value of the last KEY=, ${VAR}-expanded (env in scope).
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

# locate — the engine's own dirs, from this file's path.
locate() {
  OS_ENGINE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  OS_META="$(cd "${OS_ENGINE}/../meta" && pwd)"
  REPO_ROOT="$(cd "${OS_ENGINE}/../.." && pwd)"
  export OS_ENGINE OS_META REPO_ROOT
}

# load_config — the product SELECTION: source local.conf (knobs + OS_VIRTUALS + os_preferred_provider).
load_config() {
  locate
  : "${PRODUCT_DIR:?os: PRODUCT_DIR unset (Make injects it)}"
  # shellcheck source=/dev/null
  source "${PRODUCT_DIR}/local.conf"
}

# byname <name> -> its recipe.sh path (product recipes-*/ + packages/ shadow os/meta).
byname() {
  local n="$1" r
  for r in "${PRODUCT_DIR}"/recipes-*/"${n}"/recipe.sh "${PRODUCT_DIR}"/packages/"${n}"/recipe.sh "${OS_META}"/recipes-*/"${n}"/recipe.sh; do
    [ -f "${r}" ] && { printf '%s' "${r}"; return 0; }
  done
  return 1
}

# resolve <token> -> a virtual/<x> becomes its verified preferred provider NAME; else passes through.
resolve() {
  case "$1" in
    virtual/*)
      local name path
      name="$(os_preferred_provider "$1")" || die "no preferred provider for $1 (local.conf os_preferred_provider)"
      path="$(byname "${name}")" || die "preferred provider $1=${name}: no such recipe"
      case " $(recipe_get "${path}" PKG_PROVIDES) " in
        *" $1 "*) : ;;
        *) die "preferred provider $1=${name}: recipe does not provide $1" ;;
      esac
      printf '%s' "${name}" ;;
    *) printf '%s' "$1" ;;
  esac
}

# node_deps <recipe-path> -> its resolved prerequisite recipe names: PKG_DEPENDS + PKG_HOST_DEPENDS +
# PKG_HOST_DEPENDS_<MEDIA> (recipe_get expands ${PACKAGES}) + the implied compiler edge for a target
# that links libc, each virtual/<x> resolved. No `make` barrier — that's a Make-ordering edge, added by
# resolve-dependencies. Used by BOTH resolve-dependencies (graph edges) + compute_taskhash (hash fold).
node_deps() {
  local path="$1" raw tok out=""
  raw="$(recipe_get "${path}" PKG_DEPENDS) $(recipe_get "${path}" PKG_HOST_DEPENDS) $(recipe_get "${path}" "PKG_HOST_DEPENDS_${MEDIA}")"
  if [ "$(recipe_get "${path}" PKG_CLASS target)" = target ]; then
    case " $(recipe_get "${path}" PKG_DEPENDS) " in *" virtual/libc "*) raw="${raw} virtual/cross-cc" ;; esac
  fi
  for tok in ${raw}; do out="${out} $(resolve "${tok}")"; done
  printf '%s' "${out# }"
}

# resolve-dependencies <recipe> -> node_deps + the `make` barrier (all nodes but make itself).
resolve_dependencies() {
  load_config
  local recipe="$1" path deps
  path="$(byname "${recipe}")" || return 0
  deps="$(node_deps "${path}")"
  [ "${recipe}" = make ] && printf '%s\n' "${deps}" || printf 'make %s\n' "${deps}"
}

# load_env — the FULL build environment (from local.conf + board.conf), for execute-recipe. Sets the
# same variable names recipes/classes read.
load_env() {
  load_config
  BUILD_DIR="${PRODUCT_DIR}/build"
  BOARD_NAME="${BOARD}"; BOARD_DIR="${PRODUCT_DIR}/boards/${BOARD}"
  # shellcheck source=/dev/null
  [ -f "${BOARD_DIR}/board.conf" ] && source "${BOARD_DIR}/board.conf"
  : "${KERNEL_TARGET:?boards/${BOARD}/board.conf must set KERNEL_TARGET}"
  ROOTFS_TARGET="${ROOTFS_TARGET:-${KERNEL_TARGET}}"

  # resolved providers — PROVIDER_<x> path per virtual (bash-safe key: virtual/cross-cc -> cross_cc)
  local v key
  for v in ${OS_VIRTUALS}; do
    key="PROVIDER_${v#virtual/}"; key="${key//-/_}"
    printf -v "${key}" '%s' "$(byname "$(resolve "${v}")")"
    export "${key?}"
  done

  # toolchain scalars (the compiler is cross-cutting — CROSS_COMPILE threads into every compile)
  ARCH="${ARCH:-arm}"
  CROSS_COMPILE="${CROSS_COMPILE:-$(recipe_get "${PROVIDER_cross_cc}" PKG_HOST_CC_PREFIX)}"
  [ -n "${CROSS_COMPILE}" ] || die "CROSS_COMPILE empty: virtual/cross-cc provider has no PKG_HOST_CC_PREFIX"
  TOOLCHAIN_DIR="${BUILD_DIR}/$(basename "$(dirname "${PROVIDER_cross_cc}")")"
  LIBC_TC_DIR="${BUILD_DIR}/$(basename "$(dirname "${PROVIDER_cross_cc_initial}")")"

  # derived paths (all a fixed function of BUILD_DIR)
  DOWNLOAD_DIR="${BUILD_DIR}/downloads"; OUTPUT_DIR="${BUILD_DIR}/output"
  PYENV_DIR="${BUILD_DIR}/pyenv"; HOSTMAKE_DIR="${BUILD_DIR}/hostmake"; HOSTTOOLS_DIR="${BUILD_DIR}/hosttools"
  OS_STAMPS="${BUILD_DIR}/.os/stamps"; OS_SIGS="${BUILD_DIR}/.os/sigs"
  HOSTTOOLS_FARM="${BUILD_DIR}/.os/hosttools-farm"; OVERLAY_DIR="${PRODUCT_DIR}/overlay"

  # libc staging (link-keyed — the producer + consumers agree here)
  local link="${LINKAGE:-${PKG_LINK:-static}}"
  LIBC_STAGE_DIR="${BUILD_DIR}/libc/stage-${LIBC:-custom}-${link}"
  STAGE_INC="${BUILD_DIR}/libc/include"

  # host-tool policy (Yocto HOSTTOOLS / ASSUME_PROVIDED / sanity) — engine policy, not per-product
  HOSTTOOLS="as awk basename bash cat cc cp curl cut dirname echo env false find gcc git grep gzip head install ld ln ls mkdir mktemp mv nproc pwd readlink rm rmdir sed sh sha256sum sleep sort tail tar tr true xargs xz"
  HOSTTOOLS_NONFATAL="addr2line ar bc bison bzip2 c++filt chmod cmp comm cpio cpp date dd diff du egrep expr fgrep file flex g++ gawk getconf gettext hostname id lz4 lzop m4 makeinfo msgfmt nm objcopy objdump od openssl patch perl pkg-config pod2html pod2man pod2text printf python3 ranlib readelf rsync seq size strings stat swig tee touch uname uniq wc whoami zstd"
  ASSUME_PROVIDED="make"
  SANITY_REQUIRED="make:3.81 gcc:4.8 python3:3.6 git:1.8"

  export BUILD_DIR BOARD_NAME BOARD_DIR KERNEL_TARGET ROOTFS_TARGET ARCH CROSS_COMPILE \
         TOOLCHAIN_DIR LIBC_TC_DIR DOWNLOAD_DIR OUTPUT_DIR PYENV_DIR HOSTMAKE_DIR HOSTTOOLS_DIR \
         OS_STAMPS OS_SIGS HOSTTOOLS_FARM OVERLAY_DIR LIBC_STAGE_DIR STAGE_INC \
         HOSTTOOLS HOSTTOOLS_NONFATAL ASSUME_PROVIDED SANITY_REQUIRED \
         KERNEL BOOTLOADER LIBC INIT TOOLCHAIN PACKAGES MEDIA LINKAGE
}

setup_build_env() {
  load_build_env
  setup_host_env        # farm + sanity while PATH is still the host's
  setup_path            # then scrub PATH
  set_recipe_env
}

load_build_env() {
  load_env
  RECIPE="$(byname "${LAYER}" || true)"; export RECIPE
  [ -n "${RECIPE}" ] && [ -f "${RECIPE}" ] \
    || die "no recipe for node '${LAYER}' — no recipes-*/ or packages/ dir by that name (typo in PACKAGES or a selection?)"
}

build_hosttools_farm() {
  local key keyfile t p missing=""
  # type -P, NOT command -v: a shell builtin (true/pwd/printf) makes command -v print a bare word -> ln -s true true self-loop.
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

sanity_check() {
  local stamp key spec tool min have
  key="$(printf '%s' "${SANITY_REQUIRED}" | sha256sum | cut -d' ' -f1)"
  stamp="${BUILD_DIR}/.os/.sanity-ok"
  [ "$(cat "${stamp}" 2>/dev/null || true)" = "${key}" ] && return 0
  for spec in ${SANITY_REQUIRED}; do
    tool="${spec%%:*}"; min="${spec#*:}"
    command -v "${tool}" >/dev/null 2>&1 || die "sanity: required host tool '${tool}' not found (need >= ${min})"
    have="$("${tool}" --version 2>&1 | head -n1 | grep -oE '[0-9]+(\.[0-9]+)+' | head -n1 || true)"
    [ -n "${have}" ] || { log "sanity: could not read ${tool} version; assuming OK"; continue; }
    [ "$(printf '%s\n%s\n' "${min}" "${have}" | sort -V | head -n1)" = "${min}" ] \
      || die "sanity: ${tool} ${have} is older than the required ${min}"
  done
  mkdir -p "${BUILD_DIR}/.os"; printf '%s' "${key}" > "${stamp}"
}

setup_host_env() { build_hosttools_farm; sanity_check; }

# Scrub PATH to os's built-tool dirs (first, so they shadow the host) + the HOSTTOOLS farm, nothing else.
setup_path() {
  # LIBC_TC_DIR/bin (stage-1) last: the libc builds before stage-2 exists + invokes cross-ar by bare name.
  local d p=""
  for d in "${HOSTMAKE_DIR}/bin" "${TOOLCHAIN_DIR}/bin" "${LIBC_TC_DIR:+${LIBC_TC_DIR}/bin}" "${HOSTTOOLS_FARM}"; do
    [ -n "$d" ] && [ -d "$d" ] || continue
    case ":$p:" in *":$d:"*) ;; *) p="${p:+$p:}$d" ;; esac
  done
  PATH="$p"; export PATH
}

set_recipe_env() {
  export PKG_LINK="${PKG_LINK:-${LINKAGE:-static}}"
  RECIPE_DIR="$(cd "$(dirname "${RECIPE}")" && pwd)"; export RECIPE_DIR
  export STAGE="${BUILD_DIR}/rootfs/stage"
  export PKG_DEST="${BUILD_DIR}/rootfs/pkgstage/${LAYER}"
  export LIBC REPO_ROOT ROOTFS_TARGET
  while IFS='=' read -r _v _; do export "${_v?}"; done < <(set | grep '^ROOTFS_ARCH_FLAGS' || true)
  RECIPE_SCRATCH="${BUILD_DIR}/scratch/${LAYER}"
  export PROVIDER_RECIPE="${RECIPE}"

  # inherit <class>: source classes-global/ then classes-recipe/, recording each for the taskhash.
  _INHERITED_CLASSES=""
  inherit() {
    local _c
    for _c in "${OS_META}/classes-global/$1.sh" "${OS_META}/classes-recipe/$1.sh"; do
      [ -f "${_c}" ] || continue
      _INHERITED_CLASSES="${_INHERITED_CLASSES} ${_c}"
      source "${_c}"; return 0
    done
    die "inherit: class '$1' not found in classes-global/ or classes-recipe/"
  }
  # require <path>: source a shared .inc (Yocto's require), recording it for the taskhash.
  _REQUIRED_INCS=""
  require() { _REQUIRED_INCS="${_REQUIRED_INCS} $1"; source "$1"; }
}

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
    mkdir -p "${OS_STAMPS}"; printf '%s' "${_taskhash}" > "${_stamp}"; exit 0
  fi
}

# _output = the durable artifact this recipe declares; empty => never cacheable, always rebuilds.
resolve_output() {
  _output=""
  if   [ -n "${PKG_HOST_BIN:-}" ];        then _output="${PKG_HOST_BIN}"
  elif [ -n "${PKG_HOST_VERIFY_BIN:-}" ]; then _output="${PKG_HOST_DEST:-}/bin/${PKG_HOST_VERIFY_BIN}"
  elif [ -n "${PKG_HOST_DEST:-}" ];       then _output="${PKG_HOST_DEST}"
  elif [ -n "${PKG_ARTIFACT:-}" ];        then _output="$(_artifact_path "${RECIPE}" PKG_ARTIFACT)"
  fi
}

_recipe_src_dir() {
  local recipe="$1"
  case "$(recipe_get "${recipe}" PKG_FETCH local)" in
    local) printf '%s' "${REPO_ROOT}/$(recipe_get "${recipe}" PKG_SOURCE)" ;;
    git)   printf '%s' "${BUILD_DIR}/$(recipe_get "${recipe}" PKG_GIT_CHECKOUT)" ;;
    *)     printf '' ;;
  esac
}

# Resolve a recipe's <key> artifact spec (base:path) to an absolute path. base = out|src|stage|libcstage.
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

# _taskhash + _stamp: hash the recipe dir + classes + includes + engine + source + link + toolchain,
# then fold each dep's recorded taskhash so a bump ripples.
compute_taskhash() {
  local base dep deps _linksens
  base="$(
    {
      # the whole recipe dir: recipe.sh + its siblings (cc-profile.sh, *.config, stage-runtime.sh, …)
      printf '=== recipe ===\n'
      ( cd "${RECIPE_DIR}" && find . -type f -exec sha256sum {} + 2>/dev/null | sort )
      printf '=== classes ===\n'
      for dep in ${_INHERITED_CLASSES}; do [ -f "${dep}" ] && { printf '# %s\n' "${dep##*/}"; cat "${dep}"; }; done
      printf '=== includes ===\n'
      for dep in ${_REQUIRED_INCS}; do [ -f "${dep}" ] && { printf '# %s\n' "${dep##*/}"; cat "${dep}"; }; done
      # Engine hashed comment-stripped, so a comment/whitespace edit doesn't rebuild the world.
      printf '=== engine ===\n'
      grep -vE '^[[:space:]]*#|^[[:space:]]*$' "${OS_ENGINE}/engine.sh" 2>/dev/null
      printf '=== source ===\n';  _hash_source
      _linksens=0
      [ "${PKG_LINKSENS:-0}" = 1 ] && _linksens=1
      case " ${PKG_DEPENDS:-} " in *" virtual/libc "*) _linksens=1 ;; esac
      [ "${_linksens}" = 1 ] && printf 'link:%s\n' "${PKG_LINK:-static}"
      # Target builds fold the cross-toolchain + arch + board; host classes opt out via PKG_TARGET_INDEPENDENT.
      if [ "${PKG_TARGET_INDEPENDENT:-0}" != 1 ]; then
        printf '=== toolchain/arch ===\n'
        printf '%s|%s\n' "${CROSS_COMPILE:-}" "${ARCH:-}"
        if [ -d "${BOARD_DIR:-/nonexistent}" ]; then
          printf '=== board ===\n'
          ( cd "${BOARD_DIR}" && find . -type f -exec sha256sum {} + 2>/dev/null | sort )
        fi
      fi
    } | sha256sum | cut -d' ' -f1
  )"

  deps="$(node_deps "${RECIPE}")"   # same resolver the graph edges use (incl PKG_HOST_DEPENDS_<MEDIA>)
  _taskhash="$(
    {
      printf '%s\n' "${base}"
      for dep in ${deps}; do
        [ -f "${OS_SIGS}/${dep}.taskhash" ] && printf 'dep:%s=%s\n' "${dep}" "$(cat "${OS_SIGS}/${dep}.taskhash")"
      done
    } | sha256sum | cut -d' ' -f1
  )"

  mkdir -p "${OS_SIGS}"
  printf '%s' "${_taskhash}" > "${OS_SIGS}/${LAYER}.taskhash"
  _stamp="${OS_STAMPS}/${LAYER}"
}

_hash_source() {
  case "${PKG_FETCH:-local}" in
    local)
      if git -C "${REPO_ROOT}" rev-parse --git-dir >/dev/null 2>&1; then
        ( cd "${REPO_ROOT}" \
          && { git ls-files -z -- "${PKG_SOURCE}"; git ls-files -z --others --exclude-standard -- "${PKG_SOURCE}"; } \
          | sort -z | xargs -0 -r sha256sum 2>/dev/null )
      else
        ( cd "${REPO_ROOT}" && find "${PKG_SOURCE}" -type f -not -path '*/build/*' -exec sha256sum {} + 2>/dev/null | sort )
      fi ;;
    git)      printf 'git:%s@%s\n' "${PKG_GIT_URL:-}" "${PKG_VERSION:-}" ;;
    tarball)  printf 'tar:%s#%s\n'  "${PKG_VERSION:-}" "${PKG_SHA256:-}" ;;
    prebuilt) printf 'pre:%s#%s\n'  "${PKG_VERSION:-}" "${PKG_SHA256:-}" ;;
    none|*)   : ;;
  esac
}

run_tasks() {
  rm -rf "${RECIPE_SCRATCH}"; mkdir -p "${RECIPE_SCRATCH}"
  do_fetch
  do_unpack
  do_patch
  do_build
  do_install
}

mark_built() {
  if [ -n "${_output}" ]; then mkdir -p "${OS_STAMPS}"; printf '%s' "${_taskhash}" > "${_stamp}"; fi
}

# execute-recipe: build one recipe end to end.
execute_recipe() {
  setup_build_env
  inherit base
  # shellcheck disable=SC1090
  source "${RECIPE}"
  skip_if_built
  run_tasks
  mark_built
}

# CLI dispatch (only when executed, not sourced)
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
  cmd="${1:?engine.sh: need a subcommand (resolve-dependencies|execute-recipe)}"; shift
  case "${cmd}" in
    resolve-dependencies) resolve_dependencies "$@" ;;
    execute-recipe)       LAYER="${1:?engine.sh execute-recipe: need a recipe name}"; export LAYER; execute_recipe ;;
    *)                    die "engine.sh: unknown subcommand '${cmd}' (resolve-dependencies|execute-recipe)" ;;
  esac
fi
