#!/usr/bin/env bash
# host-tarball-bin.sh — the `host-tarball-bin` host build STYLE: for a PREBUILT binary tarball
# (today only the Bootlin arm-* cross toolchains). The FRAMEWORK's do_fetch (base.sh) fetches +
# SHA-verifies the tarball declared in PKG_SOURCES; do_build extracts it (normalizing the on-PATH
# layout) and sanity-checks the compiler runs and emits 32-bit ARM. SHA mismatch is fatal in
# do_fetch (never run an unverified toolchain).
#
# Recipe facts: PKG_SOURCES="<name>" + PKG_SRC_<name>=<url> + PKG_SHA_<name> (the tarball),
#   PKG_HOST_DEST (extract dir), PKG_HOST_CC_PREFIX (the <triple>- to check <triple>gcc).
# do_install is a no-op; do_fetch is base's (declarative PKG_SOURCES).

do_install() { :; }

do_build() {
  : "${PKG_NAME:?host-tarball-bin: PKG_NAME unset}"
  : "${PKG_SOURCES:?${PKG_NAME}: PKG_SOURCES unset (declare the tarball name + PKG_SRC_/PKG_SHA_)}"
  : "${PKG_HOST_DEST:?${PKG_NAME}: PKG_HOST_DEST (extract dir) unset}"
  local prefix="${PKG_HOST_CC_PREFIX:-}" dest="${PKG_HOST_DEST}"
  local tb tarball DECOMP
  tb="$(pkg_src "${PKG_SOURCES%% *}")"   # base.sh's do_fetch already fetched+verified it
  tarball="${tb##*/}"
  case "${tarball}" in
    *.tar.xz)  DECOMP=xz ;;
    *.tar.bz2) DECOMP=bzip2 ;;
    *.tar.gz)  DECOMP=gzip ;;
    *) die "${PKG_NAME}: unknown tarball extension for ${tarball}" ;;
  esac
  for tool in tar file "${DECOMP}"; do
    command -v "$tool" >/dev/null 2>&1 || die "${PKG_NAME}: required tool '$tool' not on PATH"
  done

  # extract (normalize the dir name so the on-PATH layout is stable). ALWAYS re-extract when this
  # runs: the uniform taskhash cache (engine.sh) already skipped the whole node when the tree
  # was up to date, so reaching here means a (re)provision is wanted — and a stale tree from a
  # prior version must NOT be kept.
  local cc_bin="${dest}/bin/${prefix}gcc"
  log "${PKG_NAME}: extracting into ${dest}"
  rm -rf "${dest}" "${dest}.tmp"; mkdir -p "${dest}.tmp"
  tar -xf "${tb}" -C "${dest}.tmp" --strip-components=1
  mv "${dest}.tmp" "${dest}"

  # sanity check: the cross compiler runs AND targets 32-bit ARM
  if [ -n "${prefix}" ]; then
    [ -x "${cc_bin}" ] || die "${PKG_NAME}: extracted but ${cc_bin} missing — tarball layout unexpected"
    export PATH="${dest}/bin:${PATH}"
    log "${PKG_NAME}: $("${prefix}gcc" --version | head -1)"
    local TMP; TMP="$(mktemp -d)"
    printf 'int main(void){return 0;}\n' > "${TMP}/t.c"
    "${prefix}gcc" -c "${TMP}/t.c" -o "${TMP}/t.o" || { rm -rf "${TMP}"; die "${PKG_NAME}: test compile failed"; }
    local ARCH_LINE; ARCH_LINE="$(file "${TMP}/t.o" 2>/dev/null || true)"; rm -rf "${TMP}"
    case "${ARCH_LINE}" in
      *ARM*) log "${PKG_NAME}: ok -> ${ARCH_LINE#*: }" ;;
      *) die "${PKG_NAME}: test object is not ARM (${ARCH_LINE})" ;;
    esac
  fi
}
