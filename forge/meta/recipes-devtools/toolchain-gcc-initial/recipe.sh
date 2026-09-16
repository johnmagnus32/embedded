# hostpackages/toolchain-gcc-initial/recipe.sh — STAGE 1 of our from-source arm cross toolchain:
# binutils + gcc pass-1 (C only, --without-headers, static libgcc, inhibit_libc). NO libc yet — the
# libc node then compiles REPO_ROOT/libc WITH this gcc into a conforming sysroot (libc/build-sysroot.sh),
# and toolchain-gcc (stage 2) rebuilds gcc --with-sysroot=that. The SHARED bits (source pins, fetch/
# extract/patch, binutils, sanity check) live in the host-toolchain-gcc class; the STAGE-SPECIFIC gcc
# pass-1 is do_build HERE, in the recipe that owns it.
PKG_NAME=toolchain-gcc-initial
PKG_CLASS=cross
PKG_PROVIDES=virtual/cross-cc-initial                            # the stage-1 compiler the libc builds with (Yocto's virtual/${TARGET_PREFIX}gcc-initial)
PKG_ALIAS=gcc                                                    # TOOLCHAIN=gcc selects this provider
PKG_HOST_CC_PREFIX=arm-forge-linux-gnueabihf-                    # cross triple; MUST match toolchain-gcc (byte-identical stages)
inherit host-toolchain-gcc                                       # shared LOGIC (do_unpack/do_patch/tc_binutils/…)
require ${FORGE_META}/recipes-devtools/toolchain-gcc-sources.inc     # shared DATA (triple, cpu/fpu, SRC pins)

PKG_FETCH=none                 # sources come from PKG_SOURCES (in the .inc), fetched by base.sh
PKG_VERSION=gcc13.3.0-binutils2.42

# Self-identifying dest: this host package owns build/toolchain-gcc-initial (the arm-forge stage-1 gcc
# the libc node builds with, via LIBC_TC/LIBC_TC_DIR in forge.conf).
PKG_HOST_DEST=${BUILD_DIR}/toolchain-gcc-initial

do_build() {
  # do_unpack (class) already extracted the source + ran tc_setup, so triple/TC/W/jobs/cfg_arch are set.
  local SYSROOT="${TC}/${triple}/sysroot"        # stage-1 owns its OWN empty sysroot (--without-headers)
  rm -rf "${SYSROOT}"; mkdir -p "${SYSROOT}/usr/include" "${SYSROOT}/usr/lib" "${SYSROOT}/lib"
  tc_binutils "${SYSROOT}"                        # class: binutils + PATH

  log "${PKG_NAME}: gcc pass-1 (bootstrap, inhibit_libc)"
  ( mkdir -p "${W}/build/gcc1" && cd "${W}/build/gcc1" \
      && "${W}/src/gcc/configure" --target="${triple}" --prefix="${TC}" --with-sysroot="${SYSROOT}" \
           --without-headers --with-newlib --enable-languages=c \
           --disable-shared --disable-threads --disable-libssp --disable-libgomp \
           --disable-libquadmath --disable-libatomic --disable-libvtv \
           --disable-multilib --disable-decimal-float --disable-libsanitizer \
           --disable-nls --disable-default-pie "${cfg_arch[@]}" \
      && make -j"${jobs}" all-gcc all-target-libgcc \
      && make install-gcc install-target-libgcc ) >"${W}/01-gcc-pass1.log" 2>&1 \
    || die "${PKG_NAME}: gcc pass-1 failed (see ${W}/01-gcc-pass1.log)"
  rm -rf "${W}/src" "${W}/build"
  log "${PKG_NAME}: ok -> stage-1 gcc $("${triple}-gcc" --version | head -1); the libc node builds the sysroot next"
}
