#!/usr/bin/env bash
# host-pyvenv.sh — the `host-pyvenv` host build STYLE: create an isolated python venv and
# pip-install a module list. Today's one consumer is binman-venv (U-Boot's binman/pylibfdt).
#
# Three traps this picks a host python around: (1) never a cross-toolchain's bundled python
# (lacks the modules AND ssl); (2) require Python.h (pylibfdt compiles a C extension);
# (3) install from real PyPI via --index-url (the host default index is a creds-gated
# mirror that 401s for these public packages).
#
# Inputs (recipe facts, exported by host.sh): PKG_NAME, PKG_PYMODULES, PKG_HOST_DEST (venv
#   dir), TOOLCHAIN_DIR + ROOTFS_TOOLCHAIN_DIR (to exclude their bundled pythons).
# do_fetch/do_install: a host tool self-fetches inside do_build (via forge_fetch_file) and
# installs into its host prefix there — so these are no-ops (the uniform contract still calls them).

PKG_TARGET_INDEPENDENT=1   # built with host cc: omit cross-toolchain/arch/board from the taskhash

do_fetch()   { :; }
do_install() { :; }

do_build() {
  : "${PKG_NAME:?host-pyvenv: PKG_NAME unset}"
  : "${PKG_PYMODULES:?${PKG_NAME}: PKG_PYMODULES (module list) unset}"
  : "${PKG_HOST_DEST:?${PKG_NAME}: PKG_HOST_DEST (venv dir) unset}"

  log "${PKG_NAME}: creating python venv (${PKG_HOST_DEST}) for: ${PKG_PYMODULES}"
  # All three of (a) not a toolchain python, (b) Python.h, (c) ssl are required — see header.
  _py_usable() {
    local p="$1" inc
    "$p" -c 'import ssl' 2>/dev/null || return 1
    inc="$("$p" -c 'import sysconfig; print(sysconfig.get_path("include"))' 2>/dev/null)" || return 1
    [ -n "$inc" ] && [ -f "$inc/Python.h" ]
  }
  local HOST_PY="" cand
  # pyenv pythons first — on this host they carry both ssl and headers, unlike system or
  # toolchain pythons.
  local cands="$(ls -1 "${HOME}"/.pyenv/versions/*/bin/python3 2>/dev/null || true)"
  cands="${cands} /usr/bin/python3 /usr/local/bin/python3 $(command -v python3 || true)"
  for cand in ${cands}; do
    case "${cand}" in
      "${TOOLCHAIN_DIR:-/nonexistent}"/*)        continue ;;
      "${ROOTFS_TOOLCHAIN_DIR:-/nonexistent}"/*) continue ;;
    esac
    [ -x "${cand}" ] || continue
    if _py_usable "${cand}"; then HOST_PY="${cand}"; break; fi
  done
  [ -n "${HOST_PY}" ] || die "${PKG_NAME}: no host python3 with ssl + dev headers (Python.h) found — install python3-dev/python3-devel (+ ssl), or ensure a pyenv python is available. pylibfdt needs headers to compile; pip needs ssl to fetch."
  log "${PKG_NAME}: using host python ${HOST_PY} ($("${HOST_PY}" --version 2>&1))"
  # Wipe first: `python -m venv` onto an existing dir reuses it, so a venv left half-built
  # by a failed prior attempt would be reused broken. Wiping makes the step self-healing.
  rm -rf "${PKG_HOST_DEST}"
  "${HOST_PY}" -m venv "${PKG_HOST_DEST}" || die "${PKG_NAME}: venv creation failed"
  # --index-url: real PyPI, since the host default index is a creds-gated mirror.
  # shellcheck disable=SC2086
  "${PKG_HOST_DEST}/bin/pip" install --quiet --index-url https://pypi.org/simple/ ${PKG_PYMODULES} \
    || die "${PKG_NAME}: pip install failed (${PKG_PYMODULES})"
  # sanity: modules import from the venv python the build will actually use
  "${PKG_HOST_DEST}/bin/python3" -c 'import setuptools, elftools, yaml' 2>/dev/null \
    || die "${PKG_NAME}: venv missing binman deps after install"
}
