#!/usr/bin/env bash
# host-cc.sh — the `host-cc` host build STYLE: fetch a single SHA-pinned .c and compile it
# with the host `cc`. Today's one consumer is gen_init_cpio, whose source is one file in the
# Linux kernel tree (no standalone release) — we fetch+pin it rather than vendoring a copy.
#
# Inputs (recipe facts, exported by host.sh): PKG_NAME, PKG_SITE, PKG_SOURCE (saved basename),
#   PKG_SHA256, PKG_HOST_BIN (output path). Optional PKG_SITE_MIRROR, PKG_SOURCE_QUERY (a
#   ?query the URL needs but the saved name must not, e.g. cgit's "?h=<tag>").
# do_fetch/do_install: a host tool self-fetches inside do_build (via forge_fetch_file) and
# installs into its host prefix there — so these are no-ops (the uniform contract still calls them).

PKG_TARGET_INDEPENDENT=1   # built with host cc: omit cross-toolchain/arch/board from the taskhash

do_fetch()   { :; }
do_install() { :; }

do_build() {
  : "${PKG_NAME:?host-cc: PKG_NAME unset}"
  : "${PKG_SITE:?${PKG_NAME}: PKG_SITE unset}"
  : "${PKG_SOURCE:?${PKG_NAME}: PKG_SOURCE (basename) unset}"
  : "${PKG_SHA256:?${PKG_NAME}: PKG_SHA256 unset}"
  : "${PKG_HOST_BIN:?${PKG_NAME}: PKG_HOST_BIN (output path) unset}"
  command -v cc >/dev/null 2>&1 || die "${PKG_NAME}: need 'cc' on PATH"

  local src
  src="$(forge_fetch_file "${PKG_SOURCE}" "${PKG_SITE}" "${PKG_SHA256}" \
           "${PKG_SITE_MIRROR:-}" "${PKG_SOURCE_QUERY:-}")" \
    || die "${PKG_NAME}: fetch failed. If you bumped the version, update PKG_SHA256 in its recipe."

  log "${PKG_NAME}: cc -O2 ${PKG_SOURCE} -> ${PKG_HOST_BIN}"
  mkdir -p "$(dirname "${PKG_HOST_BIN}")"
  cc -O2 -o "${PKG_HOST_BIN}" "${src}" || die "${PKG_NAME}: compile failed"
  [ -x "${PKG_HOST_BIN}" ] || die "${PKG_NAME}: produced no binary at ${PKG_HOST_BIN}"
}
