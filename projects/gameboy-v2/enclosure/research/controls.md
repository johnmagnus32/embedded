# controls

## Summary
All 13 controls (SW1-SW13) are top-side parts; nothing is on the back. Nothing in the repo was changed. Scratch scripts and two check renders (left.png, right.png) are in /tmp/gb2-controls/.

**Face buttons (SW2-SW9, TS-1187A-B-A-B).** The part code decodes from the C318884 drawing as force B = 160 gf, height A = H 1.5 mm, keystake B = brass; LCSC confirms 160 gf, 1.5 mm and 100k cycles. Each switch has a Ø2.0 keystake at its footprint centre, pressed straight down (-z). The keystake top is 1.5 ±0.05 above the board top, so z = 3.10 (the 3D model says 3.15). It stands 0.3 mm above the stainless cover (z 2.80), which acts as the hard stop. Travel is 0.2 ±0.1 (the drawing; the spec block says 0.25).
- A, B, Start and Select sit exactly (0.000 mm) under the mapped GBA control centres.
- The four D-pad switches sit exactly 8.000 mm from the mapped D-pad centre (39.95, 64.09) along ±x/±y. That puts them on the arm centrelines, 1.4 mm inboard of the 9.4 mm arm tips.

**Side buttons (SW10-SW13, SKRTLBE010: Vol+, Vol-, L, R).** Each is rotated so its Ø2 plunger points straight out through the local board edge (0.00-0.01° to the edge normal). The plunger axis is 1.7 above the board top (z 3.3); travel is 0.2 mm at 1.6 N. The plunger tip:
- is 0.32 mm inside the board edge, so it does not overhang;
- is 0.82 mm short of the 2.0 mm wall's inner face;
- is 2.82 mm from the shell's outer surface (board.json mech.silhouette).

The L and R tips are within 0.12-0.64 mm of the centres of the GBA shoulder faces.

**Power switch (SW1, MSK12C02-HB).** The lever is 1.30 × 1.0 mm, protrudes 1.45 mm from the body, and sits at z 1.8-2.8. It points straight out of the bottom edge and slides along the edge, ±0.8 mm (1.6 mm total). Its tip is 0.47 mm inside the board edge, 0.97 mm short of the wall's inner face and 2.97 mm from the outer surface (0.30 / 0.80 / 2.80 if you go by the footprint silk). So it does not overhang the edge as the BOM intended.

**D-pad centre (pivot candidate).** On the top side it is plain GND copper pour under soldermask:
- no top-side tracks within 3.3 mm and no pads within 5.3 mm;
- the nearest via (tented GND) is 2.41 mm away.

That makes it a good seat for a pivot nub resting on the board at z 1.6. A pin or post through the board would need re-routing: three back-side signal tracks pass 0.9-1.8 mm from the centre.

**Biggest design driver.** Every face-button actuator is at z ≈ 3.1, but the 2090's cover glass is at z 18.3. A flat GBA-style front face therefore needs keycap plungers about 16-19 mm long, working against a 0.2 mm switch travel. The L/R plungers (z 3.3) are about 5-13 mm further back than the GBA's shoulder band. In the grips, outside the 2090 module's outline, no top-side part is taller than 3.35 mm, so a front face stepped down over the grips is possible. The step would have to fall in the ~3.1 mm gap between the Start/Select/B caps and the module edges.

## Risks
- Keycap reach is the main design driver. Every TS keystake is at z 3.10, but the 2090 glass top is at z 18.3. A flat GBA-style front face needs plungers of about 16-19 mm. The switch only travels 0.2 ±0.1 mm and hits its hard stop at about 0.3 mm. The tolerance chain (back shell, bosses, board, solder, switch 1.5 ±0.05, the printed plunger, front shell) must hold the rest gap to about 0.05-0.15 mm, or the buttons will feel dead or pre-pressed. Options: guide sleeves plus a compliant element, a front face stepped down over the grips, or a taller TS-1187A height code (up to 5.0 mm on the same footprint, stock not checked).
- A stepped front over the grips has very little room for the step. Start, Select and B are only 3.12, 3.17 and 3.35 mm (plan) from the 2090 module edges. The J3/J4 sockets (8.5 tall) and the module PCB (z 12.6 and up) sit right there.
- A tall D-pad rocking on a pivot at the board surface sways 0.48-0.73 mm at a z ≈ 21 top face. The shell recess needs that much clearance, or the pivot has to move up (for example, a centre column under the cross with four separate plungers).
- A through-board pivot pin at the D-pad centre is not possible as routed: BT_EN, USART2_TX and USART2_RTS run 0.89-1.79 mm away on B.Cu, and the inner planes are there. The pivot must rest on the top soldermask.
- L/R shoulders: the SKRT plungers are at z 3.3, about 5-13 mm behind where the GBA's shoulder band would be under a flat face. GBA-length 32 mm shoulder caps would come within 1.05 mm (plan) of the 2090 standoffs and 1.6-1.7 mm of the Ø5 shell-screw bosses. A wide cap on one centred plunger with 0.2 mm travel will tilt when pressed at its outer end, so it needs a hinge or guide (the GBA uses hinged levers).
- Side buttons and the slider need inward extensions. The SKRT tips are 0.82 mm short of the wall's inner face, and the SW1 lever is 0.97 mm short (0.80 by the footprint silk). The BOM's 'actuator slightly overhanging the board edge' intent for SW1 is not met: the lever stops 0.30-0.47 mm inside the edge.
- Shell split line: the side plungers (z 2.3-4.3) and the SW1 lever (z 1.8-2.8) sit just above the board top (z 1.6), which is where the placeholder back-tray wall ends. Either run the parting line through these openings or keep the openings clear of it.
- The SW1 ON position cannot be read from the drawing. The drawing's pin order is mirrored relative to the footprint (pad 1 is at -2.25 where the drawing's (1) is at +2.28). Pin 1 = GND = ON, so check continuity on a real part before fixing the slider's ON direction in the shell.
- The two MSK12C02-HB documents disagree: force 130 ±50 vs ±70 gf, travel ±0.1 vs ±0.2 mm, stop strength 30 vs 20 N, life 20k vs 10k (the BOM says 10k). Design the slider detent and stop for the looser figures.
- TS-1187A data is slightly inconsistent: travel 0.2 ±0.1 (drawing) vs 0.25 (spec block), and keystake top 1.5 (datasheet) vs 1.55 (3D model). That 0.05 mm is a quarter of the switch travel.
- The J5 (speaker) and J6 (LRA) wire pads are a default 3.0 mm envelope with real leads attached. J5 is 0.76 mm (plan) from the B cap circle and 1.48 mm from the A cap, so plunger guide sleeves must clear the solder joints and wires. SW11 (3.35 tall) is 1.08 mm from the A cap.
- board.json under-describes SW1: its height is a 1.5 default (the 3D model path points at another machine) and its courtyard is an axis-aligned box. Use the datasheet envelope (1.40 ±0.1 tall, lever z 1.8-2.8) for any clearance check.
- The SKRT footprint and model are for the SKRTLAE010 sibling; the BOM part is SKRTLBE010. The plunger geometry matches the LBE drawing within 0.05 mm, so the impact is negligible.
- mech_ref gives GBA front-panel positions as ±0.5 mm and the outline as ±0.3 mm. The 'exact' matches between switches and GBA centres are exact to the mapped reference, not to a real GBA.

## Facts
- **Frames used below** [measured] Board mm: +X right, +Y down (KiCad). Enclosure frame = (x, -y), board bottom z = 0, top z = 1.6. Every z below is the enclosure z (= 1.6 + height above the board top). The shell's outer surface is mech.silhouette; the wall's inner face is 2.0 mm inside it; the board edge is 2.5 mm inside it, so the inner wall face is 0.5 mm outside the board edge.  _(src: /Users/johmagnu/Desktop/embedded/projects/gameboy-v2/enclosure/README.md; board.json board.thickness and mech.shell_wall / board_inset (lines 1735-1736))_
- **GBA -> board mapping (draw_mech)** [measured] board = (20 + side(x), 30 + y). side(x) = x for x < 72.25 and x + S for x > 72.25. Stretch S = 6.0 mm: confirmed by running draw_mech.stretch() read-only, and the board.json silhouette has GBA (105.39, 0) at (131.39, 30.0).  _(src: /Users/johmagnu/Desktop/embedded/projects/gameboy-v2/pcb/gb2/draw_mech.py lines 36 (GBA_ORIGIN), 38 (XC), 135-137 (layout/side), 186 (stretch))_
- **TS-1187A-B-A-B part-code decode** [datasheet] Code is TS-1187A-[force]-[height]-[keystake].
- Force B = 160 gf (A 100 / B 160 / C 260 gf).
- Height code A: H = 1.5 mm (code table: A 1.5, B 1.6, C 1.7, D 2.0, E 2.5, F 3.0, G 3.5, H 4.0, I 4.5, J 5.0).
- Keystake B = brass (A = PA46).

LCSC C318884 lists 160 gf (1.6 N), height 1.5 mm, gold actuator, 5.1 x 5.1 mm, 100,000 cycles. The footprint name also ends in '_TL_H1.5'.  _(src: /Users/johmagnu/Desktop/embedded/cad/docs/C318884.pdf p1 (PART NO box and Code-X/H table, right side of the sheet); https://www.lcsc.com/product-detail/C318884.html (fetched 2026-10-13))_
- **TS-1187A actuator (SW2-SW9)** [datasheet] - Round Ø2.0 brass keystake, centred on the footprint origin. Faces +z; pressed toward -z.
- Keystake top H = 1.5 ±0.05 above the board top -> z 3.10.
- Stainless (Sus) cover top 1.2 above the board -> z 2.80. The keystake stands 0.3 above the cover; that is the hard stop.
- Travel 0.2 ±0.1 (drawing) / 0.25 mm (spec block).
- Force 160 gf = 1.57 N.
- Body 5.1 ±0.05 square; 6.5 ±0.25 across the terminals.
- Rating 50 mA / 12 V.  _(src: /Users/johmagnu/Desktop/embedded/cad/docs/C318884.pdf p1 (side view: Ø2.0, TRAVEL 0.2±0.1, 1.2, H±0.05; top view: 5.1, 6.5; SPECIFICATION block: Travel 0.25mm))_
- **TS-1187A 3D-model cross-check** [measured] The VRML gold keystake spans x/y ±1.0 around the origin, z 1.05-1.55. Its top (1.55) is +0.05 over the datasheet nominal, so z 3.15 instead of 3.10. The cover reaches 1.25 and the terminals ±3.25 x z 0-0.45. The board.json height 1.55 comes from this model.  _(src: /Users/johmagnu/Desktop/embedded/cad/symbols/easyeda2kicad.3dshapes/SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5.wrl (parsed, 0.1-inch units))_
- **D-pad switches SW2 UP / SW3 DOWN / SW4 LEFT / SW5 RIGHT** [measured] All top side, rot 0. Board positions: SW2 (39.95, 56.09), SW3 (39.95, 72.09), SW4 (31.95, 64.09), SW5 (47.95, 64.09); enclosure y = -board y. Contact point = keystake top at those x, y, z 3.10, press -z. Each is exactly 8.000 mm from the mapped D-pad centre (39.95, 64.09) along -y / +y / -x / +x. That is on the cross-arm centreline, 1.4 mm inboard of the 9.4 mm arm tips. The 5.1 mm bodies reach r 10.55, 1.15 mm past the arm tips.  _(src: /Users/johmagnu/Desktop/embedded/projects/gameboy-v2/pcb/gb2/gb2.kicad_pcb via pcbnew; board.json parts)_
- **Face switches SW6 A / SW7 B / SW8 START / SW9 SELECT** [measured] All top side, rot 0. SW6 (156.88, 61.92), SW7 (143.94, 66.25), SW8 (49.49, 83.82), SW9 (49.44, 91.82). Each is exactly (offset 0.000) at its mapped GBA centre. A-B spacing 13.65; Start-Select 8.00 centre to centre. Contact = keystake top, z 3.10, press -z.  _(src: gb2.kicad_pcb via pcbnew; /Users/johmagnu/Desktop/embedded/projects/gameboy-v2/pcb/gb2/mech_ref.json lines 1235, 1244, 1253, 1262)_
- **SKRTLBE010 actuator (SW10-SW13)** [datasheet] - Side push, round Ø2 plunger.
- Plunger axis 1.7 above the mounting plane (the board top) -> z 3.30; the plunger spans z 2.3-4.3.
- Plunger protrudes 0.84 beyond the 2.56-deep body (3.4 overall).
- Body 4.5 wide x 3.3 tall (top z 4.9).
- Travel 0.2 mm, force 1.6 N, life 100,000 cycles, 'sharp feeling' type, no positioning boss.  _(src: /Users/johmagnu/Desktop/embedded/cad/docs/C127481.pdf p1 (spec table), p2 (outline drawing: 4.5, 3.3, Ø2, 1.7, 0.84, 2.56, 3.4))_
- **SKRT footprint / 3D-model cross-check** [measured] Footprint KEY-SMD_SKRTLAE010-1: the plunger faces local +y, the side with mounting pads 4/5 (at y +0.98); terminals 1-3 are at y -0.98. VRML plunger: x ±1.0, local y +0.78..+1.78 (model Y is flipped), z 0.75-2.75, axis 1.75. Frame front face at local y 0.93, giving 0.85 protrusion. Silk plunger outline reaches y 1.80; courtyard y -1.59..+1.81. This matches the LBE drawing within 0.05 mm.  _(src: /Users/johmagnu/Desktop/embedded/cad/symbols/easyeda2kicad.3dshapes/KEY-SMD_SKRTLAE010-1.wrl; /Users/johmagnu/Desktop/embedded/cad/symbols/easyeda2kicad.pretty/KEY-SMD_SKRTLAE010-1.kicad_mod)_
- **SW10 Vol+** [measured] Top side, pos (163.4031, 54.5308), rot 96.87.
- Plunger tip (contact point) board (165.170, 54.318) = enclosure (165.170, -54.318), z 3.30.
- Faces board (+0.9928, -0.1196): out of the right edge, 6.87° toward the top. Enclosure (+0.9928, +0.1196).
- Press direction board (-0.9928, +0.1196).  _(src: gb2.kicad_pcb via pcbnew plus the model tip offset; /tmp/gb2-controls/final_table.py)_
- **SW11 Vol-** [measured] Top side, pos (164.3676, 62.5308), rot 96.87. Tip board (166.135, 62.318) = enclosure (166.135, -62.318), z 3.30. Same facing and press direction as SW10. SW10 and SW11 tips are 8.06 mm apart along the edge.  _(src: gb2.kicad_pcb via pcbnew; /tmp/gb2-controls/final_table.py)_
- **SW12 L shoulder** [measured] Top side, pos (37.6832, 38.1757), rot -173.09.
- Tip board (37.469, 36.409) = enclosure (37.469, -36.409), z 3.30.
- Faces board (-0.1203, -0.9927): out of the top edge. Enclosure (-0.1203, +0.9927).
- Press direction board (+0.1203, +0.9927).  _(src: gb2.kicad_pcb via pcbnew; /tmp/gb2-controls/final_table.py)_
- **SW13 R shoulder** [measured] Top side, pos (152.777, 38.1709), rot 173.09.
- Tip board (152.991, 36.404) = enclosure (152.991, -36.404), z 3.30.
- Faces board (+0.1203, -0.9927); enclosure (+0.1203, +0.9927).
- Press direction board (-0.1203, +0.9927).

It mirrors SW12 about x = 95.25 to within 0.04 mm.  _(src: gb2.kicad_pcb via pcbnew; /tmp/gb2-controls/final_table.py)_
- **SKRT tip vs board edge and shell (all four)** [measured] The plunger axis is perpendicular to the local board edge and to the silhouette (0.00-0.01°). For each switch:
- tip 0.32 mm inside the board edge (no overhang);
- body front face (metal frame) 1.17 mm inside the board edge;
- wall inner face 0.82 mm beyond the tip;
- shell outer surface 2.82 mm beyond the tip.

Edge segments: right (164.62, 47.08)->(167.06, 67.29) at 83.13°; top-left (34.58, 36.44)->(53.39, 34.16); top-right (137.08, 34.15)->(155.90, 36.43). All are straight.  _(src: board.json board.outline and mech.silhouette (ray casts in /tmp/gb2-controls/analyze_side.py))_
- **MSK12C02-HB lever and body (SW1)** [datasheet] Lever:
- 1.30 wide (along the slide) x 1.00 thick (z), protruding 1.45 from the body front face.
- Spans 0.20-1.20 above the board top -> z 1.80-2.80, scaled off the side view.
- Two positions, lever centre at ±0.80 from the body centre.
- Full travel 1.6 ±0.1 (2015 spec) / ±0.2 (2024 spec).
- Force 130 ±50 gf (2015) / ±70 gf (2024).

Strength and life: stop strength 30 N (2015) / 20 N (2024); stem pull at least 20 N; life 20k at 12 V / 50 mA (2015) / 10k no-load (2024).

Body: 8 ±0.2 incl. lugs x 2.8 ±0.1 deep x 1.40 ±0.1 tall (top z 3.0); pegs Ø0.75 x 0.55.  _(src: /Users/johmagnu/Desktop/embedded/cad/docs/C431541.pdf p1 (drawing), p4 (130±70 gf), p5 (1.6±0.2, 20 N), p7 (10000); /Users/johmagnu/Desktop/embedded/cad/docs/C431541-spec.pdf p4 (130±50 gf, 1.6±0.1), p5 (30 N, stem pull 20 N min), p8 (20000))_
- **SW1 placement and lever position** [measured] Top side, pos (43.4356, 97.5653), rot -13.36.
- Lever faces local +y -> board (-0.2311, +0.9729): out of the bottom edge, perpendicular to it (0.00°). Enclosure (-0.2311, -0.9729).
- Slide axis board (+0.9729, +0.2311): along the edge.
- Body front face at local y 1.95 (1.40 ahead of the pegs at y 0.55).
- Lever tip at local (±0.80, 3.40): board (43.428, 101.058) on the pad-3 side, (41.872, 100.688) on the pad-1 side.
- Tip is 0.47 inside the board edge, 0.97 short of the wall inner face, 2.97 from the outer surface. Footprint silk (lever to y 3.57) gives 0.30 / 0.80 / 2.80.
- Body front face is 1.92 inside the board edge.
- The edge is a straight segment (41.03, 100.97)->(46.34, 102.23).  _(src: gb2.kicad_pcb via pcbnew; /Users/johmagnu/Desktop/embedded/cad/symbols/easyeda2kicad.pretty/SW-TH_MSK12C02-HB-1.kicad_mod; board.json; /tmp/gb2-controls/analyze_sw1.py)_
- **SW1 vs the GBA power switch** [measured] At mid-travel the lever axis meets the outer surface at (41.96, 103.76). The GBA reference (22.8, 74) maps to board (42.8, 104.0); the GBA knob x ≈ 22.5 maps to 42.5. So SW1 is 0.5-0.9 mm left of the GBA knob. The GBA slider window (x 16.2-30.8) maps to board 36.2-50.8.  _(src: /Users/johmagnu/Desktop/embedded/projects/gameboy-v2/pcb/gb2/mech_ref.json line 1432)_
- **GBA D-pad, mapped to board mm** [measured] - Centre (39.95, 64.09).
- Cross span 18.8 (18.7 vertically per FIG.1), arm width 6.0.
- Board polygon: x 30.55-49.35, y 54.69-73.49; arms x 36.95-42.95 and y 61.09-67.09.
- Shell recess ≈ Ø21.3 (board x 29.66-50.94).
- GBA D-pad and A/B caps stand 1.9-2.1 mm proud of the front face.
- mech_ref gives front-panel positions as ±0.5 mm.  _(src: mech_ref.json lines 1171 (centre), 1179 (cross), 1944 (depth note); draw_mech mapping)_
- **GBA A / B / Start / Select, mapped** [measured] A: Ø9.4 at (156.88, 61.92). B: Ø9.4 at (143.94, 66.25). Start: Ø4.0 at (49.49, 83.82). Select: Ø4.0 at (49.44, 91.82). Start/Select shell recess ≈ 5 mm.  _(src: mech_ref.json lines 1235, 1244, 1253, 1262)_
- **GBA L / R shoulders, mapped** [measured] - L envelope 32.1 x 11.8: board x 21.1-53.2 (centre 37.15), y 31.7-43.5. Front-visible face x 24.7-50.0, y 32.65-43.1.
- R envelope x 137.3-169.4, y 31.7-43.5; face x 139.9-164.8.
- Depth band 3.1-11.6 mm behind the front face.
- SW12 tip x 37.47 is +0.32 from the envelope centre (+0.12 from the face centre). SW13 tip x 152.99 is -0.36 from the envelope centre (+0.64 from the face centre). Both tips are at y ≈ 36.4, inside the band.
- The GBA's own L/R switches sit about 15 mm in from the top surface, under hinged levers: GBA (15.9, 19.4) / (130.2, 17.5) -> board (35.9, 49.4) / (156.2, 47.5).  _(src: mech_ref.json lines 1370, 1380 and the gba notes)_
- **Plan clearances: GBA controls to the 2090 module outline** [measured] D-pad 5.26, Start 3.12, Select 3.17, B 3.35, A 16.29 (rule: CONTROL_CLEARANCE 3.0). L/R GBA envelopes only 1.41. A GBA-length shoulder's inner end would be 1.05 mm from the 2090 standoffs (r 2.9 hex) and 1.69 (L) / 1.61 (R) from Ø5 shell-screw bosses at (57.39, 35.47) / (133.19, 35.45).  _(src: draw_mech.clearances() run read-only; draw_mech.py lines 42, 112; board.json board.holes)_
- **Cap outlines vs shell and neighbours (plan)** [measured] Clearance to the shell:
- D-pad arm tips: at least 6.81 to the wall inner face, 6.31 to the board edge.
- A cap: 5.26 to the inner wall, 3.48 to the board edge.
- B cap: 0.60 to the speaker cut-out edge (cap is above the board).
- Select: 9.36 to the inner wall.

Neighbours (courtyard to cap outline): SW11 (3.35 tall) is 1.08 from A; J5 speaker solder pads are 1.48 from A and 0.76 from B (J5's 3.0 height is a default guess, plus leads). The real SW1 envelope is 2.53 from the Select cap edge.

Free radius from each switch centre to the nearest other top-side courtyard: SW2 6.86 (U11), SW3 6.27 (J6), SW4 8.60, SW5 7.95 (J3, 8.5 tall), SW6 5.78 (SW11), SW7 5.46 (J5), SW8 4.49 (C26), SW9 4.53 (SW1).  _(src: board.json parts courtyards; /tmp/gb2-controls/analyze_top.py, free_radius.py)_
- **Under the D-pad centre (pivot candidate)** [measured] Point (39.95, 64.09).

Top side:
- F.Cu GND pour (filled) under soldermask.
- No F.Cu track within 3.30 mm (nearest: BTN_DOWN at x 43.25).
- No pad within 5.33 mm (SW4/SW5); nearest courtyards 5.45 mm (the four switches).
- Nearest via: GND at (40.454, 66.450), 2.41 mm, tented.
- SW3 reference silk is 3.7 mm away (y 67.8-69.2).

Bottom side:
- B.Cu tracks BT_EN 0.89 mm, USART2_TX 1.34 mm, USART2_RTS 1.79 mm (y 62.3-63.2).
- No back-side courtyard within 3 mm (U12 BT830 is 4.6 mm).

Inner layers: In1 GND plane, In2 +3V3 plane.

A nub resting on the top surface (z 1.6) is clean. A hole or post through the board would cut BT_EN.  _(src: gb2.kicad_pcb via pcbnew (/tmp/gb2-controls/dpad_centre.py, silk_near.py))_
- **Depth references for keycap reach** [measured] - 2090 module PCB bottom z 12.6; cover-glass top z 18.3 (11.0 / 16.7 above the board top).
- Tallest top-side parts in the grips outside the module footprint: SKRT bodies 3.35 (z 4.95); J5/J6 solder pads 3.0 (default, plus wire leads); everything else 1.55 or less.
- J3/J4 sockets (8.5 tall) at x 55.9-58.4 and 132.1-134.6.
- The placeholder back tray's outer bottom is z -3.6.  _(src: board.json mech.module lines 1759-1760; board.json parts; /Users/johmagnu/Desktop/embedded/projects/gameboy-v2/enclosure/enclosure.py fit_frame())_
- **Keycap reach if the front face is flat** [estimated] With the face just above the glass (z ≈ 19-20) and caps about 2 mm proud like the GBA, the TS keystakes (z 3.10) are 16-19 mm below the press surfaces. The SKRT axis (z 3.3) is about 15-16 mm behind such a face; the GBA's L/R band would be at z ≈ 8-16.4.

A taller TS-1187A height code on the same footprint (up to J = 5.0 mm, keystake top z 6.6) would shorten the plungers by up to 3.5 mm. Stock of those codes was not checked.  _(src: Derived from the datasheet and board.json z values above; C318884.pdf p1 Code-X table)_
- **D-pad rocking geometry** [estimated] With nubs at r 8.0: tilt 1.43° to actuate (0.2 mm) and 2.15° to the cover hard stop (0.3 mm). With the pivot on the board top (z 1.6) and a cross top at z ≈ 21, the top face sways 0.48-0.73 mm sideways. The centre pivot nub must be about 1.5 mm longer than the four switch nubs, minus the chosen rest gap or preload.  _(src: Geometry from the switch positions and the C318884 travel)_
- **Power LED D1 (front-face light pipe, for reference)** [measured] 19-213SYGC at (144.14, 44.70), top side, 0.56 tall (z ≈ 2.2). This is exactly the mapped GBA Ø2.5 power-LED window. A flat face needs a light pipe of about 17 mm, the same reach problem as the keycaps.  _(src: board.json parts D1; gb2-bom.csv line 16; mech_ref.json line 1271)_
- **board.json SW1 envelope quality** [measured] SW1's height 1.5 is export_board.py's DEFAULT_H['SW'] fallback. The footprint's model path (/local/home/johmagnu/learning/embedded/cad/symbols/easyeda2kicad.3dshapes/SW-TH_MSK12C02-HB-1.wrl) does not exist on this machine, and no MSK12 model is present locally. Its courtyard is the footprint's axis-aligned box (38.88-47.74 x 94.58-101.51). The real body is 1.40 ±0.1 tall, rotated -13.36°, with the lever at z 1.80-2.80.  _(src: /Users/johmagnu/Desktop/embedded/projects/gameboy-v2/enclosure/export_board.py (DEFAULT_H, wrl_height); SW-TH_MSK12C02-HB-1.kicad_mod model line)_
