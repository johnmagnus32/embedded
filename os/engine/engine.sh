#!/usr/bin/env bash
# engine.sh — the OS build engine (bash half; engine.mk is the Make graph-walker). Two subcommands,
# each taking a recipe NAME + the seed PRODUCT_DIR (env); all config/resolution reads the product's
# local.conf, so Make never sees a config value:
#   engine.sh resolve-dependencies <recipe>  -> its resolved prerequisite recipe names (the Make prereqs)
#   engine.sh execute-recipe        <recipe>  -> build it: resolve the env, source the recipe, cache-gate,
#                                                then do_fetch -> do_build -> do_install by name.
set -euo pipefail

log() { printf '\033[1;34m[%s]\033[0m %s\n' "${RECIPE:-os}" "$*"; }
die() { printf '\033[1;31m[%s] ERROR:\033[0m %s\n' "${RECIPE:-os}" "$*" >&2; exit 1; }

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

# inherit <class> / require <path> — the class/include DSL (Yocto's `inherit`/`require` keywords, which
# are engine-level, not class-defined — `inherit` bootstraps `inherit base` itself). Each records what it
# pulled in (_INHERITED_CLASSES / _REQUIRED_INCS, reset per-recipe in load_env) so compute_recipehash
# hashes it. Called while the recipe + its classes are sourced, after load_env set OS_META + the resets.
inherit() {
  local _c
  for _c in "${OS_META}/classes-global/$1.sh" "${OS_META}/classes-recipe/$1.sh"; do
    [ -f "${_c}" ] || continue
    _INHERITED_CLASSES="${_INHERITED_CLASSES} ${_c}"
    source "${_c}"; return 0
  done
  die "inherit: class '$1' not found in classes-global/ or classes-recipe/"
}
require() { _REQUIRED_INCS="${_REQUIRED_INCS} $1"; source "$1"; }

# locate — the engine's own dirs, from this file's path.
locate() {
  OS_ENGINE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  OS_META="$(cd "${OS_ENGINE}/../meta" && pwd)"
  REPO_ROOT="$(cd "${OS_ENGINE}/../.." && pwd)"
  export OS_META REPO_ROOT
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

# recipe_deps <recipe-path> -> its resolved prerequisite recipe names: PKG_DEPENDS + PKG_HOST_DEPENDS +
# PKG_HOST_DEPENDS_<MEDIA> (recipe_get expands ${PACKAGES}) + the implied compiler edge for a target
# that links libc, each virtual/<x> resolved. No `make` barrier — that's a Make-ordering edge, added by
# resolve-dependencies. Used by BOTH resolve-dependencies (graph edges) + compute_recipehash (hash fold).
recipe_deps() {
  local path="$1" raw tok out=""
  raw="$(recipe_get "${path}" PKG_DEPENDS) $(recipe_get "${path}" PKG_HOST_DEPENDS) $(recipe_get "${path}" "PKG_HOST_DEPENDS_${MEDIA}")"
  if [ "$(recipe_get "${path}" PKG_CLASS target)" = target ]; then
    case " $(recipe_get "${path}" PKG_DEPENDS) " in *" virtual/libc "*) raw="${raw} virtual/cross-cc" ;; esac
  fi
  for tok in ${raw}; do out="${out} $(resolve "${tok}")"; done
  printf '%s' "${out# }"
}

# resolve-dependencies <recipe> -> recipe_deps + the `make` barrier (all recipes but make itself).
resolve_dependencies() {
  load_config
  local recipe="$1" path deps
  path="$(byname "${recipe}")" || return 0
  deps="$(recipe_deps "${path}")"
  [ "${recipe}" = make ] && printf '%s\n' "${deps}" || printf 'make %s\n' "${deps}"
}

# load_env — the FULL build environment for execute-recipe: source local.conf + board.conf, resolve the
# recipe name (RECIPE) to its recipe.sh path (RECIPE_PATH), and set the vars recipes/classes read.
load_env() {
  load_config
  RECIPE_PATH="$(byname "${RECIPE}" || true)"
  [ -n "${RECIPE_PATH}" ] && [ -f "${RECIPE_PATH}" ] \
    || die "no recipe for '${RECIPE}' — no recipes-*/ or packages/ dir by that name (typo in PACKAGES or a selection?)"
  BUILD_DIR="${PRODUCT_DIR}/build"
  BOARD_NAME="${BOARD}"; BOARD_DIR="${PRODUCT_DIR}/boards/${BOARD}"
  # shellcheck source=/dev/null
  [ -f "${BOARD_DIR}/board.conf" ] && source "${BOARD_DIR}/board.conf"
  : "${KERNEL_TARGET:?boards/${BOARD}/board.conf must set KERNEL_TARGET}"

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
  HOSTTOOLS_FARM="${BUILD_DIR}/.os/hosttools-farm"

  # libc staging (link-keyed — the producer + consumers agree here)
  local link="${LINKAGE:-${PKG_LINK:-static}}"
  LIBC_STAGE_DIR="${BUILD_DIR}/libc/stage-${LIBC:-custom}-${link}"
  STAGE_INC="${BUILD_DIR}/libc/include"

  # this recipe's build context: paths keyed by RECIPE / RECIPE_PATH + the link mode
  PKG_LINK="${PKG_LINK:-${LINKAGE:-static}}"
  RECIPE_DIR="$(cd "$(dirname "${RECIPE_PATH}")" && pwd)"
  STAGE="${BUILD_DIR}/rootfs/stage"
  PKG_DEST="${BUILD_DIR}/rootfs/pkgstage/${RECIPE}"
  RECIPE_SCRATCH="${BUILD_DIR}/scratch/${RECIPE}"
  PROVIDER_RECIPE="${RECIPE_PATH}"
  # export board.conf's ROOTFS_ARCH_FLAGS[_*] (read by classes); reset the inherit/require accumulators
  # for this recipe (they feed compute_recipehash).
  while IFS='=' read -r _v _; do export "${_v?}"; done < <(set | grep '^ROOTFS_ARCH_FLAGS' || true)
  _INHERITED_CLASSES="" _REQUIRED_INCS=""

  # host-tool policy (Yocto HOSTTOOLS): a required allowlist + a nonfatal (config-specific) one. A
  # `name:min` entry also version-gates the host tool (build_hosttools_farm). Engine policy, not per-product.
  HOSTTOOLS="as awk basename bash cat cc cp curl cut dirname echo env false find gcc:4.8 git:1.8 grep gzip head install ld ln ls make:3.81 mkdir mktemp mv nproc pwd readlink rm rmdir sed sh sha256sum sleep sort tail tar tr true xargs xz"
  HOSTTOOLS_NONFATAL="addr2line ar bc bison bzip2 c++filt chmod cmp comm cpio cpp date dd diff du egrep expr fgrep file flex g++ gawk getconf gettext hostname id lz4 lzop m4 makeinfo msgfmt nm objcopy objdump od openssl patch perl pkg-config pod2html pod2man pod2text printf python3:3.6 ranlib readelf rsync seq size strings stat swig tee touch uname uniq wc whoami zstd"

  export BUILD_DIR BOARD_NAME BOARD_DIR KERNEL_TARGET ARCH CROSS_COMPILE RECIPE_PATH \
         TOOLCHAIN_DIR LIBC_TC_DIR DOWNLOAD_DIR OUTPUT_DIR PYENV_DIR HOSTMAKE_DIR HOSTTOOLS_DIR \
         OS_STAMPS OS_SIGS HOSTTOOLS_FARM LIBC_STAGE_DIR STAGE_INC \
         HOSTTOOLS HOSTTOOLS_NONFATAL \
         PKG_LINK RECIPE_DIR STAGE PKG_DEST PROVIDER_RECIPE \
         KERNEL BOOTLOADER LIBC INIT TOOLCHAIN PACKAGES MEDIA LINKAGE
}

setup_build_env() {
  load_env
  build_hosttools_farm  # provision + version-gate host tools while PATH is still the host's
  setup_path
}

build_hosttools_farm() {
  local key keyfile t min p missing=""
  # type -P, NOT command -v: a shell builtin (true/pwd/printf) makes command -v print a bare word -> ln -s true true self-loop.
  key="$(printf 'farmv4|%s|%s' "${HOSTTOOLS}" "${HOSTTOOLS_NONFATAL}" | sha256sum | cut -d' ' -f1)"
  keyfile="${HOSTTOOLS_FARM}/.key"
  [ "$(cat "${keyfile}" 2>/dev/null || true)" = "${key}" ] && return 0
  rm -rf "${HOSTTOOLS_FARM}"; mkdir -p "${HOSTTOOLS_FARM}"
  # Required: symlink each (die if missing); a `name:min` entry also version-gates the host tool.
  for t in ${HOSTTOOLS}; do
    min=""; case "$t" in *:*) min="${t#*:}"; t="${t%%:*}" ;; esac
    if p="$(type -P "$t" 2>/dev/null)"; then ln -s "$p" "${HOSTTOOLS_FARM}/$t"; check_tool_version "$t" "$min"
    else missing="${missing} $t"; fi
  done
  [ -z "${missing}" ] || die "host is missing required tool(s):${missing}
  (Debian/Ubuntu: apt install build-essential binutils git curl xz-utils)"
  # Optional (nonfatal, config-specific): symlink if present; version-gate a present `name:min`.
  for t in ${HOSTTOOLS_NONFATAL}; do
    min=""; case "$t" in *:*) min="${t#*:}"; t="${t%%:*}" ;; esac
    if p="$(type -P "$t" 2>/dev/null)"; then ln -s "$p" "${HOSTTOOLS_FARM}/$t"; check_tool_version "$t" "$min"; fi
  done
  printf '%s' "${key}" > "${keyfile}"
}

# check_tool_version <tool> <min> — die if the host tool's --version is older than min. No-op when min
# is empty or the version can't be parsed (best-effort, like Yocto's sanity check).
check_tool_version() {
  local tool="$1" min="$2" have
  [ -n "${min}" ] || return 0
  have="$("${tool}" --version 2>&1 | head -n1 | grep -oE '[0-9]+(\.[0-9]+)+' | head -n1 || true)"
  [ -n "${have}" ] || { log "host: could not read ${tool} version; assuming OK"; return 0; }
  [ "$(printf '%s\n%s\n' "${min}" "${have}" | sort -V | head -n1)" = "${min}" ] \
    || die "host: ${tool} ${have} is older than the required ${min}"
}

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

# skip_if_built — cache gate: a recipe is up to date iff its stamp records the current recipehash.
# We trust the stamp (like Yocto's sigdata); a hand-deleted artifact isn't self-healed — run clean.
skip_if_built() {
  compute_recipehash
  if [ "$(cat "${_stamp}" 2>/dev/null)" = "${_recipehash}" ]; then
    log "cached — up to date (recipehash ${_recipehash:0:12})"; exit 0
  fi
}

# _recipehash + _stamp: hash the recipe dir + classes + includes + source + the recipe's declared var/file
# deps, then fold each dep's recorded recipehash so a bump ripples. The engine itself is NOT hashed (like
# Yocto trusting bitbake-core): its build-affecting logic — env setup + task order — changes rarely and
# is a "clean the world" edit; the orchestration that changes often doesn't affect a recipe's output.
compute_recipehash() {
  local this_recipe_hash dependency_recipe_hashes dep
  this_recipe_hash="$(
    {
      # recipe source
      ( cd "${RECIPE_DIR}" && find . -type f -exec sha256sum {} + 2>/dev/null | sort )
      for dep in ${_INHERITED_CLASSES}; do [ -f "${dep}" ] && { printf '# %s\n' "${dep##*/}"; cat "${dep}"; }; done
      for dep in ${_REQUIRED_INCS}; do [ -f "${dep}" ] && { printf '# %s\n' "${dep##*/}"; cat "${dep}"; }; done
      [ "${PKG_FETCH:-local}" = local ] \
        && ( cd "${REPO_ROOT}" && find "${PKG_SOURCE}" -type f -not -path '*/build/*' -exec sha256sum {} + 2>/dev/null | sort )
      # config values
      for dep in ${PKG_VARDEPS:-};  do printf 'var:%s=%s\n' "${dep}" "${!dep:-}"; done
      for dep in ${PKG_FILEDEPS:-}; do
        [ -e "${dep}" ] || continue
        printf 'file:%s\n' "${dep##*/}"
        if [ -d "${dep}" ]; then ( cd "${dep}" && find . -type f -exec sha256sum {} + 2>/dev/null | sort )
        else sha256sum "${dep}" 2>/dev/null; fi
      done
    } | sha256sum | cut -d' ' -f1
  )"

  dependency_recipe_hashes="$(
    for dep in $(recipe_deps "${RECIPE_PATH}"); do
      [ -f "${OS_SIGS}/${dep}.recipehash" ] && printf 'dep:%s=%s\n' "${dep}" "$(cat "${OS_SIGS}/${dep}.recipehash")"
    done
  )"
  _recipehash="$(
    {
      printf '%s\n' "${this_recipe_hash}"
      printf '%s\n' "${dependency_recipe_hashes}"
    } | sha256sum | cut -d' ' -f1
  )"

  mkdir -p "${OS_SIGS}"
  printf '%s' "${_recipehash}" > "${OS_SIGS}/${RECIPE}.recipehash"
  _stamp="${OS_STAMPS}/${RECIPE}"
}

run_tasks() {
  rm -rf "${RECIPE_SCRATCH}"; mkdir -p "${RECIPE_SCRATCH}"
  do_fetch
  do_unpack
  do_patch
  do_build
  do_install
  do_deploy
}

mark_built() { mkdir -p "${OS_STAMPS}"; printf '%s' "${_recipehash}" > "${_stamp}"; }

# execute-recipe: build one recipe end to end.
execute_recipe() {
  setup_build_env
  inherit base
  # shellcheck disable=SC1090
  source "${RECIPE_PATH}"
  skip_if_built
  run_tasks
  mark_built
}

# CLI dispatch (only when executed, not sourced)
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
  cmd="${1:?engine.sh: need a subcommand (resolve-dependencies|execute-recipe)}"; shift
  case "${cmd}" in
    resolve-dependencies) resolve_dependencies "$@" ;;
    execute-recipe)       RECIPE="${1:?engine.sh execute-recipe: need a recipe name}"; export RECIPE; execute_recipe ;;
    *)                    die "engine.sh: unknown subcommand '${cmd}' (resolve-dependencies|execute-recipe)" ;;
  esac
fi
