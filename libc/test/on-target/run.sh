#!/usr/bin/env bash
# run.sh — on-target unit tests for the libc's SYSCALL WRAPPERS (need a real kernel).
#
# Syscall-wrapper behavior (e.g. WNOHANG) can't be unit-tested on the host — only a real
# kernel exercises it. So each test program is linked STATICALLY against the custom libc
# and booted as PID 1 on the mainline reference kernel (built by ../dynamic.sh) under QEMU;
# we grep its console for a PASS marker. Same boot pattern as ../dynamic.sh; add cases here
# as socket/epoll/signalfd/timerfd/mmap wrappers land.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIBC="$(cd "${HERE}/../.." && pwd)"
REPO="$(cd "${LIBC}/.." && pwd)"
PROJ="${REPO}/projects/gameboy-v3"
BUILD="${PROJ}/build"
LOGDIR="${BUILD}/test"; mkdir -p "${LOGDIR}"
QEMU="${QEMU:-qemu-system-arm}"
REFKERNEL="${BUILD}/refkernel/virt-zImage"
GEN_INIT_CPIO="${BUILD}/hosttools/bin/gen_init_cpio"
XPREFIX="${BUILD}/toolchain-prebuilt-bootlin-musl/bin/arm-buildroot-linux-musleabihf-"
STAGE="${BUILD}/libc/stage-custom-static"     # crt0.S.o + libc.a
UAPI="${BUILD}/libc/include"                   # staged kernel UAPI (syscalls.h / abi.h)
ARCH_FLAGS="-mcpu=cortex-a7 -marm -mgeneral-regs-only"   # boards/virt/board.conf

red(){ printf '\033[31m%s\033[0m\n' "$*"; }; grn(){ printf '\033[32m%s\033[0m\n' "$*"; }
die(){ red "ERROR: $*"; exit 1; }

# ---- prerequisites --------------------------------------------------------
[ -f "${REFKERNEL}" ] || die "reference kernel missing — run ./test/dynamic.sh once (it builds ${REFKERNEL#${PROJ}/})"
command -v "${XPREFIX}gcc" >/dev/null 2>&1 || die "musl cross toolchain missing — run 'make -C ${PROJ} host-toolchain-gcc'"

# (Re)build the STATIC custom libc so libc.a picks up any libc/src changes (content-hashed;
# a no-op if unchanged). This stages crt0.S.o + libc.a into ${STAGE}.
echo "### staging static custom libc ###"
make -C "${PROJ}" rootfs LIBC=custom INIT=shell LINKAGE=static BOARD=virt PACKAGES=coreutils >/dev/null 2>&1 \
  || die "static custom-libc build failed"
[ -f "${STAGE}/libc.a" ] && [ -f "${STAGE}/crt0.S.o" ] || die "static libc not staged at ${STAGE}"
[ -x "${GEN_INIT_CPIO}" ] || { make -C "${PROJ}" host-gen_init_cpio >/dev/null 2>&1 || true; }
[ -x "${GEN_INIT_CPIO}" ] || die "gen_init_cpio missing (run 'make -C ${PROJ} host-gen_init_cpio')"

# Mirror libc/libc-profile.sh's static contract: hermetic (-nostdinc + only the compiler's
# freestanding headers), and link crt0 + libc.a + libgcc in a group (they're mutually recursive:
# libc uses libgcc's __aeabi_uldivmod, libgcc's div0 handler uses libc's raise).
GCCINC="$(${XPREFIX}gcc -print-file-name=include)"
LIBGCC="$(${XPREFIX}gcc -print-libgcc-file-name)"
CFLAGS="${ARCH_FLAGS} -ffreestanding -nostdlib -nostartfiles -nostdinc -fno-builtin -fno-stack-protector \
-Os -Wall -Wextra -I${LIBC}/include -I${UAPI} -I${LIBC}/src -isystem ${GCCINC}"
LDFLAGS="-T ${LIBC}/user.ld -nostdlib -static -Wl,--build-id=none -Wl,-z,max-page-size=0x1000"

ran=0; fails=0
run_prog() {   # <name.c> <marker>
	local src="${HERE}/$1" name="${1%.c}" marker="$2"
	local dir; dir="$(mktemp -d)"
	printf '\n=== case: %s ===\n' "$name"
	ran=$((ran + 1))
	if ! "${XPREFIX}gcc" ${CFLAGS} ${LDFLAGS} "${STAGE}/crt0.S.o" "${src}" \
	     -Wl,--start-group "${STAGE}/libc.a" "${LIBGCC}" -Wl,--end-group \
	     -o "${dir}/init" 2>"${dir}/cc.log"; then
		red "  FAIL  (link)"; sed 's/^/    /' "${dir}/cc.log"; fails=$((fails + 1)); rm -rf "${dir}"; return
	fi
	{ echo 'dir /dev 0755 0 0'; echo 'nod /dev/console 0600 0 0 c 5 1'
	  printf 'file /init %s/init 0755 0 0\n' "${dir}"; } > "${dir}/list"
	"${GEN_INIT_CPIO}" "${dir}/list" | gzip -9 > "${dir}/initrd.cpio.gz"
	local log="${LOGDIR}/unit-${name}.log"
	timeout 60 "${QEMU}" -M virt -cpu cortex-a7 -m 128M -nographic -no-reboot -net none \
		-kernel "${REFKERNEL}" -initrd "${dir}/initrd.cpio.gz" \
		-append "console=ttyAMA0 rdinit=/init panic=1" > "${log}" 2>&1
	if grep -qF -- "${marker}" "${log}"; then
		grn "  PASS  (saw '${marker}')  log: ${log#${PROJ}/}"
	else
		red "  FAIL  (marker '${marker}' not found)  log: ${log}"; tail -8 "${log}" | sed 's/^/    /'
		fails=$((fails + 1))
	fi
	rm -rf "${dir}"
}

run_prog waittest.c "WNOHANG_OK"

echo
if [ "${fails}" -eq 0 ]; then grn "ON-TARGET UNITS: ${ran}/${ran} PASS"; else red "ON-TARGET UNITS: $((ran - fails))/${ran} pass"; exit 1; fi
