# gb2 enclosure — design spec

Decided 2026-10-13: **GBA-flat** — one flat front face with the screen lens flush, the GBA silhouette, the GBA's
24.5 mm thickness, rounded back edges. Printed FDM (PLA/PETG, 0.4 mm nozzle). Every number below comes from the
2026-10-13 research pass (datasheets, mech_ref.json, the board; notes in `research/`) unless marked *choice*.
Structural review and its tested fixes: `STRUCTURAL_REVIEW.md`.

Why not a GBA-shaped back taper: our board sits at the back, full width, so a tapered back would cut through the board
in the outer ~25 mm of each grip (`build/diagrams/flat_front_gba_back.png`). Grips stay full thickness.

## 1. Frame and global parameters (`params.py`)

Board mm, +X right, **+Y up** (KiCad's Y flipped), Z up; board bottom z = 0, board top z = T = 1.6.

| Name | Value | Note |
|---|---|---|
| `Z_FRONT` | 20.9 | front outer face; total 24.5 = the GBA |
| `Z_BACK` | -3.6 | back outer face = BT830 (1.7) + 0.3 + 1.6 floor |
| `SPLIT` | 1.6 | parting plane = board top: front shell z 1.6..20.9 (prints face-down), back shell z -3.6..1.6 (prints back-down) |
| `WALL` | 2.0 | side wall; the board edge is 2.5 inside the outer surface (0.5 clearance) |
| `FLOOR` | 1.6 | back floor (inner face z -2.0) |
| `PLATE` | 1.6 | front face plate (inner face z 19.3) |
| `R_OUT_FRONT`, `R_OUT_BACK` | 2.0, 3.0 | outer perimeter fillets (R3 tested OK; R4 fails on this outline) |
| `CLR` | 0.3 | general print clearance; 0.2 sliding fits, 0.15 press fits |
| `GLASS_TOP` | 18.27 nominal / 18.9 worst | 2090 cover glass; re-measure the real module |

Outline: `board.json mech.silhouette` exactly (the shell's OUTER surface). After the Y flip the loop is clockwise:
always `extrude(..., dir=(0, 0, 1))`.

## 2. Architecture and assembly

Front shell (face plate, side walls, all control guides, bosses with M2 heat-set inserts) + back shell (floor, side
walls up to the split, battery bay, speaker cradle, screw counterbores) + printed parts (D-pad + its pivot washer, A,
B, Start, Select, Vol±, L, R caps, power slider, J1 hatch cover) + bought parts (77.5 × 56 × 1.5 cast acrylic lens,
Ø2.5 × 17.7 clear acrylic rod, 4× M2×6 screws, 4× M2×3 heat-set inserts, 4× M2.5×11 F-F standoffs ≤ 5 mm across + M2.5
screws, 0.13 acrylic transfer tape, 0.3 foam tape).

Assembly (rev 2.1 — `checks.py` (h) sweeps every step along its insertion path):
1. Lens taped into its pocket. Front shell face-down on a spacer frame ≥ 2.5 tall round the keys (or foam with
   reliefs): the key heads stand 1.8 proud, and a flat bench would push their tips below the board top.
2. D-pad, A, B, Start, Select in head-first from inside; edge caps and the power slider into their rim notches — hold
   the caps outward until the board is in (their plunger tips arrive 0.15 off), and set the slider to the same end as
   SW1's lever (both east = OFF). Speaker into its ring, lead exit north (into the back cradle's gap at 90°).
   Light-pipe rod in from the face.
3. Board: pivot washer taped onto the GND pour round the D-pad centre (its rim drops between SW2–5); cell plugged into
   J8 **before** the 2090 goes on (J8 ends up under the 2090 PCB); 2090 on its standoffs; cell taped into the back
   shell's fence. Board component-side down onto the boss tips, between the six fingers; cell leads from the fence's
   lead gap over the board edge along the free J4 strip (x 128.25–132.1); speaker leads dressed south, under B's tube.
4. Back shell folded over on the 100 mm cell leads (about 65 mm slack); 4 screws from the back.

**Board location (rev 2).** Every switch is on the board and every cap is in the front shell, so the board is located to
the *front* shell: six fingers (`BOARD_LOCATORS`, `FINGER`) hang from the front wall below the split into the 0.5 mm
gap round the board edge, 0.1 off it; the back wall has a pocket behind each. The screws only clamp (their 2.4 mm
holes alone would let the board shift ±0.25, enough to leave a side key dead or pressed).

## 3. Front shell

- **Screen (rev 2: the research's option C').** No printed plastic over the glass: the plate is cut through over the
  panel + FPC fold + clearance (`OPENING`, x 60.41..131.51, y -96.4..-45.6). A 77.5 × 56.0 × 1.5 clear cast acrylic
  lens (`LENS`, centred on the device) sits flush in a pocket (lens + 0.2 per side, depth 1.5 + 0.13 tape → floor
  z 19.27, lens bottom z 19.4 = glass worst 18.9 + 0.5) on a land 2.5–3.9 mm wide outside the opening. The land is the
  top of a frame hanging from the plate down to z 17.0 (`LAND_Z0`; the module's header pins reach 16.0, its standoff
  screw heads 16.4). A black mask on the lens back leaves the 60.6 × 46.2 R1.5 window over the **active area**
  (92.61, -71.0) — 2.64 mm left of the device centre (board-side choice, see README). The lens + mask outline is
  exported as DXF/SVG for laser cutting.
- **Keycaps:** holes at the switch centres (D-pad (39.95,-64.09) cross; A (156.88,-61.92); B (143.94,-66.25); Start
  (49.49,-83.82); Select (49.44,-91.82)): TS-1187A keystake top z 3.10, travel 0.2 ±0.1, hard stop at 0.3. Each cap
  is one piece (head 1.8 proud, flange under the plate, stem with a flat Ø2.4 tip 0.20 above the keystake), so it goes
  in head-first from inside: its guide tube (plate → z 7.0) has a bore that passes the flange (`guide_id` 12.2 A/B,
  6.8 Start/Select), and a guide ring on the stem (z 8–11, OD = the flange's, 45° top taper) rides that bore so the
  stem stays straight. B's tube has a 38.9° sector of its wall removed over its full height, toward the speaker (the
  speaker drops into its ring past it); a full ring rather than fins, so nothing can drop into that gap.
- **D-pad (rev 2.1):** one cross (arms 9.4 mm from centre, 6.6 wide), four Ø2.6 legs with R2.5 domed tips 0.10 above
  the four switches (8.000 mm from centre) + a centre pivot leg onto the board's top soldermask at z 1.6 (plain GND
  pour there: no pads within 5.3 mm). A printed washer (Ø10.7 × 0.8, Ø2.9 hole) taped round the pivot point is
  centred by the four switch bodies and holds the pivot within 0.085, so the cross sits at most 0.285 off centre.
  Hole: the sharp-jointed cross offset 1.3 (R0.3 corners): the cross sways 0.97 axial / 1.36 diagonal at the face
  when a leg takes its switch to the 0.3 stop; `checks.py` (j) rocks it at ±0.285 off centre with no binding.
  Nothing under the flange (it sweeps z 16.5–19.3), so the D-pad drops in.
- **LED:** Ø2.6 hole at D1 (144.14, -44.70) + a tube (ID 2.7) down to z 2.4 with a Ø2.0 stop at z 2.4–3.2; a Ø2.5 clear
  acrylic rod (17.7 long) goes in from the face, rests on the stop, glued flush.
- **Speaker grille:** slots inside Ø22 centred on the speaker (151.45, -85.10); a locating ring under the plate (ID 27.4,
  wall 1.0, 2.0 deep), notched (34°) where B's guide tube passes; 0.3 foam on the speaker rim seals against the plate.
  7 GBA-tilted slots, 1.2 wide.
- **J1 hatch:** 13.4 × 3.4 opening over the SWD header (x 116.9–130.3, y -37.0), a 0.8 ledge, and a 0.8-thick snap-in
  printed cover with a fingernail notch (backed by a 0.8 pad under the plate).
- **Screw bosses:** 4 (board holes (57.39,-35.47), (133.19,-35.45), (29.24,-91.91), off-board (140.5,-100.5)): Ø6
  from the plate down to z 1.6, insert hole Ø3.2 × 4.0 at the tip, gussets to the wall; top two merged into the top
  wall (with a skirt down to the split outside the board edge); top-right D-cut toward J1 (3.3 mm away).
- **Board locators:** the six fingers (section 2), 3.0 wide, 1.1 thick, z 0.3–1.6 with a rib up the wall to z 3.0,
  0.3 × 45° lead-in at the tip.
- No ribs under the plate: the tubes, bosses and the screen frame stiffen it, and every free zone is a keep-out.

## 4. Back shell

- **Locator pockets:** behind each of the six front-shell fingers, a pocket in the wall's inner face (0.9 deep: 0.2
  behind the finger, as round its sides and tip; 1.1 of wall left; z 0.1–1.6).
- **Board posts:** Ø4 posts from the floor to 0.05 under the board beneath A (SW6) and B (SW7), which are 26–31 mm
  from the nearest screw: a hard press would otherwise bend the board ~0.2 into the J4 header and switch joints.
- Floor inner z -2.0. Reliefs 0.5 mm under the four 2090 standoff screw heads (z -1.4…-1.75).
- **Battery:** rests on the floor, z -2.0…6.5 (cell 62 × 38.5 × 8.5 at (95.25, -86.5)); one 1.2 mm fence ring 0.4 off
  the cell, up to z 4.0, standing in the 2.0 mm cut-out gap (its bottom side is the stop); foam tape; a 10 mm gap in
  the right side for the PCM leads (toward J8).
- **Speaker cradle:** three 40° arc fingers (at 30/150/270°, r 13.7–14.7, up through the board's speaker cut-out with
  0.3 to its edge, to z 15.6) with seat lips at the speaker's bottom (z 14.1) and a 0.5 × 45° lead-in; the gap at 90°
  faces J5 for the leads. Holds the speaker (Ø27 × 4.9, z 14.1–19.0) against the front seal (inner face 19.3 − 0.3).
- **Screw bosses:** Ø6 × 2.0 (z -2.0…0), Ø2.5 clearance hole, counterbore Ø4.2 × 2.0 from the back face; the
  off-board boss rises to z 1.6.

## 5. Controls on the edges

All edge actuators sit at z 1.6–4.9, above the split → notches in the front shell's rim. (They have to: the caps reach
inside the board-edge line, so the shell that carries them must come from the board's top side.)
- **Vol+ / Vol− (SW10/SW11), L / R (SW12/SW13):** SKRT plunger axis z 3.3, travel 0.2, tip 0.82 short of the wall's
  inner face. Captive caps in 4.0 × 3.6 notches, 0.6 proud, rest gap 0.15 to the plunger tip (the board locators
  hold it to about ±0.2, and 0.6 of push always trips). The cap's lip sits in a 0.3-deep recess (4.8 wide, z 1.6–6.8)
  in the wall's inner face, so the cap is ≥ 0.8 thick behind the wall. No over-travel stop: a hard press bottoms the
  plunger (thumb force only).
- **Power slide (SW1):** lever 1.3 × 1.0 at z 1.8–2.8, travel 1.6 along the 13.36° bottom-left wall, tip 0.97 short of
  the inner wall. Captive printed slider in a 4.0 × 2.2 slot (z 1.6–3.8) at (41.96, -103.76), with a fork around the
  lever.
- **USB-C (J7):** mouth centre (95.25, z 3.25); 9.5 × 3.8 stadium through the wall (z 1.35–5.15), open at the rim
  below its centre line (J7's shell overhangs the board edge 0.91 into the wall, so the board could not drop in past a
  closed stadium), + a 13 × 7 overmold relief 1.1 deep on the outside (the receptacle face is 1.09 behind the outer
  surface). Use the pads, not J7's 3D model (2.22 mm off).

## 6. Checks (`checks.py`, run on every build; exit 1 on any FAIL)

(a) interference: every printed part vs every other, the board, every part envelope (real J7 / SW1 shapes), the
2090, battery, speaker, LRA; (b) every module's keep-outs vs every other module's material; (c) rest gaps (keys,
D-pad legs + pivot + washer, edge caps, slider fork); (d) one valid solid per part + print poses; (e) lens to glass
≥ 0.5, nothing over the panel or FPC fold; (f) wall to board ≥ 0.45, fingers to the board edge and the back shell;
(h) the section 2 assembly order swept step by step; (i) min wall ≥ 0.8 (ray scan); (j) D-pad rocking, off centre.
`--fast` skips (h).

## 6a. Printing

- Front shell face-down; support only inside the lens pocket (the land is a 2.5–3.9 cantilever 1.63 above the bed —
  sand it flat, the pocket depth is exactly lens + tape). Back shell back-down.
- Keys and D-pad head-down: support under the flange seat (0.2 interface gap). Edge caps, slider, washer, cover flat.
- Test-print the board fit first: FDM XY shrink of 0.2–0.5 % is 0.3–0.7 mm over the fingers' 140 mm span, and they
  leave 0.1 — set the slicer's XY compensation from the test.

## 7. Open questions (settle with a test print / the real parts)

- The 2090's real stack height (glass top, module back) — re-measure, then set `GLASS_TOP`.
- Keycap feel with 0.2 mm travel and ~17 mm plungers — print a test coupon of one guide + plunger + switch.
- The 2090 revision (original's 2.65 mm parts over the cell vs rev D's 2.0).
- The D-pad's 1.3 visible gap at the face (the GBA's is about 0.5) — judge it on the test print.
- The MSK12C02 slider detent (the fork moves the lever 1.35 of its 1.6 throw) and the SKRT's overload strength (no
  over-travel stop on the edge caps) — check on real parts.
