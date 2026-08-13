#!/usr/bin/env bash
# classes/base.sh — the BASE class every recipe implicitly inherits (Yocto's base.bbclass).
# run-recipe.sh `inherit base` for EVERY node BEFORE sourcing the recipe, so both the default do_*
# tasks AND the fetch helpers below are universally in scope — for the recipe and for every class it
# later inherits (host classes call forge_fetch_file directly). A recipe (or a class it inherits)
# overrides any do_* last-definition-wins. do_build/do_install have no universal default (a recipe
# always binds them via a class or inline), so base leaves them unset — an unbound do_build is a
# recipe bug, surfaced by the shell.
#
# WHY THE FETCH MECHANISM LIVES HERE (not in run-recipe.sh): the runner is the ORCHESTRATOR — it
# keeps only what it needs before `inherit base` (recipe_get, log/die, _prepend_path, inherit). The
# fetch mechanism is the default do_fetch's IMPLEMENTATION, so it belongs with the task, in the base
# class — the base-class model (override do_fetch to fetch differently). It is first used in
# run_tasks -> do_fetch, which runs after `inherit base`, so nothing pins it to the runner.
#
# Sourced into the node shell with the env set_node_env established: RECIPE, NODE_SCRATCH, REPO_ROOT,
# BUILD_DIR, DOWNLOAD_DIR, recipe_get, log/die.

# ---- fetch primitives (content pinned by SHA/tag; the URL is just availability) ---------------
# Shared by do_fetch's dispatch below AND by host classes that self-fetch a file/tarball into their
# own prefix (they override do_fetch to no-op and call forge_fetch_file from do_build). Since base is
# inherited first, these are in scope for every recipe/class.

# fetch_verify <url> <sha256> <dest_tarball> — download (cached; re-verified), no extraction.
fetch_verify() {
  local url="$1" sha="$2" tb="$3"
  if [ -f "${tb}" ] && echo "${sha}  ${tb}" | sha256sum --check --status; then
    echo "  cached $(basename "${tb}") checksum OK" >&2
    return 0
  fi
  echo "  downloading $(basename "${tb}")" >&2
  mkdir -p "$(dirname "${tb}")"
  curl -fL --retry 3 --retry-delay 2 --connect-timeout 20 -o "${tb}.part" "${url}" \
    || { echo "fetch_verify: download failed: ${url}" >&2; return 1; }
  mv -f "${tb}.part" "${tb}"
  echo "${sha}  ${tb}" | sha256sum --check --status \
    || { echo "fetch_verify: SHA256 mismatch for $(basename "${tb}") (expected ${sha}, got $(sha256sum "${tb}" | cut -d' ' -f1))" >&2; return 1; }
  echo "  SHA256 OK" >&2
}

# git_clone_pinned <dest> <tag> <primary> [mirror] — shallow-clone a tag, retrying primary
# (transient 502s) then mirror.
git_clone_pinned() {
  local dest="$1" tag="$2" primary="$3" mirror="${4:-}"
  local url attempt
  for url in "${primary}" "${mirror}"; do
    [ -n "${url}" ] || continue
    for attempt in 1 2 3; do
      echo "  git clone ${tag} from ${url} (attempt ${attempt})" >&2
      if git clone --depth 1 --branch "${tag}" "${url}" "${dest}" 2>&1; then
        return 0
      fi
      rm -rf "${dest}"           # clean partial checkout before retry
      sleep 3
    done
    [ -n "${mirror}" ] && echo "  primary failed; trying mirror" >&2
  done
  return 1
}

# clone_or_reuse_pinned <src> <tag> <primary> <mirror> <label> — reuse a tag-matched checkout,
# else clone. A tag MISMATCH is fatal (re-build with CLEAN=1), never a silent wrong-version build.
clone_or_reuse_pinned() {
  local src="$1" tag="$2" primary="$3" mirror="$4" label="$5"
  local _log; if declare -F log >/dev/null 2>&1; then _log=log; else _log=echo; fi
  if [ -d "${src}/.git" ]; then
    local got; got="$(git -C "${src}" describe --tags 2>/dev/null || echo '?')"
    if [ "${got}" != "${tag}" ]; then
      echo "[${label}] ERROR: existing checkout is '${got}', want '${tag}'. Re-build with CLEAN=1 to re-clone." >&2
      return 1
    fi
    "${_log}" "reusing ${label} checkout at ${tag}"
  else
    "${_log}" "cloning ${label} ${tag} (shallow) — this is the slow part"
    git_clone_pinned "${src}" "${tag}" "${primary}" "${mirror}" \
      || { echo "[${label}] ERROR: git clone failed (tried ${primary} and mirror)" >&2; return 1; }
  fi
}

# forge_fetch_file <basename> <site> <sha256> [mirror] [url-query] — download+verify ONE artifact
# into DOWNLOAD_DIR (site then mirror); echo the cached path (no extract). Arg-driven so both
# callers share it (do_fetch's tarball arm + host classes). Logs to stderr; stdout = the path.
forge_fetch_file() {
  local name="$1" site="$2" sha="$3" mirror="${4:-}" q="${5:-}"
  : "${name:?forge_fetch_file: basename unset}"
  : "${site:?forge_fetch_file: site url unset (${name})}"
  : "${sha:?forge_fetch_file: sha256 unset (${name})}"
  local dest="${DOWNLOAD_DIR}/${name}"
  if fetch_verify "${site}/${name}${q}" "${sha}" "${dest}" >&2; then
    printf '%s' "${dest}"; return 0
  fi
  [ -n "${mirror}" ] || { echo "forge_fetch_file: ${name} fetch failed (no mirror). If you bumped the version, update PKG_SHA256 in its recipe." >&2; return 1; }
  echo "forge_fetch_file: ${name} primary failed; trying mirror" >&2
  fetch_verify "${mirror}/${name}${q}" "${sha}" "${dest}" >&2 \
    || { echo "forge_fetch_file: ${name} fetch failed from primary AND mirror. If you bumped the version, update PKG_SHA256." >&2; return 1; }
  printf '%s' "${dest}"
}

# do_fetch — the DEFAULT fetch task: resolve THIS node's recipe source onto disk per PKG_FETCH and
# set PKG_SRC_DIR (run_tasks exports it for do_build). This IS the fetch mechanism. Overridden to no-op
# by recipes/classes whose source needs no forge fetch — host classes self-fetch a file/tarball into
# their prefix; prebuilt/none have nothing to fetch. Idempotent.
#   local -> $REPO_ROOT/$PKG_SOURCE | prebuilt|none -> "" | git -> $BUILD_DIR/$PKG_GIT_CHECKOUT | tarball -> $NODE_SCRATCH/src
do_fetch() {
  local fetch src ver primary mirror checkout tb name
  fetch="$(recipe_get "${RECIPE}" PKG_FETCH)"
  [ -z "${fetch}" ] && [ -n "$(recipe_get "${RECIPE}" PKG_SITE)" ] && fetch=tarball
  : "${fetch:?do_fetch: ${RECIPE} has no PKG_FETCH (and no PKG_SITE to imply tarball)}"

  case "${fetch}" in
    local)
      src="${REPO_ROOT}/$(recipe_get "${RECIPE}" PKG_SOURCE)"
      [ -d "${src}" ] || die "do_fetch: local source not found: ${src} (${RECIPE})"
      PKG_SRC_DIR="${src}" ;;

    prebuilt) PKG_SRC_DIR="" ;;   # libc baked into the cross toolchain (musl); no source dir
    none)     PKG_SRC_DIR="" ;;   # an engine step (rootfs|image) composes built inputs; nothing to fetch

    git)
      # The checkout dir is a RECIPE fact (PKG_GIT_CHECKOUT), not an engine role->var table — so a
      # new git provider needs no edit here. Persistent under build/ (reused across builds).
      checkout="$(recipe_get "${RECIPE}" PKG_GIT_CHECKOUT)"
      : "${checkout:?do_fetch: ${RECIPE} PKG_FETCH=git needs PKG_GIT_CHECKOUT (checkout dir under build/)}"
      src="${BUILD_DIR}/${checkout}"
      ver="$(recipe_get "${RECIPE}" PKG_VERSION)"
      primary="$(recipe_get "${RECIPE}" PKG_GIT_URL)"
      mirror="$(recipe_get "${RECIPE}" PKG_GIT_URL_MIRROR)"
      : "${ver:?do_fetch: ${RECIPE} PKG_VERSION unset (git fetch)}"
      : "${primary:?do_fetch: ${RECIPE} PKG_GIT_URL unset (git fetch)}"
      # CLEAN=1 (a make command-line var, present in the recipe shell's env) forces a from-scratch
      # re-fetch: wipe the checkout before cloning. A FETCH concern, so it lives here ONCE for every
      # git recipe — not re-implemented in each provider's do_build.
      if [ "${CLEAN:-0}" = 1 ] && [ -d "${src}" ]; then
        log "CLEAN=1: removing ${src} for a fresh clone"; rm -rf "${src}"
      fi
      clone_or_reuse_pinned "${src}" "${ver}" "${primary}" "${mirror}" "${LAYER}" \
        || die "do_fetch: clone failed for ${RECIPE}"
      PKG_SRC_DIR="${src}" ;;

    tarball)
      : "${NODE_SCRATCH:?do_fetch: NODE_SCRATCH unset (tarball extract dir)}"
      name="$(recipe_get "${RECIPE}" PKG_SOURCE)"
      : "${name:?do_fetch: ${RECIPE} PKG_SOURCE unset (tarball fetch)}"
      tb="$(forge_fetch_file "${name}" "$(recipe_get "${RECIPE}" PKG_SITE)" \
              "$(recipe_get "${RECIPE}" PKG_SHA256)" \
              "$(recipe_get "${RECIPE}" PKG_SITE_MIRROR)" \
              "$(recipe_get "${RECIPE}" PKG_SOURCE_QUERY)")" \
        || die "do_fetch: fetch/verify failed for ${RECIPE}"
      src="${NODE_SCRATCH}/src"; mkdir -p "${src}"
      tar -xf "${tb}" -C "${src}" --strip-components=1
      PKG_SRC_DIR="${src}" ;;

    *)
      die "do_fetch: unknown PKG_FETCH '${fetch}' in ${RECIPE} (want local|prebuilt|git|tarball|none)" ;;
  esac
}
