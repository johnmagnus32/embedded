# providers/init/runit/recipe.sh — runit as PID-1 (INIT=runit): a small process-supervision suite
# (Void Linux's init). Cross-built the Buildroot way: point runit's conf-cc/conf-ld at the rootfs
# musl toolchain (static), and seed the two RUN-probe headers so their compile-AND-run rules never
# fire under cross (chkshsgr/tryshsgr -> hasshsgr.h, trypoll -> iopause.h) — `make -o` marks those
# up-to-date. Stages the binaries + /etc/runit/{1,2,3} + an empty /etc/service; /init execs runit.
# Per-service run scripts are NOT baked here — a package ships its own service/<n>/run and installs
# it into /etc/service/<n>/run (see the console package), which runsvdir picks up.
PKG_NAME=runit
PKG_FETCH=tarball
PKG_VERSION=2.1.2
PKG_SITE=http://smarden.org/runit
PKG_SOURCE=runit-2.1.2.tar.gz
PKG_SHA256=6fd0160cb0cf1207de4e66754b6d39750cff14bb0aa66ab49490992c0c47ba18
PKG_DEPENDS=libc
PKG_ARTIFACT=stage:

# The tarball extracts to admin/runit-<ver>/ (base do_fetch strips one component -> runit-<ver>/).
_runit_src() { echo "${PKG_SRC_DIR}/runit-${PKG_VERSION}/src"; }

do_build() {
  : "${PKG_SRC_DIR:?}"; : "${PKG_VERSION:?}"
  : "${ROOTFS_CROSS_COMPILE:?runit do_build: ROOTFS_CROSS_COMPILE unset}"
  local rsrc; rsrc="$(_runit_src)"
  [ -d "${rsrc}" ] || die "runit: source not at ${rsrc} (tarball layout changed?)"
  cd "${rsrc}"
  local CC="${ROOTFS_CROSS_COMPILE}gcc"
  command -v "${CC}" >/dev/null 2>&1 || die "runit: ${CC} not on PATH — run 'make toolchain'"

  # Cross config: first line is the command runit's DJB compile/load scripts read. Static -> the
  # binaries are self-contained in the initramfs (no ld.so needed).
  printf '%s\n' "${CC} -O2 -Wall" > conf-cc
  printf '%s\n' "${CC} -static -s" > conf-ld
  # Seed the RUN-probe headers (the only cross-hostile step). Modern Linux setgroups takes gid_t,
  # not short -> no HASSHORTSETGROUPS (hasshsgr.h1); Linux has poll(2) (iopause.h2). `make -o`
  # treats them as up-to-date so chkshsgr / tryshsgr / trypoll never execute.
  cp -f hasshsgr.h1 hasshsgr.h
  cp -f iopause.h2  iopause.h
  echo "  [runit] cross-compiling ${PKG_VERSION} (static musl, -j$(nproc))"
  make -o hasshsgr.h -o iopause.h >/dev/null
  [ -x runit ] && [ -x runsvdir ] || die "runit: build produced no runit/runsvdir"
}

do_install() {
  : "${PKG_DEST:?runit do_install: PKG_DEST unset}"; : "${PKG_SRC_DIR:?}"
  local rsrc b; rsrc="$(_runit_src)"
  rm -rf "${PKG_DEST}"; mkdir -p "${PKG_DEST}/sbin"
  for b in runit runit-init runsv runsvdir runsvchdir sv svlogd chpst utmpset; do
    install -m 0755 "${rsrc}/${b}" "${PKG_DEST}/sbin/${b}"
  done
  # /init sets PATH (the kernel starts PID 1 with none) then execs runit; runit runs the stages by
  # absolute path. The stage scripts live beside this recipe.
  install -D -m 0755 "${RECIPE_DIR}/init"   "${PKG_DEST}/init"
  install -D -m 0755 "${RECIPE_DIR}/stage1" "${PKG_DEST}/etc/runit/1"
  install -D -m 0755 "${RECIPE_DIR}/stage2" "${PKG_DEST}/etc/runit/2"
  install -D -m 0755 "${RECIPE_DIR}/stage3" "${PKG_DEST}/etc/runit/3"
  # runsvdir watches /etc/service; ship it empty so the watch dir always exists. Real services come
  # from packages, each installing its own /etc/service/<n>/run (see the console package).
  install -d -m 0755 "${PKG_DEST}/etc/service"
}
