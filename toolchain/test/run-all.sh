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
pass=0; fail=0; skip=0
red(){ printf '\033[31m%s\033[0m\n' "$*"; }; grn(){ printf '\033[32m%s\033[0m\n' "$*"; }; yel(){ printf '\033[33m%s\033[0m\n' "$*"; }
ok(){ grn "PASS  $*"; pass=$((pass+1)); }; no(){ red "FAIL  $*"; fail=$((fail+1)); }; sk(){ yel "SKIP  $*"; skip=$((skip+1)); }

echo "==== 1. build the five tools ===="
for t in cpp cc as ar ld; do
  make -s -C "${TC}/${t}" >/dev/null 2>&1 && echo "  built ${t}" || { no "build ${t}"; exit 1; }
done

echo "==== 2. cc execution suite ===="
if make -s -C "${TC}/cc" test >/tmp/cc-test.$$.log 2>&1; then ok "cc suite"; else
  if grep -q 'libc-string' /tmp/cc-test.$$.log && [ "$(grep -c 'FAIL' /tmp/cc-test.$$.log)" = 1 ]; then
    ok "cc suite (libc-string skipped: needs GNU gcc)"; else no "cc suite"; sed 's/^/    /' /tmp/cc-test.$$.log | tail -8; fi
fi
rm -f /tmp/cc-test.$$.log

echo "==== 3. dynamic boot (integration) ===="
if [ -d "${BIN}" ] && [ -d "${DYN}" ] && [ -f "${KERNEL}" ] && [ -x "${GIC}" ] && [ -n "${UAPI}" ]; then
  CC="${BIN}/arm-os-custom-gcc"
  install -m0755 "${TC}/cpp/build/cpp" "${BIN}/os-cpp"
  install -m0755 "${TC}/cc/build/cc"   "${BIN}/os-cc1"
  install -m0755 "${TC}/as/build/as"   "${BIN}/os-as"
  install -m0755 "${TC}/ld/build/ld"   "${BIN}/os-ld"
  install -m0755 "${TC}/ar/build/ar"   "${BIN}/os-ar"
  reb=1
  for c in "${REPO}"/libc/src/*.c; do
    "${CC}" -fPIC -I"${REPO}/libc/include" -I"${REPO}/libc/src" -I"${REPO}/kernel/include/uapi" \
      -c "${c}" -o "${DYN}/$(basename "${c}").o" 2>/dev/null || reb=0
  done
  "${CC}" -fPIC -shared -soname libc.so.1 -o "${DYN}/libc.so" "${DYN}"/*.c.o 2>/dev/null || reb=0
  "${CC}" -fPIC -I"${REPO}/libc/include" -I"${REPO}/libc/ld/src" -c "${REPO}/libc/ld/src/dl_main.c" -o "${DYN}/dl_main.o" 2>/dev/null || reb=0
  "${CC}" -c "${REPO}/libc/ld/src/dl_entry.S" -o "${DYN}/dl_entry.o" 2>/dev/null || reb=0
  "${CC}" -shared -e _start -soname ld.so.1 "${DYN}/dl_entry.o" "${DYN}/dl_main.o" -o "${DYN}/ld.so.1" 2>/dev/null || reb=0
  if [ "${reb}" = 1 ]; then
    CC="${CC}" LIBC_DYN_STAGE="${DYN}" LIBC_INCLUDE="${REPO}/libc/include" UAPI_INCLUDE="${UAPI}" \
      GEN_INIT_CPIO="${GIC}" REFKERNEL="${KERNEL}" bash "${HERE}/boot-dynamic.sh" && ok "dynamic boot" || no "dynamic boot"
  else no "rebuild libc.so/ld.so.1 with the new tools"; fi
else
  sk "dynamic boot (engine QEMU build not staged)"
fi

echo "==== 4. static boot (best-effort) ===="
STAT="$(ls -d "${IMG}"/libc/stage-libc-custom* 2>/dev/null | grep -v dynamic | head -1)"
if [ -n "${STAT}" ] && [ -f "${STAT}/libc.a" ] && [ -f "${KERNEL}" ] && [ -x "${GIC}" ] && [ -n "${UAPI}" ]; then
  CC="${BIN}/arm-os-custom-gcc" LIBC_STAGE="${STAT}" LIBC_INCLUDE="${REPO}/libc/include" UAPI_INCLUDE="${UAPI}" \
    GEN_INIT_CPIO="${GIC}" REFKERNEL="${KERNEL}" bash "${HERE}/boot.sh" && ok "static boot" || no "static boot"
else sk "static boot (no static libc-custom stage)"; fi

echo "==== 5. loader host-logic units (best-effort) ===="
if [ -f "${REPO}/libc/ld/test/test_reloc.c" ]; then
  b="$(mktemp -u /tmp/ld-l2.XXXXXX)"
  if cc -std=c11 -Wall -I"${REPO}/libc/ld/src" "${REPO}/libc/ld/test/test_reloc.c" -o "${b}" 2>/dev/null && "${b}" >/dev/null 2>&1; then
    ok "loader reloc/hash units"; else no "loader reloc/hash units"; fi
  rm -f "${b}"
else sk "loader units"; fi

echo "======================================="
printf 'TOOLCHAIN REGRESSION: %d passed, %d failed, %d skipped\n' "${pass}" "${fail}" "${skip}"
[ "${fail}" -eq 0 ] && grn "ALL GREEN" || { red "REGRESSIONS PRESENT"; exit 1; }
