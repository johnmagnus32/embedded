#!/usr/bin/env bash
# os-env.sh — the engine's config + resolution brain (bash). All config and provider resolution
# live here; engine.mk is just a graph walker. Two entry points:
#   * SOURCED by run-recipe.sh  -> os_load_env sets the whole build environment.
#   * EXECUTED by engine.mk:  os-env.sh deps <recipe> -> that recipe's resolved prerequisite
#     recipe names (the Make prerequisites for its rule).
# Config is the product's local.conf (plain bash, env-overridable); providers are resolved via the
# os_preferred_provider() it defines. Make never sees a config value — only PRODUCT_DIR (the seed).
set -euo pipefail

log() { printf '\033[1;34m[%s]\033[0m %s\n' "${LAYER:-os}" "$*"; }
die() { printf '\033[1;31m[%s] ERROR:\033[0m %s\n' "${LAYER:-os}" "$*" >&2; exit 1; }

# recipe_get <recipe> <KEY> [default] -> bare value of the last KEY=, ${VAR}-expanded (env in scope).
# Normalization: strip inline `# comment`, trim, collapse runs, strip one quote layer, then eval.
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

# os_locate — the engine's own dirs, from this file's path (no config needed).
os_locate() {
  OS_ENGINE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  OS_META="$(cd "${OS_ENGINE}/../meta" && pwd)"
  REPO_ROOT="$(cd "${OS_ENGINE}/../.." && pwd)"
  export OS_ENGINE OS_META REPO_ROOT
}

# os_load_config — the product SELECTION: source local.conf (knobs with env-override defaults +
# OS_VIRTUALS + os_preferred_provider()). PRODUCT_DIR is the one required seed (from the env).
os_load_config() {
  os_locate
  : "${PRODUCT_DIR:?os: PRODUCT_DIR unset (Make injects it)}"
  # shellcheck source=/dev/null
  source "${PRODUCT_DIR}/local.conf"
}

# os_byname <name> -> its recipe.sh path. Product recipes-*/ + packages/ shadow os/meta.
os_byname() {
  local n="$1" r
  for r in "${PRODUCT_DIR}"/recipes-*/"${n}"/recipe.sh "${PRODUCT_DIR}"/packages/"${n}"/recipe.sh "${OS_META}"/recipes-*/"${n}"/recipe.sh; do
    [ -f "${r}" ] && { printf '%s' "${r}"; return 0; }
  done
  return 1
}

# os_resolve <token> -> a virtual/<x> becomes its VERIFIED preferred provider NAME (fail loud if the
# preference is unset, names no recipe, or names one that doesn't provide it); anything else passes
# through. Used for graph edges (deps) and the taskhash dep keys.
os_resolve() {
  case "$1" in
    virtual/*)
      local name path
      name="$(os_preferred_provider "$1")" || die "no preferred provider for $1 (local.conf os_preferred_provider)"
      path="$(os_byname "${name}")" || die "preferred provider $1=${name}: no such recipe"
      case " $(recipe_get "${path}" PKG_PROVIDES) " in
        *" $1 "*) : ;;
        *) die "preferred provider $1=${name}: recipe does not provide $1" ;;
      esac
      printf '%s' "${name}" ;;
    *) printf '%s' "$1" ;;
  esac
}

# os_deps <recipe> -> its resolved prerequisite recipe names. Reads PKG_DEPENDS + PKG_HOST_DEPENDS +
# PKG_HOST_DEPENDS_<MEDIA> (recipe_get expands ${PACKAGES}); adds the implied compiler edge for a
# target that links libc; resolves each virtual/<x>; prepends the `make` barrier (all but make itself).
os_deps() {
  os_load_config
  local recipe="$1" path raw tok out=""
  path="$(os_byname "${recipe}")" || return 0   # unknown recipe: no prereqs (run-recipe errors at build)
  raw="$(recipe_get "${path}" PKG_DEPENDS) $(recipe_get "${path}" PKG_HOST_DEPENDS) $(recipe_get "${path}" "PKG_HOST_DEPENDS_${MEDIA}")"
  if [ "$(recipe_get "${path}" PKG_CLASS target)" = target ]; then
    case " $(recipe_get "${path}" PKG_DEPENDS) " in *" virtual/libc "*) raw="${raw} virtual/cross-cc" ;; esac
  fi
  for tok in ${raw}; do out="${out} $(os_resolve "${tok}")"; done
  [ "${recipe}" = make ] && printf '%s\n' "${out}" || printf 'make%s\n' "${out}"
}

# os_load_env — the FULL build environment (derived here from local.conf + board.conf). Sets the
# same variable names recipes/classes read, so they are unchanged. Sourced by run-recipe.sh.
os_load_env() {
  os_load_config
  BUILD_DIR="${PRODUCT_DIR}/build"
  BOARD_NAME="${BOARD}"; BOARD_DIR="${PRODUCT_DIR}/boards/${BOARD}"
  # shellcheck source=/dev/null
  [ -f "${BOARD_DIR}/board.conf" ] && source "${BOARD_DIR}/board.conf"
  : "${KERNEL_TARGET:?boards/${BOARD}/board.conf must set KERNEL_TARGET}"
  ROOTFS_TARGET="${ROOTFS_TARGET:-${KERNEL_TARGET}}"

  # resolved providers — PROVIDER_<x> path for each virtual (bash-safe key: virtual/cross-cc -> cross_cc)
  local v key
  for v in ${OS_VIRTUALS}; do
    key="PROVIDER_${v#virtual/}"; key="${key//-/_}"
    printf -v "${key}" '%s' "$(os_byname "$(os_resolve "${v}")")"
    export "${key?}"
  done

  # toolchain scalars (the compiler is cross-cutting — CROSS_COMPILE threads into every compile)
  TC_ARCH="${TC_ARCH:-armv7-eabihf}"; ARCH="${ARCH:-arm}"
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

  # product artifact/bundle tags
  local libctag
  [ "${LIBC}" = custom ] && libctag="${LIBC}-${TOOLCHAIN}" || libctag="${LIBC}"
  ROOTFS_TAG="${libctag}-${INIT}-$(echo ${PACKAGES} | tr ' ' '+')"
  CFG="${BOOTLOADER}-${KERNEL}-${ROOTFS_TAG}"
  INITRAMFS_IMAGE="initramfs-${ROOTFS_TAG}-${link}.cpio.gz"

  # host-tool policy (Yocto HOSTTOOLS / ASSUME_PROVIDED / sanity) — engine policy, not per-product
  HOSTTOOLS="as awk basename bash cat cc cp curl cut dirname echo env false find gcc git grep gzip head install ld ln ls mkdir mktemp mv nproc pwd readlink rm rmdir sed sh sha256sum sleep sort tail tar tr true xargs xz"
  HOSTTOOLS_NONFATAL="addr2line ar bc bison bzip2 c++filt chmod cmp comm cpio cpp date dd diff du egrep expr fgrep file flex g++ gawk getconf gettext hostname id lz4 lzop m4 makeinfo msgfmt nm objcopy objdump od openssl patch perl pkg-config pod2html pod2man pod2text printf python3 ranlib readelf rsync seq size strings stat swig tee touch uname uniq wc whoami zstd"
  ASSUME_PROVIDED="make"
  SANITY_REQUIRED="make:3.81 gcc:4.8 python3:3.6 git:1.8"

  export BUILD_DIR BOARD_NAME BOARD_DIR KERNEL_TARGET ROOTFS_TARGET TC_ARCH ARCH CROSS_COMPILE \
         TOOLCHAIN_DIR LIBC_TC_DIR DOWNLOAD_DIR OUTPUT_DIR PYENV_DIR HOSTMAKE_DIR HOSTTOOLS_DIR \
         OS_STAMPS OS_SIGS HOSTTOOLS_FARM OVERLAY_DIR LIBC_STAGE_DIR STAGE_INC \
         ROOTFS_TAG CFG INITRAMFS_IMAGE HOSTTOOLS HOSTTOOLS_NONFATAL ASSUME_PROVIDED SANITY_REQUIRED \
         KERNEL BOOTLOADER LIBC INIT TOOLCHAIN PACKAGES MEDIA LINKAGE
}

# CLI dispatch (only when executed, not sourced)
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
  cmd="${1:?os-env.sh: need a subcommand (deps)}"; shift
  case "${cmd}" in
    deps) os_deps "$@" ;;
    *)    die "os-env.sh: unknown subcommand '${cmd}' (deps)" ;;
  esac
fi
