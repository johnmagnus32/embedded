#!/usr/bin/env bash
# flash.sh — flash a forge-built BUNDLE to the T113-breakout and FEL-boot it, in one go.
#
#   tools/flash.sh <bundle-dir> <media>   (or: make flash  from the product dir)
#     <bundle-dir>  a directory produced by `make image` (forge) (has manifest.env + components)
#     <media>       nor            (SD/eMMC: see the clear error below)
#
# It runs the whole remote loop against the rig (the NUC that has the board + xfel):
#   1. scp the bundle to the NUC
#   2. power-cycle the board into FEL (t113power.sh, replug-proof VID addressing)
#   3. xfel spinor write the components + the GV3NOR1 table to NOR
#   4. FEL-deliver the loader (custom bootloader, OR U-Boot proper driven over UART)
#   5. tail the UART console so you see it boot
# NOR offset 0 stays a non-eGON table -> BROM always falls back to FEL (un-strandable).
#
# Rig config (env, sensible defaults for this setup):
#   RIG_HOST=johmagnu-nuc      ssh host with the board + xfel + t113power.sh/uartcap.sh
#   RIG_DIR=~/t113boot/flash   where the bundle is staged on the NUC
#   RIG_TTY=/dev/ttyUSB1       the board's UART console on the NUC
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

log()  { printf '\033[1;35m[flash]\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31m[flash] ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

BUNDLE="${1:-}"; MEDIA="${2:-}"
[ -n "$BUNDLE" ] && [ -n "$MEDIA" ] || die "usage: $0 <bundle-dir> <media:nor>"
[ -d "$BUNDLE" ] || die "bundle dir not found: $BUNDLE"
[ -f "$BUNDLE/manifest.env" ] || die "no manifest.env in $BUNDLE — is it a forge bundle?"

# Media gate — be explicit about what is/isn't remotely flashable.
case "$MEDIA" in
  nor) ;;
  sd)   die "MEDIA=sd is not remotely flashable: xfel can only write SPI-NOR/NAND, not SD.
       Use MEDIA=nor for the FEL loop, or build an SD .img (make image MEDIA=sd) and dd it to a card by hand." ;;
  emmc) die "MEDIA=emmc: this board has no eMMC (BOM = SPI-NOR + microSD). Use nor." ;;
  *)    die "MEDIA must be 'nor' (got '$MEDIA')" ;;
esac

# shellcheck source=/dev/null
. "$BUNDLE/manifest.env"

RIG_HOST="${RIG_HOST:-johmagnu-nuc}"
RIG_DIR="${RIG_DIR:-t113boot/flash}"     # relative to the NUC home (~ expands remotely)
RIG_TTY="${RIG_TTY:-/dev/ttyUSB1}"
SSH="ssh -o BatchMode=yes ${RIG_HOST}"

log "config: $BUNDLE_CFG  (bootloader=$BOOTLOADER kernel=$KERNEL rootfs=$ROOTFS) -> media=$MEDIA"

# --- 1. stage the bundle to the NUC -----------------------------------------
log "staging bundle -> ${RIG_HOST}:${RIG_DIR}/"
$SSH "mkdir -p ${RIG_DIR}" || die "cannot mkdir on rig (ssh ${RIG_HOST} reachable?)"
scp -o BatchMode=yes -q \
  "$BUNDLE/manifest.env" \
  "$BUNDLE/$KERNEL_FILE" "$BUNDLE/$DTB_FILE" "$BUNDLE/$INITRD_FILE" "$BUNDLE/$LOADER_FILE" \
  "${RIG_HOST}:${RIG_DIR}/" || die "scp of bundle failed"

# helper: does the rig have t113power.sh / uartcap.sh? (staged earlier this project)
POWER="~/t113boot/matrix/t113power.sh"
UARTCAP="~/t113boot/matrix/uartcap.sh"

# --- 2..5. drive the flash + FEL boot on the NUC ----------------------------
# All the on-NUC logic in one remote script (it has the board, xfel, the tools).
log "flashing NOR + FEL-booting on ${RIG_HOST} ..."
$SSH "MEDIA='$MEDIA' RIG_DIR='$RIG_DIR' RIG_TTY='$RIG_TTY' POWER='$POWER' UARTCAP='$UARTCAP' bash -s" <<'REMOTE'
set -uo pipefail
cd "${RIG_DIR/#\~/$HOME}" 2>/dev/null || cd "$HOME/$RIG_DIR" || exit 3
. ./manifest.env
XFEL="xfel"; [ "$(id -u)" -eq 0 ] || XFEL="sudo -n xfel"
POWER_EX=$(eval echo "$POWER"); UARTCAP_EX=$(eval echo "$UARTCAP")
rlog(){ printf '  \033[1;36m[rig]\033[0m %s\n' "$*"; }
rdie(){ printf '  \033[1;31m[rig] ERR:\033[0m %s\n' "$*" >&2; exit 4; }

# 2. FEL
rlog "power-cycling into FEL ..."
if [ -x "$POWER_EX" ] || eval "[ -f $POWER ]"; then
  eval "$POWER felwait 30" >/dev/null 2>&1 || true
fi
ok=0; for i in $(seq 1 25); do $XFEL version >/dev/null 2>&1 && { ok=1; break; }; sleep 1; done
[ $ok = 1 ] || rdie "no FEL device (SD out + NOR offset0 blank?). Try: $POWER felwait"
rlog "FEL up: $($XFEL version 2>&1 | head -1)"

# 3. write NOR: kernel, dtb, initramfs, then the GV3NOR1 table (built from real sizes)
rlog "writing NOR: kernel@$NOR_KERNEL_OFF ($(du -h $KERNEL_FILE|cut -f1)) ..."
$XFEL spinor write "$NOR_KERNEL_OFF" "$KERNEL_FILE" >/dev/null 2>&1 || rdie "kernel NOR write failed"
rlog "writing NOR: dtb@$NOR_DTB_OFF ..."
$XFEL spinor write "$NOR_DTB_OFF" "$DTB_FILE" >/dev/null 2>&1 || rdie "dtb NOR write failed"
rlog "writing NOR: initramfs@$NOR_INITRD_OFF ($(du -h $INITRD_FILE|cut -f1)) ..."
$XFEL spinor write "$NOR_INITRD_OFF" "$INITRD_FILE" >/dev/null 2>&1 || rdie "initramfs NOR write failed"
python3 - "$KERNEL_FILE" "$DTB_FILE" "$INITRD_FILE" table.bin \
          "$NOR_KERNEL_OFF" "$NOR_DTB_OFF" "$NOR_INITRD_OFF" <<'PY'
import sys,os,struct,zlib
k,d,i,out,ko,do,io=sys.argv[1:8]
ko,do,io=int(ko,16),int(do,16),int(io,16)
body=b"GV3NOR1\x00"+struct.pack("<8I",1,0,ko,os.path.getsize(k),do,os.path.getsize(d),io,os.path.getsize(i))
open(out,"wb").write(body+struct.pack("<I",zlib.crc32(body)&0xffffffff))
PY
$XFEL spinor write "$NOR_TABLE_OFF" table.bin >/dev/null 2>&1 || rdie "GV3NOR1 table write failed"
rlog "NOR flashed (offset 0 = GV3NOR1 table, NOT an eGON -> FEL fallback intact)."

# 4. start console capture, then FEL-deliver the loader
[ -f "$UARTCAP_EX" ] && eval "$UARTCAP start $RIG_TTY" >/dev/null 2>&1 || {
  sudo -n stty -F "$RIG_TTY" 115200 raw -echo 2>/dev/null
  sudo -n pkill -9 -f "cat $RIG_TTY" 2>/dev/null; sleep 0.3
  sudo -n bash -c "nohup cat $RIG_TTY > /tmp/uart.log 2>/dev/null & echo \$! > /tmp/uart.pid"
}
uart_bytes(){ ( [ -f "$UARTCAP_EX" ] && eval "$UARTCAP dump" || sudo -n cat /tmp/uart.log ) 2>/dev/null | wc -c; }
if [ "$BOOTLOADER" = custom ]; then
  rlog "FEL-loading the custom bootloader @0x28000 (it does DRAM init + NOR read)"
  # The FEL exec can silently no-op on this rig (flaky window); retry until the
  # console actually produces output, re-arming FEL between tries.
  booted=0
  for try in 1 2 3; do
    $XFEL write 0x28000 "$LOADER_FILE" >/dev/null 2>&1 || rdie "loader write failed"
    $XFEL exec 0x28000 >/dev/null 2>&1 || true
    sleep 5
    [ "$(uart_bytes)" -gt 200 ] && { booted=1; break; }
    rlog "no console output (attempt $try) — re-arming FEL + retrying"
    eval "$POWER felwait 25" >/dev/null 2>&1 || true
    $XFEL version >/dev/null 2>&1 || rdie "lost FEL during retry"
    [ -f "$UARTCAP_EX" ] && eval "$UARTCAP start $RIG_TTY" >/dev/null 2>&1
  done
  [ $booted = 1 ] || rlog "warning: no console output after 3 tries (flaky FEL exec?); check UART below"
else
  # xfel QUIRK: after `spinor write` ops (the NOR flash above), a subsequent
  # `xfel ddr` + high-DRAM write (0x42e00000) FAILS (rc=255) even though ddr itself
  # returns 0 — the spinor activity leaves the FEL/DRAM path in a bad state. A fresh
  # FEL re-arm (power-cycle) clears it. (The custom loader avoids this: it goes to
  # SRAM 0x28000 with no ddr.) So: re-enter FEL cleanly before delivering U-Boot.
  rlog "re-arming FEL (post-NOR-write) before the U-Boot DRAM load ..."
  eval "$POWER felwait 30" >/dev/null 2>&1 || true
  $XFEL version >/dev/null 2>&1 || rdie "no FEL after re-arm"
  [ -f "$UARTCAP_EX" ] && eval "$UARTCAP start $RIG_TTY" >/dev/null 2>&1   # capture survives the cycle
  rlog "FEL-loading U-Boot proper @0x42e00000 + driving sf read -> bootz"
  $XFEL ddr t113-s3 >/dev/null 2>&1 || rdie "xfel ddr failed"
  $XFEL write 0x42e00000 "$LOADER_FILE" >/dev/null 2>&1 || rdie "u-boot write failed"
  $XFEL exec 0x42e00000 >/dev/null 2>&1 || rdie "u-boot exec failed"
  sleep 4
  send(){ printf '%s\r\n' "$1" | sudo -n tee "$RIG_TTY" >/dev/null; sleep "${2:-1}"; }
  for _ in $(seq 1 12); do printf ' ' | sudo -n tee "$RIG_TTY" >/dev/null; sleep 0.1; done
  send '' 1; send 'sf probe' 3
  send "sf read $DRAM_KERNEL $NOR_KERNEL_OFF 0x556000" 25
  send "sf read $DRAM_DTB $NOR_DTB_OFF 0x8000" 3
  send "sf read $DRAM_INITRD $NOR_INITRD_OFF 0x$(printf %x "$INITRD_SIZE")" 6
  send "fdt addr $DRAM_DTB" 2; send 'fdt resize 0x1000' 2
  send "fdt set /chosen bootargs \"console=$KERNEL_CONSOLE earlycon panic=10 initrd=$DRAM_INITRD,$INITRD_SIZE\"" 2
  send "bootz $DRAM_KERNEL - $DRAM_DTB" 6
fi
rlog "boot issued — capturing console (giving it time: gunzip+probe can take ~15s)."
sleep 18
echo "==== UART (last 30 lines) ===="
( [ -f "$UARTCAP_EX" ] && eval "$UARTCAP dump" || sudo -n cat /tmp/uart.log ) 2>/dev/null | tail -30
REMOTE
rc=$?
[ $rc -eq 0 ] || die "remote flash/boot failed (rc=$rc)"
log "DONE — flashed $BUNDLE_CFG to NOR and FEL-booted. See the UART capture above."
