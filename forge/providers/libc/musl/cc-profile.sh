# providers/libc/musl/cc-profile.sh — musl's CC/link contract (sourced by a compile class).
#
# musl is a PREBUILT complete libc: the Bootlin musl cross toolchain's sysroot supplies crt +
# libc + headers, so a NORMAL cross-link works — no -nostdlib, no linker script, no gv3libc
# artifacts. (This is what a conforming libc looks like; contrast providers/libc/custom.)
#
# In:  ROOTFS_ARCH_FLAGS (board arch tune), PKG_LINK (static|dynamic),
#      ROOTFS_CROSS_COMPILE (the musl cross-toolchain prefix) — all from run-recipe.sh.
# Out: PKG_CC, PKG_CFLAGS, PKG_LDFLAGS, LIBC_CRT, LIBC_LIB.
# The compiler is a LIBC fact: musl wants ITS matching toolchain (arm-...-musleabihf-).
: "${ROOTFS_CROSS_COMPILE:?musl cc-profile: ROOTFS_CROSS_COMPILE unset (the musl toolchain prefix)}"
: "${ROOTFS_ARCH_FLAGS:?musl cc-profile: ROOTFS_ARCH_FLAGS unset in board.conf (arch tuning is a board fact)}"
PKG_CC="${ROOTFS_CROSS_COMPILE}gcc"
PKG_CFLAGS="${ROOTFS_ARCH_FLAGS} -Os -Wall -Wextra"
LIBC_CRT=""
LIBC_LIB=""
if [ "${PKG_LINK}" = "dynamic" ]; then
  PKG_LDFLAGS="-Wl,--build-id=none"
else
  PKG_LDFLAGS="-static -Wl,--build-id=none"
fi
