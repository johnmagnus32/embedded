#!/usr/bin/env bash
# providers/libc/musl/stage-runtime.sh — musl's DYNAMIC-RUNTIME staging (sourced by the rootfs
# step when LINK=dynamic). Counterpart to custom/stage-runtime.sh; the rootfs step branches on no
# libc name, it just sources the selected libc's hook.
#
# In scope (rootfs step env): STAGE, ROOTFS_CROSS_COMPILE, log(), die().
# musl is one-file: the loader IS libc.so (from the toolchain sysroot); PT_INTERP
# /lib/ld-musl-armhf.so.1 is a symlink to it.
local libc_so; libc_so="$(${ROOTFS_CROSS_COMPILE}gcc -print-file-name=libc.so)"
[ -f "${libc_so}" ] || die "musl libc.so not found via ${ROOTFS_CROSS_COMPILE}gcc -print-file-name"
install -m 0755 "${libc_so}" "${STAGE}/lib/libc.so"
ln -sf libc.so "${STAGE}/lib/ld-musl-armhf.so.1"
log "staged musl runtime -> /lib (libc.so + ld-musl-armhf.so.1)"
