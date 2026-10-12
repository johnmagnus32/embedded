#!/usr/bin/env python3
"""planecheck.py — return-path check of gb2's reference planes (run after zones are filled).

Every signal track is sampled every 0.1 mm and checked against the plane it references: F.Cu tracks against In1
(GND), B.Cu tracks against In2 (+3V3 / the +1V2 island). Reported per track: the longest stretch with no plane
copper under it (a via antipad crossing ~0.5-0.7 mm is normal; a slot is longer), and any B.Cu track that runs
from the +3V3 plane onto the +1V2 island (crosses its edge).

Run:  <kicad python> route/planecheck.py [board] [--min 1.0]
"""
import pcbnew, os, sys, math
HERE = os.path.dirname(os.path.abspath(__file__)); TO = pcbnew.ToMM
args = [a for a in sys.argv[1:] if not a.startswith("--")]
MIN = float(sys.argv[sys.argv.index("--min") + 1]) if "--min" in sys.argv else 1.0
B = pcbnew.LoadBoard(os.path.abspath(args[0]) if args else os.path.join(HERE, "..", "gb2.kicad_pcb"))
pcbnew.ZONE_FILLER(B).Fill(B.Zones())
PLANE_NETS = {"GND", "+3V3", "+1V2"}
def fill(layer, nets):
    out = []
    for z in B.Zones():
        if z.GetIsRuleArea() or not z.IsOnLayer(layer) or z.GetNetname() not in nets: continue
        out.append((z.GetNetname(), z.GetFilledPolysList(layer)))
    return out
REF = {pcbnew.F_Cu: fill(pcbnew.In1_Cu, {"GND"}), pcbnew.B_Cu: fill(pcbnew.In2_Cu, {"+3V3", "+1V2"})}
def under(layer, x, y):
    v = pcbnew.VECTOR2I(int(pcbnew.FromMM(x)), int(pcbnew.FromMM(y)))
    for n, ps in REF[layer]:
        if ps.Contains(v): return n
    return None
gaps, island = [], []
for t in B.GetTracks():
    if t.Type() == pcbnew.PCB_VIA_T or t.GetLayer() not in REF or t.GetNetname() in PLANE_NETS: continue
    a, b = t.GetStart(), t.GetEnd(); ax, ay, bx, by = TO(a.x), TO(a.y), TO(b.x), TO(b.y)
    L = math.hypot(bx - ax, by - ay); n = max(1, int(L / 0.1))
    run = best = 0.0; seen = set(); where = None
    for k in range(n + 1):
        x, y = ax + (bx - ax) * k / n, ay + (by - ay) * k / n
        p = under(t.GetLayer(), x, y)
        if p: seen.add(p); run = 0.0
        else:
            run += L / n
            if run > best: best, where = run, (x, y)
    if best >= MIN: gaps.append((best, t.GetNetname(), B.GetLayerName(t.GetLayer()), where))
    if len(seen) > 1: island.append((t.GetNetname(), (round(ax, 2), round(ay, 2)), (round(bx, 2), round(by, 2))))
# a net can also change reference between two of its segments: check per net on B.Cu
per = {}
for t in B.GetTracks():
    if t.Type() == pcbnew.PCB_VIA_T or t.GetLayer() != pcbnew.B_Cu or t.GetNetname() in PLANE_NETS: continue
    m = (TO(t.GetStart().x + t.GetEnd().x) / 2, TO(t.GetStart().y + t.GetEnd().y) / 2)
    p = under(pcbnew.B_Cu, *m)
    if p: per.setdefault(t.GetNetname(), set()).add(p)
mixed = sorted(n for n, s in per.items() if len(s) > 1)
gaps.sort(reverse=True)
print(f"plane gaps >= {MIN} mm under signal tracks: {len(gaps)}")
for g, n, l, w in gaps[:40]: print(f"  {g:5.2f} mm  {l:5s} {n:24s} near ({w[0]:.2f},{w[1]:.2f})")
print(f"B.Cu tracks crossing the +1V2 island edge: {len(island)}")
for r in island: print("  ", *r)
print(f"B.Cu nets referencing both +3V3 and +1V2: {mixed or 'none'}")
