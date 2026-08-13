#!/usr/bin/env bash
# t113power.sh — robust remote power control for the T113-breakout on the MEGA4 hub.
#
# WHY THIS EXISTS: the board's USB-C power is on the UUGear MEGA4, and its uhubctl
# *location* path (e.g. 1-6.4.4) CHANGES whenever the hub is unplugged/replugged or
# the NUC re-enumerates USB. Addressing by location broke a whole session once. This
# script addresses the hub by its stable **vendor id (2109 = VIA Labs / MEGA4)** via
# `uhubctl -n`, and always uses the fixed physical **port 3** the board lives on — so
# it keeps working across replugs (as long as the board stays in MEGA4 port 3).
#
# It ALSO verifies a cut actually happened the ONLY reliable way — the FEL USB device
# (1f3a:efe8) leaving/returning in lsusb — NOT the board LED (the 3.3V-rail bulk caps
# hold the LED lit for seconds after VBUS drops, so the LED lies) and NOT uhubctl's
# own "off" readout (that only means the command was accepted). See projects/gameboy-v3/README.md (Remote rig section).
#
# Runs ON the NUC. From the dev host:  ssh johmagnu-nuc 'bash -s' < tools/t113power.sh <cmd>
# Or copy it to the NUC and run directly.
#
# Usage:
#   t113power.sh off          # cut VBUS (leave off)
#   t113power.sh on           # restore VBUS
#   t113power.sh cycle [secs] # off, wait secs (default 3), on
#   t113power.sh reset        # alias for: cycle 3  (the normal "reboot the board")
#   t113power.sh status       # show port state + whether FEL device is present
#   t113power.sh felwait [s]  # power-cycle then wait up to s (default 20) for FEL to appear
#
# Env overrides (rarely needed):
#   HUB_VID=2109   PORT=3   FEL_ID=1f3a:efe8

set -uo pipefail

HUB_VID="${HUB_VID:-2109}"     # MEGA4 = VIA Labs 2109 (stable across replugs)
PORT="${PORT:-3}"             # physical MEGA4 port the T113 USB-C is in
FEL_ID="${FEL_ID:-1f3a:efe8}" # Allwinner sunxi FEL device

# uhubctl needs root; use sudo -n if not already root.
UHUBCTL="uhubctl"; [ "$(id -u)" -eq 0 ] || UHUBCTL="sudo -n uhubctl"

log()  { printf '\033[1;36m[t113power]\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[t113power]\033[0m %s\n' "$*"; }
err()  { printf '\033[1;31m[t113power] ERROR:\033[0m %s\n' "$*" >&2; }

# Is the board's FEL device currently on the bus? (the real "is it powered + in FEL?")
fel_present() { lsusb -d "$FEL_ID" >/dev/null 2>&1; }

# Confirm uhubctl can actually see the MEGA4 by vendor id (fails loudly if the hub
# vanished / dropped to a non-switching mode, instead of silently no-op'ing).
require_hub() {
  if ! $UHUBCTL -n "$HUB_VID" 2>/dev/null | grep -q "status for hub"; then
    err "no uhubctl hub with vendor id '$HUB_VID' found."
    err "  is the MEGA4 plugged in? check: sudo uhubctl | grep -i $HUB_VID"
    exit 2
  fi
}

do_off() { require_hub; $UHUBCTL -n "$HUB_VID" -p "$PORT" -a off >/dev/null 2>&1; }
do_on()  { require_hub; $UHUBCTL -n "$HUB_VID" -p "$PORT" -a on  >/dev/null 2>&1; }

cmd_status() {
  require_hub
  log "MEGA4 (vid $HUB_VID) port $PORT status:"
  $UHUBCTL -n "$HUB_VID" 2>/dev/null | grep -iE "status for hub|Port ${PORT}:"
  if fel_present; then log "FEL device $FEL_ID: PRESENT (board powered + in FEL)"
  else                 log "FEL device $FEL_ID: absent (board off, or booted-not-in-FEL)"; fi
}

cmd_off() {
  local was=$(fel_present && echo yes || echo no)
  do_off
  sleep 3
  if [ "$was" = yes ]; then
    if fel_present; then
      warn "commanded OFF but FEL device still present — VBUS may NOT have dropped."
      warn "  (do NOT trust the board LED; caps hold it lit. check: sudo uhubctl -n $HUB_VID)"
      return 1
    fi
    log "OFF confirmed — FEL device gone (VBUS really cut)."
  else
    log "OFF sent (board was not in FEL, so can't confirm via FEL device; port set off)."
  fi
}

cmd_on() {
  do_on
  log "ON sent; waiting for the board to re-enter FEL ..."
  for _ in $(seq 1 20); do fel_present && { log "FEL device back — board powered."; return 0; }; sleep 1; done
  warn "FEL device not seen within 20s. Board may be booting (not FEL), or check the hub."
  return 1
}

cmd_cycle() {
  local secs="${1:-3}"
  log "power-cycle: OFF ${secs}s -> ON  (MEGA4 vid $HUB_VID port $PORT)"
  do_off; sleep "$secs"; do_on
  log "cycle issued."
}

cmd_felwait() {
  local timeout="${1:-20}"
  cmd_cycle 3
  log "waiting up to ${timeout}s for FEL ..."
  for _ in $(seq 1 "$timeout"); do
    fel_present && { log "FEL UP."; xfel version 2>/dev/null | head -1; return 0; }
    sleep 1
  done
  err "FEL did not appear within ${timeout}s."
  err "  SD removed + NOR offset0 blank? try again, or verify hub: sudo uhubctl -n $HUB_VID"
  return 1
}

case "${1:-}" in
  off)     cmd_off ;;
  on)      cmd_on ;;
  cycle)   cmd_cycle "${2:-3}" ;;
  reset)   cmd_cycle 3 ;;
  status)  cmd_status ;;
  felwait) cmd_felwait "${2:-20}" ;;
  *) echo "usage: $0 {off|on|cycle [secs]|reset|status|felwait [secs]}" >&2; exit 1 ;;
esac
