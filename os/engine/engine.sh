#!/usr/bin/env bash
# engine.sh — the OS build engine (bash half; engine.mk is the Make graph-walker). Two subcommands,
# each taking a recipe NAME + the seed PRODUCT_DIR (env); all config/resolution reads the product's
# local.conf, so Make never sees a config value:
#   engine.sh resolve-dependencies <recipe>  -> its resolved prerequisite recipe names (Make prereqs)
#   engine.sh execute-recipe        <recipe>  -> build it: setup env, source recipe, cache-gate, run tasks
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

# inherit <class> / require <path> — the class/include DSL (Yocto's keywords; engine-level, not
# class-defined — inherit bootstraps `inherit base`). Each records what it pulled in (reset per-recipe in
# load_env) so compute_recipe_stamp hashes it.
inherit() {
  local _c
  for _c in "${OS_META}/classes-global/$1.sh" "${OS_META}/classes-recipe/$1.sh"; do
    [ -f "${_c}" ] || continue
    _INHERITED_CLASSES="${_INHERITED_CLASSES} ${_c}"
    source "${_c}"; return 0
  done
  die "inherit: class '$1' not found in classes-global/ or classes-recipe/"
}

require() {
  _REQUIRED_INCS="${_REQUIRED_INCS} $1"
  source "$1"
}

# locate — the engine's own dirs, from this file's path.
locate() {
  OS_ENGINE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  OS_META="$(cd "${OS_ENGINE}/../meta" && pwd)"
  REPO_ROOT="$(cd "${OS_ENGINE}/../.." && pwd)"
  export OS_ENGINE OS_META REPO_ROOT
}

# load_config — source the product's config (conf/local.conf, Yocto's local.conf) + the selected machine's
# BSP (conf/machine/<machine>.conf). local.conf sets PROVIDER_<slot>=recipe per swappable slot + plain knobs
# (PACKAGES/MEDIA/LINKAGE — edit to change, no env override); machine.conf sets board facts (ARCH, KERNEL_*,
# UBOOT_*, NOR_*, ...). BOTH are sourced under `set -a`, so everything they set is exported for subprocesses
# (the kernel/u-boot builds read ARCH; compiles read ROOTFS_ARCH_FLAGS) — every key is namespaced except
# ARCH, whose value (arm) is correct for the tools that read it and inert for the ones that don't. MACHINE is
# the build-time selector (env, REQUIRED, no magical default) and is UN-exported — it must NOT reach the
# toolchain build (gcc bakes it into the target, arm:<machine>); neither conf sets it, so that's the only
# leak path. Called by resolve_dependencies + load_env — like bitbake reading local.conf + machine.conf.
load_config() {
  locate
  : "${PRODUCT_DIR:?os: PRODUCT_DIR unset (Make injects it)}"
  local conf="${PRODUCT_DIR}/conf/local.conf"
  [ -f "${conf}" ] || die "no product config at ${conf}"

  # MACHINE selects the machine's BSP (conf/machine/<machine>.conf) + the shared DT-overlay pool
  # (DEVICETREE_DIR). Both required — an unknown machine fails loud. Un-export MACHINE first (see above).
  [ -n "${MACHINE:-}" ] || die "MACHINE unset — pick one: $(ls "${PRODUCT_DIR}/conf/machine"/*.conf 2>/dev/null | xargs -n1 basename 2>/dev/null | sed 's/\.conf$//' | tr '\n' ' ')(e.g. MACHINE=t113 make image)"
  export -n MACHINE
  MACHINE_CONF="${PRODUCT_DIR}/conf/machine/${MACHINE}.conf"
  [ -f "${MACHINE_CONF}" ] || die "no machine config at ${MACHINE_CONF} (MACHINE=${MACHINE})"
  DEVICETREE_DIR="${PRODUCT_DIR}/recipes-bsp/device-tree/files"

  # shellcheck source=/dev/null
  set -a; source "${conf}"; source "${MACHINE_CONF}"; set +a
}

# byname <name> -> its recipe.sh path (product recipes-*/ + packages/ shadow os/meta).
byname() {
  local n="$1" r
  for r in "${PRODUCT_DIR}"/recipes-*/"${n}"/recipe.sh "${PRODUCT_DIR}"/packages/"${n}"/recipe.sh "${OS_META}"/recipes-*/"${n}"/recipe.sh; do
    [ -f "${r}" ] && { printf '%s' "${r}"; return 0; }
  done
  return 1
}

# resolve <token> -> if PROVIDER_<token> is set (local.conf marks <token> a slot), return that provider
# NAME, verified to declare PKG_PROVIDES=<token>. Otherwise <token> is a concrete recipe -> passthrough.
# (cross-cc -> PROVIDER_cross_cc: hyphens aren't legal in a shell var name.)
resolve() {
  local key="PROVIDER_${1//-/_}" name path
  name="${!key:-}"
  [ -n "${name}" ] || { printf '%s' "$1"; return 0; }
  path="$(byname "${name}")" || die "${key}=${name}: no such recipe"
  case " $(recipe_get "${path}" PKG_PROVIDES) " in
    *" $1 "*) : ;;
    *) die "${key}=${name}: recipe '${name}' does not declare it provides '${1}'" ;;
  esac
  printf '%s' "${name}"
}

# recipe_deps <recipe-path> -> its resolved prerequisite recipe names: PKG_DEPENDS + PKG_HOST_DEPENDS +
# PKG_HOST_DEPENDS_<MEDIA> + the implied compiler edge for a target that links libc, each slot resolved.
# Used by both resolve_dependencies (graph edges) + compute_recipe_stamp (hash fold).
recipe_deps() {
  local path="$1" raw tok out=""
  raw="$(recipe_get "${path}" PKG_DEPENDS) $(recipe_get "${path}" PKG_HOST_DEPENDS) $(recipe_get "${path}" "PKG_HOST_DEPENDS_${MEDIA}")"
  if [ "$(recipe_get "${path}" PKG_CLASS target)" = target ]; then
    case " $(recipe_get "${path}" PKG_DEPENDS) " in *" libc "*) raw="${raw} cross-cc" ;; esac
  fi
  for tok in ${raw}; do out="${out} $(resolve "${tok}")"; done
  printf '%s' "${out# }"
}

# load_env — the FULL build environment for execute-recipe: parse local.conf + machine.conf, resolve the
# recipe name (RECIPE) to its path (RECIPE_PATH), and set the vars recipes/classes read.
load_env() {
  load_config
  RECIPE_PATH="$(byname "${RECIPE}" || true)"
  [ -n "${RECIPE_PATH}" ] && [ -f "${RECIPE_PATH}" ] \
    || die "no recipe for '${RECIPE}' — no recipes-*/ or packages/ dir by that name (typo in PACKAGES or a selection?)"
  BUILD_DIR="${PRODUCT_DIR}/build/${MACHINE}"   # per-machine cache (MACHINE is required — no bare build/)

  # PROVIDER_<slot> vars come straight from local.conf (sourced + exported in load_config); recipes read
  # them ([ "${PROVIDER_libc}" = musl ]) and a path is byname "$PROVIDER_x". No resolution loop here —
  # recipe_deps resolves each recipe's own slots (scoped), so a mis-set slot only fails a build that uses it.

  # toolchain scalars (the compiler is cross-cutting — CROSS_COMPILE threads into every compile)
  CROSS_COMPILE="${CROSS_COMPILE:-$(recipe_get "$(byname "${PROVIDER_cross_cc:-}")" PKG_HOST_CC_PREFIX)}"
  [ -n "${CROSS_COMPILE}" ] || die "CROSS_COMPILE empty: the cross-cc provider (PROVIDER_cross_cc) is unset or has no PKG_HOST_CC_PREFIX"
  TOOLCHAIN_DIR="${BUILD_DIR}/${PROVIDER_cross_cc}"
  LIBC_TC_DIR="${BUILD_DIR}/${PROVIDER_cross_cc_initial}"

  # derived paths (a fixed function of BUILD_DIR). DOWNLOAD_DIR is SHARED across configs (tarballs are
  # config-independent) — it stays at build/downloads, not under the per-config BUILD_DIR.
  DOWNLOAD_DIR="${PRODUCT_DIR}/build/downloads"
  OUTPUT_DIR="${BUILD_DIR}/output"
  PYENV_DIR="${BUILD_DIR}/pyenv"
  HOSTMAKE_DIR="${BUILD_DIR}/hostmake"
  HOSTTOOLS_DIR="${BUILD_DIR}/hosttools"
  OS_STAMPS="${BUILD_DIR}/.os/stamps"
  OS_SIGS="${BUILD_DIR}/.os/sigs"
  HOSTTOOLS_FARM="${BUILD_DIR}/.os/hosttools-farm"

  # libc staging (link-keyed — producer + consumers agree here)
  local link="${LINKAGE:-${PKG_LINK:-static}}"
  LIBC_STAGE_DIR="${BUILD_DIR}/libc/stage-${PROVIDER_libc}-${link}"
  STAGE_INC="${BUILD_DIR}/libc/include"

  # this recipe's build context, keyed by RECIPE / RECIPE_PATH + the link mode
  PKG_LINK="${PKG_LINK:-${LINKAGE:-static}}"
  RECIPE_DIR="$(cd "$(dirname "${RECIPE_PATH}")" && pwd)"
  # LAYER_FILES — this recipe's product-layer files/ dir (its scattered BSP inputs). A shared recipe
  # under os/meta/ mirrors to the product layer (recipes-kernel/linux -> ${PRODUCT_DIR}/recipes-kernel/linux/files);
  # a product-layer recipe uses its own files/. Not exported — recipes read it in-shell.
  case "${RECIPE_DIR}" in
    "${OS_META}"/*) LAYER_FILES="${PRODUCT_DIR}/${RECIPE_DIR#${OS_META}/}/files" ;;  # shared recipe -> product-layer mirror
    *)              LAYER_FILES="${RECIPE_DIR}/files" ;;                              # product-layer recipe -> its own files/
  esac
  STAGE="${BUILD_DIR}/rootfs/stage"
  PKG_DEST="${BUILD_DIR}/rootfs/pkgstage/${RECIPE}"
  RECIPE_SCRATCH="${BUILD_DIR}/scratch/${RECIPE}"
  PROVIDER_RECIPE="${RECIPE_PATH}"

  # reset the inherit/require accumulators for this recipe (they feed compute_recipe_stamp). machine.conf's
  # ROOTFS_ARCH_FLAGS (+ everything else it sets) is already exported by load_config's `set -a`.
  _INHERITED_CLASSES=""
  _REQUIRED_INCS=""

  export BUILD_DIR CROSS_COMPILE RECIPE_PATH \
         TOOLCHAIN_DIR LIBC_TC_DIR DOWNLOAD_DIR OUTPUT_DIR PYENV_DIR HOSTMAKE_DIR HOSTTOOLS_DIR \
         OS_STAMPS OS_SIGS HOSTTOOLS_FARM LIBC_STAGE_DIR STAGE_INC \
         PKG_LINK RECIPE_DIR STAGE PKG_DEST PROVIDER_RECIPE
  # local.conf's knobs/providers + machine.conf's board facts are already exported by load_config's set -a;
  # the engine enumerates no product knob.
}

# build_hosttools_farm — provision the HOSTTOOLS allowlist (engine/hosttools.txt) into a farm of symlinks
# (keyed by the file's hash, so an edit reprovisions). Required tools die if missing; optional ones are
# symlinked only if present. A `name:min` entry also version-gates the tool (check_tool_version).
build_hosttools_farm() {
  local file="${OS_ENGINE}/hosttools.txt" key keyfile mode="" line t min p missing="" required="" optional=""
  [ -f "${file}" ] || die "hosttools: ${file} not found"
  while IFS= read -r line || [ -n "${line}" ]; do
    line="${line%%#*}"; line="${line//[[:space:]]/}"   # strip comment + whitespace (each tool is one token)
    [ -z "${line}" ] && continue
    case "${line}" in
      '[required]') mode=required ;;
      '[optional]') mode=optional ;;
      *) [ "${mode}" = required ] && required="${required} ${line}" || optional="${optional} ${line}" ;;
    esac
  done < "${file}"

  key="farmv5-$(sha256sum "${file}" | cut -d' ' -f1)"
  keyfile="${HOSTTOOLS_FARM}/.key"
  [ "$(cat "${keyfile}" 2>/dev/null || true)" = "${key}" ] && return 0
  rm -rf "${HOSTTOOLS_FARM}"; mkdir -p "${HOSTTOOLS_FARM}"
  # type -P, NOT command -v: a shell builtin (true/pwd/printf) makes command -v print a bare word -> ln -s true true self-loop.
  for t in ${required}; do
    min=""; case "$t" in *:*) min="${t#*:}"; t="${t%%:*}" ;; esac
    if p="$(type -P "$t" 2>/dev/null)"; then ln -s "$p" "${HOSTTOOLS_FARM}/$t"; check_tool_version "$t" "$min"
    else missing="${missing} $t"; fi
  done
  [ -z "${missing}" ] || die "host is missing required tool(s):${missing}
  (Debian/Ubuntu: apt install build-essential binutils git curl xz-utils)"
  for t in ${optional}; do
    min=""; case "$t" in *:*) min="${t#*:}"; t="${t%%:*}" ;; esac
    if p="$(type -P "$t" 2>/dev/null)"; then ln -s "$p" "${HOSTTOOLS_FARM}/$t"; check_tool_version "$t" "$min"; fi
  done
  printf '%s' "${key}" > "${keyfile}"
}

# check_tool_version <tool> <min> — die if the host tool's --version is older than min. No-op when min is
# empty or unparseable (best-effort, like Yocto's sanity check).
check_tool_version() {
  local tool="$1" min="$2" have
  [ -n "${min}" ] || return 0
  have="$("${tool}" --version 2>&1 | head -n1 | grep -oE '[0-9]+(\.[0-9]+)+' | head -n1 || true)"
  [ -n "${have}" ] || { log "host: could not read ${tool} version; assuming OK"; return 0; }
  [ "$(printf '%s\n%s\n' "${min}" "${have}" | sort -V | head -n1)" = "${min}" ] \
    || die "host: ${tool} ${have} is older than the required ${min}"
}

# setup_path — scrub PATH to os's built-tool dirs (first, so they shadow the host) + the HOSTTOOLS farm.
setup_path() {
  # LIBC_TC_DIR/bin (stage-1) last: the libc builds before stage-2 exists + invokes cross-ar by bare name.
  local d p=""
  for d in "${HOSTMAKE_DIR}/bin" "${TOOLCHAIN_DIR}/bin" "${LIBC_TC_DIR:+${LIBC_TC_DIR}/bin}" "${HOSTTOOLS_FARM}"; do
    [ -n "$d" ] && [ -d "$d" ] || continue
    case ":$p:" in *":$d:"*) ;; *) p="${p:+$p:}$d" ;; esac
  done
  PATH="$p"; export PATH
}

setup_build_env() {
  load_env
  build_hosttools_farm  # provision + version-gate host tools while PATH is still the host's
  setup_path
}

# compute_recipe_stamp — hash the recipe dir + classes + includes + source + declared var/file deps, then
# fold each dep's recorded stamp so a bump ripples. Sets _recipe_stamp (the value) + publishes it as this
# recipe's sig (for dependents to fold). The engine itself is NOT hashed (like Yocto trusting bitbake-core):
# its env-setup + task-order logic changes rarely and is a clean-the-world edit.
compute_recipe_stamp() {
  local this_recipe_hash dependency_recipe_hashes dep
  this_recipe_hash="$(
    {
      ( cd "${RECIPE_DIR}" && find . -type f -exec sha256sum {} + 2>/dev/null | sort )
      for dep in ${_INHERITED_CLASSES}; do [ -f "${dep}" ] && { printf '# %s\n' "${dep##*/}"; cat "${dep}"; }; done
      for dep in ${_REQUIRED_INCS}; do [ -f "${dep}" ] && { printf '# %s\n' "${dep##*/}"; cat "${dep}"; }; done
      [ "${PKG_FETCH:-local}" = local ] \
        && ( cd "${REPO_ROOT}" && find "${PKG_SOURCE}" -type f -not -path '*/build/*' -exec sha256sum {} + 2>/dev/null | sort )
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
  _recipe_stamp="$(
    {
      printf '%s\n' "${this_recipe_hash}"
      printf '%s\n' "${dependency_recipe_hashes}"
    } | sha256sum | cut -d' ' -f1
  )"

  mkdir -p "${OS_SIGS}"
  printf '%s' "${_recipe_stamp}" > "${OS_SIGS}/${RECIPE}.recipehash"
}

# skip_if_built — cache gate: a recipe is up to date iff its stamp file records the current recipe stamp.
# We trust the stamp (like Yocto's sigdata); a hand-deleted artifact isn't self-healed — run clean.
skip_if_built() {
  compute_recipe_stamp
  if [ "$(cat "${OS_STAMPS}/${RECIPE}" 2>/dev/null)" = "${_recipe_stamp}" ]; then
    log "cached — up to date (recipe stamp ${_recipe_stamp:0:12})"; exit 0
  fi
}

mark_built() {
  mkdir -p "${OS_STAMPS}"
  printf '%s' "${_recipe_stamp}" > "${OS_STAMPS}/${RECIPE}"
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

# resolve-dependencies <recipe> -> recipe_deps + the `make` barrier (all recipes but make itself).
resolve_dependencies() {
  # MACHINE is required only to BUILD. Stay quiet at parse time (Make runs this per recipe) when it's
  # unset — execute-recipe's load_config dies loud, once, when a build is actually attempted. This also
  # lets MACHINE-less goals (clean, or canvas/flash which set MACHINE for their inner $(MAKE)) parse cleanly.
  [ -n "${MACHINE:-}" ] || return 0
  load_config
  local recipe="$1" path deps
  path="$(byname "${recipe}")" || return 0
  deps="$(recipe_deps "${path}")"
  [ "${recipe}" = make ] && printf '%s\n' "${deps}" || printf 'make %s\n' "${deps}"
}

# execute-recipe <recipe> -> build one recipe end to end.
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
