#!/usr/bin/env bash
# host-autotools.sh — the `host-autotools` host build STYLE: fetch+verify+extract a tarball,
# then `./configure --prefix=<dest> [flags] && make && make install`. Consumers: libconfuse,
# genimage, GNU make — all ship a pre-generated ./configure (no autoreconf).
#
# Inputs (recipe facts, exported by host.sh): PKG_NAME, PKG_SOURCE (tarball), PKG_SITE,
#   PKG_SHA256, PKG_HOST_DEST (install --prefix). Optional: PKG_HOST_CONFIGURE_FLAGS,
#   PKG_HOST_CONFIGURE_ENV (an env prefix so a dep's headers/libs resolve), PKG_SITE_MIRROR,
#   PKG_HOST_VERIFY_BIN (a binary that must exist afterward).
# do_fetch/do_install: a host tool self-fetches inside do_build (via forge_fetch_file) and
# installs into its host prefix there — so these are no-ops (the uniform contract still calls them).

PKG_TARGET_INDEPENDENT=1   # built with host cc: omit cross-toolchain/arch/board from the taskhash

do_fetch()   { :; }
do_install() { :; }

do_build() {
  : "${PKG_NAME:?host-autotools: PKG_NAME unset}"
  : "${PKG_SOURCE:?${PKG_NAME}: PKG_SOURCE (tarball) unset}"
  : "${PKG_SITE:?${PKG_NAME}: PKG_SITE unset}"
  : "${PKG_SHA256:?${PKG_NAME}: PKG_SHA256 unset}"
  : "${PKG_HOST_DEST:?${PKG_NAME}: PKG_HOST_DEST (install prefix) unset}"
  for tool in curl sha256sum tar gcc make; do
    command -v "$tool" >/dev/null 2>&1 || die "${PKG_NAME}: need '$tool' on PATH"
  done

  local tb
  tb="$(forge_fetch_file "${PKG_SOURCE}" "${PKG_SITE}" "${PKG_SHA256}" "${PKG_SITE_MIRROR:-}")" \
    || die "${PKG_NAME}: fetch/verify failed"

  local work="${BUILD_DIR}/${PKG_NAME}-src.tmp"
  rm -rf "${work}"; mkdir -p "${work}"
  tar -xf "${tb}" -C "${work}" --strip-components=1

  log "${PKG_NAME}: ./configure --prefix=${PKG_HOST_DEST} ${PKG_HOST_CONFIGURE_FLAGS:-}"
  # shellcheck disable=SC2086  (PKG_HOST_CONFIGURE_ENV/FLAGS word-split is intentional)
  ( cd "${work}" \
      && env ${PKG_HOST_CONFIGURE_ENV:-} ./configure --prefix="${PKG_HOST_DEST}" ${PKG_HOST_CONFIGURE_FLAGS:-} >/dev/null 2>&1 \
      && make -j"$(nproc)" >/dev/null 2>&1 \
      && make install >/dev/null 2>&1 ) \
    || die "${PKG_NAME}: build failed"
  rm -rf "${work}"

  # optional post-build assertion: the recipe names a binary that must now exist
  if [ -n "${PKG_HOST_VERIFY_BIN:-}" ]; then
    [ -x "${PKG_HOST_DEST}/bin/${PKG_HOST_VERIFY_BIN}" ] \
      || die "${PKG_NAME}: build produced no ${PKG_HOST_DEST}/bin/${PKG_HOST_VERIFY_BIN}"
    log "${PKG_NAME}: built -> ${PKG_HOST_DEST}/bin/${PKG_HOST_VERIFY_BIN}"
  fi
}
