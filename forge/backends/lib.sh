#!/usr/bin/env bash
# forge/backends/lib.sh — the ENGINE's shared build mechanism (the generic "how").
#
# This is the reusable half of the old projects/gameboy-v3/scripts/env.sh, moved
# OUT of the product into the engine (forge refactor Phase 6). It holds only what
# is generic across products: the build-tree layout, the HOST-constrained cross
# toolchains + GNU make pins (chosen by the BUILD HOST, not the target), PATH/venv
# setup, and the fetch helper. It is the shell analogue of a Buildroot package.mk:
# a generic recipe parameterized on the product.
#
# The PER-PRODUCT "what" is data the product supplies, sourced below:
#   $PRODUCT_DIR/versions.env            OSS component pins (kernel/uboot/busybox)
#   $PRODUCT_DIR/board/$BOARD_NAME/board.env    board build facts (defconfigs, DT, console)
#   $PRODUCT_DIR/board/$BOARD_NAME/layout.env   memory/storage layout (NOR/DRAM/SD)
#   $PRODUCT_DIR/board/$BOARD_NAME/board.mk     provider build targets (KERNEL/ROOTFS_TARGET)
#
# CONTRACT: the caller (forge/*.mk, or a human via `make ...`) MUST set PRODUCT_DIR
# and BOARD_NAME in the environment. The backends do NOT assume a product — that is
# exactly the inversion this refactor fixed (a generic recipe must not hardcode one
# product). Every backend does:  source "$(dirname "$0")/lib.sh"
#
# Reproducibility rule: versions + checksums are PINNED (host pins here, component
# pins in the product's versions.env). Never use "latest" — bump a pin deliberately.

# --- required inputs ---------------------------------------------------------
: "${PRODUCT_DIR:?lib.sh: PRODUCT_DIR must be set (run via 'make <target>', which passes it, or export it)}"
PRODUCT_DIR="$(cd "${PRODUCT_DIR}" && pwd)"        # normalize to absolute
: "${BOARD_NAME:?lib.sh: BOARD_NAME must be set (forge passes it from config.mk BOARD; or export it)}"

# --- resolve the engine + repo roots from OUR OWN location -------------------
# lib.sh lives at forge/backends/lib.sh, so the repo root is two dirs up. This is
# how the engine finds the repo-root PROVIDERS (kernel/, bootloader/, libc/, …)
# regardless of which product it is building.
_LIB_SH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ ! -f "${_LIB_SH}/lib.sh" ]; then
  echo "lib.sh: could not resolve my own location (got '${_LIB_SH}')." >&2
  return 1 2>/dev/null || exit 1
fi
FORGE_DIR="$(cd "${_LIB_SH}/.." && pwd)"          # forge/
REPO_ROOT="$(cd "${FORGE_DIR}/.." && pwd)"        # repo root (holds the providers)

# --- build-tree layout (product-agnostic SHAPE, rooted under the product) ----
# All build inputs/outputs live under the product's build/ (git-ignored). One
# tree, easy to nuke and rebuild — that is the reproducibility test.
BUILD_DIR="${PRODUCT_DIR}/build"
DOWNLOAD_DIR="${BUILD_DIR}/downloads"             # fetched tarballs (cached)
TOOLCHAIN_DIR="${BUILD_DIR}/toolchain"            # extracted glibc cross-toolchain
ROOTFS_TOOLCHAIN_DIR="${BUILD_DIR}/toolchain-musl" # extracted musl cross-toolchain
UBOOT_SRC_DIR="${BUILD_DIR}/u-boot"               # U-Boot git checkout
OUTPUT_DIR="${BUILD_DIR}/output"                  # final flashable artifacts
PYENV_DIR="${BUILD_DIR}/pyenv"                    # host python venv (binman deps)
HOSTMAKE_DIR="${BUILD_DIR}/hostmake"              # locally-built GNU Make >=4
BUSYBOX_SRC_DIR="${BUILD_DIR}/busybox"            # BusyBox source
ROOTFS_DIR="${BUILD_DIR}/rootfs"                  # assembled initramfs root

# --- board + overlay (product BUILD INPUTS) ----------------------------------
BOARD_DIR="${PRODUCT_DIR}/board/${BOARD_NAME}"
OVERLAY_DIR="${PRODUCT_DIR}/overlay"
[ -d "${BOARD_DIR}" ] || { echo "lib.sh: board dir '${BOARD_DIR}' not found (BOARD_NAME=${BOARD_NAME})" >&2; return 1 2>/dev/null || exit 1; }

# =============================================================================
# HOST-CONSTRAINED PINS  (engine tier — chosen by the BUILD HOST, not the product)
# =============================================================================
#
# Cross toolchain: Bootlin prebuilt, armv7-eabihf (hard-float), glibc. The T113-S3
# is 32-bit ARMv7-A (Cortex-A7), so an arm .*-gnueabihf toolchain, NOT arm-none-eabi.
#
# VERSION CHOICE — constrained by the HOST, not the target (why this is engine-tier):
#   * This build host is glibc 2.26. Bootlin's NEWER toolchains (e.g. 2024.x /
#     2025.08) ship a binutils `ld` linked against GLIBC_2.27, so its `ld` literally
#     won't run here — U-Boot's build dies at the first LINK step.
#   * U-Boot v2026.04 requires host/target GCC >= 10 (arch/arm/config.mk checkgcc10).
#   => 2021.11 is the sweet spot: GCC 10.3.0 (satisfies checkgcc10) AND its `ld`
#      only needs GLIBC_2.14 (runs on this host).
#   If you build on a newer host (glibc >= 2.27), bump to 2025.08-1
#      (sha 97d6fbaf…f9b4, .tar.xz) for a newer GCC.
# To bump: pick a dir from
#   https://toolchains.bootlin.com/downloads/releases/toolchains/armv7-eabihf/tarballs/
# then update version, SHA256 (from the matching .sha256), AND extension (older
# releases are .tar.bz2, newer .tar.xz).
TOOLCHAIN_ARCH="armv7-eabihf"
TOOLCHAIN_LIBC="glibc"
TOOLCHAIN_CHANNEL="stable"
TOOLCHAIN_VERSION="2021.11-1"
TOOLCHAIN_EXT="tar.bz2"
TOOLCHAIN_TARBALL="${TOOLCHAIN_ARCH}--${TOOLCHAIN_LIBC}--${TOOLCHAIN_CHANNEL}-${TOOLCHAIN_VERSION}.${TOOLCHAIN_EXT}"
TOOLCHAIN_URL="https://toolchains.bootlin.com/downloads/releases/toolchains/${TOOLCHAIN_ARCH}/tarballs/${TOOLCHAIN_TARBALL}"
TOOLCHAIN_SHA256="6d10f356811429f1bddc23a174932c35127ab6c6f3b738b768f0c29c3bf92f10"

# The cross-compiler prefix. Bootlin toolchains are built WITH Buildroot, so the
# triple is arm-buildroot-linux-gnueabihf- (not the generic arm-linux-gnueabihf-).
CROSS_COMPILE="arm-buildroot-linux-gnueabihf-"
ARCH="arm"
export CROSS_COMPILE ARCH

# A SECOND, musl toolchain (rootfs only). U-Boot + the kernel don't link a target
# libc, so they stay on glibc above. The ROOTFS (BusyBox / our libc) links libc,
# and musl makes the static binary ~34% smaller + shrinks the syscall surface.
# Same 2021.11 vintage so its `ld` runs on this host.
ROOTFS_TC_VERSION="2021.11-1"
ROOTFS_TC_EXT="tar.bz2"
ROOTFS_TC_TARBALL="${TOOLCHAIN_ARCH}--musl--${TOOLCHAIN_CHANNEL}-${ROOTFS_TC_VERSION}.${ROOTFS_TC_EXT}"
ROOTFS_TC_URL="https://toolchains.bootlin.com/downloads/releases/toolchains/${TOOLCHAIN_ARCH}/tarballs/${ROOTFS_TC_TARBALL}"
ROOTFS_TC_SHA256="767c99155f74d5620cfd59d0224df2f82dec7ce58be24d702081dca9793408a9"
ROOTFS_CROSS_COMPILE="arm-buildroot-linux-musleabihf-"

# Host GNU Make >= 4.0 (the Linux kernel Makefile hard-requires it). toolchain.sh
# builds this into HOSTMAKE_DIR ONLY when the host's own make is too old.
# Source: kernel.org GNU mirror (ftp.gnu.org has no egress here).
MAKE_VERSION="4.4.1"
MAKE_TARBALL="make-${MAKE_VERSION}.tar.gz"
MAKE_URL="https://mirrors.kernel.org/gnu/make/${MAKE_TARBALL}"
MAKE_SHA256="dd16fb1d67bfab79a72f5e8390735c49e3e8e70b4945a15ab1f81ddb78658fb3"

# =============================================================================
# PER-PRODUCT DATA  (the "what" — supplied by the product being built)
# =============================================================================
# OSS component version pins (kernel/uboot/busybox tags + urls + shas). In
# Buildroot these are the board's defconfig BR2_*_VERSION lines: per-product data,
# read by the generic recipe. Kept in versions.env (bash) rather than config.mk
# because config.mk uses Make `?=` syntax bash can't source.
[ -f "${PRODUCT_DIR}/versions.env" ] && source "${PRODUCT_DIR}/versions.env"
# Board build facts (defconfig names, board DT, console) + memory/storage layout +
# the make-readable provider targets. board.mk is KEY=value so it is sourceable
# here AND includable by forge/providers.mk.
[ -f "${BOARD_DIR}/board.env" ]  && source "${BOARD_DIR}/board.env"
[ -f "${BOARD_DIR}/layout.env" ] && source "${BOARD_DIR}/layout.env"
[ -f "${BOARD_DIR}/board.mk" ]   && source "${BOARD_DIR}/board.mk"
KERNEL_TARGET="${KERNEL_TARGET:-${BOARD_NAME%%-*}}"
ROOTFS_TARGET="${ROOTFS_TARGET:-${KERNEL_TARGET}}"

# =============================================================================
# MECHANISM  (generic helpers + PATH setup)
# =============================================================================

# git_clone_pinned <dest> <tag> <primary_url> [mirror_url]
# Shallow-clone a specific tag, retrying the primary a few times (git hosts have
# transient 502s), then falling back to the mirror. Reproducible: the TAG pins the
# content; primary vs mirror is just availability.
git_clone_pinned() {
  local dest="$1" tag="$2" primary="$3" mirror="${4:-}"
  local url attempt
  for url in "${primary}" "${mirror}"; do
    [ -n "${url}" ] || continue
    for attempt in 1 2 3; do
      echo "  git clone ${tag} from ${url} (attempt ${attempt})" >&2
      if git clone --depth 1 --branch "${tag}" "${url}" "${dest}" 2>&1; then
        return 0
      fi
      rm -rf "${dest}"           # clean partial checkout before retry
      sleep 3
    done
    [ -n "${mirror}" ] && echo "  primary failed; trying mirror" >&2
  done
  return 1
}

# _prepend_path <dir> — put <dir> at the front of PATH if present and not already there.
_prepend_path() {
  [ -d "$1" ] || return 0
  case ":${PATH}:" in
    *":$1:"*) : ;;
    *) PATH="$1:${PATH}"; export PATH ;;
  esac
}

# Locally-built GNU Make >=4 (only exists if the host's was too old); kernel needs it.
_prepend_path "${HOSTMAKE_DIR}/bin"
# The cross toolchains (toolchain.sh extracts them here; later steps get them free).
# Different triples (gnueabihf vs musleabihf) so they can't collide.
_prepend_path "${TOOLCHAIN_DIR}/bin"
_prepend_path "${ROOTFS_TOOLCHAIN_DIR}/bin"
# Host python venv for binman/pylibfdt. The Bootlin toolchain ships its OWN python3
# (lacking setuptools/pyelftools/pyyaml), so when the venv exists (uboot.sh creates
# it) prepend it AHEAD of everything — including the toolchain bin — and point
# U-Boot's PYTHON at it explicitly. Keeps the host's system python untouched.
if [ -x "${PYENV_DIR}/bin/python3" ]; then
  _prepend_path "${PYENV_DIR}/bin"
  PYTHON="${PYENV_DIR}/bin/python3"
  export PYTHON
fi
