# T113-S3 Breakout — Rev-B Fixes & Debug Notes

Hardware issues found during Rev-A bring-up, the fixes for the next spin, and the
debug history that established them. **Read the status + Rev-B fix list first;**
the theory/bodge history at the bottom is past-tense background.

---

## Current status (2026-07) — the board WORKS

**The Rev-A board boots mainline Linux to an interactive BusyBox shell** over the
UART0 console, dual-core (both Cortex-A7 up), 128 MiB DRAM, from a microSD.
Verified end-to-end: `BROM → U-Boot SPL → U-Boot → kernel → initramfs → /bin/sh`,
with `/proc /sys /dev` mounted and `/dev/mmcblk0p1` visible.

Getting there required **one hardware bodge (a VBUS bulk cap)** plus several
software fixes (device-tree + boot-script; tracked in the `gameboy-v3` project,
not here). The earlier "USB/FEL flashing is blocked / never reaches high-speed"
symptoms all traced back to **one root cause: a missing VBUS bulk input
capacitor** (see Root Cause below). Once a bulk cap is added, the board is stable.

**What is NOT yet done / open:**
- Rev-B respin has not been fabricated — the fixes below are the shopping list.
- The custom-bootloader path (`bootloader/`, replacing U-Boot) has not yet booted
  on silicon (first attempt was a bring-up-in-progress; unrelated to Rev-A HW).
- The USB PHY never negotiated **high-speed** (always full-speed / 12 Mbps).
  This did **not** block booting (full-speed works fine for FEL and everything
  else), so it's a low-priority quality item, not a blocker. Fix 3 (impedance)
  is the likely lever if you want HS.

---

## Rev-B hardware fix list (do these on the next spin)

| # | Fix | Priority | Status |
|---|-----|----------|--------|
| 1 | **VBUS bulk input capacitor** on +5V near the buck VINs | **REQUIRED** | ✅ root cause confirmed on silicon |
| 2 | **UART0 TX/RX series resistors** (back-power protection) | **REQUIRED** | ✅ confirmed (killed Rev-A board #1) |
| 3 | **Retarget USB D+/D- to 90 Ω** differential | recommended | likely-contributor (never confirmed sole) |
| 4 | **Wire the microSD card-detect** (or keep the `broken-cd` SW workaround) | optional | ✅ confirmed; SW-worked-around today |

Details for each below.

---

### Fix 1 — VBUS bulk input capacitor (REQUIRED) ⭐ root cause of the instability

**Problem.** The `+5V` / VBUS rail (USB-C in) feeds **both** buck converters
(U2 = 3.3 V, U7 = 0.9 V core) but has **only C3 + C5 = 2×100 nF** on it and **no
bulk reservoir**. The MangoPi reference and the FP6161 datasheet both call for a
~10 µF input cap; Rev-A omitted it. Under any **sustained current draw** (USB
bulk transfer, SD kernel read, kernel decompression) the rail sags, a buck
droops, and the SoC **browns out / resets**. This produced every one of the
early mystery symptoms: FEL bulk-transfer drops, SD-read reset loops, and a
kernel that reached "Starting kernel" then died silently during decompression.

**Confirmed on silicon (2026-07):** tacking bulk capacitance across +5V→GND took
FEL from **0 consecutive `xfel write32` before a drop → many hundreds, stable**,
and was ultimately what let the kernel survive decompression and reach a shell.

**Fix (Rev-B).** Add a **bulk input capacitor on +5V**, in parallel with the
existing 100 nF CIN, close to the buck VIN pins (U2-4 / U7-4):
- **Minimum 10 µF; 22–47 µF X5R/X7R ceramic recommended** (low ESR beats a small
  electrolytic here). A **10 µF ceramic ∥ a larger (47–100 µF) bulk cap** is the
  belt-and-suspenders choice: ceramic for the fast transient, bulk for sustained
  reads.
- **Sizing note from the bench:** a **10 µF via breadboard jumper wires was NOT
  enough** for large sustained transfers (a 490 KB U-Boot FEL load still browned
  out); a bigger cap with **short/soldered leads** was needed. On Rev-B use a
  proper bulk cap with a tight layout — don't under-spec it to 10 µF.

---

### Fix 2 — UART0 TX/RX series resistors (REQUIRED) — back-power protection

**Problem.** UART0 runs straight from the SoC pins to the header with no
protection:

```
UART0_TX : U1 pin 35 (PE2) ───────────── H11-18
UART0_RX : U1 pin 33 (PE3) ───────────── H10-18
```

If a **powered** USB-serial adapter drives its TX into the board's RX pin (PE3)
while the **board's 3.3 V rail is off**, current flows backward through the SoC
pin's ESD diode into the 3.3 V rail — **back-powering the whole chip through one
GPIO**. On a multi-rail SoC (separate 3.3 V I/O and 0.9 V core) this creates a
cross-domain imbalance that can trigger **latch-up** and permanently damage the
die. **This is the confirmed cause of death of Rev-A board #1** — its power LED
lit from the UART adapter alone, USB-C unplugged (the tell-tale of back-powering).

(Why the STM32F411 boards never hit this: single external rail with an on-die
core LDO, a rated ±5 mA pin current-injection tolerance, high latch-up immunity,
and Nucleo wiring that shares the adapter+target supply. The T113 has none of
those protections.)

**Fix (Rev-B).** A **series resistor on each UART0 line** at the header:
- `UART0_TX` (PE2 → H11-18): **330 Ω** (100 Ω–1 kΩ acceptable).
- `UART0_RX` (PE3 → H10-18): **330 Ω**.

The series R caps any back-injection through the ESD diode to a safe ~10 mA and
is negligible for 115200-baud. Optionally add a small TVS/ESD clamp to GND on
each line for hot-plug robustness. Two extra 0402s near H10-18/H11-18.

**Bench workaround used on Rev-A:** a **1 kΩ series R on the TX line** (on a
breadboard) let the console TX stay connected safely; RX-only (adapter-RX ←
board-TX + GND, TX disconnected) is also safe because an adapter RX input sources
no current. Rule of thumb: never let the adapter drive TX into an unpowered board.

---

### Fix 3 — USB2 D+/D- differential impedance (recommended)

The pair likely runs **below the 90 Ω target and loosely coupled**, which is
*consistent with* the board never negotiating USB high-speed (it fell back to
full-speed). Not proven to be the sole cause and it did **not** block booting, so
this is a quality/robustness fix, not a blocker.

**Measured Rev-A geometry (from `t113-breakout.kicad_pcb`):**

| Property | Rev-A value | Assessment |
|---|---|---|
| Intra-pair skew | **41 µm** | ✅ excellent (spec < 1500 µm) |
| Vias on D+/D- | **0** (all on F.Cu top) | ✅ ideal, no discontinuities |
| Reference plane | **In1.Cu = solid GND, 0.1 mm below top** | ✅ textbook |
| Trace width | 0.20 mm, uniform | see below |
| Pair center-to-center | ~0.586 mm (≈0.39 mm gap) | loosely coupled |
| Length | ~19 mm (DP 19.248 / DN 19.207) | ✅ short |
| Stackup | 4-layer 1.6 mm; F.Cu / 0.1mm / In1(GND) / 1.24mm / In2(+3.3V) / 0.1mm / B.Cu | ✅ |

**Fix (Rev-B) — retarget to 90 Ω differential,** best options first:
1. **Tighten the pair** — reduce the D+/D- gap so it's strongly coupled (aim
   edge-to-edge ≈ trace width); stronger odd-mode coupling raises Zdiff toward 90 Ω.
2. Re-solve width/gap for 90 Ω over the 0.1 mm prepreg to In1.Cu (field solver or
   JLCPCB's impedance calculator for this exact stackup; may want ~0.13–0.15 mm
   width with a tight gap).
3. Keep it short + via-free (already good); keep the GND plane solid and unbroken
   under the whole pair.
4. **Crosstalk (minor):** the **+0.9 V core switching rail runs ~0.76 mm** from
   the pair on F.Cu — move it away or ensure the In1 GND plane fully shields it.

---

### Fix 4 — microSD card-detect (optional; software-worked-around today)

**Problem.** The SD socket's card-detect (CD, socket pin 9) is **unconnected** on
Rev-A (BOM: *"card-detect available → wire to a spare GPIO if wanted"* — it wasn't).
But the reused MangoPi MQ-R device tree declares SD card-detect on
`cd-gpios = <&pio 5 6>` = **PF6**, which on this board is a plain broken-out GPIO
(H10-13). So U-Boot/Linux read PF6, see it high, and declare **"no card present"**,
refusing to scan mmc0 even with a card seated.

**Confirmed on silicon:** the BROM boots from SD fine (it ignores the DT/CD), but
U-Boot *proper* reported `Card did not respond to voltage select! : -110` /
`MMC: no card present`. First full Linux boot only worked after the SW workaround.

**Software workaround (in the `gameboy-v3` repo, currently in use):**
`scripts/uart0-console.dtsi` adds `&mmc0 { /delete-property/ cd-gpios; broken-cd; };`
so the MMC driver ignores CD and always assumes a card is present. Shared by the
U-Boot control DTB and the kernel DTB.

**Rev-B options (pick one):**
1. **Wire socket CD (pin 9) → PF6** with a 3V3 pull-up — matches the upstream DT,
   enables real hotplug/insert events, lets you drop `broken-cd`. Preferred for a
   stock-clean DT.
2. **Leave CD unwired, keep `broken-cd`.** Zero cost; the slot just can't report
   insert/removal (fine for a boot medium present at power-on). Current state.

Not a defect — a deliberate wiring choice the reused DT disagrees with.

---
---

# Debug history / appendix (how the above was established)

Everything below is the past-tense investigation that led to the fixes. Kept for
reference — the conclusions are already folded into the fix list above.

## The original symptom (before root cause was known)

FEL/USB flashing was blocked: the SoC powered up correctly (all rails good, LED
on, RESET released) and entered USB **FEL** (`1f3a:efe8`), but **would not sustain
a USB bulk transfer** — the device dropped off the bus mid-transfer and
re-enumerated. **Idle-stable** (20 s+ enumerated, no disconnects) but **failed
under load**; drop intervals were irregular (1.4 s–123 s), tracking activity not a
clock. Always full-speed (12 Mbps), never high-speed. `strace`: bulk URB submitted
OK, first completion reaped, next completion never arrives (`EAGAIN`) — device
vanishes mid-transfer. Two Rev-A boards affected (one wouldn't enumerate at all —
that one was later found to be the back-power / latch-up death, Fix 2).

The "stable at idle, resets under load" signature was the key clue → something
that only manifests when current draw rises → **power delivery (Fix 1)**.

## Candidate theories that were considered

- **A — VBUS brownout, missing bulk cap.** ✅ **CONFIRMED (this was it).** +5V had
  only 2×100 nF, no bulk. See Fix 1.
- **B — 0.9 V core buck transient response too slow.** Plausible secondary; adding
  VBUS bulk (A) resolved the symptoms, so not separately pursued. If Rev-B still
  dips the core under load, increase U7 COUT.
- **C — Host-port current limit / inrush trip.** Partially in play (no bulk cap →
  inrush); mitigated by the same bulk-cap fix.
- **D — USB PHY analog supply marginal.** Not needed to explain the data once A
  was confirmed.
- **E — D+/D- impedance mismatch.** Real but minor; see Fix 3. Consistent with the
  never-high-speed observation; did not block booting.
- **F — 24 MHz clock instability / G — thermal.** Ruled unlikely; never implicated.

## Oscilloscope probe points (if a rail-dip needs re-confirming)

A multimeter **cannot** catch these transients (it time-averages → false "3.3 V
looks fine"). Use a scope: 10× probe, DC coupling, GND clip to **H12-4**; set a
**falling-edge trigger just below the nominal rail** in Normal/single-shot so it
only fires on a dip. Generate load with a continuous `xfel` write loop.

| # | Probe | Point | Trigger | Proves |
|---|---|---|---|---|
| P1 | +5V / VBUS | H11-1 (or C3/C5, U2-4/U7-4 VIN) | falling < 4.5 V | VBUS sag/inrush (A/C) |
| P2 | +3.3V | H10-1 / H13-1 (L1-2 out) | falling < 3.0 V | 3.3 V dip (A/B/D) |
| P3 | +0.9V core | H12-1 (L2-2 out) | falling < 0.8 V | core brownout (B) — most likely to reset SoC |
| P4 | RESET | SW1 leg / R14 top (U1-27) | falling < 2.0 V | confirms a real chip reset vs. PHY hiccup |
| P5 | DXOUT 24 MHz | U1 pin 22 (probe *near* xtal, not DXIN) | — | clock keeps oscillating? (F) |
| P6 | D+/D- | USB-C or U1 115/114 (needs diff probe) | eye | SI of the pair (E) |

Reading it: any of P1–P3 dips at the disconnect → brownout (which rail localizes
it); P4 low at the drop → real reset; all rails flat + RESET high → not power (SI
/ protocol). This was the plan; in practice the V1 bodge (below) confirmed A
directly without needing the scope.

## Bodge experiments run on the Rev-A board (no respin)

- **V1 — bulk cap on VBUS → CONFIRMED Fix 1.** Tacked bulk capacitance across
  +5V→GND near the buck inputs. FEL small-write survival went **0 → many**; a
  bigger, short-lead cap was needed for large sustained transfers (jumpered 10 µF
  alone was insufficient for a 490 KB load). This is the whole ballgame — do Fix 1.
- **V4 — external 5 V** into +5V (H11-1), USB for data only: tests host-port
  current-limit (Theory C). Not needed once V1 worked, but a clean confirmation
  if desired.
- **V5 — slow the transfer:** `xfel` bulk chunk was reduced 128 KB → 4 KB (+ retry)
  in `fel.c` on the NUC to reduce the current step; helped small transfers, but
  the cap (V1) was the real fix.
- **V6 — reflow USB-C + U1 pins 114/115:** if a board still won't reach high-speed
  after Fix 1+3, suspect a cold joint on the USB-C D+/D-/GND or SoC USB pins.
- **V7 — different host/cable:** the cable was cleared (an iCEBreaker HS-enumerated
  on it); the NUC's own USB was also flaky, so a different host removes that
  variable.
- **V8 — third-board A/B:** swapping boards distinguished per-unit assembly defects
  from design issues.

## Related software fixes (tracked in the gameboy-v3 project, not a HW respin)

These were also needed to reach a shell and are noted here only so a HW reader
isn't surprised by them — they are **not** Rev-B board changes:
- **DT `broken-cd`** for the unwired card-detect (Fix 4 above).
- **boot.cmd `fdt set /chosen bootargs`** — the DTB shipped a placeholder bootargs
  that silenced the kernel (no `console=`); the loader now sets it explicitly.
  The custom bootloader's `fdt.c` was also reworked to grow/insert bootargs so the
  placeholder could be removed entirely.
- **bare `earlycon`** (not `earlycon=on`, which is a silent no-op).
- **Clean BusyBox rebuild** — a stale initramfs had a corrupt applet table
  (`mount` ran `more`'s code); `03-rootfs.sh --clean` regenerates it.

_Last updated: 2026-07 bring-up session. Root cause (missing VBUS bulk cap)
confirmed on silicon; board boots Linux to a shell with the bodge in place._
