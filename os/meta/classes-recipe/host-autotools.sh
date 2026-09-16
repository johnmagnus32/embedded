#!/usr/bin/env bash
# host-autotools.sh — the `host-autotools` host build STYLE: extract a tarball, then
# `./configure --prefix=<dest> [flags] && make && make install`. Consumers: libconfuse, genimage,
# GNU make — all ship a pre-generated ./configure (no autoreconf). The FRAMEWORK's do_fetch (base.sh)
# fetches + SHA-verifies the tarball declared in PKG_SOURCES; do_unpack extracts it; do_build builds it.
#
# Recipe facts: PKG_SOURCES="<name>" + PKG_SRC_<name>=<url> + PKG_SHA_<name> (the tarball),
#   PKG_HOST_DEST (install --prefix). Optional: PKG_HOST_CONFIGURE_FLAGS, PKG_HOST_CONFIGURE_ENV (an
#   env prefix so a dep's headers/libs resolve), PKG_HOST_VERIFY_BIN (a binary that must exist after).
# do_install is a no-op; do_fetch is base's (declarative PKG_SOURCES); do_patch defaults to base's no-op.

do_install() { :; }

# do_unpack — extract the (base-fetched, SHA-verified) tarball into a work dir, set PKG_SRC_DIR. Overrides
# base's default do_unpack (which handles PKG_FETCH=tarball; here the source is a PKG_SOURCES entry).
do_unpack() {
  : "${PKG_NAME:?host-autotools: PKG_NAME unset}"
  : "${PKG_SOURCES:?${PKG_NAME}: PKG_SOURCES unset (declare the tarball name + PKG_SRC_/PKG_SHA_)}"
  command -v tar >/dev/null 2>&1 || die "${PKG_NAME}: need 'tar' on PATH"
  local tb; tb="$(pkg_src "${PKG_SOURCES%% *}")"   # base.sh's do_fetch already fetched+verified it
  PKG_SRC_DIR="${BUILD_DIR}/${PKG_NAME}-src.tmp"
  rm -rf "${PKG_SRC_DIR}"; mkdir -p "${PKG_SRC_DIR}"
  tar -xf "${tb}" -C "${PKG_SRC_DIR}" --strip-components=1
}

do_build() {
  : "${PKG_NAME:?host-autotools: PKG_NAME unset}"
  : "${PKG_HOST_DEST:?${PKG_NAME}: PKG_HOST_DEST (install prefix) unset}"
  : "${PKG_SRC_DIR:?${PKG_NAME}: PKG_SRC_DIR unset (do_unpack should have set it)}"
  for tool in gcc make; do
    command -v "$tool" >/dev/null 2>&1 || die "${PKG_NAME}: need '$tool' on PATH"
  done

  log "${PKG_NAME}: ./configure --prefix=${PKG_HOST_DEST} ${PKG_HOST_CONFIGURE_FLAGS:-}"
  # shellcheck disable=SC2086  (PKG_HOST_CONFIGURE_ENV/FLAGS word-split is intentional)
  ( cd "${PKG_SRC_DIR}" \
      && env ${PKG_HOST_CONFIGURE_ENV:-} ./configure --prefix="${PKG_HOST_DEST}" ${PKG_HOST_CONFIGURE_FLAGS:-} >/dev/null 2>&1 \
      && make -j"$(nproc)" >/dev/null 2>&1 \
      && make install >/dev/null 2>&1 ) \
    || die "${PKG_NAME}: build failed"
  rm -rf "${PKG_SRC_DIR}"

  # optional post-build assertion: the recipe names a binary that must now exist
  if [ -n "${PKG_HOST_VERIFY_BIN:-}" ]; then
    [ -x "${PKG_HOST_DEST}/bin/${PKG_HOST_VERIFY_BIN}" ] \
      || die "${PKG_NAME}: build produced no ${PKG_HOST_DEST}/bin/${PKG_HOST_VERIFY_BIN}"
    log "${PKG_NAME}: built -> ${PKG_HOST_DEST}/bin/${PKG_HOST_VERIFY_BIN}"
  fi
}
