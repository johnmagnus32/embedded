# Display bus: U1 (T113-S3, B.Cu) -> P1 (display FPC, F.Cu), the 23 nets RGB_R2-7 / G2-7 / B2-7, DCLK, DE,
# HSYNC, VSYNC and PANEL_DISP (chained through its pull-up R39). 23 vias: one per net, which is the floor
# because U1 and P1 are on opposite sides. 0 crossings: the 86 a naive "right then up" bundle forces are all paid
# at those same vias.
#   blue + control: drop to F.Cu at U1 (blue in a diagonal staircase inside the pin ring, DCLK/DE by their S-row
#     pins), one 45 deg F.Cu band over the cap wall, rows over P1, straight drops into the pads from the north.
#   green + red: stay on B.Cu out of U1's E and S rows (R4/R5 down the C30/C18 gap, R6/R7 down the C12/C30 gap and
#     under the caps, re-laned so U1.77 keeps a via spot straight south of its pad), one 45 deg B.Cu band under P1,
#     each colour's bit order reversed in its own via staircase north of P1, short F.Cu stubs down.
#   DCLK >= 0.5 mm from its neighbours; HSYNC/VSYNC carry one trombone each, sized for the 25 mm RGB skew rule.
# U1.65 (LDOA), U1.66 and U1.77 (+3V3) each keep a legal 0.6/0.3 via escape: keep it that way when routing power.
#
# The geometry lives in groups/display/ (lib.py, geom_f.py, geom_b.py) as literal board-mm coordinates, NOT yet
# pad-relative: if U1, P1, R39 or the caps around U1 move, re-derive it (P("U1.70") etc.) before re-running.
# It is exec'd twice: pass 1 measures each net, pass 2 sizes the sync trombones from the longest RGB net.
import math, os

_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "display")      # route.py passes __file__
_SRC = "\n".join(open(os.path.join(_DIR, f)).read() for f in ("lib.py", "geom_f.py", "geom_b.py"))
SKEW, MARGIN = 25.0, 0.25          # gb3.kicad_dru "RGB bus matched", plus margin

def _run(extra):
    segs, vias, length = [], [], {}
    def poly(net, layer, pts, w=0.2):
        pts = [p for i, p in enumerate(pts) if i == 0 or abs(p[0] - pts[i - 1][0]) > 1e-9 or abs(p[1] - pts[i - 1][1]) > 1e-9]
        segs.append((net, layer, pts, w))
        length[net] = length.get(net, 0.0) + sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(pts, pts[1:]))
    def via(net, x, y): vias.append((net, x, y))
    exec(compile(_SRC, "groups/display", "exec"), {"poly": poly, "via": via, "F": "F", "BK": "B", "EXTRA": extra})
    return segs, vias, length

_, _, _nat = _run({})
_floor = max(v for k, v in _nat.items() if k.startswith("RGB_")) - SKEW + MARGIN
_segs, _vias, _ = _run({k: max(0.0, _floor - v) for k, v in _nat.items() if k.startswith("RGB_")})

for net, layer, pts, w in _segs:
    if len(pts) > 1: T(net, layer, w, pts, miter_=False)       # corners are already 45 deg; keep them exact
for net, x, y in _vias:
    VIA(net, x, y)
