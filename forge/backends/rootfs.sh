#!/usr/bin/env bash
# rootfs.sh — build a BusyBox initramfs (boot-to-shell rootfs).
#
# Linkage is selectable (LINKAGE=static|dynamic, default static):
#   static  — CONFIG_STATIC=y: one self-contained busybox, no libs on the target.
#             The simplest thing that boots; what the mainline-Linux path uses.
#   dynamic — CONFIG_STATIC=n: busybox links against musl's shared libc and needs
#             the dynamic loader at runtime. We ship /lib/libc.so plus the
#             /lib/ld-musl-armhf.so.1 symlink musl's PT_INTERP names (for musl the
#             loader AND libc are the SAME file). This is the artifact used to
#             bring up + verify the kernel's dynamic-linking support (ET_DYN load
#             bias, PT_INTERP, full auxv, file-backed mmap2).
#
# What it does:
#   1. Fetch + verify (SHA256) BusyBox, extract into build/busybox.
#   2. Configure the selected linkage, cross-compile, `make install` into a
#      rootfs skeleton; for dynamic, also stage the musl loader into /lib.
#   3. Add /init (PID 1 script) + the proc/sys/dev mountpoints.
#   4. Pack into a root-owned cpio.gz initramfs via the kernel's gen_init_cpio
#      (creates /dev/console without needing root), copy to build/output/.
#
# The output is named per-linkage so both can coexist:
#   static  -> build/output/initramfs.cpio.gz          (the default name)
#   dynamic -> build/output/initramfs-dynamic.cpio.gz
#
#   make rootfs KERNEL=mainline LIBC=musl COREUTILS=busybox                     # static (default)
#   LINKAGE=dynamic make rootfs KERNEL=mainline LIBC=musl COREUTILS=busybox     # dynamic (ships musl ld.so)
#   make rootfs KERNEL=mainline LIBC=musl COREUTILS=busybox --clean             # wipe busybox src + rootfs first
#
# Prereqs: make toolchain (cross gcc) and make kernel KERNEL=mainline
# (we reuse the kernel's gen_init_cpio host tool for packaging).

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "${HERE}/lib.sh"

log()  { printf '\033[1;34m[03-rootfs]\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31m[03-rootfs] ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

# --- linkage selection -------------------------------------------------------
# LINKAGE=static (default) | dynamic. Chooses CONFIG_STATIC, the runtime libs we
# stage, and the output filename.
LINKAGE="${LINKAGE:-static}"
case "${LINKAGE}" in
  static|dynamic) : ;;
  *) die "LINKAGE must be 'static' or 'dynamic' (got '${LINKAGE}')" ;;
esac
if [ "${LINKAGE}" = "dynamic" ]; then
  OUT_IMAGE="initramfs-dynamic.cpio.gz"
else
  OUT_IMAGE="${INITRAMFS_IMAGE}"     # static keeps the canonical name
fi

CLEAN=0
[ "${1:-}" = "--clean" ] && CLEAN=1

# --- preflight ---------------------------------------------------------------
# The ROOTFS is built with the MUSL toolchain (not the glibc one U-Boot/kernel
# use): musl-static BusyBox is ~34% smaller and has a lighter syscall surface.
command -v "${ROOTFS_CROSS_COMPILE}gcc" >/dev/null 2>&1 \
  || die "musl cross compiler '${ROOTFS_CROSS_COMPILE}gcc' not on PATH — run make toolchain first"
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
    die "gen_init_cpio not found — run make kernel KERNEL=mainline first (we reuse its host tool)"
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

log "configuring ${LINKAGE} build (CONFIG_STATIC=$([ "${LINKAGE}" = static ] && echo y || echo n), CONFIG_TC=n)"
# CONFIG_STATIC  → y: fully static binary (no shared libs on target).
#                  n: dynamic — links musl's shared libc, needs the loader at
#                     runtime (we stage it below).
# CONFIG_TC=n      → the traffic-control applet (tc.c) uses TCA_CBQ_MAX, which
#                    the kernel removed in v6.8; tc.c is UNFIXED upstream and
#                    breaks against modern kernel headers. We don't need tc for
#                    a minimal rootfs, so disable it (also clears TC_INGRESS).
# (CONFIG_WERROR is already n in defconfig, so warnings won't fail the build —
#  including the harmless static-glibc getpwnam/gethostbyname NSS warnings.)
if [ "${LINKAGE}" = "static" ]; then
  sed -i -e 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
else
  # ensure STATIC is OFF whether the line is currently set or unset
  sed -i -e 's/^CONFIG_STATIC=y/# CONFIG_STATIC is not set/' .config
fi
sed -i -e 's/^CONFIG_TC=y/CONFIG_TC=n/' .config
# Pin the cross-compiler prefix IN THE CONFIG. BusyBox's CONFIG_CROSS_COMPILER_PREFIX
# takes precedence over the CROSS_COMPILE make/env var, so setting it on the make
# line alone is silently ignored (defconfig ships it ="", which then picks up the
# host/PATH gcc). Writing it here is what actually selects the musl toolchain.
sed -i "s|^CONFIG_CROSS_COMPILER_PREFIX=.*|CONFIG_CROSS_COMPILER_PREFIX=\"${ROOTFS_CROSS_COMPILE}\"|" .config
# normalize any dependent symbols non-interactively (accept all defaults)
make oldconfig </dev/null >/dev/null
if [ "${LINKAGE}" = "static" ]; then
  grep -q '^CONFIG_STATIC=y' .config || die "CONFIG_STATIC=y did not take"
else
  grep -q '^CONFIG_STATIC=y' .config && die "CONFIG_STATIC still on for a dynamic build"
fi
grep -q "^CONFIG_CROSS_COMPILER_PREFIX=\"${ROOTFS_CROSS_COMPILE}\"" .config \
  || die "CONFIG_CROSS_COMPILER_PREFIX did not take (musl toolchain not selected)"

log "cross-compiling BusyBox with musl (-j$(nproc)) ..."
# `make clean` first so a re-run always recompiles every object with the
# selected toolchain (without it, objects from a prior glibc build can survive
# and the link silently mixes toolchains / keeps the old libc).
make clean >/dev/null
# CROSS_COMPILE on the command line overrides the glibc value lib.sh exported
# (for U-Boot/kernel); combined with CONFIG_CROSS_COMPILER_PREFIX this selects
# the musl toolchain. musl-static → ~34% smaller than glibc-static.
make -j"$(nproc)" CROSS_COMPILE="${ROOTFS_CROSS_COMPILE}" >/dev/null

# confirm the binary is ARM and matches the requested linkage
BB="${BUSYBOX_SRC_DIR}/busybox"
[ -x "${BB}" ] || die "busybox binary not produced"
BB_FILE="$(file "${BB}")"
case "${BB_FILE}" in
  *ARM*) : ;;
  *) die "busybox is not an ARM binary: ${BB_FILE#*: }" ;;
esac
if [ "${LINKAGE}" = "static" ]; then
  case "${BB_FILE}" in
    *dynamically*) die "expected STATIC busybox but it is dynamic: ${BB_FILE#*: }" ;;
  esac
else
  case "${BB_FILE}" in
    *dynamically*) : ;;
    *) die "expected DYNAMIC busybox but it is not: ${BB_FILE#*: }" ;;
  esac
fi
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
install -m 0755 "${OVERLAY_DIR}/init.busybox" "${ROOTFS_DIR}/init"
mkdir -p "${ROOTFS_DIR}/proc" "${ROOTFS_DIR}/sys" "${ROOTFS_DIR}/dev"

# --- dynamic: stage the musl loader (== libc) into /lib ----------------------
# A dynamic busybox has PT_INTERP = /lib/ld-musl-armhf.so.1 and NEEDED libc.so.
# For musl the loader and libc are the SAME file: ld-musl-armhf.so.1 -> libc.so.
# So we copy the real libc.so and recreate that symlink; nothing else is needed
# (musl has no separate libdl/libm/libpthread — all folded into libc).
if [ "${LINKAGE}" = "dynamic" ]; then
  LIBC_SO="$(${ROOTFS_CROSS_COMPILE}gcc -print-file-name=libc.so)"
  [ -f "${LIBC_SO}" ] || die "musl libc.so not found (gcc -print-file-name=libc.so -> '${LIBC_SO}')"
  # The interpreter path busybox actually requests (defensive: verify it matches).
  WANT_INTERP="$(${ROOTFS_CROSS_COMPILE}readelf -l "${BB}" 2>/dev/null \
                 | sed -n 's/.*Requesting program interpreter: \(.*\)\]/\1/p')"
  log "dynamic: PT_INTERP = ${WANT_INTERP:-<none>}; staging musl loader into /lib"
  [ "${WANT_INTERP}" = "/lib/ld-musl-armhf.so.1" ] \
    || die "unexpected interpreter '${WANT_INTERP}' (expected /lib/ld-musl-armhf.so.1)"
  mkdir -p "${ROOTFS_DIR}/lib"
  install -m 0755 "${LIBC_SO}" "${ROOTFS_DIR}/lib/libc.so"
  ln -sf libc.so "${ROOTFS_DIR}/lib/ld-musl-armhf.so.1"   # loader == libc (musl)
fi

# --- 4. pack a root-owned cpio.gz via gen_init_cpio --------------------------
# gen_init_cpio reads a device-table listing; it emits a cpio with the ownership
# we declare (uid/gid 0) and can create device nodes without any privilege —
# cleaner and more reproducible than mknod-as-root or fakeroot.
log "generating initramfs listing"
LISTING="${BUILD_DIR}/initramfs-${LINKAGE}.list"
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

log "packing ${OUT_IMAGE}"
mkdir -p "${OUTPUT_DIR}"
"${GEN_INIT_CPIO}" "${LISTING}" | gzip -9 > "${OUTPUT_DIR}/${OUT_IMAGE}"

# --- report ------------------------------------------------------------------
SIZE="$(du -h "${OUTPUT_DIR}/${OUT_IMAGE}" | cut -f1)"
# applet symlinks minus any lib symlinks we added (keep the count meaningful)
NAPPLETS="$(find "${ROOTFS_DIR}/bin" "${ROOTFS_DIR}/sbin" "${ROOTFS_DIR}/usr" -type l 2>/dev/null | wc -l)"
DYN_NOTE=""
[ "${LINKAGE}" = "dynamic" ] && DYN_NOTE="
  Loader     : /lib/ld-musl-armhf.so.1 -> /lib/libc.so (musl: loader == libc)"
cat <<EOF

$(printf '\033[1;32m[03-rootfs] DONE\033[0m')
  BusyBox    : ${BUSYBOX_VERSION} (${LINKAGE}, ARM)
  Applets    : ${NAPPLETS} symlinks → /bin/busybox${DYN_NOTE}
  Init       : /init (PID 1) mounts proc/sys/dev, execs /bin/sh on the console
  Artifact   : ${OUTPUT_DIR}/${OUT_IMAGE}  (${SIZE})

Build the other linkage with:
  $([ "${LINKAGE}" = static ] && echo "LINKAGE=dynamic make rootfs KERNEL=mainline LIBC=musl COREUTILS=busybox" || echo "make rootfs KERNEL=mainline LIBC=musl COREUTILS=busybox   # static (default)")

Boot it under QEMU with the custom kernel:
  qemu-system-arm -M virt -cpu cortex-a7 -m 128M -nographic -net none \\
    -kernel ${REPO_ROOT}/kernel/build/virt/gv3kernel.bin -initrd ${OUTPUT_DIR}/${OUT_IMAGE}
EOF
