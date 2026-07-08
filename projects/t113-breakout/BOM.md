# T113-S3 Breakout PCB — Bill of Materials

**Status:** ✅ All parts sourced & JLCPCB-verified (in stock, footprints confirmed). Ready for schematic capture.

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 1 | SoC | T113-S3 | Dual Cortex-A7 SoC, 128 MB DDR3, eLQFP-128 | 1 | C5197687 | ✅ |
| 2 | Power | TYPE-C-31-M-12 | USB-C receptacle (5 V in + USB2 data) | 1 | C165948 | ✅ |
| 3 | Power | RC0603FR-075K1L | 5.1 KΩ 0603 resistor (USB-C CC pulldowns) | 2 | C105580 | ✅ |
| 4 | Power | FP6161KR-LF-ADJ | Synchronous buck, SOT-23-5, 1A, 0.6V FB (3.3 V + 0.9 V core), AAT | 2 | C77234 | ✅ |
| 5 | Power | SMNR4020-2.2UH | 2.2 µH 3.4 A 46 mΩ power inductor, 4×4mm SMD | 2 | C135262 | ✅ |
| 6 | Power | RS-03K6803FT | 680K ±1% 0603 — FB divider R_top, 3.3 V rail | 1 | C140074 | ✅ |
| 7 | Power | 0603WAF1503T5E | 150K ±1% 0603 (Basic) — FB divider R_bottom, both bucks | 2 | C22807 | ✅ |
| 8 | Power | 0603WAF7502T5E | 75K ±1% 0603 (Basic) — FB divider R_top, 0.9 V core rail | 1 | C23242 | ✅ |
| 9 | Storage | W25Q128JVSIQ | 128 Mbit SPI NOR flash, SOIC-8 | 1 | C97521 | ✅ |
| 10 | Storage | A-MicroTF-1.85A | microSD/TF socket, SMD, push-push (MyAntenna) | 1 | C22467599 | ✅ |
| 11 | Clock | X322524MRB4SI | 24 MHz crystal, SMD3225-4P, CL=18 pF, ±10/20 ppm (YXC) | 1 | C70571 | ✅ |
| 12 | Clock | SC-20S 32.768kHz | 32.768 kHz RTC crystal, SMD2012-2P, ±20 ppm, CL=12.5 pF (Seiko) | 1 | C97607 | ✅ |
| 13 | Clock | 0402CG220J500NT | 22 pF 50V C0G 0402 (Basic) — 24 MHz crystal load caps | 2 | C1555 | ✅ |
| 14 | Clock | 0402CG180J500NT | 18 pF 50V C0G 0402 (Basic) — 32.768 kHz crystal load caps | 2 | C1549 | ✅ |
| 15 | Decoupling | CC0603KRX7R9BB104 | 100nF 0603 X7R cap (1 per power pin) | 22 | C14663 | ✅ |
| 16 | Decoupling | CL21A106…0805 | 10µF 0805 cap (bulk) | 8 | C1713 | ✅ |
| 17 | Decoupling | CC0603…475 | 4.7µF 0603 cap (buck out / bulk) | 4 | C109456 | ✅ |
| 18 | Header | Pin Header 1x3 | 2.54mm UART0 console (TX/RX/GND) | 1 | C49257 | ✅ |
| 19 | Header | KH-2.54PH180-1X13P | 2.54mm 1x13 — ALL free GPIO (PB/PD/PE/PG ≈58 + GND), per-edge in pin order | 6 | C2932703 | ✅ |
| 20 | Header | KH-2.54PH180-1X7P | 2.54mm 1x7 — QSPI flash debug tap (CLK/CS/IO0-3 + GND) for Saleae | 1 | C2932700 | ✅ |
| 21 | Header | KH-2.54PH180-1X7P | 2.54mm 1x7 — SD (SDC0/PF) debug tap (CLK/CMD/DAT0-3 + GND) for Saleae | 1 | C2932700 | ✅ |
| 22 | Header | Pin Header 1x5 | 2.54mm power/probe (5V/3V3/CORE/GND/GND) | 1 | C358687 | ✅ |
| 23 | Status | 19-213SYGC | Green LED, 0603 (power indicator) | 1 | C2986027 | ✅ |
| 24 | Status | RC0603…510R | 510Ω 0603 resistor (LED limit) | 1 | C23193 | ✅ |

> **Crystal load caps (#13, #14):** values are starting points, tuned at bring-up.
> #13 = 22 pF matches the MangoPi reference for the same 18 pF-CL 24 MHz crystal
> (implies ~7 pF board stray); strict formula for typical 3–5 pF stray → ~26–30 pF.
> Barely affects USB (±2500 ppm tolerant). #14 = 18 pF is on-target for the 12.5 pF-CL
> RTC. If a clock reads off-frequency on a scope during bring-up, adjust these caps.
> Both are C0G/NP0 0402.

> **Power rails (from T113 datasheet + MangoPi reference):** USB-C 5 V → two FP6161
> bucks → 3.3 V (FB 680K/150K) and 0.9 V core (FB 75K/150K); the SoC's internal
> LDOA/LDOB generate the 1.8 V and 1.5 V (DDR3) rails from the 3.3 V LDO-IN, so no
> PMIC is needed. Core buck enable is gated ~2 ms after 3.3 V is good (datasheet
> sequencing rule); RESET held low >64 ms after rails stable. 22 power pins total →
> one 100nF each (#15). Exposed pad = only GND, needs thermal vias.

> **Boot / debug:** BROM → SPI-NOR (SPL/U-Boot) → kernel/rootfs on microSD; USB-C
> also gives FEL recovery (unbrickable). BOOT-SEL straps on PC4/PC5 (shared with the
> SPI-NOR bus) select boot media. SD and QSPI buses each have a Saleae debug tap
> (#20, #21) — short stubs, probe at slow clock. i8080 parallel-LCD signals are on
> the PD bank (all broken out via #19), so an ILI9341 can be driven in 8-bit parallel.

> **Board:** 4-layer minimum (128-pin 0.5 mm QFP + in-package DDR3 + USB2 → needs
> dedicated GND/power planes). eLQFP-128 is hand-solderable / JLC-assemblable (not a
> BGA). Camera (DVP) and on-board audio deliberately omitted for a minimal boot board.
