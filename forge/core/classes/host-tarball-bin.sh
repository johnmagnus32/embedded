#!/usr/bin/env bash
# host-tarball-bin.sh — the `host-tarball-bin` host build STYLE: for a PREBUILT binary
# tarball (today only the Bootlin arm-* cross toolchains) — fetch+verify+extract, then
# sanity-check the compiler runs and emits 32-bit ARM. SHA mismatch is fatal (never run an
# unverified toolchain).
#
# Inputs (recipe facts, exported by host.sh): PKG_NAME, PKG_SOURCE (tarball), PKG_SITE,
#   PKG_SHA256, PKG_HOST_DEST (extract dir), PKG_HOST_CC_PREFIX (the <triple>- to check
#   <triple>gcc), FORCE.
# do_fetch/do_install: a host tool self-fetches inside do_build (via forge_fetch_file) and
# installs into its host prefix there — so these are no-ops (the uniform contract still calls them).

PKG_TARGET_INDEPENDENT=1   # built with host cc: omit cross-toolchain/arch/board from the taskhash

do_fetch()   { :; }
do_install() { :; }

do_build() {
  : "${PKG_NAME:?host-tarball-bin: PKG_NAME unset}"
  : "${PKG_SOURCE:?${PKG_NAME}: PKG_SOURCE (tarball) unset}"
  : "${PKG_SITE:?${PKG_NAME}: PKG_SITE unset}"
  : "${PKG_SHA256:?${PKG_NAME}: PKG_SHA256 unset}"
  : "${PKG_HOST_DEST:?${PKG_NAME}: PKG_HOST_DEST (extract dir) unset}"
  local prefix="${PKG_HOST_CC_PREFIX:-}"
  local tarball="${PKG_SOURCE}" dest="${PKG_HOST_DEST}"
  local DECOMP
  case "${tarball}" in
    *.tar.xz)  DECOMP=xz ;;
    *.tar.bz2) DECOMP=bzip2 ;;
    *.tar.gz)  DECOMP=gzip ;;
    *) die "${PKG_NAME}: unknown tarball extension for ${tarball}" ;;
  esac
  for tool in curl sha256sum tar file "${DECOMP}"; do
    command -v "$tool" >/dev/null 2>&1 || die "${PKG_NAME}: required tool '$tool' not on PATH"
  done

  mkdir -p "${BUILD_DIR}"
  local tb="${DOWNLOAD_DIR}/${tarball}"
  if [ "${FORCE:-0}" -eq 1 ]; then
    log "${PKG_NAME}: --force: removing cached tarball + extracted tree"
    rm -f "${tb}"; rm -rf "${dest}"
  fi

  tb="$(forge_fetch_file "${tarball}" "${PKG_SITE}" "${PKG_SHA256}")" \
    || die "${PKG_NAME}: fetch/verify failed. If you bumped the version, update PKG_SHA256 in its recipe."

  # extract (normalize the dir name so the on-PATH layout is stable). ALWAYS re-extract when this
  # runs: the uniform taskhash cache (run-recipe.sh) already skipped the whole node when the tree
  # was up to date, so reaching here means a (re)provision is wanted — and a stale tree from a
  # prior version must NOT be kept.
  local cc_bin="${dest}/bin/${prefix}gcc"
  log "${PKG_NAME}: extracting into ${dest}"
  rm -rf "${dest}" "${dest}.tmp"; mkdir -p "${dest}.tmp"
  tar -xf "${tb}" -C "${dest}.tmp" --strip-components=1
  mv "${dest}.tmp" "${dest}"

  # sanity check: the cross compiler runs AND targets 32-bit ARM
  if [ -n "${prefix}" ]; then
    [ -x "${cc_bin}" ] || die "${PKG_NAME}: extracted but ${cc_bin} missing — tarball layout unexpected"
    export PATH="${dest}/bin:${PATH}"
    log "${PKG_NAME}: $("${prefix}gcc" --version | head -1)"
    local TMP; TMP="$(mktemp -d)"
    printf 'int main(void){return 0;}\n' > "${TMP}/t.c"
    "${prefix}gcc" -c "${TMP}/t.c" -o "${TMP}/t.o" || { rm -rf "${TMP}"; die "${PKG_NAME}: test compile failed"; }
    local ARCH_LINE; ARCH_LINE="$(file "${TMP}/t.o" 2>/dev/null || true)"; rm -rf "${TMP}"
    case "${ARCH_LINE}" in
      *ARM*) log "${PKG_NAME}: ok -> ${ARCH_LINE#*: }" ;;
      *) die "${PKG_NAME}: test object is not ARM (${ARCH_LINE})" ;;
    esac
  fi
}
