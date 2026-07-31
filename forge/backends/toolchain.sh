#!/usr/bin/env bash
# toolchain.sh — prepare the build environment (Step 0 of the OSS pipeline).
#
# Two things must be in place before we can build U-Boot / the kernel, and both
# are HOST-environment setup (not target code), so they live together here as
# two functions:
#
#   setup_host_make()        Ensure GNU Make >= 4.0 is available — the Linux
#                            kernel (Step 2) hard-requires it. Uses the host's
#                            own make if it's new enough; only builds a pinned
#                            one into build/hostmake/ when the host is too old.
#
#   setup_cross_toolchain()  Fetch + verify (SHA256) + extract the pinned
#                            Bootlin arm-*-gnueabihf cross toolchain into
#                            build/toolchain/. The T113-S3 is 32-bit ARMv7-A, so
#                            we cross-compile everything from this x86_64 host.
#
# Both are idempotent: re-running re-verifies and skips finished work.
# All pins (versions + checksums) live in forge/backends/lib.sh.
#
#   make toolchain          # do it
#   make toolchain --force  # rebuild both from scratch

set -euo pipefail

# --- locate + load the pinned config ----------------------------------------
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "${HERE}/lib.sh"

FORCE=0
[ "${1:-}" = "--force" ] && FORCE=1

log()  { printf '\033[1;34m[toolchain]\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31m[toolchain] ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

# =============================================================================
# setup_host_make — GNU Make >= 4.0 (kernel requirement)
# =============================================================================

# Is a "GNU Make X.Y" version string >= 4.0 ?
_make_ge4() { case "${1:-}" in 4.*|5.*|6.*|7.*|8.*|9.*) return 0 ;; *) return 1 ;; esac; }
# Version of the locally-built make, if present and runnable (empty otherwise).
_local_make_version()  { [ -x "${HOSTMAKE_DIR}/bin/make" ] && "${HOSTMAKE_DIR}/bin/make" --version 2>/dev/null | sed -n '1s/.*GNU Make //p' || true; }
# Version of whatever 'make' is first on PATH right now (the host's, when no
# local one exists — lib.sh only prepends hostmake if it's already built).
_system_make_version() { make --version 2>/dev/null | sed -n '1s/.*GNU Make //p' || true; }

setup_host_make() {
  [ "${FORCE}" -eq 1 ] && rm -rf "${HOSTMAKE_DIR}"

  # 1. Already built a good local make? lib.sh put it on PATH → nothing to do.
  local lv; lv="$(_local_make_version)"
  if _make_ge4 "${lv}"; then
    log "host make: using locally-built GNU Make ${lv}"
    return 0
  fi

  # 2. Is the host's OWN make new enough? Then we don't need to build anything.
  local sv; sv="$(_system_make_version)"
  if _make_ge4 "${sv}"; then
    log "host make: system GNU Make ${sv} is >= 4.0 — no local build needed"
    return 0
  fi

  # 3. Too old or missing → build the pinned GNU Make into build/hostmake/.
  log "host make: system make is '${sv:-none}' (< 4.0 or absent); building GNU Make ${MAKE_VERSION}"
  for tool in curl sha256sum tar gcc make; do
    command -v "$tool" >/dev/null 2>&1 || die "need '$tool' on PATH to build GNU Make"
  done

  mkdir -p "${DOWNLOAD_DIR}"
  local tb="${DOWNLOAD_DIR}/${MAKE_TARBALL}"
  if [ -f "${tb}" ] && echo "${MAKE_SHA256}  ${tb}" | sha256sum --check --status; then
    log "  cached ${MAKE_TARBALL} checksum OK"
  else
    log "  downloading ${MAKE_TARBALL} from ${MAKE_URL}"
    curl -fL --retry 3 --retry-delay 2 --connect-timeout 20 -o "${tb}.part" "${MAKE_URL}" \
      || die "GNU Make download failed"
    mv -f "${tb}.part" "${tb}"
    echo "${MAKE_SHA256}  ${tb}" | sha256sum --check --status \
      || die "GNU Make SHA256 mismatch (expected ${MAKE_SHA256}, got $(sha256sum "${tb}" | cut -d' ' -f1))"
    log "  SHA256 OK"
  fi

  local src="${BUILD_DIR}/make-src.tmp"
  rm -rf "${src}"; mkdir -p "${src}"
  tar -xf "${tb}" -C "${src}" --strip-components=1
  log "  configuring + building (bootstrapped by the host's old make)"
  ( cd "${src}" \
      && ./configure --prefix="${HOSTMAKE_DIR}" >/dev/null \
      && make >/dev/null 2>&1 \
      && make install >/dev/null 2>&1 ) \
    || die "GNU Make build failed"
  rm -rf "${src}"

  # Put it on PATH for the rest of this process and confirm.
  export PATH="${HOSTMAKE_DIR}/bin:${PATH}"
  lv="$(_local_make_version)"
  _make_ge4 "${lv}" || die "built make is not >= 4.0 (got '${lv:-none}')"
  log "  built GNU Make ${lv} at ${HOSTMAKE_DIR}/bin/make"
}

# =============================================================================
# setup_cross_toolchain — Bootlin arm-* cross toolchains (glibc + musl)
# =============================================================================

# fetch_toolchain <tarball> <url> <sha256> <dest_dir> <cc_prefix> <label>
# Fetch + verify (SHA256) + extract one Bootlin toolchain into dest_dir, then
# sanity-check the compiler runs and emits 32-bit ARM. Used for both the glibc
# (U-Boot/kernel) and musl (rootfs) toolchains.
fetch_toolchain() {
  local tarball="$1" url="$2" sha="$3" dest="$4" prefix="$5" label="$6"
  local DECOMP
  case "${tarball}" in
    *.tar.xz)  DECOMP=xz ;;
    *.tar.bz2) DECOMP=bzip2 ;;
    *.tar.gz)  DECOMP=gzip ;;
    *) die "unknown tarball extension for ${tarball}" ;;
  esac
  for tool in curl sha256sum tar file "${DECOMP}"; do
    command -v "$tool" >/dev/null 2>&1 || die "required tool '$tool' not found on PATH"
  done

  mkdir -p "${DOWNLOAD_DIR}" "${BUILD_DIR}"
  local tb="${DOWNLOAD_DIR}/${tarball}"

  if [ "${FORCE}" -eq 1 ]; then
    log "${label}: --force: removing cached tarball and extracted toolchain"
    rm -f "${tb}"
    rm -rf "${dest}"
  fi

  if [ -f "${tb}" ] && echo "${sha}  ${tb}" | sha256sum --check --status; then
    log "${label}: cached tarball present and checksum OK — skipping download"
  else
    [ -f "${tb}" ] && log "${label}: cached tarball failed checksum — re-downloading"
    log "${label}: downloading ${tarball}"
    log "  from ${url}"
    curl -fL --retry 3 --retry-delay 2 --connect-timeout 20 \
         -o "${tb}.part" "${url}" || die "download failed"
    mv -f "${tb}.part" "${tb}"
    log "${label}: verifying SHA256"
    echo "${sha}  ${tb}" | sha256sum --check --status \
      || die "SHA256 mismatch for ${tarball}! expected ${sha}
         got      $(sha256sum "${tb}" | cut -d' ' -f1)
         Refusing to use an unverified toolchain. If you bumped the version,
         update the matching SHA256 in forge/backends/lib.sh."
    log "${label}: SHA256 OK"
  fi

  # extract (normalize dir name so lib.sh's PATH is stable)
  local cc_bin="${dest}/bin/${prefix}gcc"
  if [ -x "${cc_bin}" ]; then
    log "${label}: already extracted at ${dest}"
  else
    log "${label}: extracting into ${dest}"
    rm -rf "${dest}" "${dest}.tmp"
    mkdir -p "${dest}.tmp"
    tar -xf "${tb}" -C "${dest}.tmp" --strip-components=1
    mv "${dest}.tmp" "${dest}"
    [ -x "${cc_bin}" ] || die "extracted, but ${cc_bin} is missing — tarball layout unexpected"
  fi

  # sanity check: compiler runs AND targets 32-bit ARM
  export PATH="${dest}/bin:${PATH}"
  log "${label}: $("${prefix}gcc" --version | head -1)"
  local TMP; TMP="$(mktemp -d)"
  printf 'int main(void){return 0;}\n' > "${TMP}/t.c"
  "${prefix}gcc" -c "${TMP}/t.c" -o "${TMP}/t.o" || { rm -rf "${TMP}"; die "${label}: test compile failed"; }
  local ARCH_LINE; ARCH_LINE="$(file "${TMP}/t.o" 2>/dev/null || true)"
  rm -rf "${TMP}"
  case "${ARCH_LINE}" in
    *ARM*) log "${label}: ok -> ${ARCH_LINE#*: }" ;;
    *) die "${label}: test object is not ARM (${ARCH_LINE})" ;;
  esac
}

setup_cross_toolchain() {
  # glibc toolchain — for U-Boot + kernel (they don't link a target libc).
  fetch_toolchain "${TOOLCHAIN_TARBALL}" "${TOOLCHAIN_URL}" "${TOOLCHAIN_SHA256}" \
                  "${TOOLCHAIN_DIR}" "${CROSS_COMPILE}" "glibc-tc"
  # musl toolchain — for the rootfs (BusyBox): smaller static binaries.
  fetch_toolchain "${ROOTFS_TC_TARBALL}" "${ROOTFS_TC_URL}" "${ROOTFS_TC_SHA256}" \
                  "${ROOTFS_TOOLCHAIN_DIR}" "${ROOTFS_CROSS_COMPILE}" "musl-tc"
}

# =============================================================================
# main
# =============================================================================
setup_host_make
setup_cross_toolchain

cat <<EOF

$(printf '\033[1;32m[toolchain] DONE\033[0m')
  Host make  : $(make --version 2>/dev/null | sed -n '1s/.*GNU Make //p')  ($( [ -x "${HOSTMAKE_DIR}/bin/make" ] && echo "built locally at build/hostmake" || echo "host's own, >= 4.0" ))
  Toolchain  : ${TC_ARCH} ${TOOLCHAIN_LIBC} ${TOOLCHAIN_CHANNEL}-${TOOLCHAIN_VERSION}  [${CROSS_COMPILE}]  (U-Boot + kernel)
  Rootfs TC  : ${TC_ARCH} musl ${TOOLCHAIN_CHANNEL}-${ROOTFS_TC_VERSION}  [${ROOTFS_CROSS_COMPILE}]  (BusyBox — smaller static)
  On PATH    : source forge/backends/lib.sh   (both compilers + make available)

Next: make bootloader BOOTLOADER=uboot
EOF
