# gb3 enclosure (code-CAD, build123d 0.13)

The gameboy-v3 handheld's enclosure, generated in Python from the KiCad board. It is a flat slab split at the board's
top face: 194.7 x 89.8 mm, **15.35 mm thick** in the baseline (Architecture A: flush cover glass, the 2000 mAh
LP-803860 cell on a 1.0 mm FR4 battery plate). The design, the stack and its sources, the variants and the open
questions are in `DESIGN.md`. The module contract is in `CONTRACT.md`.

| File | What it does |
|---|---|
| `export_board.py` | KiCad -> `build/board.json`: the board outline and holes, every part's hull and z, the switches, the edge features, the panel / cell / speaker / LRA envelopes. Run it with **KiCad's python** (see "After a board change"). |
| `params.py` | The single source of numbers: every stack input with its source, the `GB3_*` flags, every derived z, the outline, frame helpers, named positions and the shared contracts. Importing it prints the per-region thickness table and asserts the board data against the stack. |
| `context.py` | The bodies the enclosure fits round (board, 193 part envelopes, panel layers, flexes, cell, USB-C / jack / microSD, both FPC1 placements, LRA, speaker) and the core keep-outs. |
| `shell.py` | The two bare shells: the outline slab, hollowed, R2 front / R3 back fillets, split at z 1.6. |
| `front.py` | Front-shell features: module pocket, ledge, fold-bay skin, key holes, LED light pipes, CN1 thinning, bosses with insert holes, posts, jack and SKRT notches. |
| `back.py` | Back-shell features: cell bay + FR4 plate, fences, LRA ring, speaker cup + grille, USB-C / microSD / reset openings, screw bosses and counterbores, posts, the snap. |
| `controls.py` | The printed controls: A B X Y START SELECT caps, the D-pad + washer, VOL+ / VOL- / POWER edge caps, L / R shoulder bumpers. |
| `enclosure.py` | Assembles everything (`build()`), exports STEP / STL / 3MF / DXF, renders the PNGs, pushes to the OCP viewer when asked. |
| `render.py` | Headless PNG renderer (numpy z-buffer views, matplotlib sections). |
| `checks.py` | The fit checks (a b c d e f h j x; `DESIGN.md` section 6). |

## Setup (once)

The `.venv` is already set up. To recreate it:

```sh
cd projects/gameboy-v3/enclosure
/opt/homebrew/bin/python3.13 -m venv .venv
printf '[global]\nindex-url = https://pypi.org/simple\n' > .venv/pip.conf   # this venv only: public PyPI
.venv/bin/pip install -r requirements.txt
```

## Build

From this folder:

```sh
.venv/bin/python enclosure.py               # build + export + render, about 45 s
.venv/bin/python enclosure.py --check       # ... and the fast checks on the same build (+11 s, exit 1 on a FAIL)
.venv/bin/python enclosure.py --no-export --no-render    # build only, about 3 s after the imports
.venv/bin/python checks.py                  # the full checks, about 22 s; --fast skips (h) and the slab scan
.venv/bin/python -c "import params"         # the stack and the per-region thickness table only
```

Outputs (baseline):

- `out/assembly.step`: every part and context body, named and coloured. F / B part envelopes are grouped as "F parts" /
  "B parts".
- `out/<part>.stl` and `.3mf`: the printed parts in their print poses: front, back, A, B, X, Y, START, SELECT, DPAD,
  DPAD_washer, VOL_UP, VOL_DOWN, POWER, L, R. Bought parts (light pipes, ledge tape, battery plate) are STEP only.
- `out/battery_plate.dxf`: the FR4 plate outline (mm, layer Edge.Cuts; it lands at its KiCad position on import).
- `build/render_iso_exploded.png`, `render_top.png`, `render_bottom.png`, `section_y75.png`, `section_x160.png`.

## Variants

Each flag is an environment variable (the default is listed first). Every z follows from the stack inputs and the
flags, so a variant is a plain rebuild.

| Flag | Values | Total |
|---|---|---|
| `GB3_FRONT` | `flush` / `lip` (0.8 lip on a 0.1 gasket over the glass border) | 15.35 / 16.15 |
| `GB3_BACK` | `flat` / `hump` (back at -4.86 outside the cell bay, -6.80 over it) | 15.35 / 15.35 at the hump, 13.41 elsewhere |
| `GB3_CELL` | `LP803860` / `LP503759JU` / `ADA258` (thin cells: printed floor, flat back at -4.86) | 15.35 / 13.41 / 13.41 |
| `GB3_LCD_PLATE` | `0` / `1` (0.5 SUS304 plate between the LCM and the cell) | 15.35 / 16.00 |
| `GB3_PROCESS` | `mjf` / `fdm` (walls 1.5 / 1.67, FDM counterbore collars) | 15.35 / 15.35 |

```sh
GB3_FRONT=lip .venv/bin/python enclosure.py
GB3_BACK=hump GB3_CELL=ADA258 .venv/bin/python checks.py
.venv/bin/python tests/checks/run_variants.py      # checks.py under 8 flag sets, logs in build/checks/variants/
```

A variant writes its exports to `out/variants/<slug>/` and its renders to `build/variants/<slug>/` (slug = the
non-default flags, e.g. `front-lip`, `back-hump_cell-ada258`), so it never overwrites the baseline's `out/`.

## View in VS Code (OCP CAD Viewer)

The OCP CAD Viewer already listening on port 3939 belongs to another VS Code window (the gb2 enclosure). Never push to
it. `enclosure.py` only pushes when `OCP_PORT` is set and something is listening on that port.

1. In this window, open `projects/gameboy-v3/enclosure/enclosure.py` (click it, or
   `'/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code' -r projects/gameboy-v3/enclosure/enclosure.py`).
   It contains `from ocp_vscode import`, so the extension starts a viewer for this window on the next free port,
   usually 3940. The port shows in the status bar ("OCP: 3940") and in `~/.ocpvscode`. If nothing starts, run the
   command "OCP CAD Viewer: Open viewer". If VS Code asks for an interpreter, pick `projects/gameboy-v3/enclosure/.venv/bin/python`.
2. From this folder, push with that port:

   ```sh
   OCP_PORT=3940 .venv/bin/python enclosure.py --no-export --no-render
   ```

Always pass the port: with two viewers registered, an unpinned `ocp_vscode` push prompts and a scripted run hangs. The
viewer shows the shells at alpha 0.35, every part and context body named, and the F / B part envelopes as two groups.
`.vscode/settings.json` picks the `.venv` interpreter when this folder is opened as its own window
(`code projects/gameboy-v3/enclosure`).

## Print notes

- **Process.** Final shells: JLC3DP MJF PA12 (1.2 plate, 1.5 walls, 1.2 floor, the baseline). Fit prints: home FDM
  PETG or PLA with `GB3_PROCESS=fdm` (1.67 walls, D6.0 collars round the counterbores). For PLA, set `back.SNAP = False`:
  the snap blade strains 2.4 % at full deflection. JLC asks for 2.0 walls on 100-200 mm parts and holds about
  +/-0.4 % (+/-0.78 at 195 mm); the model keeps 1.5 inside the drawn outline (`DESIGN.md`, open questions).
- **Orientation** (the STL / 3MF are already posed): front shell face-down, back shell back-down; keys and D-pad
  head-down; the washer, edge caps and shoulder bumpers flat on their bottoms.
- **Supports.** MJF needs none. FDM: under the ledge (1.35 into the pocket) and the LED stop rings in the front shell,
  under each key's flange seat, and with `GB3_BACK=hump` under the body's back face round the hump (or print the hump
  as a separate tub).
- **Heat-set inserts** (M2 x 3, for D3.2 holes) go into the 7 front bosses from the split side, flush with the boss
  tip or 0.05 under it: the tips are the board's datum, and a proud insert lifts the board.
- **Bought parts:** the FR4 battery plate from `out/battery_plate.dxf` (JLCPCB 1.0 mm, no copper, black mask both
  sides, outline +/-0.2; bond it into its pocket from the back with 0.05 3M 467MP before the cell); two D2.0 clear
  acrylic rods, 5.49 long, for the LED light pipes (ream the D2.2 bore, glue flush); 0.15 die-cut acrylic tape for the
  ledge (the shape is the "ledge tape" body in the STEP); with `GB3_FRONT=lip` the 0.1 die-cut "lip gasket" instead;
  7 DIN 912 M2 x 10 screws (M2 x 8 on the 13.41 builds and with the hump); 5 PORON / EPDM washers D5.5 x D2.4 x
  0.3-0.5 on the H1-H5 back bosses (they clamp the board onto the datum; `DESIGN.md`); 0.15 removable tape under the
  cell; a 0.3 foam ring under the speaker lip; 0.05 transfer tape for the D-pad washer.
- **First print:** check the board fit, the key rest gaps (sand a key's flange seat if it is pre-pressed), the D-pad's
  0.7 hole clearance and the shoulder bumpers' travel before ordering MJF shells.

## Checks

`checks.py` builds the model and runs (a) interference and clearances (context vs context too), (b) keep-outs,
(c) rest gaps and the board datum, (d) one valid solid per part and the print poses, (e) thickness ray scans,
(f) the region table, (h) the assembly sweep, (j) press travel and D-pad rocking, (x) cross-module cuts. It prints
every reading and exits 1 on any FAIL. Each module also has a self-test: `.venv/bin/python front.py --no-render`,
`.venv/bin/python controls.py --no-render`, `.venv/bin/python tests/back/test_back.py`, and the checks' own tests
`.venv/bin/python tests/checks/test_checks.py`.

## After a board change

Regenerate `build/board.json` with KiCad's python. The script reads the board read-only and runs kicad-cli only on a
temporary copy; never edit anything under `projects/gameboy-v3/pcb/` from here:

```sh
/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 export_board.py
.venv/bin/python enclosure.py --check && .venv/bin/python checks.py
```

`params.py`'s asserts fail loudly if a part no longer fits the stack.

## Frame and conventions

Board mm, +X right, **+Y up** (KiCad's Y flipped: `P(x, y) = (x, -y)`), +Z toward the screen; the board's bottom face
is z 0, its top face z 1.6 (the split and the datum). Positions in the docs are KiCad numbers unless marked
"enclosure". Every shared number is a named constant in `params.py`.
