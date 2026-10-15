#!/usr/bin/env python3
"""pour.py — the last copper step on gb3: whole-board GND pours on F.Cu and B.Cu, and the GND vias stitching them to
the In1 plane and the In2 GND islands (ported from gb2's route/pour.py; PLAN "Grounding and zones").

  1. pours      one GND zone per outer layer over the whole board, priority 0 (under the In2 islands' priority 2,
                which are inner-layer anyway), 0.2 mm clearance, SOLID pad connection: gb3.kicad_dru turns the
                0402/0603 chip parts' GND pads to thermal reliefs (tombstoning) and fixes the hemmed-in ones; islands
                removed at the end
  2. islands    the In2 GND islands (switcher returns, X1, USB-C/D4) keep their fill only through GND vias: a 1.5 mm
                grid of them wherever all four layers are free (outside hard channels)
  3. usb        a via every USB_PITCH along both sides of the USB band (the F pour beside the pair stays at In1)
  4. return     a GND via within RET_R of every signal via: a layer change moves the signal's reference between
                In1 (GND) and In2 (+3V3) / the B pour, and this via is the short path its pour current takes back
  5. edge       a via every EDGE_PITCH along the board outline (and its cut-outs), EDGE_IN inside it
  6. field      a FIELD_PITCH grid wherever both pours have copper and no GND via is already near

A via goes only where it sits inside BOTH outer pours' fill with margin (so it clears every other net's copper on
F.Cu and B.Cu), on In1 (always GND) and on In2's GND or +3V3 copper (any: an antipad there is fine), keeps the
hole-to-hole rule to every other drill, and stays out of rule areas that forbid vias. Vias are the Default 0.6/0.3.
Rerunnable (strips route:pour first). Order: route.py -> fanout.py -> pour.py.

Run:  <kicad python> route/pour.py [board]
"""
import pcbnew, os, sys, math, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from channels import CHANNELS, SOFT
MM, TO = pcbnew.FromMM, pcbnew.ToMM
V = lambda x, y: pcbnew.VECTOR2I(int(round(MM(x))), int(round(MM(y))))
VIA_D, VIA_H = 0.6, 0.3
CLR, MIN_W = 0.2, 0.2
RET_R = 1.2                      # mm: a signal via with a GND via this close is covered
EDGE_PITCH, EDGE_IN = 2.5, 1.0
FIELD_PITCH, FIELD_CLEAR = 4.0, 2.0
ISLAND_PITCH = 1.5
USB_PITCH, USB_OFF = 2.0, 0.69   # mm along the band; mm outside the pair's outer edges (between D+ and CC_SENSE north)
USB_BAND = [((52.0, 47.0), (116.6, 47.0)), ((52.15, 47.475), (116.6, 47.475))]   # D+ / D- straight F runs (group 30)
HOLE_CLR = 0.25 + 0.05           # board min_hole_clearance / min_hole_to_hole + margin
PLANE_NETS = {"GND", "+3V3", "+0V9"}

board_path = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "..", "gb3.kicad_pcb")
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
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
    z.SetThermalReliefGap(MM(0.25)); z.SetThermalReliefSpokeWidth(MM(0.3))
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_NEVER)
    ol = z.Outline(); ol.NewOutline(); o = ps.COutline(0)
    for k in range(o.PointCount()): ol.Append(o.CPoint(k))
    B.Add(z); G.AddItem(z); zones[L] = z
pcbnew.ZONE_FILLER(B).Fill(B.Zones())
fills = {L: [z.GetFilledPolysList(L)] for L, z in zones.items()}
islands = [z for z in B.Zones() if not z.GetIsRuleArea() and z.GetNetname() == "GND" and z.IsOnLayer(pcbnew.In2_Cu)]
inner = {pcbnew.In1_Cu: [], pcbnew.In2_Cu: []}
for z in B.Zones():
    if z.GetIsRuleArea(): continue
    for L in inner:
        if z.IsOnLayer(L) and z.GetNetname() in ("GND", "+3V3"): inner[L].append(z.GetFilledPolysList(L))
no_via = [z for z in B.Zones() if z.GetIsRuleArea() and z.GetDoNotAllowVias()]

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

def _inside(x, y, poly):
    c = False
    for (x1, y1), (x2, y2) in zip(poly, poly[1:] + poly[:1]):
        if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1): c = not c
    return c
def in_channel(x, y, m=0.35):
    return any(_inside(x + dx, y + dy, poly) for name, (_, poly, _) in CHANNELS.items() if name not in SOFT
               for dx, dy in ((0, 0), (m, 0), (-m, 0), (0, m), (0, -m)))

def ring(x, y, r):
    return [(x, y)] + [(x + r * math.cos(a), y + r * math.sin(a)) for a in [k * math.pi / 4 for k in range(8)]]
def on_copper(L, x, y, r):
    return all(any(pp.Contains(V(px, py)) for pp in fills[L]) for px, py in ring(x, y, r))
def on_inner(L, x, y, r):
    return all(any(pp.Contains(V(px, py)) for pp in inner[L]) for px, py in ring(x, y, r))

def fits(x, y, island_ok=True):
    if any(math.hypot(hx - x, hy - y) < hr + VIA_H / 2 + HOLE_CLR for hx, hy, hr in holes): return False
    if any(z.Outline().Contains(V(x, y)) or z.Outline().SquaredDistance(V(x, y)) < MM(VIA_D / 2 + 0.1) ** 2 for z in no_via): return False
    if not (on_copper(pcbnew.F_Cu, x, y, VIA_D / 2 + 0.03) and on_copper(pcbnew.B_Cu, x, y, VIA_D / 2 + 0.03)): return False
    # In1 is GND everywhere but antipads; In2 must be plane copper (+3V3 or a GND island), not the edge of the +0V9 island
    return on_inner(pcbnew.In1_Cu, x, y, VIA_D / 2) and on_inner(pcbnew.In2_Cu, x, y, VIA_D / 2)

placed = {"island": 0, "usb": 0, "return": 0, "edge": 0, "field": 0}
uncovered = []
def add(x, y, kind):
    v = pcbnew.PCB_VIA(B); v.SetViaType(pcbnew.VIATYPE_THROUGH); v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    v.SetPosition(V(x, y)); v.SetWidth(MM(VIA_D)); v.SetDrill(MM(VIA_H)); v.SetNet(gnd); B.Add(v); G.AddItem(v)
    holes.append((x, y, VIA_H / 2)); gvias.append((x, y)); placed[kind] += 1

# ---- 2. the In2 GND islands: a 1.5 mm grid where all four layers are free, outside hard channels
for z in islands:
    bb = z.GetBoundingBox(); fill = z.GetFilledPolysList(pcbnew.In2_Cu)
    y = TO(bb.GetTop()) + ISLAND_PITCH / 2
    while y < TO(bb.GetBottom()):
        x = TO(bb.GetLeft()) + ISLAND_PITCH / 2
        while x < TO(bb.GetRight()):
            if (all(fill.Contains(V(px, py)) for px, py in ring(x, y, VIA_D / 2)) and not in_channel(x, y)
                    and not any(math.hypot(gx - x, gy - y) < ISLAND_PITCH * 0.7 for gx, gy in gvias) and fits(x, y)):
                add(x, y, "island")
            x += ISLAND_PITCH
        y += ISLAND_PITCH

# ---- 3. both sides of the USB band
(ax, ay), (bx, by) = USB_BAND[0]
y_n, y_s = USB_BAND[0][0][1] - 0.1375 - USB_OFF, USB_BAND[1][0][1] + 0.1375 + USB_OFF
x = ax + 1.0
while x < bx - 1.0:
    for yy in (y_n, y_s):
        if not any(math.hypot(gx - x, gy - yy) < USB_PITCH * 0.6 for gx, gy in gvias) and fits(x, yy):
            add(x, yy, "usb")
    x += USB_PITCH

# ---- 4. return vias beside signal vias
for sx, sy in sig_vias:
    if any(math.hypot(gx - sx, gy - sy) <= RET_R for gx, gy in gvias): continue
    done = False
    for r in (0.8, 0.9, 1.0, 1.2, 1.5, 1.8):
        for k in range(24):
            a = 2 * math.pi * k / 24; x, y = sx + r * math.cos(a), sy + r * math.sin(a)
            if fits(x, y): add(x, y, "return"); done = True; break
        if done: break
    if not done: uncovered.append((sx, sy))

# ---- 5. edge stitching
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

# ---- 6. field stitching
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
pro = os.path.splitext(board_path)[0] + ".kicad_pro"
keep = open(pro, "rb").read() if os.path.exists(pro) else None
pcbnew.SaveBoard(board_path, B)
if keep is not None and open(pro, "rb").read() != keep: open(pro, "wb").write(keep)   # SaveBoard rewrites the project file
print("pour: GND on F.Cu + B.Cu; stitching vias: %(island)d island, %(usb)d USB band, %(return)d return, %(edge)d edge, "
      "%(field)d field" % placed)
print("  signal vias with no GND via within %.1f mm (no room): %d" % (RET_R, len(uncovered)))
for u in uncovered[:40]: print("    (%.2f, %.2f)" % u)
sys.stdout.flush(); os._exit(0)
