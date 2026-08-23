# projects/gameboy-v3/packages/console/recipe.sh — the gameboy-v3 "canvas" console
# userspace (compositor canvasd + appletd + launcher + powerd + native games) as a
# PRODUCT-LOCAL package. It lives HERE, not in forge/packages/ — forge finds it via the
# product-package search path (resolve.mk _pkg_recipe: product-first, then the forge
# catalog — forge's bblayers / BR2_EXTERNAL equivalent). Built from projects/gameboy-v3/src/
# by its own Makefile, cross-compiled against the SELECTED libc, installed into /usr/bin.
#
# Needs a COMPLETE libc + mainline drivers (DRM/evdev/ALSA), so build the console flavor:
#   make console                                          # mainline + musl + this package
#   make image KERNEL=mainline LIBC=musl PACKAGES="busybox console"
#
# Inline do_build/do_install (no dedicated class yet — extract a `make-install` class if a
# SECOND such package appears; forge's uboot recipe likewise carries inline do_*).
PKG_NAME=console

PKG_FETCH=local
PKG_SOURCE=projects/gameboy-v3/src     # relative to REPO_ROOT (the git root)
PKG_DEPENDS=libc                       # link the selected libc + rebuild on its change
PKG_INSTALL=/usr/bin
PKG_ARTIFACT=stage:                    # artifact = this package's pkgstage dir (cacheable)

# do_fetch = base default (PKG_FETCH=local -> PKG_SRC_DIR = $REPO_ROOT/$PKG_SOURCE).

do_build() {
  : "${PKG_SRC_DIR:?console do_build: PKG_SRC_DIR unset}"
  : "${NODE_SCRATCH:?}"; : "${LIBC_CC_PROFILE:?console do_build: LIBC_CC_PROFILE unset (forge.conf)}"
  # shellcheck source=/dev/null
  source "${LIBC_CC_PROFILE}"          # -> PKG_CC / PKG_CFLAGS / PKG_LDFLAGS for the SELECTED libc
  local O="${NODE_SCRATCH}/build"; mkdir -p "${O}"
  echo "  [console] make (LIBC=${LIBC}/${PKG_LINK:-static})"
  make --no-print-directory -C "${PKG_SRC_DIR}" O="${O}" \
       CC="${PKG_CC}" CFLAGS="${PKG_CFLAGS}" LDFLAGS="${PKG_LDFLAGS}"
}

do_install() {
  : "${PKG_DEST:?console do_install: PKG_DEST unset}"; : "${NODE_SCRATCH:?}"; : "${PKG_SRC_DIR:?}"
  : "${LIBC_CC_PROFILE:?}"
  # shellcheck source=/dev/null
  source "${LIBC_CC_PROFILE}"
  local O="${NODE_SCRATCH}/build"
  rm -rf "${PKG_DEST}"; mkdir -p "${PKG_DEST}"
  make --no-print-directory -C "${PKG_SRC_DIR}" O="${O}" install \
       CC="${PKG_CC}" CFLAGS="${PKG_CFLAGS}" LDFLAGS="${PKG_LDFLAGS}" \
       DESTDIR="${PKG_DEST}" BINDIR="${PKG_INSTALL:-/usr/bin}"
  echo "  [console] installed into ${PKG_INSTALL:-/usr/bin} (pkgstage)"

  # Ship the supervision descriptor for EVERY supported init (Yocto multi-init model: authored files,
  # no synthesis; the active init reads its own, the others sit inert — harmless, like a .service file
  # on a sysvinit image). This keeps the package init-agnostic + cacheable (its output doesn't depend
  # on the INIT selection). runit reads /etc/service/<n>/run; the custom init reads /etc/init/<n>.conf.
  # canvas-launcher + canvas-hello are appletd-spawned clients, not init services. (Add a
  # service/systemd/<n>.service set + an install line here when a systemd INIT provider is added.)
  local svc
  for svc in canvasd appletd powerd; do
    install -D -m 0755 "${RECIPE_DIR}/service/runit/${svc}/run" "${PKG_DEST}/etc/service/${svc}/run"    # INIT=runit
    install -D -m 0644 "${RECIPE_DIR}/service/init/${svc}.conf" "${PKG_DEST}/etc/init/${svc}.conf"       # INIT=custom
    echo "  [console] service ${svc}: /etc/service/${svc}/run + /etc/init/${svc}.conf"
  done
}
