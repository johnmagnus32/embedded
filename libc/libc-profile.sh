# libc/libc-profile.sh — gv3libc's COMPILE/LINK CONTRACT (how to build AGAINST it).
#
# The libc-provider analogue of the kernel/u-boot providers' build.sh: a compile class
# (via the LIBC_CC_PROFILE shim) stays libc-agnostic and SOURCES this when the selected
# libc is this from-source provider. Everything gv3libc-specific — the -nostdlib
# freestanding profile, its include dirs, the linker script, the crt0/libc.a/libc.so
# artifact names, and the /lib/ld-gv3.so.1 dynamic-linker path — lives HERE, with the
# libc, not baked into the engine. (Buildroot toolchain-wrapper model: a package's
# build is libc-blind; the libc-specificity is encapsulated by the toolchain/provider.)
#
# Inputs (all from run-recipe.sh + the node env, set before this is sourced):
#   ROOTFS_ARCH_FLAGS     the board's arch tuning (board.conf)
#   PKG_LINK              static | dynamic
#   STAGE_INC             the staged kernel-UAPI include dir (build.sh populates it)
#   LIBC_STAGE_DIR        where build.sh put crt0.S.o / libc.a / libc.so
#   ROOTFS_CROSS_COMPILE  the cross-toolchain prefix
# Sets (the CC/link contract): PKG_CC, PKG_CFLAGS, PKG_LDFLAGS, LIBC_CRT, LIBC_LIB.

LIBC_PROVIDER_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
: "${STAGE_INC:?libc-profile.sh: STAGE_INC unset (staged gv3 UAPI dir)}"
: "${LIBC_STAGE_DIR:?libc-profile.sh: LIBC_STAGE_DIR unset (gv3libc build output)}"
: "${ROOTFS_ARCH_FLAGS:?libc-profile.sh: ROOTFS_ARCH_FLAGS unset in board.conf (arch tuning is a board fact)}"
PKG_LINK="${PKG_LINK:-static}"

# The compiler: gv3libc has no toolchain of its own — it rides the musl cross-gcc purely as a
# bare ARM code generator (we pass -ffreestanding -nostdlib below, so NONE of musl's libc/crt
# is used; only the compiler + binutils). Hence the musl prefix, even though this isn't musl.
: "${ROOTFS_CROSS_COMPILE:?libc-profile.sh: ROOTFS_CROSS_COMPILE unset (the cross-gcc gv3libc drives)}"
PKG_CC="${ROOTFS_CROSS_COMPILE}gcc"

# -ffreestanding -nostdlib -nostartfiles: no host/musl libc, no crt. -fno-builtin so
# the compiler assumes no libc semantics (we provide memcpy/memset ourselves). Include
# order: our headers, the staged kernel UAPI snapshot, then libc/src (private headers).
PKG_CFLAGS="${ROOTFS_ARCH_FLAGS} -ffreestanding -nostdlib -nostartfiles \
-fno-builtin -fno-stack-protector -Os -Wall -Wextra \
-I${LIBC_PROVIDER_DIR}/include -I${STAGE_INC} -I${LIBC_PROVIDER_DIR}/src"

# crt0 (our _start) is ALWAYS linked into the executable, never the library.
LIBC_CRT="${LIBC_STAGE_DIR}/crt0.S.o"
if [ "${PKG_LINK}" = "dynamic" ]; then
  # PIC + shared libc.so, resolved at runtime by our ld-gv3.so.1 (PT_INTERP).
  LIBC_LIB="-L${LIBC_STAGE_DIR} -lc"
  PKG_LDFLAGS="-nostdlib -no-pie -Wl,--dynamic-linker=/lib/ld-gv3.so.1 -Wl,--build-id=none"
else
  # static: our user.ld places the ET_EXEC; libc.a is copied into each program.
  LIBC_LIB="${LIBC_STAGE_DIR}/libc.a"
  PKG_LDFLAGS="-T ${LIBC_PROVIDER_DIR}/user.ld -nostdlib -static -Wl,--build-id=none -Wl,-z,max-page-size=0x1000"
fi
