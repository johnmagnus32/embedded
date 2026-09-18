# hostpackages/toolchain-gcc/recipe.sh — STAGE 2 (final) of our from-source arm cross toolchain:
# binutils + gcc pass-2, --with-sysroot=<the libc recipe's conforming sysroot> (LIBC_STAGE_DIR), so
# `arm-forge-…-gcc hello.c` is a NORMAL cross-link against the selected libc (musl or our custom libc;
# --with-sysroot + --dynamic-linker are libc-aware). Stock GCC 13.3.0 + GNU binutils 2.42 — what is
# "ours" is that WE build it (from source, no prebuilt downloads), targeting the selected libc. The
# SHARED bits (source pins, fetch/extract/patch, binutils, sanity check) live in the host-toolchain-gcc
# class; the STAGE-SPECIFIC gcc pass-2 is do_build HERE, in the recipe that owns it.
PKG_NAME=toolchain-gcc
PKG_CLASS=cross
PKG_PROVIDES=cross-cc                                    # the rootfs/package compiler (Yocto's ${TARGET_PREFIX}gcc)
PKG_HOST_CC_PREFIX=arm-forge-linux-gnueabihf-                    # cross triple; MUST match toolchain-gcc-initial (byte-identical stages)
inherit host-toolchain-gcc                                       # shared LOGIC (do_unpack/do_patch/tc_binutils/…)
require ${OS_META}/recipes-devtools/toolchain-gcc-sources.inc     # shared DATA (triple, cpu/fpu, SRC pins)

PKG_FETCH=none                 # sources come from PKG_SOURCES (in the .inc), fetched by base.sh
PKG_VERSION=gcc13.3.0-binutils2.42

# PKG_DEPENDS=libc: the final gcc's --with-sysroot IS the libc recipe's staged sysroot, so (a) engine.mk
# adds the `host-toolchain-gcc: libc` Make edge, and (b) compute_recipe_stamp folds the libc recipehash (+
# link mode, since PKG_DEPENDS=libc), so a libc/cc-profile edit rebuilds this gcc. The initial→libc→
# packages ripple (via the shared class body's source pins) covers gcc/binutils version bumps.
PKG_DEPENDS=libc

# Self-identifying dest/prefix: owns build/toolchain-gcc + the arm-os triple. TOOLCHAIN_DIR /
# CROSS_COMPILE resolve to these when TOOLCHAIN=gcc.
PKG_HOST_DEST=${BUILD_DIR}/toolchain-gcc

do_build() {
  # the sysroot IS the libc recipe's staged conforming sysroot (the libc built first — Make edge above).
  : "${LIBC_STAGE_DIR:?${PKG_NAME}: LIBC_STAGE_DIR unset = the libc recipe sysroot}"
  local SYSROOT="${LIBC_STAGE_DIR}"
  [ -e "${SYSROOT}/usr/lib/crt1.o" ] || die "${PKG_NAME}: libc sysroot incomplete at ${SYSROOT} — the libc recipe must build first"

  # do_unpack (class) already extracted the source + ran tc_setup, so triple/TC/W/jobs/cfg_arch are set.
  tc_binutils "${SYSROOT}"                        # class: binutils(--with-sysroot=libc) + PATH

  log "${PKG_NAME}: gcc pass-2 (final, against libc sysroot ${SYSROOT})"
  ( mkdir -p "${W}/build/gcc2" && cd "${W}/build/gcc2" \
      && "${W}/src/gcc/configure" --target="${triple}" --prefix="${TC}" --with-sysroot="${SYSROOT}" \
           --enable-languages=c \
           --disable-shared --disable-threads --disable-libssp --disable-libgomp \
           --disable-libquadmath --disable-libatomic --disable-libvtv \
           --disable-multilib --disable-libsanitizer --disable-nls --disable-default-pie \
           "${cfg_arch[@]}" \
      && make -j"${jobs}" && make install ) >"${W}/03-gcc-pass2.log" 2>&1 \
    || die "${PKG_NAME}: gcc pass-2 failed (see ${W}/03-gcc-pass2.log)"

  # sanity: a NORMAL link (no -nostdlib) must be ARM + wired to /lib/ld.so.1 + NEEDED libc.so. Only
  # meaningful now that a real libc sysroot exists — hence here (final), not the initial stage.
  local TMP; TMP="$(mktemp -d)"
  printf '#include <stdio.h>\nint main(void){return 0;}\n' > "${TMP}/t.c"
  # shellcheck disable=SC2086  (arch is a multi-flag string — MUST word-split, not be one arg)
  "${triple}-gcc" ${arch} -o "${TMP}/t.dyn" "${TMP}/t.c" \
    || { rm -rf "${TMP}"; die "${PKG_NAME}: sanity normal-link failed"; }
  : "${PROVIDER_libc:?${PKG_NAME}: PROVIDER_libc unset (from the recipe env)}"
  local F NEEDED INTERP LOADER="/lib/ld.so.1"
  [ "${PROVIDER_libc}" = musl ] && LOADER="/lib/ld-musl-armhf.so.1"
  F="$(file "${TMP}/t.dyn" 2>/dev/null || true)"
  NEEDED="$("${triple}-readelf" -d "${TMP}/t.dyn" 2>/dev/null | grep -c 'NEEDED.*libc.so')"
  INTERP="$("${triple}-readelf" -p .interp "${TMP}/t.dyn" 2>/dev/null | grep -c "${LOADER}")"
  rm -rf "${TMP}"
  case "${F}" in *ARM*) : ;; *) die "${PKG_NAME}: sanity object not ARM (${F})" ;; esac
  [ "${NEEDED}" = 1 ] || die "${PKG_NAME}: sanity link did not need libc.so"
  [ "${INTERP}" = 1 ] || die "${PKG_NAME}: sanity link interp != ${LOADER}"
  log "${PKG_NAME}: ok -> $("${triple}-gcc" --version | head -1); normal-link => libc.so + ${LOADER}"
  rm -rf "${W}/src" "${W}/build"
}
