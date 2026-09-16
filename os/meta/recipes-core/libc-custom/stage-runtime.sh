#!/usr/bin/env bash
# providers/libc/custom/stage-runtime.sh — the from-scratch libc's DYNAMIC-RUNTIME staging (sourced
# by the rootfs step when LINK=dynamic; static builds never call this). WHERE /lib/{libc.so,ld.so.1}
# come from depends on the TOOLCHAIN axis (from the recipe env):
#   TOOLCHAIN=source  — the libc recipe built them into its conforming sysroot (LIBC_STAGE_DIR), which
#                       IS the final toolchain-gcc's --with-sysroot; fetch via the compiler itself
#                       (-print-file-name / -print-sysroot both resolve back to that sysroot).
#   TOOLCHAIN=prebuilt— the bare -nostdlib build put them in LIBC_STAGE_DIR; copy from there.
#
# In scope (rootfs step env): STAGE, LIBC_STAGE_DIR, CROSS_COMPILE, log(), die().
: "${TOOLCHAIN:?stage-runtime: TOOLCHAIN unset (from the recipe env)}"
if [ "${TOOLCHAIN}" = gcc ]; then
  local libc_so sysroot
  libc_so="$(${CROSS_COMPILE}gcc -print-file-name=libc.so)"
  sysroot="$(${CROSS_COMPILE}gcc -print-sysroot)"
  [ -f "${libc_so}" ] || die "custom(source) libc.so not found via ${CROSS_COMPILE}gcc -print-file-name"
  [ -f "${sysroot}/lib/ld.so.1" ] || die "custom(source) ld.so.1 not found in ${sysroot}/lib"
  install -m 0755 "${libc_so}" "${STAGE}/lib/libc.so"
  install -m 0755 "${sysroot}/lib/ld.so.1" "${STAGE}/lib/ld.so.1"
  log "staged custom(source) runtime -> /lib (libc.so + ld.so.1)"
else
  install -m 0755 "${LIBC_STAGE_DIR}/ld.so.1" "${STAGE}/lib/ld.so.1"
  install -m 0755 "${LIBC_STAGE_DIR}/libc.so" "${STAGE}/lib/libc.so"
  log "staged custom runtime -> /lib (ld.so.1 + libc.so)"
fi
