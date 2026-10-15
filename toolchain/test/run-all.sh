#!/usr/bin/env bash
# run-all.sh — the toolchain REGRESSION GATE. One command, GNU-independent: run after every change to
# cc/cpp/as/ld or the libc/loader to confirm nothing regressed.
#   1. build all five tools (cpp/cc/as/ar/ld)
#   2. cc execution suite (compile->assemble->link->run on QEMU, exit-code checked)
#   3. INTEGRATION: rebuild libc.so + ld.so.1 from source with the just-built tools, then boot a
#      dynamically-linked program on QEMU (the end-to-end proof that caught this project's cc codegen bugs)
#   4. (best-effort) static boot + loader host-logic units, when inputs are present
# as/ld GNU "gold" suites gate PARITY (byte-identical vs GNU) and SKIP unless a GNU cross-gcc is built.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TC="$(cd "${HERE}/.." && pwd)"; REPO="$(cd "${TC}/.." && pwd)"
IMG="${REPO}/projects/gameboy-v3/image/build/qemu"
BIN="${IMG}/toolchain-custom/bin"; DYN="${IMG}/libc/stage-libc-custom-dynamic"
KERNEL="${IMG}/output/zImage"; GIC="${IMG}/hosttools/bin/gen_init_cpio"
UAPI="$(find "${IMG}" -type d -name include -path '*linux-libc-headers*' 2>/dev/null | head -1)"
pass=0; fail=0; skip=0; known=0
red(){ printf '\033[31m%s\033[0m\n' "$*"; }; grn(){ printf '\033[32m%s\033[0m\n' "$*"; }; yel(){ printf '\033[33m%s\033[0m\n' "$*"; }
ok(){ grn "PASS  $*"; pass=$((pass+1)); }; no(){ red "FAIL  $*"; fail=$((fail+1)); }; sk(){ yel "SKIP  $*"; skip=$((skip+1)); }
# A DECLARED, pinned difference from GNU: not a pass, and it must show in the summary rather than vanish.
kn(){ yel "KNOWN $*"; known=$((known+1)); }

echo "==== 1. build every tool ===="
TOOLS="cpp cc as ar ld rt nm objcopy objdump readelf size addr2line"
for t in ${TOOLS}; do
  make -s -C "${TC}/${t}" >/dev/null 2>&1 && echo "  built ${t}" || { no "build ${t}"; exit 1; }
done

echo "==== 1b. reference kernel provenance ===="
# ${KERNEL} is the ENGINE'S DEPLOY SLOT, written by whichever kernel provider last built — so the thing the
# boot steps below boot is not necessarily the thing anyone intended. State it, or refuse.
if [ -f "${KERNEL}" ]; then
  if prod="$(bash "${HERE}/refkernel-check.sh" "${KERNEL}")"; then
    ok "reference kernel: ${prod#Linux version }"
  else no "reference kernel provenance"; printf 'TOOLCHAIN REGRESSION: aborted (unidentified reference kernel)\n'; exit 1; fi
else sk "reference kernel provenance (no kernel at ${KERNEL})"; fi

echo "==== 2. every tool's own suite ===="
# EVERY tool, not just cc. Eleven of them have a `make test` and this gate used to run exactly one, so a
# regression in as, ld, ar, objcopy, nm, objdump, readelf, size, rt or cpp passed unnoticed — including the
# suites that cover the float-ABI check, non-allocated section placement, and 36 cases of assembler parity.
#
# A failure is a failure. There used to be a clause here that passed the cc suite when the log mentioned
# 'libc-string' and the FAIL count was 1 — but all three libc-string branches in cc/Makefile print that
# literal string, so the first half was always true and the clause reduced to "exactly one FAIL ⇒ PASS".
#
# objcopy is the one declared exception, and it is held to an exact count rather than waved through: all 14 of
# its differences are .bss's sh_offset on the single 121 MiB vmlinux input. GNU's rule for a NOBITS section's
# nominal offset depends on the file kind — aligned in a relocatable object (main.o: input 21700, GNU 21704,
# addralign 8), the running offset in an executable (vmlinux: input 113816448, GNU 113816432, addralign 64).
# Applying either rule uniformly makes things worse, measured: 14 -> 24 differences one way, 14 -> 124 the
# other. Until that is done properly the count is pinned here, so a 15th difference still fails the gate.
OBJCOPY_KNOWN_DIFF=14
for t in ${TOOLS}; do
  log=/tmp/suite-${t}.$$.log
  if make -s -C "${TC}/${t}" test >"${log}" 2>&1; then ok "${t} suite"
  elif [ "${t}" = objcopy ] && [ "$(sed -n 's/.*objcopy-diff: [0-9]* identical, \([0-9]*\) different.*/\1/p' "${log}" | tail -1)" = "${OBJCOPY_KNOWN_DIFF}" ]; then
    kn "${t} suite: ${OBJCOPY_KNOWN_DIFF} declared differences (.bss sh_offset on vmlinux; see the comment above)"
  else
    no "${t} suite"; grep -E '^FAIL|passed|identical' "${log}" | tail -6 | sed 's/^/    /'
  fi
  rm -f "${log}"
done

echo "==== 3. dynamic boot (integration) ===="
if [ -d "${BIN}" ] && [ -d "${DYN}" ] && [ -f "${KERNEL}" ] && [ -x "${GIC}" ] && [ -n "${UAPI}" ]; then
  CC="${BIN}/arm-os-custom-gcc"
  install -m0755 "${TC}/cpp/build/cpp" "${BIN}/os-cpp"
  install -m0755 "${TC}/cc/build/cc"   "${BIN}/os-cc1"
  install -m0755 "${TC}/as/build/as"   "${BIN}/os-as"
  install -m0755 "${TC}/ld/build/ld"   "${BIN}/os-ld"
  install -m0755 "${TC}/ar/build/ar"   "${BIN}/os-ar"
  install -m0755 "${TC}/os-cc"         "${BIN}/arm-os-custom-gcc"               # the driver (links the runtime)
  mkdir -p "${BIN}/../lib"; install -m0644 "${TC}/rt/build/libosrt.a" "${BIN}/../lib/libosrt.a"
  reb=1; rm -f "${DYN}"/*.c.o   # objects of sources since moved/removed must not be linked
  for c in "${REPO}"/libc/src/*.c; do
    "${CC}" -fPIC -I"${REPO}/libc/include" -I"${REPO}/libc/src" -I"${REPO}/kernel/include/uapi" \
      -c "${c}" -o "${DYN}/$(basename "${c}").o" 2>/dev/null || reb=0
  done
  "${CC}" -fPIC -shared -soname libc.so.1 -o "${DYN}/libc.so" "${DYN}"/*.c.o 2>/dev/null || reb=0
  "${CC}" -fPIC -I"${REPO}/libc/include" -I"${REPO}/libc/ld/src" -c "${REPO}/libc/ld/src/dl_main.c" -o "${DYN}/dl_main.o" 2>/dev/null || reb=0
  "${CC}" -c "${REPO}/libc/ld/src/dl_entry.S" -o "${DYN}/dl_entry.o" 2>/dev/null || reb=0
  "${CC}" -shared -e _start -soname ld.so.1 -Wl,-Bsymbolic "${DYN}/dl_entry.o" "${DYN}/dl_main.o" -o "${DYN}/ld.so.1" 2>/dev/null || reb=0
  if [ "${reb}" = 1 ]; then
    CC="${CC}" LIBC_DYN_STAGE="${DYN}" LIBC_INCLUDE="${REPO}/libc/include" UAPI_INCLUDE="${UAPI}" \
      GEN_INIT_CPIO="${GIC}" REFKERNEL="${KERNEL}" bash "${HERE}/boot-dynamic.sh" && ok "dynamic boot" || no "dynamic boot"
    CC="${CC}" LIBC_DYN_STAGE="${DYN}" LIBC_INCLUDE="${REPO}/libc/include" UAPI_INCLUDE="${UAPI}" \
      GEN_INIT_CPIO="${GIC}" REFKERNEL="${KERNEL}" PIC=0 SRC="${HERE}/boot/nopic.c" BUILD="${HERE}/.bootbed-nopic" \
      MARKER="nopic: environ shared; libc function pointers work" bash "${HERE}/boot-dynamic.sh" \
      && ok "dynamic boot, non-PIC program (copy relocs + canonical PLT)" || no "dynamic boot, non-PIC program"
    CC="${CC}" LIBC_DYN_STAGE="${DYN}" LIBC_INCLUDE="${REPO}/libc/include" UAPI_INCLUDE="${REPO}/kernel/include/uapi" \
      GEN_INIT_CPIO="${GIC}" REFKERNEL="${KERNEL}" SRC="${HERE}/boot/relro.c" BUILD="${HERE}/.bootbed-relro" \
      MARKER="relro: .data.rel.ro is read-only after relocation (alpha beta)" bash "${HERE}/boot-dynamic.sh" \
      && ok "dynamic boot, RELRO write-protected" || no "dynamic boot, RELRO"
  else no "rebuild libc.so/ld.so.1 with the new tools"; fi
else
  sk "dynamic boot (engine QEMU build not staged)"
fi

echo "==== 4. static boot (best-effort) ===="
STAT="$(ls -d "${IMG}"/libc/stage-libc-custom* 2>/dev/null | grep -v dynamic | head -1)"
if [ -n "${STAT}" ] && [ -f "${STAT}/libc.a" ] && [ -f "${KERNEL}" ] && [ -x "${GIC}" ] && [ -n "${UAPI}" ]; then
  # rebuild libc.a from the CURRENT sources with the new tools (the staged one may predate them)
  ST="$(mktemp -d)"; reb=1
  for c in "${REPO}"/libc/src/*.c; do
    "${BIN}/arm-os-custom-gcc" -I"${REPO}/libc/include" -I"${REPO}/libc/src" -I"${REPO}/kernel/include/uapi" \
      -c "${c}" -o "${ST}/$(basename "${c}").o" 2>/dev/null || reb=0
  done
  rm -f "${STAT}/libc.a"; "${BIN}/os-ar" rc "${STAT}/libc.a" "${ST}"/*.c.o || reb=0; rm -rf "${ST}"
  if [ "${reb}" = 1 ]; then
    CC="${BIN}/arm-os-custom-gcc" LIBC_STAGE="${STAT}" LIBC_INCLUDE="${REPO}/libc/include" UAPI_INCLUDE="${UAPI}" \
      GEN_INIT_CPIO="${GIC}" REFKERNEL="${KERNEL}" bash "${HERE}/boot.sh" && ok "static boot" || no "static boot"
  else no "rebuild libc.a with the new tools"; fi
else sk "static boot (no static libc-custom stage)"; fi

echo "==== 5. musl link: OUR ld against a real GCC-built libc.a (best-effort) ===="
MUSL="${IMG}/libc/stage-musl-static"; GNUCC="${IMG}/toolchain-gcc/bin/arm-forge-linux-gnueabihf-"
if [ -f "${MUSL}/usr/lib/libc.a" ] && [ -x "${GNUCC}gcc" ] && [ -f "${KERNEL}" ] && [ -x "${GIC}" ]; then
  MUSL="${MUSL}" GNU="${GNUCC}" REFKERNEL="${KERNEL}" GEN_INIT_CPIO="${GIC}" RT=ours bash "${HERE}/musl-link.sh" \
    && ok "musl link (our ld + musl libc.a + our rt)" || no "musl link"
else sk "musl link (no musl static stage / reference gcc)"; fi

echo "==== 6. musl SELF-BUILD: real musl built by our whole toolchain, then booted ===="
if [ -d "${IMG}/scratch/musl/src" ] && [ -x "${BIN}/arm-os-custom-gcc" ] && [ -f "${KERNEL}" ] && [ -x "${GIC}" ] \
   && [ -d "${IMG}/libc/stage-musl-static/usr/include" ]; then
  MUSL_SRC="${IMG}/scratch/musl/src" TC="${BIN}" SYSROOT_INC="${IMG}/libc/stage-musl-static/usr/include" \
    REFKERNEL="${KERNEL}" GEN_INIT_CPIO="${GIC}" WORK="${IMG}/scratch/musl-selfbuild" bash "${HERE}/musl-selfbuild.sh" \
    && ok "musl self-build (our cpp/cc/as/ar/ld built libc.a; 4 programs booted)" || no "musl self-build"
else sk "musl self-build (no musl source tree / custom toolchain / kernel)"; fi

echo "==== 7. loader host-logic units (best-effort) ===="
if [ -f "${REPO}/libc/ld/test/test_reloc.c" ]; then
  b="$(mktemp -u /tmp/ld-l2.XXXXXX)"
  if cc -std=c11 -Wall -I"${REPO}/libc/ld/src" "${REPO}/libc/ld/test/test_reloc.c" -o "${b}" 2>/dev/null && "${b}" >/dev/null 2>&1; then
    ok "loader reloc/hash units"; else no "loader reloc/hash units"; fi
  rm -f "${b}"
else sk "loader units"; fi

echo "==== 8. driver option parity vs gcc (-g family) ===="
if [ -x "${BIN}/arm-os-custom-gcc" ]; then
  if CC="${BIN}/arm-os-custom-gcc" GNU="${GNUCC}" bash "${HERE}/driver-opts.sh" > /tmp/dopt.$$.log 2>&1; then
    ok "$(tail -1 /tmp/dopt.$$.log)"; else no "driver options"; sed 's/^/    /' /tmp/dopt.$$.log | tail -8; fi
  rm -f /tmp/dopt.$$.log
else sk "driver options (no installed os-cc)"; fi

# ---- FULL=1: the slow, broad suites. Minutes rather than seconds, so not on the default path, but these are
# the widest coverage the project has and nothing ran them automatically either.
#   kernel-parity    324 real kernel objects, our cc's output through our as vs GNU as, byte for byte
#   kernel-parity-S  106 real kernel .S files, including the EHABI unwind tables
#   torture          1548 gcc.c-torture/execute programs, compiled+linked+run
#   torture-compile  1927 gcc.c-torture/compile programs
#   gnu-diff         results compared against GCC's rather than predicted
#   builtin-diff     14887 builtin cases against exact arithmetic
#   abi-interop      AAPCS/AAPCS-VFP calls both directions against GCC
#   headers-diff     our compiler headers' constants against GCC's
if [ "${FULL:-0}" = 1 ]; then
  echo "==== 9. broad suites (FULL=1) ===="
  run_full() {   # run_full <label> <make-target> <dir>
    local label=$1 tgt=$2 dir=$3
    local log=/tmp/full-${tgt}.$$.log     # (separate statement: ${tgt} is not set yet within the one above)
    if make -s -C "${dir}" "${tgt}" >"${log}" 2>&1; then ok "${label}: $(grep -E 'identical|pass|PASS' "${log}" | tail -1 | sed 's/^ *//')"
    else no "${label}"; grep -E 'FAIL|differ|identical|pass' "${log}" | tail -5 | sed 's/^/    /'; fi
    rm -f "${log}"
  }
  run_full "kernel parity (C)"   kernel-parity    "${TC}/as"
  run_full "kernel parity (.S)"  kernel-parity-S  "${TC}/as"
  run_full "torture (execute)"   torture          "${TC}/cc"
  run_full "torture (compile)"   torture-compile  "${TC}/cc"
  run_full "gnu-diff"            gnu-diff         "${TC}/cc"
  run_full "builtin-diff"        builtin-diff     "${TC}/cc"
  run_full "abi-interop"         abi-interop      "${TC}/cc"
  run_full "headers-diff"        headers-diff     "${TC}/cc"
fi

echo "======================================="
printf 'TOOLCHAIN REGRESSION: %d passed, %d failed, %d skipped, %d known-different\n' "${pass}" "${fail}" "${skip}" "${known}"
[ "${fail}" -eq 0 ] && grn "ALL GREEN" || { red "REGRESSIONS PRESENT"; exit 1; }
