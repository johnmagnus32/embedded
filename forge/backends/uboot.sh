#!/usr/bin/env bash
# uboot.sh — build mainline U-Boot (SPL + U-Boot proper) for the
# T113-S3, with the console moved to UART0/PE2-PE3 for our t113-breakout board.
#
# What it does:
#   1. Clone mainline U-Boot at the pinned tag (or reuse an existing checkout).
#   2. Reset any tracked files we patch back to pristine (idempotent re-runs).
#   3. make mangopi_mq_r_defconfig, then flip CONFIG_CONS_INDEX=1 (SPL console
#      → PE2/PE3) reproducibly via a .config fragment + olddefconfig.
#   4. Overlay the control DTB so U-Boot *proper* also uses UART0 (see
#      board/t113-gameboy/uart0-console.dtsi for the full why — CONS_INDEX alone is a trap).
#   5. Build → u-boot-sunxi-with-spl.bin, copied to build/output/.
#
# The T113 is 32-bit ARMv7 (no EL3) → NO TF-A/BL31 is needed; the build emits no
# BL31 warning (CONFIG_SUNXI_BL31_BASE=0 for R528). Both Cortex-A7 cores come up
# at runtime via U-Boot's built-in ARMv7 PSCI monitor (MACH_SUN8I_R528 selects
# it), which patches enable-method="psci" into the kernel DTB at boot.
#
#   make bootloader BOOTLOADER=uboot            # build (idempotent)
#   make bootloader BOOTLOADER=uboot --clean    # wipe the U-Boot build tree first
#
# Prereq: make toolchain   (this script checks the compiler exists)

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "${HERE}/lib.sh"

log()  { printf '\033[1;34m[uboot]\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31m[uboot] ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

CLEAN=0
[ "${1:-}" = "--clean" ] && CLEAN=1

# --- preflight ---------------------------------------------------------------
command -v git >/dev/null 2>&1 || die "git not found"
command -v "${CROSS_COMPILE}gcc" >/dev/null 2>&1 \
  || die "cross compiler '${CROSS_COMPILE}gcc' not on PATH — run make toolchain first"
# Host tools U-Boot's sunxi+SPL (binman) build needs. dtc is optional (U-Boot
# builds its own), so we don't hard-require it.
for t in bison flex bc swig; do
  command -v "$t" >/dev/null 2>&1 || die "host build dep '$t' missing (apt: bison flex bc swig libssl-dev)"
done

# --- python venv for binman/pylibfdt -----------------------------------------
# The Bootlin toolchain ships its own python3 (no setuptools/pyelftools/pyyaml),
# which would shadow the host python and break binman. Create an isolated venv
# with those modules so the build is self-contained + reproducible under build/.
# Uses real PyPI explicitly (the host's default pip index is a creds-gated
# CodeArtifact mirror that 401s for these public packages).
PYVENV_MARKER="${PYENV_DIR}/.deps-ok"
if [ ! -f "${PYVENV_MARKER}" ]; then
  log "creating python venv for binman deps (${PYENV_DIR})"
  # Pick a host python >=3.6 that is NOT the toolchain's AND has dev headers
  # (Python.h). pylibfdt compiles a C extension (libfdt_wrap.c), so a python
  # without its -dev headers fails at build time with "Python.h: No such file".
  # We probe candidates and require Python.h to exist for the chosen one.
  py_has_headers() {
    local p="$1" inc
    inc="$("$p" -c 'import sysconfig; print(sysconfig.get_path("include"))' 2>/dev/null)" || return 1
    [ -n "$inc" ] && [ -f "$inc/Python.h" ]
  }
  HOST_PY=""
  # search: common system paths, PATH python3s, and any pyenv/conda pythons
  CANDS="/usr/bin/python3 /usr/local/bin/python3 $(command -v python3 || true)"
  CANDS="$CANDS $(ls -1 "$HOME"/.pyenv/versions/*/bin/python3 2>/dev/null || true)"
  for cand in $CANDS; do
    case "$cand" in "${TOOLCHAIN_DIR}"/*) continue ;; esac   # never the toolchain's
    [ -x "$cand" ] || continue
    if py_has_headers "$cand"; then HOST_PY="$cand"; break; fi
  done
  [ -n "${HOST_PY}" ] || die "no host python3 with dev headers (Python.h) found — install python3-dev/python3-devel, or ensure a pyenv python is available. pylibfdt needs it to compile."
  log "using host python: ${HOST_PY} ($("${HOST_PY}" --version 2>&1))"
  "${HOST_PY}" -m venv "${PYENV_DIR}" || die "python venv creation failed"
  # setuptools+pyelftools+pyyaml: pylibfdt/binman core deps.
  # importlib_resources: binman needs importlib.resources.files(), which only
  # exists natively in python >=3.9; this backport covers 3.6-3.8 hosts.
  "${PYENV_DIR}/bin/pip" install --quiet --index-url https://pypi.org/simple/ \
      setuptools pyelftools pyyaml importlib_resources \
      || die "pip install of binman deps failed"
  touch "${PYVENV_MARKER}"
fi
# Prepend the venv so 'python'/'python3' resolve to it (ahead of the toolchain's).
PATH="${PYENV_DIR}/bin:${PATH}"
export PATH PYTHON="${PYENV_DIR}/bin/python3"
# sanity: the modules must import from the python the build will actually use
python3 -c 'import setuptools, elftools, yaml' 2>/dev/null \
  || die "build venv missing binman deps (setuptools/pyelftools/pyyaml)"

BOARD_DTS_REL="dts/upstream/src/arm/allwinner/${UBOOT_BOARD_DT}.dts"
BOARD_DTS_DIR_REL="dts/upstream/src/arm/allwinner"
CONSOLE_OVERLAY="uart0-console.dtsi"
INCLUDE_LINE="#include \"${CONSOLE_OVERLAY}\""

# --- 1. clone or reuse -------------------------------------------------------
if [ "${CLEAN}" -eq 1 ]; then
  log "--clean: removing ${UBOOT_SRC_DIR}"
  rm -rf "${UBOOT_SRC_DIR}"
fi

if [ -d "${UBOOT_SRC_DIR}/.git" ]; then
  GOT_TAG="$(git -C "${UBOOT_SRC_DIR}" describe --tags 2>/dev/null || echo '?')"
  if [ "${GOT_TAG}" != "${UBOOT_TAG}" ]; then
    die "existing checkout is '${GOT_TAG}', want '${UBOOT_TAG}'. Re-run with --clean to re-clone."
  fi
  log "reusing U-Boot checkout at ${UBOOT_TAG}"
else
  log "cloning U-Boot ${UBOOT_TAG} (shallow) — this is the slow part"
  git_clone_pinned "${UBOOT_SRC_DIR}" "${UBOOT_TAG}" "${UBOOT_GIT_URL}" "${UBOOT_GIT_URL_MIRROR}" \
    || die "git clone failed (tried ${UBOOT_GIT_URL} and mirror)"
fi

cd "${UBOOT_SRC_DIR}"
[ -f "${BOARD_DTS_REL}" ] || die "board DTS not found at ${BOARD_DTS_REL} (tag layout changed?)"

# --- 2. reset patched tracked files to pristine (idempotency) ----------------
# We modify the board .dts (tracked). Restore it from git so a re-run doesn't
# stack duplicate #include lines. The overlay .dtsi is untracked → just recopy.
log "restoring pristine board DTS"
git checkout -- "${BOARD_DTS_REL}"

# --- 3. defconfig + config tweaks --------------------------------------------
log "make ${UBOOT_DEFCONFIG}"
make "${UBOOT_DEFCONFIG}" >/dev/null

# Reproducible, non-interactive config edits. We remove each symbol's existing
# line then append the wanted value, and let olddefconfig normalize.
#
# CONFIG_CONS_INDEX=1        -> SPL console pinmux to UART0/PE2-PE3 (our board).
# The remaining three disable HOST-TOOL features that need libraries this build
# host lacks and that a sunxi boot image does NOT need:
#   TOOLS_LIBCRYPTO=n   host OpenSSL is 1.0.2 (too old for U-Boot's signing
#                       tools' 1.1+ API); we don't sign images. Also drags via
#                       TOOLS_KWBIMAGE (Marvell — irrelevant to sunxi).
#   TOOLS_KWBIMAGE=n    selects TOOLS_LIBCRYPTO; not needed for Allwinner.
#   TOOLS_MKEFICAPSULE=n needs gnutls headers (absent); UEFI capsule update is
#                       not used for our SD boot.
log "applying config fragment (CONS_INDEX + host-tool trims)"
sed -i '/^CONFIG_CONS_INDEX=/d;/CONFIG_TOOLS_LIBCRYPTO/d;/CONFIG_TOOLS_KWBIMAGE/d;/CONFIG_TOOLS_MKEFICAPSULE/d' .config
cat >> .config <<EOF
CONFIG_CONS_INDEX=${UBOOT_CONS_INDEX}
# CONFIG_TOOLS_LIBCRYPTO is not set
# CONFIG_TOOLS_KWBIMAGE is not set
# CONFIG_TOOLS_MKEFICAPSULE is not set
EOF
make olddefconfig >/dev/null
# verify the load-bearing one stuck (olddefconfig can silently drop a bad symbol)
grep -q "^CONFIG_CONS_INDEX=${UBOOT_CONS_INDEX}$" .config \
  || die "CONFIG_CONS_INDEX did not take — check symbol name for this U-Boot version"

# --- 3b. SPI-NOR flash support (the `sf` command) ----------------------------
# The NOR boot path FEL-loads U-Boot proper and drives `sf probe`/`sf read` to
# pull the kernel+DTB+initramfs out of the on-board W25Q128 (see flash.sh). The
# sunxi defconfig does NOT enable this, so add it reproducibly here.
#
# ENABLE ORDER MATTERS: the whole SPI-flash Kconfig menu is behind `if MTD`, so
# MTD must be enabled FIRST or olddefconfig silently drops SPI_FLASH/CMD_SF (this
# bit us — see BOOT-MATRIX.md §"Reproducible sf build"). scripts/config applies in
# order, then olddefconfig resolves deps; we verify the load-bearing symbols stuck.
log "enabling SPI-NOR flash (MTD -> SPI_FLASH -> CMD_SF; W25Q128 = Winbond)"
./scripts/config --enable MTD --enable SPI_FLASH --enable DM_SPI_FLASH \
                 --enable SPI_FLASH_WINBOND --enable SPI_FLASH_SFDP_SUPPORT --enable CMD_SF
make olddefconfig >/dev/null
grep -q "^CONFIG_CMD_SF=y$" .config \
  || die "CONFIG_CMD_SF did not take — SPI-flash menu likely still gated (MTD must precede it)"
grep -q "^CONFIG_SPI_FLASH=y$" .config \
  || die "CONFIG_SPI_FLASH did not take — check the MTD-first enable order"

# --- 4. control-DTB console overlay (U-Boot proper console → UART0) ----------
log "applying UART0 console overlay to the control DTB"
cp -f "${BOARD_DIR}/${CONSOLE_OVERLAY}" "${BOARD_DTS_DIR_REL}/${CONSOLE_OVERLAY}"
# Append the include once (pristine .dts was just restored, so it's absent).
printf '\n%s\n' "${INCLUDE_LINE}" >> "${BOARD_DTS_REL}"
grep -qF "${INCLUDE_LINE}" "${BOARD_DTS_REL}" || die "failed to append console overlay include"

# --- 4b. control-DTB NOR overlay (so `sf probe` finds the on-board W25Q128) ---
# CONFIG_CMD_SF alone gives the `sf` COMMAND, but `sf probe` still needs the chip
# DECLARED in U-Boot proper's control DTB (spi0 + flash@0 jedec,spi-nor on CS0/PC3)
# or it reports "No SPI flash selected". Same overlay pattern as the console one.
# Verified on silicon: sf probe -> "Detected w25q128 ... 16 MiB" (BOOT-MATRIX.md).
NOR_OVERLAY="nor-spi.dtsi"
NOR_INCLUDE="#include \"${NOR_OVERLAY}\""
log "applying SPI-NOR overlay to the control DTB (declares flash@0 for sf probe)"
cp -f "${BOARD_DIR}/${NOR_OVERLAY}" "${BOARD_DTS_DIR_REL}/${NOR_OVERLAY}"
printf '\n%s\n' "${NOR_INCLUDE}" >> "${BOARD_DTS_REL}"
grep -qF "${NOR_INCLUDE}" "${BOARD_DTS_REL}" || die "failed to append NOR overlay include"

# --- 5. build ----------------------------------------------------------------
JOBS="$(nproc)"
log "building U-Boot (-j${JOBS}) ..."
make -j"${JOBS}"

[ -f "${UBOOT_IMAGE}" ] || die "build finished but ${UBOOT_IMAGE} not found"

mkdir -p "${OUTPUT_DIR}"
cp -f "${UBOOT_IMAGE}" "${OUTPUT_DIR}/${UBOOT_IMAGE}"

# --- report ------------------------------------------------------------------
SIZE="$(du -h "${OUTPUT_DIR}/${UBOOT_IMAGE}" | cut -f1)"
cat <<EOF

$(printf '\033[1;32m[uboot] DONE\033[0m')
  U-Boot     : ${UBOOT_TAG}  (${UBOOT_DEFCONFIG})
  Console    : UART0 / PE2(TX) PE3(RX) @ 115200  [CONS_INDEX=${UBOOT_CONS_INDEX} + DTB overlay]
  Cores      : both A7s via U-Boot ARMv7 PSCI (no TF-A/BL31 needed)
  Artifact   : ${OUTPUT_DIR}/${UBOOT_IMAGE}  (${SIZE})

  Flash to a microSD (Phase 1, all-on-SD) at the 8 KiB offset:
    sudo dd if=${OUTPUT_DIR}/${UBOOT_IMAGE} of=/dev/sdX bs=1024 seek=8 conv=fsync

Next: make kernel KERNEL=mainline
EOF
