#!/usr/bin/env bash
# dynamic.sh — the KNOWN-GOOD reference-loader test bed for dynamic linking.
#
# WHY THIS EXISTS: our from-scratch dynamic linker (ld.so.1) is the most
# error-prone thing in the project. To develop it we need a loader we already
# TRUST — so a failure is unambiguously OUR code, not the runner. This harness
# boots a MAINLINE ARM Linux kernel (full dynamic-linking support) under QEMU
# `-M virt` and runs a dynamic ARM binary as init.
#
# It has two cases, sharing one boot+check path:
#   ref  — a KNOWN-GOOD musl-dynamic binary, built INLINE here with the cross
#          toolchain (${CROSS_COMPILE}) and linked against that toolchain's real
#          libc.so + ld-musl. Proves the HARNESS itself is correct. Always runs.
#   gv3  — OUR dynamic rootfs (${DYNAMIC_INITRD}), driven by OUR ld.so.1. This is
#          the from-scratch loader under test: it maps /lib/libc.so, relocates,
#          and hands off to an interactive /bin/sh (the 'gv3$' prompt is the marker).
#
# This harness is SELF-CONTAINED: it knows nothing about the os/ build system. It
# takes the reference kernel and our dynamic rootfs as PATHS; whoever drives it is
# responsible for building those and passing them in.
#
# ---- INPUT CONTRACT (environment) -------------------------------------------
#   CROSS_COMPILE    (required) cross-gcc prefix used to build the inline `ref` binary
#                    AND to source musl's libc.so + ld-musl from its baked sysroot,
#                    e.g. /path/to/arm-forge-linux-gnueabihf- .
#   REFKERNEL        (required) path to a bootable MAINLINE QEMU `virt` zImage (full
#                    dynamic-linking support). Booted for BOTH cases.
#   DYNAMIC_INITRD   (required) path to OUR prebuilt dynamically-linked rootfs
#                    (PT_INTERP=/lib/ld.so.1) — booted as the `gv3` case.
#   GEN_INIT_CPIO    (required) path to a gen_init_cpio host binary (packs the ref initramfs).
#   QEMU             (optional) qemu-system-arm binary. Default: qemu-system-arm.
#
# Exit 0 iff every case that ran PASSED.
set -u

: "${CROSS_COMPILE:?dynamic.sh: set CROSS_COMPILE=<cross-gcc prefix (musl sysroot)>}"
: "${REFKERNEL:?dynamic.sh: set REFKERNEL=<path to a bootable mainline virt zImage>}"
: "${DYNAMIC_INITRD:?dynamic.sh: set DYNAMIC_INITRD=<path to our dynamically-linked rootfs cpio.gz>}"
: "${GEN_INIT_CPIO:?dynamic.sh: set GEN_INIT_CPIO=<path to a gen_init_cpio binary>}"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${BUILD:-${HERE}/.dynbed}"                 # this harness's own scratch dir (ref case)
LOGDIR="${LOGDIR:-${HERE}/logs}"
QEMU="${QEMU:-qemu-system-arm}"

red() { printf '\033[31m%s\033[0m\n' "$*"; }
grn() { printf '\033[32m%s\033[0m\n' "$*"; }
ylw() { printf '\033[33m%s\033[0m\n' "$*"; }
info(){ printf '  %s\n' "$*"; }
die() { red "ERROR: $*"; exit 1; }

command -v "${CROSS_COMPILE}gcc" >/dev/null 2>&1 || die "cross gcc not found: ${CROSS_COMPILE}gcc"
[ -f "${REFKERNEL}" ]      || die "REFKERNEL not found: ${REFKERNEL}"
[ -f "${DYNAMIC_INITRD}" ] || die "DYNAMIC_INITRD not found: ${DYNAMIC_INITRD}"
[ -x "${GEN_INIT_CPIO}" ]  || die "GEN_INIT_CPIO not executable: ${GEN_INIT_CPIO}"
command -v "${QEMU}" >/dev/null 2>&1 || die "${QEMU} not on PATH"

mkdir -p "${LOGDIR}" "${BUILD}"

info "reference kernel: ${REFKERNEL} (given)"

# ---- 1. assemble a dynamic-capable initramfs from a staging tree ------------
# Walks the tree (dir/file/slink) + appends /dev nodes. $1 = staging dir, $2 = output cpio.gz.
pack_initrd() {
  local stage="$1" out="$2"
  {
    echo 'dir /dev 0755 0 0'
    echo 'nod /dev/console 0600 0 0 c 5 1'
    cd "${stage}"
    find . -mindepth 1 -path ./dev -prune -o -print | while read -r p; do
      rel="${p#.}"
      if   [ -L "$p" ]; then printf 'slink %s %s 0777 0 0\n' "$rel" "$(readlink "$p")"
      elif [ -d "$p" ]; then printf 'dir %s 0%s 0 0\n' "$rel" "$(stat -c '%a' "$p")"
      elif [ -f "$p" ]; then printf 'file %s %s/%s 0%s 0 0\n' "$rel" "${stage}" "$rel" "$(stat -c '%a' "$p")"
      fi
    done
  } > "${out%.gz}.list"
  "${GEN_INIT_CPIO}" "${out%.gz}.list" | gzip -9 > "${out}"
}

# Build the KNOWN-GOOD reference initramfs: a musl-dynamic binary as /init plus
# musl's real interpreter + libc.so. Proves the loader + harness are correct.
build_ref_initrd() {
  local stage="${BUILD}/reftest"
  rm -rf "${stage}"; mkdir -p "${stage}/lib"
  local cc="${CROSS_COMPILE}gcc"
  local sysroot; sysroot="$("$cc" -print-sysroot)"
  # a trivial DYNAMIC program (normal link -> PT_INTERP + DT_NEEDED=libc.so)
  cat > "${BUILD}/refdyn.c" <<'EOF'
#include <stdio.h>
int main(int argc, char **argv){ printf("refdyn: dynamic-linked OK, argc=%d\n", argc); return 0; }
EOF
  "$cc" "${BUILD}/refdyn.c" -o "${stage}/init" || die "refdyn build failed"
  # from-source musl sysroot layout: the lib is usr/lib/libc.so, and musl's loader IS libc.so
  # (/lib/ld-musl-armhf.so.1 is a symlink to it). Flatten both into the initramfs /lib as real files:
  # ld-musl-armhf.so.1 (PT_INTERP) + libc.so (DT_NEEDED) — same file.
  cp "${sysroot}/usr/lib/libc.so" "${stage}/lib/libc.so"
  cp "${sysroot}/usr/lib/libc.so" "${stage}/lib/ld-musl-armhf.so.1"
  pack_initrd "${stage}" "${BUILD}/reftest.cpio.gz"
}

# ---- 2. boot the reference kernel with an initramfs, check for a marker -----
# run_case <name> <initrd> <required-marker>
run_case() {
  local name="$1" initrd="$2" marker="$3"
  local log="${LOGDIR}/dyn-${name}.log"
  printf '\n=== case: %s ===\n' "$name"
  ran=$((ran + 1))
  # rdinit=/init runs our binary as PID 1; panic=1 + -no-reboot makes a PID-1
  # exit terminate QEMU promptly (our trivial init returns -> expected panic).
  # -net none avoids QEMU's default virtio-net (missing efi-virtio.rom aborts it).
  timeout 60 "$QEMU" -M virt -cpu cortex-a7 -m 128M -nographic -no-reboot -net none \
    -kernel "${REFKERNEL}" -initrd "${initrd}" \
    -append "console=ttyAMA0 rdinit=/init panic=1" >"${log}" 2>&1

  if grep -qF -- "${marker}" "${log}"; then
    grn "  PASS  (saw '${marker}')  log: ${log}"
  else
    red "  FAIL  (marker '${marker}' not found)  log: ${log}"
    ylw "  --- last lines ---"; tail -6 "${log}" | sed 's/^/    /'
    fails=$((fails + 1))
  fi
}

ran=0; fails=0

# ---- case: ref (known-good; validates the harness) --------------------------
info "building known-good musl-dynamic reference initramfs ..."
build_ref_initrd
run_case ref "${BUILD}/reftest.cpio.gz" "refdyn: dynamic-linked OK"

# ---- case: gv3 (our dynamic rootfs; the dev target) -------------------------
# Boots the prebuilt DYNAMIC_INITRD. Our init.sh is a shebang script; the mainline
# kernel needs /bin/sh to be OUR dynamic shell, loaded by OUR ld.so.1. PASS = the
# loader mapped libc.so, relocated, and reached the interactive shell prompt ('gv3$').
run_case gv3 "${DYNAMIC_INITRD}" "gv3\$"

# ---- summary ----------------------------------------------------------------
printf '\n'
if [ "${fails}" -eq 0 ]; then
  grn "OK — ${ran}/${ran} case(s) passed."
  exit 0
else
  red "${fails}/${ran} case(s) FAILED."
  exit 1
fi
