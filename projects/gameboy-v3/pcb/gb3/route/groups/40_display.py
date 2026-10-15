# Display bus + everything else that lands on P1: U1 (T113-S3, B.Cu) -> P1 (display FPC, F.Cu, pin 1 at the LEFT).
#
#   RGB666 + DE/HSYNC/VSYNC + PANEL_DISP (22 nets): enter P1's pads from the SOUTH, under the connector body.
#     B rows (east, 0.6 mm, U1 pin order) x F columns (north, P1 pad order), one via per net at its row x column:
#     a block staircase (red bottom-left, C1 top-right) that pays all 167 single-layer crossings at the vias the
#     B -> F change needs anyway. North of the via field every column turns NE (one parallel 45-degree jog) and drops
#     straight into its pad. B lanes stay south of y 77.6 (the U1.33-45 east escapes own B y 69.8-75.6).
#   DCLK: U1.75 -> via inside the pin ring -> R50 (33R, F, under U1.75) -> F only, round the outside (SE) of the
#     staircase, >= 0.6 mm c-c to B7 in the jog: RGB_DCLK has 0 vias and In1 GND under it end to end.
#   DE: crosses nothing, so it runs one straight F diagonal outside DCLK (via straight south of U1.76).
#   PANEL_DISP: its row continues past its via to the 10k pull-up R39 (moved to (90.6, 77.45) B, rot 0).
#   Backlight feed (P1.1 BL_LED_N -> U6.6 FB / R38.1, P1.2 BL_LED_P <- C45.1 via TP11): B north of P1, F stubs into
#     the pad tips. P1's plane pads keep their via spots north of the tips (groups 49 preview = what fanout must do).
#
# Spec: route/PLAN.md "Choke points / 2. The display corridor" (step 3; first trial ~/.cache/gb3-step3/display/). Needs
# R39 at (90.60, 77.45) rot 0 B (PLAN P1). Integration trial (~/.cache/gb3-step3/final/trial/s2-s5): 912.4 mm, 25 vias,
# skew 23.66 mm (B2 32.17 .. DE 55.83, KiCad incl. vias), DCLK 50.23 mm F-only, DRC: no new violations under the repo and
# the proposed DRU (incl. "TP11 isolation": BL_LED_N's row sits at y 61.0, 0.625 mm from TP11's bare pad).
# Geometry: groups/display/geom.py (pad-relative, solves the column positions and jog heights from P()). Re-run after
# any move of U1 / P1 / R50 / R39 / C45 / TP11 / U6; check RGB skew in the DRC (gb3.kicad_dru "RGB bus matched").
import importlib.util, os

_spec = importlib.util.spec_from_file_location("display_geom", os.path.join(os.path.dirname(os.path.abspath(__file__)), "display", "geom.py"))
G = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(G)

r39 = P("R39.1")
near = abs(r39[1] - P("U1.52")[1]) < 2.0          # R39 sits beside the DISP via (else: leave its stub to step 4)
bus = G.build(P, r39=r39 if near else None)
bl = G.build_backlight(P)
for net, layer, pts, w in bus["segs"] + bl["segs"]:
    if len(pts) > 1: T(net, layer, w, pts, miter_=False)     # every corner is already 45 deg
for net, x, y, d, h in bus["vias"] + bl["vias"]:
    VIA(net, x, y, d=d, h=h)
