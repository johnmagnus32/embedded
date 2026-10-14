#!/usr/bin/env bash
# refkernel-check.sh <zImage> — name what built the gate's reference kernel, and refuse to run the gate
# against an artifact whose identity has silently changed.
#
# WHY THIS EXISTS. The path the gate uses, projects/gameboy-v3/image/build/qemu/output/zImage, is the
# ENGINE'S DEPLOY SLOT (OUTPUT_DIR, engine.sh). Both kernel providers write it — linux/recipe.sh deploys
# arch/arm/boot/zImage there and so does kernel-custom — so any `MACHINE=qemu make linux` replaces the gate's
# reference without a word, and the next gate run reads as a toolchain regression. That has already happened.
#
# Worse, and the reason this is a gate step rather than a comment: the artifact in that slot is GCC-built
# ("arm-forge-linux-gnueabihf-gcc (GCC) 13.3.0, GNU ld (GNU Binutils) 2.42") while build/qemu/linux/vmlinux
# beside it is ours ("arm-os-custom-linux-gnueabihf-gcc (os toolchain: our cpp/cc/as/ld)"). So the gate's
# three boot steps have been running our USERSPACE on someone else's kernel and calling it an end-to-end
# proof of our toolchain. That is a defensible thing to do deliberately — a known-good kernel isolates
# userspace regressions — but it must be stated, not discovered.
#
#   ./refkernel-check.sh <zImage>            verify against refkernel.provenance; exit 1 on mismatch
#   ./refkernel-check.sh --record <zImage>   rewrite the record (do this deliberately, never from a build)
#   ./refkernel-check.sh --producer <zImage> print just the producer string
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REC="${HERE}/refkernel.provenance"

# A zImage is a self-decompressing image: scan for the gzip member and pull the banner out of the payload.
producer() {
	python3 -I - "$1" <<'EOF'
import sys, zlib, re
d = open(sys.argv[1], 'rb').read()
for i in range(len(d) - 1):
    if d[i] == 0x1f and d[i+1] == 0x8b:
        try:
            raw = zlib.decompressobj(31).decompress(d[i:])
        except Exception:
            continue
        m = re.search(rb'Linux version [^\x00\n]{0,300}', raw)
        if m:
            print(m.group(0).decode('utf8', 'replace'))
            sys.exit(0)
        break
sys.exit(1)
EOF
}

K="${2:-${1:-}}"
[ -n "${K}" ] && [ -f "${K}" ] || { echo "refkernel: no kernel image given/found: '${K}'" >&2; exit 2; }

case "${1:-}" in
--producer) producer "${K}"; exit $? ;;
--record)
	p="$(producer "${K}")" || { echo "refkernel: cannot read a Linux banner out of ${K}" >&2; exit 2; }
	{ printf '# The reference kernel the toolchain gate is entitled to use. Refresh DELIBERATELY:\n'
	  printf '#   toolchain/test/refkernel-check.sh --record <zImage>\n'
	  printf 'sha256 %s\n' "$(sha256sum "${K}" | cut -d" " -f1)"
	  printf 'producer %s\n' "${p}"; } > "${REC}"
	echo "refkernel: recorded ${p}"; exit 0 ;;
esac

[ -f "${REC}" ] || {
	cat >&2 <<EOF
refkernel: no provenance record at ${REC}.
  The gate will not boot an unidentified kernel. Record the one you intend to test against:
    ${HERE}/refkernel-check.sh --record ${K}
EOF
	exit 1
}
want_sha="$(awk '$1=="sha256"{print $2}' "${REC}")"
want_prod="$(sed -n 's/^producer //p' "${REC}")"
got_sha="$(sha256sum "${K}" | cut -d" " -f1)"
if [ "${got_sha}" != "${want_sha}" ]; then
	got_prod="$(producer "${K}" || echo '(no Linux banner — not a zImage?)')"
	cat >&2 <<EOF
refkernel: the reference kernel at
    ${K}
  is NOT the one this gate recorded. That path is the engine's deploy slot, so a product build has very
  likely overwritten it.
    recorded: ${want_prod}
              ${want_sha}
    actual:   ${got_prod}
              ${got_sha}
  Either restore the recorded kernel, or — if replacing it is what you meant — adopt the new one on purpose:
    ${HERE}/refkernel-check.sh --record ${K}
EOF
	exit 1
fi
echo "${want_prod}"
