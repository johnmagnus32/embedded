# gb3 routing plan

How the board gets routed, in order, and the rules every route group follows. The reserved corridors and via spots are
in `channels.py` (drawn with `route/plot.py ... --channels`); U1's per-pin escape is in `mcu_escape.py` (`--escape`).

Board state this plan was measured on: d6a3177 (step-2 placement), no copper. DRC 5 errors, all pre-existing (U18
pad 14 in its antenna keep-out, 4 inside USBC1's own footprint), 399 unconnected, schematic parity 0.

The plan assumes the proposals under "Decisions": netlist A1-A8 and O1, placement P1-P4, mounting holes M1, DRU
D2-D8, zones Z1-Z4. `mcu_escape.ESCAPE_ALT` and the notes below say what changes if one is turned down. Everything
here was checked on ONE combined scratch board (`~/.cache/gb3-step3/fix/trial/s5`: every drafted group and every trial
together); see "Status". Revised 2026-10-14 after the first review (its items 1-6; the "fix" pass).

**Applied 2026-10-14 (step 3, user-approved):** netlist A1-A8 + O1 (A5 in the second review's port-0 order, see the
table), placement P1-P4 (`placement/step3.csv`), the return caps C95/C96 (the trial's C_RET1/2), the mounting holes H1-H5
(the first H1 at (102, 36) dropped), the keep-outs and "pins:" areas (`route/footprint_keepouts.py`), the jumper pad groups
(`jumper_groups.py`) and the DRU. Groups 05/10/40 are drafted here but not built yet (step 4). The second review's
items for step 4 are listed under "Second review" at the end.

## Stackup and layer rules

JLC04161H-7628, already set in the board file's stackup: F.Cu 35 µm / 0.2104 mm 7628 (εr 4.4) / In1 15.2 µm /
1.065 mm core (εr 4.6) / In2 15.2 µm / 0.2104 mm 7628 / B.Cu 35 µm, LPI mask 15 µm (εr 3.8). Order this stackup.

| Layer | Role |
|---|---|
| F.Cu | P1, CN1, Audio1, the 15 buttons, the two status LEDs and U1's decap island (x 57.45–72.98, y 67.05–86.11). Under the panel it is otherwise empty, so it is the long-haul layer: the display F bundle, the USB pair, the F highways H1/M1/S2/T0, the top-strip highway T1 and BAT/+5V along the strip's north edge |
| In1.Cu | Solid GND, the reference for every F.Cu signal (0.21 mm below it). Only via antipads cut it |
| In2.Cu | +3V3 plane, the reference for every B.Cu signal (0.21 mm above it). GND islands under the three back-side switch nodes, X1 and (optionally) USB-C/D4; a +0V9 island under U1's body |
| B.Cu | U1 and 155 of the 209 parts. U1's escape band, the display rows, the power trunks, the short B fans (SD, NOR, HOSC, IMU), the B neck into the right grip, the short hops under F routes |

- **B.Cu signals return on +3V3, not GND.** In1 is 1.29 mm from B.Cu, In2 0.21 mm. The In1/In2 pair is 1.065 mm apart,
  so it couples only ε0·εr/d = 3.8 pF/cm²: a B.Cu signal's return current reaches GND through the nearest +3V3↔GND
  capacitor, not through the planes. So:
  - the long edge-rich runs (RGB, USB, I2S) are on F.Cu over In1, B.Cu only for escapes and hops. SD, NOR and HOSC are
    short B-only fans over In2 (SD 25.6–34.8 mm, NOR 11.8–21.9 mm, HOSC ≤ 5.1 mm) next to their own +3V3/GND decaps
    (C11/C40/C7, C3/C4) - not "long runs on F";
  - a cluster of fast F↔B vias has a +3V3/GND decap close by (see Grounding; the RGB field gets two new ones, P3).
- B.Cu signals never cross the edge of an In2 island. Only DC or slow lines may (SYS, +0V9, PWR_BTN_3V3, BL_CTRL,
  BL_LED_P/N, CC_SENSE at its via). Under U1's body the only B copper is the pin stubs to the island vias.
- F.Cu signals never cross an In1 void. Port gb2's `planecheck.py` to check every track's reference plane, including
  the antipad slots the RGB via field and the strip's via staircase make (Grounding). Measured exception: RGB_DCLK
  passes over the display staircase's antipads for 1.1 mm (review measurement).
- Copper stays ≥ 0.5 mm from the edge (DRU `min_copper_edge_clearance`). Default 0.15/0.15 mm tracks and 0.6/0.3
  vias (the board minimum is 0.5/0.3; RESET's U1.27 via uses it); netclass widths and DRU neck-downs as in
  `gb3.kicad_pro` / `gb3.kicad_dru`.
- **The USB L is a fence on F.** The pair's riser and band cut F from U1's north and west to the SD socket, the BT
  module and the upper left grip. Nothing crosses it on F; the crossings it forces are all on B: SD, UART3, RESET,
  PWR_BTN_3V3/PSHOLD/BL_CTRL and CC_SENSE's hop.

## Decisions (yours, before step 4 writes copper)

Drafts, nothing applied: `~/.cache/gb3-step3/fix/proposed/` (`nets_changes.csv`, `placement_step3.csv`,
`gb3.kicad_dru`, `bom_notes.csv`), `route/footprint_keepouts.py`, `route/groups/05_switcher_gnd.py`,
`route/groups/10_power.py` and the new parts C95/C96 (gb3-bom.csv, placed by `placement/step3.csv`) and the mounting hole H2 (`route/footprint_keepouts.py`).

**Netlist** (`nets_changes.csv`, rows replace the same refdes,pin; firmware/DT changes in brackets):

| # | Change | Evidence | Recommend |
|---|---|---|---|
| A1 | Plain PE13↔PE10 swap: U1.31 PE13 = PWR_BTN_3V3, U1.37 PE10 (PWM4) = BL_CTRL; U1.36 PE11 stays PSHOLD [power key on PE13 (EINT13); backlight PWM4 on PE10; the SPL/U-Boot hold pin does not change] | Today BL_CTRL's PWM runs up N4 0.56 mm from HOSC_IN (C3.1) and needs a via pair to cross PSHOLD/PWR_BTN. Swapped, on the combined board with U17's SYS feed drawn first (T3 → C68.1, group 10): RESET, PWR_BTN_3V3, PSHOLD, BL_CTRL all on B, 0 extra vias (maze legs 23.2 / 34.7 / 25.9 / 32.8 mm), BL_CTRL enters U6.2 from under the SOT-23 at x 103.85 (no via in the U6–L2 gap). The first draft's rotation (31 PSHOLD, 36 PWR_BTN_3V3) fails with the SYS feed on B in either order (review trials ne_sys_rot.py) | adopt |
| A2 | SD_CD PF6 (13) → PG10 (5) [cd-gpios <&pio 6 10>] | PG10: SD_CD joins the planar B fan. With A8 and P4, every ESD tap and pull-up routed: 0 vias, SD bus 25.57–29.34 mm, skew 3.77 mm (DRU 5), SD_CD 34.80 mm. PF6 has to cross all six SD lines (2-via F hop, ESCAPE_ALT) | adopt |
| A3 | JACK_DET PE12 (32) → PG14 (126) [hp-det-gpios <&pio 6 14>; the i2s1 pin group must not claim PG14] | Required by group 40: its RGB_R2 column (x 73.4) takes PE12's only F path. PG14: 47.8 mm / 2 vias, same pull class (T113 DS Table 5-4) | adopt |
| A4 | USBLC6 D4: I/O1 (1/6) = D−, I/O2 (3/4) = D+ | 3 D+/D− crossings receptacle→D4 plus 1 at U1 → 1 (the receptacle's own jumper), drawn DRC-clean | adopt |
| A5 | PCA9555 U16: port 0 (east pins 4–11) = right grip in the order the strip lanes reach it (U16.4–11 = R1, VOL+, VOL−, B, X, SELECT, Y, A; second review), port 1 (west 13–18) = left grip [keymap] | 416 → 356 mm, 20 → 14 vias | adopt |
| A6 | TPA6132A2 channel swap (north R/C chain carries left) | the DAC outputs nest with 0 vias (combined trial, audio core routed before the buttons) | adopt |
| A7 | PE0 (44) ↔ PE1 (45): U1.44 = IMU_INT2, U1.45 = IMU_INT1 [IMU interrupt specifiers] | with P2 the ribbon to U7 is planar: 0 vias, all 14 U7 pads and their passives routed | adopt, with P2 |
| A8 | SP3004 channel remap (new): D1.3 = SD_D0, D1.4 = SD_D1, D1.6 = SD_CD, D2.6 = SD_CMD, D2.4 = NC (D1.1 SD_CLK, D2.1 SD_D2, D2.3 SD_D3 unchanged) | the four I/O channels are identical (SP3004 C207280 p.1: I/O1 1, GND 2, I/O2 3, I/O3 4, VCC 5, I/O4 6). With today's pinout CD sits on D1's east column and SD_D1 on its west, so their taps cross; remapped, every tap is a stub off its own lane (0 vias). The remap alone (without P4) left 2 nets unrouted in the exploratory taps trial | adopt, with P4 |
| O1 | PB4 (84) ↔ PB6 (80): U1.80 = CHG_PGOOD, U1.84 = BT_EN_RST [gpio-charger, BT enable] | the S row splits planar: 79/80/82 east, 84–86 west; saves BT_EN_RST's via pair | optional |
| — | BT_EN_RST → PG5 (122) | no route: PG5 lands on the wrong side of U18's UART pads (swaps-zones) | reject |

**Placement** (`placement_step3.csv`, apply with `place_moves.py`; 16 rows):

| # | Move | Evidence | Recommend |
|---|---|---|---|
| P1 | R39 (DISP pull-up) → (90.60, 77.45) rot 0, B | its old spot is the backlight feed's lane north of P1 | required by group 40 |
| P2 | IMU cluster east of the RGB via field: U7 (96.60, 80.20) rot 90, R13 (95.85, 84.20) 270, R41 (97.50, 76.90) 270, R42 (100.30, 80.70) 0, C48 (94.40, 77.30) 90, C49 (94.40, 81.45) 270, all B | ribbon 0 vias (SDA 35.5, SCL 34.1, INT1 33.3, INT2 27.7 mm). Re-trialled with all 14 U7 pads, C48/C49/R13/R41/R42 and 9 plane vias (V14): C48 rot 90 gives its GND via a spot (95.25, 76.30) clear of RGB_DE's F diagonal; C49 0.25 mm south frees U7.6/U7.7's GND via (95.05, 79.85); CS (U7.12) reaches +3V3 at (98.00, 78.55) round the body, nothing under the LGA. U7 is not rotated | adopt |
| — | alternatives to P2 | keep U7: variant C (escape trial C) costs 8 vias | — |
| P3 | 2 × 100 nF +3V3/GND return-path caps C95 (90.6, 82.6) and C96 (84.6, 89.6), rot 90, B, BOM #19 (C14663), new parts | nearest +3V3/GND decap to the bus vias 5.1–17.2 → 4.0–8.4 mm | adopt |
| P4 | SD support row (new): D1 (51.1, 50.8) 0, D2 (58.3, 50.8) 0, R8 (49.75, 52.85) 90, R7 (52.0, 52.85) 90, R6 (55.35, 52.85) 90, R10 (59.35, 52.85) 270, R9 (62.0, 49.3) 0, C11 (54.2, 48.75) 180, C40 (61.8, 52.6) 270 | with A8 the seven lanes pass under D1/D2 and every tap / pull-up is a stub; the SP3004 VCC/GND and the pull-ups' +3V3 share one via row at y 50.85 (V13, reserved; H1's lanes turn east at y ≥ 51.5) | adopt, with A8 |
| M1 | M2 mounting holes (MountingHole_2.2mm_M2 NPTH, keep-out r 2.75 F+B, board-only footprints placed by `route/footprint_keepouts.py`): **H2 (180.5, 35.0)** placed. The first H1 (102.0, 36.0) dropped (user review: its M2 head would sit ~0.9 mm under the display module, y 37.0). Scheme (gb2 practice): screws clamp, shell fingers locate. **Five holes (user-approved 2026-10-14, after a free-area scan):** H1 (26.5, 58.0) left grip outer edge, 23.6 mm from the BT830 antenna (DS 12.1.1: metal < 20 mm degrades it); H2; H3 (204.0, 37.5) right grip top outer; H4 (208.0, 83.5) under the ABXY corner; H5 (26.25, 110.75) left grip bottom, 11 mm from the jack | screws only where no front-shell boss meets the display module, the antenna or the cell buffer; each grip clamped at its ends, the right grip (it hangs off the 20 mm strip above the cell notch) at three points | adopted (H1-H5) |

**DRU** (`proposed/gb3.kicad_dru`, a full replacement). Under the repo's rules the combined board fails exactly these:
8 diff_pair_gap, 5 HighCurrent width, 4 NOR skew, 1 USB skew, 1 USB via count, 1 USB uncoupled (plus the 4 USBC1).

| # | Rule | Why | Recommend |
|---|---|---|---|
| D1 | RGB skew stays 25 mm | measured 23.66 mm (B2 32.17 .. DE 55.83 mm); 35–40 mm only if the panel's colour map turns out swapped (the generator then gives 36.5 mm) | keep 25 (draft's 35 withdrawn) |
| D2 | RGB_DCLK via_count 2 → 0; "Pixel clock source stub": /soc/LCD0_CLK ≤ 4.5 mm, ≤ 1 via | locks R50 at the pin and DCLK on F (1.1 mm over antipads); the stub measures 3.77 mm | adopt |
| D3 | USB pair via_count 3 → 4 | measured: D− 4 (two layer changes + the receptacle jumper), D+ 2 | adopt |
| D4 | "TP11 isolation": TP11's pad ≥ 0.6 mm from everything | IPC-2221B Table 6-1 B2 (31–50 V) for the 37–39 V OVP | adopt |
| D5 | pours: thermal reliefs on 0402/0603 GND pads; solid for C2/C21; one spoke for C3, C5, C28, C69 | both ends of small passives reflow together | adopt |
| D6 | USB: in-pair skew 0.5 → 4 mm, diff_pair_uncoupled 12 → 16 mm, and "USB pair through USBC1 / D4 pins" (diff_pair_gap 0.18–2.0 mm inside the two pin fields) | measured with the D4 end drawn: skew 1.30 mm in KiCad's length (D− 115.97, D+ 114.68; the jumper's 2 vias are 3.17 mm), uncoupled 14.37 mm (the D4 split and the receptacle's interleave); 4 mm ≈ 25 ps vs the 2.08 ns UI | adopt |
| D7 | NOR QSPI skew 5 → 12 mm | measured 11.76 (CS) .. 21.94 mm (IO3), skew 10.19 mm: IO0 and IO3 hop with 2 vias each; 12 mm ≈ 75 ps | adopt |
| D8 | the HighCurrent neck-down rules also match the restriction-free "pins: <ref>" areas (`footprint_keepouts.py`) | the EasyEDA courtyards hug the bodies, so `intersectsCourtyard()` misses the pin tips (U15's up to 1.4 mm out): the Kelvin taps and pin stubs fail "HighCurrent width" (5 on the combined board) | adopt |

SD stays at 5 mm skew (measured 3.77 mm with every tap).

**Zones and rule areas** (Z1 in group 05, Z2 in group 20, Z3/Z4/M1/D8 areas in `route/footprint_keepouts.py`):

| # | Change | Evidence | Recommend |
|---|---|---|---|
| Z1 | In2 GND islands: U2 (39.6–49.3 × 69.3–79.55), U6 (101.4–111.0 × 49.4–63.2), U12 (136.2–146.6 × 37.3–46.6), X1 (66.2–71.4 × 62.6–68.6), optional USB-C/D4 (112–124 × 38–48) | TPS61165 p.21, FP6161 checklist #4/#5. Combined-board fill: 99.4 / 132.5 / 95.6 / 30.5 / 113.1 mm², one piece each, 17 / 13 / 3 / 0 (+2 in group 05) / 13 stitching vias outside the reserved areas; the +3V3 plane 10259.2 mm², one piece. X1's island now runs 0.21 mm past U1's pin tips: beyond the tips both HOSC lines are over the island at all 288 refcheck samples (centre and both edges, every 0.1 mm) | adopt; USB-C optional |
| Z2 | +0V9 In2 island under U1 (core 58.6–70.6 × 73.6–82.5 + fingers), LDOA by a B ring inside the via ring | fill 89.7 mm², one piece, all five +0V9 vias on it, the six U1 +3V3 vias on the plane | adopt |
| Z3 | U18 antenna keep-out: south edge 39.72 → 39.45, west edge to the board edge | BT830 p.25/p.27; DRC 5 → 4 errors | adopt |
| Z4 | keep-outs: X2 no pattern (F; SC-20S p.1), no tracks over X1 (F), U4 prohibition bars (B; A-MicroTF-1.85A p.2), SKRT lug gaps SW12–16 (F; C127481 p.2), the M1 hole keep-outs, the D8 "pins:" areas (no restrictions) | datasheets as cited | adopt |

**Docs / other:**
- PINOUT.md's pin table follows A1–A3, A7 and O1 (U1.5, 13, 31, 32, 37, 44, 45, 80, 84, 126; U1.36 unchanged); the
  gb3-bom.csv notes that name the old pins are in `proposed/bom_notes.csv` (R40 → PE13, R49 → PG14, R51 → PE10).
- **CN1 rating:** JST S2B-PH-SM4-TB (C295747.pdf p.1) 2 A per contact (AWG #24), contact resistance 10 mΩ initial /
  20 mΩ after the environmental tests. The cell's 1C is 2 A (LiIon2000mAh37V.pdf p.2). The 2.7 A coincident worst case
  (U12 1.87 + U6 0.33 + U9 0.5 A) exceeds both: BAT copper is sized for 2.7 A, but treat 2 A as the system's continuous
  limit (firmware: no full-brightness + max-volume + max +3V3 load at once) or move to a 3 A connector. **Decided
  2026-10-14: the firmware load cap** (on battery, powerd caps backlight and volume when the INA226 reads > 1.8 A).
- **TS-1187A (SW1, SW6–SW9, SW11):** pads 1/2 (and 3/4) are one contact inside the switch but separate pads to DRC.
  SW1 gets the copper tie (drawn); the button switches show "unconnected" pad pairs. Either tie them on F (6 mm under each
  switch) or renumber the footprint's pads 2/4 as 1/3 (a library edit). **Done in step 3:** board-level jumper pad
  groups (`jumper_groups.py`), so neither is needed.
- Panel colour map: confirmed, bench-verified 2026-08-23 (BOM-REVIEW H2); PINOUT.md and BOM.md §2 now say so. Group 40 routes as designed.
- Ask JLC for plugged/capped vias in U1's EP (16 open 0.3 mm holes under a 5.6 mm pad).

## Choke points

### 1. U1's fan-out ring

The eLQFP-128 has a 0.4 mm pitch and 0.20 × 1.46 mm pads, so the gap between pads is 0.20 mm: nothing passes between
pins, every signal leaves radially from its pad tip (tips at x 56.14 / 72.86, y 68.39 / 85.11, B.Cu). The band about
4 mm outside the tips, on B.Cu, is the only way out: In1/In2 are planes and F.Cu under U1 is the decap island.

- **Inward:** 28 step-2 `viafit2` pin → via → cap connections (0.6/0.3, stubs ≤ 3.15 mm; RESET's U1.27 via is 0.5/0.3
  at (69.40, 67.95), see Grounding; the 29th, U1.75, is group 40's), the 4 × 4 EP field at 1.1 mm, the C9/C10/C17 GND
  vias, R5.2's GND via (76.15, 76.05) and U1.91's AGND via (60.30, 85.85).
- **74 signal pins** (N 21, E 26, S 16, W 11 with the proposals). Eight leave through an inner via or a local stub; 66
  through the band.

| Side | Signals | Destinations (channel) |
|---|---|---|
| N | 21 | I2C1 ×2 to R30/R31, then F (W4 → H1). SD ×6 + SD_CD (A2) one fan to U4 (N1). NOR ×6 to U3 (N2, 2 via pairs at U3). HOSC ×2 to X1 (N3, 0 vias). RTC ×2 to X2 (inner vias, F). RESET (inner via → F → NE1). PWR_BTN_3V3 (A1) up N4 |
| E | 26 | UART0 ×2 (local). PSHOLD, BL_CTRL north (NE1). TOUCH ×2 → FPC1, EXP_INT → U16, SPK_EN → M1 (E1). I2C2 ×2 + IMU_INT ×2 → U7 (E1 → E2). DDR_ZQ (R5). RGB/DISP ×13 (D1) |
| S | 16 | RGB ×8 + DE (D1/D2), LCD0_CLK (inner via → R50). PB2–7 ×6 (S1: 3 east, 3 west with O1) |
| W | 11 | CC_SENSE (inner via → W4). USB ×2 (W2). UART3 ×4 (W1 → W5). I2S1 ×3 + JACK_DET (A3) to the W4 staircase |

- **Margins at the ring:** PWR_BTN_3V3 0.18 mm to C3.1 (N4, 0.2 mm Power width), the 0.6 mm +0V9 trunk 0.225 mm to
  UART3_RX and 0.2 mm to USB_D+, RESET's B riser 0.22 mm to U3's east pads; DRC-clean on the combined board.
- **Left for step 4 at U1:** UART3 arriving reversed at U18 (2 via pairs at U18), the LDOA B ring, the redrawing of the
  viafit2 stubs on the 45° grid (they fail route.py's lint as drawn). The NOR via pairs are trialled (IO0, IO3).

### 2. The display corridor U1 → P1 (22 bus nets + DCLK)

Unchanged from the integration (group 40, `groups/40_display.py` + `groups/display/geom.py`): south entry under P1's
body, B rows in U1 pin order (D1), one via per net in a block staircase, F columns in P1 order with one parallel 45° jog
(D2). DCLK: U1.75 → inner via → R50 → RGB_DCLK on F only, 0 vias, round the staircase (1.1 mm of it over antipads).
Lengths (KiCad, incl. 1.586 mm per via): B2 32.17 mm .. DE 55.83 mm, skew 23.66 mm; DCLK 50.23 mm. North of P1:
BL_LED_N / BL_LED_P on B, 0.25 mm (D4); P1's plane-via spots at y 63.75 (D5). Re-run the generator after any move of
U1, P1, R50, R39, C45, TP11 or U6 and read the skew in the DRC.

### 3. The north-east corridor and U17 (A1 plain swap)

Four lines go north from U1's NE corner on B (NE1 → NE2), west to east: RESET (its F riser at x 72.45 from C31 drops
to B at (72.45, 58.5)), PWR_BTN_3V3 (U1.31 via N4, x 73.0), PSHOLD (U1.36, x 77.2) and BL_CTRL (U1.37, x 77.6).
Destinations: SW1.1 (79.0, 37.55), R40.1/U17.8 (U17's east side), U17.4, R51.1 (100.75, 52.3) → U6.2.

- **Group 10 first:** SYS from T3 → C68.1 → U17.1 (0.6 / 0.25 mm, B) is drawn before group 60, the case that broke
  the rotation.
- **Combined trial (s4 `ne`), all B, 0 extra vias:** RESET 23.2 mm to SW1.1 (SW1.1–SW1.2 tied), PWR_BTN_3V3 34.7 mm
  to R40.1 → U17.8, PSHOLD 25.9 mm to U17.4, BL_CTRL 32.8 mm east at y 50.8 to R51.1, then down x 103.85 into U6.2
  from under the SOT-23 (west, between the pin rows): no via in the U6–L2 gap; the trace keeps 0.58 mm from U6.3's BL_SW pad (the pins themselves are 0.35 mm apart). R38's Kelvin GND passes under U6's body at
  x 103.25 to U6.4. Full net lengths with the escape stubs: PSHOLD 50.3, PWR_BTN_3V3 53.2, BL_CTRL 58.0 mm.
- U17's /PB (U17.6, to SW14) and PWR_EN (U17.9, to U12) leave on F through vias at (96.2, 35.6) and (98.85, 37.45)
  into T0; U17.12's GND via (98.85, 39.0).

### 4. The USB band

The pair runs on F over In1 nearly all the way: B escape west (W2) to the vias D+ (50.95, 75.95) and D− (52.15,
76.30); F riser x 50.95 / 51.425 north to y 48; east along y 47.0 / 47.475; down to B at the D4-end vias D− (117.05,
48.2) and D+ (118.95, 47.0); flow-through D4 (A4); the receptacle with the A7 jumper (F, vias (118.25, 39.5) /
(117.25, 40.5), 0.5/0.3). 0.275/0.20 mm (90.7 Ω, DRU).

- **Measured with the D4 end drawn (combined board):** D− 115.97 mm / 4 vias, D+ 114.68 mm / 2 vias, skew 1.30 mm,
  uncoupled 14.37 mm (D6), via count ≤ 4 (D3); the diff_pair_gap exception covers only the USBC1 and D4 pin fields.
- **CC_SENSE:** two hops (the review: one hop cannot reach R56.2 and R57.2 on either side of the pair's B legs): C94 →
  F up W4 at x 56.1, east at y 52.4 → via (64.8, 52.4) → B down x 64.8 under the I2S lanes and the band → via (64.8, 45.4) → F
  east along y 45.4 (north of the band, south of T0) → B drops at (113.9, 45.4) to R56.2 and (122.1, 45.4) to R57.2.

### 5. The top strip over the charger

The strip (x 127.5–181.7, y 32–52.3, 20.3 mm) is the only link between the left block and the right grip.

- **Group 05 runs first** and reserves (channels.py VIA_SPOTS V1–V27, 100 spots, every via of groups 05/10 and the
  trials): U12's EP 2 × 4 + V1, U10's EP 5, U13's EP 2, U9/U20 2 × 2, the switcher-cap and decap GND vias (two per
  CIN/COUT GND pad where they fit), U12's COUT +3V3 pairs at y ≤ 37.0, U15/C44/C65/C66, with explicit pad-to-via links
  (a starved thermal otherwise).
- **Group 10, trunks around that field (s2):**
  1. **BAT** (F, 1.2 mm = 2.73 A, IPC-2221 10 °C): CN1.1 → north to y 40.1 → west to (176.0, 40.1) (0.3 mm south of
     U13's EP vias; H2 and CN1.1 rule out the north side there) → NW to the top edge y 33.2 → west north of U10's EP →
     down x 153.6 to 3 × 0.8/0.4 vias (2.5 A) → C57.1 / U10.2–3. ≈ 41 mm, ≈ 17 mΩ: 34 mV at 2 A.
  2. **+5V** (F, 0.8 mm = 2.0 A): one 0.8/0.4 via under each VBUS pad → F join at y 38.3 → T1's north edge y 40.7 →
     jog to y 38.45 over U12 (north of its EP grid and V1, south of the COUT vias) → y 40.7 → two 0.8/0.4 drops at
     (158.75, 41.05/41.95) → C55.1 → U10.13. ≈ 42 mm, 38 mV at 1.45 A. D4.5/C93 via (118.45, 42.8).
  3. **SYS_SRC** (B, 1.2 mm) U10.10/11 → C56.1 → R27.1 along the top; **SYS** (B) R27.2 → T2 at y 50.6 (2.0 mm) → the
     U12 branch (0.75 mm up x 140.55), VINA (0.6, up x 134.0), T3 west at y 49.3, east at y 47.5 (0.6 mm) through the B
     neck to U9; INA226 Kelvin taps at R27.
- **Pin locals (s4 `strip_locals`, B):** U10's EN2/LED F hops north of BAT, PGOOD west to its T1 drop (148.5, 42.4),
  TS to its via (154.6, 48.0), ISET/TMR/ILIM/ILIM_SW (ILIM ≤ 8 mm, 0 vias), CHG_FAST to (162.6, 48.6); U12's L1/L2 at
  full width, FB, PS/SYNC, EN's via (137.5, 42.45); R47.2 / R55.2 share R15.2's / Q1.2's GND vias (B links), so T1's
  button band at y 45.3–47.1 passes the U10 field. **I2C1 at U15/U13:** SDA keeps one B link U15.4 → U13.8 (north);
  SCL is two B stubs (U15.5, U13.7) joined on F between vias A (177.9, 41.25) and B (179.4, 41.25), so SDA's drop
  (178.65, 41.8) comes up between them without crossing SCL; GAUGE_ALRT drops at R43.1 (182.3, 42.35); R43.2's +3V3
  via moves to (180.45, 38.95), north of BAT's F.
- **T1 lanes (s4 `strip_lanes`, explicit, F, 0.15 mm at 0.30 pitch, 45° jogs with bends staggered 0.13 mm):**

| x | Bands N→S (y) |
|---|---|
| 124 (handoff from T0/M1/H1, group 80) | /PB 41.65 (B hop (124.6, 41.65) → (127.6, 46.95) under the north lanes), PWR_EN 42.45, CHG_PGOOD 43.0, SCL 43.3, SDA 43.6, GAUGE 43.9, R1 44.2, VOL+ 44.5, VOL− 44.8, B 47.8, X 48.1, SELECT 48.4, Y 48.7, A 49.0, CHG_FAST 49.3, SPK_EN 49.6, DOUT 49.9, BCLK 50.2, LRCK 50.5 (/PB rejoins at 47.5) |
| 133–145 (U12 field) | PWR_EN ends at its via (137.5, 42.45); 7 lanes 43.0–44.8 between it and C60/C61/R23's vias (45.7–46.95); 11 lanes 47.5–50.5 south of them |
| 152–161 (U10 field) | SCL, SDA, GAUGE, R1, VOL+ 42.65–43.85 (+5V drops above, GND row 44.63–44.75 below); VOL−, /PB, B, X, SELECT, Y, A 45.3–47.1; CHG_TS from its via 48.0; CHG_FAST 48.6 to its via; SPK_EN, DOUT, BCLK, LRCK 49.2–50.1 |
| 163–182 | SCL/SDA/GAUGE up to 41.0–41.6 (between C66/C44's vias and BAT) to their drops (V11); the 9 button lanes into the staircase (V12, vias (174.6 + 0.7k, 43.15 + 0.3k), each F lane 0.225 S of its via, each B lane 0.225 N); CHG_TS and the I2S/SPK_EN down to 50.45–51.65 for T1n |

- **Plane neck:** In1 GND keeps ≥ 14.35 mm of the strip's 19.25 mm height at every x in 160–182.5 (narrowest at x
  180.5: H2's hole, V11, V12), In2 +3V3 ≥ 15.0 mm (x 179.5). The staircase is diagonal (2.4 mm tall), not a row
  across the neck.

### 6. Into the right grip

The cutout's corner is an arc (centre 179.7/54.3, r 2.0; copper limit r 2.5); CN1 occupies F x 182.6–190.8,
y 40.77–50.23, and BAT now holds F north of CN1, so the grip is reached on B (9 lanes) and through the F neck (5).
- **B neck (T1b):** the 9 lanes R1, VOL+, VOL−, /PB, B, X, SELECT, Y, A at y 42.925–45.325 (0.30 pitch) between R43
  (y ≤ 41.03) and SYS's east branch (y ≥ 47.2): 1.8 mm spare on each side. East of the neck they fan on B, each turning
  where it is the outermost lane: R1 NE to its via (192.99, 40.6) → SW16.2; VOL+ (206.3, 45.99) → SW12.2; VOL−
  (205.6, 53.99) → SW13.2; /PB (204.9, 61.99) → SW14.2; B (203.2, 71.8) → SW7.1; X (195.9, 63.95) → SW8.1; SELECT
  (195.2, 47.0) → SW11.1; Y (194.4, 79.9) → SW9.1; A (187.9, 71.9) → SW6.1, all columns east of SYS (x 186.8). 2 vias per
  button; /PB 5 (U17's exit, the hop pair, the staircase, the grip).
- **F neck (T1n):** CHG_TS, SPK_EN, DOUT, BCLK, LRCK as a 45° octagon r 3.85 .. 2.65 about the arc's centre (1.08 mm
  spare to CN1.4's corner: 3 more lanes), then down the wall at x 183.55 .. 182.35 on F to their drops: CHG_TS (184.1,
  60.4) → R17.1; SPK_EN (185.0, 76.0) → R14.2; DOUT (184.0, 78.95) → U9.1; BCLK (182.9, 79.6) → U9.16; LRCK (182.55,
  82.6) → U9.14. No vias in the neck.
- **SYS to U9:** B east at y 47.5 through the B neck, down x 186.8 to C53.1/C54.1 → U9.7/8.
- Left for group 85: U9.2 (GAIN_SLOT → SYS) is boxed in on the north by R14/SPK_SD (route it under U9's body or move
  R14 0.5 mm east); the TS-1187A pad ties.

## Channels (route/channels.py)

Hard channels keep fanout's plane vias and test points out; route groups place their own signal vias where this table
says. On the combined board `chancheck.py` finds no foreign part in any hard channel, and `check_plan.py` passes (every
escape channel exists, every ESCAPE net matches the re-netted board, every escape via is in VIAS and on the board, every
via in a hard channel is the channel's own).

| Ch | Layer | Kind | Carries |
|---|---|---|---|
| N1 | B | hard | SD_CD + SD ×6, one planar fan through D1/D2 (A8) and the pull-ups to U4 (A2, P4) |
| N2 | B | hard | NOR ×6 north to U3; IO0/IO3 via pairs (V22) |
| N3 | B | hard | HOSC to X1/C3/C4 only (the In2 island under it runs to y 68.6) |
| N4 | B | hard | PWR_BTN_3V3 riser (A1) beside C3.1, 0.2 mm |
| K1 / K2 | F | hard | rule areas: no tracks over X1; no pattern between X2's pads |
| N5 | F | hard | LDOB lane, U1.30 via → U1.48/49 vias |
| NE1 / NE2 | B | hard / soft | RESET, PWR_BTN_3V3, PSHOLD, BL_CTRL risers / their fan to SW1, R40/U17, R51/U6 |
| E1 / E2 | B | hard | U1.33–45 east escapes / the IMU ribbon to U7 (P2, A7) |
| D1 / D2 / D4 / D5 | B / F / B / * | hard (D5 soft) | group 40: rows + staircase / F bundle / backlight feed / P1's plane-via row |
| S1 | B | hard | PB2–7 south (O1) |
| W1 / W2 / W3 | B | hard | UART3 + the I2S/JD staircase stubs / USB escape / west turn strip |
| W4 | F | hard | I2S T, CC_SENSE's riser, the I2C1 risers |
| USB | F | hard | USB riser and band (y 47.0 / 47.475) |
| W5 | B | soft | UART3 to U18 |
| H1 / M1 / S2 / T0 | F | soft | highways (H1's lanes turn east at y ≥ 51.5, clear of the SD via row) |
| T1 / T1n / T1b | F / F / B | soft / hard / hard | strip highway / the F neck + wall / the B neck |
| T2 / T3 | B | hard / soft | strip power band (SYS, y 50.6) / SYS west to U6 and U17 |
| P2 | B | hard | C45 → U6.4 boost return |
| A1 / A1c / A1f | B / B / F | soft / hard / hard | audio quiet zone / the routed audio core (group 75) / HP_L's F leg |

`VIA_SPOTS` (with `VIA_POINTS`, the exact vias): 100 tight boxes in 27 named sets, generated from the combined board:
V1 U12 PGND, V2 USB-C band, V3 U12 EP, V4 U12 COUT, V5 U12 input caps, V6 U10 EP + C57, V7 U10 GND vias, V8 BAT drops,
V9 +5V drops, V10 U15/U13/C44/C66/C65, V11 I2C1/GAUGE drops, V12 the B-neck staircase, V13 SD support row, V14 IMU
pocket, V15 X1 stitch, V16 U2 core buck, V17 U6 boost, V18 U9/U20 EP, V19 audio core, V20 grip drops, V21 strip lane
drops, V22 NOR hops, V23/V24 CC_SENSE hops and drops, V25 USB D4 end, V26 U17 locals, V27 U10 north hops. Also
`NO_PARTS` (the display flex landings, the touch flex landing).

## Power budget (route/ipc_check.py, to port)

IPC-2221 curve, I = k·ΔT^0.44·A^0.725 (k 0.048 outer, 0.024 inner/barrel), ΔT 10 °C, 1 oz outer. Via capacity with JLC's
18 µm hole plating: 0.68 A for a 0.3 mm drill, 0.84 A for 0.4, 1.00 A for 0.5; ≈ 1.7 mΩ each. Sheet resistance
0.49 mΩ/□ outer, 1.13 mΩ/□ inner. Worst-case SYS load at once: U12 input 1.87 A, U6 0.33 A, U9 ≤ 0.5 A peak: 2.7 A
(BOM-REVIEW expects 1.8–2 A peaks, ≈ 1 A typ).

| Net | Worst I | IPC min | Plan (as trialled) | Vias per layer change |
|---|---|---|---|---|
| +5V (VBUS) | 1.45 A | 0.53 mm | 0.8 mm F, ≈ 42 mm (jog at y 38.45 over U12), 38 mV | one 0.8/0.4 per VBUS pad; 2 × 0.8/0.4 at C55/U10.13; one 0.6 at TP4, one at D4.5 |
| SYS_SRC | 2.7 A | 1.18 mm | 1.2 mm B U10.10/11 → C56 → R27.1; 0.25 mm at the 0.5-pitch pins (D8) | none |
| SYS | 2.7 A at R27; 2.2 A west; 0.5 A east | 1.18 / 0.89 / 0.12 mm | R27.2 → T2 2.0 mm (25 mV at 2.2 A); U12 branch 0.75 mm; T3 0.6 mm; east to U9 0.6 mm | none |
| BAT | 2.7 A out (CN1: 2 A per contact), 0.445 A in | 1.18 mm | 1.2 mm F, ≈ 41 mm along the top edge, 34 mV at 2 A | 3 × 0.8/0.4 at C57.1; the gauge tap (0.6) at C65.1 |
| +3V3 | 1.4 A | In2 | U12 → C62–64 → In2, 2 vias per 22 µF cap; plane one piece (10259 mm²) | 4 at COUT |
| +0V9 | 0.8 A | 0.22 mm | C34.1 → U1.116/117 B 0.6 mm, then the Z2 island | five cap vias ≤ 0.16 A each |
| LDOB / LDOA | 0.4 A / 85 mA | 0.1 mm / – | N5 lane 0.35 mm F / B ring | 1 |
| CORE_SW / BB_L1/L2 / BL_SW | 1.0 / 4.5 pk / 1.44 pk A | 0.30 / 0.78 / 0.5 mm | pad + ≤ 1 mm / 0.8 mm past V1 (0.25 at the pins) / one polygon | – |
| BL_LED_P/N, SPK_P/N | 40 mA, 0.45 A pk | – / 0.12 mm | 0.25 mm (D4) / ≥ 0.4 mm U9.9/10 → J3 | 1 / – |

## Signal integrity

- **USB 2.0 HS:** 0.275/0.20 mm, 90.7 Ω differential (DRU field solver; review 2D solve 92.4 Ω, 91.3 Ω with the F pour).
  ≈ 115 mm, D− 4 vias / D+ 2, skew 1.30 mm, uncoupled 14.37 mm; F over In1 with nothing else on F inside the band.
- **RGB666:** DCLK 23–27 MHz, period ≥ 37 ns; skew 23.66 mm ≈ 0.15 ns. Keep PD18 at drive <20> or <30>.
- **SD:** ≤ 50 MHz; all B over In2, 0 vias, CLK/CMD/D0–D3 25.57–29.34 mm (skew 3.77 mm), ESD D1/D2 in the lanes.
- **NOR:** 11.76–21.94 mm, skew 10.19 mm, IO0/IO3 2 vias each.
- **I2S1:** BCLK 3.07 MHz. U9 branch from the strip's west end 104.9–107.2 mm (BCLK, DOUT, LRCK) + ≈ 85 mm W4/H1, on F
  over In1, 2 vias (W4 and the wall drop). Lower PG12/13/15's drive strength if BCLK rings at U9.
- **Crystals:** HOSC on B, ≤ 5.1 mm, 0 vias, over the X1 island from the pin tips; a partial GND guard (X1.2–X1.4
  linked under the crystal, two stitch vias north), NOT a ring: it cannot close on the east, C3 is 0.08 mm from N4. The
  line beside C3.1 is the static PWR_BTN_3V3. RTC: X2 on F, 1 via each, K2.
- **I2C1** (≈ 203 mm per line, 1.5 kΩ): ≈ 119 pF, t_r ≈ 151 ns vs 300 ns; SCL takes one more via (the A/B split at
  U15/U13). **I2C2:** 34–36 mm on B, ≈ 22 ns.
- **TP11:** 0.6 mm from everything (D4).

## Grounding and zones

- **In1:** solid GND, no slots; antipads only (the RGB field's merge into ≈ 4–5 mm slots; the strip staircase is a
  2.4 × 5.6 mm diagonal one, crossed by no F lane: each button's F lane ends at it).
- **In2:** +3V3 plane with the Z1 GND islands and the Z2 +0V9 island (priority 2). Islands keep their fill only through
  GND vias: the switcher caps' own and the EP grids (group 05), pour.py's island pass (1.5 mm grid where all four layers
  are free, outside hard channels and VIA_SPOTS; X1's two stitch vias are group 05's).
- **X1 island:** 66.2–71.4 × 62.6–68.6. RESET's U1.27 via (69.40, 67.95) is inside it: 0.5/0.3 and 0.05 mm north of
  the step-2 spot so its In2 antipad (r 0.45) stays 0.075 mm clear of HOSC_IN's edge. PWR_BTN_3V3 (x 71.6) runs over
  the island's east gap. U2's VIN/RUN vias and U12's COUT stay outside their islands.
- **Exposed pads:** U1 4 × 4 at 1.1 mm (plugged/capped), U10 5, U12 2 × 4 + V1, U9 2 × 2, U20 2 × 2, U13 2.
- **Switchers:** FP6161 CIN/COUT GND plate + 3 vias; TPS63021 PGND/GND at one point, PGND vias in V1; TPS61165 C45
  return in P2, R38's GND Kelvin-routed to U6.4 under the body (drawn in the ne trial).
- **Pours (port gb2's `pour.py`):** whole-board GND on F and B, solid by default; D5's thermal rules; stitching as
  before (edge every 2.5 mm, a 4 mm field grid, a GND via per U18 GND pad, both sides of the USB band).
- **Audio:** SGND (U20.15) has its own 0.15 mm B trace to a via at the jack's sleeve pad (TPA6132A2: sense ground at
  the jack); C83.2/C84.2 their own GND vias (the chains box their pads in).

## Rule areas and DRU edits

- `route/footprint_keepouts.py` (run once after placement, idempotent; 22 areas): X2 no pattern (F), X1 no tracks (F),
  U18 antenna (all layers), U4's two prohibition bars (B), the SKRT lug gaps of SW12–SW16 (F), the H2 keep-out (F+B; more holes to come,
  r 2.75), and the restriction-free "pins: U9, U10, U12, U13, U15, U17, U2, U6, D4, USBC1" areas that D8 reads.
- `proposed/gb3.kicad_dru`: D2–D8 above.

## Order (route/groups/, built by route.py; then fanout.py, pour.py)

| Group | Scope |
|---|---|
| `05_switcher_gnd` | FIRST (drafted): the Z1 In2 GND islands, the EP via grids, the switcher-cap / decap GND vias with explicit pad links, U12's COUT +3V3 vias, U2's plate vias, U6's boost GND vias, X1's stitch vias |
| `10_power` | Trunks (drafted): VBUS/+5V and BAT on F, SYS_SRC/SYS on B (T2, T3, the U12 branch, VINA, the U17 feed, east to U9), Kelvin taps; then the strip's pin locals (trialled: `scripts/trials/strip_locals.py`), the U2 core buck loop and the +0V9 trunk, the U6 boost loop |
| `20_soc_island` | U1's inside: viafit2 connections on the 45° grid (U1.27's via 0.5/0.3), the EP 4 × 4, N5, the C9/C10/C17 vias, R5, U1.91's AGND via, TP9/TP10, Z2, the LDOA ring, RTC_IN into X2.1 |
| `25_clocks_reset` | HOSC (N3), RTC (X2), RESET (→ SW1.1 + the SW1.1–SW1.2 tie) |
| `30_usb` | The pair end to end (4 vias on D−), CC1/CC2, CC_SENSE with its two hops |
| `40_display` | Drafted and trial-routed (groups/40_display.py + display/geom.py); P3's two caps |
| `50_sd_nor` | SD: the N1 fan with A8/P4, all taps, the V13 row; NOR: N2 with the IO0/IO3 hops |
| `60_east` | E1/E2/NE1/NE2: UART0, PWR_BTN_3V3 → R40.1 → U17.8, PSHOLD → U17.4, U17's /PB and PWR_EN vias, BL_CTRL → R51.1 → U6.2 under the body, TOUCH ×2, EXP_INT, SPK_EN → M1, the IMU ribbon with U7's passives and V14 |
| `70_west` | W1/W5 UART3 → U18, BT_EN_RST; W4 (the I2S/JD staircase, the DAC branch → R32–34, JACK_DET → R49, the U9 branch riser); I2C1 → R30/R31 |
| `75_audio` | Before 80/85 (was 90): the U19 DAC / U20 HP-amp core with A6 inside A1c/A1f (trialled: `scripts/trials/audio.py`), the jack Audio1, JACK_DET's end |
| `80_highways` | H1/M1/S2/T0 to T1's west-end order (section 5), T1 itself (trialled: `scripts/trials/strip_lanes.py`), T1n |
| `85_grips` | Right grip (the B neck fan, the switch drops, U9/R14/R17 locals, U9.2's strap, the TS-1187A ties) and left grip (D-pad SW2–5, SW10, SW15, U18's GND vias, C70–C73) |

Then: `fanout.py` (one plane via per remaining plane pad, outside hard channels and VIA_SPOTS; P1's at D5), `pour.py`,
`planecheck.py`, `ipc_check.py`, `i2c_check.py`, `audit.py`, `silk.py` (incl. the new silk-over-copper on R8, R43,
D1, C95/C96 and H2's reference at the edge).

## Status (2026-10-14, fix pass): plan revised, no copper in the repo

**One combined scratch board** (`~/.cache/gb3-step3/fix/scripts/build_trial.sh`, KiCad 10.0.4, the proposed DRU):
1. s1: the 16 placement moves, the re-net (nets_changes.csv), the new parts (C_RET1/2 = C95/C96 in the repo; H1/H2 - that H1, at (102, 36), later dropped), footprint_keepouts.py.
2. s2: route.py groups 05, 10, 40 and the P1 plane-via preview: lint clean.
3. s3: the U1 escape geometry (58 vias, 188 stubs, no item re-netted).
4. s4: the trials on the same board, in group order: ne (60), imu (60), usb + CC_SENSE (30), sd (50), nor (50), audio
   (75), strip_locals (10), strip_lanes (80/85): 0 fails.
5. s5: the Z2 +0V9 island and the island stitching (pour.py stand-in), refill.
- DRC (refilled): 4 errors, all pre-existing inside USBC1's footprint; vs the repo board 0 new errors; warnings: 43
  dangling tracks and 2 dangling vias (the open ends of partial routes: T1's west end, the escape stubs, U17's T0 vias),
  silk (10 silk-over-copper, H2's reference).
- Measured on s5: RGB skew 23.66 mm; USB skew 1.30 / uncoupled 14.37 mm; SD skew 3.77 mm; NOR skew 10.19 mm; HOSC
  288/288 samples over the island; In1 ≥ 14.35 mm across the strip; +3V3 plane one piece.
- `chancheck.py`: no foreign part in a hard channel. `check_plan.py`: OK.
- Evidence: renders `fix/img3/s5_*.png` (strip + grip, X1 over In2, audio, NE, SD, IMU); measurements `fix/work/meas/`
  (netlen_s5.txt, ref_s5.json = refcheck, usbt/drc.json = KiCad's own skew/uncoupled under tightened rules,
  repodru/drc.json = the combined board under the repo's rules).

**Open for step 4:** the LDOA ring and the 45° redraw of the island stubs (20); UART3's reversal at U18 (70); group 80's
west half (deliver T1's x-124 order through the H1/M1/T0 junction: the I2C1 lanes and the right-grip buttons both come
up M1); the left grip (85); U9.2's strap and the TS-1187A ties (85); port fanout/pour/planecheck/ipc_check/i2c_check/
audit/silk.

## Step 4 (2026-10-14): groups 05-60 built in the repo

`route.py` (no arguments) rebuilds 05, 10, 20, 25, 30, 40, 50, 60 from the bare board; a full rebuild equals the staged
group-by-group build (1096 items). DRC errors 4 -> 4 (USBC1's), unconnected 388 -> 138, KiCad parity unchanged, sync
all zeros. Tooling: `maze.py` now blocks tracks only in rule areas that forbid them and only on their own layers (the
DRC-only "pins: <ref>" areas used to wall off their parts), takes extra per-layer keep-outs and a search budget;
`route.py` takes pad keys as route ends and keeps `.kicad_pro` as it was.

What changed against the trials:
- **10:** + the strip's pin locals (trial), the U2 core buck (SW 0.6 mm, VIN -> CIN, L1 -> COUT 1.0 mm, RUN, FB; the
  divider top and the feed-forward cap Kelvin-sensed at L1 / TP8, routed west of the RUN / FB parts and over L1), the
  +0V9 trunk (W3), U6's boost loop (SW node U6.3 / L2.2 / D3.2, D3 -> C45, C45.2 -> U6.4 up P2).
- **20:** the island stubs are on the 45 deg grid: 12 vias re-fitted by <= 0.33 mm (`mcu_escape.py` has the new spots)
  and U1.28 shares U1.26's LDOA via; the LDOA ring is on B (4 maze legs, ~35 mm, a few mA).
- **25:** RESET's NE2 leg is explicit (north, then 45 deg to SW1.1): a fresh maze took the diagonal first and boxed
  PSHOLD in.
- **60:** PWR_BTN_3V3 / PSHOLD are the trial's parallel 45 deg pair, explicit; BL_CTRL's second leg starts at R51.1
  (second review); E1 east is explicit: TOUCH_RST / TOUCH_INT to FPC1, EXP_INT north of U16's decaps (C43 / C67 keep
  both thermal spokes) to R29 and U16.1. SPK_EN ends at x 101 for group 80.
- A few trial segments were 1-16 deg off the 45 deg grid (route.py lint): redrawn.

Open: 70_west, 75_audio, 80_highways (SPK_EN from x 101; the T0 vias for /PB and PWR_EN; the T1 drops group 10
placed: CHG_TS, CHG_FAST, CHG_PGOOD, GAUGE_ALRT, I2C1_SDA), 85_grips, then fanout / pour / checks / silk.

## Second review (2026-10-14): items for step 4

The second independent review reproduced the fix pass (same DRC, measurements and zones) and kept the plan's
decisions. Its trial-level findings, each to be closed by the group that owns it:

- **75_audio:** draw U19's local caps first (C81/C82 to U19.2/4/5, C74, C77, then the TPA charge-pump caps), only then
  the DAC legs; the maze-first trial boxed CAPP/CAPM/VNEG in. Fix C74.2's starved thermal (direct GND link); redraw
  A1c from the corrected result.
- **60_east (BL_CTRL):** the maze chamfered into R51.1 and left a 0.5 mm gap; start the second leg at R51.1. The trial
  build must fail when a trialled net is still unconnected.
- **80_highways (west half):** A5's port-0 order is now in the netlist (above). The old H1 hole at (102, 36) is gone, so T0 has
  its normal exit; plan T0's dip under D4 (y 43.4 / 43.7, /PB on top). I2S needs a B hop under the M1 bundle (+6 vias, 4
  per line) or an entry via S2 from the south-east. Widen M1 to x ≈ 128.5. Prove T0 + the H1 channel + M1 by a trial
  without the pour stand-in.
- **Jumper pads:** done in step 3 (`jumper_groups.py`: D4 1–6 / 3–4, TS-1187A SW1–SW11 1–2 / 3–4), so no copper ties.
- **Smaller:** the B neck's north spare is 0.20 mm at the GAUGE via (182.3, 42.35), not 1.8 mm; BAT's 3 drop vias carry
  ~2.5 A (the firmware load cap holds the battery at ≤ 1.8 A, so 3 suffice); draw X2's C5/C6 links; trials did not run
  in group order (harmless).
