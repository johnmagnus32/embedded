#!/usr/bin/env bash
# classes/libc.sh — the "libc" CLASS: build the SELECTED C library into its staging dir. libc
# recipes `inherit libc`. Dispatches on a PROPERTY, not a libc name:
#   * FROM-SOURCE libc (gv3libc): ships build.sh beside its source (LIBC_SRC) — we run it.
#   * PREBUILT complete libc (musl): no build.sh — nothing to build (sysroot is complete).
# do_install is a no-op: from-source artifacts already land in LIBC_STAGE_DIR, which the
# rootfs recipe + cc-profile read directly.

# LINK-SENSITIVE OUTPUT: the from-source libc is built static OR dynamic, so its staging dir + taskhash
# vary by PKG_LINK. Declaring it here (not a PKG_ROLE=libc test in the engine) keeps the orchestrator
# libc-agnostic; packages get the same via PKG_DEPENDS=libc. (Prebuilt musl inherits it harmlessly.)
PKG_LINKSENS=1

do_build() {
  : "${LIBC:?libc do_build: LIBC unset}"
  local libc_build="${LIBC_SRC:+${LIBC_SRC}/build.sh}"
  if [ -z "${libc_build}" ] || [ ! -f "${libc_build}" ]; then
    echo "  [libc] LIBC=${LIBC}: prebuilt libc (no build.sh), nothing to build"
    return 0
  fi
  : "${REPO_ROOT:?}"; : "${LIBC_STAGE_DIR:?}"; : "${STAGE_INC:?}"
  # cc-profile gives PKG_CC/PKG_CFLAGS (the from-source libc's -nostdlib profile); the
  # provider's build.sh is sourced so it inherits them. LIBC_CC_PROFILE (from forge.conf)
  # is the selected libc's own contract — sourced directly, no engine-side per-libc code.
  : "${LIBC_CC_PROFILE:?libc do_build: LIBC_CC_PROFILE unset (from forge.conf)}"
  # shellcheck source=/dev/null
  source "${LIBC_CC_PROFILE}"
  # shellcheck source=/dev/null
  source "${libc_build}"
}

do_install() { :; }
