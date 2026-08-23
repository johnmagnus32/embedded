# providers/init/custom/recipe.sh — the from-scratch init + service supervisor (INIT=custom), built
# from the top-level init/ source (the event-driven C supervisor; see init/PLAN.md). Follows the
# selected LINKAGE via the libc cc-profile, exactly like a package. Mainline-only for now (needs
# signalfd/timerfd/epoll); the minimal kernel-agnostic PID-1 is INIT=shell.
PKG_NAME=init
PKG_FETCH=local
PKG_SOURCE=init                        # top-level init/ (relative to REPO_ROOT)
PKG_DEPENDS=libc                       # link the selected libc + rebuild on its change
PKG_ARTIFACT=stage:                    # artifact = this node's pkgstage dir (cacheable)

do_build() {
  : "${PKG_SRC_DIR:?init do_build: PKG_SRC_DIR unset}"; : "${NODE_SCRATCH:?}"
  : "${LIBC_CC_PROFILE:?init do_build: LIBC_CC_PROFILE unset (forge.conf)}"
  # shellcheck source=/dev/null
  source "${LIBC_CC_PROFILE}"          # -> PKG_CC / PKG_CFLAGS / PKG_LDFLAGS for the SELECTED libc + link
  local O="${NODE_SCRATCH}/build"; mkdir -p "${O}"
  echo "  [init] make (LIBC=${LIBC}/${PKG_LINK:-static})"
  make --no-print-directory -C "${PKG_SRC_DIR}" O="${O}" \
       CC="${PKG_CC}" CFLAGS="${PKG_CFLAGS}" LDFLAGS="${PKG_LDFLAGS}"
}

do_install() {
  : "${PKG_DEST:?init do_install: PKG_DEST unset}"; : "${NODE_SCRATCH:?}"; : "${PKG_SRC_DIR:?}"
  rm -rf "${PKG_DEST}"                  # pkgstage/init is shared across init providers — start clean
  local O="${NODE_SCRATCH}/build"
  # Installs /init (the binary) — the initramfs entry point. NOT /sbin/init: busybox owns that as a
  # symlink to itself, and merging a real file over it would deref + clobber busybox.
  make --no-print-directory -C "${PKG_SRC_DIR}" O="${O}" install DESTDIR="${PKG_DEST}"
  echo "  [init] installed /init (pkgstage)"
}
