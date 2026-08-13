# packages/busybox/recipe.sh — BusyBox, as a PACKAGE. Uses the shared kconfig class for the
# configure mechanism, then defines its own build/install inline.
PKG_NAME=busybox
PKG_FETCH=tarball
PKG_VERSION=1.36.1
PKG_SITE=https://busybox.net/downloads
PKG_SOURCE=busybox-1.36.1.tar.bz2
PKG_SHA256=b8cc24c9574d809e7279c3be349795c5d5ceb6fdf19ca709f80cde50e47de314
PKG_DEPENDS=libc

# ADVISORY only (not enforced): BusyBox needs a COMPLETE libc (buffered stdio, getopt_long,
# termios, regex, glob …) beyond what gv3libc implements, so in practice it's built on musl.
# LIBC=custom proceeds to the real link errors, which double as the gv3libc port worklist.
PKG_LIBC=musl

# CONFIG_TC uses TCA_CBQ_MAX, removed in kernel headers >=6.8 and unfixed upstream.
PKG_KCONFIG_DEFCONFIG=defconfig
PKG_KCONFIG_DISABLE=CONFIG_TC

PKG_ARTIFACT=stage:

inherit kconfig

do_build() {
  : "${PKG_NAME:?}"; : "${PKG_SRC_DIR:?}"
  : "${ROOTFS_CROSS_COMPILE:?busybox do_build: ROOTFS_CROSS_COMPILE unset}"
  local PKG_LINK="${PKG_LINK:-static}"
  local DEFCONFIG="${PKG_KCONFIG_DEFCONFIG:-defconfig}" DISABLE="${PKG_KCONFIG_DISABLE:-}"

  cd "${PKG_SRC_DIR}"
  echo "  [busybox] ${DEFCONFIG} (${PKG_LINK}, musl)"

  # Fixup: static/dynamic toggle, disable requested symbols, pin the cross prefix. Upstream ships
  # only interactive `oldconfig`, so that's the normalizer (fed EOF).
  kfixup() {
    if [ "${PKG_LINK}" = static ]; then
      sed -i -e 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
    else
      sed -i -e 's/^CONFIG_STATIC=y/# CONFIG_STATIC is not set/' .config
    fi
    local sym
    for sym in ${DISABLE}; do
      sed -i -e "s/^${sym}=y/# ${sym} is not set/" .config
    done
    sed -i "s|^CONFIG_CROSS_COMPILER_PREFIX=.*|CONFIG_CROSS_COMPILER_PREFIX=\"${ROOTFS_CROSS_COMPILE}\"|" .config
  }
  kconfig_configure "${DEFCONFIG}" kfixup oldconfig

  if [ "${PKG_LINK}" = static ]; then
    grep -q '^CONFIG_STATIC=y' .config || { echo "CONFIG_STATIC=y did not take" >&2; return 1; }
  else
    grep -q '^CONFIG_STATIC=y' .config && { echo "CONFIG_STATIC still on for a dynamic build" >&2; return 1; }
  fi

  echo "  [busybox] cross-compiling with musl (-j$(nproc))"
  make clean >/dev/null
  make -j"$(nproc)" CROSS_COMPILE="${ROOTFS_CROSS_COMPILE}" >/dev/null
}

do_install() {
  : "${PKG_NAME:?}"; : "${PKG_SRC_DIR:?}"; : "${PKG_DEST:?busybox do_install: PKG_DEST unset}"
  : "${ROOTFS_CROSS_COMPILE:?busybox do_install: ROOTFS_CROSS_COMPILE unset}"
  local PKG_LINK="${PKG_LINK:-static}"
  cd "${PKG_SRC_DIR}"

  # Wipe first so a rebuild never keeps a stale applet symlink.
  rm -rf "${PKG_DEST}"; mkdir -p "${PKG_DEST}"
  make CONFIG_PREFIX="${PKG_DEST}" CROSS_COMPILE="${ROOTFS_CROSS_COMPILE}" install >/dev/null

  # Verify only (installing the loader is the rootfs recipe's job): confirm the binary requests
  # the interpreter the rootfs stages.
  if [ "${PKG_LINK}" = dynamic ]; then
    local WANT
    WANT="$(${ROOTFS_CROSS_COMPILE}readelf -l "${PKG_SRC_DIR}/busybox" 2>/dev/null | sed -n 's/.*Requesting program interpreter: \(.*\)\]/\1/p')"
    [ "${WANT}" = "/lib/ld-musl-armhf.so.1" ] || { echo "unexpected interpreter '${WANT}'" >&2; return 1; }
  fi
}
