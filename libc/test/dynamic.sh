#!/usr/bin/env bash
# dynamic.sh — the KNOWN-GOOD reference-loader test bed for dynamic linking.
#
# WHY THIS EXISTS: our from-scratch dynamic linker (ld-gv3.so.1) is the most
# error-prone thing in the project. To develop it we need a loader we already
# TRUST — so a failure is unambiguously OUR code, not the runner. This harness
# boots a MAINLINE ARM Linux kernel (full dynamic-linking support) under QEMU
# `-M virt` and runs a dynamic ARM binary as init.
#
# It has two modes, sharing one boot+check path:
#   ref  — a KNOWN-GOOD musl-dynamic binary (built with the normal musl toolchain,
#          linked against musl's real libc.so + ld-musl). Proves the HARNESS
#          itself is correct. Always available. This is the baseline you run
#          first / whenever you doubt the test bed.
#   gv3  — OUR dynamic rootfs (make LINK=dynamic) + OUR ld-gv3.so.1 once it exists.
#          Currently EXPECTED TO FAIL (no real linker yet) — that's the point:
#          this is the target we develop against. Skipped unless --gv3 is asked.
#
# The mainline reference kernel is a build artifact (reproducible from the kernel
# source tree via a git worktree); this script builds it once into
# build/refkernel/ if absent. Unlike qemu-arm user-mode (only aarch64 is packaged
# on this host), qemu-system-arm + a real kernel is available and dependency-free.
#
# Usage:
#   ./test/dynamic.sh            # run the 'ref' known-good case (validate harness)
#   ./test/dynamic.sh --gv3      # ALSO run our dynamic rootfs (dev target)
#   REBUILD_KERNEL=1 ./test/dynamic.sh   # force-rebuild the reference kernel
#
# Exit 0 iff every case that ran PASSED.
set -u

# ---- locate ourselves + project dirs ----------------------------------------
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RDIR="$(cd "${HERE}/.." && pwd)"                 # projects/gameboy-v3/rootfs
PROJ="$(cd "${RDIR}/.." && pwd)"                 # projects/gameboy-v3
BUILD="${RDIR}/build"
LOGDIR="${BUILD}/test"
MUSL_BIN="${PROJ}/build/toolchain-musl/bin"
GLIBC_BIN="${PROJ}/build/toolchain/bin"
HOSTMAKE_BIN="${PROJ}/build/hostmake/bin"        # GNU Make >=4 (kernel needs it)
GEN_INIT_CPIO="${PROJ}/build/linux/usr/gen_init_cpio"

# Reference mainline kernel (built here if missing) + the source worktree.
REFKERNEL="${PROJ}/build/refkernel/virt-zImage"
KSRC="${PROJ}/build/linux"                        # the (dirty, in-tree) sunxi build
WORKTREE="${PROJ}/build/refkernel/linux-src"      # clean worktree for the virt build

MUSL_PREFIX="arm-buildroot-linux-musleabihf-"
GLIBC_PREFIX="arm-buildroot-linux-gnueabihf-"
QEMU="${QEMU:-qemu-system-arm}"

red() { printf '\033[31m%s\033[0m\n' "$*"; }
grn() { printf '\033[32m%s\033[0m\n' "$*"; }
ylw() { printf '\033[33m%s\033[0m\n' "$*"; }
info(){ printf '  %s\n' "$*"; }
die() { red "ERROR: $*"; exit 1; }

mkdir -p "${LOGDIR}"

# ---- 1. reference kernel: build once if missing -----------------------------
build_ref_kernel() {
  command -v "${GLIBC_BIN}/${GLIBC_PREFIX}gcc" >/dev/null 2>&1 \
    || die "glibc cross toolchain missing — run forge/backends/toolchain.sh"
  [ -x "${HOSTMAKE_BIN}/make" ] || die "build/hostmake/make (GNU Make >=4) missing — run forge/backends/toolchain.sh"
  [ -d "${KSRC}/.git" ] || die "${KSRC} is not a git checkout (need it for a worktree)"

  ylw "building the mainline reference virt kernel (one-time, ~minutes) ..."
  # A clean worktree at the same commit — does NOT disturb the in-tree sunxi build.
  if [ ! -f "${WORKTREE}/Makefile" ]; then
    git -C "${KSRC}" worktree add -f "${WORKTREE}" HEAD >/dev/null 2>&1 \
      || die "git worktree add failed"
  fi
  (
    cd "${WORKTREE}"
    export PATH="${HOSTMAKE_BIN}:${GLIBC_BIN}:${PATH}"
    export ARCH=arm CROSS_COMPILE="${GLIBC_PREFIX}"
    make multi_v7_defconfig >/dev/null 2>&1 || exit 1
    # same workaround as forge/backends/kernel.sh: gcc plugins need plugin headers we
    # don't ship — disable them, they aren't needed to boot.
    ./scripts/config --disable GCC_PLUGINS
    make olddefconfig >/dev/null 2>&1 || exit 1
    make -j"$(nproc)" zImage >/dev/null 2>&1 || exit 1
  ) || die "reference kernel build failed"
  mkdir -p "$(dirname "${REFKERNEL}")"
  cp "${WORKTREE}/arch/arm/boot/zImage" "${REFKERNEL}" || die "zImage not produced"
  grn "reference kernel -> ${REFKERNEL#${PROJ}/}"
}

if [ "${REBUILD_KERNEL:-0}" = 1 ] || [ ! -f "${REFKERNEL}" ]; then
  build_ref_kernel
else
  info "reference kernel: ${REFKERNEL#${PROJ}/} (present)"
fi

# ---- 2. assemble a dynamic-capable initramfs from a staging tree ------------
# Walks the tree (dir/file/slink) + appends /dev nodes, exactly like the rootfs
# Makefile and forge/backends/rootfs.sh. $1 = staging dir, $2 = output cpio.gz.
pack_initrd() {
  local stage="$1" out="$2"
  [ -x "${GEN_INIT_CPIO}" ] || die "gen_init_cpio missing (run forge/backends/kernel.sh)"
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
  local cc="${MUSL_BIN}/${MUSL_PREFIX}gcc"
  command -v "$cc" >/dev/null 2>&1 || die "musl toolchain missing — run forge/backends/toolchain.sh"
  local sysroot; sysroot="$("$cc" -print-sysroot)"
  # a trivial DYNAMIC program (normal link -> PT_INTERP + DT_NEEDED=libc.so)
  cat > "${BUILD}/refdyn.c" <<'EOF'
#include <stdio.h>
int main(int argc, char **argv){ printf("refdyn: dynamic-linked OK, argc=%d\n", argc); return 0; }
EOF
  "$cc" "${BUILD}/refdyn.c" -o "${stage}/init" || die "refdyn build failed"
  cp "${sysroot}/lib/libc.so" "${stage}/lib/libc.so"
  cp -P "${sysroot}/lib/ld-musl-armhf.so.1" "${stage}/lib/" 2>/dev/null \
    || cp "${sysroot}/lib/ld-musl-armhf.so.1" "${stage}/lib/"
  pack_initrd "${stage}" "${BUILD}/reftest.cpio.gz"
}

# ---- 3. boot the reference kernel with an initramfs, check for a marker -----
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
    grn "  PASS  (saw '${marker}')  log: ${log#${PROJ}/}"
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
if [ "${1:-}" = "--gv3" ]; then
  info "building OUR dynamic rootfs (make LINK=dynamic) ..."
  ( cd "${RDIR}" && make LINK=dynamic BOARD=virt rootfs >/dev/null 2>&1 ) \
    || die "our dynamic rootfs failed to build"
  # our init.sh is a shebang script; the mainline kernel needs /bin/sh to be OUR
  # dynamic shell, and OUR ld-gv3.so.1 to actually load it. Expected to FAIL until
  # the linker exists — this is the target we iterate on.
  run_case gv3 "${BUILD}/rootfs.cpio.gz" "gv3\$"
fi

# ---- summary ----------------------------------------------------------------
printf '\n'
if [ "${fails}" -eq 0 ]; then
  grn "OK — ${ran}/${ran} case(s) passed."
  exit 0
else
  red "${fails}/${ran} case(s) FAILED."
  exit 1
fi
