#!/usr/bin/env python3
"""fanout_planes.py — drop a stitching via next to every GND / +3V3 SMD pad that can't reach its plane.

Stackup is F.Cu / GND / PWR / B.Cu with a solid GND plane on layer 2 and a solid +3V3 plane on layer 3,
so a plane-net pad never needs a route, only a short stub and a via. FreeRouting leaves a lot of these
open (it spends its passes on signals), and they are ~40% of the ratsnest, so do them deterministically.

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

Run:  <kicad python> fanout_planes.py [board.kicad_pcb]      (default gb3.kicad_pcb, in place)
"""
import pcbnew, os, sys, math, json, fnmatch
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
MM = pcbnew.FromMM; TO = pcbnew.ToMM
def V(x, y): return pcbnew.VECTOR2I(int(round(x)), int(round(y)))

PLANES = {"GND": "GND", "+3V3": "PWR"}      # net -> inner plane layer name
VIA_D, VIA_H = MM(0.6), MM(0.3)
CLR_MARGIN = MM(0.01)                       # on top of the pair's netclass clearance
SAME_NET_PAD_CLR = MM(0.10)                 # keep the via off its neighbours' pads
HOLE_CLR = MM(0.27)                         # min_hole_to_hole / min_hole_clearance 0.25 + margin
EDGE_CLR = MM(0.52)                         # min_copper_edge_clearance 0.5 + margin
KEEPOUT_CLR = MM(0.20)
STUB_W = (MM(0.20), MM(0.30))
TIE_MAX = MM(5.0)
BEARINGS = 24
RADII = [MM(0.1 * k) for k in range(3, 26)]  # 0.3 .. 2.5 mm
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


def obstacle(item, kind):
    ob = {"kind": kind, "net": item.GetNetCode(), "clr": CLR_OF(item.GetNetname()),
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
    t = max(0.0, min(L, t - w / 2))
    return V(c.x + dx * t, c.y + dy * t)


def seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    L = dx * dx + dy * dy
    t = 0 if L == 0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / L))
    return math.hypot(px - ax - t * dx, py - ay - t * dy)


def main():
    path = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "gb3.kicad_pcb")
    B = pcbnew.LoadBoard(path)
    global CLR_OF
    pro = os.path.splitext(path)[0] + ".kicad_pro"
    CLR_OF = netclass_clearances(pro if os.path.exists(pro) else os.path.join(HERE, "gb3.kicad_pro"))
    need = lambda pad, ob: max(CLR_OF(pad.GetNetname()), ob["clr"]) + CLR_MARGIN
    conn = B.GetConnectivity()

    plane = {}
    for z in B.Zones():
        if z.GetIsRuleArea(): continue
        for net, lname in PLANES.items():
            if z.GetNetname() == net and z.IsOnLayer(B.GetLayerID(lname)):
                plane[net] = z.m_Uuid.AsString()
    keepouts = [z.GetBoundingBox() for z in B.Zones() if z.GetIsRuleArea()]
    for bb in keepouts: bb.Inflate(VIA_D // 2 + KEEPOUT_CLR)

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
            if p.GetNetname() in plane and not p.HasHole() and (is_thermal(p) or not on_plane(p)):
                todo.append(p)

    def fits(pad, layer, w, x, y, in_pad=False):
        via = pcbnew.SHAPE_CIRCLE(V(x, y), VIA_D // 2)
        hole = pcbnew.SHAPE_CIRCLE(V(x, y), VIA_H // 2)
        c = pad.GetPosition()
        stub = None if in_pad else pcbnew.SHAPE_SEGMENT(stub_start(pad, x, y, w), V(x, y), w)
        if any(bb.Contains(V(x, y)) for bb in keepouts): return "keepout"
        if not ps.Contains(V(x, y)): return "off-board"
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
                if stub is not None and l == layer and sh.Collide(stub, need(pad, ob)): return "stub vs " + ob["kind"]
        return None

    def add_via(pad, x, y):
        v = pcbnew.PCB_VIA(B)
        v.SetViaType(pcbnew.VIATYPE_THROUGH)
        v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        v.SetPosition(V(x, y)); v.SetWidth(VIA_D); v.SetDrill(VIA_H)
        v.SetNet(pad.GetNet()); B.Add(v)
        grid.add(obstacle(v, "via"))

    placed, failed, thermal, thermal_none = [], [], [], []
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
        hit = None
        blockers = defaultdict(int)
        for r in RADII:
            for k in range(BEARINGS):
                a = 2 * math.pi * k / BEARINGS
                x, y = c.x + r * math.cos(a), c.y + r * math.sin(a)
                why = fits(pad, layer, w, x, y)
                if why is None:
                    hit = (x, y, r); break
                blockers[why] += 1
            if hit: break
        if not hit:
            failed.append((pad, ref, max(blockers, key=blockers.get))); continue
        x, y, r = hit
        add_via(pad, x, y)
        t = pcbnew.PCB_TRACK(B)
        t.SetStart(stub_start(pad, x, y, w)); t.SetEnd(V(x, y)); t.SetWidth(w); t.SetLayer(layer)
        t.SetNet(pad.GetNet()); B.Add(t)
        grid.add(obstacle(t, "track"))
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

    tied = []
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
            a = stub_start(pad, q.x, q.y, w)
            if path_clear(pad, layer, a, q, w, it): best = (d, a, q)
        if best:
            d, a, q = best
            t = pcbnew.PCB_TRACK(B)
            t.SetStart(a); t.SetEnd(q); t.SetWidth(w); t.SetLayer(layer); t.SetNet(pad.GetNet()); B.Add(t)
            grid.add(obstacle(t, "track"))
            tied.append((ref, pad.GetNetname(), TO(d)))
            failed.remove(f)
    failed = [(ref, pad.GetNetname(), TO(pad.GetPosition().x), TO(pad.GetPosition().y), why)
              for pad, ref, why in failed if not on_plane(pad)]

    pcbnew.ZONE_FILLER(B).Fill(B.Zones())
    pcbnew.SaveBoard(path, B)

    by = defaultdict(int)
    for _, n, _ in placed: by[n] += 1
    sys.stderr.write("fanout_planes: %d plane pads off-plane -> %d stub vias (%s), %d thermal pads "
                     "(%d vias), %d joined via a neighbour, %d tied to a nearby plane item, %d no fit\n"
                     % (len(todo), len(placed), ", ".join("%s %d" % kv for kv in sorted(by.items())),
                        len(thermal), sum(n for *_, n in thermal), len(joined), len(tied), len(failed)))
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
