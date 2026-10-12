"""maze.py — a small octilinear maze router for gb2's dense local regions (used from route group files).

The group decides *what* connects, on which layers, through which waypoints and channels; the maze finds the exact
clean path: 8 directions only (0/45/90 deg), a cost for every bend (no 135 deg turns at all), a via cost, and a
per-cell cost map so a route prefers its channel. It sees the board's real copper: every pad, track, via, keep-out
and the board edge, each inflated by the pair's clearance plus the new track's half width, so its output is DRC-clean
by construction (DRC still checks it). Routed copper is added to the obstacle map as it goes, so later connections
in the same session route around earlier ones.

    m = Maze(B, (x0, y0, x1, y1))                 # window, board mm
    pts = m.route(net, (x, y, "F"), (x, y, "B"), w=0.15, layers="FB", cost=None)
        -> [(x, y, layer), ...] corner points (a layer change = a via at that point), or None
    m.commit(net, pts, w)                         # mark it as an obstacle for the next routes

Resolution 0.05 mm; clearance from route/../gb2.kicad_pro net classes (Default 0.15 / Power 0.2).
"""
import pcbnew, math, heapq, json, os, fnmatch

TO = pcbnew.ToMM
RES = 0.05
MARGIN = 0.03            # extra clearance for the cell-centre approximation
VIA_D, VIA_H = 0.45, 0.25
DIRS = [(1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1), (0, -1), (1, -1)]
HERE = os.path.dirname(os.path.abspath(__file__))


def _classes():
    ns = json.load(open(os.path.join(HERE, "..", "gb2.kicad_pro")))["net_settings"]
    clr = {c["name"]: c["clearance"] for c in ns["classes"]}
    pats = [(p["pattern"], p["netclass"]) for p in ns.get("netclass_patterns", [])]
    def of(n):
        for pat, cls in pats:
            if fnmatch.fnmatchcase(n, pat): return clr[cls]
        return clr["Default"]
    return of


class Maze:
    def __init__(self, B, box, layers=("F", "B")):
        self.B = B; self.x0, self.y0, self.x1, self.y1 = box
        self.nx = int((self.x1 - self.x0) / RES) + 1; self.ny = int((self.y1 - self.y0) / RES) + 1
        self.layers = layers; self.clr = _classes()
        self.LID = {"F": pcbnew.F_Cu, "B": pcbnew.B_Cu}
        # per layer: the copper that blocks - list of (net, kind, geom, clearance) - rasterised lazily per width
        self.items = {L: [] for L in layers}; self.holes = []; self.pads = []
        self.edge = []
        self._collect()
        self._cache = {}; self._nets = {}; self._vcache = {}

    # ---- geometry
    def _collect(self):
        B = self.B; pad = 2.0
        inwin = lambda bx0, by0, bx1, by1: not (bx1 < self.x0 - pad or bx0 > self.x1 + pad or by1 < self.y0 - pad or by0 > self.y1 + pad)
        for fp in B.GetFootprints():
            for p in fp.Pads():
                bb = p.GetBoundingBox(); b = (TO(bb.GetLeft()), TO(bb.GetTop()), TO(bb.GetRight()), TO(bb.GetBottom()))
                if not inwin(*b): continue
                net = p.GetNetname() or "<pad:%s.%s>" % (fp.GetReference(), p.GetNumber())
                c = self.clr(net)
                circ = p.GetShape() == pcbnew.PAD_SHAPE_CIRCLE
                g = ("circle", ((b[0] + b[2]) / 2, (b[1] + b[3]) / 2, (b[2] - b[0]) / 2)) if circ else ("box", b)
                for L in self.layers:
                    if p.IsOnLayer(self.LID[L]): self.items[L].append((net, g, c))
                self.pads.append(g)
                if p.HasHole():
                    self.holes.append(((b[0] + b[2]) / 2, (b[1] + b[3]) / 2, TO(p.GetDrillSize().x) / 2))
        for t in B.GetTracks():
            if t.Type() == pcbnew.PCB_VIA_T:
                q = t.GetPosition(); x, y = TO(q.x), TO(q.y); r = TO(t.GetWidth(pcbnew.F_Cu)) / 2
                if not inwin(x - r, y - r, x + r, y + r): continue
                for L in self.layers: self.items[L].append((t.GetNetname(), ("circle", (x, y, r)), self.clr(t.GetNetname())))
                self.holes.append((x, y, TO(t.GetDrill()) / 2))
            else:
                a, b = t.GetStart(), t.GetEnd(); ax, ay, bx, by = TO(a.x), TO(a.y), TO(b.x), TO(b.y)
                if not inwin(min(ax, bx), min(ay, by), max(ax, bx), max(ay, by)): continue
                for L in self.layers:
                    if t.GetLayer() == self.LID[L]:
                        self.items[L].append((t.GetNetname(), ("seg", (ax, ay, bx, by, TO(t.GetWidth()) / 2)), self.clr(t.GetNetname())))
        for z in B.Zones():
            if not z.GetIsRuleArea(): continue
            bb = z.GetBoundingBox()
            if not inwin(TO(bb.GetLeft()), TO(bb.GetTop()), TO(bb.GetRight()), TO(bb.GetBottom())): continue
            for L in self.layers: self.items[L].append(("<keepout>", ("zone", z), 0.0))
        ps = pcbnew.SHAPE_POLY_SET(); B.GetBoardPolygonOutlines(ps, True); self.board = ps

    @staticmethod
    def _dist(g, x, y):
        k, d = g
        if k == "box":
            x0, y0, x1, y1 = d; dx = max(x0 - x, 0, x - x1); dy = max(y0 - y, 0, y - y1); return math.hypot(dx, dy)
        if k == "circle":
            cx, cy, r = d; return max(0.0, math.hypot(x - cx, y - cy) - r)
        if k == "seg":
            ax, ay, bx, by, hw = d; dx, dy = bx - ax, by - ay; L = dx * dx + dy * dy
            t = 0 if L == 0 else max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / L))
            return max(0.0, math.hypot(x - ax - t * dx, y - ay - t * dy) - hw)
        if k == "zone":
            z = d; v = pcbnew.VECTOR2I(int(pcbnew.FromMM(x)), int(pcbnew.FromMM(y)))
            return 0.0 if z.Outline().Contains(v) else TO(int(math.sqrt(z.Outline().SquaredDistance(v))))
        return 1e9

    def _edges(self):
        if hasattr(self, "_edge_segs"): return self._edge_segs
        segs = []
        for k in range(self.board.OutlineCount()):
            chains = [self.board.COutline(k)] + [self.board.CHole(k, h) for h in range(self.board.HoleCount(k))]
            for ch in chains:
                pts = [(TO(ch.CPoint(i).x), TO(ch.CPoint(i).y)) for i in range(ch.PointCount())]
                for (ax, ay), (bx, by) in zip(pts, pts[1:] + pts[:1]):
                    if max(ax, bx) < self.x0 - 1 or min(ax, bx) > self.x1 + 1 or max(ay, by) < self.y0 - 1 or min(ay, by) > self.y1 + 1: continue
                    segs.append((ax, ay, bx, by, 0.0))
        self._edge_segs = segs; return segs

    def _paint(self, g, n, geom, c, half):
        MULTI = 65535; idx = self.net_ix(n)
        reach = max(0.15, c) + half + MARGIN
        k, d = geom
        if k == "box": bx0, by0, bx1, by1 = d
        elif k == "circle": bx0, by0, bx1, by1 = d[0] - d[2], d[1] - d[2], d[0] + d[2], d[1] + d[2]
        elif k == "seg": bx0, by0, bx1, by1 = min(d[0], d[2]) - d[4], min(d[1], d[3]) - d[4], max(d[0], d[2]) + d[4], max(d[1], d[3]) + d[4]
        else:
            bb = d.GetBoundingBox(); bx0, by0, bx1, by1 = TO(bb.GetLeft()), TO(bb.GetTop()), TO(bb.GetRight()), TO(bb.GetBottom())
        i0 = max(0, int((bx0 - reach - self.x0) / RES)); i1 = min(self.nx - 1, int((bx1 + reach - self.x0) / RES) + 1)
        j0 = max(0, int((by0 - reach - self.y0) / RES)); j1 = min(self.ny - 1, int((by1 + reach - self.y0) / RES) + 1)
        dist = self._dist
        for j in range(j0, j1 + 1):
            y = self.y0 + j * RES; row = j * self.nx
            for i in range(i0, i1 + 1):
                o = g[row + i]
                if o == MULTI or o == idx: continue
                if dist(geom, self.x0 + i * RES, y) < reach:
                    g[row + i] = idx if o == 0 else MULTI

    def _owner(self, L, half):
        """array: 0 = free for a centre line of half-width `half`; else the index of the one net whose copper is
        within clearance (that net may pass); MULTI = two or more nets, or the board edge (nobody passes)."""
        key = (L, round(half, 3))
        if key in self._cache: return self._cache[key]
        import array
        g = array.array("H", [0]) * (self.nx * self.ny)
        MULTI = 65535
        for n, geom, c in self.items[L]:
            self._paint(g, n, geom, c, half)
        # board edge: cells within 0.5 mm (+ half width) of any outline / hole edge, and every cell outside the board
        e = 0.5 + half + MARGIN
        segs = self._edges()
        for sg in segs:
            ax, ay, bx, by, _ = sg
            i0 = max(0, int((min(ax, bx) - e - self.x0) / RES)); i1 = min(self.nx - 1, int((max(ax, bx) + e - self.x0) / RES) + 1)
            j0 = max(0, int((min(ay, by) - e - self.y0) / RES)); j1 = min(self.ny - 1, int((max(ay, by) + e - self.y0) / RES) + 1)
            for j in range(j0, j1 + 1):
                y = self.y0 + j * RES; row = j * self.nx
                for i in range(i0, i1 + 1):
                    if g[row + i] != MULTI and self._dist(("seg", sg), self.x0 + i * RES, y) < e: g[row + i] = MULTI
        allsegs = self._all_edges()
        for j in range(self.ny):                       # scanline even-odd: outside cells blocked
            y = self.y0 + j * RES + 1e-6; row = j * self.nx
            xs = sorted(ax + (y - ay) * (bx - ax) / (by - ay) for ax, ay, bx, by in allsegs if (ay > y) != (by > y))
            inside = [(xs[k], xs[k + 1]) for k in range(0, len(xs) - 1, 2)]
            for i in range(self.nx):
                x = self.x0 + i * RES
                if not any(lo <= x <= hi for lo, hi in inside): g[row + i] = MULTI
        self._cache[key] = g
        return g

    def _all_edges(self):
        if hasattr(self, "_all"): return self._all
        out = []
        for k in range(self.board.OutlineCount()):
            for ch in [self.board.COutline(k)] + [self.board.CHole(k, h) for h in range(self.board.HoleCount(k))]:
                pts = [(TO(ch.CPoint(i).x), TO(ch.CPoint(i).y)) for i in range(ch.PointCount())]
                out += [(ax, ay, bx, by) for (ax, ay), (bx, by) in zip(pts, pts[1:] + pts[:1])]
        self._all = out; return out

    def net_ix(self, n):
        if n not in self._nets: self._nets[n] = len(self._nets) + 1
        return self._nets[n]

    def _blocked(self, L, net, half):
        o = self._owner(L, half); me = self.net_ix(net)
        return lambda idx: o[idx] != 0 and o[idx] != me

    def _via_ok(self, net, i, j):
        key = (net, i, j)
        if key in self._vcache: return self._vcache[key]
        r = self._via_ok_(net, i, j); self._vcache[key] = r; return r

    def _via_ok_(self, net, i, j):
        x, y = self.x0 + i * RES, self.y0 + j * RES
        for L in self.layers:
            if self._blocked(L, net, VIA_D / 2)(j * self.nx + i): return False
        for hx, hy, hr in self.holes:
            if math.hypot(hx - x, hy - y) - hr - VIA_H / 2 < 0.25 + MARGIN: return False
        for g in self.pads:                     # no via-in-pad, not even in the route's own pads
            if self._dist(g, x, y) < VIA_D / 2 + 0.05: return False
        return True

    # ---- search
    def route(self, net, a, b, w=0.15, layers="FB", cost=None, via_cost=60.0, bend=2.0, bend90=6.0):
        """a, b = (x, y, layer). Returns corner points [(x, y, layer)], layer changes = vias."""
        half = w / 2
        grids = {L: self._blocked(L, net, half) for L in layers}
        cell = lambda x, y: (int(round((x - self.x0) / RES)), int(round((y - self.y0) / RES)))
        (si, sj), (ti, tj) = cell(a[0], a[1]), cell(b[0], b[1])
        sL, tL = a[2], b[2]
        def free(L, i, j, endpoint):
            if not (0 <= i < self.nx and 0 <= j < self.ny): return False
            if endpoint: return True               # inside its own pad, whatever the neighbours' halos say
            return not grids[L](j * self.nx + i)
        near_end = lambda i, j, L: (abs(i - si) <= 3 and abs(j - sj) <= 3 and L == sL) or (abs(i - ti) <= 3 and abs(j - tj) <= 3 and L == tL)
        h = lambda i, j: (max(abs(i - ti), abs(j - tj)) + 0.414 * min(abs(i - ti), abs(j - tj)))
        if cost:
            _cc = {}; _raw = cost
            def cost(x, y, L, _cc=_cc, _raw=_raw):
                k = (round(x, 3), round(y, 3), L)
                if k not in _cc: _cc[k] = _raw(x, y, L)
                return _cc[k]
        start = (si, sj, sL, -1)
        openq = [(h(si, sj), 0.0, start)]; g = {start: 0.0}; prev = {}
        goal = None; n = 0
        while openq:
            f, gc, s = heapq.heappop(openq)
            if gc > g.get(s, 1e18): continue
            i, j, L, d = s
            if (i, j, L) == (ti, tj, tL): goal = s; break
            n += 1
            if n > 1500000: break
            for k, (dx, dy) in enumerate(DIRS):
                if d >= 0:
                    turn = min((k - d) % 8, (d - k) % 8)
                    if turn > 2: continue            # never sharper than 90 deg
                    tc = 0 if turn == 0 else (bend if turn == 1 else bend90)
                else: tc = 0
                ni, nj = i + dx, j + dy
                if not free(L, ni, nj, near_end(ni, nj, L)): continue
                if dx and dy and not (free(L, i + dx, j, near_end(i + dx, j, L)) and free(L, i, j + dy, near_end(i, j + dy, L))): continue
                step = 1.414 if dx and dy else 1.0
                if cost: step *= cost(self.x0 + ni * RES, self.y0 + nj * RES, L)
                ns = (ni, nj, L, k); ng = gc + step + tc
                if ng < g.get(ns, 1e18):
                    g[ns] = ng; prev[ns] = s; heapq.heappush(openq, (ng + 1.3 * h(ni, nj), ng, ns))
            if len(layers) > 1:
                for L2 in layers:
                    if L2 == L or not self._via_ok(net, i, j): continue
                    ns = (i, j, L2, d); ng = gc + via_cost
                    if ng < g.get(ns, 1e18):
                        g[ns] = ng; prev[ns] = s; heapq.heappush(openq, (ng + 1.3 * h(i, j), ng, ns))
        if goal is None: return None
        path = [goal]
        while path[-1] in prev: path.append(prev[path[-1]])
        path.reverse()
        pts = [(self.x0 + p[0] * RES, self.y0 + p[1] * RES, p[2]) for p in path]
        # corner points only
        out = [pts[0]]
        for p, q, r in zip(pts, pts[1:], pts[2:]):
            if q[2] != p[2] or q[2] != r[2]: out.append(q); continue
            if (round(q[0] - p[0], 4), round(q[1] - p[1], 4)) != (round(r[0] - q[0], 4), round(r[1] - q[1], 4)): out.append(q)
        out.append(pts[-1])
        return _snap_ends(out, a, b)

    def commit(self, net, pts, w):
        new = []
        for p, q in zip(pts, pts[1:]):
            if p[2] == q[2]:
                new.append((p[2], (net, ("seg", (p[0], p[1], q[0], q[1], w / 2)), self.clr(net))))
            else:
                for L in self.layers: new.append((L, (net, ("circle", (q[0], q[1], VIA_D / 2)), self.clr(net))))
                self.holes.append((q[0], q[1], VIA_H / 2))
        self._vcache.clear()
        for L, it in new:
            self.items[L].append(it)
            for (cl, half), g in self._cache.items():  # paint into every cached raster instead of rebuilding
                if cl == L: self._paint(g, it[0], it[1], it[2], half)


def _octi(dx, dy):
    """nearest of the 8 unit directions."""
    k = round(math.atan2(dy, dx) / (math.pi / 4)) % 8
    ux, uy = DIRS[k]; n = math.hypot(ux, uy); return ux / n, uy / n


def _meet(p, d, q, e):
    """intersection of line p + t d with line q + s e (None if parallel)."""
    den = d[0] * e[1] - d[1] * e[0]
    if abs(den) < 1e-9: return None
    t = ((q[0] - p[0]) * e[1] - (q[1] - p[1]) * e[0]) / den
    return (p[0] + t * d[0], p[1] + t * d[1])


def _snap_ends(out, a, b):
    """make the path start / end exactly on the pad centres a / b while every segment stays on the 45 deg grid:
    each end run is shifted (keeping its direction) to pass through its pad centre and re-met with its neighbour."""
    pts = [list(p) for p in out]
    for end, tgt in ((0, a), (-1, b)):
        i0, i1 = (0, 1) if end == 0 else (-1, -2)
        if len(pts) < 2 or pts[i0][2] != pts[i1][2]: pts[i0][0], pts[i0][1] = tgt[0], tgt[1]; continue
        d = _octi(pts[i1][0] - pts[i0][0], pts[i1][1] - pts[i0][1])
        step = 1 if end == 0 else -1
        n = len(pts)
        ok = lambda k: -n <= k < n
        k2 = i1 + step                                   # the point after the end run's corner
        via = ok(k2) and pts[k2][2] != pts[i1][2] and abs(pts[k2][0] - pts[i1][0]) < 1e-6 and abs(pts[k2][1] - pts[i1][1]) < 1e-6
        if via:                                          # end run -> via -> segment on the other layer
            k3 = k2 + step
            if ok(k3) and pts[k3][2] == pts[k2][2]:
                e = _octi(pts[k3][0] - pts[k2][0], pts[k3][1] - pts[k2][1])
                c = _meet((tgt[0], tgt[1]), d, (pts[k3][0], pts[k3][1]), e)
                if c is not None and math.hypot(c[0] - pts[i1][0], c[1] - pts[i1][1]) < 0.2:
                    pts[i1][0], pts[i1][1] = c; pts[k2][0], pts[k2][1] = c
        elif ok(k2) and pts[k2][2] == pts[i1][2]:
            e = _octi(pts[k2][0] - pts[i1][0], pts[k2][1] - pts[i1][1])
            c = _meet((tgt[0], tgt[1]), d, (pts[k2][0], pts[k2][1]), e)
            if c is not None and math.hypot(c[0] - pts[i1][0], c[1] - pts[i1][1]) < 0.2:
                pts[i1][0], pts[i1][1] = c
        pts[i0][0], pts[i0][1] = tgt[0], tgt[1]
    return [tuple(p) for p in pts]
