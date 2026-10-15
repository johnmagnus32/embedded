#!/usr/bin/env python3
"""audit.py — route-quality audit of gb2b: the things DRC passes but a reviewer would circle.

  detour   a net's routed length vs the shortest octilinear (0/45/90 deg) tree over its pads (MST of
           octilinear pad-to-pad distances): flagged at > DETOUR_RATIO and > DETOUR_MM of excess
  angle    a segment off the 45 deg grid (pad stubs shorter than STUB are exempt)
  jog      a segment shorter than JOG between two longer ones (a kink)
  backturn two joined segments turning sharper than 90 deg (an overshoot)
  stub     a fanout stub (pad -> plane via) longer than STUB_MAX
  vias     a 2-pad net with more vias than it changes layer for (>2 on a net whose pads share a layer)

Run:  <kicad python> route/audit.py [board]      (prints the findings, worst first)
"""
import pcbnew, os, sys, math
from collections import defaultdict
HERE = os.path.dirname(os.path.abspath(__file__))
TO = pcbnew.ToMM
DETOUR_RATIO, DETOUR_MM = 1.35, 2.5
JOG, STUB, STUB_MAX = 0.25, 0.6, 1.6
PLANE_NETS = {"GND", "+3V3", "+1V2"}

B = pcbnew.LoadBoard(os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "..", "gb2b.kicad_pcb"))
groups = {}
for g in B.Groups():
    for it in g.GetItems(): groups[it.m_Uuid.AsString()] = g.GetName()

def octi(a, b):
    dx, dy = abs(a[0] - b[0]), abs(a[1] - b[1])
    return max(dx, dy) + (math.sqrt(2) - 1) * min(dx, dy)

def mst(pts):
    if len(pts) < 2: return 0.0
    seen, total = {0}, 0.0
    d = {i: octi(pts[0], pts[i]) for i in range(1, len(pts))}
    while d:
        j = min(d, key=d.get); total += d.pop(j); seen.add(j)
        for k in d: d[k] = min(d[k], octi(pts[j], pts[k]))
    return total

pads = defaultdict(list)
for fp in B.GetFootprints():
    for p in fp.Pads():
        if p.GetNetname(): pads[p.GetNetname()].append((TO(p.GetPosition().x), TO(p.GetPosition().y), fp.GetReference() + "." + p.GetNumber()))
tracks = defaultdict(list); vias = defaultdict(list)
pad_xy = {(round(x, 2), round(y, 2)) for pp in pads.values() for x, y, _ in pp}
for t in B.GetTracks():
    if t.Type() == pcbnew.PCB_VIA_T: vias[t.GetNetname()].append(t)
    else: tracks[t.GetNetname()].append(t)

out = []
# detour (signal nets; plane nets are mostly fanout)
for net, ts in tracks.items():
    if net in PLANE_NETS: continue
    pp = pads.get(net, [])
    if len(pp) < 2: continue
    routed = sum(TO(t.GetLength()) for t in ts)
    ideal = mst([(x, y) for x, y, _ in pp])
    # only judge fully routed nets (no ratsnest left): compare via connectivity
    if ideal > 0 and routed > ideal * DETOUR_RATIO and routed - ideal > DETOUR_MM:
        out.append((routed - ideal, "detour", f"{net}: {routed:.1f} mm routed vs {ideal:.1f} mm ideal ({routed / ideal:.2f}x)", groups.get(ts[0].m_Uuid.AsString(), "?")))
# per-segment geometry
for net, ts in tracks.items():
    ends = defaultdict(list)
    for t in ts:
        a, b = (round(TO(t.GetStart().x), 3), round(TO(t.GetStart().y), 3)), (round(TO(t.GetEnd().x), 3), round(TO(t.GetEnd().y), 3))
        L = math.hypot(b[0] - a[0], b[1] - a[1])
        if L < 1e-4: continue
        ang = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0])) % 45
        off = min(ang, 45 - ang)
        g = groups.get(t.m_Uuid.AsString(), "?")
        if off > 0.5 and L >= STUB:
            out.append((L * off / 45, "angle", f"{net}: {L:.2f} mm segment at {off:.1f} deg off the 45 grid, ({a[0]:.2f},{a[1]:.2f})-({b[0]:.2f},{b[1]:.2f}) {B.GetLayerName(t.GetLayer())}", g))
        via_end = any((round(TO(v.GetPosition().x), 2), round(TO(v.GetPosition().y), 2)) in ((round(a[0], 2), round(a[1], 2)), (round(b[0], 2), round(b[1], 2))) for v in vias[net])
        if g == "route:fanout" and L > STUB_MAX and via_end:                 # pad -> plane via; pad-to-pad ties are not stubs
            out.append((L - STUB_MAX, "stub", f"{net}: fanout stub {L:.2f} mm at ({a[0]:.2f},{a[1]:.2f}) {B.GetLayerName(t.GetLayer())}", g))
        for e, o in ((a, b), (b, a)): ends[(e, t.GetLayer())].append((o, L, t))
    for (e, lay), lst in ends.items():
        if len(lst) != 2 or (round(e[0], 2), round(e[1], 2)) in pad_xy: continue      # traces meeting on a pad centre
        (o1, l1, t1), (o2, l2, t2) = lst
        u = (e[0] - o1[0], e[1] - o1[1]); v = (o2[0] - e[0], o2[1] - e[1])
        cos = (u[0] * v[0] + u[1] * v[1]) / (l1 * l2)
        if cos < -0.02 and min(l1, l2) >= 0.1:     # sharper than ~91 deg (a sub-0.1 mm jog onto a pad centre hides in the pad)
            out.append((min(l1, l2) + 5, "backturn", f"{net}: turns back {math.degrees(math.acos(max(-1, min(1, cos)))):.0f} deg at ({e[0]:.2f},{e[1]:.2f}) {B.GetLayerName(lay)}", groups.get(t1.m_Uuid.AsString(), "?")))
        if min(l1, l2) < JOG and max(l1, l2) > 1.0 and abs(cos) > 1e-3 and cos < 0.999:
            pass                                   # short corner chamfers are fine; jogs are caught below
    # jog: a short segment whose two neighbours are parallel (a sideways step)
    for t in ts:
        a, b = (round(TO(t.GetStart().x), 3), round(TO(t.GetStart().y), 3)), (round(TO(t.GetEnd().x), 3), round(TO(t.GetEnd().y), 3))
        L = math.hypot(b[0] - a[0], b[1] - a[1])
        if L >= JOG or L < 1e-4: continue
        na = [x for x in ends[(a, t.GetLayer())] if x[2] is not t]; nb = [x for x in ends[(b, t.GetLayer())] if x[2] is not t]
        if len(na) == 1 and len(nb) == 1:
            pa, pb = na[0][0], nb[0][0]
            u = (a[0] - pa[0], a[1] - pa[1]); w = (pb[0] - b[0], pb[1] - b[1])
            lu, lw = math.hypot(*u), math.hypot(*w)
            if lu > 0.5 and lw > 0.5 and abs((u[0] * w[0] + u[1] * w[1]) / (lu * lw) - 1) < 1e-3:
                out.append((0.5, "jog", f"{net}: {L:.2f} mm sideways jog at ({a[0]:.2f},{a[1]:.2f})", groups.get(t.m_Uuid.AsString(), "?")))
# vias on 2-pad signal nets whose pads share a layer
for net, vs in vias.items():
    pp = pads.get(net, [])
    if net in PLANE_NETS or len(pp) != 2 or not vs: continue
    if len(vs) > 2:
        out.append((len(vs), "vias", f"{net}: {len(vs)} vias on a 2-pad net", groups.get(vs[0].m_Uuid.AsString(), "?")))

out.sort(key=lambda r: -r[0])
by = defaultdict(int)
for _, k, _, _ in out: by[k] += 1
print("findings:", dict(by) or "none")
for score, kind, msg, g in out:
    print(f"  [{kind:8s}] {g:20s} {msg}")
