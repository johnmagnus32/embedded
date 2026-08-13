#!/usr/bin/env bash
# classes/compile-c.sh — the "compile-c" CLASS.
#
# A class is the typed-default analogue of a Buildroot infra macro / a Yocto .bbclass: it
# DEFINES default do_* task functions a recipe gets by `inherit compile-c`. A recipe may
# override any task inline (its own do_build after the inherit line wins — bash
# last-definition-wins); the generic runner calls the do_* by name.
#
# compile-c = one .c per program in PKG_SRC_DIR, each compiled + linked against the SELECTED
# libc (via cc-profile) into a static/dynamic ELF, installed into the staging rootfs.
# (Buildroot's generic-package analogue.)
#
# Contract (generic node env — from the runner + base's do_fetch): PKG_NAME, PKG_SRC_DIR (the
#   source dir base's do_fetch resolved), NODE_SCRATCH (this class derives its obj dir under it),
#   PKG_DEST (this package's own staging dir), PKG_INSTALL (a recipe fact, defaulted to /bin here),
#   LIBC/PKG_LINK/REPO_ROOT + (custom) LIBC_STAGE_DIR/STAGE_INC. do_build sources cc-profile.sh for
#   PKG_CC/CFLAGS/LDFLAGS + LIBC_CRT/LIBC_LIB. do_fetch is left as base's default.

# do_build — compile every .c in PKG_SRC_DIR into an ELF under the scratch obj dir (compile only;
# staging into the rootfs is do_install's job).
do_build() {
  : "${PKG_NAME:?}"; : "${PKG_SRC_DIR:?compile-c do_build: PKG_SRC_DIR unset (base do_fetch sets it)}"
  : "${NODE_SCRATCH:?compile-c do_build: NODE_SCRATCH unset}"
  local PKG_BUILD_DIR="${NODE_SCRATCH}/obj"   # this class's scratch obj dir, derived from the node scratch
  # cc-profile gives PKG_CC/PKG_CFLAGS/PKG_LDFLAGS + LIBC_CRT/LIBC_LIB for the SELECTED
  # libc — the libc threads in as a build-config input, not a per-pkg branch. LIBC_CC_PROFILE
  # (from forge.conf) is the path to the selected libc's contract; the engine sources it
  # directly, with no per-libc knowledge of its own.
  : "${LIBC_CC_PROFILE:?compile-c do_build: LIBC_CC_PROFILE unset (from forge.conf)}"
  # shellcheck source=/dev/null
  source "${LIBC_CC_PROFILE}"
  mkdir -p "${PKG_BUILD_DIR}"
  echo "  [compile-c] ${PKG_NAME}: $(basename "${PKG_SRC_DIR}")/*.c (LIBC=${LIBC}/${PKG_LINK})"
  local c name out
  for c in "${PKG_SRC_DIR}"/*.c; do
    name="$(basename "${c}" .c)"
    out="${PKG_BUILD_DIR}/${name}"
    # LIBC_CRT/LIBC_LIB are empty on musl (it supplies crt+libc); on custom they
    # are gv3libc's crt0 + libc.a/.so. Word-split intentional.
    # shellcheck disable=SC2086
    "${PKG_CC}" ${PKG_CFLAGS} ${PKG_LDFLAGS} ${LIBC_CRT} "${c}" ${LIBC_LIB} -o "${out}"
  done
}

# do_install — place the built ELFs into THIS package's OWN per-package staging dir (PKG_DEST) at
# PKG_INSTALL. The rootfs step assembles the shared rootfs from every selected package's PKG_DEST
# (Buildroot's per-package model), so a package produces a durable, cacheable artifact instead of
# racing into a shared tree. Wipe PKG_DEST first so a rebuild never keeps a stale/renamed binary.
do_install() {
  : "${PKG_NAME:?}"; : "${PKG_DEST:?compile-c do_install: PKG_DEST unset}"; : "${NODE_SCRATCH:?}"
  local PKG_BUILD_DIR="${NODE_SCRATCH}/obj" dest="${PKG_INSTALL:-/bin}"
  rm -rf "${PKG_DEST}"; mkdir -p "${PKG_DEST}${dest}"
  local out name installed=0
  for out in "${PKG_BUILD_DIR}"/*; do
    [ -f "${out}" ] || continue
    name="$(basename "${out}")"
    install -m 0755 "${out}" "${PKG_DEST}${dest}/${name}"
    installed=$((installed + 1))
  done
  echo "  [compile-c] ${PKG_NAME}: installed ${installed} program(s) into ${dest} (pkgstage)"
}
