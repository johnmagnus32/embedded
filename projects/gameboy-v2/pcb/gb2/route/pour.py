#!/usr/bin/env python3
"""pour.py — the last copper step on gb2: whole-board GND pours on F.Cu and B.Cu, and the GND vias stitching them
to the In1 plane.

  1. pours      one GND zone per outer layer over the whole board, priority 0 (under the switcher pours), 0.2 mm
                clearance, thermal spokes on every pad (small passives don't tombstone; the switcher pours above stay
                solid for their hot loops), islands removed
  2. return     a GND via within RET_R of every signal via: a layer change moves the signal's reference between
                In1 (GND) and In2 (+3V3), and this via is the short path its return current takes back to In1
  3. edge       a via every EDGE_PITCH along the board outline (and its cut-outs), EDGE_IN inside it
  4. field      a FIELD_PITCH grid wherever both pours have copper and no GND via is already near

A via goes only where it sits inside BOTH outer pours' fill with margin (so it clears every other net's copper on
F.Cu and B.Cu), keeps the hole-to-hole rule to every other drill, and stays out of rule areas. Rerunnable (strips
route:pour first). Order: route.py -> fanout.py -> pour.py.

Run:  <kicad python> route/pour.py [board]
"""
import pcbnew, os, sys, math, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
MM, TO = pcbnew.FromMM, pcbnew.ToMM
V = lambda x, y: pcbnew.VECTOR2I(int(round(MM(x))), int(round(MM(y))))
VIA_D, VIA_H = 0.45, 0.25
CLR, MIN_W = 0.2, 0.2
RET_R = 1.2                      # mm: a signal via with a GND via this close is covered
EDGE_PITCH, EDGE_IN = 2.5, 1.0
FIELD_PITCH, FIELD_CLEAR = 4.0, 2.0
HOLE_CLR = 0.2 + 0.05            # board min_hole_clearance + margin
PLANE_NETS = {"GND", "+3V3", "+1V2"}

board_path = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "..", "gb2.kicad_pcb")
subprocess.run([sys.executable, os.path.join(HERE, "route.py"), "--strip", board_path, "pour"], check=True)
B = pcbnew.LoadBoard(board_path)
G = pcbnew.PCB_GROUP(B); G.SetName("route:pour"); B.Add(G)
gnd = B.FindNet("GND")

# ---- 1. pours (islands kept for now: the vias below may connect them)
ps = pcbnew.SHAPE_POLY_SET(); B.GetBoardPolygonOutlines(ps, True)
board = ps
zones = {}
for L in (pcbnew.F_Cu, pcbnew.B_Cu):
    z = pcbnew.ZONE(B); z.SetLayer(L); z.SetNet(gnd); z.SetZoneName("GND pour " + B.GetLayerName(L))
    z.SetAssignedPriority(0); z.SetLocalClearance(MM(CLR)); z.SetMinThickness(MM(MIN_W))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    z.SetThermalReliefGap(MM(0.25)); z.SetThermalReliefSpokeWidth(MM(0.3))
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_NEVER)
    ol = z.Outline(); ol.NewOutline(); o = ps.COutline(0)
    for k in range(o.PointCount()): ol.Append(o.CPoint(k))
    B.Add(z); G.AddItem(z); zones[L] = z
# the USB-C shell / retention posts bond SOLIDLY to GND (ESD path; a thermal spoke at the board edge starves)
for fp in B.GetFootprints():
    if fp.GetReference() == "J7":
        for p in fp.Pads():
            if p.GetNetname() == "GND" and p.HasHole(): p.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
pcbnew.ZONE_FILLER(B).Fill(B.Zones())
fills = {L: z.GetFilledPolysList(L) for L, z in zones.items()}
# the switcher pours (also GND, higher priority) count as pour copper too
for z in B.Zones():
    if z.GetIsRuleArea() or z.GetNetname() != "GND" or z in zones.values(): continue
    for L in (pcbnew.F_Cu, pcbnew.B_Cu):
        if z.IsOnLayer(L): fills.setdefault(("extra", L), []).append(z.GetFilledPolysList(L))
rules = [z for z in B.Zones() if z.GetIsRuleArea()]

holes, gvias = [], []            # (x, y, r) of every drill; GND via positions
for fp in B.GetFootprints():
    for p in fp.Pads():
        if p.HasHole():
            q = p.GetPosition(); holes.append((TO(q.x), TO(q.y), TO(max(p.GetDrillSize().x, p.GetDrillSize().y)) / 2))
sig_vias = []
for t in B.GetTracks():
    if t.Type() != pcbnew.PCB_VIA_T: continue
    q = t.GetPosition(); x, y = TO(q.x), TO(q.y); holes.append((x, y, TO(t.GetDrill()) / 2))
    if t.GetNetname() == "GND": gvias.append((x, y))
    elif t.GetNetname() not in PLANE_NETS: sig_vias.append((x, y))

def on_copper(L, x, y, r):
    pts = [(x, y)] + [(x + r * math.cos(a), y + r * math.sin(a)) for a in [k * math.pi / 4 for k in range(8)]]
    polys = [fills[L]] + fills.get(("extra", L), [])
    return all(any(pp.Contains(V(px, py)) for pp in polys) for px, py in pts)

def fits(x, y):
    if any(math.hypot(hx - x, hy - y) < hr + VIA_H / 2 + HOLE_CLR for hx, hy, hr in holes): return False
    if any(z.Outline().Contains(V(x, y)) or z.Outline().SquaredDistance(V(x, y)) < MM(VIA_D / 2 + 0.1) ** 2 for z in rules): return False
    return on_copper(pcbnew.F_Cu, x, y, VIA_D / 2 + 0.03) and on_copper(pcbnew.B_Cu, x, y, VIA_D / 2 + 0.03)

placed = {"return": 0, "edge": 0, "field": 0}
uncovered = []
def add(x, y, kind):
    v = pcbnew.PCB_VIA(B); v.SetViaType(pcbnew.VIATYPE_THROUGH); v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    v.SetPosition(V(x, y)); v.SetWidth(MM(VIA_D)); v.SetDrill(MM(VIA_H)); v.SetNet(gnd); B.Add(v); G.AddItem(v)
    holes.append((x, y, VIA_H / 2)); gvias.append((x, y)); placed[kind] += 1

# ---- 2. return vias beside signal vias
for sx, sy in sig_vias:
    if any(math.hypot(gx - sx, gy - sy) <= RET_R for gx, gy in gvias): continue
    done = False
    for r in (0.7, 0.85, 1.0, 1.2, 1.5, 1.8):
        for k in range(24):
            a = 2 * math.pi * k / 24; x, y = sx + r * math.cos(a), sy + r * math.sin(a)
            if fits(x, y): add(x, y, "return"); done = True; break
        if done: break
    if not done: uncovered.append((sx, sy))

# ---- 3. edge stitching
def walk(chain):
    pts = [(TO(chain.CPoint(k).x), TO(chain.CPoint(k).y)) for k in range(chain.PointCount())]
    acc = 0.0
    for (ax, ay), (bx, by) in zip(pts, pts[1:] + pts[:1]):
        L = math.hypot(bx - ax, by - ay)
        if L < 1e-6: continue
        ux, uy = (bx - ax) / L, (by - ay) / L
        t = EDGE_PITCH - acc
        while t <= L:
            x, y = ax + ux * t, ay + uy * t
            for nx, ny in ((-uy, ux), (uy, -ux)):
                cx, cy = x + nx * EDGE_IN, y + ny * EDGE_IN
                if board.Contains(V(cx, cy)) and not any(math.hypot(gx - cx, gy - cy) < EDGE_PITCH * 0.6 for gx, gy in gvias):
                    if fits(cx, cy): add(cx, cy, "edge")
                    break
            t += EDGE_PITCH
        acc = (acc + L) % EDGE_PITCH
for k in range(ps.OutlineCount()):
    walk(ps.COutline(k))
    for h in range(ps.HoleCount(k)): walk(ps.CHole(k, h))

# ---- 4. field stitching
bb = B.GetBoardEdgesBoundingBox()
x0, y0, x1, y1 = TO(bb.GetLeft()), TO(bb.GetTop()), TO(bb.GetRight()), TO(bb.GetBottom())
y = y0 + FIELD_PITCH / 2
while y < y1:
    x = x0 + FIELD_PITCH / 2
    while x < x1:
        if board.Contains(V(x, y)) and not any(math.hypot(gx - x, gy - y) < FIELD_CLEAR for gx, gy in gvias) and fits(x, y):
            add(x, y, "field")
        x += FIELD_PITCH
    y += FIELD_PITCH

# ---- refill with islands removed (anything still not stitched is dead copper)
for z in zones.values(): z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
pcbnew.ZONE_FILLER(B).Fill(B.Zones())
pcbnew.SaveBoard(board_path, B)
print("pour: GND on F.Cu + B.Cu; stitching vias: %(return)d return, %(edge)d edge, %(field)d field" % placed)
print("  signal vias with no GND via within %.1f mm (no room): %d" % (RET_R, len(uncovered)))
for u in uncovered[:30]: print("    (%.2f, %.2f)" % u)
