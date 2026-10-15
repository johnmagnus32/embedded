#!/usr/bin/env python3
"""fanout.py — gb3 plane fanout: a stitching via beside every GND / +3V3 / +0V9 SMD pad that can't reach its plane.

Ported from gb2's route/fanout.py (itself from the first gb3's fanout_planes.py). gb3: planes are In1.Cu (GND) and
In2.Cu (+3V3, with the Z1 GND islands and the Z2 +0V9 island under U1); vias are the Default 0.6/0.3; a big IC's pins
(>= 32 pads) look for their via inward first, under the package body, so the outside of the pin ring stays free for
signal escape; a QFN's GND pin ties straight into its exposed pad when that stub is clear; exposed pads keep the via
grids groups 05 / 20 gave them (one is added only to an EP that has none). The switcher caps' GND vias are group 05's.
Everything added goes in the KiCad group "route:fanout", which a re-run deletes and rebuilds (route.py strips it too,
so it always runs after the route groups, which it dodges). The outer GND pours are pour.py's and come after: every
plane pad is judged against its inner plane alone, so each decap gets its own via.

Stackup is F.Cu / In1 GND / In2 +3V3 / B.Cu, so a plane-net pad never needs a route, only a short stub and a via.

For each pad whose connectivity cluster does not already contain its inner plane zone:
  * try via positions on 16 bearings, 0.1 mm steps out to 2.0 mm, shortest stub first
  * accept the first one where
      - the via clears every other-net pad / track / via on all four layers by the pair's netclass
        clearance (the larger of the two, as KiCad's DRC does, read from gb3.kicad_pro), and sits off every
        same-net pad too (no via-in-pad),
      - its drill clears every other hole by HOLE_CLR,
      - it is >= EDGE_CLR inside the board outline and outside the BT antenna keepout,
      - the stub (pad edge -> via, on the pad's layer) clears every other-net item on that layer likewise
  * one via per pad, not per island: these are mostly decoupling returns and each wants its own via.
  * exposed / thermal pads (narrow side >= THERMAL_MIN) instead get an array of vias inside the pad,
    no stub, which is also what they need thermally -- so they get one even when a pour already ties
    them on-plane (the second route left U12's PGND pad with no via at all), topped up to THERMAL_MAX.
Pads that still have no fit are re-checked once everything is placed: many of them are tied by a
routed trace to a pad that did get a via, and are on-plane after all. What is left then gets a second
try: one straight trace on the pad's own layer to the nearest on-plane same-net via or pad within
TIE_MAX (typically a U1 supply pin to its own bypass cap), if that path is clear.

Vias are 0.6 / 0.3 (the Default netclass via) for both nets; per-pad current here is decoupling-level,
and 0.8 mm Power vias would not fit beside U1's 0.4 mm-pitch pins. Stub width is the pad's narrow side
clamped to 0.2..0.3 mm. Pads that fit nowhere are listed for hand work. Zones are re-poured at the end.

Re-running is safe: pads the previous run connected are on-plane now and get skipped.

Run:  <kicad python> route/fanout.py [board.kicad_pcb]      (default gb3.kicad_pcb, in place; after route.py)
"""
import pcbnew, os, sys, math, json, fnmatch, subprocess
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
MM = pcbnew.FromMM; TO = pcbnew.ToMM
def V(x, y): return pcbnew.VECTOR2I(int(round(x)), int(round(y)))

PLANES = {"GND": pcbnew.In1_Cu, "+3V3": pcbnew.In2_Cu, "+0V9": pcbnew.In2_Cu}      # net -> inner plane layer
VIA_D, VIA_H = MM(0.6), MM(0.3)
BIG_IC = 32                                 # pads: an IC this big fans out inward first
SKIP_REFS = set()
sys.path.insert(0, HERE)
from channels import CHANNELS, SOFT
def _inside(x, y, poly):              # even-odd point in polygon (board mm)
    c = False
    for (x1, y1), (x2, y2) in zip(poly, poly[1:] + poly[:1]):
        if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1): c = not c
    return c
def in_channel(x, y, m=0.35):         # a through via anywhere in a reserved channel (either layer) blocks it
    return any(_inside(x + dx, y + dy, poly) for name, (_, poly, _) in CHANNELS.items() if name not in SOFT
               for dx, dy in ((0, 0), (m, 0), (-m, 0), (0, m), (0, -m)))
AVOID = [True]                        # first pass keeps out of the channels; a pad with no fit retries without
def skip(p):
    return p.GetParentFootprint().GetReference() in SKIP_REFS
CLR_MARGIN = MM(0.01)                       # on top of the pair's netclass clearance
SAME_NET_PAD_CLR = MM(0.10)                 # keep the via off its neighbours' pads
HOLE_CLR = MM(0.27)                         # min_hole_to_hole / min_hole_clearance 0.25 + margin
EDGE_CLR = MM(0.52)                         # min_copper_edge_clearance 0.5 + margin
KEEPOUT_CLR = MM(0.20)
STUB_W = (MM(0.20), MM(0.30))
TIE_MAX = MM(5.0)
TIE_FIRST = MM(2.5)                         # a tie this short beats a via inside a reserved channel
DECAP_MAX = MM(3.0)                         # a cap pad this close to its IC's same-net pin is tied straight to it
# switching stages' cap GND pads with a SECOND via (the hot-loop return): on gb3 group 05 draws those itself
DOUBLE = set()
BEARINGS = 24
RADII = [MM(0.1 * k) for k in range(3, 26)]  # 0.3 .. 2.5 mm


def candidates():
    """(radius, bearing, dogleg) to try: every radius on the 45 deg grid first (a straight stub), then the 15 deg
    bearings reached by a two-segment octilinear dogleg - no stub ever leaves the 45 deg grid."""
    for r in RADII:
        for k in range(8): yield r, 2 * math.pi * k / 8, False
    for r in RADII:
        for k in range(BEARINGS):
            if k % (BEARINGS // 8): yield r, 2 * math.pi * k / BEARINGS, True
THERMAL_MIN = MM(1.4)                       # a pad this wide on its narrow side is an exposed pad
THERMAL_PITCH, THERMAL_MAX = MM(1.1), 9
CELL = MM(2.0)


class Grid:
    """Bucket obstacles by bbox so each candidate only tests its neighbourhood."""
    def __init__(self): self.g = defaultdict(list)
    def _cells(self, bb):
        for i in range(int(bb.GetLeft() // CELL), int(bb.GetRight() // CELL) + 1):
            for j in range(int(bb.GetTop() // CELL), int(bb.GetBottom() // CELL) + 1):
                yield i, j
    def add(self, ob):
        for c in self._cells(ob["bb"]): self.g[c].append(ob)
    def near(self, x, y, r):
        seen = set()
        for i in range(int((x - r) // CELL), int((x + r) // CELL) + 1):
            for j in range(int((y - r) // CELL), int((y + r) // CELL) + 1):
                for ob in self.g.get((i, j), ()):
                    if id(ob) not in seen:
                        seen.add(id(ob)); yield ob


def netclass_clearances(pro):
    """net name -> clearance (nm), from the project's netclass patterns. The bindings can't read a net's
    effective netclass (GetEffectiveNetClass returns an unusable proxy), so resolve it the way KiCad
    does: first matching pattern wins, else Default."""
    ns = json.load(open(pro))["net_settings"]
    clr = {c["name"]: MM(c["clearance"]) for c in ns["classes"]}
    pats = [(p["pattern"], p["netclass"]) for p in ns.get("netclass_patterns", [])]
    def lookup(name):
        for pat, cls in pats:
            if fnmatch.fnmatchcase(name, pat) and cls in clr: return clr[cls]
        return clr["Default"]
    return lookup


def cu_layers(item):
    return [l for l in item.GetLayerSet().CuStack()]


SPECIAL_CLR = {"TP11": MM(0.6)}             # gb3.kicad_dru "TP11 isolation": its pad keeps 0.6 mm to everything

def obstacle(item, kind):
    clr = CLR_OF(item.GetNetname())
    if kind == "pad": clr = max(clr, SPECIAL_CLR.get(item.GetParentFootprint().GetReference(), 0))
    ob = {"kind": kind, "net": item.GetNetCode(), "clr": clr,
          "item": item, "bb": item.GetBoundingBox(),
          "shapes": {l: item.GetEffectiveShape(l) for l in cu_layers(item)}, "hole": None}
    if kind == "via" or (kind == "pad" and item.HasHole()):
        ob["hole"] = item.GetEffectiveHoleShape()
    return ob


def stub_start(pad, x, y, w):
    """Where the stub leaves the pad: the bbox exit point toward (x, y), pulled back w/2 so the round
    cap still sits inside the pad. Drawing it from the pad centre instead would test the part buried
    in the pad against the neighbouring pins, which fails on every 0.4 mm-pitch part for no reason."""
    c, bb = pad.GetPosition(), pad.GetBoundingBox()
    dx, dy = x - c.x, y - c.y
    L = math.hypot(dx, dy)
    if L == 0: return c
    dx, dy = dx / L, dy / L
    t = min(bb.GetWidth() / 2 / abs(dx) if dx else float("inf"),
            bb.GetHeight() / 2 / abs(dy) if dy else float("inf"))
    if pad.GetShape() == pcbnew.PAD_SHAPE_CIRCLE:     # a round pad's bbox corner is outside its copper
        t = min(t, bb.GetWidth() / 2)
    t = max(0.0, min(L, t - w / 2))
    return V(c.x + dx * t, c.y + dy * t)


def stub_path(pad, x, y, w, dog):
    """pad -> via (x, y): None for a straight stub, else the [start, knee, via] octilinear dogleg (two variants)."""
    if not dog: return [None]
    out = []
    for pts in octi_paths(pad.GetPosition(), V(x, y))[:2]:
        knee = pts[1]
        out.append([stub_start(pad, knee.x, knee.y, w), knee, V(x, y)])
    return out


def octi_paths(c, q):
    """pad centre c -> target q: the two octilinear doglegs (straight run then 45 deg, 45 deg then straight), then
    the plain straight line as the last resort."""
    dx, dy = q.x - c.x, q.y - c.y
    m = min(abs(dx), abs(dy)); sx, sy = (1 if dx >= 0 else -1), (1 if dy >= 0 else -1)
    if abs(dx) >= abs(dy): k1 = V(q.x - sx * m, c.y); k2 = V(c.x + sx * m, q.y)
    else: k1 = V(c.x, q.y - sy * m); k2 = V(q.x, c.y + sy * m)
    return [[c, k1, q], [c, k2, q], [c, q]]


def seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    L = dx * dx + dy * dy
    t = 0 if L == 0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / L))
    return math.hypot(px - ax - t * dx, py - ay - t * dy)


def main():
    board_path = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "..", "gb3.kicad_pcb")
    subprocess.run([sys.executable, os.path.join(HERE, "route.py"), "--strip", board_path, "fanout"], check=True)
    B = pcbnew.LoadBoard(board_path)
    G = pcbnew.PCB_GROUP(B); G.SetName("route:fanout"); B.Add(G)
    global CLR_OF
    pro = os.path.splitext(board_path)[0] + ".kicad_pro"
    CLR_OF = netclass_clearances(pro if os.path.exists(pro) else os.path.join(HERE, "..", "gb3.kicad_pro"))
    need = lambda pad, ob: max(CLR_OF(pad.GetNetname()), ob["clr"]) + CLR_MARGIN
    conn = B.GetConnectivity()

    plane, outline = {}, {}
    for z in B.Zones():
        if z.GetIsRuleArea(): continue
        for net, lid in PLANES.items():
            if z.GetNetname() == net and z.IsOnLayer(lid):
                plane[net] = z.m_Uuid.AsString(); outline[net] = z.GetFilledPolysList(lid)
    # rule areas that forbid vias (the antenna, X2, the M2 hole keep-outs ...); the DRC-only 'pins: <ref>' areas and
    # X1's no-tracks area allow them
    keepouts = [z for z in B.Zones() if z.GetIsRuleArea() and z.GetDoNotAllowVias()]

    ps = pcbnew.SHAPE_POLY_SET(); B.GetBoardPolygonOutlines(ps, True)
    ol = ps.COutline(0)
    edge = [(ol.CPoint(k).x, ol.CPoint(k).y) for k in range(ol.PointCount())]
    edge = list(zip(edge, edge[1:] + edge[:1]))

    grid = Grid()
    for fp in B.GetFootprints():
        for p in fp.Pads(): grid.add(obstacle(p, "pad"))
    for t in B.GetTracks():
        grid.add(obstacle(t, "via" if t.Type() == pcbnew.PCB_VIA_T else "track"))

    def on_plane(pad):
        z = plane[pad.GetNetname()]
        return any(it.Type() == pcbnew.PCB_ZONE_T and it.m_Uuid.AsString() == z
                   for it in conn.GetConnectedItems(pad))

    def is_thermal(p):
        # an IC's exposed pad; a connector's big pads are mounting tabs, and a via in one wicks the
        # solder out of the joint that holds the connector down, so those get an ordinary stub via
        # (the microSD socket is a U too, so also require the pad to sit under the part's centre, as an
        # exposed pad does and a mounting tab doesn't)
        bb, fp = p.GetBoundingBox(), p.GetParentFootprint()
        c, o = p.GetPosition(), fp.GetPosition()
        return (fp.GetReference().startswith("U") and math.hypot(c.x - o.x, c.y - o.y) < MM(1.0)
                and min(bb.GetWidth(), bb.GetHeight()) >= THERMAL_MIN)

    todo = []
    for fp in B.GetFootprints():
        for p in fp.Pads():
            dbl = "%s.%s" % (fp.GetReference(), p.GetPadName()) in DOUBLE   # switcher caps: own vias even if a pour ties them
            if p.GetNetname() in plane and not p.HasHole() and not is_thermal(p) and not skip(p) and (dbl or not on_plane(p)):
                todo.append(p)

    def fits(pad, layer, w, x, y, in_pad=False, path=None):
        via = pcbnew.SHAPE_CIRCLE(V(x, y), VIA_D // 2)
        hole = pcbnew.SHAPE_CIRCLE(V(x, y), VIA_H // 2)
        c = pad.GetPosition()
        if in_pad: stubs = []
        elif path: stubs = [pcbnew.SHAPE_SEGMENT(p0, p1, w) for p0, p1 in zip(path, path[1:]) if (p0.x, p0.y) != (p1.x, p1.y)]
        else: stubs = [pcbnew.SHAPE_SEGMENT(stub_start(pad, x, y, w), V(x, y), w)]
        if any(z.Outline().Contains(V(x, y)) or z.Outline().SquaredDistance(V(x, y)) < (VIA_D // 2 + KEEPOUT_CLR) ** 2
               for z in keepouts): return "keepout"
        if not ps.Contains(V(x, y)): return "off-board"
        if AVOID[0] and in_channel(TO(x), TO(y)): return "channel"
        if not outline[pad.GetNetname()].Contains(V(x, y)):   # on its own plane's copper (In2: +3V3, its GND islands, the +0V9 island)
            return "off-plane"
        if min(seg_dist(x, y, a[0], a[1], b[0], b[1]) for a, b in edge) < EDGE_CLR + VIA_D // 2: return "edge"
        me = pad.m_Uuid.AsString()
        for ob in grid.near(x, y, MM(3.0)):
            it = ob["item"]
            same = ob["net"] == pad.GetNetCode()
            if ob["hole"] is not None and ob["hole"].Collide(hole, HOLE_CLR): return "hole"
            if ob["kind"] == "pad" and it.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
                if ob["hole"] is not None and ob["hole"].Collide(via, HOLE_CLR): return "npth"
                continue
            for l, sh in ob["shapes"].items():
                if same:
                    if ob["kind"] == "pad" and it.m_Uuid.AsString() != me and sh.Collide(via, SAME_NET_PAD_CLR):
                        return "same-net pad"
                    if ob["kind"] == "pad" and it.m_Uuid.AsString() == me and not in_pad \
                            and sh.Collide(via, 0):
                        return "own pad"                  # via-in-own-pad: keep it beside the pad
                    continue
                if sh.Collide(via, need(pad, ob)): return "via vs " + ob["kind"] + ("@" + B.GetLayerName(l) if ob["kind"] != "via" else "")
                if l == layer and any(sh.Collide(st, need(pad, ob)) for st in stubs): return "stub vs " + ob["kind"]
        return None

    def add_stub(pad, layer, w, x, y, path):
        pts = path or [stub_start(pad, x, y, w), V(x, y)]
        for p0, p1 in zip(pts, pts[1:]):
            if (p0.x, p0.y) == (p1.x, p1.y): continue
            t = pcbnew.PCB_TRACK(B); t.SetStart(p0); t.SetEnd(p1); t.SetWidth(w); t.SetLayer(layer)
            t.SetNet(pad.GetNet()); B.Add(t); G.AddItem(t); grid.add(obstacle(t, "track"))

    def add_via(pad, x, y):
        v = pcbnew.PCB_VIA(B)
        v.SetViaType(pcbnew.VIATYPE_THROUGH)
        v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        v.SetPosition(V(x, y)); v.SetWidth(VIA_D); v.SetDrill(VIA_H)
        v.SetNet(pad.GetNet()); B.Add(v); G.AddItem(v)
        grid.add(obstacle(v, "via"))

    tied = []

    def tie_now(pad, layer, w):
        """straight trace on the pad's layer to the nearest same-net via/pad already on its plane, within TIE_FIRST."""
        c = pad.GetPosition(); best = None
        for ob in grid.near(c.x, c.y, TIE_FIRST):
            it = ob["item"]
            if ob["net"] != pad.GetNetCode() or ob["kind"] == "track" or layer not in ob["shapes"] or it is pad: continue
            if ob["kind"] == "pad" and not on_plane(it): continue
            q = it.GetPosition(); d = math.hypot(q.x - c.x, q.y - c.y)
            if d > TIE_FIRST or (best and d >= best[0]): continue
            tpath = None
            for pts in octi_paths(c, q):
                a = stub_start(pad, pts[1].x, pts[1].y, w); pts = [a] + pts[1:]
                if all(not any(ob2["net"] != pad.GetNetCode() and ob2["shapes"].get(layer) is not None and
                               ob2["shapes"][layer].Collide(pcbnew.SHAPE_SEGMENT(p0, p1, w), need(pad, ob2))
                               for ob2 in grid.near((p0.x + p1.x) / 2, (p0.y + p1.y) / 2, d / 2 + MM(1.5)))
                       for p0, p1 in zip(pts, pts[1:]) if (p0.x, p0.y) != (p1.x, p1.y)):
                    tpath = pts; break
            if tpath is None: continue
            best = (d, tpath)
        if not best: return False
        d, tpath = best
        for p0, p1 in zip(tpath, tpath[1:]):
            if (p0.x, p0.y) == (p1.x, p1.y): continue
            t = pcbnew.PCB_TRACK(B); t.SetStart(p0); t.SetEnd(p1); t.SetWidth(w); t.SetLayer(layer); t.SetNet(pad.GetNet())
            B.Add(t); G.AddItem(t); grid.add(obstacle(t, "track"))
        return True

    placed, failed, thermal, thermal_none = [], [], [], []
    ep_ties = []
    in_chan = []
    doubled = []

    def ep_tie(pad, layer):
        """a QFN GND pin: a straight stub from the pin into its own exposed pad (which has thermal vias)."""
        fp = pad.GetParentFootprint()
        eps = [q for q in fp.Pads() if q is not pad and q.GetNetname() == "GND" and is_thermal(q)]
        if not eps: return False
        ep = eps[0]; e = ep.GetBoundingBox(); c = pad.GetPosition()
        tx = min(max(c.x, e.GetLeft() + MM(0.2)), e.GetRight() - MM(0.2))
        ty = min(max(c.y, e.GetTop() + MM(0.2)), e.GetBottom() - MM(0.2))
        bb = pad.GetBoundingBox(); w = min(MM(0.25), min(bb.GetWidth(), bb.GetHeight()))
        seg = pcbnew.SHAPE_SEGMENT(c, V(tx, ty), w)
        for ob in grid.near(c.x, c.y, MM(3.0)):
            if ob["net"] == pad.GetNetCode(): continue
            sh = ob["shapes"].get(layer)
            if sh is not None and sh.Collide(seg, need(pad, ob)): return False
        t = pcbnew.PCB_TRACK(B); t.SetStart(c); t.SetEnd(V(tx, ty)); t.SetWidth(w); t.SetLayer(layer)
        t.SetNet(pad.GetNet()); B.Add(t); G.AddItem(t); grid.add(obstacle(t, "track"))
        ep_ties.append("%s.%s" % (fp.GetReference(), pad.GetPadName())); return True
    # ---- decaps first: each supply-net cap pad gets a direct octilinear trace to its IC's nearest same-net pin on
    # the same layer (the pin it decouples), and that IC's other same-net pins next to it (logic ties such as a
    # flash's /WP, /HOLD) link to that pin - so cap and pin share ONE via, placed by the normal search at the cap.
    decap_tied, decap_log = set(), []
    ic_pads = [p for fp in B.GetFootprints() if fp.GetReference().startswith("U") for p in fp.Pads() if not p.HasHole()]
    def clear_path(pad, layer, w, pts):
        for p0, p1 in zip(pts, pts[1:]):
            if (p0.x, p0.y) == (p1.x, p1.y): continue
            seg = pcbnew.SHAPE_SEGMENT(p0, p1, w)
            for ob in grid.near((p0.x + p1.x) / 2, (p0.y + p1.y) / 2, math.hypot(p1.x - p0.x, p1.y - p0.y) / 2 + MM(1.5)):
                if ob["net"] == pad.GetNetCode(): continue
                sh = ob["shapes"].get(layer)
                if sh is not None and sh.Collide(seg, need(pad, ob)): return False
        return True
    def draw(pad, layer, w, pts):
        for p0, p1 in zip(pts, pts[1:]):
            if (p0.x, p0.y) == (p1.x, p1.y): continue
            t = pcbnew.PCB_TRACK(B); t.SetStart(p0); t.SetEnd(p1); t.SetWidth(w); t.SetLayer(layer); t.SetNet(pad.GetNet())
            B.Add(t); G.AddItem(t); grid.add(obstacle(t, "track"))
    for fp in B.GetFootprints():
        if not fp.GetReference().startswith("C") or len(fp.Pads()) != 2: continue
        pp = list(fp.Pads())
        rail = [p for p in pp if p.GetNetname() in plane and p.GetNetname() != "GND"]
        if len(rail) != 1 or not any(p.GetNetname() == "GND" for p in pp): continue
        cp = rail[0]; layer = pcbnew.F_Cu if cp.IsOnLayer(pcbnew.F_Cu) else pcbnew.B_Cu
        c0 = cp.GetPosition()
        near = sorted((q for q in ic_pads if q.GetNetCode() == cp.GetNetCode() and q.IsOnLayer(layer) and not is_thermal(q)
                       and math.hypot(q.GetPosition().x - c0.x, q.GetPosition().y - c0.y) < DECAP_MAX),
                      key=lambda q: math.hypot(q.GetPosition().x - c0.x, q.GetPosition().y - c0.y))
        linked = {it.m_Uuid.AsString() for it in conn.GetConnectedItems(cp)}
        if any(q.m_Uuid.AsString() in linked for q in near): continue   # a route group already joins them
        for q in near:
            qb = q.GetBoundingBox(); w = min(MM(0.3), min(qb.GetWidth(), qb.GetHeight()))
            dpath = next((pts for pts in octi_paths(c0, q.GetPosition()) if clear_path(cp, layer, w, pts)), None)
            if not dpath: continue
            draw(cp, layer, w, dpath); decap_tied.add(q.m_Uuid.AsString())
            ties = []
            for o in q.GetParentFootprint().Pads():       # logic-tie neighbours of that pin
                if o.m_Uuid.AsString() == q.m_Uuid.AsString() or o.GetNetCode() != q.GetNetCode() or not o.IsOnLayer(layer) or is_thermal(o): continue
                if math.hypot(o.GetPosition().x - q.GetPosition().x, o.GetPosition().y - q.GetPosition().y) > MM(1.6): continue
                ob = o.GetBoundingBox(); w2 = min(MM(0.3), min(ob.GetWidth(), ob.GetHeight()))
                pts = [q.GetPosition(), o.GetPosition()]
                if clear_path(o, layer, w2, pts):
                    draw(o, layer, w2, pts); decap_tied.add(o.m_Uuid.AsString()); ties.append(o.GetPadName())
            decap_log.append("%s -> %s.%s%s" % (fp.GetReference(), q.GetParentFootprint().GetReference(), q.GetPadName(),
                                                 (" (+ pins %s)" % ",".join(ties)) if ties else ""))
            break
    todo = [p for p in todo if p.m_Uuid.AsString() not in decap_tied]

    for pad in todo:
        layer = [l for l in cu_layers(pad) if l in (pcbnew.F_Cu, pcbnew.B_Cu)][0]
        bb = pad.GetBoundingBox()
        ref = "%s.%s" % (pad.GetParentFootprint().GetReference(), pad.GetPadName())
        if is_thermal(pad):
            inset = VIA_D // 2 + MM(0.10)
            xs = bb.GetLeft() + inset, bb.GetRight() - inset
            ys = bb.GetTop() + inset, bb.GetBottom() - inset
            nx = max(1, int((xs[1] - xs[0]) // THERMAL_PITCH) + 1)
            ny = max(1, int((ys[1] - ys[0]) // THERMAL_PITCH) + 1)
            cand = [(xs[0] + (xs[1] - xs[0]) * (i / (nx - 1) if nx > 1 else 0.5),
                     ys[0] + (ys[1] - ys[0]) * (j / (ny - 1) if ny > 1 else 0.5))
                    for i in range(nx) for j in range(ny)]
            c = pad.GetPosition()
            cand.sort(key=lambda q: math.hypot(q[0] - c.x, q[1] - c.y))   # centre outward
            have = [ob["item"].GetPosition() for ob in grid.near(c.x, c.y, max(bb.GetWidth(), bb.GetHeight()))
                    if ob["kind"] == "via" and pad.HitTest(ob["item"].GetPosition())]
            n, why = 0, defaultdict(int)
            if have: continue                 # groups 05 / 20 own the EP grids; only a bare EP gets vias here
            for x, y in cand:
                if len(have) + n >= THERMAL_MAX: break
                if any(math.hypot(q.x - x, q.y - y) < THERMAL_PITCH * 0.8 for q in have):
                    why["existing via"] += 1; continue
                if not pad.HitTest(V(x, y)): why["outside pad"] += 1; continue
                r = fits(pad, layer, 0, x, y, in_pad=True)
                if r is None:
                    add_via(pad, x, y); have.append(V(x, y)); n += 1
                else: why[r] += 1
            if n: thermal.append((ref, pad.GetNetname(), n))
            elif len(have) < THERMAL_MAX:
                thermal_none.append((ref, len(have), dict(why)))
                if not on_plane(pad): failed.append((pad, ref, "no room in pad"))
            continue
        w = max(STUB_W[0], min(STUB_W[1], min(bb.GetWidth(), bb.GetHeight())))
        c = pad.GetPosition()
        fp = pad.GetParentFootprint()
        if pad.GetNetname() == "GND" and ep_tie(pad, layer):
            continue
        hit = None
        blockers = defaultdict(int)
        if len(fp.Pads()) >= BIG_IC:
            # a power pin's own escape lane: straight out along the pin axis (where the pitch leaves room beside the
            # neighbours' straight 0.15 lanes), else straight in under the body
            o = fp.GetPosition(); dx, dy = o.x - c.x, o.y - c.y
            if abs(dx) > abs(dy): dx, dy = math.copysign(1, dx), 0
            else: dx, dy = 0, math.copysign(1, dy)
            half = max(bb.GetWidth(), bb.GetHeight()) / 2
            AVOID[0] = False                  # the pin's own lane is the power pin's even inside a channel
            for sgn in (-1, 1):
                for r in [half + MM(0.05 * k) for k in range(6, 21)]:
                    x, y = c.x + sgn * r * dx, c.y + sgn * r * dy
                    if fits(pad, layer, w, x, y) is None:
                        hit = (x, y, r); break
                if hit: break
            AVOID[0] = True
        spath = None
        for r, a, dog in ([] if hit else candidates()):
            x, y = c.x + r * math.cos(a), c.y + r * math.sin(a)
            for pth in stub_path(pad, x, y, w, dog):
                why = fits(pad, layer, w, x, y, path=pth)
                if why is None: hit, spath = (x, y, r), pth; break
                blockers[why] += 1
            if hit: break
        if not hit and tie_now(pad, layer, w):  # before giving up a channel: a short tie to on-plane copper
            tied.append((ref, pad.GetNetname(), 0.0)); continue
        if not hit:                           # second chance: allow a reserved channel rather than leave it off-plane
            AVOID[0] = False
            for r, a, dog in candidates():
                x, y = c.x + r * math.cos(a), c.y + r * math.sin(a)
                for pth in stub_path(pad, x, y, w, dog):
                    if fits(pad, layer, w, x, y, path=pth) is None: hit, spath = (x, y, r), pth; break
                if hit: break
            AVOID[0] = True
            if hit: in_chan.append(ref)
        if not hit:
            failed.append((pad, ref, max(blockers, key=blockers.get))); continue
        x, y, r = hit
        add_via(pad, x, y)
        if ref in DOUBLE:                       # second via: nearest other spot >= 0.8 mm from the first
            done2 = False
            for r2, a, dog in candidates():
                x2, y2 = c.x + r2 * math.cos(a), c.y + r2 * math.sin(a)
                if math.hypot(x2 - x, y2 - y) < MM(0.8): continue
                for pth in stub_path(pad, x2, y2, w, dog):
                    pts2 = pth or [stub_start(pad, x2, y2, w), V(x2, y2)]
                    if any(seg_dist(x, y, a_.x, a_.y, b_.x, b_.y) < VIA_D / 2 + w / 2 for a_, b_ in zip(pts2, pts2[1:])):
                        continue                    # its stub would run over the first via
                    if fits(pad, layer, w, x2, y2, path=pth) is None:
                        add_via(pad, x2, y2); add_stub(pad, layer, w, x2, y2, pth); doubled.append(ref); done2 = True; break
                if done2: break
        add_stub(pad, layer, w, x, y, spath)
        placed.append((ref, pad.GetNetname(), TO(r)))

    # a pad tied by a routed trace to one that got a via is on-plane now
    B.BuildConnectivity()
    conn = B.GetConnectivity()
    joined = [ref for pad, ref, _ in failed if on_plane(pad)]
    failed = [f for f in failed if not on_plane(f[0])]

    # second try: a straight trace to the nearest on-plane same-net via or pad on the same layer
    def path_clear(pad, layer, a, b, w, target):
        seg = pcbnew.SHAPE_SEGMENT(a, b, w)
        mx, my = (a.x + b.x) / 2, (a.y + b.y) / 2
        for ob in grid.near(mx, my, math.hypot(b.x - a.x, b.y - a.y) / 2 + MM(1.5)):
            if ob["net"] == pad.GetNetCode(): continue
            sh = ob["shapes"].get(layer)
            if sh is not None and sh.Collide(seg, need(pad, ob)): return False
        return True

    for f in list(failed):
        pad, ref = f[0], f[1]
        layer = [l for l in cu_layers(pad) if l in (pcbnew.F_Cu, pcbnew.B_Cu)][0]
        bb = pad.GetBoundingBox()
        w = max(STUB_W[0], min(STUB_W[1], min(bb.GetWidth(), bb.GetHeight())))
        c = pad.GetPosition()
        best = None
        for ob in grid.near(c.x, c.y, TIE_MAX):
            it = ob["item"]
            if ob["net"] != pad.GetNetCode() or ob["kind"] == "track" or layer not in ob["shapes"]: continue
            if it.m_Uuid.AsString() == pad.m_Uuid.AsString(): continue
            if not any(z.Type() == pcbnew.PCB_ZONE_T and z.m_Uuid.AsString() == plane[pad.GetNetname()]
                       for z in conn.GetConnectedItems(it)): continue
            q = it.GetPosition()
            d = math.hypot(q.x - c.x, q.y - c.y)
            if d > TIE_MAX or (best and d >= best[0]): continue
            for pts in octi_paths(c, q):                  # octilinear: straight run + 45 deg, either order
                pts = [stub_start(pad, pts[1].x, pts[1].y, w)] + pts[1:]
                if all(path_clear(pad, layer, p0, p1, w, it) for p0, p1 in zip(pts, pts[1:]) if (p0.x, p0.y) != (p1.x, p1.y)):
                    best = (d, pts); break
        if best:
            d, pts = best
            for p0, p1 in zip(pts, pts[1:]):
                if (p0.x, p0.y) == (p1.x, p1.y): continue
                t = pcbnew.PCB_TRACK(B)
                t.SetStart(p0); t.SetEnd(p1); t.SetWidth(w); t.SetLayer(layer); t.SetNet(pad.GetNet()); B.Add(t); G.AddItem(t)
                grid.add(obstacle(t, "track"))
            tied.append((ref, pad.GetNetname(), TO(d)))
            failed.remove(f)
    failed = [(ref, pad.GetNetname(), TO(pad.GetPosition().x), TO(pad.GetPosition().y), why)
              for pad, ref, why in failed if not on_plane(pad)]

    pcbnew.ZONE_FILLER(B).Fill(B.Zones())
    keep = open(pro, "rb").read() if os.path.exists(pro) else None
    pcbnew.SaveBoard(board_path, B)
    if keep is not None and open(pro, "rb").read() != keep: open(pro, "wb").write(keep)   # SaveBoard rewrites it

    by = defaultdict(int)
    for _, n, _ in placed: by[n] += 1
    sys.stderr.write("fanout: %d plane pads off-plane -> %d stub vias (%s), %d thermal pads "
                     "(%d vias), %d joined via a neighbour, %d tied to a nearby plane item, %d no fit\n"
                     % (len(todo), len(placed), ", ".join("%s %d" % kv for kv in sorted(by.items())),
                        len(thermal), sum(n for *_, n in thermal), len(joined), len(tied), len(failed)))
    if decap_log: sys.stderr.write("  decap -> pin ties (one shared via): %s\n" % "; ".join(decap_log))
    if ep_ties: sys.stderr.write("  tied into the exposed pad: %s\n" % " ".join(ep_ties))
    if doubled: sys.stderr.write("  second GND via (switcher caps): %s\n" % " ".join(doubled))
    if in_chan: sys.stderr.write("  via inside a reserved channel (no other fit): %s\n" % " ".join(in_chan))
    for ref, n, d in tied:
        sys.stderr.write("  tied:    %-10s %-5s %.2f mm trace\n" % (ref, n, d))
    for ref, n, k in thermal:
        sys.stderr.write("  thermal: %-10s %-5s %d vias in pad\n" % (ref, n, k))
    for ref, have, why in thermal_none:
        sys.stderr.write("  thermal: %-10s none added (%d already there; candidates blocked by %s)\n" % (ref, have, why))
    if placed:
        sys.stderr.write("  stub length: median %.2f mm, max %.2f mm\n"
                         % (sorted(r for *_, r in placed)[len(placed) // 2], max(r for *_, r in placed)))
    for ref, n, x, y, why in failed:
        sys.stderr.write("  no fit: %-10s %-5s (%7.2f, %6.2f)  mostly blocked by %s\n" % (ref, n, x, y, why))
    sys.stderr.flush(); os._exit(0)


if __name__ == "__main__":
    main()
