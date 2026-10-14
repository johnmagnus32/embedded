# gb2 enclosure — structural review (rev 2.1, 24.5 mm)

Done 2026-10-14 on the committed rev 2.1 geometry (`enclosure.build()`), before any fixes. Three analyses — FEA of the
front shell, FEA of the back shell, a design review without FEA — then an independent verification that re-meshed and
re-solved the key cases with its own code. Numbers are for **PETG** (E 2.0 GPa); PLA (E 3.0 GPa) deflects ×0.67, stresses
are the same. Layer strength used: PETG 40 MPa in-plane / 20 MPa across layers, PLA 45 / 25.

**Method.** gmsh tet mesh of the STEP export (no defeaturing), scikit-fem quadratic tets (P2, no plate locking),
CHOLMOD direct solve. Front: rim at z 1.6 held in z, boss tips fixed; back: rim held by the front shell, boss tops fixed,
floor on the cell. Validated against Roark plate cases (FEA/Roark 1.00–1.04), mesh convergence (h 1.8 → 1.2 → 0.9: under
2 %), and hand beam checks. Two independent meshes agreed within 1 %.

## Findings, most severe first (each fix was tested on a scratch variant unless marked)

| # | Severity | Problem | Evidence | Tested fix |
|---|---|---|---|---|
| 1 | High | **Phantom presses:** a firm press beside the screen triggers START or B (SELECT borderline) | The lens pocket leaves two plate strips (x 50.7–56.3, x 134.2–139) spanning ~71 mm wall to wall, I = 27.8 mm⁴. 20 N clear of every key head moves the flange seats START 0.61 / B 0.57 / SELECT 0.40 mm (0.47 / 0.46 / 0.31 with the taped lens) vs the ~0.4 trigger → START/B fire at ~13–17 N | Wall-to-wall ribs under the plate beside the screen frame (left x 52.8–54.2, right x 136.3–137.4, plate down to z 9.0, merged into the Start/Select and B tubes, clear of every keep-out; +1.6 cm³ ≈ 2 g). START 0.11, B 0.17, SELECT 0.08, A 0.08 mm; trigger force 46 N (B) to >100 N |
| 2 | High | **Speaker-cradle fingers snap** at ~1.5 N sideways (handling, fold-over assembly) | Three 40° arcs, 1.0 × 17.6 mm: 5 N at the tip → 3.7–4.0 mm, 80–84 MPa peak at the root, nearly all across layers | One 300° C-ring, same section + seat lip + lead-in, gap at 70–130° (a gap centred at 90° hits B's tube): 16× lower stress mid-ring; the free ends take ~3 N |
| 3 | Med-high | **Squeezing the right edge presses Vol−** (Vol+ at the limit) | The flat seam lets the notched front wall slide in: 20 N at the cap → 0.47 / 0.41 mm vs the 0.35 trigger (~15–20 N squeeze) | A 5th screw boss at (165.5, −73.5) (off the board, clear of the speaker/cradle): 0.20 / 0.29 mm, and B under the right palm 0.56 → 0.34. A lap joint along the right wall (broken ±3.5 mm at the notches) takes it to 0.03 / 0.07 |
| 4 | Medium | **Board clamp has zero tolerance** (hand calc) | Front boss tips, rim and board top all at z 1.6, back boss tips at z 0; JLC 1.6 mm FR4 is ±10 %: a thick board opens a 0.05–0.16 seam gap, a thin one rattles | (not FEA) shorten the back boss tips 0.2–0.3 and clamp through 0.5 mm EPDM/PORON washers or 3 crush ribs; measure the delivered boards first |
| 5 | Med-low | **Pressing the screen pushes the lens onto the LCD glass** | 20 N on Ø10 at the lens centre moves the lens bottom 0.98 mm (taped; 0.86 bonded) — 1.5 mm acrylic bends ~0.4 mm per 10 N over the 71 × 51 opening. Contact at ~10 N with the 0.5 worst-case gap, ~24 N with the 1.13 nominal gap; ribs barely help | (not a geometry fix) measure GLASS_TOP and use the real gap; a 1.1 mm glass cover is 8.6× stiffer and frees 0.4 of gap |
| 6 | Med-low | **D-pad hole corners** are the face's most stressed spot | R0.3 outer corners: 30–33 MPa principal (≈36 von Mises converged) at 20 N → safety factor 1.2–1.35 | DPAD_HOLE_R 0.3 → 1.0: 16–19 MPa; checks (j) clearances change ≤ 0.003 |
| 7 | Med-low | **Back floor oil-cans** over the battery (feel) | 20 N at the bay centre / corner / U12: 0.91 / 0.93 / 0.51 mm (compression-only rim); reaches the BT830 (0.3 gap) at ~12–15 N | Floor 2.4 mm inside the fence (cell 0.8 higher, still 2.65 under the 2090's parts): centre 0.66 → 0.35 |
| 8 | Med-low | **Locator fingers crack in a hard side drop** (hand calc) | 3.0 × 1.1 fingers crack across layers at ~19–20 N; the board + 2090 (~60 g) overcomes the clamp friction above ~450 g | Widen to 6 mm; the 5th screw also helps |
| 9 | Low-med | **Battery fence inner corner** | 37 MPa (29 across layers) for 20 N on the bay corner | Round the fence corners R2–3 + 0.8 root chamfer (the 2.4 floor alone: 27 / 20) |
| 10 | Low-med | **J1 cover snap tabs** strain 1.7 % across layers per cycle | 0.8 thick, 3.8 mm flex, 0.2 barb — PLA cracks, PETG marginal | Move the catch down to ~z 12.5 (7.6 mm flex, 0.4 %), or print the cover on edge |
| 11 | Low-med | **Screw heads crush the PETG** and the clamp relaxes | DIN 912 M2 heads bear on 6.4 mm²: 39–78 MPa at 0.1–0.2 N·m | Torque 0.05–0.08 N·m, re-torque after a day; set inserts 0.1–0.2 below the boss tip |
| 12 | Low | A hard squeeze on the back of the right grip could lift the board via post B | 0.07–0.10 mm per 50 N with the 2090 tied in (~4× short of a trigger) | Hardware check only |

**Not issues (verified):** the grips outside the bezels flex 0.01–0.2 mm per 20 N; the seam does not open under face
presses (rim lift ≤ 0.023); plate flex cannot press the D-pad (it rocks on its pivot — needs ~1.3 mm vs ≤ 0.99); boss,
tube, hatch and speaker-ring stresses ≤ 15 MPa; the Vol− notch corner is at SF 1.4 (the 5th screw → 2.6+).

**Material.** PLA's extra stiffness alone does not fix #1 or #3. PETG (or ASA) for the final shells for toughness and
heat; PLA is fine for fit test prints.

**Rev B note.** #1 is specific to this stack (the face 17 mm above the board); in rev B the face sits ~5 mm above the
board and can be backed by it. #2–#11 carry over as design rules.
