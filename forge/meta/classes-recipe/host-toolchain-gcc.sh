#!/usr/bin/env bash
# host-toolchain-gcc.sh — SHARED MECHANISM for building our from-source arm cross-toolchain. Both
# stage recipes `inherit` this for the parts that are IDENTICAL between them — the class binds the
# shared TASKS do_unpack (extract binutils/gcc + math symlinks) and do_patch (repoint gcc's loader to
# /lib/ld.so.1), plus the tc_binutils helper. Each recipe's own do_build holds the STAGE-SPECIFIC part
# (its gcc configure/make + sysroot choice, and — final only — the normal-link sanity check), NOT shared:
#
#   toolchain-gcc-initial  do_build: binutils(own empty sysroot) + gcc PASS-1 (C only, --without-headers,
#                                    static libgcc, inhibit_libc: no libc deps)
#         │ the libc node then compiles REPO_ROOT/libc WITH the initial gcc into a conforming SYSROOT
#         ▼ (crt1/crti/crtn + libc.a/.so + /lib/ld.so.1) — see libc/build-sysroot.sh
#   toolchain-gcc          do_build: binutils(--with-sysroot=that libc) + gcc PASS-2, inhibit_libc OFF
#                                    (full libgcc + unwinder vs the libc) + the normal-link sanity check
#
# So `arm-forge-…-gcc hello.c` (the FINAL gcc) is a NORMAL cross-link against our libc — no
# -nostdlib/-nostdinc gymnastics (contrast host-tarball-bin's prebuilt Bootlin toolchains, and
# LIBC=custom+TOOLCHAIN=prebuilt which rides a foreign gcc bare). This mirrors Yocto's
# gcc-cross-initial → glibc → gcc-cross split. The shared source pins live in toolchain-gcc-sources.inc
# (Yocto's gcc-${PV}.inc), `require`d by both stages; compute_taskhash hashes the .inc, so a version
# bump there re-hashes BOTH stages, then (initial → libc → packages) ripples everywhere.
#
# A recipe sets: PKG_NAME, PKG_HOST_DEST (install prefix), PKG_VERSION (for the banner) + its own
# do_build (which runs after this class's do_unpack+do_patch, so triple/TC/W/jobs/cfg_arch — set by
# tc_setup in do_unpack — are already in scope), and `require`s toolchain-gcc-sources.inc for the SHARED
# DATA (triple, cpu/fpu, SRC_URI/checksum pins) — that data lives in the .inc, NOT here (a class carries
# LOGIC, not SRC_URI). The final recipe also sets PKG_DEPENDS=libc (fold the libc taskhash + the
# host-toolchain-gcc: libc Make edge in engine.mk — its --with-sysroot IS that libc). do_install: no-op.
# do_fetch: base's (declarative PKG_SOURCES, from the .inc). PKG_TARGET_INDEPENDENT: built for a fixed
# triple, not the selected board (the .inc's hard-float VFP baseline serves every board).

PKG_TARGET_INDEPENDENT=1

do_install() { :; }

# tc_setup — validate facts + host tools, derive the shared build vars (triple/TC/W/jobs/arch/cfg_arch),
# left GLOBAL so do_patch + the recipe's do_build see them. Called first by do_unpack (the first task
# that needs them; do_fetch = base's, runs before). Pure derivation — no I/O.
tc_setup() {
  : "${PKG_NAME:?host-toolchain-gcc: PKG_NAME unset}"
  : "${PKG_HOST_DEST:?${PKG_NAME}: PKG_HOST_DEST (install prefix) unset}"
  : "${PKG_HOST_CC_PREFIX:?${PKG_NAME}: PKG_HOST_CC_PREFIX (triple-) unset}"
  : "${REPO_ROOT:?${PKG_NAME}: REPO_ROOT unset}"
  for v in PKG_TC_CPU PKG_TC_FPU; do [ -n "${!v:-}" ] || die "${PKG_NAME}: recipe fact ${v} unset"; done
  for tool in tar xz bzip2 gzip gcc g++ make bison flex makeinfo gawk file; do
    command -v "$tool" >/dev/null 2>&1 || die "${PKG_NAME}: required tool '$tool' not on PATH"
  done
  triple="${PKG_HOST_CC_PREFIX%-}"
  TC="${PKG_HOST_DEST}"
  W="${BUILD_DIR}/${PKG_NAME}-work"
  jobs="$(nproc)"
  arch="-mcpu=${PKG_TC_CPU} -marm"
  cfg_arch=(--with-cpu="${PKG_TC_CPU}" --with-fpu="${PKG_TC_FPU}" --with-float=hard --with-mode=arm)
}

# do_unpack — extract binutils + gcc, symlink the math libs (gmp/mpfr/mpc/isl) INTO the gcc tree for
# gcc's in-tree build, and reset a clean work + install prefix. Sources were SHA-verified into
# DOWNLOAD_DIR by base's do_fetch (the PKG_SOURCES from the .inc); read them by name via pkg_src.
do_unpack() {
  tc_setup
  local bt gt gmp mpfr mpc isl
  bt="$(pkg_src binutils)"; gt="$(pkg_src gcc)"
  gmp="$(pkg_src gmp)"; mpfr="$(pkg_src mpfr)"; mpc="$(pkg_src mpc)"; isl="$(pkg_src isl)"

  rm -rf "${W}"; mkdir -p "${W}/src" "${W}/build"
  log "${PKG_NAME}: extracting binutils + gcc (+ gmp/mpfr/mpc/isl)"
  mkdir -p "${W}/src/binutils"; tar -xf "${bt}" -C "${W}/src/binutils" --strip-components=1
  mkdir -p "${W}/src/gcc";      tar -xf "${gt}" -C "${W}/src/gcc"      --strip-components=1
  local m name tb d
  for m in "gmp:${gmp}" "mpfr:${mpfr}" "mpc:${mpc}" "isl:${isl}"; do
    name="${m%%:*}"; tb="${m#*:}"
    tar -xf "${tb}" -C "${W}/src/gcc"
    d="$(cd "${W}/src/gcc" && ls -d "${name}"-*/ | head -1)"
    ln -sfn "${d%/}" "${W}/src/gcc/${name}"
  done
  rm -rf "${TC}"; mkdir -p "${TC}"
}

# do_patch — point gcc's ARM dynamic-linker default at the SELECTED libc's loader (so a normal
# `gcc hello.c` emits the right PT_INTERP with no flags): our custom libc => /lib/ld.so.1; musl =>
# /lib/ld-musl-armhf.so.1. LIBC comes from forge.conf (the node env).
do_patch() {
  local eabi="${W}/src/gcc/gcc/config/arm/linux-eabi.h"
  : "${LIBC:?${PKG_NAME}: LIBC unset (should come from forge.conf)}"
  local loader="/lib/ld.so.1"
  [ "${LIBC}" = musl ] && loader="/lib/ld-musl-armhf.so.1"
  sed -i -e "s#\"/lib/ld-linux\.so\.3\"#\"${loader}\"#" \
         -e "s#\"/lib/ld-linux-armhf\.so\.3\"#\"${loader}\"#" "${eabi}" \
    || die "${PKG_NAME}: dynamic-linker patch failed"
}

# tc_binutils <sysroot> — build + install GNU binutils for the triple against <sysroot>, then put the
# just-installed cross tools (as/ld/ar) on PATH so the gcc build finds them. IDENTICAL for both stages
# (only the sysroot arg differs: the initial stage's own empty one vs the final stage's libc sysroot).
tc_binutils() {
  local sysroot="${1:?tc_binutils: sysroot arg required}"
  log "${PKG_NAME}: binutils"
  ( mkdir -p "${W}/build/binutils" && cd "${W}/build/binutils" \
      && "${W}/src/binutils/configure" --target="${triple}" --prefix="${TC}" --with-sysroot="${sysroot}" \
           --disable-nls --disable-werror --disable-multilib --disable-gdb --disable-gprofng \
      && make -j"${jobs}" && make install ) >"${W}/00-binutils.log" 2>&1 \
    || die "${PKG_NAME}: binutils build failed (see ${W}/00-binutils.log)"
  export PATH="${TC}/bin:${PATH}"
}
