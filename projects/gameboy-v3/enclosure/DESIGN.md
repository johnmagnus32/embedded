# gb3 enclosure: design

First model, 2026-10-15. **Architecture A**: one flat slab, flush cover glass, the 2000 mAh PKCELL LP-803860 on a
1.0 mm FR4 battery plate. **15.35 mm** thick (front face z +8.55, back face z -6.80), 194.7 x 89.8 mm, R7.1 corners.
It is generated from `build/board.json` (the KiCad board) and from the research budget
(`~/.cache/gb3-encl/synth/stack.json`, `synth/scripts/budget.py`, `research_result.json` budget.first_model_plan).
The style follows the gb2 enclosure (frame helpers, shell split, bosses, keys head-first from inside), with the
parametric z of its thin-stack variant: every z is derived from the 67 stack inputs (each with its source in
`params.INPUTS`) and the `GB3_*` flags. No z is typed twice.

**Before the JLC order:** the FPC1 fix is in the board (step 7, dd7c895); wrap-test the real touch tail (section 8), and
see REVIEW.md for the board decisions the enclosure review raised (hole scheme, LCM ground pads).

## 1. Frame

Board mm, +X right, **+Y up** (KiCad's Y flipped, `P(x, y) = (x, -y)`), +Z toward the screen. The board's bottom face
is z 0 and its top face z 1.6. That face is the split between the shells and the datum: it rests on the front boss
tips. Positions below are KiCad numbers.

Outline: an analytic rounded rectangle x 20..214.7, y 30..119.8, R7.1 (gb3.kicad_pcb Eco1.User). The board is
190.7 x 85.8 with three cut-outs (the cable notch, the cell + LRA notch, the speaker cut-out) and sits 2.0 inside the
outline. Walls 1.5 (MJF) / 1.67 (FDM), front plate 1.2, printed floor 1.2, R2 front / R3 back outer fillets.

## 2. The stack (baseline; `params.py` is the source, `python -c "import params"` prints it)

The cell column has no PCB in it, and it sets the thickness:
rim 0.1 + panel 4.55 max + air 0.2 + swell 0.85 + cell 8.5 + tape 0.15 + FR4 1.0 = **15.35**.

| z (baseline) | what | derived from (source) |
|---|---|---|
| +8.55 | face (front plate outer, flush) | thickest glass + rim 0.1 (design) |
| +8.45 / +8.15 | cover glass top, max / nominal | LCM back + module 4.25 +/-0.3 (ATM0500D27-CT.pdf p4 item 8, p17) |
| +6.90 / +7.50 / +8.20 | tops of the model's panel layers: LCM 3.0, touch 0.6 (3M 9495 0.175 + ITO 0.425), cover glass 0.7 | ATM0500D27-CT.pdf p6, p17 (the parts sum to 4.30; the datasheet total is 4.25 +/-0.3) |
| +7.35 | front plate inner face | face - plate 1.2 (design, MJF/FDM) |
| +7.55 | plate inner face over CN1 (plate 1.0) | CN1 top 7.20 (S2B-PH-SM4-TB 5.5 tall, C295747.pdf p3) + 0.35 |
| >= +7.90 | plate inner face over the fold bay (skin <= 0.65) | glass max - 0.55 (panel report fold rule) |
| +3.90 | LCM back | board top 1.6 + P1 2.1 max (C9160.pdf p1: 2.0 +/-0.1 closed) + air 0.2 |
| +3.75..+3.90 | ledge tape | 0.15 die-cut acrylic (panel report 0.15-0.2) |
| +2.75..+3.75 | ledge (top, left, bottom-left only) | 1.0 (thin report option b); reaches 1.0 under the LCM (synth ledge.py) |
| +3.70 | swollen cell top, 0.20 under the LCM | cell top + swell 0.85 = 10 % of 8.5 (gb2 REVB_DESIGN.md s3.1; BU-301a 8-10 %) |
| +1.60 | board top = split = datum | JLC04161H-7628 1.6 +/-10 % |
| 0.00 | board bottom | |
| -5.65..+2.85 | cell LP-803860 | 62 x 38.5 x 8.5 max (LiIon2000mAh37V.pdf p5) |
| -5.80..-5.65 | cell tape | 0.15 removable double-sided (design) |
| -6.80..-5.80 | FR4 battery plate, flush with the back face | 1.0 (JLCPCB +/-10 %; 0.4 mm at 20 N, thin report) |
| -5.60 | printed floor inner face | back - floor 1.2 (6 x 0.2 layers) |
| -4.86 | the board-limited back (the hump and thin-cell builds) | USB-C 3.36 (C165948.pdf p1: 3.26 +/-0.10) + CLR_B 0.3 + floor 1.2 |
| **-6.80** | **back face** | the cell column (FR4 plate bottom) |

Clearances in the stack: B parts to the floor >= 0.3 (CLR_B: board tolerance 0.16 + air), F parts to the plate
>= 0.3 (CLR_F; CN1 0.35), LCM over P1 and every F part 0.2, swollen cell to the LCM 0.2. `params.py` asserts all of
them against board.json's z_design on import.

Per region (baseline; "needs" is what that region alone would need, all are 15.35 in the flat slab):

| region (KiCad x / y) | needs | governed by |
|---|---|---|
| cell bay 139.2-181.7 / 52.3-117.8 | 15.35 | LP-803860 8.50 + swell 0.85 + air 0.20, FR4 floor |
| USB-C corner 108-127.5 / 30-47 | 13.41 | B USBC1 z -3.36 |
| top strip 127.5-181.7 / 30-52.3 | 13.15 | B L3 z -3.10 |
| centre under the panel | 12.50 | B D3 z -2.45; F P1 top 3.70 |
| left grip 20-57 / 30-100 | 12.25 | B L1 z -2.20 |
| jack corner, right grip | 11.35 | B 0805 MLCCs z -1.30 (the budget assumed 1.45: 11.50); F Audio1 6.75, CN1 7.20 |
| fold bay, LRA strip, speaker cut-out | 10.35 / 9.58 / 8.10 | never govern |

## 3. Architecture

**Front shell** (prints face-down; `front.py`):
- Module pocket 121.4 x 76.5 through the face (the LCM max outline + 0.2 per side). The visible gap round the glass
  is 0.65 at the sides and 0.55 at top and bottom. A 0.7 relief in the right frame wall clears the panel's unlabeled
  right-edge tab (KiCad y 40.4-46.4, datasheet rear view only).
- 1.0 frame walls round the pocket (1.7 on the right). They carry the ledge on the top, left and bottom-left (z 2.75)
  and stop at z 4.00 over the cell, above its swell. None in the fold bay or the cell-lead corner.
- Ledge z 2.75..3.75 on the top, left and bottom-left (to x 81.0) segments, 1.0 under the LCM, relieved from below
  over C20 / C94 (0.75 left there). None over the cell or the flex fold.
- Fold-bay skin: the plate's inner face is cut up to z 7.90 over x 84.5-133, y 112.6-117.5, leaving 0.65; no ribs or
  bosses in the bay.
- Face holes D8.4 for A / B / X / Y, D5.4 for START / SELECT, the D-pad cross + 0.7 with its arm-end corners R0.5.
- LED light pipes over U5 (125.4, 34.6) and U14 (160.5, 34.6): a D2.2 bore with a D1.4 stop ring 0.3 over the LED,
  for a bought D2.0 acrylic rod flush with the face.
- The plate thinned to 1.0 over CN1 (inner face 7.55).
- D5.5 bosses with M2 heat-set insert holes (D3.2 x 4.0) at H1 (26.5, 58.0), H2 (180.5, 35.0), H3 (204.0, 37.5),
  H4 (208.0, 83.5), H5 (26.25, 110.75) and the seam screws S1 (136.0, 116.3), S2 (209.6, 115.6). Tips at z 1.6 (the
  datum). Each on-board boss is merged into a wall by a gusset that stops 0.3 over the board; H3 goes into the right
  wall (`params.BOSS_MERGE_DIR`) so the top wall stays free for the R shoulder's yoke.
- Ten D3 plate posts stop 0.2 over the board beside the keys (D-pad diagonals, START sides, the ABXY centre, X-SELECT,
  SELECT sides), so a firm press beside a key cannot push its flange (gb2 STRUCTURAL_REVIEW #1).
- Jack: a D5.4 U-slot on the barrel axis z 4.1, open at the split, plus a D6.6 spot-face 0.1 behind the barrel tip
  (the plug's D6.0 shoulder seats on the tip, 0.375 inside the outline). Both dip under the split, so they are cut from
  the back rim too.
- Notches open at the split: VOL+ / VOL- / POWER 4.0 wide in the right wall, L / R 12.0 wide in the top wall,
  z 1.6..5.2. The back rim closes them from below.

**Back shell** (prints back-down; `back.py`):
- Cell bay: a 1.05-deep pocket in the back face holds the bought FR4 plate (40.1 x 63.6 R0.8, flush, 0.2 clearance);
  a window through the floor over the cell lets the cell's tape sit on the plate. The plate laps 0.4 under the fence
  feet and is bonded there.
- Fences 1.2 thick, 0.4 off the cell, up to z 0, in the 2 mm notch gaps, open along the cell's top end from x 152.15
  east and in the riser corner for the cell leads (core keep-out "cell leads"). The bottom fence is joined to the wall
  (a 0.4 slot would trap MJF powder).
- LRA ring ID 8.4 at (133.4, 77.2), speaker cup ID 27.4 at (197.3, 102.8) with a lead gap toward J3 and a bridge to
  S2; the grille is 55 holes D1.6 inside D22 (29 % open). The speaker fires back-down onto a 0.3 foam ring.
- USB-C: a 9.50 x 3.96 stadium round the receptacle shell, open up to the split (the back shell closes past the
  receptacle), plus a 13.31 x 7.31 R0.5 x 0.3 overmold relief (the receptacle face ends 0.36 behind it; Type-C allows
  0.40); the relief's top is cut from the front rim.
- microSD: an 11.9 x 1.7 slot with a 0.6 finger scallop. Reset: a D1.5 pinhole under SW1 (82.0, 35.7) with a guide
  tube to 0.5 under its keystake.
- Back bosses D5.5 at all seven positions, D2.4 holes, D4.2 x 2.1 counterbores (DIN 912 M2 heads 0.1 under the back
  face). H1-H5 stop 0.2 under the board; S1 / S2 rise to the split and meet the front boss tips. FDM builds add a D6.0
  collar round each counterbore. D4 posts 0.2 under the board at the D-pad pivot and the ABXY centre.
- One releasable snap hook on the bottom wall at x 81.8, between H5 and S1, into a pocket in the front wall. With S1
  and S2 the 184 mm bottom edge has three fasteners.

**Controls** (`controls.py`; all printed, each one solid at rest):
- A / B / X / Y (D8.0) and START / SELECT (D5.0): 6.70 long (z 3.25..9.95), 1.4 proud, flange D10.0 / D7.0 x 0.8 under
  the plate (inserted head-first from inside, gb2 style), a D2.4 tip 0.15 over the TS-1187A keystake (z 3.10), 0.45 to
  its hard stop. Mapping: Y SW6, A SW7, X SW8, B SW9, START SW10, SELECT SW11.
- D-pad: one cross (arm 9.4, width 6.6) over SW2 left / SW3 right / SW4 up / SW5 down, four D2.6 domed legs 0.10 over
  the switches, a D3.0 near-hemispherical pivot on the board's F.Cu GND pour at (36.5, 74.9), held by a printed
  8.80 x 10.60 x 0.6 washer. Its flange rides 1.12 under the plate (z 5.43..6.23) so it can rock a switch to its hard
  stop on a diagonal.
- VOL+ / VOL- / POWER: 3.6 x 3.2 buttons in the 4.0 x 3.6 notches, 0.5 proud, a lip on the wall's inner face as the
  outward stop, a pad 0.15 off the SKRT plunger (axis z 3.3).
- L / R: 11.6-wide faces, 0.5 proud, on a 2.0-deep yoke along the top wall's inner face (L x 27.6-56.4, R x
  183.55-207.3) and a 3.0 stem across the 4.72 to SW15 / SW16, whose plungers sit 6.2 inside the outline.

**Context bodies** (`context.py`, never printed): the board with H1-H5; 193 part hull prisms over z_design (F blue,
B tan); USB-C, jack and microSD (+ the latched card) from board.json edge_features, not their KiCad 3D models (1.5-2.2
off); the panel as LCM + touch + glass with the active area tinted; the display loop and the touch U-turn as 0.3 strips
along the panel report's fold model; the cell (and its swell as a keep-out); LRA; speaker + foam; FPC1 as built (green; a board
without the step-7 fix shows it as placed, red, and corrected, green).

Printed mass, baseline: about 45 cm3 (front 16.7, back 25.2, controls 3.0), about 45 g in PA12.

**Bought parts** (per console; the STEP has a body for each modelled one):

| part | qty | spec | where |
|---|---|---|---|
| FR4 battery plate | 1 | 40.1 x 63.6 R0.8 x 1.0, JLCPCB FR4, no copper, black mask both sides, outline +/-0.2 (`out/battery_plate.dxf`) | the cell bay, FR4 builds |
| heat-set insert | 7 | M2 x 3 brass, for the D3.2 x 4.0 holes | front bosses, flush with the tips |
| screw | 7 | DIN 912 M2 x 10 (M2 x 8 on the 13.41 builds and with the hump) | H1-H5, S1, S2, from the back |
| board washer | 5 | PORON / EPDM D5.5 x D2.4 x 0.3-0.5 | on the H1-H5 back bosses (open question 7) |
| light pipe | 2 | D2.0 clear acrylic rod, 5.49 long | U5, U14 |
| ledge tape / lip gasket | 1 | 0.15 die-cut acrylic tape (flush) / 0.1 die-cut gasket (lip), shaped as their STEP bodies | panel |
| cell tape | 1 | 0.15 removable double-sided | under the cell |
| transfer tape | - | 0.05 3M 467MP | FR4 plate bond, D-pad washer |
| speaker foam ring | 1 | 0.3 thick, D23.4 x D22.2 | under the speaker's lip |
| SUS304 plate | 1 | 0.5, 42.5 x 65.5 (the cell buffer outline) | `GB3_LCD_PLATE=1` only |

## 4. Variants and their thickness

| build | flags | face | back | total | checks |
|---|---|---|---|---|---|
| **A, baseline** | defaults | +8.55 | -6.80 | **15.35** | 9 / 9 PASS |
| lip front | `GB3_FRONT=lip` | +9.35 | -6.80 | **16.15** | 9 / 9 PASS |
| battery hump | `GB3_BACK=hump` | +8.55 | -4.86 body, -6.80 over x 137.7-183.2, y 50.8-119.8 | **15.35** at the hump, **13.41** body and grips | 9 / 9 PASS |
| Jauch cell | `GB3_CELL=LP503759JU` (5.4 / 5.8 x 38 x 62, 1300 mAh) | +8.55 | -4.86 | **13.41** | 9 / 9 PASS |
| Adafruit 258 | `GB3_CELL=ADA258` (5.0 +/-0.3 x 35 x 62, 1140 mAh) | +8.55 | -4.86 | **13.41** | 9 / 9 PASS |
| LCD plate | `GB3_LCD_PLATE=1` (0.5 SUS304 plate, SS z 3.10..3.60) | +8.55 | -7.45 | **16.00** | 9 / 9 PASS |
| hump + thin cell | `GB3_BACK=hump GB3_CELL=ADA258` | +8.55 | -4.86 (no hump needed) | **13.41** | 9 / 9 PASS |
| FDM walls | `GB3_PROCESS=fdm` (walls 1.67) | +8.55 | -6.80 | **15.35** | 8 / 9: (e) the S1 wedge, section 7 item 8 |

With a thin cell the USB-C sets a flat back at -4.86, the cell rests on the printed floor (no FR4 plate) and has about
1.6 of air above it when swollen. The lip variant references the module from its glass (top at 8.45 under the 0.1
gasket and the 0.8 lip), so the LCM back sits at 4.15 nominal (3.90..4.50 over the module tolerance).

## 5. Assembly order

1. Heat-set the 7 M2 x 3 inserts into the front bosses, flush with the tips. Tape the LED light pipes and the ledge
   tape in.
2. Front shell face-down on a spacer frame at least 1.5 tall round the keys (the heads stand 1.4 proud). Keys, D-pad
   and edge caps go in head-first from inside; hold the edge caps and shoulders outward until the board is in.
3. The module, flush front: stand it on its glass bottom edge in the pocket with the flexes hanging into the cavity
   (their roots stand at y 110.1 / 111.9, inside the pocket edge at 113.15), then swing it flat about that edge onto
   the ledge tape. A straight drop does not pass the flexes (0.35 / 0.55 between the LCM / glass edge and the pocket).
   `front.swing_in()` sweeps this every 2 deg and it is clear. Lip front: the module goes in glass-first from inside
   onto the lip gasket.
4. Tape the D-pad washer round the pivot spot on the board (<= 0.05 transfer tape). Plug the display tail into P1
   with the board held above.
5. Plug the cell into CN1 (the PHR-2 leads run over the board edge and down the notch corner outside the panel).
   With `GB3_LCD_PLATE=1` the SUS plate goes in from inside first, with the cell taped under it (the plate's
   sharp-cornered buffer outline cannot pass the board's R2 notch corners from below); checks (h) assumes this order.
6. Lower the board onto the front boss tips at z 1.6.
7. Wrap the touch tail round the notch edge into FPC1, in its corrected position (section 8).
8. Bond the FR4 plate into the back shell's pocket (0.05 3M 467MP), tape the cell onto it, fit the PORON washers on
   the H1-H5 back bosses, and close the back shell from below; the snap clicks in.
9. Seven DIN 912 M2 screws from the back: M2 x 10 (M2 x 8 on the 13.41 builds and with the hump).

`checks.py` (h) sweeps steps 2, 3 (as a straight move), 6 and 8 along z; front.py's self-test sweeps the swing-in.

## 6. Checks

`checks.py` on the finished parts (`python checks.py`, about 22 s; `--fast` 13 s):
(a) interference of every part with every part and context body (the cell and its swell included), context vs
context (no allowed overlaps since the board's step 7), and the stack clearances;
(b) keep-outs, core and per module; (c) rest gaps (keys 0.150, D-pad legs 0.100, edge caps and shoulders 0.150) and
the board datum at all 7 bosses, the screw paths; (d) one valid solid per part, print poses, bought parts;
(e) thickness ray scans (plate, CN1, skin, ledge, walls, floor, FR4 plate, fences) and a slab scan of every printed
part against MJF 0.5 / FDM 0.86 (xy) and 0.5 / 0.6 (z); (f) the region table against params and the research budget;
(h) the assembly sweep; (j) press travel, the D-pad rocked toward 8 directions with its lateral play; (x) cross-module
cuts. The exports were also verified: every STL and 3MF is watertight and consistently wound, within 0.1 % of its
solid's volume and on the bed, and the STEP reads back with every part and context body.

## 7. Open questions (settle with the owner, the real parts or a test print)

**Decisions for the owner**
1. **Cell:** keep the LP-803860 (15.35, about 2 h) or a thin cell in the same notch (13.41, 35-40 % less capacity;
   Adafruit 258 is 1C, below powerd's 1.8 A cap). No board change either way. Adafruit 2011 showed out of stock on
   2026-10-15.
2. **Front:** flush (thinnest; exposed glass edge, a 0.55-0.65 gap round it, a 0.65 skin over the fold) or the 0.8 lip
   (+0.8, protects the glass, full 1.2 plate over the fold, easier assembly).
3. **Back:** flat, or the hump (13.41 in the hands, 15.35 at the cell; harder to print on FDM).
4. **Cell vs screen:** the base lets the swollen cell press toward the LCD (0.2 left); the LCD plate option (+0.65,
   16.00) turns that toward the back. In the model the SUS plate is not held by anything (it floats at z 3.10..3.60):
   it needs joggled tabs onto GND pads on the board's top strip (gb2 rev B decision 23; a board change, see the GND
   pads in section 8) and/or bonding under the front frame walls over the cell, lowered to the plate top.
5. **Walls:** JLC asks for 2.0 walls on 100-200 mm MJF parts and holds +/-0.4 % (+/-0.78 at 195 mm). The model uses
   1.5 inside the drawn outline (the board is 2.0 inside it). The alternative is growing the outline 0.5 per side.
   With the FDM walls (1.67) the board edge is only 0.33 from the wall (the thin report recommends 0.5): test-print the
   board fit and set the slicer's XY compensation from it.

**Structure and fit**
6. **Fold-bay skin:** the 0.65 skin the flush front needs is a 4.35 cantilever, 48.5 long, with a free edge at the
   glass: about 0.67 mm and 62 MPa at 10 N on 10 mm (hand calculation; PA12 48 MPa), and it reaches the touch flex
   after 0.15. Narrowing the core "fold bay" top band to the touch funnel (x 97.6-131.6) would shorten it to 34 mm;
   the lip variant keeps the full plate.
7. **Board clamp:** the screws pass through the board's holes, so with the back bosses 0.2 short nothing presses the
   board onto the front datum (it can move 0.04-0.36 in z with the board's tolerance). Fit 0.3-0.5 PORON / EPDM washers
   (D5.5 x D2.4) on the H1-H5 back bosses (section 3, bought parts), or add crush ribs (gb2 STRUCTURAL_REVIEW #4).
8. **S1 / S2 in the R3 back fillet:** they sit 3.5 / 4.2 from the bottom outline, so their D4.2 counterbores break out
   through the fillet on the outline side, in every build: a knife edge up to 0.46 above the back face at S1 (0.14 at
   S2), and each head's outer edge stands 0.21-0.25 proud of the local fillet surface (still 0.1 above the back plane).
   Above the break-out the wedge at S1 is 0.80 thick where the slab scan sees it: over MJF's 0.5, under FDM's 0.86 (2
   perimeters), which is the one FDM FAIL (the hump build has the same edge; the scan's 0.4 grid misses it there).
   Options: a low-head M2 at S1 / S2, a 0.3 deeper counterbore with M2 x 8, a smaller local back fillet, moving S1
   (hard: its boss is 0.4 from the panel pocket and 0.25 from the fold bay), or accept it.
9. **Shoulders:** a 12-wide bumper over one SKRT switch (0.2 travel) with 0.5 proud rocks about the far yoke end when
   pressed off-centre. L needs 0.49 at both face ends (it triggers across the face). R's yoke is stopped by H2's block on
   one side, so a press at x 198.8 needs 0.57 (it triggers from x 187.2 to about 197.0). With the SKRT's worst-case 0.3
   travel both only trigger near the plunger. Options: 0.7-0.8 proud, a narrower notch, moving H2's gusset, a
   flexure-guided bumper, or a second switch per shoulder (a board change).
10. **D-pad:** its flange rides 1.12 under the plate, so the cross can rattle up to 1.12 face-down; a 1.2 PORON ring
    between the flange and the plate would preload it. Its hole budget uses 0.65 of the 0.7 clearance per axis (0.695
    with the lip) without the switches' placement error: if the first print binds, raise `hole_clr` to about 0.8.
11. **Rest gaps** hang on two printed lengths (front boss plate-to-tip 5.75, cap flange-seat-to-tip 4.10, MJF
    +/-0.1-0.15 each) against the 0.15 gap and the TS-1187A's 0.2 +/-0.1 travel. Check on the first print. Edge caps
    and shoulders have no over-travel stop.
12. **Thin cells:** the 1.2 printed floor deflects about 1.3-2.3 mm at 20 N over the bay (thin report), against 1.6 of
    air over the swollen cell. Use a 2.0 printed floor or keep the FR4 plate before trusting it.
13. **Hump:** S1 (x 136.0) is 1.7 west of the hump, so its counterbore notches the hump's side wall, and the USB-C
    relief's foot (z -5.29) is under the body's back face (-4.86): a plugged overmold stands proud of the back plane.
14. **Snap:** hand estimate (MJF PA12: about 10 N/mm, 2.4 % strain, 6-7 N to pull off). The catch is designed at zero
    play, so MJF tolerance decides whether the seam is preloaded or rattles. Use `back.SNAP = False` for PLA.
15. **microSD:** the latched card edge is 1.14 inside the outer face (0.54 inside the scallop); push-push needs a
    fingernail or a card corner.
16. **Not built yet:** a speaker hold-down beyond its adhesive foam ring, ribs on the printed floor, posts under
    START / SELECT (13-14 mm from a screw). The front plate posts stop 0.2 over the board: a board bowed more than
    that toward the face between bosses touches them (harmless; IPC's 0.75 % bow is 0.3 over 40 mm).

**Estimates to replace with measurements**
17. The panel and cell maxima drive the stack: a measured panel can save about 0.3 (shim the ledge), a measured cell
    0.5-1.5 (Adafruit lists the LP-803860 at 7). The panel's right-edge tab relief comes from the datasheet's rear view
    only.
18. The flex paths (fold model, +/-1.2), the microSD card's z, the PHR-2 plug and lead route behind the "cell leads"
    keep-out (lead exit +/-8), the speaker lip and foam stack, the TS-1187A pad extent that locates the D-pad washer in
    x (`controls.TS_PAD_IN`). The 0805 heights are 1.30 from the 3D models (the budget used 1.45).
19. Keep metal out of the top-left grip near the BT830 antenna: H1 is 23.6 mm from it (the datasheet asks for 20), so
    its brass insert passes; a nylon screw at H1 is optional.

## 8. Before the board order: FPC1 and the other board items

**FPC1 (the touch FFC connector, B side) was a functional blocker; the board's step 7 (dd7c895) fixed it.** As first
placed, its slot faced the board's top edge (away from the incoming tail), and the 53.4 mm touch tail could not reach it
after wrapping round the notch edge outside the display loop. As built now: rotated 180 deg at (118.5, 72.8), lid edge
y 76.28 (the furthest south its west tab clears U16.1 by 0.18; U16 did not move), pads remapped, R29 moved west of the
tab. The fold model puts the tail end at y 72.18 and the touch U-turn 0.163 outside the display loop at the nominal
tail: wrap-test the real tail before ordering. The model shows the as-built "FPC1" (green); a board without the fix
still shows both placements. The research's fix, for the record (panel report, fold model, synth/scripts/fpc1_new.py):
- rotate FPC1 180 deg so the slot faces +y (the bottom edge) and move it about 5 mm toward the bottom edge: origin
  about (118.4, 73.6), outer lid edge at y about 77.0 (feasible 75.6-77.5; the model's corrected lid edge is 77.08);
- mirror the pad nets: 6 +3V3, 5 GND, 4 I2C1_SCL, 3 I2C1_SDA, 2 TOUCH_INT, 1 TOUCH_RST;
- move R29 and shift U16 1.4 mm in -x (C43 / C67 stay clear). B:R29 overlapping the corrected FPC1 by 0.004 mm3 is the
  one context overlap the checks allow; FPC1 corrected is 0.197 from U16. Check pad-to-pad clearance, not courtyards
  (the footprint's courtyard does not enclose its tab pads), and that pin 1 (VDD) lands at x about 115.9 on a real
  tail;
- re-route the 6 nets and the 2 GND tabs, re-run DRC and the fab outputs.
A 10-minute paper mock-up of the tail (a 1.6 mm card on 2.3 mm spacers) confirms the position if a panel is at hand.

Also recommended before the order (no thickness effect): widen the B-side NO_PARTS for the touch flex to x 104.2-125.9,
y 91-111 plus the tail strip x 114.4-122.4 from FPC1 to y 95; fix the 3D-model metadata (LRA1, SPK1 orientation;
USBC1 / Audio1 registration; Q1's missing model). To consider: 2-3 exposed GND pads on the F pour (top strip under the
LCM's top edge, the notch's top edge) for conductive tape to the LCM frame and to an LCD plate.
