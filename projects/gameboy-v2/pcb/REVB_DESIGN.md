# gb2 rev B: design spec

The ER-TFT028A2-4 IPS panel is held in the front shell under a glass lens. The battery sits in a cut-out in the board,
behind a stainless plate. The device is flat front and back, about 12.5 mm thick all round.

> **Revision 2 (2026-10-14): what changed against the draft (Revision 1)**
> - **Screen:** the Adafruit 1774 (MI0283QT-11, 4.00 mm, TN, touch film) becomes the **EastRising ER-TFT028A2-4**
>   (2.80 mm, IPS, no touch). Pins 1-43 are the same, and so are the FPC tail, the fold and the contact side. So J3,
>   the netlist and the joggle all carry over.
> - **Mounting:** the panel is no longer taped to the board. It hangs in the front shell, on a 0.3 mm gasket under a
>   1.1 mm glass lens (1.5 mm acrylic for the prototype), with a 0.3 mm gap to the board (§2 decision 1).
> - **Battery:** the bump behind the board (PKCELL LP-803860, 8.5 mm) becomes the **Adafruit 258** (LP-503562, 5.0 mm).
>   It sits in a **portrait cut-out in the board**, open at the bottom edge, behind a 0.5 mm stainless plate. Its leads
>   come out at the top, and J8 sits beside the charger.
> - **Thickness:** about 12.5 mm uniform, with a flat back. Revision 1 had 10.9 mm at the grips and 18.2 mm at the
>   bump. You were quoted about 12.2 mm; §3.1 shows where the extra 0.3 mm comes from.
> - **Board:** 1.2 mm thick (Revision 1: 1.0), still 4 layers. A routing-capacity count shows 6 layers aren't needed
>   (§6.4).
> - **Placement:** the MCU moves to the back of the right grip. The FPGA moves to the back, above the opening, and J1
>   becomes a right-angle header lying flat. Everything you need for service is behind one battery door (§6).
> - **New electrical parts:**
>   - USB D+/D- to the STM32 for DFU, with an ESD array (U13) and a VBUS TVS (D4);
>   - GND contact pads for the panel's frame and for the plate;
>   - the backlight resistors go from 100 Ω to 120 Ω, because this panel's maximum is 80 mA (§4.3).
> - **Schematic-review fixes (2026-10-14, after the schematic step):**
>   - BOOT1 gets its own strap: CDONE moves from PB2 (BOOT1) to the spare PB0, and a new R39 pulls PB2 low, so the
>     BOOT0 pad selects the ROM after an NRST reset too (decision 22).
>   - R25 becomes a PS/SYNC pull-**up**, so the buck-boost is in forced PWM whenever PC3 isn't driven (decision 14).
>   - R32-R35 become 120 Ω basic parts (C22787); 110 Ω reached 80.1 mA with resistor tolerance (decision 11).
>   - Two GND pads, MP7/MP8, are added for the plate's copper-foil bridges (decision 23).
>   - J1's real length puts a mated Dupont housing at the board edge; its zone is an open placement item (decision 9).
>   - The DFU key combo moves to Select + Start, and the ROM's interfaces, the USB pull-up rule and the VBUS limit are
>     documented (§4.7).
> - **Removed:** the panel tape rules, the touch-film notes, the bay and bump, the vertical SWD header and the MI0283QT-11
>   init table.
> - **Unchanged:** every Revision 1 item not listed above, notably the FPC route, J3, the U2 pin map, the straps and the
>   reset. Appendix B logs every number re-checked for this revision.

**Status, 2026-10-14:** rev A (`gb2/`) is ordered and frozen. In `gb2b/`, the §4-5 schematic delta is applied
(`gb2b-nets.csv` / `gb2b-bom.csv`, regenerated with `gb2b/gen.py`; BOM delta in `gb2b/BOM_REVB.md`; fab BOM
`gb2b/BOM.md`), including the schematic-review fixes: 154 parts, with LCD_TE on U2.44 per decision 16. The PCB is still
rev A's until §9 step 3.

**Sources**
- **Panel:** EastRising ER-TFT028A2-4 datasheet V1.0 (Dec-02-2019). Archived copy:
  `http://web.archive.org/web/20250425090544id_/https://www.buydisplay.com/download/manual/ER-TFT028A2-4_Datasheet.pdf`.
  Page numbers are the PDF footers:
  - p.6 outline drawing ("with No Touch Panel")
  - p.9 pin table, p.10 IM table, p.11 DC and backlight characteristics
  - p.18-20 handling and installation precautions
- **ILI9341:** `cad/docs/ILI9341-datasheet.md`: §4 pin table, §12 power-on, §15.4 reset timing, §18 note 2.
- **Cell:** PKCELL LP-503562 spec QA.S.0228, as approved for Adafruit 258 (`cad/docs/C101-_Li-Polymer_503562_1200mAh_3.7V_with_PCM_APPROVED_8.18.pdf`):
  p.2 Form 1 (ratings), p.5 pack drawing.
- **Connectors:**
  - The JUSHUO AFC07 drawing (LCSC C262650), plus the easyeda2kicad footprint `FPC-SMD_AFC07-S50ECA-00` and its 3D
    model.
  - JST PH catalogue `cad/docs/C295747.pdf` p.3 (S2B-PH-SM4-TB: 5.5 tall).
- **Rev A** (all read-only; the board was opened with KiCad's python and never saved):
  - `gb2/`: `gb2-nets.csv`, `gb2-bom.csv`, `gb2.kicad_pcb`, `gb2.kicad_pro` (net classes), `gb2.kicad_dru`,
    `mech_ref.json`, `draw_mech.py`, `route/PLAN.md`
  - `pcb/BOM.md`
  - `enclosure/`: `DESIGN.md`, `params.py`, `STRUCTURAL_REVIEW.md`, `build/board.json`
- **Studies:**
  - `pcb/revb_architecture/` (`ARCHITECTURE.md`, `ee_plan.json`, `me_*.png`), with the teardown, ME and EE results of
    2026-10-14;
  - the sourcing results for the panel and the cell.
- **Other datasheets:**
  - `cad/docs/EZ-DS-BT830_v3_0.pdf` (§12.1.1)
  - `cad/docs/STM32F411RE-datasheet.md`: §3.13 boot modes, Table 8 (PA11 USB_FS_DM, PA12 USB_FS_DP), memory map
    (system memory 0x1FFF 0000)
  - `cad/docs/C7519.pdf` (USBLC6-2SC6, Fig. 1)
  - `cad/docs/tps63021.pdf`, `cad/docs/C84817.pdf` (MT3608)
- **JLC parts API**, queried live on 2026-10-14. The tier and stock figures below come from it.
- **Scratch scripts** in `/tmp/gb2b/r2/` reproduce every Revision 2 number; they are not kept:
  - `model.py`: outline, cut-out, blocks and netlist delta;
  - `check.py`: fit and collision checks;
  - `capacity.py`: routing capacity;
  - `stack.py`: z-stack and joggle, using the formulas of `/tmp/revb/mech/stack.py`.

**Conventions**
- **XY:** KiCad board mm, +X right, +Y down, seen from the front. Back-side positions use the same frame; they are **not
  mirrored**.
- **Z:** z = 0 at the board's bottom face, and the board top is at z = t (DESIGN.md:13). "Above the top" and "below the
  top" are measured from the board's top face. The enclosure frame flips Y (params.py:37-38).
- **u:** distance inboard from the panel's FPC edge, so x = 132.55 − u.
- **t:** board thickness. The baseline is t = 1.2; t = 1.6 is shown in [brackets] where it differs.
- **Tracks:** rev A's Default class is 0.15 track with 0.15 clearance (`gb2.kicad_pro`), so one routing slot is 0.30 mm.
  Copper stays 0.5 from any edge.

---

## 1. Goal and summary numbers

**What changes:** rev A's Adafruit 2090 breakout goes. That is a panel on its own PCB, 11 mm up on 8.5 mm sockets
J3/J4, with the cell under it in a board cut-out. In its place:
- The bare ER-TFT028A2-4 panel hangs in the front shell, 0.3 mm in front of the board. Nothing is mounted on the front
  under it.
- Its FPC folds behind it and joggles through an opening into J3, on the back.
- The cell sits in a portrait notch in the board, left of J3, behind a stainless plate. It is closed in by a flush door.

**What stays:**
- The STM32F411 and the iCE40UP5K PPU, which drive the ILI9341 over the same 8-bit 8080 bus.
- Rev A's power section, audio, haptics, BT830, buttons and edge parts, in rev A's positions.

| Thickness, front face to back face (mm) | Rev A | Rev B, glass lens (t = 1.2) | Rev B, acrylic prototype | GBA (AGB-001) |
|---|---|---|---|---|
| Everywhere: grips, screen and battery | 24.5 | **12.5** (12.495) | 13.1 (13.095) | 10.6 at 2 mm from the end; 16.4 at 10; 20.8 at 20; 24.0 at 35; 24.5 in the central band |
| Front face above the board top | 19.3 | 4.615 | 5.215 | - |
| Back face below the board top | 5.2 | 7.88 | 7.88 | - |

- **Sources:** rev A's figures are Z_FRONT 20.9 and Z_BACK −3.6 at t = 1.6 (DESIGN.md:17-18). Rev B's come from §3.1,
  and the GBA's from the mech_ref.json gba "Depth budget".
- **Thickness at t = 1.6:** 12.8 mm, because J8 then sets the back (§3.1).
- **Comparison with the GBA:** rev B is about half the GBA's central thickness and 1.9 mm thicker at the very grip
  ends.

Other headline numbers:
- **Board area:** 6185 mm², against rev A's 6273 mm². That is the 10 079 mm² outline minus the speaker notch, the
  2307 mm² cell notch and the 694 mm² FPC opening.
- **Screen position:** the active area is centred on the device, at (95.25, 71.0). Rev A's sat 2.64 mm left
  (params.py:213).
- **Footprints:** 67 stay, 65 move (24 of them change side), J4 is deleted, J3 is replaced, and 21 are new (R32-R39,
  Q2, C52, C53, U13, D4 and MP1-MP8). That makes 154 in total.
- **BT830 clearance to the nearest large metal:** 16.9 mm in rev A (the 2090 PCB). In rev B, from U12, it is:
  - 25.5 mm to the panel frame;
  - 28.5 mm to the plate;
  - 28.8 mm to the cell.
- **Battery trace:** about 2-5 mm from J8 to U9 (J8 sits right behind U9), against about 60 mm in Revision 1.

---
## 2. Decisions

1. **Panel: EastRising ER-TFT028A2-4, no-touch version, held in the front shell.** *(Revision 2)*
   - **Specs:** IPS with ILI9341 chip-on-glass, 240 × 320, 350 cd/m², 80° viewing all round.
     - The module is 69.30 × 50.20 × 2.80 mm with the FPC folded (p.6).
     - The LCD glass is 64.75 × 46.20, and the active area is 57.60 × 43.20.
     - Its back carries two strips of double-sided tape (T = 0.115). The drawing doesn't say whether the 2.80
       includes them, so the stack budgets them.
   - **Mounting:**
     - The panel drops into a pocket in the front shell, located to about ±0.15.
     - A 0.3 mm black closed-cell foam gasket ring joins its top border to the masked back of a 1.1 mm strengthened-glass
       lens, flush with the face. The ring is low-tack on the panel side.
     - The prototype uses 1.5 mm cast acrylic on a 0.5 mm gasket instead (+0.6 mm overall).
     - The panel's back sits 0.3 mm in front of the board's top face, and nothing is mounted under it.
   - **Why:** the teardown study found that none of the 11 handhelds it surveyed tapes the LCD to the PCB. The GBA, DS
     Lite and Analogue Pocket all hold the panel in the housing. The datasheet asks for the same:
     - "free of twisting, warping and distortion" and "free from torque and mechanical stress" (p.18-19);
     - "Cover the surface with a transparent protective plate" with a spacer "to avoid causing stress to the module
       surface" (p.20).
   - **Trade-offs:** the lens gap becomes the gasket's ±0.05 instead of a five-part tolerance chain, the panel can be
     serviced, and the glass isn't loaded when the board flexes.
2. **Interface: 8080 MCU 8-bit bus I.**
   - IM[3:0] = 0000, and the data is on DB7..DB0, which are FPC pins 25-32 (p.10).
   - The nets are the same as rev A's, still driven by the iCE40 (U2), not the STM32 (gb2-nets.csv:128-146).
3. **Panel position: the active area is centred on the device centre (95.25, 71.0), and the FPC exits at +X.**
   - **Extents:** the module spans x 63.25-132.55, y 45.90-96.10. The p.6 borders are the same as the MI0283QT-11's:
     3.20 to the active area at the far end, 8.50 at the FPC end, 3.50 at the sides.
   - **Why the FPC exits at +X (BT830):** with the FPC on the left, the panel's metal frame would sit 19-20 mm from U12.
     The BT830 datasheet (§12.1.1) says any metal closer than 20 mm significantly degrades the antenna. With the FPC on
     the right it is 25.5 mm.
   - **Pin 1:** p.6's front view has pin 1 at the top with the FPC to the right, the same as Revision 1's MI0283QT-11
     analysis.
4. **FPC route: the factory 180° fold plus a Z-joggle through a board opening, into a connector on the back.**
   - The opening is x 118.25-133.05, y 47.55-94.45. That is unchanged: the ER-TFT028A2-4 tail has the same p.6 numbers
     as the MI0283QT-11's:
     - 26.70 ±0.50 long, 45.90 wide for the first 13.20, then a 25.50 ±0.10 tail;
     - a 5.50 stiffener;
     - 0.30 ±0.03 thick at the contacts;
     - 1.00 max for the fold and the component area.
   - **The 0.3 mm lift:** the drop to J3 becomes 2.445 at t = 1.2, which moves J3 (§3.3, §3.4).
5. **J3 = JUSHUO AFC07-S50ECA-00 (LCSC C262650; extended part; 17,479 in stock).**
   - 50 pins at 0.5 mm, top contact, slide lock, H 2.00, for a 0.30 mm FPC. It sits on B.Cu.
   - FPC pin n mates pad n, and pad 1 is at the top. Placement is in §3.4.
   - **Why top contact:** the contacts are on the viewing face (p.6 "CONDUCTOR" arrow to FRONT). After the fold and the
     joggle they face away from the board's back, which is the "top" of a connector mounted on the back.
6. **Board: 4 layers, 1.2 mm.** *(Revision 2; was 1.0)*
   - **Thickness: the cell, not the board, sets the back.** The plate is referenced to the board's top face, so the
     cell column (7.88 below the top) doesn't depend on t. The tallest back part, J8 (5.5), needs t + 5.5 + 0.3 clearance
     + 0.8 door:
     - at t = 1.2 that is 7.80, under 7.88, so 1.2 costs nothing;
     - at t = 1.6 it is 8.20, which would make J8 the limit and the device 12.8 mm.
     - 1.0 gains nothing over 1.2 and flexes 1.7 times as much.
   - **Posts:**
     - Without posts, a hard press bends a 1.2 mm board about 0.47 mm. That is (1.6/1.2)³ × the 0.2 mm measured at 1.6
       (DESIGN.md:97-99), against the switch's 0.2 ±0.1 travel and 0.3 hard stop (DESIGN.md:66). So every face switch
       gets a post: 8 in all.
     - With a flat 12.5 mm back, a post is a simple Ø4 × 5.0 stub from the back floor. Rev A already has the A and B
       posts.
   - **Other reasons for 1.2 over 1.0:**
     - no bonded panel stiffens the board any more;
     - board bow eats into the 0.3 mm panel gap (JLC's ±10 % thickness tolerance only moves the back, by ±0.12, inside J8's 0.3 clearance);
     - the narrow parts of the board need the strength: the 13 mm bridge over the notch, the 3.4 mm strip beside the
       opening, and the area round USB-C.
   - **Layers:** §6.4 counts the slots: there are at least 26 spare slots on the outer layers alone at every
     bottleneck, so 6 layers are not needed.
7. **Cell: Adafruit 258 (PKCELL LP-503562 + PCM), portrait, in a notch in the board left of J3.** *(Revision 2)*
   - **The cell:** 35 ±0.3 × 62 ±0.3 × 5.0 ±0.3 including the PCM (p.5), 1200 mAh. It has 100 mm UL1007 24 AWG leads
     to a JST PH 2-pin plug, with Adafruit polarity, which matches J8 pin 1 = BAT. Its ratings (p.2) are 1C (1.2 A)
     continuous discharge and 1C quick charge; standard charge is 0.2C.
   - **The notch:** x 66.10-102.40 (36.30 wide = 35.30 + 2 × 0.5), from y 45.55 down through the bottom edge.
     - **Why it's open at the bottom:** the cell's maximum footprint is x 66.60-101.90, y 45.85-108.15, which leaves
       only 0.31 to the shell wall at its bottom-left corner. A closed cut-out would leave a strip under 1 mm.
     - **Why y 45.55:** the top edge can't go any higher. The kept power section ends at y 45.02 (Q1).
   - **The plate:** a 0.5 mm SUS304 plate, 35.9 × 62.3, between the panel and the cell.
     - It sits in the notch like a window pane, its face 0.15 ±0.05 under the board's top face, which leaves 0.45 to
       the panel back.
     - **Location and ground:** 4 joggled tabs land on F.Cu GND pads (MP3-MP6) through Z-axis conductive tape. Two are
       at the top edge, on the top bridge; two are at the bottom corners, below the panel. The tabs set the plate's
       height from the board's top face, so the board's thickness tolerance doesn't matter.
     - **Support for front-ward loads:** the long edges are bonded to the notch walls with a structural epoxy fillet.
       Front-shell ribs press on the 4 tabs.
     - It weighs 9.2 g.
     - **Why:** a swelling cell can only push the door out, never into the LCD. A 20 N press on the door deflects the
       plate 0.16 mm or less, against a gap of 0.40 or more (§3.5).
   - **The leads:** they leave the PCM end at the top, behind the board, and run east in a lead pocket on the back of
     the top bridge to J8.
   - **The door:** flush with the back, 1.2 mm thick, about 55 × 71. It is deliberately the weak wall.
8. **Placement plan "R2" (§6):**
   - **Layout:**
     - The MCU, NOR and IMU go in the back of the right grip.
     - The FPGA and its LDO go on the back, above the FPC opening.
     - J8, the backlight block, J3, C52/C53, J1 and the debug pads run down the column between the notch and the
       opening, all inside the door.
     - The front keeps rev A's power section and grip parts. It gains only the USB ESD pair and C51/R31.
   - **Why, part 1:** the display bus stays at about 35-45 mm, away from the antenna.
   - **Why, part 2:** the BAT trunk shrinks to a few mm.
   - **Why, part 3:** everything you need for service is reachable through the door: the cell, J8, the J3 latch, SWD
     and the console.
   - **Why, part 4:** every gate keeps a slot margin on the outer layers (§6.4).
9. **SWD J1: a right-angle SMD 1 × 5, 2.54 mm header lying flat on the back,** HX PZ2.54-1x5P WT (C46061678;
   extended part; 15,802 in stock). *(Revision 2)*
   - **Size:** 2.5 mm insulator, 6 mm mating pins: 13.5 mm from the pad edge to the pin tips (footprint y −1.585 to
     +11.9 about the pad row).
   - **Position:** x 105.0-117.7, from y 88.2 (pad edge), inside the door, with the pins pointing +Y. The part itself
     ends at y 101.7.
   - **Open placement item (schematic review):** a standard 14 mm Dupont housing stops on the insulator and ends about
     19.9 mm past the pad row, at about **y 109.7**: at the board edge (109.5) and against the back shell's bottom wall.
     Revision 2's "housing ends at y 106" was wrong. At placement (§9 step 3), re-check the zone with a real housing:
     either plug the cable with the back shell off (bring-up is a bench job, and USB DFU covers updates), or find about
     4 mm in the column, for example by moving C52/C53 beside J3.
   - **Pinout:** unchanged ST-Link V2 order: 1 3V3, 2 SWDIO, 3 SWCLK, 4 GND, 5 NRST.
   - **Why not the alternatives:**
     - The vertical SMD header (C42453972, 8.5 tall, 338 in stock) can't fit: the back has 5.2 mm inside the door.
     - Rev A's THT header (C358687, 8.5) has the same problem, and its tails would poke out on the front under the
       panel.
     - A Tag-Connect TC2030 needs a cable you don't have.
     - Bare pads need pogo pins or soldered wires.
     - The right-angle header takes your existing Dupont ST-Link cable, sits 2.5 high, and with USB DFU it is only needed
       for bring-up.
10. **J8: same part (S2B-PH-SM4-TB, C295747, 39,055 in stock) and same nets (1 BAT, 2-4 GND), moved behind the
    charger.** *(Revision 2)*
    - **Position:** x 104.9-112.9, y 37.3-45.0 on the back, directly behind U9. Its mouth faces −X, toward the lead
      pocket.
    - **BAT path:** J8 pin 1 → at least 4 vias → U9 pins 2/3. That is about 2-5 mm instead of 60.
    - The BAT_SENSE divider R26/R27 and TP2 move here too, so only the high-impedance BAT/2 node runs to the MCU.
11. **Backlight: one resistor per anode from +5V_LCD, and a low-side switch on the shared cathode.**
    - Keep the MT3608 boost (U5) that makes +5V_LCD.
    - **R32-R35 = 120 Ω 1 % 0603** (0603WAF1200T5E, C22787, basic part, 2.23 M in stock), one per LED anode.
      *(Schematic review; Revision 1 had 100 Ω and Revision 2 110 Ω.)*
    - Q2 = AO3400A (C20917, basic part, 48 mΩ at 2.5 V), with its drain on LEDK.
    - R36 = 100 Ω (C22775) from PC6 to Q2's gate, and R37 = 100 kΩ (C25803) from the gate to GND.
    - **Current:** 14.9 mA per LED, 59.7 mA in total, typical: 85 % of the panel's 70 mA typical (p.11). The worst
      case is 73.4 mA (76.8 mA even at Vf 2.9 V), inside its 80 mA maximum; the minimum is 46.5 mA.
    - **Why 120 Ω:** with 110 Ω the worst corner is 80.1 mA once the resistor's −1 % is counted (83.7 mA at Vf
      2.9 V), at the limit, and 110 Ω is an extended part; with 100 Ω it reaches 87 mA (§4.3). The cost is about 8 %
      less typical brightness than 110 Ω.
    - **Why this circuit:** the four LEDs share one cathode, so a series boost LED driver cannot be used. Vf max (3.4 V)
      is above +3V3, and SYS falls to 3.0 V, so the 5 V boost stays.
12. **Swap the 10 kΩ reel on every 10 kΩ position.** Use C25804 (0603WAF1002T5E, 1 %, basic part) instead of C99198
    (RC0603JR, 5 %, extended part).
    - **Positions:** R1, R4, R5, R8, R10, R12, R22, R23, R31, and the new R38 and R39. For the nine reused positions
      this is a BOM-only change: the gb2b `0603WAF1002T5E` symbol points at rev A's own `easyeda2kicad:R0603`, so
      "Update PCB from Schematic" keeps their footprints (including R10, R22, R23 and R31 in the routed top bridge).
      Only the value and LCSC fields change.
    - **Why:** R10 sets +5V_LCD, which now drives the LEDs through fixed resistors. With a 5 % R10 the rail spans
      4.65-5.37 V; with 1 % it spans 4.81-5.18 V (MT3608 FB is 0.588-0.612 V, C84817.pdf).
13. **Straps (FPC side):**
    - IM0-IM3 go straight to GND.
    - VSYNC, HSYNC, DOTCLK, DE, DB17-DB8 and SDI go to GND.
    - RD goes to +3V3.
    - SDO is left open, and the four touch pins (44-47) are not connected; they are absent on the no-touch part.
    - **Why:** the ILI9341 §4 pin table says "D[17:0]: fix to VSS when not in use", "RDX: fix to VDDI" and "SDO: if not
      used, open". The panel's own note says "NC terminal should be open" (p.18).
14. **Panel power from +3V3:**
    - VDDI (pins 40, 41) and VCI (pin 42) go on +3V3, with C52 (10 µF, C1713) and C53 (100 nF, C14663) at the connector.
    - **Forced PWM is the hardware default** *(schematic review)*: R25 (100 kΩ) now pulls PS/SYNC **up** to +3V3
      (rev A pulled it down). The panel is on +3V3 permanently, so "forced PWM whenever the panel is powered" can only
      hold in hardware: with a pull-down, power-save ran through every window where PC3 isn't driven (reset, the ROM
      bootloader for a whole DFU session, a debugger reset). Firmware may drive PC3 low for power-save only where it
      accepts the 3.50 V.
    - **Why:** VCI is 2.5-3.3 V and VDDI 1.65-3.3 V, both 2.8 V typical with a 4.6 V absolute maximum (p.11). One rail
      also satisfies VDDI ≤ VCI (ILI9341 §18 note 2).
    - **Margin:** forced PWM holds 3.267-3.333 V (tps63021.pdf). That is 33 mV over the 3.3 V operating maximum in the
      worst corner, which is accepted.
    - **Power-save mode** would run up to 3.50 V, so it stays excluded. BOM.md #46 already requires forced PWM for the
      iCE40.
15. **Panel reset from the FPGA:**
    - LCD_RST gets R38, a 10 kΩ pull-up to +3V3.
    - New gateware drives RESX low for at least 10 µs once the supplies are up. It then waits at least 5 ms before sending
      commands and at least 120 ms before Sleep Out (ILI9341 §15.4).
    - **Why:** rev A relied on the 2090's own APX803 supervisor, and ppu_top.v has no reset output.
16. **U2 pin map:**
    - LCD_RST moves from U2.36 to U2.23 (IOT_37a).
    - LCD_TE goes on U2.44 (IOB_3b_G6).
    - U2.36 becomes spare.
    - **Why:** with U2 and J3 both on the back, J3's pin order then matches U2's pin order, so the bus fans out with no
      crossings:
      - J3: RST (10), DB7..DB0 (25-32), WR (36), DC (37), CS (38), TE (39).
      - U2: 23, 25-35, 37, 42, 43, 44.
    - **The brief named U2 pin 38 for TE.** That would put TE between WR (37) and DC (42), so it would cross DC and CS.
      Pin 44 keeps the order. Nothing needs migrating either way: no board .pcf exists yet.
17. **TE goes to the FPGA, not the STM32.** The FPGA owns the bus, so it can hold RAMWR until TE's rising edge with no SPI
    latency.
18. **NTC (R20): at the notch's top edge, on the back, beside the cell's PCM end.** *(Revision 2)*
    - **Position:** x 99.4-101.6, y 43.4-45.0, about 9 mm from U9 along CHG_TS.
    - **Why:** the notch leaves no board under the cell, so the NTC goes at the cell's edge. The KNTC0603 (C2892547) is
      0.8 tall, which is fine in the lead pocket.
19. **Board outline:** the draw_mech recipe, with:
    - **Kept:** the speaker notch, the 3 shell-screw holes and the FPC opening.
    - **Added:** the cell notch and 2 board-only screw holes, B1 and B2 (§3.6).
    - **Deleted:** rev A's battery cut-out and the 4 standoff holes for the 2090.
20. **Layer roles (changed):**
    - **In1:** solid GND, never cut.
    - **In2:** GND, except a +3V3 pour confined to the top bridge under the reused power section, so its kept +3V3 vias
      stay valid.
    - **+1V2:** a B.Cu pour round U2 and U3.
    - **+3V3:** 0.4 mm Rail-class tracks and pours.
    - **Why:** the MCU, the FPGA and J3 are now on the back. Their B.Cu fan-outs need a GND reference, which a +3V3
      plane is not (TI SPRAAR7 §2.4, as the EE study recommended). The capacity check (§6.4) doesn't need In2 for
      signals.
21. **USB data to the STM32 (new):**
    - J7's D+ (A6, B6) and D− (A7, B7) go to PA12 (U1.45, USB_FS_DP) and PA11 (U1.44, USB_FS_DM). Pin numbers are from
      Table 8.
    - U13 is ST's USBLC6-2SC6 (C7519; extended part; 33,736 in stock), at J7.
    - D4 is an SMF5.0A 5 V TVS on VBUS (MDD, C193402; extended part; 559,541 in stock), at J7.
    - **VBUS limit (accepted):** D4 (VBR 6.4 V minimum, 1 W steady state) and U13's VBUS pin (about 6 V) now cap
      sustained VBUS near 6 V, below the BQ24073's 6.6 V OVP. A source stuck above about 6.4 V would burn D4 (§4.7).
    - Route as a 90 Ω pair on F.Cu over In1, about 55 mm; full speed is tolerant. The OTG FS block has its pull-ups
      inside (§3.27), so no external resistor is needed.
22. **DFU entry (new):**
    - **Normal path:** a firmware jump to the ROM bootloader at 0x1FFF 0000 (memory map; AN2606), triggered by
      Select + Start (both on the U11 expander) held at power-up, or by a USB command (§4.7).
    - **Recovery:** bridge the BOOT0 pad TP8 (in the door) to +3V3 (TP5 or J1 pin 1), then cycle SW1 or pulse NRST
      (J1 pin 5). R1 still pulls BOOT0 low.
    - **BOOT1 strap (schematic review):** BOOT1 is PB2 (STM32 pin table, LQFP64 pin 28), and BOOT0 = 1 with BOOT1 = 1
      boots SRAM. Rev A had FPGA_CDONE on PB2: it reads high once the FPGA is configured, and an MCU reset doesn't
      reset the FPGA, so BOOT0 + NRST booted SRAM (only a power cycle worked, while the iCE40 held CDONE low). Rev B
      moves CDONE to the spare PB0 (U1.26) and adds **R39, 10 kΩ from PB2 to GND** (C25804, the 10 k reel), so BOOT0 = 1
      always selects the ROM. board.dts `cdone-pin` becomes 0 (§4.9).
    - **Fallbacks:** the same ROM also serves USART1 on TP11/TP12, and SWD stays on J1.
23. **Ground the panel frame and the plate (new):**
    - **Panel frame:** two exposed ENIG GND pads under the panel, MP1 and MP2 on F.Cu, touched by conductive foam pads
      on the panel's metal back. Before relying on them, check with a meter that the back is bare metal.
    - **Plate:** its 4 tabs land on F.Cu GND pads MP3-MP6 through Z-axis conductive tape. Two copper-foil bridges at the
      middle of the long edges (y ≈ 76.9) land on the F.Cu GND pads **MP7** (left strip) and **MP8** (centre column),
      and cut the longest ungrounded run below λ/4 at 2.4 GHz (§3.5).
24. **IMU placement rule (new):**
    - U7 sits at least 10 mm from every screw, post, board-only screw and opening corner, and away from the LRA and the
      speaker.
    - Its block is on the back of the right grip, at x 144.0-149.6, y 36.2-41.6:
      - 10.8 mm from S2;
      - 21.6 mm from the A post;
      - 30 mm from the speaker's edge.
    - Its axes are mirrored, which board.h must remap.
25. **Where the work lives:**
    - **Board:** `projects/gameboy-v2/pcb/gb2b/`, already created as a copy of `gb2/`.
    - **Libraries:** new symbols and footprints go in `gb2b/lib/`, never in `cad/symbols/`.
    - **Enclosure:** the rev B enclosure starts from `enclosure/variants/thin-stack/`, rebuilt for the flat-back stack
      (§7).

---
## 3. Mechanical

### 3.1 Z stack per region (t = 1.2; heights above (+) or below (−) the board's top face) *(Revision 2)*

The front face and the back face are each one flat plane across the whole device. Every region fits between them.

**Front (the same everywhere):**

| Layer, front to back | Glass (production) | Acrylic (prototype) | Source |
|---|---|---|---|
| Lens, flush with the front face | 1.10 (+3.515 to +4.615) | 1.50 (+3.715 to +5.215) | ME study (glass); params.py:34 (acrylic) |
| Gasket ring, black closed-cell foam, ±0.05 | 0.30 | 0.50 | ME study |
| Panel, ER-TFT028A2-4 | 2.80 | 2.80 | p.6 side view |
| Panel back tape (budgeted; may be inside the 2.80) | 0.115 | 0.115 | p.6 "Double-sided adhesive (T=0.115mm)" |
| Gap to the board's top face (no parts there) | 0.30 | 0.30 | locked |
| **Front face** | **+4.615** | **+5.215** | |

**Back:**

| Region | Column, front to back (below the board top) | Total | Source |
|---|---|---|---|
| **Cell (in the notch)** | plate recess 0.15 · SUS304 plate 0.50 · polyimide film 0.05 · stretch-release tape 0.15 · cell 5.30 max · free swell gap 0.53 (10 % of 5.30) · door 1.20 | **−7.88** | PKCELL p.5 (5.0 ±0.3); swell 10 % (brief) |
| J8 (in the door area) | board 1.20 · J8 5.50 · clearance 0.30 · door thinned to 0.80 over J8 | −7.80 | JST p.3; JLC attribute "Z-height 5.5 mm" |
| Other door-area parts | board 1.20 · parts ≤ 5.18 · clearance 0.30 · door 1.20 | −7.88 | |
| Grips, top and bottom bands (fixed back shell) | board 1.20 · parts ≤ 4.78 · clearance 0.30 · floor 1.60 | −7.88 | floor as rev A (DESIGN.md:18) |
| **Back face** | | **−7.88** | |

**Totals:**
- **Glass:** 4.615 + 7.88 = **12.495 mm**.
- **Acrylic:** 13.095 mm.
- **At t = 1.6:** 12.815 mm (J8's column becomes 8.20). This is one reason for 1.2 (§2 decision 6).
- **At t = 1.0:** 12.495 mm, the same as 1.2.

**Against the ~12.2 mm quoted earlier:** that figure left out three things:
- the 0.15 plate recess, which covers the board's ±10 % thickness;
- the 0.05 insulating film between the plate and the PCM;
- the 0.115 panel tape allowance.

**Trim options, all to test before adopting:**
- a 1.0 mm door saves 0.2;
- peeling the panel tape, if the 2.80 turns out to exclude it, saves 0.115.

**Why the plate sits at the board's top face:**
- The panel side of the plate is the same plane the panel already clears by 0.3. So the cell sinks a full board
  thickness into the notch, and the column is independent of t.
- The plate's tabs sit on the board's top face, so the plate face is 0.15 ±0.05 (forming) under the top, whatever the
  board's ±10 % thickness. The panel-to-plate gap is 0.40-0.50 (plus the tape allowance).
- The front shell clamps the board by its top face (DESIGN.md:86), so the 0.3 panel gap doesn't depend on the board's
  thickness either.

**Height limits that follow:**
- **Front, under the face plate** (1.6 thick, inner face at +3.015): parts ≤ 2.7.
  - Where the plate is pocketed down to 0.95, parts can be ≤ 3.35. Pockets are needed over:
    - J7 (3.25) and L2 (3.1);
    - J5 and J6 (3.0);
    - the LRA (3.2);
    - SW10-SW13 (3.35, at the rim notches).
  - Everything else on the front is ≤ 2.1: D3 is 2.1, U4 has moved to the back, and C27/C28 are 1.3.
- **Front, under the lens land** (the 2.5 mm ring round the panel pocket): parts ≤ 2.0, except a land relief over L2.
  - Over L2 there is then 0.4 from L2's top to the glass, and 0.6 with acrylic.
- **Front, under the panel pocket:** 0. Only mask, tented vias, the MP1/MP2 frame-contact pads and the MP7/MP8
  plate-bridge pads are allowed there.
- **Back, outside the door:** parts ≤ 4.78. U12 is 1.7 and the LQFP64 is 1.6 max (STM32 Table 82).
- **Back, inside the door:** parts ≤ 5.18. J8 (5.5) is covered by the door's 0.8 local thinning.
- **Speaker** (Ø27 × 4.9, sealed 0.3 under a 1.6 plate): it spans +2.715 to −2.185. It hangs 0.985 below the board's
  back face in the notch, 4.1 above the back floor's inner face (−6.28).

### 3.2 Panel position (KiCad mm)

| Feature | x | y | Note |
|---|---|---|---|
| LCD active area, 57.60 × 43.20 | 66.45-124.05 | 49.40-92.60 | p.6 borders: 3.20 at the far end, 8.50 at the FPC end, 3.50 at the sides |
| Visual area, 59.20 × 44.80 | 65.65-124.85 | 48.60-93.40 | p.6 |
| LCD glass, 64.75 × 46.20 | 64.95-129.70 | 47.90-94.10 | p.6: 1.70 from the far module edge |
| Module, 69.30 × 50.20 | 63.25-132.55 | 45.90-96.10 | Module centre (97.90, 71.0) |
| FPC fold bulge (1.00 max) | 132.55-133.55 | 48.05-93.95 | p.6 folded view |
| Front-shell panel pocket = panel + fold + 0.3 | 62.95-133.85 | 45.60-96.40 | Pocket walls locate the panel to about ±0.15 |
| Mask window = active area + 0.4 per side, 58.4 × 44.0 | 66.05-124.45 | 49.00-93.00 | Stays 0.4 inside the visual area |
| Gasket ring | on the module's border, outside the visual area | | About 1.8 wide on 3 sides, 2 pads at the FPC end, none over the COG driver |
| Lens land ring, 2.5 wide | 60.45-136.35 | 43.10-98.90 | |
| Lens, centred option 82.2 × 56 | 54.15-136.35 | 43.0-99.0 | Off-centre option: 75.9 × 55.8 over the land ring |

**Clearances to the panel** (measured on rev A's bounding boxes):
- SW5 (D-pad right): 11.8.
- Start/Select: 10.3.
- B (SW7): 6.9 to the fold.
- Speaker: 4.40 to the fold.
- J7: 6.6.
- U12: 25.5. The BT830 datasheet asks for 30 mm left/right and says under 20 mm significantly degrades.

**What's under the panel:** with the notch cut out, the board under the panel is only two areas:
- the left strip, x 63.25-66.10;
- the centre column, x 102.40-118.25.

Both carry routing and GND pads only.

### 3.3 FPC path

**Unfolded** (p.6):
- **Exit:** from the 50.2 edge, centred; 26.70 ±0.50 from the module edge to the tail end.
- **Wide part:** 45.90 wide for the first 13.20, with 4.5 × 45° chamfers. Its component area is on the back face, 1.00
  max including the FPC.
- **Tail:** single layer, 25.50 ±0.10 wide and 13.50 long. A stiffener covers the last 5.50. The tail is 0.30 ±0.03
  thick at the contacts.
- **Contacts:** 50 at 0.5 pitch, 0.35 wide and 3.50 long, on the **viewing face** (the side view's "CONDUCTOR" arrow
  points to FRONT).
- **Assumption:** the drawing gives no folded tail-end dimension. Revision 1's MI0283QT-11 "(25.00)" is assumed, since
  the two FPCs share every unfolded number. The fit test confirms it.

**Folded** (the maker's 180° fold):
- **Bulge:** 1.00 max beyond the edge.
- **Component area:** with its components toward the panel's back, it takes u ≈ 0-11.5 and hangs in the board opening,
  below the board's top face.

**Joggle and route to J3:**
- **Joggle:** an S-bend in the single-layer tail, between u ≈ 12.3 and 14.3. It passes the opening's inboard edge at
  x 118.25 (u 14.3).
  - **Drop:** h = t + 0.3 + 0.115 − 0.05 + 0.88 = 2.445 [2.845]. That is from the tail, lying against the panel back, to
    the J3 slot centre, 0.88 below the board's back.
  - **Radius:** r ≈ 1.0 (0.6-1.6 is usable). The datasheet gives no bend radius; it says "do not forcibly pull or bend
    the cable" (p.18).
  - **Length used:** 1.59 [1.99].
  - **Where to bend:** only in the single-layer zone, u 11.5-17.9. Never bend the component area or the stiffener.
- **Under the board:** the tail and the stiffener (u 17.9-23.4) lie flat against the back, with the contacts facing away.
- **FPC end:** u ≈ 23.41 (x 109.14) [u 23.01, x 109.54]. It enters J3 2.84 behind the slider face.
- **If the tail rides at component height** instead of against the panel back, h is about 0.85 smaller. J3 then moves
  0.8 mm west (§3.4 range). The fit test decides.

**Contact-side chain:**
1. Unfolded, the contacts are on the viewing face.
2. After the 180° fold, they face the board.
3. After the Z-joggle (two opposite bends), they face −Z, away from the board's back.

So a **top-contact** connector on the back is right.

**Pin chain:**
- p.6's front view has PIN 1 at the top and pin 50 at the bottom, with the FPC to the right. The fold and the joggle bend
  about axes parallel to Y.
- So **FPC pin n mates pad n**, with pin 1 at the top (y 58.75) and pin 50 at the bottom (y 83.25).
- This assumes the tail is centred on the module's centre line. Check it on the sample.

**Tolerance:**
- The FPC length is ±0.5. The 50-circuit housing is DIM E 5.75 against 5.20 for the ≤34-circuit drawing, so its stop
  could be up to 0.55 deeper.
- In Revision 2 two more tolerances add: the panel is located by the shell (±0.15) and the board by its locators (±0.2).
- The joggle radius and the fold bulge absorb only about ±0.6, so measure the real parts before freezing J3 (§9 step 2).

**Assembly:**
1. In the face-down front shell: the lens, then the gasket, then the panel with its tail lifted.
2. Fit the caps, D-pad, edge caps, slider, speaker and light pipe.
3. Lower the board, threading the tail down through the opening. Fit the B1 and B2 screws.
4. On the back, open J3's slider, insert the tail and close the slider. The front module can now be powered over USB
   and tested.

**Rework:** with the door open, the slider is reachable from the back. The panel comes out of the front side once the
board is lifted.

### 3.4 J3 placement (for place.py)

The footprint is `FPC-SMD_AFC07-S50ECA-00` (easyeda2kicad from C262650). Its local frame:
- pads 1-50 are 0.30 × 1.50 at y −1.08, with pad 1 at x −12.25;
- the nails, pads 51 and 52, are 2.2 × 3.3 at (±13.85, +1.09);
- the 3D body runs from y −1.575 (the tails) to +4.175 (the latch face) and is 2.00 tall.

Revision 1 checked this frame in an in-memory pcbnew board: the footprint goes on B.Cu with `Flip(pos, False)`, then
`SetOrientationDegrees(270)`. The origin follows from the FPC end: origin x = FPC end − 1.335, and the pad row is at
origin − 1.09.

| Case (t = 1.2) | Origin (x, y) | Rot | Pad row x | Body x, latch closed / open | FPC end x |
|---|---|---|---|---|---|
| **Baseline: r 1.0, tail on the panel back** | **(107.80, 71.00)** | 270 | 106.71 | 106.23-111.98 / 113.23 | 109.14 |
| r 0.6 / r 1.6 | (108.15 / 107.37, 71.00) | 270 | 107.06 / 106.28 | | 109.48 / 108.71 |
| Tail at component height, r 1.0 | (106.99, 71.00) | 270 | 105.90 | 105.42-111.17 / 112.42 | 108.33 |
| [t = 1.6, r 1.0] | (108.20, 71.00) | 270 | 107.11 | | 109.54 |

- **Pad position:** pad n sits at y = 58.75 + 0.5(n − 1). The nails are at x = origin + 1.09, y 57.15 and 84.85.
- **Room for the range:** across the whole range (origin 106.79-108.15), the pads stay at least 2.5 from the
  notch edge (x 102.4) and inside the column.
- **Fit-test correction:** the fit test (§9 step 2) moves the origin's x by (measured − predicted) FPC-end position.
- **Fan-out:** the vias for the bus and for the GND pins go **east of the pad row, under the connector body**, tented. Keep
  the west side free, toward the notch and the epoxy fillet.
- **Check after placing:**
  - J3 is on B.Cu.
  - pad("1").y < pad("50").y.
  - The pads' x is less than the body centre's x.
  - The latch faces +X, toward the opening, in a 3D render of the back.

### 3.5 Battery, plate, door and service access *(Revision 2)*

**Cell:** Adafruit 258 = PKCELL LP-503562 with PCM.
- **Size:** 35 ±0.3 × 62 ±0.3 × 5.0 ±0.3 including the PCM (p.5), so 35.3 × 62.3 × 5.3 at most. 1200 mAh.
- **Leads:** UL1007 24 AWG, 100 mm, out of the PCM end, to a JST PH 2-pin plug. Adafruit polarity matches J8 pin 1 = BAT,
  but check it with a meter before the first plug-in.
- **Ratings (p.2):**
  - 1C (1.2 A) maximum continuous discharge.
  - Charge at 0.2C standard, 1C quick. The board's ISET gives about 445 mA, which is 0.37C (BOM.md #41).
  - Rev A's power budget allows 1.5 A peaks (route/PLAN.md); see §8.2.
- **Position:** portrait, PCM end at the top.
  - Maximum footprint: x 66.60-101.90, y 45.85-108.15.
  - It sits 0.5 from the notch's long edges and 0.3 from its top edge.
  - It is 0.31 from the shell wall at its bottom-left corner and 1.75 at the bottom-right.
  - Its front face is at −0.85: the plate recess 0.15, the plate 0.50, the film 0.05 and the tape 0.15.

**Notch** (Edge.Cuts): x 66.10-102.40, from y 45.55 through the bottom edge.
- The two inner corners at the top are R1.0, for stress relief and a standard router bit.
- The top edge is 0.53 below Q1's courtyard (y 45.02), the lowest front part of the kept power section.

**Plate:** 0.5 mm SUS304, laser-cut and joggled.
- **Size:** a 35.9 × 62.3 body at x 66.30-102.20, y 45.75-108.05. The bottom-left corner is R3, to clear the wall.
- **Tabs:** 4 tabs joggled up by 0.70, so their undersides sit on the board's top face. Each lands on an exposed ENIG GND
  pad through Z-axis conductive transfer tape (about 0.05):
  - MP3 at x 68.0-78.0, y 43.3-45.05, between R10/C27 and L2;
  - MP4 at x 93.3-96.5, y 43.3-45.05, between U13 and Q1;
  - MP5 at x 62.8-65.6, y 98-106 (bottom left);
  - MP6 at x 102.9-106.0, y 98-106 (bottom right).
  - All four are outside the panel pocket (y < 45.6 or y > 96.4). The tabs are 0.55 tall, under the 2.0 limit of the
    lens land and the front plate.
- **Why tabs on the front face:** they reference the plate to the top face, so the board's ±0.12 thickness tolerance
  drops out. Each joggle's leg stays in front of the cell's plane (z ≥ −0.65 against −0.85), so it needs no side
  clearance.
  - Flanges under the board's back would need a leg through the cell's plane. That means either 1.4 per side, which
    doesn't fit between the wall and J3, or a 0.7 recess, which adds 0.55 mm overall (the fallback, §8.1).
- **Support:**
  - **Front-ward loads** (a door press through the cell, or swelling) go two ways:
    - into the front shell, through ribs that press on the 4 tabs (from the lens land at MP3/MP4, and from the face
      plate at MP5/MP6);
    - through a structural-epoxy fillet that bonds each long edge to its notch wall. The fillet is on the back side,
      inside the 0.5 gap beside the cell.
  - **Backward loads** go through the tabs into the board.
  - **Stiffness:** with both long edges held, a 20 N line load mid-span bends the plate 0.159 mm, against the 0.40 or more
    to the panel (`stack.py`). The tabs alone are not stiff enough, so the edge bond is required.
- **Insulation:** a 0.05 polyimide film covers the cell-facing side, so the PCM's B+ can never touch the grounded plate.
- **Grounding:**
  - The 4 tab pads connect to GND, each with at least 4 vias to In1.
  - Two copper-foil conductive-adhesive bridges, at the middle of the long edges, join the plate face to two exposed
    ENIG GND pads under the panel. They are 0.07 thick, inside the 0.45 gap. The pads are centred on the plate's
    long-edge midpoint, y ≈ 76.9 (the plate spans y 45.75-108.05):
    - **MP7**, 2.0 × 8.0, at x 63.6-65.6, y 72.9-80.9 on the left strip (0.5 from the notch edge at x 66.10), 4.9 below
      MP1;
    - **MP8**, 1.6 × 8.0, at x 102.9-104.5, y 72.9-80.9 at the west edge of the centre column. The F.Cu bus corridor
      therefore starts at x 105 (§4.5).
    - Each gets at least 3 GND vias to In1.
  - The longest ungrounded edge is then about 29 mm (from the plate's top corner past MP3 down to MP7/MP8), under λ/4
    (31 mm) at 2.4 GHz.
- **Mass:** 9.2 g.
- **Distance to the BT830:** the plate is 31.4 mm from the antenna keep-out and 28.5 mm from U12. The panel frame
  (28.4 / 25.5) stays the nearest metal, so the plate adds nothing closer.
- **Mounting the cell:** stretch-release tape (about 0.15; for example tesa 70415-class) on the plate's back face, in a
  printed jig.

**Door:** flush with the back face, 1.2 mm PETG.
- **Outline:** about x 63.4-118.7, y 36.8 to the bottom wall. It steps to y 47.5 for x > 113.3, round the FPGA block,
  so it is about 55 × 71.
- **Retention:** hooks along the top edge and a snap at the bottom, GBA-style. Optionally 2 M2 screws into back-shell
  bosses at the lower corners, placed in the enclosure step.
- **Locally thinned to 0.8** over J8 (x 104.9-112.9, y 37.3-45.0). No ribs over the cell, so the 0.53 swell gap stays
  free.
- **It is the deliberately weak wall:** a swelling cell bulges the door outward (which also warns you), never into the
  LCD.

**Lead pocket:** the back of the top bridge, x 66.6-104.6, y 37.0-45.55. It is 5.5 deep, from the board's back to the
door's inner face.
- The leads leave the PCM end behind the board's back plane and run east to J8's mouth, at x 104.9. That is about 25 mm;
  the other ~75 mm folds in S-loops in the pocket.
- **Optional:** shorten the leads to about 35 mm and re-crimp them (JST SPH-002T-P0.5S + PHR-2).
- **Pocket rules:** only parts ≤ 1.0 there, nothing sharp. The test pads TP1/TP3/TP4/TP5 may stay under the loops.

**Service with only the door open:**
- **Cell:** unplug J8, pull the stretch-release tab, lift the cell out.
- **Screen cable:** J3's slider and the whole FPC sweep are inside the door outline (x 106.2-118.25).
- **Firmware:**
  - J1 takes the Dupont ST-Link cable. The housing lies flat along +Y and ends at about y 109.7, at the board edge, so
    plug it with the back shell off, or with the door open and the leads bent out of the door right at the housing.
    Its zone is re-checked at placement (decision 9).
  - BOOT0 (TP8), the console (TP11, TP12) and GND (TP10) sit in the row below J1.
  - In normal use, firmware updates go over USB-C, with the device closed.
- **Measurements:** the rail pads are reachable:
  - TP1 (VBUS), TP3 (SYS) and TP5 (+3V3) in the lead pocket;
  - TP2 (BAT) at J8;
  - TP4 (GND) and TP6 (+5V_LCD), moved into the pocket.
  - TP7 (+1V2) sits by the LDO, outside the door, and needs the back shell off.

**Board out:** door off, then unplug J8 and lift the cell. Remove S1-S4 and the back shell, open J3, remove B1/B2, and lift
the board. The plate stays on the board, and the panel stays in the front shell.

### 3.6 Board outline (Edge.Cuts)

- **Outline:** the draw_mech recipe: the silhouette inset by 2.5 mm, minus the speaker notch. It spans x 22.5-167.3,
  y 32.5-109.5, and comes to 10 079 mm² before cut-outs.
- **Speaker notch:** keep it. The edge is at x 136.45 below y 70.1, with an arc to (151.4, 70.1).
- **FPC opening:** an internal cut-out at x 118.25-133.05, y 47.55-94.45 (14.8 × 46.9, 694 mm²), with inside corners R0.5
  and a 0.5 mm copper keep-out round it.
- **Cell notch (new):** x 66.10-102.40 from y 45.55 to the edge (2307 mm²), top corners R1.0. Its long walls get the
  plate's epoxy fillet, so keep 0.5 of copper-free board along them, the normal edge rule.
- **Delete:** rev A's battery cut-out, and the four Ø3.2 standoff holes for the 2090 at (57.15 / 133.35, 42.425 / 99.575).
- **Keep:** the three Ø2.4 shell-screw holes at (57.39, 35.47), (133.19, 35.45) and (29.24, 91.91), each with a 3 mm
  copper keep-out radius. The fourth screw stays off-board at (140.5, 100.5).
- **Add (new):** two Ø2.4 board-only screw holes into the front shell, B1 (55.0, 101.5) and B2 (126.5, 103.0), each with
  a 3 mm keep-out (ME study).
  - **Why:** with the notch, the board is two lobes joined only by the 13 mm top bridge. Each lobe now has its own
    screws: S1, S3 and B1 on the left; S2 and B2 on the right.
  - B1/B2 also hold the board while J3 is connected, before the back shell goes on.
- **Result:** 6185 mm².
- **3.4 mm strip:** the board strip between the opening and the speaker notch (x 133.05-136.45, y 70.1-94.45) gets no
  parts; routing is allowed.
- **Top bridge:** the 13 mm strip over the notch (x 66.1-102.4, y 32.5-45.55) carries the reused power section on the
  front and the lead pocket on the back. Panelize with tabs on both lobes, never only on the bridge.

### 3.7 Keep-outs and height limits (KiCad mm; heights from the board face on that side)

| Zone | Side | Bounds | Rule |
|---|---|---|---|
| Panel pocket + fold | F | x 62.95-133.85, y 45.60-96.40 | **No parts.** Mask, tented vias, no test points, no silk over the pads. Exposed copper only on MP1/MP2 (frame contacts) and MP7/MP8 (the plate-bridge GND pads) |
| Plate tabs MP3-MP6 | F | §3.5 | GND pad only, parts ≥ 0.5 away |
| Lens land ring | F | x 60.45-136.35, y 43.10-98.90, minus the pocket | Parts ≤ 2.0; L2 (3.1) gets a land relief |
| Front under the face plate | F | everywhere else | Parts ≤ 2.7; ≤ 3.35 under a local plate pocket (J7, L2, J5, J6, LRA, SW10-SW13) |
| D-pad pivot | F | Ø10.6 round (39.95, 64.09) | Plain GND pour, no pads (DESIGN.md:73-75) |
| Cell notch | all | x 66.10-102.40, y ≥ 45.55 | Cut-out |
| Lead pocket | B | x 66.6-104.6, y 37.0-45.55 | Parts ≤ 1.0, no sharp parts; test pads allowed |
| J3 body, slider open, over the whole fit range | B | x 105.2-113.6, y 55.6-86.4 | J3 only |
| FPC sweep (tail and stiffener against the back) | B | x 112.0-118.25, y 57.25-84.75 | No parts and no test pads; mask only |
| J1 with its mated Dupont housing | B | x 105.0-117.7, y 88.2-109.7 (J1 to 101.7, a 14 mm housing to about 109.7, at the board edge) | J1 only, plus the flat debug pads under the housing. Open: re-check with a real housing at placement (decision 9) |
| Inside the door, elsewhere | B | door outline (§3.5) | Parts ≤ 5.18; J8 ≤ 5.5 under the 0.8 door patch |
| Outside the door (grips, top band, below the opening) | B | - | Parts ≤ 4.78 |
| Face-key post zones, Ø5 | B | SW2 (39.95, 56.09), SW3 (39.95, 72.09), SW4 (31.95, 64.09), SW5 (47.95, 64.09), SW8 (49.49, 83.82), SW9 (49.44, 91.82), SW6/A (156.88, 61.92), SW7/B (143.94, 66.25) | No parts. All 8 are needed at t = 1.2 |
| FPC opening | all | x 118.25-133.05, y 47.55-94.45 | Cut-out, plus a 0.5 copper keep-out |
| BT830 antenna | all | x 25.2-34.9, y 38.6-52.4 | No copper (rev A zone) |
| Screws S1-S3, B1, B2 | all | r 3 round each hole | No copper |
| IMU zone | B | ≥ 10 mm from every screw, post, B1/B2 and opening corner | U7 only (decision 24) |

**Rev A footprints that collide with the Revision 2 zones** (`check.py`):
- **Front, under the panel pocket (22):** C11 C24 C31 C32 C51 D2 J4 J8 R3 R5 R6 R7 R8 R12 R13 R14 R31 U1 U2 U4 U7 Y1.
  - C51 and R31 are also in the notch + 0.5. Move them to x 102.5-104.4, y 40.4-45.0.
- **Back, in the notch + 0.5 (29):** C1-C10 C13 C16 C17 C21 C22 C23 C45 FB1 R1 R4 R26 R27 TP7-TP12 U3.
- **In the FPC opening:** J4 and rev A's J8.
- **Moved with their blocks, though not colliding:**
  - C12 and R29 (front, with the MCU and U2);
  - the U2 ring C14 C15 C18 C19 C20 FB2 (back);
  - C25 and C26 (repurposed);
  - J1;
  - R20 (FPC sweep);
  - TP2.
- **Front, taller than 2.7 and staying:** J5 and J6 (3.0), J7 (3.25), L2 (3.1), SW10-SW13 (3.35). All get plate pockets
  (§3.1).
- **Kept parts:** none collides with the new zones. One known exception: SW2's post is 0.17 from U12 (§3.8).

### 3.8 Board thickness and posts

- **Posts:** Ø4 stubs from the back floor's inner face (−6.28) to 0.05 under the board, about 5.0 long. There is one
  under each face switch, 8 in all. Rev A already has the A and B posts (DESIGN.md:98-99).
- **Why:** without posts, a hard press bends a 1.2 mm board about 0.47 mm, against the switch's 0.2 ±0.1 travel and its
  0.3 hard stop (DESIGN.md:66). The 0.47 is the 0.2 measured at 1.6 × (1.6/1.2)³.
- **SW2's post:** it comes within 0.17 mm of U12's bounding box (x ≤ 37.78), so make it Ø3.5 or move it +0.5 in x.
- **The notch and the board's strength:** the left lobe hangs on the 13 mm top bridge until the board is screwed in.
  Handle and ship the bare board on a carrier. Once it is in the shell, each lobe is screwed on its own (§3.6).
- **Thickness tolerance:**
  - The front shell's bosses clamp the board's top face (DESIGN.md:86). So the board's ±10 % only moves the back, and
    the plate (referenced to the top face) and the panel gap are unaffected.
  - Per STRUCTURAL_REVIEW #4, shorten the back boss tips and clamp through a compliant washer.

---
## 4. Electrical

### 4.1 FPC pinout and rev B nets (J3; panel pin names from p.9)

| FPC pin | Panel signal | Rev B net | Connects to | Rule / reason |
|---|---|---|---|---|
| 1 | LEDK | LCD_LEDK | Q2 drain | Common cathode of the 4 LEDs (p.6 LED backlight circuit) |
| 2-5 | LEDA1-LEDA4 | LCD_LEDA1-4 | R32-R35 → +5V_LCD | One anode per LED |
| 6-9 | IM0-IM3 | GND | - | 0000 = 8080 MCU 8-bit bus I (p.10) |
| 10 | RESET | LCD_RST | U2.23, and R38 10 k to +3V3 | §4.4 |
| 11-14 | VSYNC, HSYNC, DOTCLK, DE | GND | - | ILI9341 §4: "fix to VDDI or VSS when not in use" |
| 15-24 | DB17-DB8 | GND | - | ILI9341 §4: "fix to VSS level when not in use" |
| 25-32 | DB7-DB0 | LCD_D7-LCD_D0 | U2.25, 26, 27, 28, 31, 32, 34, 35 | Same nets as rev A |
| 33 | SDO | NC | - | "If not used, open this pin" |
| 34 | SDI | GND | - | "If not used, fix this pin at VDDI or VSS" |
| 35 | RD | +3V3 | - | "Fix to VDDI level when not in use" (write-only bus) |
| 36 | WRX (D/CX) | LCD_WR | R29 33 Ω ← U2.37 (LCD_WR_FPGA) | The serial-mode alias in brackets is unused |
| 37 | D/CX (SCL) | LCD_DC | U2.42 | |
| 38 | CS | LCD_CS | U2.43 | |
| 39 | TE | LCD_TE | U2.44 | §4.5 |
| 40, 41 | VDDI | +3V3 | C52, C53 | §4.2 |
| 42 | VCI | +3V3 | | |
| 43, 48, 49, 50 | GND | GND | | |
| 44-47 | X+, Y+, X−, Y− | NC | - | No touch panel on this variant; p.18 says "NC terminal should be open" |
| 51, 52 | Fitting nails | GND | | |

- **Pinout check:** pins 1-43 and 48-50 match the Adafruit 2090's DT280QV10 connector and the MI0283QT-11, which
  Revision 1 checked against `cad/docs/adafruit-2090-cap-touch-revD.sch`.
- So the J3 netlist rows of Revision 1 carry over unchanged.

### 4.2 Panel power

- **Range:** VCI is 2.5-3.3 V and VDDI 1.65-3.3 V, both 2.8 V typical with a 4.6 V absolute maximum (p.11). VDDI ≤ VCI
  (ILI9341 §18 note 2). One +3V3 rail satisfies all of this.
- **Load:** no operating current is given; standby is 100 µA max (ILI9341 §18.2.1). Estimate a few mA, which is
  negligible for the TPS63021.
- **Forced PWM:** R25 pulls BB_PSYNC (PC3) up, so the TPS63021 is in forced PWM (3.267-3.333 V) whenever PC3 isn't
  driven, and firmware drives it high while running (decision 14). The panel and the FPGA are on +3V3 from power-up,
  so there is no "panel off" state in which power-save would be harmless. The only remaining power-save windows are the
  TPS63021's own start-up (PS/SYNC follows +3V3 up through its 1.2 V threshold) and any time firmware deliberately
  drives PC3 low. Also, per p.18, never drive the bus while VDDI is off. The FPGA and the panel share +3V3, so this
  holds by construction.
- **Decoupling:**
  - C52 (10 µF 0805, C1713) and C53 (100 nF, C14663) go on the back, just below J3, at x 104.9-110.6, y 86.6-88.0.
  - Pins 40-42 (y 78.25-79.25) also via to the +3V3 pour at their pads.

### 4.3 Backlight *(schematic review: 120 Ω, C22787)*

**Circuit:**
- **Supply:** +5V_LCD comes from U5 (MT3608; R9 = 73.2 k, R10 = 10 k, giving 0.6 × 8.32 = 4.99 V).
- **Anodes:** R32-R35 (120 Ω 1 %, 0603WAF1200T5E, C22787, basic) feed LEDA1-4.
- **Cathode:** LEDK goes to Q2's drain (AO3400A), and Q2's source goes to GND.
- **Gate drive:** PC6 drives the gate through R36 (100 Ω), and R37 (100 k) pulls the gate to GND.

**Panel spec** (p.11): "4-chip Parallel", forward current 70 mA typical and 80 mA max in total; Vf 3.2 V typical and
3.4 V max at 80 mA. No minimum Vf is given, so 3.0 V is assumed below (and 2.9 V as a check: the real Vf at 15-20 mA
per LED sits a little below the 80 mA figures).

| Current per LED (mA), 120 Ω nominal | +5V_LCD 4.81 V (min) | 4.99 V (nominal) | 5.18 V (max) |
|---|---|---|---|
| Vf 3.4 (max) | 11.7 | 13.3 | 14.8 |
| Vf 3.2 (typical) | 13.4 | **14.9** (59.7 mA in total) | 16.5 |
| Vf 3.0 (assumed min) | 15.1 | 16.6 | 18.2 (72.7 mA in total) |

- **Corners including the resistors' ±1 %:** worst case 4 × (5.18 − 3.0) / 118.8 Ω = **73.4 mA** (76.8 mA at Vf 2.9);
  minimum 4 × (4.81 − 3.4) / 121.2 Ω = 46.5 mA. Typical is 85 % of the panel's 70 mA.
- **Why not Revision 2's 110 Ω (C22781, extended):** 65.1 mA typical, but 80.1 mA in the worst corner once the −1 % is
  counted (Revision 2's 79.3 mA left it out), and 83.7 mA at Vf 2.9: at or over the 80 mA maximum. It would need a
  firmware PWM cap of about 95 % and one more extended part (§8.1 Q4).
- **Why not Revision 1's 100 Ω:** 71.6 mA typical, but 87.2 mA in the worst corner (more with the tolerance).
- **Rail tolerance:** +5V_LCD spans 4.81-5.18 V with R10 at 1 % (decision 12) and FB at 0.588-0.612 V.
- **Dissipation:**
  - Each resistor: 40 mW or less (0603 parts are rated 100 mW).
  - Q2: Vgs ≈ 3.3 V through R36/R37, 73 mA × 48 mΩ, about 0.3 mW.
  - Boost load: 0.38 W or less at 100 % duty.
- **PWM:**
  - PC6 runs as TIM3_CH1 (AF2), push-pull, at about 20-25 kHz, with high meaning on. In rev A it was open-drain into the
    2090's LITE pin (gb2-nets.csv:38).
  - The gate's RC is 100 Ω × 630 pF (Ciss) = 63 ns.
  - **Off at boot:** R37 holds the backlight off through reset, so the screen stays dark until firmware enables it.

### 4.4 Reset

- **Line:** LCD_RST (U2.23) with R38, a 10 k pull-up to +3V3. While the FPGA is configuring, its pins float with weak
  pull-ups, so RESX stays high. ILI9341 §12.1 allows this, as long as a hardware reset follows ("no timing restriction").
- **Gateware sequence:**
  1. Once both rails are up, drive RESX low for at least 10 µs (pulses under 5 µs are rejected).
  2. Release it and wait at least 5 ms before the first command.
  3. Send Sleep Out (11h) no earlier than 120 ms after the release (§15.4, notes 5-7).
- **FPGA reloads:** with a pull-up, reconfiguring the FPGA leaves the panel running. A pull-down would reset the panel
  every time.

### 4.5 TE and the U2 pin map

- **TE pin:** LCD_TE goes to U2.44, an input. TE is low until it is enabled; send 35h 00h for TE mode 1 (V-blank only).
  The gateware then holds RAMWR until TE's rising edge.
- **Pin order (decision 16):**
  - Seen from the front, U2's pins run clockwise, because U2 is on the back.
  - J3's bus pads, in the order RST, D7…D0, WR, DC, CS, TE, reach U2 in rising pin-number order: 23, 25, 26, 27, 28, 31,
    32, 34, 35, 37, 42, 43, 44.
  - Pins 24 (VPP), 29/30 (VCCPLL/VCC), 33 (VCCIO_0) and 38-41 are skipped, with no crossings.
- **Bus route:** from the vias under J3's body, north on F.Cu up the centre column (x 105-112, over In1; MP8 holds
  x 102.9-104.5 at y 72.9-80.9, and 13 nets need only about 3.9 mm). It then drops
  to B.Cu in a via field at x 113-118, y 47.5-56, just south-west of the U2 block. About 35-45 mm per net.
- **Tearing:** in landscape (MADCTL MV = 1), the writes run across the panel's 320-line scan.
  - A full frame takes 15.4 ms at 100 ns per byte (153,600 bytes), but the panel refreshes every 14.3 ms at the
    ILI9341's default 70 Hz (B1h 00h 1Bh).
  - So TE limits tearing but cannot remove it. A lower refresh rate through B1h is a gateware or firmware choice.

### 4.6 Bus timing (unchanged)

- **WR cycle:** 2 clocks at 20 MHz = 100 ns per cycle.
- **What the panel needs** (ILI9341 §18.3.1; this datasheet defers to the IC's, see its p.1 note A): twc ≥ 66 ns,
  twrl/twrh ≥ 15 ns and tdst ≥ 10 ns.
  - The cycle meets twc. The pulse widths and setup/hold come from the gateware state machine, as in rev A.
- **R29:** 33 Ω source termination, which stays within a few mm of U2.37.
- **Stale notes:** gb2-bom.csv:30 and BOM.md #79 still say "pin 38". The netlist (gb2-nets.csv:140, 169) has pin 37,
  which is correct.

### 4.7 USB data, ESD and DFU *(new in Revision 2)*

**Circuit:**
- **D+:** J7 A6 and B6, tied together at the receptacle, go to net USB_DP → U13 I/O2 (pins 3, 4) → U1.45 (PA12,
  USB_FS_DP).
- **D−:** J7 A7 and B7 go to net USB_DN → U13 I/O1 (pins 1, 6) → U1.44 (PA11, USB_FS_DM).
  - The USBLC6 pins are from `cad/docs/C7519.pdf` Fig. 1: I/O1 on 1 and 6, GND on 2, I/O2 on 3 and 4, VBUS on 5.
  - The part is symmetric, so swapping I/O1 and I/O2 at layout is allowed if it shortens the pair.
- **U13 pins 2 and 5:** pin 2 to GND and pin 5 to VBUS. This is ST's recommended connection; it biases the internal
  Zener.
- **D4:** an SMF5.0A on VBUS, with the cathode on VBUS (MDD, C193402):
  - Vrwm 5.0 V, VBR 7 V, Vc 9.2 V, 200 W (10/1000 µs) (JLC data);
  - it goes beside J7's VBUS pins.
  - The BQ24073 input survives 28 V and shuts out above its 6.6 V OVP, so D4 only has to clamp fast edges and ESD on
    the rail.
  - **Sustained-VBUS limit (accepted, schematic review):** D4 breaks down at 6.4 V minimum and is rated 1.0 W steady
    state, and U13's VBUS pin breaks down at about 6 V. So rev B's input no longer rides up to the BQ24073's 6.6 V OVP
    as rev A's did: a charger stuck above about 6.4 V would heat D4 until it fails, probably short, which shorts VBUS
    (the source's own protection then trips). Accepted because a sink with only 5.1 k Rd on CC is only ever offered
    5 V, and nothing on the board negotiates QC or PD. If that changes, use a TVS with a higher stand-off and tie U13
    pin 5 to +3V3 instead of VBUS.
  - The footprint's cathode is pad 1, under the silk bar (BOM_REVB.md).
- **No series resistors or pull-ups:** the F411's OTG FS PHY has them inside (STM32 §3.27).
- **USB_FS_VBUS sense on PA9 is not used.** PA9 stays USART1_TX. The device runs self-powered with VBUS sensing
  disabled (GCCFG.NOVBUSSENS).
- **D+ pull-up rule (firmware):** with NOVBUSSENS the internal D+ pull-up stays on with no cable. U13 pin 5 sits on
  VBUS, so D+ would back-feed the VBUS node through U13's upper diode to about 2.7 V, and a device must not drive the bus
  without VBUS. So firmware connects the pull-up (clears DCTL.SDIS) only while CHG_PGOOD (PC2, the BQ24073's /PGOOD)
  is low, i.e. while VBUS is present, and sets SDIS again when /PGOOD goes high. The ROM bootloader manages its own
  pull-up; it only runs when you invoke it, normally with the cable plugged in.
- **CC1/CC2:** they keep rev A's 5.1 k Rd (R15/R16), so a C-to-C host enumerates the device.

**Layout:**
- U13 and D4 go on the front, in the ESD block at x 85.8-92.8, y 40.3-44.9, between L2 and R16. They are within 6 mm of
  J7's pins.
- The pair runs on F.Cu over In1: from J7 to U13, just south-west of it, then east along the top band, through G1/G2
  into the right grip, to U1.
  That is about 55 mm.
- **Pair geometry:** 90 Ω differential where it fits. Full speed (12 Mb/s) tolerates the rest.
- **No stubs:** the receptacle's A/B duplicates join right at J7.

**DFU entry:**
- **Firmware:** **Select + Start** held at power-up (both are on the U11 expander, read over I2C, so neither holds an
  MCU pin), or a USB vendor request. Firmware then **waits until every key is released**, sets a flag in a backup
  register, resets, and (before any clock set-up) jumps to the ROM bootloader at 0x1FFF 0000:
  1. set MSP from 0x1FFF 0000;
  2. jump to the reset vector at 0x1FFF 0004.
  - Revision 2's example, Select + Vol−, is out: Vol− is PA15, which is SPI3_NSS to the ROM, so the held key would
    select SPI3 as the ROM starts.
  - The ROM serves USB DFU on PA11/PA12 (datasheet §3.13). Per AN2606, the DFU path needs the HSE crystal, which Y1
    (8 MHz) provides.
- **Every interface the ROM scans** (datasheet §3.13), and what this board has on its pins while the ROM runs:

  | ROM interface | Pins | On this board |
  |---|---|---|
  | USB OTG FS DFU | PA11/PA12 | USB_DN/USB_DP: the one we want |
  | USART1 | PA9/PA10 | console TP11/TP12, idle |
  | USART2 | PD5/PD6 | not bonded on LQFP64 |
  | I2C1 | PB6 (SCL)/PB7 (SDA) | PB6 spare and floating; PB7 = I2C1_SDA (R14 pull-up), idle |
  | I2C2 | PB10 (SCL)/PB3 (SDA) | PB10 spare and floating; PB3 = the A key (no pull-up in the ROM; pressed = low) |
  | I2C3 | PA8 (SCL)/PB4 (SDA) | PA8 spare and floating; PB4 = Vol+ |
  | SPI1 | PA4 (NSS)/PA5/PA6/PA7 | **PA4 = FPGA_SS_B, held low (asserted) by R6 3.3 k all the time**; PA5/PA7 to the FPGA, PA6 unused |
  | SPI2 | PB12-PB15 | PB12/13/15 = I2S2 to U6 (idle, U6 not clocked); PB14 = the B key |
  | SPI3 | PA15 (NSS)/PC10-PC12 | PA15 = Vol−; PC10-12 = the NOR (U4 deselected by R8) |

  - So: don't hold A, B, Vol+ or Vol− while entering DFU (the firmware path waits for release).
  - **Bring-up check:** confirm that DFU enumerates with SPI1_NSS held low and the three SCL pins floating. If noise
    on them makes the ROM lock onto I2C or SPI1, the fixes are a pull-up on PA5 or the SCL pins, or holding the FPGA
    in reset, at the next spin.
- **Hardware recovery:** bridge TP8 (BOOT0) to +3V3 (TP5, or J1 pin 1), then cycle SW1 or pulse NRST (J1 pin 5).
  Both work in rev B, because R39 holds BOOT1 (PB2) low (decision 22). R1 (10 k) pulls BOOT0 low in normal use.
- **Bring-up order:**
  1. SWD (J1) for the first flash.
  2. Then prove DFU from the BOOT0 pad: once with a power cycle, and once with an NRST pulse after the FPGA is
     configured (this proves R39).
  3. Then the firmware jump.
  4. If the ROM's USB DFU turns out to need VBUS on PA9, fall back to USART1 through TP11/TP12 (§8.2).

### 4.8 Panel frame and plate grounding *(new in Revision 2)*

- **Why:** in rev A the 2090's PCB grounded its panel frame, and Revision 1's CN4190 tape would have done it. In
  Revision 2 the panel floats in the shell, so its frame needs its own path to GND.
- **Panel frame, MP1 and MP2:** exposed ENIG GND pads on F.Cu under the panel's back, outside the FPC tail's path.
  - MP1: 2 × 8 at x 63.6-65.6, y 60-68, on the left strip.
  - MP2: 5 × 6 at x 112.0-117.0, y 88-94, in the centre column, clear of the bus corridor.
  - Each has at least 4 vias to In1.
  - The plate-bridge pads MP7 (x 63.6-65.6) and MP8 (x 102.9-104.5), both y 72.9-80.9, are the only other exposed
    copper under the panel (§3.5).
  - A die-cut conductive foam pad on each (about 0.6 free, rated for 40 % or more compression) fills the 0.40-0.53 gap
    to the panel's back.
  - **Check on arrival:** that the panel's back frame is bare metal; meter it from the frame to a GND pin of the FPC.
    If it is painted or plastic, delete MP1/MP2 and ground the frame's edge with copper tape to the plate.
- **Plate:** the 4 tab pads MP3-MP6 and the 2 foil bridges on MP7/MP8 (§3.5) put it on GND at about 30 mm spacing. It then acts as
  part of the ground structure under the panel, not as a floating patch near the antenna.

### 4.9 Firmware and gateware work (not PCB, but rev B doesn't light up without it)

1. **Pin file:** create the first board pin file, `fpga/gameboy-v2b.pcf`, from gb2b-nets.csv. It covers D0-D7 on
   35/34/32/31/28/27/26/25, WR on 37, DC on 42, CS on 43, RST on 23 and TE on 44.
2. **Reset:** add an LCD_RST output and the reset sequence (§4.4) to ppu_top.v and lcd_driver.v.
3. **Init ROM:** today it sends only 11h / 3Ah 55h / 36h 28h / 29h (lcd_driver.v:73-90).
   - **No published init for this glass:** EastRising's demo zip for the ER-TFT028A2-4 is behind Cloudflare and not
     archived, so its power, VCOM and gamma values are unknown.
   - **Start from:**
     - the ILI9341 power-on defaults;
     - **21h (Display Inversion ON)**, which IPS glass needs; a PJRC user running this panel only had to add inversion;
     - **MADCTL:** lcd_driver.v already sends 28h (MV + BGR). If red and blue swap on this glass, send 20h.
   - **Then, if the picture needs it,** tune C0h/C1h/C5h/C7h and the E0h/E1h gamma. Displaytech's IPS ILI9341 init
     (DT028CTFT_Init.c: C0 20h, C1 10h, C5 32h 3Ch, C7 B9h) is a reasonable second reference.
   - Send each parameter as one byte.
4. **TE:** send 35h 00h, then sync RAMWR to TE.
5. **Backlight:** switch PC6 to AF2 push-pull PWM, and turn the backlight on after the panel init.
6. **Forced PWM:** drive BB_PSYNC (PC3) high as soon as the GPIOs are set up (R25 already holds it high until then);
   drive it low for power-save only where 3.50 V on the panel and the FPGA is acceptable.
7. **USB:**
   - enable the OTG FS clock (48 MHz from the PLL off HSE), with NOVBUSSENS set;
   - connect the D+ pull-up (clear DCTL.SDIS) only while CHG_PGOOD (PC2) is low, and disconnect it when PC2 goes high
     (§4.7);
   - add the DFU-entry key combo (Select + Start on U11, wait for all keys released) and the jump (§4.7);
   - optionally a USB CDC console.
8. **CDONE pin:** board.dts `cdone-port`/`cdone-pin` become GPIOB / 0 (PB0, decision 22). PB2 is now the BOOT1 strap
   and stays an input.
9. **Orientation:** check MADCTL on the real panel. Its pin 1 is in the same place as the MI0283QT-11's, but p.6 doesn't
   show the first-pixel corner. Start from 28h (MV = 1) and flip MX/MY if the image is mirrored.
10. **Pin map:** update board.h; the gb2 `src/` still uses the breadboard map.
    - The IMU is now on the back, so remap X and Z: two axes mirror.
    - USB is on PA11/PA12.

---
## 5. BOM and netlist deltas

### 5.1 BOM (gb2b-bom.csv; JLC tier and stock checked live on 2026-10-14)

| Change | Ref | Part | LCSC | Tier, stock | BOM.md row |
|---|---|---|---|---|---|
| Replace | (hand-fitted) | Adafruit 2090 → **EastRising ER-TFT028A2-4, "No Touch Panel"** (buydisplay.com, US$7.08). Buy 3: bare glass, and the FPC can tear | - | BuyDisplay | #15 |
| Replace | (hand-plugged) | Adafruit 2011 (LP-803860) → **Adafruit 258** (LP-503562, 1200 mAh, JST PH) | - | Adafruit | #54 note |
| Delete | J4 | HX PM2.54-1x20P ZC socket | C41417332 | Extended | #17 |
| Replace | J3 | 2090 socket → **AFC07-S50ECA-00**, 50P 0.5 mm top-contact ZIF, H2.0 | C262650 | Extended, 17,479 | #16 (rewritten) |
| Replace | J1 | MTP125-1105S1 (THT, C358687) → **HX PZ2.54-1x5P WT**, right-angle SMD, 2.5 high | C46061678 | Extended, 15,802 | #13 |
| New | R32-R35 | 0603WAF1200T5E, 120 Ω 1 % (schematic review; Revision 2 had 110 Ω C22781) | C22787 | Basic, 2.23 M | new #84 |
| New | Q2 | AO3400A, N-channel SOT-23 (pin 1 G, 2 S, 3 D, as Q1) | C20917 | Basic, 768,956 | new #85 |
| New | R36 | 0603WAF1000T5E, 100 Ω 1 % | C22775 | Basic, 11.4 M | new #86 |
| New | R37 | 0603WAF1003T5E, 100 kΩ 1 % | C25803 | Basic (reel already used 4 times) | new #87 |
| New | R38 | 0603WAF1002T5E, 10 kΩ 1 % | C25804 | Basic, 31 M | new #88 |
| New | C52 | CL21A106KOQNNNE, 10 µF 0805 | C1713 | Extended (reel already used) | new #89 |
| New | C53 | CC0603KRX7R9BB104, 100 nF | C14663 | Basic (reel already used) | new #89 |
| New | U13 | **USBLC6-2SC6** (ST), 2-line USB ESD + VBUS, SOT-23-6 | C7519 | Extended, 33,736 | new #90 |
| New | D4 | **SMF5.0A** (MDD), 5 V unidirectional TVS, SOD-123FL, 200 W | C193402 | Extended, 559,541 | new #91 |
| New | MP1-MP8 | Exposed GND pads, footprint only, no part: panel-frame contacts (MP1, MP2), plate tabs (MP3-MP6) and plate-bridge landings (MP7, MP8) | - | - | new #92 |
| New | R39 | 0603WAF1002T5E, 10 kΩ 1 %: BOOT1 (PB2) pull-down (decision 22) | C25804 | Basic, 32.8 M | #10 |
| Change | R25 | Same part (100 kΩ); pin 2 moves from GND to +3V3: PS/SYNC pull-up (decision 14) | C25803 | Basic | #46 (rewritten) |
| Reel swap | R1 R4 R5 R8 R10 R12 R22 R23 R31 | RC0603JR-0710KL → 0603WAF1002T5E | C99198 → C25804 | Basic | #10, #25, #72, #82 |
| Repurpose | C25, C26 | Unchanged parts. Now "+5V_LCD decoupling at R32-R35" | C14663, C1713 | | #18 |
| Note fix | R29 | Place by **U2 pin 37**; its output goes to **J3 pin 36** | C23140 | | #79 |
| Move only | J8, R20, R26, R27, TP2, … | Unchanged parts (§6.1). Re-verified: J8 C295747 has 39,055 in stock | C295747, C2892547 | Extended | #43, #51, #54 |

- **Extended parts:** this removes C41417332, C358687 and C99198, and adds C262650, C46061678, C7519 and C193402
  (no longer C22781: the 120 Ω C22787 is basic). That is net +1 extended part; 110 Ω would make it +2 (§8.1
  Q4).
- **Mechanical parts, not in the JLC BOM:**
  - the SUS304 plate, 0.5 mm, laser-cut and joggled;
  - Z-axis conductive transfer tape for the 4 tabs, and 2 copper-foil bridges;
  - structural epoxy;
  - 0.05 polyimide film;
  - stretch-release cell tape;
  - 2 conductive foam pads for MP1/MP2 (the 2 copper-foil bridges land on MP7/MP8);
  - a 0.3 black foam gasket ring;
  - a 1.1 mm glass lens (1.5 mm acrylic for the prototype);
  - the door.
- **Hand assembly:** the panel and the cell.
- **THT:** the only THT parts left are SW1 and J7's shell tabs.
- **Symbols and footprints:** fetch them into the **project-local** library `gb2b/lib/` (never `cad/symbols/`). Register
  it in gb2b's `sym-lib-table` and `fp-lib-table`.
  - LCSC parts: C262650 C46061678 C22787 C20917 C22775 C25804 C7519 C193402. The 0603WAF symbols use rev A's
    `easyeda2kicad:R0603` footprint (decision 12).
  - MP1-MP8 are simple custom pad footprints.
  - The AFC07 symbol name is `AFC07-S50ECA-00`.

### 5.2 Netlist (gb2b-nets.csv: refdes, pin, net, kind, note)

**Delete:** J3 pins 1-20 (gb2-nets.csv:211-230) and J4 pins 1-20 (:231-250). This also removes J3.2 / J3.6 = +5V_LCD (the
2090's VIN and the RD tie) and J3.8 = LCD_LITE.

**Add, J3 (unchanged from Revision 1; panel names from p.9):**
```
J3,1,LCD_LEDK,,LEDK — common cathode of the 4 backlight LEDs → Q2 drain
J3,2,LCD_LEDA1,,LEDA1 ← R32
J3,3,LCD_LEDA2,,LEDA2 ← R33
J3,4,LCD_LEDA3,,LEDA3 ← R34
J3,5,LCD_LEDA4,,LEDA4 ← R35
J3,6,GND,gnd,IM0 (IM[3:0]=0000: 8080 8-bit bus I, p.10)
J3,7,GND,gnd,IM1
J3,8,GND,gnd,IM2
J3,9,GND,gnd,IM3
J3,10,LCD_RST,,RESET ← U2.23; R38 10k pull-up
J3,11,GND,gnd,VSYNC (RGB i/f unused: fix to VSS)
J3,12,GND,gnd,HSYNC
J3,13,GND,gnd,DOTCLK
J3,14,GND,gnd,DE
J3,15,GND,gnd,DB17 (unused in 8-bit mode: fix to VSS)
J3,16,GND,gnd,DB16
J3,17,GND,gnd,DB15
J3,18,GND,gnd,DB14
J3,19,GND,gnd,DB13
J3,20,GND,gnd,DB12
J3,21,GND,gnd,DB11
J3,22,GND,gnd,DB10
J3,23,GND,gnd,DB9
J3,24,GND,gnd,DB8
J3,25,LCD_D7,,DB7
J3,26,LCD_D6,,DB6
J3,27,LCD_D5,,DB5
J3,28,LCD_D4,,DB4
J3,29,LCD_D3,,DB3
J3,30,LCD_D2,,DB2
J3,31,LCD_D1,,DB1
J3,32,LCD_D0,,DB0
J3,33,,,SDO — open
J3,34,GND,gnd,SDI (fix to VSS)
J3,35,+3V3,pwr,RD (fix to VDDI, write-only bus)
J3,36,LCD_WR,,WRX ← R29
J3,37,LCD_DC,,D/CX
J3,38,LCD_CS,,CS
J3,39,LCD_TE,,TE → U2.44
J3,40,+3V3,pwr,VDDI
J3,41,+3V3,pwr,VDDI
J3,42,+3V3,pwr,VCI
J3,43,GND,gnd,GND
J3,44,,,X+ — NC (no-touch panel)
J3,45,,,Y+ — NC
J3,46,,,X- — NC
J3,47,,,Y- — NC
J3,48,GND,gnd,GND
J3,49,GND,gnd,GND
J3,50,GND,gnd,GND
J3,51,GND,gnd,fitting nail
J3,52,GND,gnd,fitting nail
```

**Add, backlight, reset and panel decoupling (as Revision 1; R32-R35 now 120 Ω):**
```
R32,1,+5V_LCD,pwr,1
R32,2,LCD_LEDA1,,2
R33,1,+5V_LCD,pwr,1
R33,2,LCD_LEDA2,,2
R34,1,+5V_LCD,pwr,1
R34,2,LCD_LEDA3,,2
R35,1,+5V_LCD,pwr,1
R35,2,LCD_LEDA4,,2
Q2,1,BL_GATE,,G
Q2,2,GND,gnd,S
Q2,3,LCD_LEDK,,D
R36,1,LCD_LITE,,1 — PC6 side
R36,2,BL_GATE,,2 — Q2 gate side
R37,1,BL_GATE,,1 — gate pull-down: backlight off through reset
R37,2,GND,gnd,2
R38,1,LCD_RST,,1
R38,2,+3V3,pwr,2
R39,1,BOOT1,,1 — PB2 (BOOT1) pull-down (schematic review)
R39,2,GND,gnd,2
C52,1,+3V3,pwr,1 — panel VCI/VDDI bulk, below J3
C52,2,GND,gnd,2
C53,1,+3V3,pwr,1 — panel VCI/VDDI HF, below J3
C53,2,GND,gnd,2
```

**Add, USB and ground pads (new in Revision 2):**
```
U13,1,USB_DN,,I/O1 — D− line (flow-through to pin 6)
U13,2,GND,gnd,GND
U13,3,USB_DP,,I/O2 — D+ line (flow-through to pin 4)
U13,4,USB_DP,,I/O2
U13,5,VBUS,pwr,VBUS — biases the clamp (ST recommended)
U13,6,USB_DN,,I/O1
D4,1,VBUS,pwr,K — SMF5.0A cathode on VBUS (confirm pin 1 = K on the fetched footprint)
D4,2,GND,gnd,A
MP1,1,GND,gnd,panel-frame contact pad (left strip, under the panel)
MP2,1,GND,gnd,panel-frame contact pad (centre column, under the panel)
MP3,1,GND,gnd,plate tab pad (top bridge, west)
MP4,1,GND,gnd,plate tab pad (top bridge, east)
MP5,1,GND,gnd,plate tab pad (bottom left)
MP6,1,GND,gnd,plate tab pad (bottom right)
MP7,1,GND,gnd,plate copper-foil bridge pad (left strip, under the panel, y ≈ 76.9)
MP8,1,GND,gnd,plate copper-foil bridge pad (centre column west edge, under the panel, y ≈ 76.9)
```

**Change:**
```
J7,A6,USB_DP,,DP1 — D+ → U13 → PA12 (rev B)
J7,B6,USB_DP,,DP2 — D+ (rev B)
J7,A7,USB_DN,,DN1 — D− → U13 → PA11 (rev B)
J7,B7,USB_DN,,DN2 — D− (rev B)
U1,44,USB_DN,,PA11 — USB_FS_DM (OTG FS device; ROM DFU) (rev B)
U1,45,USB_DP,,PA12 — USB_FS_DP (rev B)
U2,23,LCD_RST,,IOT_37a — panel RESET (rev B: moved from 36 so the J3 fan-out has no crossings)
U2,36,,,IOT_48b — spare (rev B)
U2,44,LCD_TE,,IOB_3b_G6 — ← panel TE (rev B)
U1,37,LCD_LITE,,"PC6 — TIM3_CH1 (AF2) push-pull PWM → R36 → Q2 gate (high = backlight on); R37 holds it off through reset"
R29,2,LCD_WR,,2 — J3 pin 36 side
J8,1,BAT,pwr,1 — cell + (Adafruit 258 red) — verify polarity with a meter
TP8,1,BOOT0,,BOOT0 pad in the battery door: bridge to +3V3 (TP5 / J1 pin 1), then cycle SW1 or pulse NRST (J1 pin 5) → ROM bootloader (USB DFU / USART1); R39 holds BOOT1 low
U1,26,FPGA_CDONE,,PB0 — iCE40 CDONE (poll) (rev B: moved from PB2, which is the BOOT1 strap)
U1,28,BOOT1,,PB2 — BOOT1 strap, R39 10k pull-down (rev B: was FPGA_CDONE)
R25,2,+3V3,pwr,2 — pull-up (rev B; was GND): PS/SYNC high = forced PWM unless PC3 drives it low
U1,46,SWDIO,,PA13 — SWD → J1 pin 2 (rev B: right-angle header in the battery-door column)
```

**Unchanged:**
- U5, L1, D3, C25-C28, R9, R10 and TP6 on +5V_LCD (:251-272, 574);
- J1 (:206-210, the same 5 nets; footprint only);
- J8 pins 2-4 (:398-400);
- R20 (:389-390);
- R26/R27/C45 (BAT_SENSE; the parts move, the nets stay).

**Check after the change:** 154 footprints. Every net touches at least two pins, except intentional NCs. U1.44 and U1.45
are no longer listed as spare, and neither is U1.26 (PB0, now CDONE). The generator's validation gate passes (§9 step 1).

---

## 6. Placement zone plan ("R2")

### 6.1 Zones and blocks

Every block below was checked with shapely (`check.py`) against:
- the outline with a 0.5 inset;
- the notch, the opening + 0.5, the panel pocket (front) and the FPC sweep;
- the 8 post zones, the 3 screws (r 3) and the antenna keep-out;
- every rev A part that stays, on the same side.

The result is **0 collisions**. The only flags are the two known rev A ones: SW2's post against U12, and U12's own
antenna zone.

| Zone | Side | Block (x, y; KiCad mm) | Contents | Why here |
|---|---|---|---|---|
| **BR**, back of the right grip | B | **MCU** 136.8-156.0, 43.5-59.2 | U1 and its ring C1-C10, FB1, Y1, C11, C12, R3, R1 (BOOT0), R39 (BOOT1, at U1.28), R13/R14 (I2C pull-ups), C45, TP9 | Beside the right-grip keys and I2S. It is the hub for the long nets |
| | B | **IMU** 144.0-149.6, 36.2-41.6 | U7, C31, C32, R12 | Decision 24: 10.8 mm from S2, 21.6 from the A post, 30 from the speaker |
| | B | **NOR** 147.0-154.0, 60.0-67.5 | U4 (SOIC-8, 2.07 tall; the back allows 4.78), C24, R8 | Between the A and B posts; SPI3 about 10 mm |
| **BTR**, back above the opening | B | **FPGA** 113.5-125.0, 33.2-47.0 | U2 and its ring C13-C20, FB2, R4-R7, D2, R29 (at U2.37), R38 | Closest spot to J3 that keeps the column free; +1V2 pour on B.Cu |
| | B | **LDO** 125.4-129.4, 38.5-46.8 | U3, C21-C23, TP7 | Moved off x > 129.4, so B.Cu stays free through the right gate (§6.4) |
| **Door column**, back x 104.9-118.25 | B | **J8** 104.9-112.9, 37.3-45.0 | J8 (mouth −X), TP2, R26/R27 (BAT/2 divider) | Right behind U9 |
| | B | **Backlight** 104.9-112.2, 47.0-55.2 | R32-R37, Q2, C25, C26 | By J3 pins 1-5 |
| | B | **J3** 105.9-113.3, 55.6-86.4 | J3 (§3.4) | Fixed by the FPC |
| | B | **C52/C53** 104.9-110.6, 86.6-88.0 | C52, C53 | Below pins 40-42 |
| | B | **J1** 105.0-117.7, 88.2-101.7 (+ a mated housing to about 109.7, at the board edge: open item, decision 9) | J1, pins +Y | Reachable with the door open |
| | B | **Debug pads** 105.0-118.0, 99.5-106.5 | TP8 (BOOT0), TP10 (GND), TP11, TP12 (console) | Beside J1 |
| **Lead pocket**, back of the top bridge | B | 66.6-104.6, 37.0-45.55 | Cell leads; R20 (NTC) at 99.4-101.6, 43.4-45.0; TP1, TP3, TP5 stay; TP4 → (98.5, 38.5), TP6 → (67.5, 39.0) | Parts ≤ 1.0 |
| **BL**, back of the left grip | B | - | U12 and C47-C50 stay, plus 6 post zones | Free space otherwise; parts ≤ 4.78 |
| **FT**, front top bridge | F | - | Rev A power section unchanged: J7, R15, R16, U9 group, Q1, R30, U10 group, U5 group | Reused copper |
| | F | **ESD** 85.8-92.8, 40.3-44.9 | U13, D4 | Within 6 mm of J7's pins |
| | F | **Soft start** 102.5-104.4, 40.4-45.0 | C51, R31 | Out of the notch |
| | F | **Plate tabs** | MP3, MP4 | §3.5 |
| Front, under the panel | F | Left strip, centre column | MP1, MP2, MP7, MP8; routing only (the display bus on F.Cu in the centre column, x 105-112) | |
| **FRT/FB**, front above and below the opening | F | x 118.25-136.35 | Free: routing lanes for the gates | |
| Grips, front | F | - | Unchanged: U11 and C46, U8 with C33-C35, J6 and the LRA, all switches, U6 group, J5, D1, R2 | |

**Placement rules:**
- **Decap rings:** on the same side as their IC, round it.
- **R29:** within a few mm of U2.37.
- **Power-rail bring-up pads:** inside the door where they fit (§3.5).
- **The column between the notch and the opening** (x 102.4-118.25) holds J8, the backlight block, J3, C52/C53, J1 and
  the debug pads on the back, top to bottom. Keep its front free for the bus corridor, MP2 and MP8.

### 6.2 What is reused

**Unchanged placement and local routing (67 footprints):**
- The front top bridge:
  - J7, R15, R16.
  - U9 with C36-C38, R17, R18, R21-R23, R30, Q1.
  - U10 with L2, C39-C44, R24, R25.
  - U5 with L1, D3, C27, C28, R9, R10.
- All 13 switches (SW1-SW13).
- The left grip front: U11 and C46, U8 with C33-C35, J6 and the LRA.
- The right grip front: U6 with C29, C30, R11, plus J5, D1 and R2.
- U12 and C47-C50 on the back; R19 and TP1, TP3, TP5 on the back of the top bridge.
- The speaker notch and the 3 shell-screw holes.

**Power-section copper inside 0.5 of the new notch edge** (for example rev A's SYS run at y 45.2) must be re-routed.

**R25 is re-tied:** it keeps its place, but its pad 2 moves from GND to +3V3 (decision 14). Give it a via to the In2
+3V3 pour under the top bridge and remove its old GND connection.

**place.py** (placement sequence at place.py:1921-1992):

| Step | Status in rev B |
|---|---|
| `button_targets`, `place_usb`, `place_charger`, `place_buckboost`, `place_lcdboost`, `place_expander`, `place_haptics`, `place_amp`, `place_bt` | Run unchanged. Exceptions: in `place_charger`, J8 and CHG_NTC_X (R20) get the new positions, and C51/R31 move |
| `DISPLAY_SOCKETS`, `display_pins`, `pose_on_pins` | Go. A fixed J3 pose from §3.4 replaces them |
| `place_mcu`, `place_fpga`, `place_fpga_lane`, `place_fpgaldo`, `place_nor`, `place_imu`, `place_batsense`, `place_display_caps` (becomes the backlight block), `place_testpoints`, `place_swd` | Rewritten for §6.1, most on B.Cu |
| new `place_usb_esd`, `place_plate_pads` | U13/D4; MP1-MP8 |
| `set_4layer` | New layer roles (decision 20) |
| `place_thermal_vias` | Re-run |

**Routing:**
- Every rev A channel (W1, W2, W2b, P, H, E1, E2, A) goes, because the J3/J4 choke points are gone.
- Only channel N, under J7, survives, and only if it doesn't need In2.
- Rev A took about 4 days from placement to DRC-clean (2026-10-10 to 10-13).

### 6.3 Critical routes and routing rules

Lengths are estimates between block centres.

1. **Display bus** (13 nets: RST, D7-D0, WR, DC, CS, TE):
   - **Path:** J3 pads → tented vias under J3's body → F.Cu north up the centre column over In1 → a via field at x 113-118,
     y 47.5-56 → B.Cu into U2's west and south pins. About 35-45 mm per net.
   - **Order:** crossing-free in the §4.5 order.
   - **Returns:** a GND stitching via beside each via cluster.
   - **R29:** within a few mm of U2.37.
2. **Backlight:**
   - LEDA1-4 (≤ 20 mA each) and LEDK (≤ 80 mA, ≥ 0.3 mm wide) run from J3 pads 1-5 north into the backlight block,
     about 5 mm.
   - +5V_LCD runs from U5 (front, (69.8, 39.3)) along the top bridge to R32-R35, about 41 mm, 0.4 mm (Rail class).
     Budget 0.15 A (PLAN.md:57).
   - LCD_LITE runs from PC6 in BR to R36, through G2, about 45 mm.
3. **PPU SPI** (SCK, MOSI, SS_B, CRESET_B, CDONE): from BR to U2 through the band above the opening, about 25-30 mm, over
   GND.
4. **SPI3 to U4:** inside BR, about 10 mm.
5. **USB D+/D−:** J7 → U13 → east along the top bridge on F.Cu → G2 → U1, about 55 mm, as a pair (§4.7).
6. **MCU to the left grip** (USART2 ×4, BT_EN, I2C1 SDA/SCL to U11 and U8):
   - **Path:** BR → G2 → over the opening → G3 → the top bridge (G1) → the left grip. About 110-130 mm. The UART can run
     on B.Cu in the gates.
   - **I2C:** re-run `route/i2c_check.py`. Rev A was about 115 mm per line, 45-50 pF, t_r ≈ 60 ns against a 300 ns limit.
     The longer run is expected to stay under 80 ns.
7. **MCU to the right grip** (I2S ×3; BTN_A, BTN_B, BTN_R, VOL±): local, under 25 mm.
8. **MCU to the power section** (CHG_STAT, CHG_PGOOD, BB_PSYNC, BAT_SENSE): about 45-60 mm, through G2 and the top band.
9. **Debug** (SWDIO, SWCLK, NRST; USART1 TX/RX; BOOT0): BR → G2 → the band above the opening → down the centre column's
   front → J1 and the debug pads. About 60-70 mm.
10. **BAT trunk:** J8 pin 1 → at least 4 × 0.3 mm vias → U9 BAT (pins 2/3) and C38. About 2-5 mm, at 1.5 A (PLAN.md:56).
    It no longer crosses anything.
11. **SYS to the amp** (1 A peak): from Q1 east along the front top band, through G2, to U6. About 55 mm, ≥ 0.8 mm,
    re-routed clear of the notch edge.
12. **+1V2:** U3 → U2's core pins (5, 30), and through FB2 to VCCPLL (29), as a B.Cu pour inside the FPGA/LDO blocks.
13. **+3V3:** from U10 (front top bridge) as 0.4 mm Rail-class trunks:
    - to J3/C52 (back column), U2/U3 and BR via G2;
    - to the left grip via G1 (U12, U11, U8).
    - The In2 +3V3 pour serves only the top bridge.

**Rules carried over from rev A** (PLAN.md):
- 0.15/0.15 tracks and 0.45/0.25 vias. Power class 0.8/0.2; Rail class 0.4/0.15.
- A GND return via within 1.2 mm of every signal via.
- Edge stitching every 2.5 mm, and F.Cu GND pours under the switchers.

**New rules:**
- **Under the panel and in the FPC sweep:** solder mask everywhere, tented vias, no test points. Exposed copper only on
  MP1/MP2 and the plate-bridge pads MP7/MP8.
- **Reference planes:** fast nets on F.Cu run over In1; fast nets on B.Cu run over In2 GND, never over the top-bridge
  +3V3 pour. Only slow nets (UART, I2C, enables, buttons) may run on B.Cu across the top bridge.
- **Edges:** a 0.5 mm copper keep-out round the opening and the notch, and GND stitching along both.
- **J3's GND pins** (6-9, 11-24, 34, 43, 48-52): connect each with its own short stub and via under the body, and keep the
  mask dams between the 0.30 mm pads.

### 6.4 Routing capacity: 4 or 6 layers? *(new in Revision 2)*

**Method** (`capacity.py`):
- **The gates:** with the notch and the FPC opening cut out, the board has three gates between its left and right
  parts:
  - **G1:** the 13 mm bridge over the notch. It is the only path to the left grip.
  - **G2:** the band above the opening at x ≈ 132.5 (cut down by S2's keep-out), plus the 3.4 mm strip beside the
    opening. Together they are the only paths to the right grip.
  - **G3:** the line along the opening's inboard side, above it and below it.
- **Capacity:** for each gate and each layer, count 0.30 mm slots (rev A's 0.15/0.15 Default class) in the copper
  cross-section. The copper stays 0.5 from edges and 3 from screws.
- **What counts as an obstacle:**
  - every §6.1 block on its side, taken as a full obstacle;
  - every kept rev A part's courtyard + 0.15;
  - the kept rev A copper of the top bridge's local nets;
  - the plate tab pads.
- **Demand:** every non-GND net of the Revision 2 netlist with pins on both sides of the gate. Power nets are counted at
  their width: SYS/BAT 4 slots, +3V3/+5V_LCD/VBUS 2.
- **In1 is solid GND** and counts 0. In2 is GND too, so it is reported only as a reserve.

| Gate | Narrowest cross-section | Slots per layer | Free slots: F / In2 / B | Must cross: signals + power (slots) | Spare on F + B alone |
|---|---|---|---|---|---|
| **G1**, top bridge, x 95.0 | y 33.0-45.05, 12.05 mm | 40 | 15 / 40 / 33 | 12 + VBUS, +3V3, +5V_LCD, SYS (22) | **+26** |
| G1, x 101.5 | 12.05 mm | 40 | 34 / 40 / 20 | 13 + VBUS, SYS_SRC, SYS, +3V3, +5V_LCD (27) | +27 |
| G1, x 67-85 | 12.05 mm | 40 | 27-34 / 40 / 32-40 | 8-10 + 2-3 power (14-17) | +43 to +60 |
| **G2**, right gate, x 132.5 | band 8.68 mm (y 38.45-47.05, below S2) + strip 2.4 mm | 29 + 8 | 37 / 37 / 37 | 25 + SYS, +3V3 (31) | **+43** |
| G3, x 119-128 | above 14.05 mm + below about 13 mm | 47 + 40-44 | 87-91 / 87-91 / 42-58 | 25-33 + 2-3 power (31-39) | +94 to +114 |

**What has to cross:**
- **At G1:** BT UART ×4, BT_EN, I2C ×2, PWR_SW, BB_PSYNC and USB ×2, plus the rails. Near its east end, the power
  section's own USBC_CC2, Q1_GATE and CHG_TS are counted too.
- **At G2:** the 25 MCU nets that leave the right grip:
  - the PPU SPI ×5;
  - USART2 ×4 and BT_EN;
  - I2C ×2;
  - USB ×2;
  - CHG ×2, BB_PSYNC, BAT_SENSE;
  - LCD_LITE;
  - SWD ×3, USART1 ×2 and BOOT0;
  - plus SYS and +3V3.

**Decision: 4 layers.**
- Every gate keeps at least 26 spare slots (about 8 mm of copper) on the two outer layers alone. In1 stays an unbroken
  GND plane, and In2 stays GND as a 37-40-slot reserve.
- Six layers would only buy capacity that isn't needed. They would also add cost and a stack change; the EE study notes
  that JLC's 6-layer boards start at 1.2 mm.
- **Two placement choices protect this margin:**
  - The LDO sits west of x 129.4. When it was at x 129-133.4 it blocked B.Cu in G2, and the margin there was only +15.
  - The NOR and IMU sit in the right grip, so SPI3 and IMU_INT1 stay local.
- **Estimate caveat:** the demand uses block centres. Real routes detour, so treat the +26 at G1 as the figure to watch
  when the top bridge is re-routed.

---

## 7. Enclosure implications

**Starting point:**
- Start from `enclosure/variants/thin-stack/` (its parametric stack and `battery_door.py`).
- Build rev B in its own folder (for example `enclosure-revb/`). The rev A enclosure stays as it is, for the ordered
  board.

**Frame (t = 1.2; KiCad z from the board bottom):**
- T = 1.2.
- Z_FRONT = t + 4.615 = 5.815 (glass), or 6.415 (acrylic). Z_BACK = t − 7.88 = −6.68.
- The split stays at the board's top face.
- PLATE = 1.6, with an inner face at t + 3.015. There are local pockets down to 0.95 over J7, L2, J5, J6, the LRA and
  SW10-SW13 (§3.1).
- Delete `module_2090()`, the standoffs and the J1 hatch.

**Screen:**
- **Panel pocket:** x 62.95-133.85, y 45.60-96.40 (Y negated in the enclosure frame). Its walls locate the panel to
  ±0.15 and stop 0.6 above the board top, over the plate tabs MP3/MP4.
- **Lens:**
  - 1.1 mm strengthened glass (production) or 1.5 mm cast acrylic (prototype), flush with the face.
  - Centred 82.2 × 56, or 75.9 × 55.8.
  - It sits on a land frame 2.5 wide whose bottom is at t + 2.3, with a relief over L2.
  - A black mask on its back leaves a 58.4 × 44.0 window at (95.25, −71.0).
- **Gasket:** 0.3 (glass) or 0.5 (acrylic) black closed-cell foam ring, about 1.8 wide, on the module border. It is
  low-tack on the panel side.
- **Parameters:** make the lens and gasket thicknesses parameters, along with the 0.3 back gap.

**Face keys:**
- **Caps:** short. The keystake top is at t + 1.5 and the plate's inner face at t + 3.015, so each cap has about a 1.5 mm
  stem.
- **Guide tubes:** they go.
- **D-pad:** re-derive its legs and pivot.
- **Structural review:** #6 (D-pad hole corners R1.0) still applies. #1 (phantom presses) mostly goes away, because the
  plate is now backed by the board about 3.0 below it. Re-check it by FEA.

**Back shell: one flat face at z −6.68.**
- **Floor:** 1.6, with its inner face at −5.08 (−6.28 below the board top).
- **Posts:** Ø4 × about 5.0, at all 8 face switches. SW2's post needs Ø3.5 or a +0.5 mm shift in x (§3.8).
- **Door opening:** §3.5, about 55 × 71.
  - It has a ledge round it for the flush 1.2 mm door, with hooks at the top and a snap at the bottom.
  - The door's inner face is thinned to 0.8 over J8 (x 104.9-112.9, y 37.3-45.0).
- **Lead pocket:** behind the top bridge, open to the door.
- **Speaker cradle:** through the notch, as the 300° C-ring from STRUCTURAL_REVIEW #2. The speaker spans t + 2.715 to
  t − 2.185.
- **Front-shell ribs** press the 4 plate tabs (§3.5):
  - from the lens land at MP3/MP4;
  - from the face plate at MP5/MP6, about 2.4 tall.
- **Screws:**
  - S1-S4 as in rev A, plus the 5th boss from STRUCTURAL_REVIEW #3;
  - the board-only screws B1 and B2 (§3.6);
  - fixes #4 (compliant washers), #8 (6 mm fingers), #9 and #11 apply.

**Edge keys and other controls:**
- **Edge-key notches:** z t to t + 3.6, at the rim.
- **Cap lip recess:** must stop below the plate's inner face, t + 3.015.
- **USB-C stadium and power slide:** unchanged relative to the board top.
- **LED light pipe:** about 4 mm instead of 17.7.

**checks.py:** new envelopes for:
- the panel and gasket;
- the plate with its tabs;
- the cell in the notch with its 0.53 swell gap;
- J3 with its slider open;
- J1 and its Dupont housing;
- J8 under the door patch;
- the 8 posts.

**Assembly order:**
1. Front module, face-down: lens, gasket, panel, caps, D-pad, edge caps, slider, speaker, light pipe.
2. The board, with the plate already bonded, threading the FPC tail through the opening. Fit B1/B2, then close J3.
   - Power-test it over USB.
3. Back shell; S1-S4.
4. Through the door: the cell onto the plate (stretch-release tape), then J8.
5. The door.

---

## 8. Open questions and risks

### 8.1 Open questions (you decide or measure)

1. **Lens:** a custom 1.1 mm chemically strengthened glass, 82.2 × 56 with a printed black mask, for production? Or stay
   on 1.5 mm acrylic with a 0.5 gasket (13.1 mm)? The prototype uses acrylic either way.
2. **Lens shape:** centred 82.2 × 56 (recommended: the seam is symmetric) or off-centre 75.9 × 55.8?
3. **Door:** 1.2 (baseline) or 1.0 (−0.2 mm, 12.3 total)? Print-test it against the real cell.
4. **Backlight resistors:** 120 Ω (baseline since the schematic review: C22787, basic; 59.7 mA typical, 73.4 mA worst
   case, so about 85 % of the panel's typical brightness) or 110 Ω (C22781, one more extended part; 65.1 mA typical,
   but 80.1 mA worst case with the resistor tolerance, so firmware caps the PWM duty at about 95 %)? 100 Ω (87 mA
   worst case) is out.
5. **Plate:** epoxied window plate with front tabs (baseline, 12.5 mm)? Or back-face flanges with a 0.7 recess, no
   adhesive, 13.05 mm (§3.5)?
6. **Cell leads:** fold the 100 mm leads in the pocket (baseline), or shorten them to about 35 mm and re-crimp?
7. **Measure when the parts arrive** (§9 step 2):
   - the real FPC length (spec 26.70 ±0.50) and where the folded tail lands (assumed 25.00);
   - whether the tail lies against the panel back or at the component height;
   - the AFC07 50-pin insertion depth (2.84 assumed, up to +0.55);
   - the panel's thickness with and without its tape;
   - whether its back is bare metal.

   These set J3's x to about ±0.3, the front stack, and MP1/MP2.
8. **Panel ID readback for bring-up (optional):** keep RD tied to +3V3 (baseline), or route RD to U2.36 with a 10 k
   pull-up and U2.14 (SPI_SO) to a free MCU MISO? The second option lets the MCU read RDDID (04h).

### 8.2 Risks

1. **FPC length stack-up.**
   - The FPC is ±0.5, the stop depth is uncertain by up to +0.55, and the panel and board locations add about ±0.25.
   - The joggle absorbs only about ±0.6.
   - The fold's real geometry (the tail against the panel or at component height) moves J3 by 0.8.
   - **Fix:** measure before freezing J3 (§9 step 2). The column has room for the whole range (§3.4).
2. **FPC damage.** The datasheet gives no bend radius ("do not forcibly pull or bend the cable", p.18), and there is no
   service loop. Fixes:
   - Bend only in the single-layer zone.
   - Keep the opening's edge free of copper and burrs, and put polyimide tape on it.
   - Fold it in a jig, and always open J3 through the door before separating the board and the shell.
3. **Plate bond.**
   - The front-ward load path relies on the epoxy edge fillet and the front-shell ribs. A failed fillet lets the plate
     bow toward the panel under a hard press.
   - **Fixes:** test the bond with a 30 N press on a coupon. The fallback is back flanges with a 0.7 recess (§8.1 Q5).
4. **The 0.31 mm cell-to-wall corner.** The cell's maximum footprint comes within 0.31 of the wall at the bottom-left;
   nominally it is 0.6. Print-check the wall position.
5. **Thin board lobes.** The left lobe hangs on the 13 mm top bridge, and the 3.4 mm strip sits beside the opening.
   Handle and ship the bare board on a carrier, and panelize with tabs on both lobes.
6. **Peak current against the cell rating.** Rev A's budget allows 1.5 A peaks (amp + buck-boost at 3.0 V; PLAN.md). The
   Adafruit 258 is rated 1C (1.2 A) continuous (p.2), and its PCM trip point isn't published.
   - **Mitigation:** firmware limits the amp's volume (gain) and the backlight when the cell is low. Log VBAT during
     audio tests.
7. **Unknown init values.** There is no EastRising init for this glass (§4.9). Expect a few iterations on inversion,
   BGR, VCOM and gamma.
8. **USB DFU through the ROM.** Not yet proven on this board:
   - the ROM needs HSE for DFU (AN2606);
   - PA9 is USART1_TX, not VBUS sense;
   - the ROM also scans SPI1 with its NSS (PA4) held low by R6, and three floating SCL pins (PB6, PB10, PA8); noise
     there could make it lock onto the wrong interface (§4.7 table).
   - **Fallbacks:** BOOT0 + USART1 (TP11/TP12), or SWD.
   - **VBUS above about 6.4 V** (a faulty charger) would burn D4; accepted, because the board only ever asks for 5 V
     (§4.7).
9. **BT830 range.** The panel frame sits 25.5 mm and the plate 28.5 mm from U12, between the datasheet's 20 mm and
   30 mm figures. That is better than rev A's 16.9 mm, but range should be tested in the enclosure.
10. **Panel frame grounding.** It depends on the panel's back being bare metal and on the foam pads' compression in a
    0.40-0.53 gap. Verify both on a sample.
11. **Thickness assumptions.** 2.80 has no tolerance on p.6. The 10 % swell is an assumption the PKCELL spec doesn't
    give. The tape allowance may or may not be needed.
12. **Dark screen at first power-up.** It has three silent causes: the backlight is off until firmware starts the PC6
    PWM, the gateware sends no reset pulse yet, and the init lacks inversion. Bring these up in that order.
13. **Tearing.** TE cannot remove tearing in landscape: a frame write takes 15.4 ms against a 14.3 ms refresh.
14. **Supply.**
    - BuyDisplay's live stock couldn't be read; the last archived copy said "In stock". Buy 3 panels.
    - C46061678 (J1) has 15,802 in stock. The 120 Ω C22787 is basic, with 2.23 M; the 110 Ω C22781 it replaced had
      10,981.
    - Re-check stock on order day.
15. **JLC placement rotations.** Bottom-side rotations in the placement (CPL) file are a common source of JLC assembly
    errors, because KiCad's and JLC's conventions differ. Rev B puts J3 (at 270°), U1, U2, U3, U4, U7, J1 and J8 on
    B.Cu, so check every one in JLC's 3D preview.
16. **IMU axes.** U7 is on the back, so two of its axes mirror; board.h must remap them.

---

## 9. Work plan (in order; verify before moving on)

**0. You, now.**
- Answer questions 1-6 in §8.1.
- Order:
  - three ER-TFT028A2-4 "No Touch" panels;
  - five AFC07-S50ECA-00 (C262650) from LCSC, for the fit test;
  - two Adafruit 258 cells.

**1. Schematic**, in `projects/gameboy-v2/pcb/gb2b/`.
- **Build:**
  1. Fetch the new symbols and footprints into `gb2b/lib/` and register them in gb2b's sym-lib-table and fp-lib-table
     (§5.1).
  2. Apply §5 to `gb2b-nets.csv` and `gb2b-bom.csv`.
  3. Run `python3 gb2b/gen.py`, then `python3 gb2b/title.py` (gen.py wraps `cad/generate_schematic.py` and adds the
     gb2b library, which the plain generator can't load; see gb2b/README.md).
- **Verify:**
  - The generator's validation gate passes.
  - ERC is clean, or every remaining item is explained.
  - A script check confirms:
    - J3 pins 1-52 match §4.1 exactly;
    - U2.23 is LCD_RST, U2.44 is LCD_TE and U2.36 is empty;
    - U1.26 is FPGA_CDONE, U1.28 is BOOT1 with R39 to GND, and R25 pulls BB_PSYNC to +3V3;
    - R29 sits between U2.37 and J3.36;
    - U1.44 is USB_DN, U1.45 is USB_DP, U13 follows §5.2, and D4's cathode is on VBUS.
  - No net touches only one pin.
  - Every LCSC number is still live on JLC that day.
  - Render each changed sheet (display, lcd-boost, fpga, mcu, usb-c) and look at it before calling it done.
  - `git status -- projects/gameboy-v2/pcb/gb2` stays empty.

**2. Fit test**, when the panel arrives. This can run alongside step 3, but must finish before step 4 routes near J3.
- **Do:**
  1. Measure the panel's thickness and its unfolded FPC.
  2. Fold the FPC over a printed coupon: a 1.2 board with the opening edge, and a 0.3 spacer under the panel.
  3. Make the joggle at r ≈ 1.
  4. Insert it into a loose AFC07.
- **Verify:**
  - The tail end lands within ±0.3 of the predicted x (109.14).
  - The stiffener lies flat past the joggle.
  - No bend falls in the component area.
  - The panel's back conducts.
- **Then:** move J3's origin x by the measured difference.

**3. Outline and placement.**
- **Build:**
  1. Draw the Edge.Cuts as in §3.6.
  2. Place J3 as in §3.4.
  3. Place the blocks per §6.1, with the MP1-MP8 pads.
  4. "Update PCB from Schematic": re-link C25 and C26 by reference (they changed sheet, so their links no longer match
     the board's), and check that the nine reused 10 k resistors keep their `easyeda2kicad:R0603` footprints and silk
     (decision 12). J3, J1 and the 21 new parts come in new; J4 goes.
  5. Re-check J1's zone with a real mated Dupont housing (decision 9).
- **Verify:**
  - J3 pad 1 is at (106.71 + Δ, 58.75) and pad 50 at y 83.25, on B.Cu, with the latch toward +X.
  - No courtyard overlaps.
  - A height script against board.json shows nothing breaks the §3.7 limits: 0 under the panel pocket, ≤ 2.0 under the
    land, ≤ 2.7 (≤ 3.35 in pockets) on the front, ≤ 4.78 on the back outside the door, ≤ 5.18 inside it.
  - Nothing sits in the notch, sweep, post, antenna, screw or opening zones.
  - The IMU obeys the 10 mm rule.
  - Front and back renders (`cad/render_pcb.py`) look right.
  - `capacity.py` is re-run on the real placement and every gate still has spare slots.
- **Export:** `enclosure/export_board.py` writes board.json for step 6.

**4. Routing.**
- **Build:** re-plan the channels for G1-G3, then route in rev A's group order: power trunks, then the switcher GND
  pours, then the local blocks, then the display bus, the PPU SPI, USB, the cross-board nets and debug. Then fanout,
  then pour.
- **Verify:**
  - DRC: 0 errors, 0 unconnected.
  - `audit.py` lists only explained detours.
  - `ipc_check.py` passes: BAT 1.5 A, the amp feed 1 A, +5V_LCD 0.15 A.
  - `planecheck.py` finds In1 unbroken and nothing fast crossing the In2 +3V3 pour.
  - `i2c_check.py` passes at the new lengths.
  - Under the panel and in the sweep, every via is tented and the only exposed copper is MP1/MP2 and MP7/MP8.
  - Nothing is within 0.5 of the opening or the notch.
  - The bus fans out with no crossings.

**5. DRC and fab.**
- **Build:**
  - Pick JLC's 4-layer 1.2 mm stackup.
  - Generate the outputs with `fab/fab.py`. It reads `gb2b/BOM.md` (rev A's rows plus rev B's #84-#92) and stops if a
    part's `#N` has no row there. Update its rev A checklist notes for J1, J3 and J8 first.
  - Work through `fab/FOOTPRINT_CHECKLIST.md` for the AFC07, the right-angle header, the USBLC6, the SMF5.0A (cathode)
    and the AO3400A (pin 1 is G).
- **Verify:**
  - The JLC 3D preview shows every back-side part at the right rotation, with J3's pad 1 at the top.
  - The JLC BOM and CPL match gb2b-bom.csv, minus the bare pads (TP, MP), the solder-pad connectors J5/J6 and the parts
    with no LCSC number (U12, the panel, the cell).

**6. Enclosure** (§7). It can start once step 3 exports board.json; print the door, the plate coupon and the panel pocket
early.
- **Verify:**
  - `checks.py` (a)-(j) pass with the new stack.
  - Print tests with the real cell (door, swell gap, corner clearance) and the real panel (gasket, lens gap, pocket fit).
  - The plate-bond press test (§8.2 risk 3).
  - Re-run the FEA for key presses on the 1.2 mm board with the posts.

---
## Appendix A. Revision 1 cross-check log (historical: MI0283QT-11, taped panel, cell bump)

1. **Cell layout.**
   - **Placement study:** used the brief's landscape cell (x 63.75-126.75, y 66.75-106.25) and found that no top-contact
     connector position escapes it. Its options were a tray (+2.9 mm), an off-centre cell, or a smaller cell.
   - **Mechanical study:** its portrait cell (x 65.6-104.1) leaves J3's spot (x 105.73-111.48) free.
   - **Resolved:** adopted the portrait cell. The placement study's zones B-C and B-X (the MCU behind the panel's upper
     half, the FPGA under the cell) no longer exist, so I re-planned them as §6.
2. **FPC path.**
   - **Placement study:** a 1.5 × 26.5 slot at x 115-116.5, with the 45.9 mm wide part lying on the board front. That needs
     a 0.4-0.5 mm foam bed under the whole panel.
   - **Mechanical study:** a 14.8 × 46.9 opening that holds the wide part and adds 0 mm.
   - **Resolved:** adopted the mechanical study's opening.
3. **J3's position.**
   - **The studies:** placement had x 108.0-113.75; mechanical had x 105.73-111.48 at t = 1.0.
   - **Resolved:** adopted the mechanical study's, since it follows from the joggle at the chosen t. I confirmed it with the
     footprint in pcbnew (rot 270 on B.Cu, pad 1 at (106.215, 58.75)) and the 3D model's extents (−1.575 to +4.175 =
     DIM E 5.75; height 2.00).
4. **Contact type.**
   - **Electrical study:** said folding under the panel needs bottom contact.
   - **Resolved:** that holds only for a connector on the front under the panel (route c, rejected). With the joggle to the
     back, the contacts face away from the back, so top contact is right. The other two studies agree, and p.5 confirms
     it: the contacts are on the viewing face, and the components on the back face.
5. **Pin mapping.**
   - **Electrical study:** derived the 1:1 mapping for an unfolded S-bend.
   - **Resolved:** re-derived for the fold and joggle using the folded view (PIN:01 on the left). It is still pad n = pin n,
     and pad 1 must be at the top (y 58.75).
6. **Panel position.** Both studies put the module at x 63.25-132.55: placement via "centre 97.9", mechanical via "active
   area at 95.25". They are identical.
7. **Board thickness.**
   - **The studies:** the mechanical study recommends 1.0; the placement study assumed 1.6.
   - **Resolved:** baseline 1.0, user to confirm. Plan B works for both, but the left-grip alternative only works at 1.6
     (overlaps of 16.6 and 19.8 mm² with the D-pad post zones).
8. **Height limits.**
   - **Placement study:** front ≤ 4.25 and grip back ≤ 2.0-2.1, from its own stack guess.
   - **Mechanical study:** front ≤ 4.4 and grip back ≤ 1.7.
   - **Resolved:** adopted the mechanical study's limits. One consequence: U4 (SOIC-8, 2.07) can't go on the grip back.
     I also added the ≤ 3.2 land-ring limit. LQFP64 is 1.60 max (STM32 datasheet Table 82), against 1.0 in board.json.
9. **Bus driver.** The brief named STM32 nets. gb2-nets.csv:126-146 shows U2 (the iCE40) drives D0-D7, RST, DC, CS and WR.
   Corrected.
10. **+5V_LCD tolerance.**
    - **Electrical study:** 4.89-5.09 V, counting only the FB tolerance.
    - **Source:** R10 is a 5 % part (BOM.md #72), which gives 4.65-5.37 V.
    - **Resolved:** a 1 % R10 (decision 12) brings it to 4.81-5.18 V, and the anode resistors become 100 Ω.
11. **TE and RST pins.**
    - **Electrical study:** TE on U2.38, with RST staying on U2.36.
    - **Resolved:** changed to U2.44 and U2.23 for a crossing-free fan-out.
    - **Also found:** the R29 notes in BOM.md #79 and gb2-bom.csv:30 say WR is on iCE40 pin 38. The netlist
      (gb2-nets.csv:140, 169) has pin 37, so the notes are stale.
12. **SWD.**
    - **The studies:** mechanical put a vertical SMD header in the bay at x 107.5-110, y 40.5-53.2. Electrical listed a
      right-angle SMD header, a THT header and bare pads. Placement put a TC2030 at (122.5, 40), outside the bay.
    - **Resolved:** C42453972 (a vertical SMD header, found on JLC) in the bay below J3. Plan B needs the bay's top-right
      corner for the FPGA.
13. **J8.** The placement study's spot (x 127.4-135.6, y 72.2-81.8) falls inside the mechanical study's FPC opening.
    Adopted the mechanical study's spot, in the bay below J3.
14. **NTC.**
    - **Source:** BOM.md #43 says "under the cell's footprint", but the KNTC0603 is 0.8 mm tall (JLC), more than the 0.3 mm
      cell gap.
    - **Resolved:** the cell's edge, inside the bay.
15. **U12's real extent.** It is x 28.03-37.78, y 46.38-61.0 (pcbnew bounding box). The placement study's table centred a
    9.79 × 13.79 box on the origin, which is about 5 mm off. 32 back parts fall in the cell zone (the mechanical study
    said 31).
16. **Page references.** The placement study's panel page numbers are one lower than the PDF footers (the pinout is p.10,
    not p.9). The init code is on p.18-19, not p.17-18 as the electrical study said.
17. **Touch.** The electrical study kept touch as an option. Under a rigid lens with a 0.5 mm gap the film can't be used,
    so the pins are NC.
18. **+3V3 in power-save mode.** Neither study noted that it runs up to 3.50 V (BOM.md #46, tps63021.pdf §7.3.1). The
    existing forced-PWM rule covers the panel too.
19. **Lens flex.** The mechanical study's 0.37 mm at 10 N agrees with the FEA in STRUCTURAL_REVIEW #5 (~0.4 mm per 10 N).
20. **Stack numbers.** Re-running `/tmp/revb/mech/stack.py` reproduces the mechanical study's numbers: grips
    10.92 / 11.12 / 11.52 and bump 18.17 / 18.36 / 18.77 for t = 1.0 / 1.2 / 1.6, and the FPC end at u 23.91 [23.31].


## Appendix B. Revision 2 cross-check log (numbers changed in this revision, against their sources)

1. **Panel drawing (p.6).**
   - **Re-read numbers:**
     - module 69.30 × 50.20, LCD glass 64.75 × 46.20, visual area 59.20 × 44.80, active area 57.60 × 43.20;
     - borders 1.70 / 2.40 / 3.20 at the far end;
     - 2.80 thick; back tape T = 0.115;
     - FPC 26.70 ±0.50, 45.90 × 13.20 wide part, 25.50 ±0.10 tail, 13.50 tail length, 5.50 stiffener, 0.30 ±0.03,
       contacts 0.35 / 3.50, fold 1.00 MAX;
     - pin 1 at the top; "CONDUCTOR" points to FRONT.
   - **Typo:** the drawing's "P0.5*(40-1)=24.50" means 50 pins (49 × 0.5 = 24.50).
2. **Pin table and electrical (p.9-11).** Pins 1-50 are as §4.1. IM 0000 = 8080 8-bit I. VCI 2.5/2.8/3.3, VDDI
   1.65/2.8/3.3, absolute maximum 4.6. Backlight 70 typ / 80 max mA for 4 in parallel, Vf 3.2/3.4 at 80 mA.
   - **The brief said** "70-80 mA total". Read as typical / maximum, it drove the change to 110 Ω, and the schematic review's tolerance check to 120 Ω (§4.3).
3. **Cell (PKCELL QA.S.0228).** p.5: 35 ±0.3 × 62 ±0.3 × 5.0 ±0.3, 100 mm leads, "Reverse the plug JST PH-2P". p.2:
   1200 mAh nominal and 1140 minimum, 1C maximum continuous, 0.2C standard and 1C quick charge.
   - **Charge current:** BOM.md #41 gives ISET ≈ 445 mA, which is 0.37C, well inside 1C.
4. **Stack.**
   - **Quoted against computed:** you were quoted about 12.2 mm, and `stack.py` gives 12.495 at t = 1.2. The difference
     is the 0.15 plate recess, the 0.05 PCM insulation and the 0.115 tape allowance (§3.1).
   - **The ME study's R2 variant** (17.2 mm) is a different architecture: the cell behind the board, with an 8.5 mm
     cell. It is not comparable.
5. **Board thickness.**
   - **The ME study chose 1.2 for stiffness.** Revision 2 adds the J8 column argument: at 1.6, J8 sets the back, at
     12.8 mm.
   - **Flex:** 0.47 mm at 1.2 = 0.2 × (1.6/1.2)³, which matches the ME study.
6. **Battery placement.**
   - **Conflict:** the ME study kept the cell behind a continuous board ("no cut-out"). The locked baseline puts it in a
     cut-out.
   - **Resolved:** the locked baseline wins. The cut-out became an open-bottom notch (§2 decision 7), because a closed
     one would leave a strip under 1 mm.
   - The ME study's plate-on-shell-ledges was replaced by a window plate referenced to the board's top face, because no
     shell ledge can reach a plate that sits in the board plane.
7. **Plate support.**
   - **First idea:** joggled flanges under the board's back. Each joggle's leg would cross the cell's plane, and that
     needs about 1.4 per side or a 0.7 recess (+0.55 mm overall).
   - **Resolved:** front-face tabs plus an epoxy edge fillet. The plate's deflection (0.159 mm at 20 N, one-way span
     36.3, E = 193 GPa) was checked in `stack.py`.
8. **J3 position.**
   - **The ME study's estimate:** origin about x 107.80 for the 0.3 lift.
   - **Reproduced:** `stack.py` gives 107.802 (r 1.0, h 2.445), using Revision 1's constants (ENTRY 0.88, INS 2.84,
     folded tail 25.00, footprint offsets 1.335 and 1.09).
   - **New:** the range for a tail riding at component height, 106.79-107.30.
9. **Antenna distances.** Measured with shapely on board.json:
   - panel 28.4 / 25.5 mm (keep-out / U12);
   - plate 31.4 / 28.5;
   - cell 31.7 / 28.8.
   - The EE study's 28.4 (screen frame to the keep-out) agrees.
10. **U2 pin for TE.**
    - **Conflict:** the brief said U2 pin 38 (from the electrical study). Revision 1 had moved it to U2.44, with RST on
      U2.23, for a crossing-free fan-out.
    - **Resolved:** kept 44 (§2 decision 16). Flag this if the schematic step reads the brief literally.
11. **USB pins.**
    - STM32 Table 8: LQFP64 pin 44 = PA11 = USB_FS_DM, and pin 45 = PA12 = USB_FS_DP. gb2-nets.csv:45-46 lists both as
      spare.
    - USBLC6-2SC6 Fig. 1: 1/6 I/O1, 2 GND, 3/4 I/O2, 5 VBUS.
    - J7 A6/B6 = DP and A7/B7 = DN (gb2-nets.csv:347-354).
12. **DFU.**
    - STM32 datasheet §3.13: the ROM serves USB OTG FS DFU on PA11/12, as well as USART1, I2C1-3, SPI1-3.
    - The memory map puts system memory at 0x1FFF 0000-0x1FFF 7A0F.
    - **Not in the local docs, and taken from AN2606:** the HSE requirement for DFU. Treated as a bring-up check
      (§8.2 risk 8).
13. **Parts (JLC API, 2026-10-14):**
    - C262650 17,479;
    - C46061678 15,802 (right angle, 2.5 insulation, 6 mm pins);
    - C42453972 338 (vertical, rejected);
    - C7519 33,736;
    - C193402 559,541 (200 W, Vrwm 5 V, Vc 9.2 V);
    - C22781 10,981 (110 Ω, extended);
    - C20917 768,956;
    - C295747 39,055 (Z-height 5.5 mm, which matches JST p.3).
14. **Routing capacity.**
    - **The EE study counted** "36 slots against about 23-30 signals" with the FPGA blocking B.Cu above the opening.
    - **Revision 2** moves the LDO off the right gate and puts the NOR and IMU in the grip. It then counts per gate and
      per layer (§6.4): +26 at G1, +43 at G2 and +94 at G3 on the outer layers alone.
    - **Before the LDO move,** G2 had only +15 spare. That is the reason for the move.
15. **Placement collisions.** `check.py` on the §6.1 blocks: 0 collisions. The only flags are rev A's own SW2-post/U12
    0.17 mm and U12's antenna zone.
    - **IMU:** 10.8 mm from S2. The first try (x 143.0) gave 9.8 mm, so the block moved 1 mm east.
16. **Schematic-review fixes (2026-10-14).** Applied in gb2b after the schematic step; each was checked against its
    source.
    - **BOOT1:** STM32F411 pin table, LQFP64 pin 28 = PB2 = BOOT1; §3.13 boot modes. iCE40UP5K datasheet: CRESET_B has
      no internal pull-up and CDONE has a weak pull-up, and the driver (`rtos/drivers/fpga/ice40.c`) leaves CRESET_B
      high after init, so CDONE stays high across an MCU reset. Fix: CDONE to PB0 and R39 on PB2 (decision 22). The
      rejected alternative, R4 as a pull-down, would also need the driver's init order changed (it releases CRESET_B
      with SS_B high, which starts the iCE40 as an SPI master against the MCU's PA4).
    - **PS/SYNC:** tps63021.pdf: PS/SYNC absolute maximum 7 V, VIH 1.2 V. R25 to +3V3 (decision 14).
    - **Backlight:** p.11 70 / 80 mA, Vf 3.2 / 3.4 at 80 mA, no minimum. JLC API: C22787 = UNI-ROYAL 0603WAF1200T5E,
      basic, 2,230,468 in stock (§4.3).
    - **ROM interfaces:** datasheet §3.13 lists USART1, USART2, USB OTG FS, I2C1-3 and SPI1-3; mapped to this
      netlist in the §4.7 table.
    - **D4:** VBR 6.4 V minimum, 1 W steady state (MDD); accepted (§4.7).
    - **Plate bridges:** the plate spans y 45.75-108.05, so its long-edge midpoint is y 76.9, not Revision 2's "y ≈ 71";
      MP7/MP8 sit there (§3.5).
    - **J1:** footprint pads y ±1.585, insulator 3.33-5.87, pins to 11.9 (gb2b.pretty); a 14 mm housing ends at
      5.87 + 14 = 19.9 past the pad row (decision 9).
