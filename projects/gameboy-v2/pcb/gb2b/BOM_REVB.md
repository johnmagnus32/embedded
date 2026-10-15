# gb2 rev B: BOM changes against rev A

This is rev B's BOM delta. Rev A's full BOM is `../BOM.md`, which is unchanged. Rev B's source of truth is
`gb2b-bom.csv` (154 parts; rev A has 134). Its machine-readable BOM, the one `fab/fab.py` reads, is `BOM.md` in this
directory: rev A's rows with the rev B rows applied (#84-#92 new). The reasoning behind each change is in
`../REVB_DESIGN.md` §4-5. Tier and stock come from the JLC parts API, checked live on 2026-10-14. "Extended" means a JLC
extended part, which carries the per-part feeder fee; "Basic" means a basic part.

## Parts

| Change | Ref | Part | LCSC | JLC tier | Stock | Subcircuit |
|---|---|---|---|---|---|---|
| Delete | J4 | HX PM2.54-1x20P ZC socket (2090 JP2) | C41417332 | Extended | 3,139 | - |
| Replace | J3 | 2090 socket -> **JUSHUO AFC07-S50ECA-00**, 50P 0.5 mm top-contact ZIF, H2.0, on B.Cu | C262650 | Extended | 17,479 | display |
| Replace | J1 | MTP125-1105S1 (THT, C358687) -> **HX PZ2.54-1x5P WT**, right-angle SMD lying flat, 2.5 mm high | C46061678 | Extended | 15,802 | mcu |
| New | R32 R33 R34 R35 | 0603WAF1200T5E, 120 Ω 1 %: one backlight anode resistor per LED, from +5V_LCD | C22787 | Basic | 2,230,468 | backlight |
| New | Q2 | AO3400A N-MOSFET, SOT-23 (1 G, 2 S, 3 D): low-side switch on LEDK | C20917 | Basic | 767,275 | backlight |
| New | R36 | 0603WAF1000T5E, 100 Ω 1 %: Q2 gate series R from PC6 | C22775 | Basic | 11.4 M | backlight |
| New | R37 | 0603WAF1003T5E, 100 kΩ 1 %: Q2 gate pull-down (backlight off through reset) | C25803 | Basic (already on the board) | 19.9 M | backlight |
| New | R38 | 0603WAF1002T5E, 10 kΩ 1 %: LCD_RST pull-up | C25804 | Basic | 32.8 M | display |
| New | R39 | 0603WAF1002T5E, 10 kΩ 1 %: BOOT1 (PB2) pull-down, so BOOT0 = 1 always selects the ROM | C25804 | Basic | 32.8 M | mcu |
| New | C52 | CL21A106KOQNNNE, 10 µF 0805: panel VCI/VDDI bulk at J3 | C1713 | Extended (already on the board) | 290,538 | display |
| New | C53 | CC0603KRX7R9BB104, 100 nF: panel VCI/VDDI HF at J3 | C14663 | Basic (already on the board) | 51.2 M | display |
| New | U13 | **USBLC6-2SC6** (ST): USB D+/D- ESD + VBUS clamp, SOT-23-6, at J7 | C7519 | Extended | 33,612 | usb-c |
| New | D4 | **SMF5.0A** (MDD): 5 V unidirectional TVS 200 W on VBUS, SOD-123FL, pin 1 = cathode | C193402 | Extended | 559,201 | usb-c |
| New | MP1 MP2 | Exposed ENIG GND pads under the panel, for the panel-frame foam contacts (2 x 8, 5 x 6 mm) | - (not assembled) | - | - | display |
| New | MP3-MP6 | Exposed ENIG GND pads for the plate tabs (10 x 1.75, 3.2 x 1.75, 2.8 x 8, 3.1 x 8 mm) | - (not assembled) | - | - | display |
| New | MP7 MP8 | Exposed ENIG GND pads under the panel for the plate's two copper-foil bridges, at its long-edge midpoints (2 x 8, 1.6 x 8 mm) | - (not assembled) | - | - | display |
| Change | R25 | Same part (C25803, 100 kΩ). Its pin 2 moves from GND to +3V3: PS/SYNC pull-**up**, so the buck-boost is in forced PWM whenever PC3 isn't driven | C25803 | Basic | 19.9 M | buck-boost |
| Reel swap | R1 R4 R5 R8 R10 R12 R22 R23 R31 | RC0603JR-0710KL (5 %) -> 0603WAF1002T5E (1 %), same 10 kΩ 0603, same footprint `easyeda2kicad:R0603` | C99198 -> C25804 | Extended -> Basic | 32.8 M | (unchanged) |
| Move sheet | C25 C26 | Unchanged parts, now "+5V_LCD decoupling at R32-R35" | C14663, C1713 | | | display -> backlight |
| Note only | R29 | Place by U2 pin 37; its output goes to J3 pin 36 (WRX) | C23140 | Basic | 5.1 M | fpga |
| Note only | J7 | Now also carries USB FS data (to PA11/PA12) | C165948 | Extended | 382,916 | usb-c |
| Note only | J8 | Same part; mates the Adafruit 258 cell, moves behind U9 | C295747 | Extended | 39,025 | charger |

**Extended part count:** C41417332, C358687 and C99198 drop out, and C262650, C46061678, C7519 and C193402 come in: net
**+1 unique extended part**. (With 110 Ω anode resistors, C22781, extended, 10,981 in stock, it would be +2;
REVB_DESIGN.md §8.1 Q4.)

**Hand-fitted, not in the JLC BOM:**
- EastRising ER-TFT028A2-4, "No Touch Panel" (buydisplay.com). Buy 3: it is bare glass and the FPC can tear.
- Adafruit 258 cell (PKCELL LP-503562 + PCM, 1200 mAh, JST PH). Check its polarity with a meter before the first plug-in.
- The mechanical parts listed in REVB_DESIGN.md §5.1: the stainless plate, tapes, epoxy, film, the MP1/MP2 foam pads,
  the two MP7/MP8 copper-foil bridges, the gasket, the lens and the door.

**JLC BOM/CPL filtering:** the TP and MP pads, J5/J6 (solder pads) and U12 (no LCSC number) have no part to place. The
MP footprints carry `exclude_from_bom` and `exclude_from_pos_files`, as the TP footprints do. `fab/fab.py` reads each
part's source from `BOM.md` in this directory by its `#N` and stops with an error if a `#N` is missing.

## Library

The new symbols and footprints are in the project-local library `lib/` (nickname `gb2b`), fetched with easyeda2kicad by
LCSC number. `cad/symbols/` was not touched. Each footprint's pad numbering was checked against its datasheet:

| Ref | Footprint | Check |
|---|---|---|
| J3 | `FPC-SMD_AFC07-S50ECA-00` | 50 signal pads at 0.5 mm, pad 1 at x -12.25, pad 50 at +12.25 (24.50 = DIM A for 50 circuits). Fitting nails are pads 51/52, 2.2 x 3.3 at x ±13.85, which matches the drawing's 2.70 from circuit 1 to the nail's outer edge. The signal pads are 1.50 long, centred on the drawing's 1.20 land, so 0.15 longer at each end. Contact type "E" = upper (top) contact. Courtyard enlarged to x ±15.3, y -2.1 to +5.7: it now covers the pads (to y -1.84), the nails and the open slider (about +5.43); the easyeda one stopped at y -0.71. The symbol's pins carry the panel's pin names (LEDK, IM0, VSYNC, DB7 ...). |
| J1 | `HDR-SMD_5P-P2.54-H-M-W12.8-N` | 5 pads, 1.02 x 3.17 at 2.54 mm (drawing: 1.02 x 3.20). Pad 1 at x -5.08, with the silk dot. The pins point +Y. The part has no locating pegs, so it is held by solder alone. 13.5 mm from the pad edge (y -1.585) to the pin tips (+11.9). Courtyard enlarged to x ±6.6, y -1.85 to +12.15 (it covered only the insulator, y 3.35-5.85). A mated 14 mm Dupont housing reaches about +19.9 (not in the courtyard; REVB_DESIGN.md §3.7 keeps its zone). The symbol's pins are 5.08 apart so the sheet's labels don't touch. |
| Q2 | `SOT-23-3_L2.9-W1.3-P1.90-LS2.4-BR` | Pads 1 and 2 on one side, 3 alone. The AOS top view shows G = 1, S = 2, D = 3. |
| U13 | `SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BL` | Standard SOT-23-6 numbering. Pins 1/6 are I/O1, 2 GND, 3/4 I/O2, 5 VBUS (cad/docs/C7519.pdf Fig. 1); the symbol's pins carry those names. |
| D4 | `SOD-123FL_L2.7-W1.8-LS3.8-RD` | Pad 1 is under the silk cathode bar. Symbol pin 1 is C, which matches the MDD drawing (band = cathode). |
| R32-R39, reel swap | `easyeda2kicad:R0603` | 2 pads. The gb2b 0603WAF symbols point at rev A's own `easyeda2kicad:R0603`, so "Update PCB from Schematic" keeps the footprints of the reused, routed resistors. The project-local copy `gb2b:R0603` (identical pads, but an embedded "LCSC Part" = C22781) was removed. |
| MP1-MP8 | `GNDPad_<w>x<h>mm` (custom) | One F.Cu + F.Mask pad, no paste. Sizes are from REVB_DESIGN.md §3.5 / §4.8; MP7 reuses the 2.0 x 8.0, MP8 is the new 1.6 x 8.0. The GND vias are added at layout. |

The 3D models are in `lib/gb2b.3dshapes/`, referenced as `${KIPRJMOD}/lib/gb2b.3dshapes/`. They take about 20 MB, and
the AFC07 .step alone is 11 MB. J1 has no 3D model from LCSC.
