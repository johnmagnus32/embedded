# providers/libc/custom/cc-profile.sh — the from-scratch libc's COMPILE/LINK contract (sourced by a
# compile class). Its SHAPE depends on the resolved cross-cc provider (from the recipe env):
#
#   cross-cc=toolchain-gcc — the from-source toolchain-gcc carries our libc as its REAL sysroot, so
#                       a NORMAL cross-link works (like musl): no -nostdlib/-nostdinc, no linker
#                       script, no explicit crt/lib — the sysroot supplies crt+libc+headers and the
#                       /lib/ld.so.1 loader. This is the Tier-B end-state.
#   cross-cc=toolchain-custom — OUR OWN from-scratch tools (cpp/cc/as/ar/ld behind the forge-cc
#                       driver). Static-only: our ld places the ET_EXEC with its native two-segment
#                       W^X layout + -Ttext and pulls libc.a to a fixpoint (no linker script, no
#                       foreign gcc, no libgcc). Includes = our headers + staged kernel UAPI.
#
# Out (all modes): PKG_CC, PKG_CFLAGS, PKG_LDFLAGS, LIBC_CRT, LIBC_LIB.
: "${PROVIDER_cross_cc:?custom cc-profile: PROVIDER_cross_cc unset (from the recipe env)}"
if [ "${PROVIDER_cross_cc}" = toolchain-gcc ]; then
  : "${CROSS_COMPILE:?custom cc-profile (gcc): CROSS_COMPILE unset}"
  : "${ROOTFS_ARCH_FLAGS:?custom cc-profile (gcc): ROOTFS_ARCH_FLAGS unset in machine.conf (arch tuning is a board fact)}"
  PKG_CC="${CROSS_COMPILE}gcc"
  PKG_CFLAGS="${ROOTFS_ARCH_FLAGS} -Os -Wall -Wextra"
  LIBC_CRT=""      # the sysroot's crt1/crti/crtn are auto-linked by gcc's driver
  LIBC_LIB=""      # -lc (=libc.so) is the driver default; PT_INTERP=/lib/ld.so.1 is baked into gcc
  if [ "${PKG_LINK}" = "dynamic" ]; then
    PKG_LDFLAGS="-Wl,--build-id=none"
  else
    PKG_LDFLAGS="-static -Wl,--build-id=none"
  fi
elif [ "${PROVIDER_cross_cc}" = toolchain-custom ]; then
  : "${CROSS_COMPILE:?custom cc-profile: CROSS_COMPILE unset}"
  : "${STAGE_INC:?custom cc-profile: STAGE_INC unset (staged kernel UAPI dir)}"
  : "${REPO_ROOT:?custom cc-profile: REPO_ROOT unset}"
  : "${LIBC_STAGE_DIR:?custom cc-profile: LIBC_STAGE_DIR unset}"
  : "${PKG_LINK:?custom cc-profile: PKG_LINK unset}"
  [ "${PKG_LINK}" = dynamic ] && die "TOOLCHAIN=custom: dynamic linking not supported yet (our ld has no .so exports); use LINKAGE=static"
  # PKG_CC = the forge-cc driver (arm-forge-custom-gcc), on PATH via TOOLCHAIN_DIR/bin. Our cpp
  # needs only our own headers + the staged kernel UAPI (no gcc freestanding set, no -isystem). Our cc
  # emits no libgcc calls for this code, so there is no libgcc to add.
  PKG_CC="${CROSS_COMPILE}gcc"
  PKG_CFLAGS="-I${REPO_ROOT}/libc/include -I${STAGE_INC} -I${REPO_ROOT}/libc/src -Os"
  LIBC_CRT="${LIBC_STAGE_DIR}/crt0.S.o"                 # our _start, linked into each program
  LIBC_LIB="${LIBC_STAGE_DIR}/libc.a"                   # our ld pulls the needed members to a fixpoint
  PKG_LDFLAGS="-Ttext 0x40000000"                       # static ET_EXEC base (bare-metal virt RAM)
else
  die "custom cc-profile: cross-cc=${PROVIDER_cross_cc} unsupported (toolchain-gcc|toolchain-custom only)"
fi
