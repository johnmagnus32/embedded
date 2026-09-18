# providers/init/custom/recipe.sh — the from-scratch init + service supervisor (INIT=custom), built
# from the top-level init/ source (the event-driven C supervisor; see init/PLAN.md). Follows the
# selected LINKAGE via the libc cc-profile, exactly like a package. Mainline-only for now (needs
# signalfd/timerfd/epoll); the minimal kernel-agnostic PID-1 is INIT=shell.
PKG_NAME=init
PKG_CLASS=target
PKG_PROVIDES=init
PKG_FETCH=local
PKG_SOURCE=init                        # top-level init/ (relative to REPO_ROOT)
PKG_DEPENDS=libc                       # link the selected libc + rebuild on its change

do_build() {
  : "${PKG_SRC_DIR:?init do_build: PKG_SRC_DIR unset}"; : "${RECIPE_SCRATCH:?}"
  : "${PROVIDER_libc:?init do_build: PROVIDER_libc unset (recipe env)}"
  # shellcheck source=/dev/null
  source "$(dirname "$(byname "${PROVIDER_libc}")")/cc-profile.sh"   # -> PKG_CC / PKG_CFLAGS / PKG_LDFLAGS for the SELECTED libc + link
  local O="${RECIPE_SCRATCH}/build"; mkdir -p "${O}"
  echo "  [init] make (${PROVIDER_libc}/${PKG_LINK:-static})"
  # LIBC_CRT/LIBC_LIB are empty on musl (it supplies crt+libc); on the custom -nostdlib
  # libc they are crt0.S.o + libc.a, which the Makefile links (crt0 before, libc.a after).
  make --no-print-directory -C "${PKG_SRC_DIR}" O="${O}" \
       CC="${PKG_CC}" CFLAGS="${PKG_CFLAGS}" LDFLAGS="${PKG_LDFLAGS}" \
       LIBC_CRT="${LIBC_CRT}" LIBC_LIB="${LIBC_LIB}"
}

do_install() {
  : "${PKG_DEST:?init do_install: PKG_DEST unset}"; : "${RECIPE_SCRATCH:?}"; : "${PKG_SRC_DIR:?}"
  rm -rf "${PKG_DEST}"                  # its own pkgstage dir (pkgstage/<recipe>) — start clean
  local O="${RECIPE_SCRATCH}/build"
  # Installs /init (the binary) — the initramfs entry point. NOT /sbin/init: busybox owns that as a
  # symlink to itself, and merging a real file over it would deref + clobber busybox.
  make --no-print-directory -C "${PKG_SRC_DIR}" O="${O}" install DESTDIR="${PKG_DEST}"
  echo "  [init] installed /init (pkgstage)"
}
