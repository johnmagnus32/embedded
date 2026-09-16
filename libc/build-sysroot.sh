# libc/build-sysroot.sh — build this libc into a CONFORMING SYSROOT (TOOLCHAIN=source path).
#
# Sourced by forge/core/classes/libc.sh when TOOLCHAIN=source. Compiles the libc with the STAGE-1
# ("initial") from-source gcc into a Linux-style sysroot (headers + crt1/crti/crtn + libc.a + libc.so
# + /lib/ld.so.1) at LIBC_STAGE_DIR. The stage-2 ("final") gcc is then built --with-sysroot=that, so
# packages cross-link NORMALLY against our libc. Complete (both .a AND .so) so the final gcc serves
# static + dynamic without a per-link rebuild of its sysroot contents.
#
# Contract (env in): LIBC_STAGE_DIR (output sysroot), REPO_ROOT, CROSS_COMPILE (arm-forge-…-),
#   LIBC_TC_DIR (the stage-1 toolchain prefix — its bin/ has the gcc we build with),
#   ROOTFS_TARGET (t113|virt, for the ld/ sub-make). This runs in run-recipe.sh's shell.
: "${LIBC_STAGE_DIR:?build-sysroot.sh: LIBC_STAGE_DIR unset}"
: "${LIBC_TC_DIR:?build-sysroot.sh: LIBC_TC_DIR unset (the stage-1 toolchain dir)}"
: "${CROSS_COMPILE:?build-sysroot.sh: CROSS_COMPILE unset}"
LIBC_PROVIDER_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UAPI="${REPO_ROOT}/kernel/include/uapi"
# stage-1 gcc by FULL PATH (NOT on PATH — packages get the stage-2 gcc via TOOLCHAIN_DIR; both
# share the arm-forge- prefix, so we must be explicit about which one builds the libc).
_CC="${LIBC_TC_DIR}/bin/${CROSS_COMPILE}gcc"
_AR="${LIBC_TC_DIR}/bin/${CROSS_COMPILE}ar"
[ -x "${_CC}" ] || die "build-sysroot.sh: stage-1 gcc missing: ${_CC}"
# hard-float VFP baseline (board-independent; per-board -mgeneral-regs-only is a package-compile flag,
# not a libc-build one). Hermetic -nostdinc: only the compiler's own freestanding headers via -isystem.
_GCCINC="$(${_CC} -print-file-name=include)"
_CF="-mcpu=cortex-a7 -marm -ffreestanding -nostdlib -nostartfiles -nostdinc -fno-builtin -fno-stack-protector -Os -Wall"
_IFL="-I${LIBC_PROVIDER_DIR}/include -I${LIBC_STAGE_DIR}/usr/include -isystem ${_GCCINC}"

rm -rf "${LIBC_STAGE_DIR}"; mkdir -p "${LIBC_STAGE_DIR}/usr/include" "${LIBC_STAGE_DIR}/usr/lib" "${LIBC_STAGE_DIR}/lib"
# 1. headers: our libc headers + the staged kernel UAPI snapshot (headers_install model)
cp -r "${LIBC_PROVIDER_DIR}/include/." "${LIBC_STAGE_DIR}/usr/include/"
cp -f "${UAPI}/syscalls.h" "${UAPI}/abi.h" "${LIBC_STAGE_DIR}/usr/include/"
# 2. crt set: crt1 (=our crt0 _start), crti/crtn (.init/.fini frames)
${_CC} ${_CF} ${_IFL} -c "${LIBC_PROVIDER_DIR}/src/crt/crt0.S" -o "${LIBC_STAGE_DIR}/usr/lib/crt1.o"
${_CC} ${_CF} ${_IFL} -c "${LIBC_PROVIDER_DIR}/src/crt/crti.S" -o "${LIBC_STAGE_DIR}/usr/lib/crti.o"
${_CC} ${_CF} ${_IFL} -c "${LIBC_PROVIDER_DIR}/src/crt/crtn.S" -o "${LIBC_STAGE_DIR}/usr/lib/crtn.o"
# 3. libc.a (static) + libc.so (shared, soname libc.so) — both, so the final gcc serves either link
_OBJS=(); _PIC=(); _wd="${LIBC_STAGE_DIR}/.obj"; mkdir -p "${_wd}"
for c in "${LIBC_PROVIDER_DIR}"/src/*.c; do
  b="$(basename "${c}")"
  ${_CC} ${_CF} ${_IFL}       -c "${c}" -o "${_wd}/${b}.o";     _OBJS+=("${_wd}/${b}.o")
  ${_CC} ${_CF} ${_IFL} -fPIC -c "${c}" -o "${_wd}/${b}.pic.o"; _PIC+=("${_wd}/${b}.pic.o")
done
${_AR} rcs "${LIBC_STAGE_DIR}/usr/lib/libc.a" "${_OBJS[@]}"
_LIBGCC="$(${_CC} -print-libgcc-file-name)"
${_CC} ${_CF} -fPIC -shared -Wl,-soname,libc.so -Wl,--build-id=none "${_PIC[@]}" "${_LIBGCC}" -o "${LIBC_STAGE_DIR}/usr/lib/libc.so"
rm -rf "${_wd}"
# 4. the dynamic linker at /lib/ld.so.1 (the libc's own ld/ sub-make, built with the stage-1 gcc)
make -C "${LIBC_PROVIDER_DIR}/ld" BUILD="${LIBC_STAGE_DIR}/.ld-build" BOARD="${ROOTFS_TARGET:-t113}" CROSS_COMPILE="${LIBC_TC_DIR}/bin/${CROSS_COMPILE}" >/dev/null 2>&1
cp -f "${LIBC_STAGE_DIR}/.ld-build/ld.so.1" "${LIBC_STAGE_DIR}/lib/ld.so.1"; rm -rf "${LIBC_STAGE_DIR}/.ld-build"
echo "  [libc] TOOLCHAIN=source: built conforming sysroot -> ${LIBC_STAGE_DIR} (crt1/crti/crtn + libc.a/.so + /lib/ld.so.1)"
