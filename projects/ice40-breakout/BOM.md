# iCE40 Breakout PCB — Bill of Materials

**Status:** DRAFT / tentative parts — pending JLCPCB availability check. See
[docs/BREADBOARD_ICE40_BOARD.md](../../docs/BREADBOARD_ICE40_BOARD.md) for the design spec.

**~21 placements (22 with optional SS pull-down), 10 unique part numbers**

Sourcing legend:
- ✅ **Reused from gameboy-v1 / t113-breakout** — already validated JLC-sourceable and in `cad/` library.
- ⚠️ **New / verify** — confirm LCSC stock + tier on JLCPCB before ordering.

> **Header rework (2026-07-13):** consolidated the six functional headers (config /
> runtime-SPI / LCD / power-probe / 2× spare-GPIO) into **2× 1x25 female headers, one
> per left/right board side**. Power rails (3V3, 1V2) and grounds now interspersed into
> the two big headers rather than a dedicated J5 probe header. Per-side pin assignment
> still TBD — needs board edge length + decision on grouping (by chip edge vs. keeping
> functional buses together). See Pin Mapping section.

| # | Part Number | Description | Qty | LCSC # | Src | Notes |
|---|-------------|-------------|-----|--------|-----|-------|
| 1 | iCE40UP5K-SG48I | FPGA, QFN-48 7×7mm 0.5mm pitch, 5.3K LUT | 1 | C2678152 | ✅ | **Confirmed in stock on JLCPCB.** Exposed pad = only GND, needs thermal vias. Extended tier — order early as it is the non-swappable part. |
| 2 | AP2127K-1.2TRG1 | 1.2V LDO, SOT-23-5, 300mA | 1 | C151376 | ✅ | **Confirmed in stock on JLCPCB.** Core rail. 3.3V in → 1.2V out (~2.1V headroom). Exact UPduino v3 part. Feeds VCC + VCCPLL. |
| 3 | CC0603KRX7R9BB104 | 100nF 0603 X7R ceramic cap | 7 | C14663 | ✅ | Per-supply-pin decoupling: VCC ×2, VCCIO_0/1/2 ×3, VPP_2V5 ×1, VCCPLL ×1. |
| 4 | CC0603KRX5R6BB475 | 4.7µF 0603 X5R ceramic cap | 4 | C109456 | ✅ | LDO in (C1), LDO out (C2), 1.2V bulk (C4), VCCPLL filter (C5). |
| 5 | CL21A106KOQNNNE | 10µF 0805 X5R ceramic cap | 1 | C1713 | ✅ | 3.3V input bulk (C3). |
| 6 | GZ1608D601TF | Ferrite bead 600Ω@100MHz 0603 | 1 | C1002 | ✅ | 1.2V → VCCPLL noise filter (FB1). |
| 7 | RC0603JR-0710KL | 10KΩ 0603 resistor | 2 | C99198 | ✅ | CRESET_B pull-up to 3.3V (R1), CDONE pull-up to 3.3V (R2). |
| 8 | 0603WAF5100T5E | 510Ω 0603 resistor | 1 | C23193 | ✅ | CDONE "config OK" LED current limit (~2.5mA). |
| 9 | 19-213SYGC/S530-E2/5T | Green LED, 0603 | 1 | C2986027 | ✅ | CDONE status indicator (D1). |
| 10 | 2044-1X25G00SA | 2.54mm 1x25 female header | 2 | C49569761 | ✅ | **2 headers, one per board side (BBB/Nucleo-style).** Consolidates all breakout: config bus + runtime SPI + LCD bus + all spare user I/O + interspersed GND. 50 positions total vs. 48 currently used → ~2 spare. Reuses the 1x25 female footprint (HDR-TH_25P-P2.54-V-F) from t113-breakout. Per-side pin split TBD (see Pin Mapping). |

**Optional / not counted in the 25:** an optional 10KΩ SPI_SS_B pull-**down** (R3, C99198) may be
fitted to guarantee slave-boot even if the STM32 is absent — see Design Note 4.

**Deliberately omitted:** no manual CRESET_B reset button. The STM32 is the sole driver of CRESET_B
and reconfigures the FPGA on every boot, so a button is redundant; CRESET_B is on header J1 if a manual
reset is ever needed on the bench.

---

## Pin Mapping

> **Two-header layout (2026-07-13):** consolidated to **2× 1x25 female**, one per board side.
> Split follows the QFN perimeter — pins **43→48→1→18** (chip's left half) go to the LEFT header,
> pins **19→42** (right half) go to the RIGHT header. Because QFN pins run continuously around the
> perimeter, listing them in arc order gives crossing-free fanout. The RIGHT header lists **descending**
> (42→19) so both headers run top-to-bottom the same physical direction.
> **Orientation caveat:** whether pos 1 is at the top or bottom of each header depends on final U2
> rotation/placement — flip a header's order at layout if the fanout comes out reversed.
> The J1–J7 tables further below are retained as the **STM32↔FPGA net reference** (unchanged).

### LEFT header (1x25) — chip left half, pins 43→18

| Pos | Net | iCE40 pin |
|-----|-----|-----------|
| 1 | +3V3 | — |
| 2 | LCD_CS | 43 |
| 3 | IOB_3b_G5 | 44 |
| 4 | IOB_5b | 45 |
| 5 | IOB_0a | 46 |
| 6 | GND | — |
| 7 | IOB_2a | 47 |
| 8 | IOB_4a | 48 |
| 9 | IOB_6a | 2 |
| 10 | IOB_9b | 3 |
| 11 | IOB_8a | 4 |
| 12 | IOB_13b | 6 |
| 13 | GND | — |
| 14 | CDONE | 7 |
| 15 | CRESET_B | 8 |
| 16 | IOB_16a | 9 |
| 17 | IOB_18a | 10 |
| 18 | IOB_20a | 11 |
| 19 | IOB_22a | 12 |
| 20 | IOB_24a | 13 |
| 21 | SPI_SO | 14 |
| 22 | SPI_SCK | 15 |
| 23 | SPI_SS_B | 16 |
| 24 | SPI_SI | 17 |
| 25 | RUNTIME_SPI_CS | 18 |

*22 signals + 1 power + 2 GND = 25.*

### RIGHT header (1x25) — chip right half, pins 42→19 (descending)

| Pos | Net | iCE40 pin |
|-----|-----|-----------|
| 1 | +1V2 | — |
| 2 | LCD_DC | 42 |
| 3 | RGB2 | 41 |
| 4 | RGB1 | 40 |
| 5 | GND | — |
| 6 | RGB0 | 39 |
| 7 | LCD_WR | 38 |
| 8 | IOT_45a_G1 | 37 |
| 9 | LCD_D7 | 36 |
| 10 | GND | — |
| 11 | IOT_46b_G0 | 35 |
| 12 | LCD_D6 | 34 |
| 13 | LCD_D5 | 32 |
| 14 | LCD_D4 | 31 |
| 15 | GND | — |
| 16 | LCD_D3 | 28 |
| 17 | LCD_D2 | 27 |
| 18 | LCD_D1 | 26 |
| 19 | LCD_D0 | 25 |
| 20 | GND | — |
| 21 | IOT_37a | 23 |
| 22 | RUNTIME_SPI_MOSI | 21 |
| 23 | RUNTIME_SPI_CLK | 20 |
| 24 | IOB_29b | 19 |
| 25 | +3V3 | — |

*19 signals + 2 power + 4 GND = 25. Total both headers: 41 signals + 3 power + 6 GND = 50.*

FPGA-centric view. Config-pin package numbers on the SG48 are fixed by the part (confirm against the
datasheet during schematic); runtime/LCD numbers are the `.pcf` assignments from the spec (§3b/§3c) and
**may be reassigned** to route cleanly, provided `icebreaker.pcf` is updated to match.

### Configuration bus → J1 (used only at boot)

| iCE40 Config Pin | Role | Connects To |
|------------------|------|-------------|
| SPI_SCK (IOB_34a) | Config SPI clock | J1 ← STM32 PC10 (SPI3_SCK) |
| SPI_SI  (IOB_33b) | Config SPI data in | J1 ← STM32 PC12 (SPI3_MOSI) |
| SPI_SO  (IOB_32a) | Config SPI data out | J1 → STM32 PC11 (only if NVCM programming) |
| SPI_SS_B (IOB_35b) | Config select / boot strap | J1 ← STM32 PB6 (GPIO) |
| CRESET_B | Config reset | J1 ← STM32 PB1 (GPIO), 10KΩ pull-up (R1) |
| CDONE | Config done | J1 → STM32 PB2 (GPIO in), 10KΩ pull-up (R2) + LED (D1) |

### Runtime game-data bus → J2 (every frame, one-way STM32 → FPGA)

| iCE40 Pin (.pcf) | Net | Connects To |
|------------------|-----|-------------|
| 4  | SPI_CLK  | J2 ← STM32 PA5 (SPI1_SCK) |
| 2  | SPI_MOSI | J2 ← STM32 PA7 (SPI1_MOSI) |
| 47 | SPI_CS   | J2 ← STM32 PA4 (GPIO) |

### Runtime display bus → J3/J4 (FPGA → ILI9341, 8-bit parallel 8080)

| iCE40 Pin (.pcf) | Net | Connects To |
|------------------|-----|-------------|
| 43, 38, 34, 31 | LCD_D0–D3 | J3/J4 → ILI9341 D0–D3 |
| 28, 27, 26, 25 | LCD_D4–D7 | J3/J4 → ILI9341 D4–D7 |
| 21 | LCD_WR | J3/J4 → ILI9341 WR (write strobe) |
| 20 | LCD_DC | J3/J4 → ILI9341 DC (data/command) |
| 19 | LCD_CS | J3/J4 → ILI9341 CS |

### Power / spare

| FPGA Rail | Source | Header |
|-----------|--------|--------|
| VCC ×2 (core, 1.2V) | U2 LDO out | J5 (1V2 probe) |
| VCCPLL (1.2V, filtered) | U2 LDO out via FB1 | — |
| VCCIO_0 / SPI_VCCIO1 / VCCIO_2 (3.3V) | 3V3 header input | J5 (3V3 probe) |
| VPP_2V5 (3.3V) | 3V3 header input | — |
| All remaining ~21 user I/O | — | J6, J7 (spare GPIO) |

---

## Design Notes

1. **Power input is 3.3V on a header** (J5), not a barrel/USB — jumper from the Nucleo 3V3, bench
   supply, or elsewhere. Only the 1.2V core rail is regulated on-board (U2). **No 3.3V or 5V regulator.**
2. **Power sequencing: simple cascade, accepted.** 1.2V is derived from 3.3V, so the 3.3V I/O rails
   come up *before* the 1.2V core — reverse of Lattice's recommended order. The UP5K tolerates this
   (order recommended, not required; POR gates config on VCC). No load switch / supervisor fitted. See
   spec §5b for the optional load-switch fix if config proves flaky on silicon.
3. **VPP_2V5 → 3.3V.** Because the design uses `SB_HFOSC`, VPP_2V5 must be ≥2.30V (DS Table 4.2 Note 4);
   3.3V satisfies this. All three VCCIO banks → 3.3V so config + runtime I/O match the STM32 (no level
   shifters).
4. **SPI_SS_B is the boot strap.** It has an *internal pull-up* → idles HIGH → would default to MASTER
   boot. The STM32 must drive it LOW when releasing CRESET_B for slave config. **Do NOT add a pull-up.**
   An optional 10KΩ pull-**down** (R3) guarantees slave mode even with no STM32 attached.
5. **CRESET_B** has no internal pull-up: 10KΩ to 3.3V (R1) gives a defined idle level before the MCU
   boots; STM32 push-pull GPIO still drives it. Optional tactile reset button (SW1).
6. **CDONE** is open-drain: external 10KΩ pull-up (R2) to 3.3V + green LED (D1, via 510Ω R8) as a
   "configured OK" indicator.
7. **QFN-48 assembly** — exposed center paddle is the ONLY ground; tie to GND with thermal vias.
   Reflow/hot-air only → full JLCPCB turnkey PCBA. Headers likely hand-soldered (JLC often won't place
   generic 0.1" THT headers).
8. **Decoupling** (starting point, per iCEBreaker/UPduino — confirm against Lattice TN1252): 100nF on
   each of the 7 supply pins + 10µF bulk on 3.3V in + 4.7µF bulk on 1.2V. VCCPLL filtered from 1.2V via
   FB1 (600Ω ferrite) + 100nF + 4.7µF.
9. **No external crystal** — the PPU uses the internal `SB_HFOSC` (~24MHz). No oscillator part needed.
10. **Config SCK must be 1–25MHz** (slave-SPI min is 1MHz — do not run slower). Set by the STM32 SPI3
    prescaler, not on this board.
11. **All ~39 user I/O broken out.** Functional nets (config/SPI/LCD) get their own labeled headers;
    the remaining ~21 land on J6/J7 spare-GPIO headers so signals can be added without a respin.
