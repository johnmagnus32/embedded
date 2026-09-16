# hostpackages/toolchain-custom/recipe.sh — OUR from-scratch toolchain (cpp/cc/as/ar/ld + the forge-cc
# driver), as a HOST PACKAGE. Selected by TOOLCHAIN=custom (with LIBC=custom): the rootfs libc — and any
# compile-c package — is built with OUR tools instead of a GNU cross-gcc. Unlike the prebuilt/source
# toolchains (a foreign gcc), this one is FIVE small programs we build from the repo's toolchain/ tree,
# then expose behind a gcc-shaped front end so os's compile classes drive it unchanged.
#
# Layout it stages under build/toolchain-custom/bin (on PATH via TOOLCHAIN_DIR):
#   forge-cpp forge-cc1 forge-as forge-ld forge-ar   the raw tools (the driver finds them as siblings)
#   arm-forge-custom-gcc                             the forge-cc driver (.c/.S -> .o|ELF; PKG_CC)
#   arm-forge-custom-{ar,as,ld}                      prefixed names build.sh / make-c invoke
PKG_NAME=toolchain-custom
PKG_CLASS=cross
PKG_PROVIDES="virtual/cross-cc virtual/cross-cc-initial"         # our tools are BOTH stages (no bootstrap split — no foreign libc to build against)
PKG_FETCH=local
PKG_SOURCE=toolchain                 # REPO_ROOT/toolchain — the five tool sources + the forge-cc driver
PKG_HOST_DEST=${BUILD_DIR}/toolchain-custom
PKG_HOST_CC_PREFIX=arm-forge-custom-

do_install() { :; }

do_build() {
  : "${PKG_SRC_DIR:?toolchain-custom: PKG_SRC_DIR unset (base do_fetch sets it)}"
  : "${PKG_HOST_DEST:?toolchain-custom: PKG_HOST_DEST unset}"
  local src="${PKG_SRC_DIR}" dest="${PKG_HOST_DEST}" pfx="${PKG_HOST_CC_PREFIX}" bin
  bin="${dest}/bin"
  command -v cc >/dev/null 2>&1 || die "toolchain-custom: need host 'cc' to build the tools"

  log "toolchain-custom: building cpp/cc/as/ar/ld from ${src}"
  local tool
  for tool in cpp cc as ar ld; do
    make -s -C "${src}/${tool}" >/dev/null || die "toolchain-custom: build failed for ${tool}"
  done

  # Fresh stage (a rebuild reaching here wants a re-provision — the taskhash cache already skipped us
  # if nothing changed). Raw tools under forge- names; the driver locates them by its own dir.
  rm -rf "${dest}"; mkdir -p "${bin}"
  install -m0755 "${src}/cpp/build/cpp" "${bin}/forge-cpp"
  install -m0755 "${src}/cc/build/cc"   "${bin}/forge-cc1"
  install -m0755 "${src}/as/build/as"   "${bin}/forge-as"
  install -m0755 "${src}/ld/build/ld"   "${bin}/forge-ld"
  install -m0755 "${src}/ar/build/ar"   "${bin}/forge-ar"
  install -m0755 "${src}/forge-cc"      "${bin}/${pfx}gcc"
  ln -sf forge-ar "${bin}/${pfx}ar"
  ln -sf forge-as "${bin}/${pfx}as"
  ln -sf forge-ld "${bin}/${pfx}ld"

  # sanity: the driver compiles + links a trivial program to a 32-bit ARM object
  local TMP; TMP="$(mktemp -d)"
  printf 'int main(void){return 0;}\n' > "${TMP}/t.c"
  "${bin}/${pfx}gcc" -c "${TMP}/t.c" -o "${TMP}/t.o" || { rm -rf "${TMP}"; die "toolchain-custom: driver test compile failed"; }
  local A; A="$(file "${TMP}/t.o" 2>/dev/null || true)"; rm -rf "${TMP}"
  case "${A}" in *ARM*) log "toolchain-custom: ok -> ${A#*: }" ;; *) die "toolchain-custom: test object is not ARM (${A})" ;; esac
}
