# recipes-core/linux-libc-headers/recipe.sh — the sanitized Linux UAPI headers (Yocto's
# linux-libc-headers analogue). A from-source libc sysroot needs the kernel's userspace headers
# (linux/*, asm/*, asm-generic/*, …) — a prebuilt cross toolchain bundles them; we install them from
# the Linux source via `make headers_install`. musl (and any from-source libc) depends on this and
# copies the headers into its sysroot, so real userland (busybox's <linux/kd.h>, etc.) compiles.
#
# Shares the Linux git checkout (PKG_GIT_CHECKOUT=linux) with recipes-kernel/linux — pinned to the same
# tag, so a mainline build and a rootfs-only build reuse ONE checkout (clone-once, idempotent). Built on
# the host (class=native): headers are arch-parameterized (ARCH=arm) but board- and toolchain-independent.
PKG_NAME=linux-libc-headers
PKG_CLASS=native
PKG_FETCH=git
PKG_GIT_URL=https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git
PKG_GIT_URL_MIRROR=https://github.com/gregkh/linux.git
PKG_GIT_CHECKOUT=linux
PKG_VERSION=v6.12.95
PKG_HOST_DEPENDS=make                      # `make headers_install` needs GNU Make >= 4.0
PKG_HOST_DEST=${BUILD_DIR}/linux-libc-headers   # staged headers root

do_install() { :; }

do_build() {
  : "${PKG_SRC_DIR:?linux-libc-headers: PKG_SRC_DIR unset}"; : "${PKG_HOST_DEST:?}"
  command -v make >/dev/null 2>&1 || die "linux-libc-headers: make (>=4) not on PATH"
  rm -rf "${PKG_HOST_DEST}"; mkdir -p "${PKG_HOST_DEST}"
  log "linux-libc-headers: make headers_install ARCH=arm (${PKG_VERSION})"
  make --no-print-directory -C "${PKG_SRC_DIR}" ARCH=arm INSTALL_HDR_PATH="${PKG_HOST_DEST}" headers_install \
    >"${RECIPE_SCRATCH}/headers_install.log" 2>&1 \
    || { tail -15 "${RECIPE_SCRATCH}/headers_install.log"; die "linux-libc-headers: headers_install failed"; }
  [ -f "${PKG_HOST_DEST}/include/linux/kd.h" ] || die "linux-libc-headers: install incomplete (no linux/kd.h)"
  echo "  [linux-libc-headers] UAPI headers -> ${PKG_HOST_DEST}/include"
}
