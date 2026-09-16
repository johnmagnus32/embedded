#!/usr/bin/env bash
# host-cc.sh — the `host-cc` host build STYLE: compile a single SHA-pinned .c with the host `cc`.
# Today's one consumer is gen_init_cpio, whose source is one file in the Linux kernel tree (no
# standalone release) — we pin + fetch just that file. The FRAMEWORK's do_fetch (base.sh) fetches +
# verifies the file declared in PKG_SOURCES (which may carry PKG_MIRROR_<name> + PKG_QUERY_<name>,
# e.g. cgit's ?h=<tag>); do_build compiles it.
#
# Recipe facts: PKG_SOURCES="<name>" + PKG_SRC_<name>=<url> + PKG_SHA_<name> (the .c), PKG_HOST_BIN
#   (output path). do_install is a no-op; do_fetch is base's (declarative PKG_SOURCES).

do_install() { :; }

do_build() {
  : "${PKG_NAME:?host-cc: PKG_NAME unset}"
  : "${PKG_SOURCES:?${PKG_NAME}: PKG_SOURCES unset (declare the .c name + PKG_SRC_/PKG_SHA_)}"
  : "${PKG_HOST_BIN:?${PKG_NAME}: PKG_HOST_BIN (output path) unset}"
  command -v cc >/dev/null 2>&1 || die "${PKG_NAME}: need 'cc' on PATH"

  local src; src="$(pkg_src "${PKG_SOURCES%% *}")"   # base.sh's do_fetch already fetched+verified it
  log "${PKG_NAME}: cc -O2 ${src##*/} -> ${PKG_HOST_BIN}"
  mkdir -p "$(dirname "${PKG_HOST_BIN}")"
  cc -O2 -o "${PKG_HOST_BIN}" "${src}" || die "${PKG_NAME}: compile failed"
  [ -x "${PKG_HOST_BIN}" ] || die "${PKG_NAME}: produced no binary at ${PKG_HOST_BIN}"
}
