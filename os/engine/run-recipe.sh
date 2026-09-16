#!/usr/bin/env bash
# run-recipe.sh — the one node runner: resolve the env (os-env.sh), source the recipe, gate on the
# content cache, then do_fetch -> do_build -> do_install by name. In: node NAME ($1) + PRODUCT_DIR (env).
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/os-env.sh"   # recipe_get, log/die, os_load_env, os_byname
LAYER="${1:?run-recipe.sh: need a node name}"; export LAYER

setup_build_env() {
  load_build_env
  setup_host_env        # farm + sanity while PATH is still the host's
  setup_path            # then scrub PATH
  set_recipe_env
}

load_build_env() {
  os_load_env
  RECIPE="$(os_byname "${LAYER}" || true)"; export RECIPE
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

# _taskhash + _stamp: hash the recipe + classes + includes + engine + siblings + source + link + toolchain,
# then fold each dep's recorded taskhash so a bump ripples.
compute_taskhash() {
  local base dep deps _linksens
  base="$(
    {
      printf '=== recipe ===\n';  cat "${RECIPE}"
      printf '=== classes ===\n'
      for dep in ${_INHERITED_CLASSES}; do [ -f "${dep}" ] && { printf '# %s\n' "${dep##*/}"; cat "${dep}"; }; done
      printf '=== includes ===\n'
      for dep in ${_REQUIRED_INCS}; do [ -f "${dep}" ] && { printf '# %s\n' "${dep##*/}"; cat "${dep}"; }; done
      # Engine hashed comment-stripped, so a comment/whitespace edit doesn't rebuild the world.
      printf '=== engine ===\n'
      for _e in run-recipe.sh os-env.sh; do grep -vE '^[[:space:]]*#|^[[:space:]]*$' "${OS_ENGINE}/${_e}" 2>/dev/null; done
      printf '=== siblings ===\n'
      ( cd "${RECIPE_DIR}" && find . -type f ! -name recipe.sh -exec sha256sum {} + 2>/dev/null | sort )
      printf '=== source ===\n';  _hash_source
      _linksens=0
      [ "${PKG_LINKSENS:-0}" = 1 ] && _linksens=1
      case " ${PKG_DEPENDS:-} " in *" virtual/libc "*) _linksens=1 ;; esac
      [ "${_linksens}" = 1 ] && printf 'link:%s\n' "${PKG_LINK:-static}"
      # Target builds fold the cross-toolchain + arch + board; host classes opt out via PKG_TARGET_INDEPENDENT.
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
  # A target recipe that links virtual/libc gets the implied compiler edge (matches engine.mk _ndeps).
  if [ "${PKG_CLASS:-target}" = target ]; then
    case " ${PKG_DEPENDS:-} " in *" virtual/libc "*) deps="${deps} virtual/cross-cc" ;; esac
  fi
  _rdeps=""; for dep in ${deps}; do _rdeps="${_rdeps} $(os_resolve "${dep}")"; done   # virtual/* -> provider name
  deps="${_rdeps}"
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

main() {
  setup_build_env
  inherit base
  # shellcheck disable=SC1090
  source "${RECIPE}"
  skip_if_built
  run_tasks
  mark_built
}
main
