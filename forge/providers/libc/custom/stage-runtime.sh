#!/usr/bin/env bash
# providers/libc/custom/stage-runtime.sh — gv3libc's DYNAMIC-RUNTIME staging (sourced by the
# rootfs step when LINK=dynamic). Each libc owns "which runtime files land in the image's /lib",
# so the rootfs step branches on NO libc name — it just sources the selected libc's hook (like
# it sources the libc's cc-profile.sh). Static builds never call this (libc is baked into each
# binary); the rootfs step's gate handles that.
#
# In scope (rootfs step env): STAGE (the image root), LIBC_STAGE_DIR (where the libc build put
# its artifacts), log().
# gv3libc: the from-source loader + shared lib sit in LIBC_STAGE_DIR; copy both to /lib.
install -m 0755 "${LIBC_STAGE_DIR}/ld-gv3.so.1" "${STAGE}/lib/ld-gv3.so.1"
install -m 0755 "${LIBC_STAGE_DIR}/libc.so"     "${STAGE}/lib/libc.so"
log "staged gv3 runtime -> /lib (ld-gv3.so.1 + libc.so)"
