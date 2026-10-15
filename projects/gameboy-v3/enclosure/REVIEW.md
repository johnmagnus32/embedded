# gb3 enclosure, first model: adversarial mechanical review (2026-10-15)

An independent review of the first model (commit 72a4bfb), done before the board's step 7 moved FPC1. Its scripts,
FE models and section images are in `~/.cache/gb3-encl/review/` (r02-r16, `fea_common.py`, `out/`), not in the repo.
Method: checks.py's ray engine for thickness envelopes, build123d `distance_to` for clearances, `render.section` for
19 sections, a Morley-plate FE model of the panel, and solid FEA (gmsh + scikit-fem P2 tets) on STEP exports of the
shells, PA12 E 1700 MPa, linear, no contact; mesh convergence checked.

**As built since step 7** (dd7c895): FPC1's lid edge is at y 76.28, not 77.08, so the tail ends at y 72.18 and the touch
U-turn sits 0.26 from the display loop (0.16 beyond the 0.1 air gap the model keeps); FPC1 is 0.30 from U16.
checks.py passes 9/9 on the as-built board.

## Verdict

The model is internally consistent: checks.py passes 9/9 on the baseline and on hump, ADA258, lip, LCD plate and
hump + FDM; FDM on the flat back fails only the S1 counterbore wedge (0.803 < 0.86, in DESIGN.md). Every documented gap
reproduces. The 15.35 mm slab is real: 16.75 over the keys and D-pad, nothing proud of the back.

It is not ready for final MJF shells. The checks prove geometry at rest; the design fails on physics and tolerance in
four places they can't see:
1. **Panel mount:** the panel sits on 2.4 % of its back and its right 50 mm floats over the cell.
2. **In-plane fits:** screw play 0.1, the edge-key gap 0.15 and the 0.05 left in the D-pad hole budget are all smaller
   than MJF scale error.
3. **Twist:** a picture-frame front and an open tray joined by a butt seam with no fastener for 160 mm along the top:
   2.2-5.7x less twist stiffness than a closed seam.
4. **Back floor:** the unribbed floor moves 3-3.7 mm under a 20 N palm and reaches B-side parts at about 22 N.

Most fixes cost no thickness. Two are board decisions for this order: the mounting-hole scheme and GND pads for the LCM.
A fit print is fine now.

## Blockers

**B0: the FPC1 fix and a tail mock-up.** Done in the board (step 7); the fold model's lid window comes from a ±1.2 mm
model, so wrap the real tail (or a 53.4 mm paper strip) round a 1.6 mm board edge before ordering.

**B1: the panel is neither backed nor properly retained.**
- The ledge has only top, left and bottom-left segments (`params.py` LEDGE); the frame walls over the cell
  (`front.py` right_cell / bottom_cell) stop at z 4.00, above the LCM back.
- Support map: 219 of the LCM back's 9149 mm² is on tape (2.4 %). 2135 mm² floats 1.05 over the fresh cell, the rest
  1.35-2.3 over the flexes and board; the bottom-right corner is 75 mm from any support.
- A touch at the bottom-right of the active area (170, 105) deflects 0.27-0.59 mm/N: it reaches the fresh cell at
  1.8-3.9 N and an end-of-life cell (0.2 gap) at 0.34-0.74 N, i.e. ordinary touches. ATM0500D27 #8e asks for mounting
  "without bending or twisting"; #10d warns that pressing the viewing area makes segments look "fractured".
- A face-down drop at 300 g puts about 150 N into 219 mm² of PSA (0.69 MPa average, peaking at the strip ends) on
  porous MJF.
- **Fix (no thickness):** bond the LCM's right edge (y 54.6-112.8) and its bottom edge east of the fold bay
  (x 133.3-177.7) to the existing frame walls, which already overlap the LCM's sides by 2.9 mm (z 4.0-6.9) across a
  0.35 gap. Use a soft addition-cure silicone or flexible UV acrylic (no oxime / alkoxy silicone, no amine epoxy:
  datasheet #14). Ideal bond: about 0.002 mm/N at (170, 105). Cost: the panel can no longer be removed.
- Alternatives: the LCD plate (+0.65, 16.00), but only if actually supported (today it floats); or the lip front.
  Prime MJF under the PSA (3M 94) or use a foam-core tape.

**B2: in-plane tolerances tighter than MJF scale error (decide before the board order).**
- Five D2.2 holes for M2 (0.1 radial play) over 181.8 x 75.8 mm: the worst-hole misfit is 0.11 / 0.23 / 0.34 / 0.45 mm
  at 0.1 / 0.2 / 0.3 / 0.4 % shell scale error (JLC quotes ±0.4 %, HP ±0.2 %): screws bind or cross-thread.
- 0.3 % moves the right wall 0.26 against the edge keys' 0.15 rest gap + 0.2 travel (VOL / POWER pre-pressed or
  dead); the D-pad hole 0.28 against the 0.05 left; round keys 0.19-0.28 against 0.2; board to wall 0.37.
- **Fix:** board, now and free: keep H3 at D2.2 as the datum (next to the edge keys and R), make H1 a D2.2 x 3.0 slot
  aimed at H3, open H2, H4, H5 to D2.8 (inside the r2.75 keep-out). Shells: print one MJF fit set, measure the hole
  pattern and wall-to-wall, add a scale factor to params. Raise the D-pad hole_clr to about 0.85.

## Major

- **M1, seam:** no fastener for 160.5 mm along the top edge (x 20 to H2) and 53.5 mm on the right edge; check (c) only
  looks at the bottom edge. Twist stiffness (N·m/°, left wall clamped, 1° at the right): front alone 0.009, back alone
  0.067, unconnected 0.077, bonded at the 8 fastener patches 0.197, everywhere but the top edge 0.263, fully bonded
  0.442. As built about 2.5-6.5° at 0.5 N·m against 1.1° closed. Fix: a stepped lap round the rim (0.75 + 0.75, about
  1.5 deep, outside the board edge) plus 2-3 hooks on the top wall; add a top-edge fastener check.
- **M2, cell window:** R0.4 corners and a sharp 0.6 step at the lead cut. 39-44 MPa at its top-left corner at 0.5 N·m
  (back alone), 12-19 MPa assembled (PA12's 48 MPa at 1.1-1.7 N·m); 20 N on the right grip gives a singular 131 MPa at
  the step. The plate bond is a 0.4 lap of 0.05 tape (about 80 mm²). Fix: R2+ corners, no step, PLATE_LAP about 1.2,
  epoxy or a conformable 0.1-0.2 tape so the FR4 closes the window structurally.
- **M3, back floor:** 20 N on a D20 patch: 3.68 mm at the centre, 3.52 under U1 (gap 4.0), 3.03 under D3 / L2 (gap
  3.15-3.4: contact at 21-23 N), 1.2-1.8 at the grips; only 2 back posts. Fix: 6-8 more D4 posts stopping 0.2 under bare
  B-side spots (≤ 30 mm spacing) and 1.5-2.0 mm ribs where there is 2.2 of room. No thickness cost.
- **M4, no over-travel stops:** at the switch hard stop the face keys still stand 0.95 proud, edge caps and shoulders
  0.15; the TS-1187A and SKRT take every drop. Fix: shell stops about 0.05 past each switch's stop (edge caps: a rib
  from the plate with its face at x 211.03 down to z 5.2; the same for the L/R yokes; stop pads under the face-key
  flanges).
- **M5, shoulders:** travel needed at the face ends, nominal / worst SKRT: L 0.49 / 0.63, R 0.565 / 0.726, against
  0.5 proud. Fix: a guided bumper (two D1.5 pins or a pivot pin), 0.8 proud, plus the M4 stop.
- **M6, assembly order:** keys go into the face-down shell before the module goes in from the face (flipping drops the
  caps); the cell is plugged into the F-side CN1 before the board goes down but taped only at the end, so face-down it
  rests on the LCM 1.05 above its tape and rattles; the 100 mm leads leave 55-75 mm of slack with no stow (the LRA and
  speaker leads also tie the back shell to the board); edge caps and washers fall out. Fix: inserts, module face-up,
  flip, controls, board as a book (P1 at about 90°), FPC1, then the cell taped into the back shell first, the back shell
  hinged at the right edge with the slack in a pocket; or re-crimp the PHR-2 on about 45 mm leads, or move CN1 to the B
  side (board change; also makes the battery serviceable). Adhesive-backed PORON.
- **M7, fold-bay skin:** FEA at 10 N: 17.6 MPa / 0.36 mm (D10 mid-skin), 22.1 / 0.42 (D8 at the free edge), 30.8 /
  0.56 (D6 at the edge): it cracks at about 15-22 N (the 62 MPa hand calc in DESIGN.md is 2-3x pessimistic) but touches
  the touch flex at 2.7-3.6 N at the thickest glass (7-9.5 N nominal). Fix: R0.5 at the skin's inner step, narrow the
  bay to x 97.6-131.6, or the lip front.
- **M8, no ESD path for the panel:** the LCM frame corner is 1.69 mm from the face edge through the open gap (an 8 kV air
  discharge reaches it; the module is ±2 kV HBM) and nothing grounds the frame. Fix, before fab: exposed F-side GND pads
  plus conductive tape to the LCM frame.
- **M9, speaker:** no hold-down (7.55 of free air above it), and its rear opens into the cavity, which leaks to the face
  through about 270 mm² (the screen gap alone about 140) against 111 mm² of grille: an acoustic short circuit. The grille
  sits under the right hand's fingers. Fix: a printed back chamber from the front plate, sealed on the speaker rim with
  foam (holds it down, about 4 cm³ sealed); a mesh; or front-firing.

## Minor

- m1 the board floats 0.04-0.36 in z without washers: adhesive 0.5 PORON, measure the boards.
- m2 the D-pad rattles 1.12 face-down; DPAD_HOLE_R is 0.5 where gb2 recommended 1.0.
- m3 key rest gaps stack two printed lengths (±0.1-0.15 each) against 0.15 and the TS 0.1 minimum actuation.
- m4 the USB-C rim opening leaves a 9.5 x 1.6 slot over the receptacle onto the board edge: a front-shell tongue to
  z 0.3.
- m5 the flush glass sits 0.1-0.7 (0.4 nominal) below the face with a 0.55-0.65 dust gap; the LCM floats ±0.35
  sideways: locating bumps on the frame walls.
- m6 sharp pocket corners reach 40-51 MPa in the front-alone twist: R0.5 reliefs.
- m7 screw heads bear on 6.8 mm² of PA12 (creep, gb2 #11): 0.05-0.08 N·m or a washer.
- m8 the microSD card sits 1.14 inside the face (needs a tool); the jack spot-face (D6.6) limits the plug body.
- m9 the cell is on low-tack removable tape (about 110 N at 300 g): stretch-release battery strips.
- m10 hump variant: its edge (x 183.2) sits 1.5 into the right grip, under the fingers.
- m11 not modelled: the display flex's 1.2 mm component section (fits), the backlight LED tail, the floating touch-IC
  area (tape it down), lead slack, washers.
- m12 missing: lanyard point, labels, dust membranes. Thermal is fine (T113-S3 about 0.5-1 W, BQ24073 ≤ 0.9 W).

## Verified OK

The board sits on the 5 front boss tips at z 1.6, 0.2 to the back bosses. Clearances reproduce: LCM tape 0.15, glass
gap 0.55 / 0.65, skin to touch flex 0.40, fences 0.40, cell to LCM 1.05, speaker 0.2, USB-C 0.28, jack 0.2, card 0.35,
the deflected snap to the board 0.23. No phantom presses from squeezing (gb2 #3): 20 N on the right wall moves the
VOL- / POWER lips 0.09 against a 0.35 trigger. Full 3.0 mm insert engagement on every build (M2 x 10; M2 x 8 on the
13.41 builds), tips 0.3 short of the hole bottom.

## Thickness per region (mm)

| region | needs | flat | hump | ADA258 | lip | LCD plate |
|---|---|---|---|---|---|---|
| cell bay | 15.35 | 15.35 | 15.35 | 13.41 | 16.15 | 16.00 |
| USB-C corner | 13.41 | 15.35 | 13.41 | 13.41 | 16.15 | 16.00 |
| top strip | 13.15 | 15.35 | 15.22* | 13.41 | 16.15 | 16.00 |
| centre under the panel | 12.50 | 15.35 | 13.41 | 13.41 | 16.15 | 16.00 |
| left grip | 12.25 | 15.35 | 13.41 | 13.41 | 16.15 | 16.00 |
| jack corner | 11.35 | 15.35 | 13.41 | 13.41 | 16.15 | 16.00 |
| right grip | 11.35 | 15.35 | 15.29* | 13.41 | 16.15 | 16.00 |
| speaker cut-out | 8.10 | 15.35 | 15.29* | 13.41 | 16.15 | 16.00 |
| fold bay | 10.35 | 15.35 | 13.41 | 13.41 | 16.15 | 16.00 |
| LRA strip | 9.58 | 15.00 | 14.97* | 13.06 | 16.15 | 15.65 |
| over the keys / D-pad | – | 16.75 | 16.75 | 14.81 | 17.55 | 17.40 |

\* the hump reaches into the region.

## Thinner

**With the 2000 mAh LP-803860** (the cell column sets 15.35): a measured cell at 8.0 (keeping 10 % swell) −0.55; a
measured, shimmed panel −0.30; a 0.5 SUS304 floor instead of the 1.0 FR4 plate (stiffer too) −0.50; rim 0 −0.10:
together **14.00**. A measured cell of 7.46 mm or less reaches **13.41**, where the board column takes over. Not safe:
cutting the 0.85 swell or the 0.2 air. Widening the notch (moving the LRA) would fit a ~6.5 x 47 x 64 cell (~2000 mAh)
at 13.41.

**With a 5 mm cell** (the board column sets 13.41; P1 and CN1 both set the face): a measured panel + the CN1 plate at
0.7 → 13.11; + rim 0 → 13.01; L3 → MWSA0518S (BOM only) + a 0.9 local USB-C chin → 12.50; all of it → **12.10**. A
1.2 mm board (impedance redo) would take another 0.40.

## Decisions for the owner

1. Panel backing: edge bond (0 mm, panel no longer removable), a supported SUS plate (+0.65), or the lip (+0.8).
2. Cell: buy and measure the cell and panel first; keep the 2000 mAh cell (about 14.0-15.35) or go to a 5 mm cell
   (13.41, about 12.1 with every trick; 35-40 % less runtime).
3. Board changes for this order: the hole scheme (H3 datum, H1 slot, the others D2.8); LCM GND pads; optionally CN1 on
   the B side.
4. Front: flush (skin cracks at 15-22 N, touches the flex at 3-4 N) or lip.
5. Back: flat or hump (the hump edge sits under the right fingers).
6. Key height: 1.4 proud (16.75 at the keys) or about 1.0.
7. Speaker: back-firing under the fingers, or a front grille.
