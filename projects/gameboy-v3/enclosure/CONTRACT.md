# gb3 enclosure: the module contract

The core (params.py, context.py, shell.py, enclosure.py, render.py) is shared. Three module files build features
against it in parallel, and checks.py checks the result. **Module agents do not edit the core files.** If a contract
value is wrong or missing, say so in your `notes` (they are printed on every build). The baseline is Architecture A:
a 15.35 mm flat slab, flush glass, and an LP-803860 cell on a 1.0 mm FR4 battery plate.

| file | owner | what it is |
|---|---|---|
| params.py | core | stack inputs (each with a source), flags, every derived z, the outline, frame helpers, named positions from board.json, **the shared contracts**, the region table and asserts |
| context.py | core | `context()`: the bought and board-side bodies; `keepouts()`: the core keep-outs |
| shell.py | core | `shells()`: the bare front and back shells |
| enclosure.py | core | `build()`, `export_all()`, `render()`, `push()`, `make_ctx()` |
| render.py | core | headless PNG renderer (numpy z-buffer for views, matplotlib for sections) |
| front.py | front module | front-shell features |
| back.py | back module | back-shell features, plus the FR4 battery plate part |
| controls.py | controls module | keycaps, D-pad, edge caps, shoulder bumpers |
| checks.py | checks module | `run(parts, ctx)` (a first pass from the core is in place) |

## Frame and units

- Units are mm, the same frame as gb2. +X is right. **+Y is up, which is KiCad's Y flipped:** `P(x, y) = (x, -y)`. +Z points toward the screen.
- The board's bottom face is at z 0 and its top face (F, the screen side) at z `BOARD_TOP` = 1.6. F parts grow up from 1.6 and B parts hang down from 0.
- Every position in params.py is already in this enclosure frame. To use a KiCad number from the brief, pass it through `P(x, y)`. For a KiCad rectangle, use `Rect.k(x0, y0, x1, y1)`; `rect.kicad()` converts back.
- Helpers (all in the enclosure frame):
  - `cyl(x, y, d, z0, z1)`, `box(cx, cy, w, h, z0, z1)`
  - `prism(face, z0, z1)`: always extrudes +Z.
  - `face(pts, board_frame=True)`: by default it takes KiCad points and flips them.
  - `yz_plate(pts_yz, x0, x1)`: a (y, z) profile extruded along X.
  - `Rect(...).prism(z0, z1, r)`, `outline_face(inset)`
  - `floor_in_at(x, y)` and `back_at(x, y)`: the local floor inner face and back face. They differ only in the hump variant.
  - `part_rect(ref)` and `part_outline(ref)`: a part's model hull, from board.json.
- **Never type a z value.** Use the names in params.py. Each one follows from the inputs and the flags, so the variants rebuild correctly.

## Variant flags (environment variables, read on import)

| flag | values (default first) | effect (total thickness) |
|---|---|---|
| `GB3_FRONT` | `flush` / `lip` | face at z 8.55, the glass max + 0.1 (15.35) / a 0.8 lip on a 0.1 gasket, face at z 9.35 (16.15). `LIP_WINDOW` is only set for `lip` |
| `GB3_BACK` | `flat` / `hump` | flat back at z -6.80 / the body back at the USB-C limit z -4.86, and z -6.80 only over `HUMP_RECT` (KiCad x 137.7..183.2, y 50.8..119.8) |
| `GB3_CELL` | `LP803860` / `LP503759JU` / `ADA258` | the thin cells give a flat back at z -4.86 (13.41), and the cell rests on the printed floor: `CELL["floor"] == "printed"`, no FR4 window |
| `GB3_LCD_PLATE` | `0` / `1` | `1` adds a 0.5 SUS304 plate (`SS_Z`) between the LCM and the cell. The cell hangs from it and swells toward the back (16.00) |
| `GB3_PROCESS` | `mjf` / `fdm` | `WALL` = 1.5 / 1.67 |

`params.FLAGS` holds the flag values and `params.VARIANT` is the one-line label. `python -c "import params"` prints the stack and the per-region thickness table for the current flags. That table is the check that would have caught gb2's 24.5 mm before its board was ordered. Set `GB3_QUIET=1` to silence the printout.

## The stack (baseline; params.py is the source)

| name | z | from |
|---|---|---|
| `Z_FACE` | 8.55 | `Z_GLASS_MAX` + `RIM` (lip: + `GASKET` + `LIP` = 9.35) |
| `Z_GLASS_MAX` / `Z_GLASS_NOM` | 8.45 / 8.15 | `Z_LCM_BACK` + module 4.25 (+ 0.3 tolerance) |
| `Z_PLATE_IN` | 7.35 | `Z_FACE` - `PLATE` 1.2 |
| `Z_PLATE_IN_CN1` | 7.55 | the plate thinned to `PLATE_CN1` 1.0 over CN1 (CN1 top 7.2) |
| `Z_FOLD_SKIN_IN` | 7.90 | the plate's inner face over the fold bay must be at or above this (`Z_GLASS_MAX` - 0.55); skin ≤ 0.65 |
| `Z_LCM_BACK` | 3.90 | `BOARD_TOP` + P1 2.1 + 0.2 air. The panel's LCM back sits here on the ledge tape (`Z_PANEL0`) |
| `LEDGE_Z0` .. `LEDGE_Z1` | 2.75 .. 3.75 | ledge top = `Z_LCM_BACK` - 0.15 tape; 1.0 thick |
| `SPLIT` = `BOARD_TOP` | 1.60 | parting plane = the board's top face = the datum (the front boss tips) |
| `Z_BOARD_BOT` | 0.00 | |
| `CELL_Z` | -5.65 .. 2.85 | the swollen top (`CELL_SWELL_Z` up to 3.70) stays 0.2 under the LCM back |
| `CELL["tape_z"]` / `CELL["floor_z"]` | -5.80 .. -5.65 / -6.80 .. -5.80 | 0.15 tape on the 1.0 FR4 plate, flush with the back face |
| `Z_FLOOR_IN` | -5.60 | printed floor inner face (`Z_BACK` + 1.2) |
| `Z_BACK` / `Z_BACK_MIN` | -6.80 | outer back face (hump variant: `Z_BACK` = -4.86, `Z_BACK_HUMP` = -6.80) |
| `TOTAL` | 15.35 | |

Also derived: `Z_BACK_BOARD` = -4.86 (USB-C 3.36 + 0.3 + 1.2) and `Z_BACK_CELL` (what the cell column needs). The panel layers are `PANEL["z_lcm"]` 3.90..6.90, `PANEL["z_touch"]` 6.90..7.50 and `PANEL["z_glass"]` 7.50..8.20 (parts sum 4.30).

## The module interface

Each of front.py, back.py and controls.py exposes:

```python
def build(ctx) -> dict:
    return {
        "front_add": [Shape, ...],   # fused into the front shell
        "front_cut": [Shape, ...],   # cut from the front shell
        "back_add":  [Shape, ...],   # fused into the back shell
        "back_cut":  [Shape, ...],   # cut from the back shell
        "parts":     {name: Shape},  # extra printed / bought parts AT REST in the assembly frame
        "keepouts":  {name: Shape},  # volumes the OTHER modules must keep their material out of
        "notes":     [str, ...],     # findings, open issues, contract problems (printed on every build)
    }
```

How enclosure.build() uses the result:

- **Validation.** Missing keys default to empty. An unknown key (a typo such as `front_adds`) raises an error. Every entry must be a build123d `Shape`.
- **Order.** Modules are imported if present, in the order front, back, controls. For each shell: `shell = bare + every module's adds`, then `- every module's cuts`. All adds go on before any cut, so no module's material can fill another's opening.
- **Any module may add to or cut from either shell.** For example, back.py cuts the USB-C relief from the front rim too, and front.py cuts the jack shoulder relief from the back rim. A cut also removes **other modules' material**, so keep cuts out of other modules' keep-outs. checks (x) reports any cut that removes another module's add.
- **Names.** Part names must be unique across modules and may not be `front`, `back` or a context name. Keep-outs are registered as `<module>:<name>`, next to the core ones (`core:<name>`).
- **Optional module-level hooks:**
  - `print_pose(name, solid)` returns the part as printed (enclosure.py drops it onto the bed at z 0 and centres it), or `None` for a bought part. A bought part is exported to the STEP only, with no STL or 3MF.
  - `COLOURS = {part name: colour}`.
  - `export(out_dir) -> [paths]`: extra files, written by `export_all()` into the build's export folder (back.py: `battery_plate.dxf`). `build()` itself writes no files.
- **Imports.** Module files import only `params`, `context`, `shell` and build123d / OCP. Never import `enclosure`: it imports you. A self-test can do `import enclosure; r = build(enclosure.make_ctx())` under `__main__`.

### ctx (an `enclosure.Ctx`; one object shared by every module)

| field | content |
|---|---|
| `ctx.params` | the params module |
| `ctx.flags` | `params.FLAGS`: `{"FRONT", "BACK", "CELL", "LCD_PLATE", "PROCESS"}` |
| `ctx.front`, `ctx.back` | the bare shells (`Solid`, labelled `front` / `back`) from `shell.shells()` |
| `ctx.context` | `{name: (shape, colour)}` from `context.context()` |
| `ctx.core_keepouts` | `{name: Shape}` from `context.keepouts()` |
| `ctx.modules` | filled after the modules run: `{module: its validated result dict}` |
| `ctx.keepouts` | filled after: `{"core:<n>" / "<module>:<n>": Shape}` |
| `ctx.part_owner` | filled after: `{part name: module}`; the shells map to `"shell"` |
| `ctx.timings` | `{step: seconds}` |

`enclosure.build()` returns `(parts, ctx)` and is cached. `parts` is `{"front": finished front shell, "back": finished back shell, + every module part}`.

`checks.py` exposes `run(parts, ctx) -> [(check, "PASS" | "FAIL", detail)]`. `python checks.py` builds, prints every result and exits 1 on any FAIL.

## Ownership: who builds what (with the params names to build it from)

**front.py**
- The module pocket `POCKET` (121.4 x 76.5, centred on the LCM) through the face. In the lip variant, the lip with `LIP_WINDOW` (active area + 0.95 per side) on a `GASKET` over the glass border.
- The ledge `LEDGE`: z `LEDGE["z"]`, reaching `LEDGE["w"]` under the LCM edge, on the `LEDGE["segments"]` `top`, `left` and `bottom_left` only. None over the cell or the fold. Relieve it over `LEDGE["relief"]` C20 / C94 (tops at z 2.4). Also the frame that carries the ledge from the plate.
- The fold-bay skin: over `FOLD_BAY["rect"]` the plate's inner face stays at or above `Z_FOLD_SKIN_IN`, and there are no ribs or bosses in it (core keep-out `fold bay`).
- The face key holes `key_hole_face(name)` for every name in `KEYS`, cut through the plate. Keep each key's column clear from `KEYSTAKE_Z` up to `Z_PLATE_IN`: the Ø `KEYS[n]["clear_d"]` circle for round keys, and `key_flange_face("DPAD")` grown by `KEY_SWEEP_CLR` (0.3) for the D-pad. controls.py declares its own sweeps as keep-outs too.
- LED windows over `LEDS["U5"]` and `LEDS["U14"]` (0603 LEDs, tops at z 2.16).
- The plate thinned to `PLATE_CN1` over `CN1["rect"]`: the inner face at `CN1["plate_in_z"]`, 0.35 over the top of CN1.
- Front bosses at every `BOSSES` entry: Ø `BOSS_D` from `SPLIT` up to the plate, with an insert hole `INSERT["hole_d"]` x `INSERT["depth"]` up from the tip. The tips at `SPLIT` are the board datum.
- The jack opening `JACK` (Ø `hole_d` 5.4 on `axis_z` 4.1, through the bottom wall, open at the split because the barrel comes in with the board). Also the `jack plug` keep-out: the plug's Ø6.0 shoulder seats on the barrel tip, which is 0.375 inside the outline, so the wall needs a spot-face there. Its z range 1.1..7.1 dips under the split, so cut the back rim too.
- The SKRT notches for `EDGE_KEYS` and `SHOULDERS`: `notch_w` wide along the wall, z `notch_z` (open at the split), through the wall at `outer` along `-dir`.
- Decided during the build (front.py's notes): **no ledge in the lip variant.** There the module goes in from inside, glass first against the lip, so a ledge would block it; it is held by the bought `lip gasket` tape plus foam pads on bare board behind the LCM. The front bosses are merged into the walls by gussets per `BOSS_MERGE_MAX` / `BOSS_MERGE_DIR` (H3 into the right wall), and ten D3 plate posts stop 0.2 over the board beside the keys (`front.POSTS`).

**back.py**
- The cell bay. When `CELL["floor"] == "fr4"`: a window in the floor, closed by the FR4 battery plate. **The plate is a part of back.py**, bought (`print_pose` returns `None`), at z `CELL["floor_z"]` and flush with the back face. Cell fences ≤ `CELL["fence_t_max"]` sit `CELL["fence_clr"]` off `CELL["rect"]`, inside `CELL["buffer"]` (the 2 mm notch gaps), with a lead gap (core keep-out `cell leads`).
- The LRA ring round `LRA` (`ring_id`; the LRA is glued on the floor at `LRA["z"]`).
- The speaker cup (`SPEAKER["cup_id"]` 27.4) and the grille (Ø `grille_d` 22) in the floor. The speaker fires back-down onto its foam ring (`foam_z`). Keep the cup wall ≤ 1.0 near the S2 seam screw, which is 0.3 away.
- The USB-C opening `USBC`: a stadium round the 8.94 x 3.36 shell on `axis_z`, **open up to the split** (the back shell comes up past the receptacle), plus the `relief` 13 x 7 x 0.3 overmold relief. The relief reaches z +1.87, so cut it from the front rim too. Core keep-out `usb_c plug`: the exterior within the 13 x 7 stays ≤ 0.40 off the receptacle face.
- The microSD slot (`MICROSD`, core keep-out `microsd card`; the latched card protrudes 1.30) and the reset pinhole `RESET` (Ø `hole_d`; core keep-out `reset pin`).
- Back bosses and holes at every `BOSSES` entry: an M2 clearance hole `SCREW["clear_d"]` and a counterbore `SCREW["cbore_d"]` from the back face. **On-board bosses (H1-H5) stop at `back_top_z` = -`BACK_BOSS_GAP`**, so the screw pulls the board onto the front datum. **Off-board seam screws (S1, S2, along the bottom edge) rise to `SPLIT`** and meet the front boss tips.
- Decided during the build (back.py's notes): one releasable snap hook on the bottom wall at KiCad x 81.8 (into a pocket back.py cuts in the front wall), so the bottom edge has S1, S2 and the snap; D4 posts 0.2 under the board at the D-pad pivot and the ABXY centre; FDM builds get a D6.0 collar round each counterbore (`back.COLLAR_D`) so its wall is 0.9, not 0.65. Note: the screw passes through the board's hole, so the 0.2 gap means nothing presses the board onto the datum; a 0.3-0.5 PORON washer on each H1-H5 back boss does (DESIGN.md).

**controls.py**
- Face caps A, B, X, Y, START, SELECT from `KEYS`. The head has diameter `d`, stands `KEY_PROUD` over the face and sits in `key_hole_face`. The flange `flange_d` x `FLANGE_T` is at `flange_z`, under the plate. The stem tip is at `tip_z` (`rest_gap` 0.15 over the keystake at `KEYSTAKE_Z` 3.10). Caps go in head-first from inside (gb2 style).
- The D-pad cross `KEYS["DPAD"]`: `arm` 9.4, `w` 6.6, `hole_clr` 0.7, legs to the four `contacts` (tip `tip_z`, rest gap 0.10), a centre pivot leg Ø `pivot_d` resting on the board top (`pivot_z`). The F side is free within 4.7 of the centre.
- SKRT edge caps VOL_UP, VOL_DOWN and POWER from `EDGE_KEYS`. They ride the notches; the contact face is `rest_gap` 0.15 short of the plunger `tip` along `dir`; the face is `proud` 0.5 out of the outer surface; the plunger axis is at `axis_z` 3.3.
- Shoulder bumpers L and R from `SHOULDERS`. Their tips are 6.22 inside the outline, so they need a long stem or a hinge; `notch_w` is 12.
- A `print_pose()` hook.
- If a control needs material in a shell (a hinge boss, a guide), return it in `front_add` and declare its sweep volumes as `keepouts`.

**checks.py**
- The plan's (a) interference, including context vs context, (d) solids, (f) minimum walls / ledge / skin, then (b), (c), (h), (i), (j) as in gb2.
- The core's first pass is already there: (d), (k) keep-outs, (x) cross-module cuts, and (a0) shells vs context. It includes `overlaps(shape, bodies)`, a grouped-boolean prefilter (about 3 s per shell against 210 bodies).

## Shared contract reference (params.py)

- `KEYS[name]`
  - round keys: `shape`, `switch`, `role`, `at`, `contacts`, `d`, `hole_d`, `flange_d`, `flange_z`, `rest_gap`, `tip_z`, `top_z`, `clear_d`
  - DPAD: `shape`, `switch`, `role`, `at`, `contacts`, `arm`, `w`, `corner_r`, `hole_clr`, `flange_lip`, `flange_z`, `pivot_d`, `pivot_z`, `rest_gap`, `tip_z`, `top_z`
  - outlines: `key_hole_face(name)` and `key_flange_face(name)` (faces at z 0)
  - key numbers: A Ø8.0 (hole 8.4, flange 10.0), START / SELECT Ø5.0 (5.4 / 7.0); caps z 3.25..9.95
  - mapping: Y = SW6, A = SW7, X = SW8, B = SW9, START = SW10, SELECT = SW11; D-pad left / right / up / down = SW2 / SW3 / SW4 / SW5, centred on KiCad (36.5, 74.9)
  - deviation, decided by controls.py and verified by checks (j): the D-pad flange does not sit at `KEYS["DPAD"]["flange_z"]`; it rides `controls.DPAD_FLANGE_Z` (z 5.43..6.23, 1.12 under the plate) so the cross can rock a switch to its hard stop on a diagonal. Its lateral sweep below the plate is `controls.DPAD_SWEEP_CLR` (0.68), more than `KEY_SWEEP_CLR`; front material stays out of `controls:DPAD sweep`.
- `EDGE_KEYS[VOL_UP / VOL_DOWN / POWER]` and `SHOULDERS[L / R]`: `switch`, `role`, `tip`, `dir` (outward unit), `inset` (tip to the outer surface: 3.92 / 6.22), `outer`, `inner` (on the wall's inner face), `contact`, `axis_z`, `notch_w`, `notch_z`, `rest_gap`, `proud`.
- `BOSSES[H1..H5, S1, S2]`: `at`, `on_board`, `front_z`, `back_top_z`.
  - S1 is at KiCad (136.0, 116.3), between the fold bay and the cell buffer, 0.4 from the pocket. S2 is at (209.6, 115.6), in the speaker cut-out's corner.
  - `BOSS_MERGE_MAX` (4.0) and `BOSS_MERGE_DIR` (`{"H3": (1, 0)}`): front.py merges a boss whose gap to a wall is under `BOSS_MERGE_MAX` into the nearest wall, or into the wall `BOSS_MERGE_DIR` names; controls.py routes the shoulder yokes round those blocks from the same two names (so the R yoke runs past H3 to the corner arc).
  - Hardware: `BOSS_D` 5.5; `INSERT` `hole_d` 3.2, `depth` 4.0; `SCREW` `clear_d` 2.4, `head_d` 3.8, `head_h` 2.0, `cbore_d` 4.2; `BACK_BOSS_GAP` 0.2.
  - The board is located laterally by the M2 screws in its Ø2.2 holes (±0.1).
- `POCKET` (Rect), `LIP_WINDOW` (Rect or None), `LEDGE` (`z`, `w`, `segments{top, left, bottom_left}`, `relief{C20, C94: rect, top_z}`), `FOLD_BAY` (`rect`, `z`, `skin_max`), `CN1` (`rect`, `top_z`, `plate_in_z`, `mouth_y`, `plug`, `plug_z`), `LEDS[U5 / U14]` (`at`, `rect`, `top_z`).
- `USBC` (`x`, `mouth_y`, `z`, `axis_z`, `shell_w`, `shell_h`, `shell_l`, `opening`, `relief`, `face_max`), `JACK` (`x`, `tip_y`, `axis_z`, `barrel_d`, `barrel_l`, `body`, `hole_d`, `plug_d`, `shoulder_d`, `z_top`), `MICROSD` (`x`, `face_y`, `z`, `card_w`, `card_t`, `card_z`, `out_latched`, `out_ejected`), `RESET` (`switch`, `at`, `keystake_z`, `pin_d`, `hole_d`).
- `CELL` (`name`, `label`, `t`, `swell`, `rect`, `buffer`, `z`, `swell_z`, `tape_z`, `floor`, `floor_z`, `fence_t_max`, `fence_clr`, `leads`), `HUMP`, `HUMP_RECT`, `HUMP_INNER`, `SS_Z` (LCD plate), `LRA` (`at`, `d`, `t`, `ring_id`, `z`), `SPEAKER` (`at`, `d`, `lip_d`, `t`, `lip_h`, `foam`, `cup_id`, `grille_d`, `foam_z`, `lip_z`, `z`).
- `PANEL` (`lcm`, `glass`, `sensor`, `active`, `view`, `max`, `z_lcm`, `z_touch`, `z_glass`), `FLEX["display" / "touch"]` (fold-model paths and widths), `FPC1` (`placed`, `corrected`, `z`, `lid_y_k`), `HOLES`, `SWITCHES` (all 16, enclosure frame), `PARTS` (board.json by ref), `INPUTS` (name -> value with `.src`).

## Context bodies (`ctx.context` keys)

| name | what |
|---|---|
| `board` | the board, with H1-H5 drilled |
| `F:<ref>`, `B:<ref>` | 193 part envelopes: hull prisms over z_design |
| `usb_c`, `jack`, `microsd`, `microsd card (latched)` | from the edge features, not the 3D models |
| `FPC1` (as built since board step 7) | rot 180 at KiCad (118.5, 72.8), lid edge y 76.28; a board without the fix gives `FPC1 as placed` + `FPC1 corrected` (`params.FPC1_KEY` names the one the tail plugs into) |
| `panel LCM`, `panel touch`, `panel glass`, `panel active area` | the panel layers |
| `display flex`, `touch flex` | 0.3 strips; the touch flex includes its ≤ 1.45 IC area under the board and ends at the FPC1 lid |
| `cell`, `lcd plate` | `lcd plate` only with `GB3_LCD_PLATE=1` |
| `lra`, `speaker`, `speaker foam` | |

Contacts between context bodies are all intended: parts on the board faces, the flex ends at P1 / FPC1, the panel layers, the speaker foam, and the microSD card. With the board's step 7 there is no real overlap (before it, B:R29 x FPC1 corrected, 0.004 mm³, was allowed). Pad-level clearances (FPC1's tab to U16.1: 0.18) do not show at envelope level.

## Core keep-outs (`ctx.keepouts["core:<name>"]`)

No module may put material in any of these:

| name | what it keeps clear |
|---|---|
| `fold bay` | KiCad x 84.5-133, y 112.6-117.5, z -1.6..7.90: no ribs or bosses, and the plate over it stays thin |
| `cell swell` | the cell's swell allowance plus the air under the LCM |
| `usb_c plug` | the 13 x 7 overmold from 0.40 off the receptacle face outward, and the plug tongue |
| `jack plug` | the Ø3.6 plug and the Ø6.0 shoulder from the barrel tip outward |
| `microsd card` | the card path, latched to ejected and the finger beyond |
| `reset pin` | the pin column up to SW1's keystake |
| `cell leads` | the PHR-2 plug in CN1, over the board edge, down the notch corner outside the panel, along the cell's top-end gap from the PCM exit (±8) |

## Assembly order (for designs and the checks' sweep)

1. The keycaps, D-pad and edge caps go into the face-down front shell, head-first from inside.
2. Flush front: the module goes in from the face. A straight drop does not pass its flexes (0.35 / 0.55 from the LCM / glass edge to the pocket), so stand it on its glass bottom edge with the flexes hanging into the cavity and swing it flat about that edge onto the ledge tape (`front.swing_in()` sweeps it). Lip front: it goes in glass-first from inside onto the lip gasket.
3. The display tail goes into P1 with the board held above; the D-pad washer is taped round the pivot spot.
4. The cell is plugged into CN1.
5. The board is lowered onto the front boss tips at z 1.6.
6. The touch tail wraps round the notch edge into FPC1 (corrected position).
7. The cell goes onto its FR4 plate (bonded into the back shell's pocket first) and the back shell closes from below; the snap clicks in.
8. M2 screws from the back at H1-H5, S1 and S2.

The back shell comes up past the USB-C receptacle, so its stadium must be open to the rim. The jack barrel comes in with the board into the front shell's rim notch.

## Status (integration pass, 2026-10-15)

The bare-shell to-do list this section used to hold is done. `checks.py` passes all 9 checks on the baseline and on GB3_FRONT=lip, GB3_BACK=hump, GB3_CELL=LP503759JU, GB3_CELL=ADA258, GB3_LCD_PLATE=1 and hump + ADA258. So does GB3_BACK=hump GB3_PROCESS=fdm. GB3_PROCESS=fdm on the flat back fails (e) on one documented spot: S1's counterbore breaks out through the R3 back fillet, and the wedge above the break-out is 0.80 thick, under FDM's 0.86 (DESIGN.md, open question 8).

## Rules

- Write only inside projects/gameboy-v3/enclosure/. Never touch projects/gameboy-v3/pcb/, projects/gameboy-v2/ or cad/. No git commands that change state.
- Never push to the OCP viewer on 3939. `push()` sends only when `OCP_PORT` is set and that port is listening.
- A full build (`python enclosure.py`) must stay under about 3 minutes. It takes about 30 s today: modules, exports and renders.
- Every z comes from params.py names. Every number that two modules share comes from params.py.

## Running

```
.venv/bin/python enclosure.py [--no-export] [--no-render] [--check]
    out/assembly.step                 every part + context body, named and coloured
    out/<part>.stl + .3mf             printed parts, in print poses
    out/battery_plate.dxf             the FR4 plate outline (back.py's export hook; FR4 builds only)
    build/render_iso_exploded.png, render_top.png, render_bottom.png
    build/section_y75.png, section_x160.png
    --check                           also run checks.run(fast=True) on the same build (exit 1 on any FAIL)
.venv/bin/python checks.py            checks; exit 1 on any FAIL
.venv/bin/python -c "import params"   the stack and per-region thickness table
GB3_BACK=hump GB3_CELL=ADA258 .venv/bin/python enclosure.py      a variant (flags as above): its exports and
                                      renders go to out/variants/<slug>/ and build/variants/<slug>/
                                      (params.VARIANT_SLUG, e.g. back-hump_cell-ada258), never over the baseline's
OCP_PORT=<this window's port> .venv/bin/python enclosure.py      also push to that viewer
```
