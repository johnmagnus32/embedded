# recipes-core/musl/cc-profile.sh — musl's COMPILE/LINK contract (sourced by a compile class for the
# compile-c packages + the libc-linkers). musl is built into a conforming sysroot and toolchain-gcc is
# --with-sysroot=that, so this is a NORMAL cross-link (like the custom libc's TOOLCHAIN=gcc path): no
# -nostdlib/-nostdinc, no explicit crt/lib — the sysroot supplies crt+libc+headers.
# Only TOOLCHAIN=gcc builds musl (our custom `cc` is a C subset that can't compile it; prebuilt is gone).
[ "${TOOLCHAIN}" = gcc ] || die "LIBC=musl needs TOOLCHAIN=gcc (musl is built from source; our custom cc can't compile it)"
: "${ROOTFS_CROSS_COMPILE:?musl cc-profile: ROOTFS_CROSS_COMPILE unset}"
: "${ROOTFS_ARCH_FLAGS:?musl cc-profile: ROOTFS_ARCH_FLAGS unset in board.conf}"
PKG_CC="${ROOTFS_CROSS_COMPILE}gcc"
PKG_CFLAGS="${ROOTFS_ARCH_FLAGS} -Os -Wall -Wextra"
LIBC_CRT=""      # the sysroot's crt1/crti/crtn are auto-linked by gcc's driver
LIBC_LIB=""      # -lc (=musl) is the driver default
if [ "${PKG_LINK}" = "dynamic" ]; then
  PKG_LDFLAGS="-Wl,--build-id=none"
else
  PKG_LDFLAGS="-static -Wl,--build-id=none"
fi
