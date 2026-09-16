#!/usr/bin/env bash
# classes/libc.sh — the "libc" CLASS: build the SELECTED C library into its staging dir. libc
# recipes `inherit libc`. Dispatches on PROPERTIES (a file's existence + the TOOLCHAIN axis), never a
# libc name, so the engine stays libc-agnostic:
#   * custom libc + TOOLCHAIN=gcc: build a CONFORMING SYSROOT (crt1/crti/crtn + libc.a/.so + /lib/ld.so.1
#       + headers) with the STAGE-1 gcc (libc/build-sysroot.sh). The STAGE-2 toolchain-gcc then builds
#       --with-sysroot=that (Yocto's glibc-between-the-two-gcc-stages shape; no no-op node).
#   * custom libc + TOOLCHAIN=custom: our own cc/as/ld build libc.a + crt0 via the bare -nostdlib
#       profile + build.sh (no stage split — our cc isn't bootstrapped).
#   (musl builds itself FROM SOURCE via its own recipe do_build, so it never reaches this class.)
# do_install is a no-op: artifacts already land in LIBC_STAGE_DIR, read directly by the rootfs step +
# cc-profile / the final toolchain's --with-sysroot.

# LINK-SENSITIVE OUTPUT: the from-source libc is built static OR dynamic, so its staging dir + taskhash
# vary by PKG_LINK. Declaring it here (not a PKG_ROLE=libc test in the engine) keeps the orchestrator
# libc-agnostic; packages get the same via PKG_DEPENDS=libc. (Prebuilt musl inherits it harmlessly.)
PKG_VARDEPS="PKG_LINK"

do_build() {
  : "${LIBC:?libc do_build: LIBC unset}"
  local libc_build="${PKG_SRC_DIR:+${PKG_SRC_DIR}/build.sh}"
  if [ -z "${libc_build}" ] || [ ! -f "${libc_build}" ]; then
    echo "  [libc] LIBC=${LIBC}: prebuilt libc (no build.sh), nothing to build"
    return 0
  fi
  : "${REPO_ROOT:?}"; : "${LIBC_STAGE_DIR:?}"; : "${STAGE_INC:?}"

  : "${TOOLCHAIN:?libc do_build: TOOLCHAIN unset (from the node env)}"
  # TOOLCHAIN=gcc: build the libc into a conforming sysroot with the stage-1 gcc (LIBC_TC_DIR). The
  # stage-2 toolchain-gcc is then built --with-sysroot=LIBC_STAGE_DIR (it PKG_DEPENDS on this node), so
  # a package cross-link pulls crt+libc+headers straight from here. build-sysroot.sh rides WITH the libc.
  if [ "${TOOLCHAIN}" = gcc ]; then
    local sysroot_build="${PKG_SRC_DIR}/build-sysroot.sh"
    [ -f "${sysroot_build}" ] || die "libc: TOOLCHAIN=gcc needs ${sysroot_build}"
    # shellcheck source=/dev/null
    source "${sysroot_build}"
    return 0
  fi

  # TOOLCHAIN=custom: our own cpp/cc/as/ar/ld build the libc (libc.a + crt0) via the bare -nostdlib
  # profile. cc-profile gives PKG_CC (the forge-cc driver) / PKG_CFLAGS; the provider's build.sh is
  # sourced so it inherits them. The selected libc's own CC/link contract lives beside its recipe.
  : "${PROVIDER_libc:?libc do_build: PROVIDER_libc unset (from the node env)}"
  # shellcheck source=/dev/null
  source "$(dirname "${PROVIDER_libc}")/cc-profile.sh"
  # shellcheck source=/dev/null
  source "${libc_build}"
}

do_install() { :; }
