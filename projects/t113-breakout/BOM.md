# T113-S3 Breakout PCB — Bill of Materials

**Status:** ✅ All 24 parts JLCPCB-verified (in stock, footprints confirmed). Ready for schematic capture.

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
| 9 | Power | CL05C100JB5NNNC | 10pF 50V C0G 0402 (Basic, Samsung) — FB feed-forward cap across R_top (one per buck), REQUIRED for loop stability | 2 | C32949 | ✅ |
| 10 | Power | 0603WAF2400T5E | 240Ω ±1% 0603 — DZQ DDR ZQ-calibration resistor, T113 pin 47 → GND | 1 | C23350 | ✅ |
| 11 | Storage | W25Q128JVSIQ | 128 Mbit SPI NOR flash, SOIC-8 (IQ: QE=1 factory-set) | 1 | C97521 | ✅ |
| 12 | Storage | A-MicroTF-1.85A | microSD/TF socket, SMD, push-push, has card-detect (MyAntenna) | 1 | C22467599 | ✅ |
| 13 | Storage | RC0603JR-0710KL | 10 KΩ 0603 — SD pull-ups (CMD + DAT0-3, 5×) + RESET pull-up (1×), all → 3V3 | 6 | C99198 | ✅ |
| 14 | Clock | X322524MRB4SI | 24 MHz crystal, SMD3225-4P, CL=18 pF, ±10/20 ppm (YXC) | 1 | C70571 | ✅ |
| 15 | Clock | SC-20S 32.768kHz | 32.768 kHz RTC crystal, SMD2012-2P, ±20 ppm, CL=12.5 pF (Seiko) | 1 | C97607 | ✅ |
| 16 | Clock | 0402CG220J500NT | 22 pF 50V C0G 0402 (Basic) — 24 MHz crystal load caps | 2 | C1555 | ✅ |
| 17 | Clock | 0402CG180J500NT | 18 pF 50V C0G 0402 (Basic) — 32.768 kHz crystal load caps | 2 | C1549 | ✅ |
| 18 | Decoupling | CC0603KRX7R9BB104 | 100nF 0603 X7R cap (per power pin + flash VCC + SD VDD + RESET filter) | 25 | C14663 | ✅ |
| 19 | Decoupling | CL21A106KOQNNNE | 10µF 16V X5R 0805 cap (bulk + SD VDD) | 9 | C1713 | ✅ |
| 20 | Decoupling | CC0603KRX5R6BB475 | 4.7µF 0603 X5R cap (buck out / bulk) | 4 | C109456 | ✅ |
| 21 | Header | 2044-1X25G00SA | 2.54mm 1x25 female — 4× (2 per side, BBB/Nucleo-style). ALL breakout: 58 free GPIO + SD (PF0–5) + flash (PC2–7) + UART0 (PE2/3) + power (5V/3V3/0V9) + ~24 GND. 100 positions total. | 4 | C49569761 | ✅ |
| 22 | Status | 19-213SYGC | Green LED, 0603 (power indicator) | 1 | C2986027 | ✅ |
| 23 | Status | 0603WAF5100T5E | 510Ω 0603 resistor (LED current limit) | 1 | C23193 | ✅ |
| 24 | Control | TS-1187A-B-A-B | 6×6mm SMD tactile switch — RESET button (RESET → GND) | 1 | C318884 | ✅ |

> **Crystal load caps (#13, #14):** values are starting points, tuned at bring-up.
> #13 = 22 pF matches the MangoPi reference for the same 18 pF-CL 24 MHz crystal
> (implies ~7 pF board stray); strict formula for typical 3–5 pF stray → ~26–30 pF.
> Barely affects USB (±2500 ppm tolerant). #14 = 18 pF is on-target for the 12.5 pF-CL
> RTC. If a clock reads off-frequency on a scope during bring-up, adjust these caps.
> Both are C0G/NP0 0402.

> **Power rails (from T113 + FP6161 datasheets):** USB-C 5 V → two FP6161 bucks →
> 3.3 V (FB 680K/150K) and 0.9 V core (FB 75K/150K), each with a **10pF feed-forward
> cap across R_top (#9), required per FP6161 datasheet for loop stability**. Divider
> ratios give 3.32 V / 0.90 V ✓ (kept from MangoPi; datasheet's 306K/68K & 75K/150K
> are equivalent ratios). Each buck: 2.2µH inductor on SW, CIN 100nF near VIN, COUT
> 10µF+4.7µF. The SoC's internal LDOA/LDOB make the 1.8 V and 1.5 V (DDR3) rails from
> the 3.3 V LDO-IN, so no PMIC. Core buck RUN gated ~2 ms after 3.3 V good (RUN
> threshold 0.3–1.5 V per datasheet → driving RUN2 from 3.3 V works); RESET held low
> >64 ms after rails stable. 22 power pins → one 100nF each (#16). Exposed pad = only
> GND, thermal vias. FB node: keep away from SW, route over ground plane (datasheet layout note).

> **Boot / debug:** BROM → SPI-NOR (SPL/U-Boot) → kernel/rootfs on microSD; USB-C
> also gives FEL recovery (unbrickable). BOOT-SEL straps on PC4/PC5 (shared with the
> SPI-NOR bus) select boot media. SD and QSPI buses each have a Saleae debug tap
> (#23) — short stubs, probe at slow clock. i8080 parallel-LCD signals are on the PD
> bank (all broken out via #22), so an ILI9341 can be driven in 8-bit parallel.

> **UART0 console (#21, 1x3 = TX/RX/GND):** route UART0 to **PE2 (UART0-TX) / PE3
> (UART0-RX)** — NOT the default PF2/PF4, which collide with the microSD (SDC0-CLK /
> SDC0-D3). PE2/PE3's only alt-function is the unused NCSI camera, so they're free and
> coexist with SD. Configure PE2/PE3 pin-mux to UART0 in software. Note: the BROM's
> earliest chatter may still come out on PF2/PF4 (not visible, = SD pins), but U-Boot
> + Linux console run on PE2/PE3 — which is all that matters. 115200 baud, 3.3V USB-serial.

> **Flash (W25Q128JVSIQ):** "IQ" = QE=1 factory-set → /WP=IO2, /HOLD=IO3 become quad
> data lines (per datasheet). Used in QSPI, IO2/IO3 are active data → no pull-ups
> needed. VCC gets a 100nF (in #17 budget). If ever run standard-SPI, tie IO2/IO3 high.

> **SD socket:** footprint pin-map verified against A-MicroTF-1.85A datasheet (CLK/CMD/
> DAT0-3/VDD/VSS + card-detect + shell). CMD is open-drain → needs pull-up; DAT0-3
> pulled up too (#12, 5×10K→3V3). VDD decoupled with 100nF + 10µF at the socket
> (in #17/#18 budget). Card-detect (pin 9) available → wire to a spare GPIO if wanted.

> **Board:** 4-layer minimum (128-pin 0.5 mm QFP + in-package DDR3 + USB2 → needs
> dedicated GND/power planes). eLQFP-128 is hand-solderable / JLC-assemblable (not a
> BGA). Camera (DVP) and on-board audio deliberately omitted for a minimal boot board.
