#!/usr/bin/env bash
# forge-env.sh — the engine's config + resolution brain (bash). All config and provider resolution
# live here; engine.mk is just a graph walker. Two entry points:
#   * SOURCED by run-recipe.sh  -> forge_load_env sets the whole build environment.
#   * EXECUTED by engine.mk / the product Makefile:
#       forge-env.sh deps  <recipe>  -> that recipe's resolved prerequisite recipe names (Make prereqs)
#       forge-env.sh print <VAR>...  -> `VAR=value` lines (for the product Makefile's flash target)
# Config is the product's local.conf (plain bash, env-overridable); providers are resolved via the
# forge_preferred_provider() it defines. Make never sees a config value — only PRODUCT_DIR (the seed).
set -euo pipefail

log() { printf '\033[1;34m[%s]\033[0m %s\n' "${LAYER:-forge}" "$*"; }
die() { printf '\033[1;31m[%s] ERROR:\033[0m %s\n' "${LAYER:-forge}" "$*" >&2; exit 1; }

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

# forge_locate — the engine's own dirs, from this file's path (no config needed).
forge_locate() {
  FORGE_ENGINE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  FORGE_META="$(cd "${FORGE_ENGINE}/../meta" && pwd)"
  REPO_ROOT="$(cd "${FORGE_ENGINE}/../.." && pwd)"
  export FORGE_ENGINE FORGE_META REPO_ROOT
}

# forge_load_config — the product SELECTION: source local.conf (knobs with env-override defaults +
# FORGE_VIRTUALS + forge_preferred_provider()). PRODUCT_DIR is the one required seed (from the env).
forge_load_config() {
  forge_locate
  : "${PRODUCT_DIR:?forge: PRODUCT_DIR unset (Make injects it)}"
  # shellcheck source=/dev/null
  source "${PRODUCT_DIR}/local.conf"
}

# forge_byname <name> -> its recipe.sh path. Product recipes-*/ + packages/ shadow forge/meta.
forge_byname() {
  local n="$1" r
  for r in "${PRODUCT_DIR}"/recipes-*/"${n}"/recipe.sh "${PRODUCT_DIR}"/packages/"${n}"/recipe.sh "${FORGE_META}"/recipes-*/"${n}"/recipe.sh; do
    [ -f "${r}" ] && { printf '%s' "${r}"; return 0; }
  done
  return 1
}

# forge_resolve <token> -> a virtual/<x> becomes its VERIFIED preferred provider NAME (fail loud if the
# preference is unset, names no recipe, or names one that doesn't provide it); anything else passes
# through. Used for graph edges (deps) and the taskhash dep keys.
forge_resolve() {
  case "$1" in
    virtual/*)
      local name path
      name="$(forge_preferred_provider "$1")" || die "no preferred provider for $1 (local.conf forge_preferred_provider)"
      path="$(forge_byname "${name}")" || die "preferred provider $1=${name}: no such recipe"
      case " $(recipe_get "${path}" PKG_PROVIDES) " in
        *" $1 "*) : ;;
        *) die "preferred provider $1=${name}: recipe does not provide $1" ;;
      esac
      printf '%s' "${name}" ;;
    *) printf '%s' "$1" ;;
  esac
}

# forge_deps <recipe> -> its resolved prerequisite recipe names. Reads PKG_DEPENDS + PKG_HOST_DEPENDS +
# PKG_HOST_DEPENDS_<MEDIA> (recipe_get expands ${PACKAGES}); adds the implied compiler edge for a
# target that links libc; resolves each virtual/<x>; prepends the `make` barrier (all but make itself).
forge_deps() {
  forge_load_config
  local recipe="$1" path raw tok out=""
  path="$(forge_byname "${recipe}")" || return 0   # unknown recipe: no prereqs (run-recipe errors at build)
  raw="$(recipe_get "${path}" PKG_DEPENDS) $(recipe_get "${path}" PKG_HOST_DEPENDS) $(recipe_get "${path}" "PKG_HOST_DEPENDS_${MEDIA}")"
  if [ "$(recipe_get "${path}" PKG_CLASS target)" = target ]; then
    case " $(recipe_get "${path}" PKG_DEPENDS) " in *" virtual/libc "*) raw="${raw} virtual/cross-cc" ;; esac
  fi
  for tok in ${raw}; do out="${out} $(forge_resolve "${tok}")"; done
  [ "${recipe}" = make ] && printf '%s\n' "${out}" || printf 'make%s\n' "${out}"
}

# forge_load_env — the FULL build environment (what forge.conf used to carry, now derived). Sets the
# same variable names recipes/classes read, so they are unchanged. Sourced by run-recipe.sh.
forge_load_env() {
  forge_load_config
  BUILD_DIR="${PRODUCT_DIR}/build"
  BOARD_NAME="${BOARD}"; BOARD_DIR="${PRODUCT_DIR}/boards/${BOARD}"
  # shellcheck source=/dev/null
  [ -f "${BOARD_DIR}/board.conf" ] && source "${BOARD_DIR}/board.conf"
  : "${KERNEL_TARGET:?boards/${BOARD}/board.conf must set KERNEL_TARGET}"
  ROOTFS_TARGET="${ROOTFS_TARGET:-${KERNEL_TARGET}}"

  # resolved providers — PROVIDER_<x> path for each virtual (bash-safe key: virtual/cross-cc -> cross_cc)
  local v key
  for v in ${FORGE_VIRTUALS}; do
    key="PROVIDER_${v#virtual/}"; key="${key//-/_}"
    printf -v "${key}" '%s' "$(forge_byname "$(forge_resolve "${v}")")"
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
  FORGE_STAMPS="${BUILD_DIR}/.forge/stamps"; FORGE_SIGS="${BUILD_DIR}/.forge/sigs"
  HOSTTOOLS_FARM="${BUILD_DIR}/.forge/hosttools-farm"; OVERLAY_DIR="${PRODUCT_DIR}/overlay"

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
  BUNDLE="${BUILD_DIR}/bundles/${CFG}"

  # host-tool policy (Yocto HOSTTOOLS / ASSUME_PROVIDED / sanity) — engine policy, not per-product
  HOSTTOOLS="as awk basename bash cat cc cp curl cut dirname echo env false find gcc git grep gzip head install ld ln ls mkdir mktemp mv nproc pwd readlink rm rmdir sed sh sha256sum sleep sort tail tar tr true xargs xz"
  HOSTTOOLS_NONFATAL="addr2line ar bc bison bzip2 c++filt chmod cmp comm cpio cpp date dd diff du egrep expr fgrep file flex g++ gawk getconf gettext hostname id lz4 lzop m4 makeinfo msgfmt nm objcopy objdump od openssl patch perl pkg-config pod2html pod2man pod2text printf python3 ranlib readelf rsync seq size strings stat swig tee touch uname uniq wc whoami zstd"
  ASSUME_PROVIDED="make"
  SANITY_REQUIRED="make:3.81 gcc:4.8 python3:3.6 git:1.8"

  export BUILD_DIR BOARD_NAME BOARD_DIR KERNEL_TARGET ROOTFS_TARGET TC_ARCH ARCH CROSS_COMPILE \
         TOOLCHAIN_DIR LIBC_TC_DIR DOWNLOAD_DIR OUTPUT_DIR PYENV_DIR HOSTMAKE_DIR HOSTTOOLS_DIR \
         FORGE_STAMPS FORGE_SIGS HOSTTOOLS_FARM OVERLAY_DIR LIBC_STAGE_DIR STAGE_INC \
         ROOTFS_TAG CFG INITRAMFS_IMAGE BUNDLE HOSTTOOLS HOSTTOOLS_NONFATAL ASSUME_PROVIDED SANITY_REQUIRED \
         KERNEL BOOTLOADER LIBC INIT TOOLCHAIN PACKAGES MEDIA LINKAGE
}

# CLI dispatch (only when executed, not sourced)
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
  cmd="${1:?forge-env.sh: need a subcommand (deps|print)}"; shift
  case "${cmd}" in
    deps)  forge_deps "$@" ;;
    print) forge_load_env; for v in "$@"; do printf '%s=%q\n' "${v}" "${!v}"; done ;;
    *)     die "forge-env.sh: unknown subcommand '${cmd}' (deps|print)" ;;
  esac
fi
