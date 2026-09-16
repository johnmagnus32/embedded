# recipes-core/musl/recipe.sh — musl libc, built FROM SOURCE (Yocto's musl recipe analogue). Part of the
# same gcc bootstrap as our custom libc: gcc-cross-initial → musl (this) → gcc-cross (--with-sysroot=this).
# musl is built with the stage-1 gcc (virtual/cross-cc-initial) into a conforming sysroot (headers + crt1/crti/crtn +
# libc.a/.so + ld-musl), which toolchain-gcc then targets so packages cross-link NORMALLY against musl.
# Only the from-source `gcc` toolchain can build musl (our custom `cc` is a C subset — see cc-profile.sh).
PKG_NAME=musl
PKG_CLASS=target
PKG_PROVIDES=virtual/libc
PKG_LINKSENS=1                 # its sysroot is built static+shared; keyed on link mode like the custom libc

PKG_FETCH=tarball
PKG_VERSION=1.2.5
PKG_SITE=https://musl.libc.org/releases
PKG_SOURCE=musl-1.2.5.tar.gz
PKG_SHA256=a9a118bbe84d8764da0ea0d28b3ab3fae8477fc7e4085d90102b8596fc7c75e4
PKG_HOST_DEPENDS="virtual/cross-cc-initial linux-libc-headers"   # stage-1 gcc builds it; UAPI headers go into its sysroot
PKG_ARTIFACT=libcstage:        # the conforming sysroot; cacheable, link-keyed

do_install() { :; }

# do_build — build musl into LIBC_STAGE_DIR as a sysroot. musl's own build system does the work; we just
# point it at the stage-1 cross gcc + the board-independent hard-float baseline (NOT ROOTFS_ARCH_FLAGS,
# which may carry a per-board -mgeneral-regs-only that would strip VFP from the libc — same reasoning as
# libc/build-sysroot.sh). `make install DESTDIR=` lays out /usr/{include,lib} + /lib/ld-musl-…, exactly
# the sysroot layout toolchain-gcc's --with-sysroot expects.
do_build() {
  : "${PKG_SRC_DIR:?musl do_build: PKG_SRC_DIR unset}"; : "${LIBC_STAGE_DIR:?}"; : "${LIBC_TC_DIR:?}"
  : "${CROSS_COMPILE:?}"
  local CC="${LIBC_TC_DIR}/bin/${CROSS_COMPILE}gcc"
  [ -x "${CC}" ] || die "musl: stage-1 gcc missing: ${CC} (virtual/cross-cc-initial must build first)"
  rm -rf "${LIBC_STAGE_DIR}"; mkdir -p "${LIBC_STAGE_DIR}"
  cd "${PKG_SRC_DIR}"
  echo "  [libc] musl ${PKG_VERSION}: configure + build from source (stage-1 gcc)"
  ./configure --host=arm-forge-linux-gnueabihf --prefix=/usr \
      CC="${CC}" CFLAGS="-mcpu=cortex-a7 -marm -O2" >"${RECIPE_SCRATCH}/musl-configure.log" 2>&1 \
    || { tail -15 "${RECIPE_SCRATCH}/musl-configure.log"; die "musl: configure failed"; }
  make -j"$(nproc)"                       >"${RECIPE_SCRATCH}/musl-make.log"    2>&1 || { tail -20 "${RECIPE_SCRATCH}/musl-make.log"; die "musl: make failed"; }
  make install DESTDIR="${LIBC_STAGE_DIR}" >"${RECIPE_SCRATCH}/musl-install.log" 2>&1 || { tail -15 "${RECIPE_SCRATCH}/musl-install.log"; die "musl: install failed"; }
  [ -e "${LIBC_STAGE_DIR}/usr/lib/crt1.o" ] || die "musl: sysroot incomplete (no crt1.o) at ${LIBC_STAGE_DIR}"
  # add the sanitized Linux UAPI headers (linux/*, asm/*, …) alongside musl's own — a from-source libc
  # sysroot needs them for real userland (busybox's <linux/kd.h>); the linux-libc-headers node staged them.
  local UAPI="${BUILD_DIR}/linux-libc-headers/include"
  [ -d "${UAPI}/linux" ] || die "musl: linux-libc-headers not staged at ${UAPI} (host-dep must build first)"
  cp -a "${UAPI}/." "${LIBC_STAGE_DIR}/usr/include/"
  echo "  [libc] musl -> ${LIBC_STAGE_DIR} (musl headers + Linux UAPI + crt + libc.a/.so + ld-musl)"
}
