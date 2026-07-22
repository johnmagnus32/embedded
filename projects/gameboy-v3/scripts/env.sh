#!/usr/bin/env bash
# env.sh — single source of truth for the gameboy-v3 build.
#
# Every build step sources this file. It defines the pinned component versions,
# the directory layout, and the cross-compile environment. Nothing here runs a
# build; it only sets variables and (when sourced) puts the toolchain on PATH.
#
# Reproducibility rule: versions and checksums are PINNED here. Do not use
# "latest" anywhere else — bump the pin in this file, deliberately, instead.
#
# Usage:
#   source scripts/env.sh      # from anywhere; resolves its own location
# or a step script does it for you.

# --- resolve paths (works regardless of caller's CWD) ------------------------
# BASH_SOURCE[0] is this file even when sourced. Step scripts source us via an
# ABSOLUTE path ("${HERE}/env.sh"), which resolves reliably. Sourcing via a bare
# relative path from an odd interactive shell can misresolve, so we GUARD below.
_ENV_SH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Guard: this file must actually live in the resolved dir. If not, BASH_SOURCE
# was relative/unreliable — fail loudly rather than build in the wrong tree.
if [ ! -f "${_ENV_SH}/env.sh" ]; then
  echo "env.sh: could not resolve my own location (got '${_ENV_SH}')." >&2
  echo "        Source me by absolute path, e.g. source \"\$(dirname \"\$0\")/env.sh\"." >&2
  return 1 2>/dev/null || exit 1
fi
PROJECT_DIR="$(cd "${_ENV_SH}/.." && pwd)"        # projects/gameboy-v3
SCRIPTS_DIR="${PROJECT_DIR}/scripts"
# Second guard: the project dir must be the gameboy-v3 dir (defensive).
if [ "$(basename "${PROJECT_DIR}")" != "gameboy-v3" ]; then
  echo "env.sh: PROJECT_DIR='${PROJECT_DIR}' is not .../gameboy-v3 — refusing." >&2
  return 1 2>/dev/null || exit 1
fi

# All build inputs/outputs live under build/ (git-ignored). One tree, easy to
# nuke and rebuild — that is the reproducibility test.
BUILD_DIR="${PROJECT_DIR}/build"
DOWNLOAD_DIR="${BUILD_DIR}/downloads"             # fetched tarballs (cached)
TOOLCHAIN_DIR="${BUILD_DIR}/toolchain"            # extracted cross-toolchain
UBOOT_SRC_DIR="${BUILD_DIR}/u-boot"               # U-Boot git checkout
OUTPUT_DIR="${BUILD_DIR}/output"                  # final flashable artifacts
PYENV_DIR="${BUILD_DIR}/pyenv"                     # host python venv (binman deps)
HOSTMAKE_DIR="${BUILD_DIR}/hostmake"               # locally-built GNU Make >=4 (kernel needs it)
BUSYBOX_SRC_DIR="${BUILD_DIR}/busybox"            # BusyBox source
ROOTFS_DIR="${BUILD_DIR}/rootfs"                  # assembled initramfs root

# =============================================================================
# PINNED VERSIONS  (the reproducibility contract)
# =============================================================================

# --- Step 0: cross toolchain -------------------------------------------------
# Bootlin prebuilt, armv7-eabihf (hard-float), glibc. The T113-S3 is 32-bit
# ARMv7-A (Cortex-A7), so this is an arm .*-gnueabihf toolchain, NOT arm-none-eabi
# (that would be bare-metal). "stable" channel, pinned to a dated release.
#
# VERSION CHOICE — constrained by the HOST, not the target:
#   * This build host is glibc 2.26. Bootlin's NEWER toolchains (e.g. 2024.x /
#     2025.08) ship a binutils `ld` linked against GLIBC_2.27, so its `ld`
#     literally won't run here — the U-Boot build dies at the first LINK step
#     (a compile-only check passes, which is why this only shows up mid-build).
#   * U-Boot v2026.04 requires host/target GCC >= 10 (arch/arm/config.mk
#     `checkgcc10`).
#   => 2021.11 is the sweet spot: GCC 10.3.0 (satisfies checkgcc10) AND its `ld`
#      only needs GLIBC_2.14 (runs on this host). Verified by running its `ld`
#      and doing a full compile+link here.
#   If you build on a newer host (glibc >= 2.27), you can bump to 2025.08-1
#      (sha 97d6fbaf…f9b4, .tar.xz) for a newer GCC.
#
# To bump: pick a dir from
#   https://toolchains.bootlin.com/downloads/releases/toolchains/armv7-eabihf/tarballs/
# then update the version, the SHA256 (from the matching .sha256 file), AND the
# tarball extension (older releases are .tar.bz2, newer are .tar.xz).
TOOLCHAIN_ARCH="armv7-eabihf"
TOOLCHAIN_LIBC="glibc"
TOOLCHAIN_CHANNEL="stable"
TOOLCHAIN_VERSION="2021.11-1"
TOOLCHAIN_EXT="tar.bz2"
TOOLCHAIN_TARBALL="${TOOLCHAIN_ARCH}--${TOOLCHAIN_LIBC}--${TOOLCHAIN_CHANNEL}-${TOOLCHAIN_VERSION}.${TOOLCHAIN_EXT}"
TOOLCHAIN_URL="https://toolchains.bootlin.com/downloads/releases/toolchains/${TOOLCHAIN_ARCH}/tarballs/${TOOLCHAIN_TARBALL}"
# SHA256 published by Bootlin alongside the tarball (…/tarballs/<name>.sha256).
TOOLCHAIN_SHA256="6d10f356811429f1bddc23a174932c35127ab6c6f3b738b768f0c29c3bf92f10"

# The cross-compiler prefix. Bootlin toolchains are built WITH Buildroot, so the
# triple is arm-buildroot-linux-gnueabihf- (not the generic arm-linux-gnueabihf-
# you'd get from a distro/Arm package). Verified from the tarball's bin/ dir.
# It is still a standard armv7-eabihf hard-float glibc toolchain (GCC 14.3.0);
# only the name prefix differs. U-Boot/kernel just need CROSS_COMPILE to match
# the actual binaries on PATH.
CROSS_COMPILE="arm-buildroot-linux-gnueabihf-"
ARCH="arm"

export CROSS_COMPILE ARCH

# --- Step 3 uses a SECOND, musl toolchain (for the rootfs only) --------------
# U-Boot and the kernel don't link a target libc, so they stay on the glibc
# toolchain above. The ROOTFS (BusyBox) does link libc, and building it against
# musl instead of glibc makes the static binary ~34% smaller AND shrinks the
# syscall surface (musl's startup is far lighter than glibc's) — which matters
# for a future hand-written kernel that must present the Linux syscall ABI.
# Same 2021.11 vintage as the glibc toolchain, so its `ld` runs on this host.
ROOTFS_TC_VERSION="2021.11-1"
ROOTFS_TC_EXT="tar.bz2"
ROOTFS_TC_TARBALL="${TOOLCHAIN_ARCH}--musl--${TOOLCHAIN_CHANNEL}-${ROOTFS_TC_VERSION}.${ROOTFS_TC_EXT}"
ROOTFS_TC_URL="https://toolchains.bootlin.com/downloads/releases/toolchains/${TOOLCHAIN_ARCH}/tarballs/${ROOTFS_TC_TARBALL}"
ROOTFS_TC_SHA256="767c99155f74d5620cfd59d0224df2f82dec7ce58be24d702081dca9793408a9"
ROOTFS_TOOLCHAIN_DIR="${BUILD_DIR}/toolchain-musl"
ROOTFS_CROSS_COMPILE="arm-buildroot-linux-musleabihf-"

# --- Step 1: U-Boot (SPL + U-Boot proper) ------------------------------------
# Mainline U-Boot. T113-S3 is grouped under CONFIG_MACH_SUN8I_R528 and supported
# since v2024.01; we pin a recent conservative stable tag. Bump deliberately.
# Canonical repo first, GitHub mirror as fallback (denx has transient 502s/
# outages; the mirror carries identical tags). Scripts try each in order.
UBOOT_GIT_URL="https://source.denx.de/u-boot/u-boot.git"
UBOOT_GIT_URL_MIRROR="https://github.com/u-boot/u-boot.git"
UBOOT_TAG="v2026.04"
UBOOT_DEFCONFIG="mangopi_mq_r_defconfig"
# The board device tree U-Boot builds as its control DTB AND ships for the kernel.
UBOOT_BOARD_DT="sun8i-t113s-mangopi-mq-r-t113"
# Final combined SPL+U-Boot image (binman output), copied into OUTPUT_DIR.
UBOOT_IMAGE="u-boot-sunxi-with-spl.bin"

# --- CONSOLE ON UART0 / PE2-PE3 (our t113-breakout wiring) -------------------
# The stock mangopi_mq_r_defconfig puts the console on UART3/PB6-PB7 (CONS_INDEX=4).
# Our board wires the console to UART0/PE2-PE3. Moving it requires TWO changes
# (verified against U-Boot source, arch/arm/mach-sunxi/board.c + serial-uclass.c):
#   1. CONS_INDEX=1  -> fixes the SPL console pinmux to PE2/PE3 (mux func 6).
#   2. control-DTB stdout-path/alias -> U-Boot *proper* uses DM serial and reads
#      stdout-path from the DTB; without this it would switch back to UART3.
# CONS_INDEX alone is a "split-brain console" trap. 01-uboot.sh applies both.
UBOOT_CONS_INDEX="1"

# --- Step 2: Linux kernel ----------------------------------------------------
# Mainline Linux, LTS pin. T113 board DTS landed v6.4 and settled into the
# allwinner/ subdir at v6.5; 6.12 is an LTS with a long support window.
KERNEL_GIT_URL="https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git"
KERNEL_GIT_URL_MIRROR="https://github.com/gregkh/linux.git"   # stable-tree mirror (same tags)
KERNEL_TAG="v6.12.95"
KERNEL_DEFCONFIG="sunxi_defconfig"        # 32-bit ARM; enables 8250_DW UART, MMC_SUNXI, D1/T113 CCU+pinctrl
KERNEL_IMAGE_TARGET="zImage"              # ARMv7 compressed kernel
# Same board DTB name as U-Boot; the kernel builds it under allwinner/.
KERNEL_DTB="${UBOOT_BOARD_DT}.dtb"
KERNEL_DTB_SUBDIR="allwinner"             # arch/arm/boot/dts/allwinner/
# Console: the kernel DTB gets the SAME UART0 overlay as U-Boot (scripts/
# uart0-console.dtsi). With serial0=&uart0, Linux names uart0 "ttyS0", so the
# kernel command line console must be ttyS0 (see KERNEL_CONSOLE).
KERNEL_CONSOLE="ttyS0,115200"

# --- Step 3: BusyBox rootfs (static, initramfs) ------------------------------
# Static BusyBox packed into a cpio.gz initramfs — the simplest thing that boots
# to a shell (no root partition / root= needed). 1.36.1 is the last release
# upstream labels 'stable'. Fetched SHA256 from busybox.net's .sha256 sidecar.
BUSYBOX_VERSION="1.36.1"
BUSYBOX_TARBALL="busybox-${BUSYBOX_VERSION}.tar.bz2"
BUSYBOX_URL="https://busybox.net/downloads/${BUSYBOX_TARBALL}"
BUSYBOX_SHA256="b8cc24c9574d809e7279c3be349795c5d5ceb6fdf19ca709f80cde50e47de314"
# Final initramfs image (raw cpio.gz — U-Boot boots it via bootz with :size).
INITRAMFS_IMAGE="initramfs.cpio.gz"

# --- Step 4: SD image assembly -----------------------------------------------
# Full-disk image: U-Boot at the 8 KiB offset, then one FAT boot partition at
# 1 MiB holding zImage + DTB + initramfs + boot.scr. U-Boot's distro_bootcmd
# auto-runs /boot.scr from the partition. dd the whole .img to a microSD.
SDIMAGE="gameboy-v3-sd.img"
SDIMAGE_SIZE_MB="64"          # plenty for ~7 MB of payload; keeps the .img small
SDIMAGE_PART_START_MB="1"     # partition 1 starts at 1 MiB (never clobbers U-Boot)
SDIMAGE_UBOOT_SEEK_KB="8"     # BROM reads SPL at the 8 KiB offset (sector 16)

# --- host GNU Make (kernel build needs >= 4.0) -------------------------------
# The Linux kernel Makefile hard-requires GNU Make >= 4.0. 00-toolchain.sh's
# setup_host_make() uses the host's own make when it's new enough, and only
# builds this pinned one into HOSTMAKE_DIR when the host is too old (e.g. this
# host ships 3.82). Pinned + checksummed like everything else.
# Source: kernel.org GNU mirror (ftp.gnu.org has no egress here).
MAKE_VERSION="4.4.1"
MAKE_TARBALL="make-${MAKE_VERSION}.tar.gz"
MAKE_URL="https://mirrors.kernel.org/gnu/make/${MAKE_TARBALL}"
MAKE_SHA256="dd16fb1d67bfab79a72f5e8390735c49e3e8e70b4945a15ab1f81ddb78658fb3"

# =============================================================================
# HELPERS
# =============================================================================

# git_clone_pinned <dest> <tag> <primary_url> [mirror_url]
# Shallow-clone a specific tag, retrying the primary URL a few times (git hosts
# have transient 502s/timeouts), then falling back to the mirror. Reproducible:
# the TAG is what pins the content; primary vs mirror is just availability.
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

# --- locally-built GNU Make on PATH (if present) -----------------------------
# 00-toolchain.sh installs make >=4 here (only if the host's is too old); prepend
# so 'make' resolves to it over the host's. Kernel needs it; harmless for U-Boot.
if [ -x "${HOSTMAKE_DIR}/bin/make" ]; then
  case ":${PATH}:" in
    *":${HOSTMAKE_DIR}/bin:"*) : ;;
    *) PATH="${HOSTMAKE_DIR}/bin:${PATH}" ;;
  esac
  export PATH
fi

# --- put the toolchain on PATH if it is already extracted --------------------
# (00-toolchain.sh extracts it here; later steps source env.sh and get it free.)
if [ -d "${TOOLCHAIN_DIR}/bin" ]; then
  case ":${PATH}:" in
    *":${TOOLCHAIN_DIR}/bin:"*) : ;;                 # already present
    *) PATH="${TOOLCHAIN_DIR}/bin:${PATH}" ;;
  esac
  export PATH
fi

# --- musl (rootfs) toolchain on PATH if extracted ----------------------------
# Different triple (arm-buildroot-linux-musleabihf-) so it can't collide with
# the glibc one above. Step 3 selects it via ${ROOTFS_CROSS_COMPILE}.
if [ -d "${ROOTFS_TOOLCHAIN_DIR}/bin" ]; then
  case ":${PATH}:" in
    *":${ROOTFS_TOOLCHAIN_DIR}/bin:"*) : ;;
    *) PATH="${ROOTFS_TOOLCHAIN_DIR}/bin:${PATH}" ;;
  esac
  export PATH
fi

# --- host python venv for binman/pylibfdt ------------------------------------
# The Bootlin toolchain ships its OWN python3 in ${TOOLCHAIN_DIR}/bin, and it
# lacks setuptools/pyelftools/pyyaml that U-Boot's binman + pylibfdt need. If we
# left the toolchain python first on PATH, the build would pick it and fail. So
# when the venv exists (01-uboot.sh creates it), prepend it AHEAD of everything
# — including the toolchain bin — so 'python'/'python3' resolve to the venv, and
# point U-Boot's PYTHON at it explicitly. This keeps the host's system python
# untouched and the whole thing reproducible under build/.
if [ -x "${PYENV_DIR}/bin/python3" ]; then
  case ":${PATH}:" in
    *":${PYENV_DIR}/bin:"*) : ;;
    *) PATH="${PYENV_DIR}/bin:${PATH}" ;;
  esac
  export PATH
  PYTHON="${PYENV_DIR}/bin/python3"
  export PYTHON
fi
