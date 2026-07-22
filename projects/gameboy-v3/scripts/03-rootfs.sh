#!/usr/bin/env bash
# 03-rootfs.sh — Step 3: build a static BusyBox initramfs (boot-to-shell rootfs).
#
# What it does:
#   1. Fetch + verify (SHA256) BusyBox, extract into build/busybox.
#   2. Configure a STATIC build (CONFIG_STATIC=y) with tc disabled (see below),
#      cross-compile it, and `make install` into a rootfs skeleton.
#   3. Add /init (PID 1 script) + the proc/sys/dev mountpoints.
#   4. Pack into a root-owned cpio.gz initramfs via the kernel's gen_init_cpio
#      (creates /dev/console without needing root), copy to build/output/.
#
# The result is the simplest possible rootfs: a single static busybox multi-call
# binary + symlinks, that boots straight to a shell over UART0. No root
# partition, no root= — U-Boot loads kernel + DTB + this initramfs together.
#
#   ./scripts/03-rootfs.sh            # build (idempotent)
#   ./scripts/03-rootfs.sh --clean    # wipe busybox src + rootfs first
#
# Prereqs: ./scripts/00-toolchain.sh (cross gcc) and ./scripts/02-kernel.sh
# (we reuse the kernel's gen_init_cpio host tool for packaging).

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "${HERE}/env.sh"

log()  { printf '\033[1;34m[03-rootfs]\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31m[03-rootfs] ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

CLEAN=0
[ "${1:-}" = "--clean" ] && CLEAN=1

# --- preflight ---------------------------------------------------------------
# The ROOTFS is built with the MUSL toolchain (not the glibc one U-Boot/kernel
# use): musl-static BusyBox is ~34% smaller and has a lighter syscall surface.
command -v "${ROOTFS_CROSS_COMPILE}gcc" >/dev/null 2>&1 \
  || die "musl cross compiler '${ROOTFS_CROSS_COMPILE}gcc' not on PATH — run ./scripts/00-toolchain.sh first"
for t in curl sha256sum tar bzip2 make; do
  command -v "$t" >/dev/null 2>&1 || die "host dep '$t' missing"
done
# We package with the kernel's gen_init_cpio (root-owned nodes without root).
GEN_INIT_CPIO="${BUILD_DIR}/linux/usr/gen_init_cpio"
if [ ! -x "${GEN_INIT_CPIO}" ]; then
  # Build just that host tool if the kernel tree is present but the tool isn't.
  if [ -f "${BUILD_DIR}/linux/usr/gen_init_cpio.c" ]; then
    log "building gen_init_cpio host tool"
    cc -O2 -o "${GEN_INIT_CPIO}" "${BUILD_DIR}/linux/usr/gen_init_cpio.c" || die "failed to build gen_init_cpio"
  else
    die "gen_init_cpio not found — run ./scripts/02-kernel.sh first (we reuse its host tool)"
  fi
fi

if [ "${CLEAN}" -eq 1 ]; then
  log "--clean: removing ${BUSYBOX_SRC_DIR} and ${ROOTFS_DIR}"
  rm -rf "${BUSYBOX_SRC_DIR}" "${ROOTFS_DIR}"
fi

# --- 1. fetch + verify + extract BusyBox -------------------------------------
mkdir -p "${DOWNLOAD_DIR}"
TB="${DOWNLOAD_DIR}/${BUSYBOX_TARBALL}"
verify_sha() { echo "${BUSYBOX_SHA256}  ${TB}" | sha256sum --check --status; }

if [ -f "${TB}" ] && verify_sha; then
  log "cached ${BUSYBOX_TARBALL} checksum OK"
else
  log "downloading ${BUSYBOX_TARBALL}"
  curl -fL --retry 3 --retry-delay 2 --connect-timeout 20 -o "${TB}.part" "${BUSYBOX_URL}" \
    || die "download failed"
  mv -f "${TB}.part" "${TB}"
  verify_sha || die "SHA256 mismatch (expected ${BUSYBOX_SHA256}, got $(sha256sum "${TB}" | cut -d' ' -f1))"
  log "SHA256 OK"
fi

if [ ! -f "${BUSYBOX_SRC_DIR}/Makefile" ]; then
  log "extracting into ${BUSYBOX_SRC_DIR}"
  rm -rf "${BUSYBOX_SRC_DIR}" "${BUSYBOX_SRC_DIR}.tmp"
  mkdir -p "${BUSYBOX_SRC_DIR}.tmp"
  tar -xf "${TB}" -C "${BUSYBOX_SRC_DIR}.tmp" --strip-components=1
  mv "${BUSYBOX_SRC_DIR}.tmp" "${BUSYBOX_SRC_DIR}"
fi

cd "${BUSYBOX_SRC_DIR}"

# --- 2. configure static + build --------------------------------------------
log "make defconfig"
make defconfig >/dev/null

log "configuring static build (CONFIG_STATIC=y, CONFIG_TC=n)"
# CONFIG_STATIC=y  → fully static binary (no shared libs needed on target).
# CONFIG_TC=n      → the traffic-control applet (tc.c) uses TCA_CBQ_MAX, which
#                    the kernel removed in v6.8; tc.c is UNFIXED upstream and
#                    breaks against modern kernel headers. We don't need tc for
#                    a minimal rootfs, so disable it (also clears TC_INGRESS).
# (CONFIG_WERROR is already n in defconfig, so warnings won't fail the build —
#  including the harmless static-glibc getpwnam/gethostbyname NSS warnings.)
sed -i \
  -e 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' \
  -e 's/^CONFIG_TC=y/CONFIG_TC=n/' \
  .config
# Pin the cross-compiler prefix IN THE CONFIG. BusyBox's CONFIG_CROSS_COMPILER_PREFIX
# takes precedence over the CROSS_COMPILE make/env var, so setting it on the make
# line alone is silently ignored (defconfig ships it ="", which then picks up the
# host/PATH gcc). Writing it here is what actually selects the musl toolchain.
sed -i "s|^CONFIG_CROSS_COMPILER_PREFIX=.*|CONFIG_CROSS_COMPILER_PREFIX=\"${ROOTFS_CROSS_COMPILE}\"|" .config
# normalize any dependent symbols non-interactively (accept all defaults)
make oldconfig </dev/null >/dev/null
grep -q '^CONFIG_STATIC=y' .config || die "CONFIG_STATIC did not take"
grep -q "^CONFIG_CROSS_COMPILER_PREFIX=\"${ROOTFS_CROSS_COMPILE}\"" .config \
  || die "CONFIG_CROSS_COMPILER_PREFIX did not take (musl toolchain not selected)"

log "cross-compiling BusyBox with musl (-j$(nproc)) ..."
# `make clean` first so a re-run always recompiles every object with the
# selected toolchain (without it, objects from a prior glibc build can survive
# and the link silently mixes toolchains / keeps the old libc).
make clean >/dev/null
# CROSS_COMPILE on the command line overrides the glibc value env.sh exported
# (for U-Boot/kernel); combined with CONFIG_CROSS_COMPILER_PREFIX this selects
# the musl toolchain. musl-static → ~34% smaller than glibc-static.
make -j"$(nproc)" CROSS_COMPILE="${ROOTFS_CROSS_COMPILE}" >/dev/null

# confirm the binary is actually static + ARM
BB="${BUSYBOX_SRC_DIR}/busybox"
[ -x "${BB}" ] || die "busybox binary not produced"
BB_FILE="$(file "${BB}")"
case "${BB_FILE}" in
  *statically*linked*ARM*|*ARM*statically*linked*|*ARM*"statically linked"*) : ;;
  # musl static binaries report as "statically linked" but sometimes without the
  # exact word order; accept any static ARM ELF.
  *ARM*) case "${BB_FILE}" in *dynamically*) die "busybox is dynamically linked: ${BB_FILE#*: }" ;; esac ;;
  *) die "busybox is not an ARM binary: ${BB_FILE#*: }" ;;
esac
log "built: ${BB_FILE#*: }"

# --- 3. assemble the rootfs skeleton -----------------------------------------
log "installing busybox + applet symlinks into ${ROOTFS_DIR}"
rm -rf "${ROOTFS_DIR}"
mkdir -p "${ROOTFS_DIR}"
# MUST pass CROSS_COMPILE here too: `make install` re-checks build deps, and
# without it the exported (glibc) CROSS_COMPILE would trigger a silent REBUILD
# of busybox with glibc, undoing the musl build above. (This bit us once.)
make CONFIG_PREFIX="${ROOTFS_DIR}" CROSS_COMPILE="${ROOTFS_CROSS_COMPILE}" install >/dev/null

# /init (PID 1) + mountpoints. devtmpfs will populate /dev at runtime, but the
# kernel needs /dev/console to exist to give PID 1 its stdio — we add that node
# in the cpio device table below (can't mknod as an unprivileged user).
install -m 0755 "${SCRIPTS_DIR}/init" "${ROOTFS_DIR}/init"
mkdir -p "${ROOTFS_DIR}/proc" "${ROOTFS_DIR}/sys" "${ROOTFS_DIR}/dev"

# --- 4. pack a root-owned cpio.gz via gen_init_cpio --------------------------
# gen_init_cpio reads a device-table listing; it emits a cpio with the ownership
# we declare (uid/gid 0) and can create device nodes without any privilege —
# cleaner and more reproducible than mknod-as-root or fakeroot.
log "generating initramfs listing"
LISTING="${BUILD_DIR}/initramfs.list"
{
  echo "# path                     mode uid gid   [maj min]"
  echo "dir /proc 0755 0 0"
  echo "dir /sys  0755 0 0"
  echo "dir /dev  0755 0 0"
  # essential console node so the kernel can wire PID 1's stdin/out/err
  echo "nod /dev/console 0600 0 0 c 5 1"
  echo "nod /dev/null    0666 0 0 c 1 3"
  # walk the installed rootfs and emit dir/file/slink entries (uid/gid 0),
  # skipping the dirs we already declared above.
  cd "${ROOTFS_DIR}"
  find . -mindepth 1 \( -path './proc' -o -path './sys' -o -path './dev' \) -prune -o -print \
  | while read -r p; do
      rel="${p#.}"
      # NB: stat -c '%a' already yields OCTAL digits (e.g. 755); pass them with
      # %s. Do NOT use printf %o here — that would treat "755" as decimal and
      # re-encode it (755 dec -> 1363 oct), corrupting every mode.
      if [ -L "$p" ]; then
        printf 'slink %s %s 0777 0 0\n' "$rel" "$(readlink "$p")"
      elif [ -d "$p" ]; then
        printf 'dir %s 0%s 0 0\n' "$rel" "$(stat -c '%a' "$p")"
      elif [ -f "$p" ]; then
        printf 'file %s %s 0%s 0 0\n' "$rel" "$p" "$(stat -c '%a' "$p")"
      fi
    done
} > "${LISTING}"

log "packing ${INITRAMFS_IMAGE}"
mkdir -p "${OUTPUT_DIR}"
"${GEN_INIT_CPIO}" "${LISTING}" | gzip -9 > "${OUTPUT_DIR}/${INITRAMFS_IMAGE}"

# --- report ------------------------------------------------------------------
SIZE="$(du -h "${OUTPUT_DIR}/${INITRAMFS_IMAGE}" | cut -f1)"
NAPPLETS="$(find "${ROOTFS_DIR}" -type l | wc -l)"
cat <<EOF

$(printf '\033[1;32m[03-rootfs] DONE\033[0m')
  BusyBox    : ${BUSYBOX_VERSION} (static, ARM)
  Applets    : ${NAPPLETS} symlinks → /bin/busybox
  Init       : /init (PID 1) mounts proc/sys/dev, execs /bin/sh on the console
  Artifact   : ${OUTPUT_DIR}/${INITRAMFS_IMAGE}  (${SIZE})

All four pieces are now in build/output/:
  u-boot-sunxi-with-spl.bin   (Step 1)
  zImage + *.dtb              (Step 2)
  ${INITRAMFS_IMAGE}         (Step 3)

Next: Step 4 — assemble + flash the SD (write U-Boot at 8K, drop
kernel/DTB/initramfs on a partition, boot script sets:
  bootz \${kernel_addr_r} \${ramdisk_addr_r}:\${filesize} \${fdt_addr_r}
  bootargs = console=${KERNEL_CONSOLE}).
EOF
