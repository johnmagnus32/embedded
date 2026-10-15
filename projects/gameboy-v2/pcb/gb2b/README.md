# gb2b: gb2 rev B

The KiCad project for **rev B** of gb2, the GBA-style handheld (STM32F411RE + iCE40UP5K driving the 8-bit 8080
display bus, BT830, BQ24073, TPS63021, MT3608). The design spec is **[../REVB_DESIGN.md](../REVB_DESIGN.md)**.

**Rev A (`../gb2/`) is ordered and frozen.** Never edit anything there. Rev B is a separate project that started
as a copy of it.

## What rev B changes (locked 2026-10-14)

- **Screen:** EastRising ER-TFT028A2-4 bare panel (2.8" IPS 240x320, ILI9341, no touch, 50-pin 0.5 mm FPC) replaces the
  Adafruit 2090 module. The panel is held in the front shell on a foam gasket under a 1.1 mm glass cover, with the board
  0.3 mm behind it. There are no front-side parts under the panel. The FPC folds behind the panel and joggles through a
  board opening to a top-contact 50-pin connector on the back (JUSHUO AFC07-S50ECA-00, LCSC C262650).
- **Battery:** Adafruit 258 (35 x 62 x 5 mm, 1200 mAh, JST-PH), in a portrait board cut-out left of that connector,
  behind a 0.5 mm stainless plate, with a flush battery door. The back is flat and the device is about 12.5 mm thick all round (REVB_DESIGN.md §3.1).
- **Extras:** USB D+/D- to the STM32 PA11/PA12 (DFU) with an ESD array and a VBUS TVS. J8 sits next to the charger U9.
  The IMU is kept away from screws, posts, the LRA and the speaker. The panel frame is grounded. TE goes to the FPGA
  (U2 pin 44, `LCD_TE`; REVB_DESIGN.md decision 16 moved it from the brief's pin 38 so the bus fans out without
  crossings). The FPGA drives the panel reset with a 10k pull-up. RD is tied to +3V3, IM0-3 and the unused
  inputs go to GND, and SDO is left open.

## Status

**Schematic: rev B's electrical changes are in (2026-10-14).** The PCB is not updated yet.

- `gb2b-nets.csv` / `gb2b-bom.csv` carry REVB_DESIGN.md §4-5:
  - the 50-pin FPC connector J3, with every panel pin wired;
  - the backlight driver (R32-R37, Q2; R32-R35 = 120 Ω C22787) on a new `backlight` sheet;
  - the LCD_RST pull-up R38, and C52/C53;
  - the GND pads MP1-MP8 (MP7/MP8 land the plate's copper-foil bridges);
  - USB D+/D- to PA11/PA12 through U13, with the VBUS TVS D4;
  - the right-angle SWD header J1, and the 10k reel swap;
  - the schematic-review fixes: FPGA_CDONE moved to PB0 with R39 pulling BOOT1 (PB2) low, and R25 as a PS/SYNC
    pull-up (REVB_DESIGN.md decisions 14 and 22).

  J4 is gone. BOM changes: `BOM_REVB.md`. `BOM.md` here is rev B's full machine-readable BOM (rev A's rows plus
  #84-#92), which `fab/fab.py` reads.
- The schematic regenerates with `gen.py` (below): 154 parts and 19 child sheets. The VALIDATE gate passes. ERC gives
  0 errors and 229 warnings, all of the same two kinds as rev A's 229: generator-made power symbols not found in the
  `power` library, and pin-to-pin warnings on the shared library's "unspecified" pins.
- `gb2b.kicad_pcb` is still rev A's placed and routed board, linked to the rev A parts. It still has J4 and the old J3,
  and it lacks the 21 new parts until "Update PCB from Schematic" (or `place.py`) runs in the placement step
  (REVB_DESIGN.md §9 step 3). `route/groups/` was deliberately **not** copied.
- These files still describe rev A and get revised as rev B lands:
  - `mech_ref.json` + `draw_mech.py` (the 2090 module and the Adafruit 2011 cell);
  - `route/PLAN.md` and `route/channels.py` (rev A's corridors);
  - `fab/verify.py` (checks J3/J4 against the 2090);
  - `fab/fab.py`'s checklist notes (J1, J3, J8). Its BOM lookup now reads `BOM.md` in this directory and stops on an
    unknown `#N`.

## Layout

| Path | What |
|---|---|
| `gb2b.kicad_pro` / `.kicad_sch` / `sheets/gb2b-*.kicad_sch` | Project + generated hierarchical schematic (one child sheet per subcircuit) |
| `gb2b.kicad_pcb`, `gb2b.kicad_dru` | Board + custom DRC rules |
| `gb2b-bom.csv`, `gb2b-nets.csv` | **The schematic source.** Edit these, never the .kicad_sch |
| `lib/gb2b.kicad_sym`, `lib/gb2b.pretty/`, `lib/gb2b.3dshapes/` | Project-local library (nickname `gb2b`) for rev B's new symbols and footprints. `../../../../cad/symbols` holds shared work, so rev B parts don't go there |
| `gen.py` | Runs the schematic generator with the project-local library |
| `BOM_REVB.md` | Rev B's BOM delta against rev A (LCSC, tier, stock) and the footprint checks |
| `BOM.md` | Rev B's full BOM rows (rev A's + #84-#92), the `#N` lookup `fab/fab.py` uses |
| `title.py` | Stamps the "gb2 rev B" title block into every sheet + the board |
| `place.py` | Schematic -> board import + placement |
| `route/` | Routing + check tools (fanout, pour, silk, audit, IPC current, I2C rise time, plane return path, plot) |
| `fab/fab.py`, `fab/verify.py` | JLC outputs (into `fab/out/`, git-ignored) + the pre-fab checks |

## Regenerating the schematic

Close KiCad first, then from the repo root:

```sh
python3 projects/gameboy-v2/pcb/gb2b/gen.py       # = cad/generate_schematic.py + the gb2b library, on this dir's CSVs
python3 projects/gameboy-v2/pcb/gb2b/title.py     # the generator writes no title block
```

`gen.py` wraps the shared generator without changing it. The generator itself only loads `easyeda2kicad` and `manual`
from one `--symbols` directory, so the wrapper also loads `lib/gb2b.kicad_sym`. Pass `--project <copy>` for a dry run
into a copy at the same folder depth.

The generator's VALIDATE gate (ERC 0 errors, netlist partition match) must say PASS.
