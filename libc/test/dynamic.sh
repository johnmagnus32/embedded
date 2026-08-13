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
#   gv3  — OUR dynamic rootfs (make LINK=dynamic) driven by OUR ld-gv3.so.1. This is
#          the from-scratch loader under test: it maps /lib/libc.so, relocates, and
#          hands off to an interactive /bin/sh (the 'gv3$' prompt is the marker).
#          Skipped unless --gv3 is asked (it builds the dynamic rootfs via make).
#
# The mainline reference kernel is a build artifact (reproducible from the kernel
# source tree via a git worktree); this script builds it once into
# build/refkernel/ if absent. Unlike qemu-arm user-mode (only aarch64 is packaged
# on this host), qemu-system-arm + a real kernel is available and dependency-free.
#
# Usage:
#   ./test/dynamic.sh            # run the 'ref' known-good case (validate harness)
#   ./test/dynamic.sh --gv3      # ALSO run our dynamic rootfs through ld-gv3.so.1
#   REBUILD_KERNEL=1 ./test/dynamic.sh   # force-rebuild the reference kernel
#
# Exit 0 iff every case that ran PASSED.
set -u

# ---- locate ourselves + project dirs ----------------------------------------
# This test lives at libc/test/ (a repo-root PROVIDER). The gv3 case's dynamic rootfs is
# built by the forge ENGINE proper — `make -C <product> rootfs LIBC=custom LINKAGE=dynamic`
# walks the graph (libc -> pkg-coreutils -> pack), so this harness no longer
# hand-runs any engine internals; it just invokes make and boots the result.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${HERE}/../.." && pwd)"          # repo root (libc/ is a top-level provider)
PROJ="${REPO_ROOT}/projects/gameboy-v3"           # the product
BUILD="${PROJ}/build/dynbed"                      # this harness's own scratch dir (ref case)
LOGDIR="${PROJ}/build/test"
MUSL_BIN="${PROJ}/build/toolchain-musl/bin"
GLIBC_BIN="${PROJ}/build/toolchain/bin"
HOSTMAKE_BIN="${PROJ}/build/hostmake/bin"        # GNU Make >=4 (kernel needs it)
# gen_init_cpio: an engine HOST PACKAGE (forge/hostpackages/gen_init_cpio) — forge fetches +
# compiles it into build/hosttools/bin/ as part of `make toolchain`. pack_initrd provisions it
# via `make toolchain` if absent. (This harness still needs build/linux too, but for its
# REFERENCE KERNEL — see below — not for the cpio writer.)
GEN_INIT_CPIO="${PROJ}/build/hosttools/bin/gen_init_cpio"

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
    || die "glibc cross toolchain missing — run 'make -C ${PROJ} toolchain'"
  [ -x "${HOSTMAKE_BIN}/make" ] || die "build/hostmake/make (GNU Make >=4) missing — run 'make -C ${PROJ} toolchain'"
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
    # gcc plugins need plugin headers (gmp/mpfr/mpc) — disable them, they aren't needed to boot.
    # (The engine's mainline kernel recipe instead installs those headers; this harness just skips them.)
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
# Walks the tree (dir/file/slink) + appends /dev nodes, like the engine's rootfs pack
# (forge/steps/rootfs/recipe.sh). $1 = staging dir, $2 = output cpio.gz.
pack_initrd() {
  local stage="$1" out="$2"
  # gen_init_cpio is an engine HOST PACKAGE (forge fetches + compiles it). Provision it
  # via the product's `make toolchain` if this tree hasn't run it yet — no vendored copy.
  if [ ! -x "${GEN_INIT_CPIO}" ]; then
    info "provisioning gen_init_cpio (make toolchain) ..."
    make -C "${PROJ}" toolchain >/dev/null 2>&1 || true
    [ -x "${GEN_INIT_CPIO}" ] || die "gen_init_cpio missing at ${GEN_INIT_CPIO} — run 'make -C ${PROJ} toolchain'"
  fi
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
  command -v "$cc" >/dev/null 2>&1 || die "musl toolchain missing — run 'make -C ${PROJ} toolchain'"
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
  info "building OUR dynamic rootfs (make rootfs LIBC=custom LINKAGE=dynamic BOARD=virt) ..."
  # Drive the forge ENGINE, not its internals: `make rootfs` walks the graph
  #   libc (builds gv3libc + ld-gv3.so.1 into LIBC_STAGE_DIR)
  #     -> pkg-coreutils (links against that libc)
  #     -> rootfs (packs the artifact, named by rootfs-tag+link).
  # The `rootfs: libc` edge guarantees the libc is built before the packages link,
  # so we don't sequence it by hand anymore. Knobs:
  #   LIBC=custom      the gv3libc from-source libc (its recipe's PKG_SOURCE = repo-root libc/;
  #                    the engine resolves LIBC_SRC/LIBC_STAGE_DIR — nothing to pass here).
  #   LINKAGE=dynamic  -> PT_INTERP=/lib/ld-gv3.so.1.
  #   BOARD=virt       the QEMU emulator board (boards/virt/, VFP-free arch) — matches the
  #                    reference kernel we boot below.
  # `rootfs` (not `image`) is the minimal target: we boot our own reference kernel, so no
  # zImage/bootloader/DTB is needed. Build output is shown — a silenced failure here once
  # booted a stale artifact and looked like a linker bug. The rootfs name is keyed by
  # (rootfs-tag + link): LIBC=custom PACKAGES=coreutils LINKAGE=dynamic -> the name below.
  make -C "${PROJ}" rootfs LIBC=custom LINKAGE=dynamic BOARD=virt PACKAGES=coreutils \
    || die "our dynamic rootfs failed to build"
  # our init.sh is a shebang script; the mainline kernel needs /bin/sh to be OUR
  # dynamic shell, loaded by OUR ld-gv3.so.1. PASS = the loader mapped libc.so,
  # relocated, and reached the interactive shell prompt ('gv3$').
  run_case gv3 "${PROJ}/build/output/initramfs-custom-coreutils-dynamic.cpio.gz" "gv3\$"
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
