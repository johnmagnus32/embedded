#!/usr/bin/env python3
"""checks.py - the gb3 enclosure's checks, on enclosure.build()'s finished parts and ctx (CONTRACT.md).

  (a) interference and clearances
      (a1) every part (the finished shells + every module part) vs every other part and every context body: the
           board, the 193 part envelopes (z_design), USB-C / jack / microSD (+ the latched card), both FPC1s, the
           panel layers, both flexes, the cell AND 'cell (swollen)' (only what the swell adds is reported), the SUS
           plate, the LRA, the speaker + its foam
      (a2) context vs context, every pair (LCM vs P1 and the F decaps, the flexes vs the parts they pass, the corrected
           FPC1 vs its neighbours ...). ALLOWED lists the intended overlaps (B:R29 x FPC1 corrected: the pending board
           fix), ALTERNATIVES the bodies that never coexist (FPC1 as placed vs the corrected one and its tail)
      (a3) the clearances the stack is built on (each derived from params): the panel in its pocket (tape / gasket),
           the touch flex under the fold skin (the glass's max offset), the swollen cell under the LCM (CELL_AIR; or
           the SUS-plate stack), the LCM over P1 + every F part (P1_AIR), the cell on its floor / in its fences, the
           board edge beside the shells (BOARD_WALL_MIN), the front shell off the board except at the boss tips
           (BOARD_FACE_MIN), the back shell under it (BACK_BOSS_GAP), every body's vertical gap to the shell facing
           it, measured off its own surface (F up to the front shell >= CLR_F, B down to the back side >= CLR_B; ray
           scan inside the walls), the corrected FPC1's nearest neighbours (info)
  (b) keep-outs: the finished shells vs every core keep-out; every module's adds and parts vs every OTHER module's
      keep-outs and the core ones (KEEPOUT_ALLOWED). Shell material inside a module's own keep-out is printed as info
      (the owner decides what of the shell stays there)
  (c) rest gaps (REST_RANGE 0.10-0.20, the brief): face-key and D-pad stem tips over the TS-1187A keystakes (also vs
      the keystake's max height), the D-pad pivot on the board top, the SKRT edge caps' and shoulder bumpers' contact
      faces to the plunger tips along the press axis (controls found by position, a part named after the key first);
      the board datum: front boss tips at SPLIT round an open insert hole, back boss tops BACK_BOSS_GAP under the
      board, the seam-screw bosses meeting at SPLIT; every M2 screw (head from the back face + shank) clear
  (d) exactly one valid solid per part; printed parts on the bed in their print pose with their volume kept; bought
      parts (print_pose None) listed; the shells print on their outer face
  (e) thickness, by ray scans through the tessellated parts: front plate >= PLATE (near CN1 >= PLATE_CN1, the lip
      >= LIP), the fold-bay skin <= FOLD_BAY skin_max and >= the printable minimum, the ledge (flush: top at LEDGE_Z1,
      >= LEDGE_T, along its three segments; lip: none needed) and nothing else of the front shell under the LCM, side
      walls >= WALL (in an opening zone, the split band, or thinned by a module's cut - a relief, scallop, snap pocket
      - at the printable minimum), printed floor >= FLOOR_PR, the FR4 plate FLOOR_FR4 thick and flush; then a slab
      scan of every printed part along x, y and z (no slab under MIN_SLAB[PROCESS]; --fast skips it)
  (f) the per-region thickness table: params.REGIONS measured from the finished parts (face / glass top to the back
      face, controls left out) next to params' needs and model and the research budget; FAIL if a region is thicker
      than the architecture total (params.TOTAL, cross-checked with the budget's architectures), thinner than its
      contents need, or if any part but the controls leaves the slab (Z_FACE, params.back_at(), the outline)
  (h) assembly path, CONTRACT.md's order: the controls up into the face-down front shell; the module down from the
      face (lip: up from inside); the board with every part on it up onto the boss tips; the back shell with the
      battery plate, cell, LRA and speaker up from below. Each body slides along z into its rest pose past what is
      already in place: blocked where material lies in its path (ray scan). Keep-outs named SWEEP_FLEX ("snap")
      mark elastic snap blades; SWEEP_ALLOWED lists intended contacts (--fast skips (h))
  (j) press travel: every face key pushed to the TS-1187A hard stop (PRESS_MARGIN short); the D-pad rocked toward 8
      directions: a small-angle solve (z rays over it: every gap above and below it bounds the tilt) finds a tilt that
      takes the pressed legs to the stop with nothing touched, or reports it jammed with the extra pivot / flange gap
      it needs; the motion found is then checked on the solid (with the cross's lateral play, ROCK_SHIFTS); every
      edge cap / shoulder bumper pushed through the SKRT travel
  (x) cross-module cuts: no module's cut removes another module's added material

Run (from this folder):  .venv/bin/python checks.py [--fast] [--only a,c,...] [--quiet] [--maps]
  builds (enclosure.build()), prints every section, then one PASS / FAIL line per check; exit 1 on any FAIL.
  --maps also writes build/checks/thickness_map.png and plate_floor_map.png (what the ray scans measured).
  About 20 s after the build; --fast skips (h) and (e)'s slab scan (about 15 s).
API: run(parts, ctx, fast=False, only=None, verbose=True) -> [(check, "PASS" | "FAIL", detail)] (CONTRACT.md);
  LAST[letter] holds each check's full list of failures; reset() drops the per-shape caches.
Tests: tests/checks/test_checks.py (synthetic modules with planted faults), tests/checks/run_variants.py (every
  GB3_* flag in its own process).
"""
import json, math, os, sys, time, traceback
import numpy as np
import params as P

# ================================================================ thresholds (published; cite them, don't copy them)
EPS_V = 0.001            # mm3: overlaps above this are reported (touching faces leave ~1e-9 slivers)
TOL = 0.005              # mm: an exact target (rest gap, datum, boss tip) counts as met within this
TOL_T = 0.02             # mm: a thickness / face reading off the tessellated parts (deflection MESH_TOL)
REST_RANGE = (0.10, 0.20)   # mm: the brief - caps rest 0.1-0.2 above the actuator (keystake top, plunger tip)
# printable minimum slab per process: (xy = a vertical wall, z = a horizontal skin). MJF: Protolabs MJF min wall 0.5
# (thin report); FDM 0.4 nozzle: 2 perimeters 0.86 (thin report: ribs 0.86-1.26), 3 layers of 0.2 for a skin
MIN_SLAB = {"mjf": {"xy": 0.5, "z": 0.5}, "fdm": {"xy": 0.86, "z": 0.6}}
SLAB_DEG = 40.0          # (e) scan: entry and exit faces within this of the ray count as a slab (else a wedge / graze)
BOARD_WALL_MIN = 0.3     # (a3) board edge to the shells, sideways (JLC3DP MJF assembly clearance 0.2-0.4)
BOARD_FACE_MIN = 0.1     # (a3) the front shell over the board's top face off the boss tips (the datum): no contact
CN1_ZONE = 1.0           # (e) the plate within this of CN1's hull may be the thinned PLATE_CN1
SPLIT_BAND = 1.0         # (e) walls within this of SPLIT may be stepped (lap / tongue): judged at MIN_SLAB only
OPENING_MARGIN = 1.5     # (e) walls within this of an opening keep-out / notch: judged at MIN_SLAB only
MESH_TOL, MESH_ANG = 0.01, 0.15   # tessellation for the ray scans (mm, rad)
GRID = 0.5               # (e) / (a3) targeted ray grids (mm)
SCAN = 0.4               # (e) slab-scan ray spacing for the shells (small parts: finer)
REGION_GRID = 1.0        # (f) ray spacing per region
PRESS_MARGIN = 0.02      # (j) stop this short of the hard stop (the stop itself is contact)
ROCK_GRID = 0.25         # (j) z-ray spacing over the D-pad for its rocking solve
ROCK_SHIFTS = (0.0, -0.05, 0.05, -0.1, 0.1, -0.2, 0.2)   # (j) lateral shifts tried along the press (the cross's play)
SWEEP_GRID = 0.3         # (h) z-ray spacing for the assembly sweeps
SWEEP_FLEX = ("snap",)   # (h) keep-outs named with these mark elastic features (a snap blade flexes past)
BUDGET_JSON = os.environ.get("GB3_BUDGET_JSON",
                             os.path.expanduser("~/.cache/gb3-encl/synth/stack.json"))   # research budget (optional)

# ================================================================ allow-lists (each with its reason)
# (a) overlaps that are intended or known (sorted name pair -> reason)
ALLOWED = {} if P.FPC1_FIXED else {
    ("B:R29", "FPC1 corrected"): "the pending pre-fab board fix (move R29; CONTRACT.md)",
}
# (a) bodies that never coexist: FPC1 as placed is the board as drawn; the corrected FPC1 and the touch tail that
# reaches it are the fixed board
ALTERNATIVES = {} if P.FPC1_FIXED else {"FPC1 as placed": {"FPC1 corrected", "touch flex"}}
# (b) keep-out x material overlaps that are intended: (keep-out key, material label) -> reason
KEEPOUT_ALLOWED = {}
# (h) assembly-sweep contacts that are intended: (mover, obstacle) -> reason
SWEEP_ALLOWED = {}
# (e) slab-scan thin spots that are intended: (part, (x, y, z), radius, reason)
THIN_ALLOWED = []

VERBOSE = True
LAST = {}                # check letter -> its full list of failures (the tests read it)


def say(*a):
    if VERBOSE:
        print(*a)


def _k(x, y):
    """enclosure (x, y) -> a KiCad label."""
    return f"KiCad ({x:.2f}, {-y:.2f})"


# ================================================================ geometry helpers
_BB = {}


def bb(s):
    """(lo, hi) numpy bounding box, cached per shape object."""
    k = id(s)
    if k not in _BB:
        b = s.bounding_box()
        _BB[k] = (np.array(tuple(b.min)), np.array(tuple(b.max)), s)
    return _BB[k][0], _BB[k][1]


def _bb_hit(a, b, pad=0.0):
    (alo, ahi), (blo, bhi) = bb(a), bb(b)
    return bool(np.all(alo - pad <= bhi) and np.all(blo - pad <= ahi))


def _bb_ub(a, b):
    """upper bound of the overlap volume from the bounding boxes."""
    (alo, ahi), (blo, bhi) = bb(a), bb(b)
    return float(np.prod(np.clip(np.minimum(ahi, bhi) - np.maximum(alo, blo), 0, None)))


def _bb_gap(a, b):
    """lower bound of the distance from the bounding boxes."""
    (alo, ahi), (blo, bhi) = bb(a), bb(b)
    g = np.maximum(np.maximum(blo - ahi, alo - bhi), 0)
    return float(np.sqrt((g * g).sum()))


def vol(a, b):
    """intersection volume (0 when the bounding boxes cannot hold EPS_V; nan if the boolean fails)."""
    if _bb_ub(a, b) <= EPS_V:
        return 0.0
    try:
        r = a & b
    except Exception:
        return float("nan")
    return sum(s.volume for s in r.solids()) if r is not None else 0.0


def overlaps(shape, bodies):
    """{name: volume} of bodies {name: Shape} that overlap shape by more than EPS_V: one grouped common (shape vs all
    the candidate bodies at once) picks them, an exact common per candidate measures them."""
    from build123d import Compound
    from build123d.topology.shape_core import List_TopoDS_Shape
    from OCP.BRepAlgoAPI import BRepAlgoAPI_Common
    near = {n: s for n, s in bodies.items() if _bb_ub(shape, s) > EPS_V}
    if not near:
        return {}
    a, t = List_TopoDS_Shape(), List_TopoDS_Shape()
    a.Append(shape.wrapped)
    for s in near.values():
        t.Append(s.wrapped)
    op = BRepAlgoAPI_Common()
    op.SetArguments(a)
    op.SetTools(t)
    op.SetRunParallel(True)
    op.Build()
    pieces = Compound(op.Shape()).solids() if op.IsDone() else None
    if pieces is None:
        cands = list(near)
    else:
        cands = {n for p in pieces for n, s in near.items() if _bb_hit(p, s, 1e-6)}
    out = {}
    for n in cands:
        v = vol(shape, near[n])
        if v > EPS_V or v != v:
            out[n] = v
    return out


def gap(a, b, need, margin=0.5):
    """(distance, exact): the exact distance when it may be under need + margin (the bigger shape cropped round the
    smaller first), else a lower bound >= need + margin from the bounding boxes."""
    from build123d import Box, Pos
    lb = _bb_gap(a, b)
    if lb >= need + margin:
        return lb, False
    A, B = (a, b) if np.prod(bb(a)[1] - bb(a)[0]) >= np.prod(bb(b)[1] - bb(b)[0]) else (b, a)
    lo, hi = bb(B)
    g = need + margin
    c, sz = (lo + hi) / 2, hi - lo + 2 * g
    try:
        crop = A & (Pos(*c) * Box(*sz))
    except Exception:
        crop = A
    if crop is None or not crop.solids():
        return g, False
    return crop.distance_to(B), True


def _cyl(x, y, d, z0, z1):
    return P.cyl(x, y, d, z0, z1)


def _box3(lo, hi):
    from build123d import Box, Pos
    lo, hi = np.asarray(lo, float), np.asarray(hi, float)
    return Pos(*((lo + hi) / 2)) * Box(*(hi - lo))


def _lowest(part, x, y, d, z0, z1):
    """lowest z of part inside the column of diameter d round (x, y), z0..z1 (None if empty)."""
    try:
        c = part & _cyl(x, y, d, z0, z1)
    except Exception:
        return None
    if c is None or not c.solids():
        return None
    return c.bounding_box().min.Z


def _highest(part, x, y, d, z0, z1):
    try:
        c = part & _cyl(x, y, d, z0, z1)
    except Exception:
        return None
    if c is None or not c.solids():
        return None
    return c.bounding_box().max.Z


# ================================================================ ray engine (axis-aligned rays through a mesh)
_AX = {0: (1, 2), 1: (0, 2), 2: (0, 1)}      # ray axis -> the two in-plane axes (increasing order)


def _triangles(shape, tol=MESH_TOL, ang=MESH_ANG):
    """(m, 3, 3) triangles of a fresh tessellation of a copy of shape, wound outward."""
    from OCP.BRepMesh import BRepMesh_IncrementalMesh
    from OCP.BRepBuilderAPI import BRepBuilderAPI_Copy
    from OCP.BRep import BRep_Tool
    from OCP.TopLoc import TopLoc_Location
    from OCP.TopExp import TopExp_Explorer
    from OCP.TopAbs import TopAbs_FACE, TopAbs_REVERSED
    from OCP.TopoDS import TopoDS
    w = BRepBuilderAPI_Copy(shape.wrapped, True, False).Shape()
    BRepMesh_IncrementalMesh(w, tol, False, ang, False)
    out = []
    ex = TopExp_Explorer(w, TopAbs_FACE)
    while ex.More():
        f = TopoDS.Face_s(ex.Current()) if hasattr(TopoDS, "Face_s") else TopoDS.Face(ex.Current())
        loc = TopLoc_Location()
        tri = BRep_Tool.Triangulation_s(f, loc)
        if tri is not None:
            tr = loc.Transformation()
            n, m = tri.NbNodes(), tri.NbTriangles()
            V = np.empty((n, 3))
            for i in range(1, n + 1):
                q = tri.Node(i).Transformed(tr)
                V[i - 1] = (q.X(), q.Y(), q.Z())
            T = np.array([tri.Triangle(i).Get() for i in range(1, m + 1)], np.int64).reshape(-1, 3) - 1
            if f.Orientation() == TopAbs_REVERSED:
                T = T[:, [0, 2, 1]]
            out.append(V[T])
        ex.Next()
    return np.concatenate(out) if out else np.zeros((0, 3, 3))


class RayMesh:
    """axis-aligned ray casting through a closed, outward-wound triangle mesh: triangles binned on a uniform grid
    in the plane across the ray axis, a vectorised 2D barycentric test per (ray, candidate) pair; per ray the hits
    must alternate entry / exit (else the ray is dropped and counted)."""

    def __init__(self, tri):
        self.tri = tri
        n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]) if len(tri) else np.zeros((0, 3))
        L = np.linalg.norm(n, axis=1) if len(tri) else np.zeros(0)
        self.n = n / (L[:, None] + 1e-300)
        flat = tri.reshape(-1, 3)
        self.lo = flat.min(0) if len(flat) else np.zeros(3)
        self.hi = flat.max(0) if len(flat) else np.zeros(3)
        self._bins = {}

    def _bin(self, axis, h):
        key = (axis, h)
        if key in self._bins:
            return self._bins[key]
        a, b = _AX[axis]
        keep = np.nonzero(np.abs(self.n[:, axis]) > 1e-6)[0]        # faces along the ray: a graze, not a hit
        Pt = self.tri[keep]
        a0, b0 = self.lo[a] - h, self.lo[b] - h
        na = int(math.ceil((self.hi[a] - a0) / h)) + 2
        nb = int(math.ceil((self.hi[b] - b0) / h)) + 2
        i0 = np.floor((Pt[:, :, a].min(1) - a0) / h).astype(np.int64)
        i1 = np.floor((Pt[:, :, a].max(1) - a0) / h).astype(np.int64)
        j0 = np.floor((Pt[:, :, b].min(1) - b0) / h).astype(np.int64)
        j1 = np.floor((Pt[:, :, b].max(1) - b0) / h).astype(np.int64)
        cj = j1 - j0 + 1
        cnt = (i1 - i0 + 1) * cj
        tid = np.repeat(np.arange(len(keep)), cnt)
        k = np.arange(cnt.sum()) - np.repeat(np.cumsum(cnt) - cnt, cnt)
        cell = (i0[tid] + k // cj[tid]) * nb + (j0[tid] + k % cj[tid])
        o = np.argsort(cell, kind="stable")
        cell, tid = cell[o], tid[o]
        start = np.searchsorted(cell, np.arange(na * nb + 1))
        A, B, C = Pt[:, 0], Pt[:, 1], Pt[:, 2]
        d = (B[:, b] - C[:, b]) * (A[:, a] - C[:, a]) + (C[:, a] - B[:, a]) * (A[:, b] - C[:, b])
        rec = dict(tid=tid, start=start, a0=a0, b0=b0, na=na, nb=nb, h=h, P=Pt, d=d,
                   sgn=np.sign(self.n[keep, axis]), cos=np.abs(self.n[keep, axis]))
        self._bins[key] = rec
        return rec

    def cast(self, axis, pa, pb, chunk=3_000_000):
        """rays along +axis through the points (pa, pb) (the other two coordinates, increasing axis order).
        -> dict(r, t0, t1, c0, c1, n, bad): material intervals [t0, t1] along ray r (sorted by ray, then t), the
        entry / exit |cos| to the ray, the ray count and the dropped rays."""
        pa, pb = np.asarray(pa, float).ravel(), np.asarray(pb, float).ravel()
        n = len(pa)
        empty = dict(r=np.zeros(0, np.int64), t0=np.zeros(0), t1=np.zeros(0), c0=np.zeros(0), c1=np.zeros(0), n=n,
                     bad=np.zeros(0, np.int64))
        if n == 0 or len(self.tri) == 0:
            return empty
        a, b = _AX[axis]
        ext = (self.hi[a] - self.lo[a]) * (self.hi[b] - self.lo[b])
        h = float(np.clip(math.sqrt(max(ext, 1e-6) / max(len(self.tri), 1) * 4), 0.05, 2.0))
        R = self._bin(axis, h)
        ii = np.floor((pa - R["a0"]) / h).astype(np.int64)
        jj = np.floor((pb - R["b0"]) / h).astype(np.int64)
        inside = (ii >= 0) & (ii < R["na"]) & (jj >= 0) & (jj < R["nb"])
        cell = np.where(inside, ii * R["nb"] + jj, 0)
        s0 = R["start"][cell]
        cnt = np.where(inside, R["start"][cell + 1] - s0, 0)
        csum = np.cumsum(cnt)
        Pt, d = R["P"], R["d"]
        rs_, ts_, ss_, cs_ = [], [], [], []
        lo = 0
        while lo < n:
            base = csum[lo - 1] if lo else 0
            hi = max(int(np.searchsorted(csum, base + chunk, side="right")), lo + 1)
            c = cnt[lo:hi]
            rr = np.repeat(np.arange(lo, hi), c)
            k = np.arange(c.sum()) - np.repeat(np.cumsum(c) - c, c)
            tt = R["tid"][np.repeat(s0[lo:hi], c) + k]
            A, B, C = Pt[tt, 0], Pt[tt, 1], Pt[tt, 2]
            qa, qb, dd = pa[rr], pb[rr], d[tt]
            w0 = ((B[:, b] - C[:, b]) * (qa - C[:, a]) + (C[:, a] - B[:, a]) * (qb - C[:, b])) / dd
            w1 = ((C[:, b] - A[:, b]) * (qa - C[:, a]) + (A[:, a] - C[:, a]) * (qb - C[:, b])) / dd
            w2 = 1.0 - w0 - w1
            m = (w0 >= -1e-12) & (w1 >= -1e-12) & (w2 >= -1e-12)
            rs_.append(rr[m])
            ts_.append(w0[m] * A[m, axis] + w1[m] * B[m, axis] + w2[m] * C[m, axis])
            ss_.append(R["sgn"][tt[m]])
            cs_.append(R["cos"][tt[m]])
            lo = hi
        r, t, s, c = (np.concatenate(x) for x in (rs_, ts_, ss_, cs_))
        if len(r) == 0:
            return empty
        o = np.lexsort((t, r))
        r, t, s, c = r[o], t[o], s[o], c[o]
        dup = np.zeros(len(r), bool)                     # a ray through a shared edge hits both triangles
        dup[1:] = (r[1:] == r[:-1]) & (s[1:] == s[:-1]) & (np.abs(t[1:] - t[:-1]) < 1e-7)
        r, t, s, c = r[~dup], t[~dup], s[~dup], c[~dup]
        first = np.ones(len(r), bool)
        first[1:] = r[1:] != r[:-1]
        k = np.arange(len(r)) - np.maximum.accumulate(np.where(first, np.arange(len(r)), 0))
        badhit = s != np.where(k % 2 == 0, -1.0, 1.0)
        bad = np.zeros(n, bool)
        bad[np.unique(r[badhit])] = True
        bad |= np.bincount(r, minlength=n) % 2 == 1
        ok = ~bad[r]
        r, t, c, k = r[ok], t[ok], c[ok], k[ok]
        e = k % 2 == 0
        return dict(r=r[e], t0=t[e], t1=t[~e], c0=c[e], c1=c[~e], n=n, bad=np.nonzero(bad)[0])


_MESH = {}


def mesh(s):
    k = id(s)
    if k not in _MESH:
        _MESH[k] = (RayMesh(_triangles(s)), s)
    return _MESH[k][0]


def _ends(res):
    """per ray: index of its first and last interval (-1: none)."""
    n, r = res["n"], res["r"]
    first, last = np.full(n, -1), np.full(n, -1)
    if len(r):
        idx = np.arange(len(r))
        st = np.r_[True, r[1:] != r[:-1]]
        en = np.r_[r[1:] != r[:-1], True]
        first[r[st]] = idx[st]
        last[r[en]] = idx[en]
    return first, last


def _grid_rect(x0, y0, x1, y1, step, jit=(0.0123, 0.0071)):
    xs = np.arange(x0 + jit[0], x1, step)
    ys = np.arange(y0 + jit[1], y1, step)
    if len(xs) == 0:
        xs = np.array([(x0 + x1) / 2 + jit[0]])
    if len(ys) == 0:
        ys = np.array([(y0 + y1) / 2 + jit[1]])
    X, Y = np.meshgrid(xs, ys)
    return X.ravel(), Y.ravel()


def _in_outline(X, Y, inset=0.0):
    """points inside the console outline (the R7.1 rounded rectangle) offset inward by inset."""
    o, r = P.OUTLINE, max(P.OUTLINE_R - inset, 0.0)
    x0, x1, y0, y1 = o.x0 + inset, o.x1 - inset, o.y0 + inset, o.y1 - inset
    inr = (X >= x0) & (X <= x1) & (Y >= y0) & (Y <= y1)
    cx = np.clip(X, x0 + r, x1 - r)
    cy = np.clip(Y, y0 + r, y1 - r)
    return inr & ((X - cx) ** 2 + (Y - cy) ** 2 <= r * r)


def _in_rect(X, Y, rect, pad=0.0):
    return (X >= rect.x0 - pad) & (X <= rect.x1 + pad) & (Y >= rect.y0 - pad) & (Y <= rect.y1 + pad)


def _in_poly(X, Y, pts):
    from matplotlib.path import Path
    return Path(np.asarray(pts)).contains_points(np.c_[X, Y], radius=1e-9)


def _poly_samples(pts, step):
    """points inside a polygon (enclosure frame): a grid + the vertices + the edge midpoints."""
    pts = np.asarray(pts, float)
    lo, hi = pts.min(0), pts.max(0)
    s = min(step, max((hi - lo).min() / 3, 0.1))
    X, Y = _grid_rect(lo[0], lo[1], hi[0], hi[1], s)
    m = _in_poly(X, Y, pts)
    c = pts.mean(0)
    edge = (pts + np.roll(pts, -1, 0)) / 2
    extra = np.vstack([pts, edge]) * 0.999 + c * 0.001         # a hair inside
    return np.r_[X[m], extra[:, 0]], np.r_[Y[m], extra[:, 1]]


# ================================================================ bodies
_CB = {}


def reset():
    """drop the per-shape caches (bounding boxes, meshes, context bodies)."""
    _BB.clear(), _MESH.clear(), _CB.clear()


def context_bodies(ctx):
    """{name: Shape}: the context bodies + 'cell (swollen)' (the cell with its swell allowance, as it ages)."""
    k = id(ctx)
    if k not in _CB:
        d = {n: s for n, (s, _) in ctx.context.items()}
        z = P.CELL["z"] + P.CELL["swell_z"]
        sw = P.CELL["rect"].prism(min(z), max(z))
        sw.label = "cell (swollen)"
        d["cell (swollen)"] = sw
        _CB[k] = (d, ctx)                                    # ctx kept alive: its id stays unique
    return _CB[k][0]


VIRTUAL_OF = {"cell (swollen)": "cell"}          # a derived body and its source: they overlap by construction


def _pair(a, b):
    return tuple(sorted((a, b)))


def _alternative(a, b):
    return b in ALTERNATIVES.get(a, ()) or a in ALTERNATIVES.get(b, ())


def _related(a, b):
    return VIRTUAL_OF.get(a) == b or VIRTUAL_OF.get(b) == a


def _owner(ctx, n):
    return ctx.part_owner.get(n, "?")


def _bought(n, part, ctx):
    import enclosure
    try:
        return enclosure.print_pose(n, part, ctx) is None
    except Exception:
        return False


def back_side_parts(parts, ctx):
    """the back shell + every module part that sits flush in the back (e.g. the FR4 battery plate): parts whose
    bounding box reaches within TOL_T of the back face under them."""
    out = {"back": parts["back"]}
    for n, p in parts.items():
        if n in ("front", "back"):
            continue
        lo, hi = bb(p)
        cx, cy = (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2
        if lo[2] <= P.back_at(cx, cy) + TOL_T and hi[2] <= P.SPLIT + TOL_T:
            out[n] = p
    return out


def find_fr4(parts, ctx):
    """the bought FR4 battery plate (a module part whose z span is CELL['floor_z'] over the cell), or None."""
    z0, z1 = P.CELL["floor_z"]
    best = None
    for n, p in parts.items():
        if n in ("front", "back"):
            continue
        lo, hi = bb(p)
        if abs(lo[2] - z0) <= 0.05 and abs(hi[2] - z1) <= 0.05 and \
                P.CELL["rect"].overlaps(P.Rect(lo[0], lo[1], hi[0], hi[1])):
            best = n
    return best


def find_control(parts, ctx, xy, zr, used, prefer=None):
    """the module part over a control position: its bounding box holds xy in plan and spans into zr; a part named
    `prefer` wins, then parts owned by controls, then the smallest."""
    cands = []
    for n, p in parts.items():
        if n in ("front", "back") or n in used:
            continue
        lo, hi = bb(p)
        if lo[0] - 1e-6 <= xy[0] <= hi[0] + 1e-6 and lo[1] - 1e-6 <= xy[1] <= hi[1] + 1e-6 and \
                lo[2] < zr[1] and hi[2] > zr[0]:
            rank = 0 if (prefer and n.lower() == prefer.lower()) else 1 if _owner(ctx, n) == "controls" else 2
            cands.append((rank, float(np.prod(hi - lo)), n))
    return min(cands)[2] if cands else None


def _switch_at(x, y, kind="top"):
    """the switch whose actuator is at (x, y)."""
    best = None
    for r, s in P.SWITCHES.items():
        if ("top_z" in s) != (kind == "top"):
            continue
        d = math.dist(s["at"], (x, y))
        if best is None or d < best[0]:
            best = (d, r)
    return best[1] if best and best[0] < 0.05 else None


# ================================================================ (a) interference + clearances
def check_a(parts, ctx, out, fast=False):
    say("\n(a) interference (mm3 > %g) and clearances" % EPS_V)
    cb = context_bodies(ctx)
    hits, n_pairs = [], 0
    pn = list(parts)
    for i, a in enumerate(pn):                                    # (a1) parts vs parts
        for b in pn[i + 1:]:
            n_pairs += 1
            v = vol(parts[a], parts[b])
            if v > EPS_V or v != v:
                hits.append(("part x part", a, b, v))
    for a in pn:                                                  # (a1) parts vs the context
        n_pairs += len(cb)
        big = np.prod(bb(parts[a])[1] - bb(parts[a])[0]) > 2e4
        res = overlaps(parts[a], cb) if big else {n: v for n in cb if (v := vol(parts[a], cb[n])) > EPS_V or v != v}
        for n, v in res.items():
            hits.append(("part x context", a, n, v))
    names = list(cb)                                              # (a2) context vs context
    lo = np.array([bb(cb[n])[0] for n in names])
    hi = np.array([bb(cb[n])[1] for n in names])
    ov = np.clip(np.minimum(hi[:, None], hi[None]) - np.maximum(lo[:, None], lo[None]), 0, None).prod(2)
    iu = np.triu_indices(len(names), 1)
    n_ctx = len(iu[0])
    for i, j in zip(*iu):
        if ov[i, j] <= EPS_V:
            continue
        a, b = names[i], names[j]
        if _alternative(a, b) or _related(a, b):
            continue
        v = vol(cb[a], cb[b])
        if v > EPS_V or v != v:
            hits.append(("context x context", a, b, v))
    plain = {(a, b): v for kind, a, b, v in hits}               # the swollen cell: report only what the swell adds
    hits2 = []
    for kind, a, b, v in hits:
        for x, y in ((a, b), (b, a)):
            if x in VIRTUAL_OF:
                src = VIRTUAL_OF[x]
                v0 = plain.get((src, y), plain.get((y, src), 0.0))
                v = v - v0
        if v > EPS_V or v != v:
            hits2.append((kind, a, b, v))
    bad, allowed = [], []
    for kind, a, b, v in hits2:
        why = ALLOWED.get(_pair(a, b))
        say(f"  {kind:17s} {a} x {b}: {v:.4f}" + (f"  (allowed: {why})" if why else "  <-- INTERFERENCE"))
        (allowed if why else bad).append(f"{a} x {b} {v:.3f}")
    say(f"  {n_pairs} part pairs ({len(pn)} parts vs each other + {len(cb)} context bodies), {n_ctx} context pairs; "
        f"{len(bad)} overlap(s), {len(allowed)} allowed")
    short = clearances(parts, ctx, cb)
    det = []
    if bad:
        det.append(f"{len(bad)} overlap(s): " + "; ".join(bad[:8]) + (" ..." if len(bad) > 8 else ""))
    if short:
        det.append(f"{len(short)} clearance(s) short: " + "; ".join(short[:6]) + (" ..." if len(short) > 6 else ""))
    LAST["a"] = bad + short
    out.append(("(a) interference + clearances", "FAIL" if det else "PASS",
                " | ".join(det) if det else f"no overlaps ({len(allowed)} allowed), every clearance met"))


def clearances(parts, ctx, cb):
    """(a3): print every required clearance, return the short ones."""
    say("  (a3) clearances (measured / required, mm)")
    short = []

    def need(label, a, b, req, why):
        g, exact = gap(a, b, req)
        ok = g >= req - TOL
        say(f"    {label:44s} {('%.3f' % g) if exact else '>%.2f' % g:>7s} / {req:.3f}  {why}"
            + ("" if ok else "  <-- SHORT"))
        if not ok:
            short.append(f"{label} {g:.3f} < {req:.3f}")
        return g

    front, back_side = parts["front"], back_side_parts(parts, ctx)
    panel_gap = min(P.PANEL_TAPE if P.FRONT == "flush" else P.GASKET, P.LCM_OUTLINE_TOL / 2)
    for n in ("panel LCM", "panel touch", "panel glass", "panel active area"):
        if n in cb:
            need(f"{n} to the front shell", cb[n], front, panel_gap,
                 "LCM on the ledge tape" if P.FRONT == "flush" else "glass under the lip gasket")
    tf = max(0.0, P.Z_GLASS_MAX - P.PANEL["z_glass"][1])
    if "touch flex" in cb:
        need("touch flex to the front shell (fold skin)", cb["touch flex"], front, tf,
             f"the glass may sit {tf:.2f} higher (Z_GLASS_MAX)")
    if P.LCD_PLATE:
        need("SUS plate to the LCM", cb["lcd plate"], cb["panel LCM"], P.SS_AIR, "SS_AIR")
        need("cell to the SUS plate", cb["cell"], cb["lcd plate"], P.CELL_TAPE + P.SS_FILM, "tape + film")
    else:
        need("cell (swollen) to the LCM", cb["cell (swollen)"], cb["panel LCM"], P.CELL_AIR, "CELL_AIR")
    below = P.CELL["z"][0] - P.CELL["floor_z"][1]
    req = min(below, P.CELL["fence_clr"])
    for n, s in back_side.items():
        need(f"cell to {n}", cb["cell"], s, req, f"floor gap {below:.2f} / fence_clr {P.CELL['fence_clr']}")
    if P.LCD_PLATE:
        for n, s in back_side.items():
            need(f"cell (swollen) to {n}", cb["cell (swollen)"], s, 0.0, "the swell gap toward the back")
    worst = None
    for n, s in cb.items():                                     # the LCM over every F part
        if n.startswith("F:") and _bb_gap(s, cb["panel LCM"]) < P.P1_AIR + 0.5:
            g, _ = gap(s, cb["panel LCM"], P.P1_AIR)
            if worst is None or g < worst[0]:
                worst = (g, n)
            if g < P.P1_AIR - TOL:
                short.append(f"{n} to the LCM {g:.3f} < {P.P1_AIR}")
                say(f"    {n + ' to the LCM':44s} {g:7.3f} / {P.P1_AIR:.3f}  P1_AIR  <-- SHORT")
    if worst:
        say(f"    {'LCM over the F parts (nearest: ' + worst[1] + ')':44s} {worst[0]:7.3f} / {P.P1_AIR:.3f}  P1_AIR")
    for ref, led in P.LEDS.items():                              # a clear light column from each LED to the face
        col = _cyl(*led["at"], 1.0, led["top_z"] + 0.05, P.Z_FACE + 0.1)
        v = vol(front, col)
        pipe = [n for n, p in parts.items() if n not in ("front", "back") and vol(p, col) > EPS_V]
        ok = v <= EPS_V
        say(f"    {'LED ' + ref + ' window (a D1.0 light column)':44s} "
            + ("clear" if ok else f"{v:.3f} mm3 of front shell in it") + (f"; light pipe: {', '.join(pipe)}" if pipe
                                                                           else "") + ("" if ok else "  <-- BLOCKED"))
        if not ok:
            short.append(f"LED {ref} window blocked {v:.3f}")
    short += board_clearances(parts, ctx, cb, need)
    short += vertical_clearances(parts, ctx, cb)
    fc = cb.get(P.FPC1_KEY)
    if fc is not None:
        near = []
        for n, s in cb.items():
            if n.startswith("B:") and _bb_gap(s, fc) < 0.6:
                g, _ = gap(s, fc, 0.5)
                if g < 0.5:
                    near.append((g, n))
        say(f"    {P.FPC1_KEY}, B parts within 0.5 (info): " +
            (", ".join(f"{n} {g:.3f}" + (" (allowed overlap)" if _pair(n, P.FPC1_KEY) in ALLOWED else "")
                       for g, n in sorted(near)) or "none"))
    return short


def board_clearances(parts, ctx, cb, need):
    """the board: its edge to the shells beside it (their material in its z band, off its footprint) >=
    BOARD_WALL_MIN; the front shell off the boss tips >= BOARD_FACE_MIN (the tips are the datum: nothing else may
    touch); the back shell >= BACK_BOSS_GAP (the screws pull the board onto the front datum). Ray scans over the
    footprint print where the nearest material is."""
    short = []
    board = cb["board"]
    pts = P.BJ["board"]["outline_yup"]
    band = P.Rect(P.OUTLINE.x0 - 1, P.OUTLINE.y0 - 1, P.OUTLINE.x1 + 1, P.OUTLINE.y1 + 1).prism(P.Z_BOARD_BOT,
                                                                                               P.BOARD_TOP)
    foot = P.prism(P.face(pts, board_frame=False), P.Z_BOARD_BOT - 1.0, P.BOARD_TOP + 1.0)
    for sh in ("front", "back"):                                  # beside the board: in its z band, off its footprint
        try:
            lat = (parts[sh] & band).cut(foot)
        except Exception:
            lat = None
        if lat is not None and lat.solids() and sum(s.volume for s in lat.solids()) > EPS_V:
            need(f"board edge to the {sh} shell beside it", board, lat, BOARD_WALL_MIN,
                 f"BOARD_WALL_MIN (its material in z 0..{P.BOARD_TOP:g})")
    tips = [_cyl(*b["at"], P.BOSS_D + 0.4, P.SPLIT - 0.1, P.SPLIT + 1.0) for b in P.BOSSES.values() if b["on_board"]]
    try:
        f_off = parts["front"].cut(*tips) if tips else parts["front"]
    except Exception:
        f_off = parts["front"]
    need("board to the front shell off the boss tips", board, f_off, BOARD_FACE_MIN, "BOARD_FACE_MIN (no contact)")
    need("board to the back shell", board, parts["back"], P.BACK_BOSS_GAP, "BACK_BOSS_GAP")
    X, Y = _grid_rect(*bb(board)[0][:2], *bb(board)[1][:2], GRID)
    m = _in_poly(X, Y, pts)
    for h in P.HOLES.values():
        m &= np.hypot(X - h["at"][0], Y - h["at"][1]) > h["d"] / 2 + 0.05
    X, Y = X[m], Y[m]
    tip = np.zeros(len(X), bool)
    for b in P.BOSSES.values():
        if b["on_board"]:
            tip |= np.hypot(X - b["at"][0], Y - b["at"][1]) <= P.BOSS_D / 2 + 0.2
    res = mesh(parts["front"]).cast(2, X[~tip], Y[~tip])
    sel = res["t1"] > P.BOARD_TOP + 1e-6
    if sel.any():
        g = res["t0"][sel] - P.BOARD_TOP
        k = np.argmin(g)
        i = res["r"][sel][k]
        say(f"      (nearest front material over the board's top face, off the boss tips: {g[k]:.3f} at "
            f"{_k(X[~tip][i], Y[~tip][i])})")
    worst = None
    for n, s in back_side_parts(parts, ctx).items():
        res = mesh(s).cast(2, X, Y)
        sel = res["t0"] < P.Z_BOARD_BOT - 1e-6
        if sel.any():
            g = P.Z_BOARD_BOT - res["t1"][sel]
            k = np.argmin(g)
            if worst is None or g[k] < worst[0]:
                worst = (g[k], n, res["r"][sel][k])
    if worst:
        say(f"      (nearest back-side material under the board's bottom face: {worst[1]} {worst[0]:.3f} at "
            f"{_k(X[worst[2]], Y[worst[2]])}; the screws pull the board onto the front datum)")
    return short


def vertical_clearances(parts, ctx, cb):
    """every part envelope's vertical gap to the shell facing it: F parts (+ the jack) up to the front shell's
    material >= CLR_F, B parts (+ USB-C, microSD, both FPC1s) down to the back shell's >= CLR_B. Sampled over each
    footprint inside the walls (openings in the wall are the modules' own clearances)."""
    short = []
    rows = {"F": [], "B": []}
    fm = mesh(parts["front"])
    bms = [mesh(s) for s in back_side_parts(parts, ctx).values()]
    for n, s in cb.items():
        side = "F" if n.startswith("F:") or n == "jack" else "B" if (n.startswith("B:") or n in (
            "usb_c", "microsd", "FPC1 as placed", "FPC1 corrected", "FPC1")) else None
        if side is None:
            continue
        lo, hi = bb(s)
        if n.startswith(("F:", "B:")):
            pts = P.part_outline(n[2:])
        elif n.startswith("FPC1"):
            pts = P.FPC1["placed" if n.endswith("placed") else "corrected"]
        else:
            pts = [(lo[0], lo[1]), (hi[0], lo[1]), (hi[0], hi[1]), (lo[0], hi[1])]
        X, Y = _poly_samples(pts, GRID)
        m = _in_rect(X, Y, P.WALL_IN, -0.05)
        X, Y = X[m], Y[m]
        if not len(X):
            continue
        body = mesh(s).cast(2, X, Y)                              # the body's own top / bottom under each sample
        first, last = _ends(body)
        has = first >= 0
        if not has.any():
            continue
        X, Y = X[has], Y[has]
        btop, bbot = body["t1"][last[has]], body["t0"][first[has]]
        g = None
        if side == "F":
            res = fm.cast(2, X, Y)
            sel = res["t1"] > btop[res["r"]] - 1e-6
            if sel.any():
                g = float((res["t0"][sel] - btop[res["r"][sel]]).min())
            req = P.CLR_F
        else:
            for m_ in bms:
                res = m_.cast(2, X, Y)
                sel = res["t0"] < bbot[res["r"]] + 1e-6
                if sel.any():
                    gg = float((bbot[res["r"][sel]] - res["t1"][sel]).min())
                    g = gg if g is None else min(g, gg)
            req = P.CLR_B
        if g is None:
            continue
        rows[side].append((g, n))
        if g < req - TOL_T:                                       # a mesh reading: within TOL_T of the target
            short.append(f"{n} {g:.3f} {'under the front' if side == 'F' else 'over the back'} (< {req})")
    for side, req, what in (("F", P.CLR_F, "up to the front shell"), ("B", P.CLR_B, "down to the back shell")):
        r = sorted(rows[side])
        lows = [f"{n} {g:.3f}" for g, n in r if g < req - TOL_T]
        say(f"    {side} parts {what} >= {req} (CLR_{side}): {len(r)} measured; nearest "
            + ", ".join(f"{n} {g:.2f}" for g, n in r[:5]) + (f"  <-- SHORT: {', '.join(lows)}" if lows else ""))
    return short


# ================================================================ (b) keep-outs
def check_b(parts, ctx, out, fast=False):
    say("\n(b) keep-outs (mm3 > %g)" % EPS_V)
    bad, n = [], 0
    for sh in ("front", "back"):                                  # the finished shells vs every keep-out
        for kn, k in ctx.keepouts.items():
            n += 1
            v = vol(parts[sh], k)
            if v <= EPS_V and v == v:
                continue
            owner = kn.split(":")[0]
            if owner != "core":
                # a module's keep-out binds the OTHER modules (their adds and parts are tested below); the shell
                # material left inside it is the owner's call (its own adds, or bare shell it keeps): info only
                say(f"  finished {sh} shell in {kn}: {v:.4f}  (info: {owner}'s keep-out; {owner} decides what of "
                    f"the shell stays there)")
                continue
            why = KEEPOUT_ALLOWED.get((kn, sh))
            say(f"  finished {sh} shell in {kn}: {v:.4f}" + (f"  (allowed: {why})" if why else "  <-- IN KEEP-OUT"))
            if not why:
                bad.append(f"{sh} shell in {kn} {v:.3f}")
    for mod, r in ctx.modules.items():                            # module material vs OTHER modules' + core keep-outs
        mats = [(f"{mod} {k}[{i}]", s) for k in ("front_add", "back_add") for i, s in enumerate(r[k])]
        mats += [(f"{mod} part {pn}", s) for pn, s in r["parts"].items()]
        for kn, k in ctx.keepouts.items():
            if kn.split(":")[0] == mod:
                continue
            for mn, s in mats:
                n += 1
                v = vol(s, k)
                if v > EPS_V or v != v:
                    why = KEEPOUT_ALLOWED.get((kn, mn))
                    say(f"  {mn} in {kn}: {v:.4f}" + (f"  (allowed: {why})" if why else "  <-- IN KEEP-OUT"))
                    if not why:
                        bad.append(f"{mn} in {kn} {v:.3f}")
    say(f"  {len(ctx.keepouts)} keep-outs ({sum(k.startswith('core:') for k in ctx.keepouts)} core), {n} pairs")
    LAST["b"] = bad
    out.append(("(b) keep-outs", "FAIL" if bad else "PASS",
                "; ".join(bad[:10]) + (" ..." if len(bad) > 10 else "") if bad else f"{len(ctx.keepouts)} keep-outs clear"))


# ================================================================ (c) rest gaps + board datum + screws
def _axis_probe(e):
    """(axis, sign, lo, hi) of the plunger's cross-section swept 4 mm outward from the tip along dir."""
    (tx, ty), (ux, uy) = e["tip"], e["dir"]
    ax = 0 if abs(ux) >= abs(uy) else 1
    sg = 1.0 if (ux, uy)[ax] > 0 else -1.0
    r = P.SWITCHES[e["switch"]]["plunger_d"] / 2
    tip = np.array([tx, ty, e["axis_z"]])
    lo, hi = tip - r, tip + r
    if sg > 0:
        lo[ax], hi[ax] = tip[ax], tip[ax] + 4.0
    else:
        lo[ax], hi[ax] = tip[ax] - 4.0, tip[ax]
    return ax, sg, lo, hi, tip


def edge_gap(part, e):
    """(gap from the plunger tip to the part along dir, the part's proud past the outer surface) or (None, None)."""
    ax, sg, lo, hi, tip = _axis_probe(e)
    try:
        c = part & _box3(lo, hi)
    except Exception:
        return None, None
    if c is None or not c.solids():
        return None, None
    cb_ = c.bounding_box()
    g = (cb_.min.X - tip[0] if ax == 0 else cb_.min.Y - tip[1]) if sg > 0 else \
        (tip[0] - cb_.max.X if ax == 0 else tip[1] - cb_.max.Y)
    plo, phi = bb(part)
    outer = e["outer"][ax]
    proud = (phi[ax] - outer) if sg > 0 else (outer - plo[ax])
    return g, proud


def check_c(parts, ctx, out, fast=False):
    lo_, hi_ = REST_RANGE
    say(f"\n(c) rest gaps (want {lo_:.2f}-{hi_:.2f}), board datum, screw paths")
    bad, used, n = [], set(), 0
    for name, k in P.KEYS.items():                                # face keys + D-pad
        cap = find_control(parts, ctx, k["at"], (P.KEYSTAKE_Z, P.Z_FACE), used, prefer=name)
        if cap is None:
            bad.append(f"{name}: no part over {k['switch']}")
            say(f"  {name:8s} no part over its switch {k['switch']} at {_k(*k['at'])}  <-- MISSING")
            continue
        used.add(cap)
        for cx, cy in k["contacts"]:
            ref = _switch_at(cx, cy)
            sw = P.SWITCHES[ref]
            n += 1
            z = _lowest(parts[cap], cx, cy, sw["actuator_d"], sw["top_z"] - 1.0, sw["top_z"] + 4.0)
            zc = _lowest(parts[cap], cx, cy, 0.3, sw["top_z"] - 1.0, sw["top_z"] + 4.0)
            if z is None or zc is None:
                bad.append(f"{name} ({cap}): no stem over {ref}")
                say(f"  {name:8s} {cap}: no material over the {ref} keystake centre  <-- MISSING")
                continue
            g, gmax = z - sw["top_z"], z - sw["top_z_max"]
            ok = lo_ - TOL <= g <= hi_ + TOL and gmax > 0 and zc - z <= 0.05
            want = k["rest_gap"]
            say(f"  {name:8s} {cap} over {ref}: tip z {z:.3f}, gap {g:.3f} (contract {want}"
                + (f", off by {g - want:+.3f}" if abs(g - want) > TOL else "") + f"), vs the max keystake {gmax:.3f}"
                + ("" if zc - z <= 0.05 else f", tip not over the centre ({zc - z:.2f} higher there)")
                + ("" if ok else "  <-- OFF"))
            if not ok:
                bad.append(f"{name} over {ref} gap {g:.3f}")
        if k["shape"] == "cross":                                 # D-pad pivot on the board top
            cx, cy = k["at"]
            n += 1
            z = _lowest(parts[cap], cx, cy, k["pivot_d"] + 0.5, P.BOARD_TOP - 1.0, P.KEYSTAKE_Z)
            ok = z is not None and abs(z - k["pivot_z"]) <= TOL
            say(f"  {name:8s} {cap} pivot: lowest z {('%.3f' % z) if z is not None else 'none'} within "
                f"D{k['pivot_d'] + 0.5:.1f} of the centre (want {k['pivot_z']}: on the board top)"
                + ("" if ok else "  <-- OFF"))
            if not ok:
                bad.append(f"{name} pivot z {z}")
    for group in (P.EDGE_KEYS, P.SHOULDERS):                      # SKRT edge caps + shoulder bumpers
        for name, e in group.items():
            ax, sg, plo, phi, tip = _axis_probe(e)
            probe = tip.copy()
            probe[ax] += sg * (e["rest_gap"] + 0.3)
            cap = find_control(parts, ctx, probe[:2], (e["axis_z"] - 0.5, e["axis_z"] + 0.5), used, prefer=name)
            n += 1
            if cap is None:
                bad.append(f"{name}: no cap at {e['switch']}")
                say(f"  {name:8s} no part on the {e['switch']} plunger axis at {_k(*e['tip'])}  <-- MISSING")
                continue
            used.add(cap)
            g, proud = edge_gap(parts[cap], e)
            ok = g is not None and lo_ - TOL <= g <= hi_ + TOL
            say(f"  {name:8s} {cap} to the {e['switch']} plunger tip along dir: "
                + (f"{g:.3f} (contract {e['rest_gap']}), proud {proud:+.2f} past the outer surface (contract "
                   f"{e['proud']})" if g is not None else "nothing on the axis") + ("" if ok else "  <-- OFF"))
            if not ok:
                bad.append(f"{name} gap {g}")
    front, back = parts["front"], parts["back"]                   # board datum
    for bn, b in P.BOSSES.items():
        x, y = b["at"]
        n += 1
        fz0, fz1 = P.SPLIT, P.SPLIT + P.INSERT["depth"]
        ring_f = _cyl(x, y, P.BOSS_D - 0.3, fz0, fz1) - _cyl(x, y, P.INSERT["hole_d"] + 0.3, fz0 - 1, fz1 + 1)
        fill_f = vol(front, ring_f) / max(ring_f.volume, 1e-9)    # the boss round the insert (the walls can graze S2)
        hole_f = vol(front, _cyl(x, y, P.INSERT["hole_d"] - 0.3, fz0 + 0.05, fz1 - 0.05))
        ft = _lowest(front, x, y, P.BOSS_D, P.SPLIT - 1.0, P.SPLIT + 2.0)
        bz1 = b["back_top_z"]
        bz0 = P.back_at(x, y) + P.SCREW["head_h"] + 0.5
        ring_b = _cyl(x, y, P.BOSS_D - 0.3, bz0, bz1 - 0.1) - _cyl(x, y, P.SCREW["cbore_d"] + 0.3, bz0 - 1, bz1)
        fill_b = vol(back, ring_b) / max(ring_b.volume, 1e-9) if bz1 - 0.1 > bz0 else 1.0
        bt = _highest(back, x, y, P.BOSS_D, P.Z_BACK_MIN, P.SPLIT + 0.5)
        okf = ft is not None and abs(ft - P.SPLIT) <= TOL and fill_f > 0.5 and hole_f <= EPS_V
        okb = bt is not None and abs(bt - bz1) <= TOL and fill_b > 0.5
        say(f"  boss {bn:3s} {_k(x, y)}: front tip z {('%.3f' % ft) if ft is not None else 'none'} (want "
            f"{P.SPLIT}), boss fill {fill_f:.0%}, insert hole {'open' if hole_f <= EPS_V else 'FILLED %.2f' % hole_f}"
            f" | back top z {('%.3f' % bt) if bt is not None else 'none'} (want {bz1}), boss fill {fill_b:.0%}"
            + ("" if okf and okb else "  <-- OFF"))
        if not (okf and okb):
            bad.append(f"boss {bn}: front " + ("ok" if okf else f"tip {ft} fill {fill_f:.0%}") + ", back "
                       + ("ok" if okb else f"top {bt} fill {fill_b:.0%}"))
    cb = context_bodies(ctx)                                      # the M2 screws: head + shank clear
    obst = {"front": front, "back": back, "board": cb["board"]}
    obst.update({n_: s for n_, s in parts.items() if n_ not in ("front", "back")})
    for bn, b in P.BOSSES.items():
        x, y = b["at"]
        z0 = P.back_at(x, y)
        head = _cyl(x, y, P.SCREW["head_d"], z0, z0 + P.SCREW["head_h"])
        shank = _cyl(x, y, P.SCREW["d"], z0 + P.SCREW["head_h"], P.SPLIT + P.INSERT["depth"] - 0.3)
        hit = []
        for on, o in obst.items():
            for what, s in (("head", head), ("shank", shank)):
                v = vol(s, o)
                if v > EPS_V or v != v:
                    hit.append(f"{what} x {on} {v:.3f}")
        n += 1
        if hit:
            bad.append(f"screw {bn}: " + ", ".join(hit))
            say(f"  screw {bn}: M2 head D{P.SCREW['head_d']} x {P.SCREW['head_h']} from the back face + shank D"
                f"{P.SCREW['d']} to z {P.SPLIT + P.INSERT['depth'] - 0.3:.2f}: " + ", ".join(hit) + "  <-- BLOCKED")
    near_bottom = [bn for bn, b in P.BOSSES.items() if b["at"][1] - P.OUTLINE.y0 <= 8.0]
    snaps = [kn for kn in ctx.keepouts if any(f in kn for f in SWEEP_FLEX)]
    say(f"  bottom edge ({P.OUTLINE.w - 2 * P.OUTLINE_R:.0f} mm straight): screws within 8 mm of it "
        f"{', '.join(near_bottom) or 'none'}" + (f" + {len(snaps)} snap(s)" if snaps else "") + " (brief: 2-3)")
    if not any(s.startswith("screw") for s in bad):
        say(f"  screws: {len(P.BOSSES)} x M2 (head D{P.SCREW['head_d']} x {P.SCREW['head_h']} from the back face, shank "
            f"D{P.SCREW['d']} into the insert) clear of both shells, the board and every part")
    LAST["c"] = bad
    out.append(("(c) rest gaps + datum", "FAIL" if bad else "PASS",
                "; ".join(bad[:10]) + (" ..." if len(bad) > 10 else "") if bad else f"{n} readings on target"))


# ================================================================ (d) solids + print poses
def check_d(parts, ctx, out, fast=False):
    import enclosure
    say("\n(d) solids, validity, print poses")
    bad, bought = [], []
    for n, p in parts.items():
        k, valid, v = len(p.solids()), p.is_valid, p.volume
        ok = k == 1 and valid and v > 0
        try:
            pp = enclosure.print_pose(n, p, ctx)
        except Exception as ex:
            pp, ok = None, False
            say(f"  {n}: print_pose raised {ex!r}  <-- BAD")
        pose = "bought (STEP only)"
        if pp is not None:
            pb = pp.bounding_box()
            good = abs(pb.min.Z) < 1e-6 and abs(pp.volume - v) < 0.01 + 1e-6 * v
            ok &= good
            pose = f"print pose z 0..{pb.max.Z:.2f}, volume {'kept' if good else 'CHANGED %.2f' % pp.volume}"
        else:
            bought.append(n)
        say(f"  {n:18s} [{_owner(ctx, n)}] {k} solid(s), valid {valid}, {v:9.1f} mm3; {pose}" + ("" if ok else "  <-- BAD"))
        if not ok:
            bad.append(f"{n}: {k} solid(s), valid {valid}")
    for n, zf in (("front", P.Z_FACE), ("back", P.Z_BACK_MIN)):  # the shells print on their outer face
        try:
            pp = enclosure.print_pose(n, parts[n], ctx)
            bed = sum(f.area for f in pp.faces() if abs(f.center().Z) < 1e-4 and abs(abs(f.normal_at().Z) - 1) < 1e-6)
            top = sum(f.area for f in parts[n].faces() if abs(f.center().Z - zf) < 1e-4 and
                      abs(abs(f.normal_at().Z) - 1) < 1e-6)
            ok = bed > 0 and abs(bed - top) < 0.01 * max(top, 1)
            note = ""
            if n == "back" and P.HUMP:
                body = sum(f.area for f in parts[n].faces() if abs(f.center().Z - P.Z_BACK) < 1e-4)
                note = f"; the body's back face ({body:.0f} mm2) is {P.Z_BACK - P.Z_BACK_HUMP:.2f} above the bed"
            say(f"  {n} prints on its {'face' if n == 'front' else 'back'}: bed faces {bed:.0f} mm2 = the faces at z "
                f"{zf:+.2f} {top:.0f} mm2{note}" + ("" if ok else "  <-- BAD"))
            if not ok:
                bad.append(f"{n} bed {bed:.0f} vs {top:.0f} mm2")
        except Exception as ex:
            bad.append(f"{n} print pose: {ex!r}")
    LAST["d"] = bad
    out.append(("(d) one valid solid per part", "FAIL" if bad else "PASS",
                "; ".join(bad) if bad else f"{len(parts)} parts" + (f" ({len(bought)} bought: {', '.join(bought)})"
                                                                     if bought else "")))


# ================================================================ (e) thickness
def opening_zones(ctx):
    """[(name, lo, hi)]: boxes round the wall openings (core keep-outs + the SKRT / shoulder notches), grown by
    OPENING_MARGIN; walls there are judged at the printable minimum."""
    z = []
    for kn in ("core:usb_c plug", "core:jack plug", "core:microsd card"):
        if kn in ctx.keepouts:
            lo, hi = bb(ctx.keepouts[kn])
            z.append((kn[5:], lo - OPENING_MARGIN, hi + OPENING_MARGIN))
    for group in (P.EDGE_KEYS, P.SHOULDERS):
        for name, e in group.items():
            (ox, oy), (ux, uy) = e["outer"], e["dir"]
            w = e["notch_w"] / 2 + OPENING_MARGIN
            if abs(ux) >= abs(uy):
                lo, hi = [ox - 3 * P.WALL, oy - w, 0], [ox + 1, oy + w, 0]
            else:
                lo, hi = [ox - w, oy - 3 * P.WALL, 0], [ox + w, oy + 1, 0]
            lo[2], hi[2] = e["notch_z"][0] - OPENING_MARGIN, e["notch_z"][1] + OPENING_MARGIN
            z.append((f"{name} notch", np.array(lo), np.array(hi)))
    return z


def _plate_scan(front, rows, bad):
    """front plate / CN1 / lip / fold skin: z rays from the face, the top interval where it starts at the face."""
    inset = max(P.R_FRONT - P.WALL, 0.0) + P.WALL + 0.3
    X, Y = _grid_rect(P.OUTLINE.x0 + inset, P.OUTLINE.y0 + inset, P.OUTLINE.x1 - inset, P.OUTLINE.y1 - inset, GRID)
    m = _in_outline(X, Y, inset)
    X, Y = X[m], Y[m]
    res = mesh(front).cast(2, X, Y)
    _, last = _ends(res)
    has = last >= 0
    t1 = np.where(has, res["t1"][np.maximum(last, 0)], np.nan)
    t0 = np.where(has, res["t0"][np.maximum(last, 0)], np.nan)
    face = has & (np.abs(t1 - P.Z_FACE) <= TOL_T)
    t = t1 - t0
    fold = _in_rect(X, Y, P.FOLD_BAY["rect"])
    cn1 = _in_rect(X, Y, P.CN1["rect"], CN1_ZONE)
    lip = (_in_rect(X, Y, P.POCKET) & ~_in_rect(X, Y, P.LIP_WINDOW)) if P.LIP_WINDOW else np.zeros_like(fold)
    plate = ~(fold | cn1 | lip)
    zmin = MIN_SLAB[P.PROCESS]["z"]
    for name, sel, req, cap in (("front plate", plate, P.PLATE, None), ("plate over CN1", cn1, P.PLATE_CN1, None),
                                ("lip", lip, P.LIP, None),
                                ("fold-bay skin", fold, zmin, P.FOLD_BAY["skin_max"])):
        s = sel & face
        if not s.any():
            if name in ("front plate", "fold-bay skin"):
                rows.append(f"    {name}: no reading (no material at the face)")
            continue
        i = np.argmin(np.where(s, t, np.inf))
        j = np.argmax(np.where(s, t, -np.inf))
        lowbad = s & (t < req - TOL_T)
        hibad = s & (t > cap + TOL_T) if cap is not None else np.zeros_like(s)
        line = (f"    {name:16s} {s.sum():6d} rays: {t[i]:.3f} at {_k(X[i], Y[i])} .. {t[j]:.3f}; want >= {req:.2f}"
                + (f" and <= {cap:.2f}" if cap is not None else ""))
        if lowbad.any():
            k = np.argmin(np.where(lowbad, t, np.inf))
            bad.append(f"{name} {t[k]:.3f} < {req:.2f} at {_k(X[k], Y[k])} ({lowbad.sum()} rays)")
            line += f"  <-- THIN ({lowbad.sum()} rays)"
        if hibad.any():
            k = np.argmax(np.where(hibad, t, -np.inf))
            bad.append(f"{name} {t[k]:.3f} > {cap:.2f} at {_k(X[k], Y[k])} ({hibad.sum()} rays)")
            line += f"  <-- THICK ({hibad.sum()} rays: the inner face under Z_FOLD_SKIN_IN {P.Z_FOLD_SKIN_IN})"
        rows.append(line)


def _floor_scan(parts, ctx, rows, bad):
    back = parts["back"]
    inset = max(P.R_BACK - P.WALL, 0.0) + P.WALL + 0.3
    X, Y = _grid_rect(P.OUTLINE.x0 + inset, P.OUTLINE.y0 + inset, P.OUTLINE.x1 - inset, P.OUTLINE.y1 - inset, GRID)
    m = _in_outline(X, Y, inset)
    if P.HUMP:
        band = P.WALL + P.R_BACK + 0.5
        m &= ~(_in_rect(X, Y, P.HUMP_RECT, band) & ~_in_rect(X, Y, P.HUMP_RECT, -band))
    X, Y = X[m], Y[m]
    res = mesh(back).cast(2, X, Y)
    first, _ = _ends(res)
    has = first >= 0
    t0 = np.where(has, res["t0"][np.maximum(first, 0)], np.nan)
    t1 = np.where(has, res["t1"][np.maximum(first, 0)], np.nan)
    ba = np.array([P.back_at(x, y) for x, y in zip(X, Y)])
    fl = has & (np.abs(t0 - ba) <= TOL_T)
    t = t1 - t0
    if fl.any():
        i = np.argmin(np.where(fl, t, np.inf))
        lowbad = fl & (t < P.FLOOR_PR - TOL_T)
        line = f"    {'printed floor':16s} {fl.sum():6d} rays: {t[i]:.3f} at {_k(X[i], Y[i])}; want >= {P.FLOOR_PR:.2f}"
        if lowbad.any():
            bad.append(f"floor {t[i]:.3f} < {P.FLOOR_PR} at {_k(X[i], Y[i])} ({lowbad.sum()} rays)")
            line += f"  <-- THIN ({lowbad.sum()} rays)"
        rows.append(line)
    open_ = (~fl).sum()
    rows.append(f"    {'':16s} {open_:6d} rays with no floor at the back face (windows, grille, counterbores, "
                f"holes)")
    if P.CELL["floor"] == "fr4":
        fr = find_fr4(parts, ctx)
        if fr is None:
            bad.append("no FR4 battery plate part (CELL floor 'fr4')")
            rows.append(f"    FR4 plate: no part spans z {P.CELL['floor_z']} over the cell  <-- MISSING")
        else:
            pts = P.CELL["rect"].grow(-1.0)
            Xp, Yp = _grid_rect(pts.x0, pts.y0, pts.x1, pts.y1, 2.0)
            r2 = mesh(parts[fr]).cast(2, Xp, Yp)
            f2, l2 = _ends(r2)
            ok = f2 >= 0
            tt = r2["t1"][l2[ok]] - r2["t0"][f2[ok]]
            bot = r2["t0"][f2[ok]]
            zb = P.back_at(P.CELL["rect"].cx, P.CELL["rect"].cy)
            good = len(tt) and np.all(np.abs(tt - P.FLOOR_FR4) <= TOL_T) and np.all(np.abs(bot - zb) <= TOL_T)
            rows.append(f"    FR4 plate '{fr}': {ok.sum()}/{len(Xp)} rays over the cell, thickness "
                        f"{(tt.min() if len(tt) else float('nan')):.3f}..{(tt.max() if len(tt) else float('nan')):.3f} "
                        f"(want {P.FLOOR_FR4}), bottom z {(bot.min() if len(bot) else float('nan')):+.3f} (back face "
                        f"{zb:+.2f})" + ("" if good and ok.all() else "  <-- OFF"))
            if not (good and ok.all()):
                bad.append(f"FR4 plate {fr} thickness / flush / coverage off")


def _ledge_scan(front, rows, bad):
    """flush: the LCM back sits on the ledge tape - the ledge must be there, at LEDGE_Z1, LEDGE_T thick, over its
    three segments, and nothing else of the front shell may be under the LCM. lip: the glass datum is the lip (the
    module goes in from inside), so no ledge is needed and nothing at all may be under the LCM."""
    lcm, zb = P.PANEL["lcm"], P.PANEL["z_lcm"][0]
    fm = mesh(front)
    reliefs = [v["rect"].grow(0.5) for v in P.LEDGE["relief"].values()]
    z0, z1 = P.LEDGE["z"]
    lip = P.FRONT == "lip"
    if lip:
        rows.append("    ledge: not needed (lip: the module goes in from inside, the lip gasket is its datum)")
    for name, seg in ({} if lip else P.LEDGE["segments"]).items():
        s = P.Rect(max(seg.x0, lcm.x0), max(seg.y0, lcm.y0), min(seg.x1, lcm.x1), min(seg.y1, lcm.y1))
        if s.w <= 0 or s.h <= 0:
            continue
        X, Y = _grid_rect(s.x0 + 0.1, s.y0 + 0.1, s.x1 - 0.1, s.y1 - 0.1, 0.3)
        rel = np.zeros(len(X), bool)
        for rr in reliefs:
            rel |= _in_rect(X, Y, rr)
        res = fm.cast(2, X, Y)
        under = res["t1"] <= zb + TOL_T
        sub = {k: v[under] for k, v in res.items() if k in ("r", "t0", "t1")}
        top = np.full(len(X), np.nan)
        bot = np.full(len(X), np.nan)
        if len(sub["r"]):
            ends = np.r_[sub["r"][1:] != sub["r"][:-1], True]
            top[sub["r"][ends]] = sub["t1"][ends]
            bot[sub["r"][ends]] = sub["t0"][ends]
        t = top - bot
        reg = ~rel
        miss = reg & np.isnan(top)
        offz = reg & ~np.isnan(top) & (np.abs(top - z1) > TOL_T)
        thin = reg & ~np.isnan(top) & (t < (z1 - z0) - TOL_T)
        cov = 100.0 * (reg & ~np.isnan(top)).sum() / max(reg.sum(), 1)
        line = (f"    ledge {name:12s} {reg.sum():5d} rays over KiCad x {s.x0:.1f}-{s.x1:.1f} / y {-s.y1:.1f}-"
                f"{-s.y0:.1f}: coverage {cov:.0f}%, top z {np.nanmin(top) if (~np.isnan(top)).any() else float('nan'):.3f}"
                f"..{np.nanmax(top) if (~np.isnan(top)).any() else float('nan'):.3f} (want {z1}), thickness >= "
                f"{np.nanmin(t[reg]) if (reg & ~np.isnan(t)).any() else float('nan'):.3f} (want {z1 - z0:.2f})")
        if miss.any() or offz.any() or thin.any():
            line += "  <-- " + ", ".join(w for w, c in (("MISSING %d" % miss.sum(), miss.any()),
                                                         ("TOP OFF %d" % offz.sum(), offz.any()),
                                                         ("THIN %d" % thin.sum(), thin.any())) if c)
            bad.append(f"ledge {name}: {miss.sum()} missing, {offz.sum()} top off, {thin.sum()} thin of {reg.sum()}")
        if rel.any():
            tr = t[rel & ~np.isnan(t)]
            line += (f"; over the C20 / C94 reliefs {rel.sum()} rays, ledge there "
                     + (f"{tr.min():.3f}..{tr.max():.3f}" if len(tr) else "none"))
            if len(tr) and tr.min() < MIN_SLAB[P.PROCESS]["z"] - TOL_T:
                bad.append(f"ledge over the reliefs {tr.min():.3f} thick")
                line += "  <-- THIN"
        rows.append(line)
    # nothing else of the front shell under the LCM inside the pocket (no ledge over the cell or the fold)
    X, Y = _grid_rect(P.POCKET.x0 + 0.05, P.POCKET.y0 + 0.05, P.POCKET.x1 - 0.05, P.POCKET.y1 - 0.05, GRID)
    m = np.ones(len(X), bool)
    for seg in ({} if lip else P.LEDGE["segments"]).values():
        m &= ~_in_rect(X, Y, seg, 0.3)
    X, Y = X[m], Y[m]
    res = fm.cast(2, X, Y)
    sel = res["t0"] < zb - TOL_T
    where = "inside the pocket (the module's path from inside)" if lip else "outside the ledge segments"
    if sel.any():
        rr = res["r"][sel]
        pts = sorted({(round(X[i], 0), round(Y[i], 0)) for i in rr})
        bad.append(f"front shell under the LCM {where} at {len(set(rr))} rays, e.g. "
                   + ", ".join(_k(x, y) for x, y in pts[:3]))
        rows.append(f"    under the LCM {where}: front material at {len(set(rr))} of {len(X)} rays, e.g. "
                    f"{', '.join(_k(x, y) for x, y in pts[:4])}  <-- NOT ALLOWED"
                    + ("" if lip else " (no ledge over the cell / fold)"))
    else:
        rows.append(f"    under the LCM {where}: clear ({len(X)} rays)")


def _cut_owner(ctx, sh, q):
    """the module whose cut (from shell sh) holds point q, or None."""
    from build123d import Vector
    v = Vector(*q)
    for mod, r in ctx.modules.items():
        for c in r[f"{sh}_cut"]:
            lo, hi = bb(c)
            if np.all(np.asarray(q) >= lo - 1e-6) and np.all(np.asarray(q) <= hi + 1e-6):
                if any(s.is_inside(v) for s in c.solids()):
                    return mod
    return None


def _wall_scan(parts, ctx, rows, bad):
    """the side walls' straight runs, rays inward from the outer surface (several z per shell). A wall thinner than
    WALL is judged at the printable minimum when it is inside an opening zone, in the split band, or thinned by a
    module's cut (a relief, spot-face, scallop, snap pocket: the cut holds the missing material); otherwise FAIL."""
    zones = opening_zones(ctx)
    xymin = MIN_SLAB[P.PROCESS]["xy"]
    o, R = P.OUTLINE, P.OUTLINE_R + 0.3
    for sh in ("front", "back"):
        m_ = mesh(parts[sh])
        worst, notes = {}, []
        for side in ("left", "right", "bottom", "top"):
            if side in ("left", "right"):
                ax, a_rng, edge = 0, (o.y0 + R, o.y1 - R), (o.x0 if side == "left" else o.x1)
            else:
                ax, a_rng, edge = 1, (o.x0 + R, o.x1 - R), (o.y0 if side == "bottom" else o.y1)
            inward = 1.0 if side in ("left", "bottom") else -1.0
            a = np.arange(a_rng[0] + 0.0123, a_rng[1], GRID)
            zr = (P.SPLIT + 0.05, P.Z_FACE - P.R_FRONT - 0.05) if sh == "front" else (P.Z_BACK_MIN + 0.05,
                                                                                      P.SPLIT - 0.05)
            zz = np.arange(zr[0] + 0.0071, zr[1], 0.25)
            A, Z = np.meshgrid(a, zz)
            A, Z = A.ravel(), Z.ravel()
            if sh == "back":                                      # above the local back face's fillet
                ba = np.array([P.back_at(edge, y) for y in A]) if ax == 0 else \
                    np.array([P.back_at(x, edge) for x in A])
                keep = Z > ba + P.R_BACK + 0.05
                A, Z = A[keep], Z[keep]
            res = m_.cast(ax, A, Z)
            first, last = _ends(res)
            idx = first if inward > 0 else last
            has = idx >= 0
            i = np.maximum(idx, 0)
            t0, t1 = res["t0"][i], res["t1"][i]
            depth = (t0 - edge) if inward > 0 else (edge - t1)       # entry behind the outer surface (a recess)
            wall = has & (depth < P.WALL - TOL_T)
            t = t1 - t0
            inz = np.zeros(len(A), bool)
            for zn, lo, hi in zones:
                if lo[ax] <= edge + P.WALL and hi[ax] >= edge - P.WALL:        # the zone reaches this wall
                    inz |= (lo[1 - ax] <= A) & (A <= hi[1 - ax]) & (lo[2] <= Z) & (Z <= hi[2])
            relaxed = inz | (np.abs(Z - P.SPLIT) < SPLIT_BAND)
            thin = wall & (t < P.WALL - TOL_T) & ~relaxed
            # attribute the thin readings outside the zones to a module's cut (one probe per 1 x 0.5 mm cell)
            by_cut = np.zeros(len(A), bool)
            cells = {}
            for j in np.nonzero(thin)[0]:
                cells.setdefault((int(A[j] // 1.0), int(Z[j] // 0.5)), []).append(j)
            for js in cells.values():
                j = min(js, key=lambda q: t[q])
                inner = t1[j] if inward > 0 else t0[j]                # the wall's inner face along the ray
                s_ = (t0[j] + edge) / 2 if depth[j] > TOL_T and inward > 0 else \
                    (t1[j] + edge) / 2 if depth[j] > TOL_T else inner + inward * 0.05
                q = [0.0, 0.0, Z[j]]
                q[ax], q[1 - ax] = s_, A[j]
                own = _cut_owner(ctx, sh, q)
                if own:
                    by_cut[js] = True
                    pt = (edge, A[j]) if ax == 0 else (A[j], edge)
                    notes.append((t[j], f"{side} {t[j]:.2f} at {_k(*pt)} z {Z[j]:.2f} ({own})"))
            soft = wall & (relaxed | by_cut)
            strict_bad = thin & ~by_cut
            soft_bad = soft & (t < xymin - TOL_T)
            if wall.any():
                k = np.argmin(np.where(wall & ~soft, t, np.inf)) if (wall & ~soft).any() else np.argmin(
                    np.where(wall, t, np.inf))
                pt = (edge, A[k]) if ax == 0 else (A[k], edge)
                worst[side] = (t[k], pt, Z[k], wall.sum(), (~has | ~wall).sum())
            for sel, req, what in ((strict_bad, P.WALL, "wall"),
                                   (soft_bad, xymin, "wall at an opening / the split / a module cut")):
                if sel.any():
                    k = np.argmin(np.where(sel, t, np.inf))
                    pt = (edge, A[k]) if ax == 0 else (A[k], edge)
                    bad.append(f"{sh} {side} {what} {t[k]:.3f} < {req:.2f} at {_k(*pt)} z {Z[k]:.2f} "
                               f"({sel.sum()} rays)")
                    rows.append(f"    {sh} {side} {what} {t[k]:.3f} < {req:.2f} at {_k(*pt)} z {Z[k]:.2f} "
                                f"({sel.sum()} rays)  <-- THIN")
        rows.append(f"    {sh:5s} walls (want >= {P.WALL:.2f}; >= {xymin:.2f} at openings / the split band / module "
                    f"cuts): " + ", ".join(f"{s} {w[0]:.3f} at {_k(*w[1])} z {w[2]:.2f} ({w[3]} rays, {w[4]} open)"
                                           for s, w in worst.items()))
        if notes:
            seen, uniq = set(), []
            for tv, s in sorted(notes):
                key = s.split(" at ")[0].split()[0] + s.split("(")[-1]
                if key not in seen:
                    seen.add(key)
                    uniq.append(s)
            rows.append(f"    {sh:5s} walls thinned by module cuts (judged >= {xymin}): " + "; ".join(uniq[:6]))


def _fence_scan(parts, ctx, rows, bad):
    """the cell fences in the notch gaps (CONTRACT: <= fence_t_max, fence_clr off the cell, inside the buffer where
    the board's notch edges are): rays across each gap from the floor + 1.5 (above any root chamfer) to the split. A
    fence that runs into the outer wall (the bottom gap is 1.5 + 0.5 to the wall) is reported, not judged."""
    back, cell, buf = parts["back"], P.CELL["rect"], P.CELL["buffer"]
    zf = max(P.floor_in_at(cell.cx, cell.cy), P.Z_FLOOR_IN)       # hump: above the body floor round the hump too
    zz = np.arange(zf + 1.5 + 0.0071, P.SPLIT - 0.05, 0.5)
    m_ = mesh(back)
    tmax = P.CELL["fence_t_max"]
    for side in ("left", "right", "top", "bottom"):
        ax = 0 if side in ("left", "right") else 1
        if ax == 0:
            a = np.arange(cell.y0 + 2.0 + 0.0123, cell.y1 - 2.0, 1.0)
            edge, out_edge, inward = (cell.x0, buf.x0, -1) if side == "left" else (cell.x1, buf.x1, 1)
        else:
            a = np.arange(cell.x0 + 2.0 + 0.0123, cell.x1 - 2.0, 1.0)
            edge, out_edge, inward = (cell.y1, buf.y1, 1) if side == "top" else (cell.y0, buf.y0, -1)
        A, Z = (v.ravel() for v in np.meshgrid(a, zz))
        res = m_.cast(ax, A, Z)
        # the first interval beyond the cell edge on this side (outward = inward sign here: -1 left / bottom)
        near = (res["t1"] <= edge + TOL_T) if inward < 0 else (res["t0"] >= edge - TOL_T)
        rr = res["r"][near]
        t0, t1 = res["t0"][near], res["t1"][near]
        if inward < 0:                                            # the interval nearest the cell: the largest t1
            o = np.lexsort((-t1, rr))
        else:
            o = np.lexsort((t0, rr))
        rr, t0, t1 = rr[o], t0[o], t1[o]
        firsts = np.r_[True, rr[1:] != rr[:-1]] if len(rr) else np.zeros(0, bool)
        rr, t0, t1 = rr[firsts], t0[firsts], t1[firsts]
        clr = (edge - t1) if inward < 0 else (t0 - edge)
        infence = clr <= (abs(out_edge - edge) + 0.5)             # material in the gap (else: the wall / beyond)
        rr, t0, t1, clr = rr[infence], t0[infence], t1[infence], clr[infence]
        if not len(rr):
            rows.append(f"    fence {side:6s}: none in the gap ({len(A)} rays)")
            continue
        th = t1 - t0
        past = (t0 < out_edge - TOL_T) if inward < 0 else (t1 > out_edge + TOL_T)
        wall_in = {"left": P.WALL_IN.x0, "right": P.WALL_IN.x1, "top": P.WALL_IN.y1, "bottom": P.WALL_IN.y0}[side]
        merged = (t0 <= wall_in + TOL_T) if inward < 0 else (t1 >= wall_in - TOL_T)   # runs into the outer wall
        cov = 100.0 * len(np.unique(rr)) / len(A)
        free = th[~merged]
        line = (f"    fence {side:6s}: {cov:3.0f}% of {len(A)} rays, "
                + (f"{free.min():.2f}..{free.max():.2f} thick (<= {tmax})" if len(free) else "")
                + (f"{', ' if len(free) else ''}{merged.sum()} rays merged into the outer wall" if merged.any() else "")
                + f", {clr.min():.2f} off the cell (>= {P.CELL['fence_clr']})")
        flags = []
        if len(free) and free.max() > tmax + TOL_T:
            flags.append(f"THICK {free.max():.2f}")
        if clr.min() < P.CELL["fence_clr"] - TOL_T:
            flags.append(f"CLOSE {clr.min():.2f}")
        if past.any() and side in ("right", "top"):               # the board's notch edges
            flags.append(f"PAST THE NOTCH EDGE ({past.sum()} rays)")
        if flags:
            bad.append(f"fence {side}: " + ", ".join(flags))
            line += "  <-- " + ", ".join(flags)
        rows.append(line)


def _speaker_seal(parts, ctx, rows, bad):
    """the floor under the speaker: open only inside the grille D (inside the foam ring), solid from there out to the
    cup's inner wall (the foam seat and the frame: a hole there short-circuits the front and back of the cone)."""
    s = P.SPEAKER
    x0, y0 = s["at"]
    X, Y = _grid_rect(x0 - s["cup_id"] / 2, y0 - s["cup_id"] / 2, x0 + s["cup_id"] / 2, y0 + s["cup_id"] / 2, 0.25)
    rad = np.hypot(X - x0, Y - y0)
    res = mesh(parts["back"]).cast(2, X, Y)
    first, _ = _ends(res)
    zb = P.back_at(x0, y0)
    floor = (first >= 0) & (np.abs(res["t0"][np.maximum(first, 0)] - zb) <= TOL_T)
    inner = rad <= s["grille_d"] / 2
    seat = (rad >= s["grille_d"] / 2 + 0.15) & (rad <= s["cup_id"] / 2 - 0.15)
    holes = seat & ~floor
    line = (f"    speaker floor: {100.0 * (inner & ~floor).sum() / max(inner.sum(), 1):.0f}% open inside the D"
            f"{s['grille_d']:g} grille; foam seat + frame annulus to D{s['cup_id']:g}: "
            + ("solid" if not holes.any() else f"{holes.sum()} open rays"))
    if holes.any():
        k = np.argmax(np.where(holes, rad, -1))
        bad.append(f"speaker floor open outside the grille at {_k(X[k], Y[k])} ({holes.sum()} rays)")
        line += "  <-- LEAK"
    rows.append(line)


def _slab_scan(parts, ctx, rows, bad):
    """every printed part, rays along x, y and z on a grid: an interval shorter than MIN_SLAB whose entry and exit
    faces both face the ray (within SLAB_DEG) is a thin slab (a graze through a fillet or a chamfer edge is not).
    Thin readings are grouped per part by plan position (3 mm cells, any z): one line per spot, its thinnest."""
    cs = math.cos(math.radians(SLAB_DEG))
    for n, p in parts.items():
        if _bought(n, p, ctx):
            continue
        lo, hi = bb(p)
        step = SCAN if n in ("front", "back") else float(np.clip((hi - lo).min() / 12, 0.05, SCAN))
        thin, nr = [], 0
        for ax in (0, 1, 2):
            a, b = _AX[ax]
            ar = np.arange(lo[a] + 0.0123 * step / SCAN, hi[a], step)
            br = np.arange(lo[b] + 0.0071 * step / SCAN, hi[b], step if ax == 2 else min(step, 0.2))
            A, B = (x.ravel() for x in np.meshgrid(ar, br))
            res = mesh(p).cast(ax, A, B)
            nr += res["n"]
            L = res["t1"] - res["t0"]
            req = MIN_SLAB[P.PROCESS]["z" if ax == 2 else "xy"]
            sel = (L < req - TOL_T) & (res["c0"] >= cs) & (res["c1"] >= cs) & (L > 1e-4)
            for i in np.nonzero(sel)[0]:
                c = [0.0, 0.0, 0.0]
                c[a], c[b], c[ax] = A[res["r"][i]], B[res["r"][i]], (res["t0"][i] + res["t1"][i]) / 2
                thin.append((c[0], c[1], c[2], float(L[i]), "xyz"[ax], req))
        spots = {}
        for row in thin:
            key = (int(row[0] // 3.0), int(row[1] // 3.0))
            if key not in spots or row[3] < spots[key][0][3]:
                spots[key] = [row, spots.get(key, [None, 0])[1] + 1]
            else:
                spots[key][1] += 1
        for (x, y, z, t, axn, req), cnt in sorted(spots.values(), key=lambda q: q[0][3]):
            why = next((r for pn, c, rad, r in THIN_ALLOWED if pn == n and math.dist(c, (x, y, z)) <= rad), None)
            rows.append(f"    {n} slab {t:.3f} along {axn} (< {req}) at {_k(x, y)} z {z:.2f} [{cnt} rays]"
                        + (f"  (allowed: {why})" if why else "  <-- THIN"))
            if not why:
                bad.append(f"{n} slab {t:.3f} along {axn} at {_k(x, y)} z {z:.2f}")
        if not thin:
            rows.append(f"    {n:18s} {nr:7d} rays: no slab under {MIN_SLAB[P.PROCESS]['xy']} (xy) / "
                        f"{MIN_SLAB[P.PROCESS]['z']} (z)")


def check_e(parts, ctx, out, fast=False):
    say(f"\n(e) thickness (ray scans; PROCESS {P.PROCESS}: printable minimum {MIN_SLAB[P.PROCESS]['xy']} xy / "
        f"{MIN_SLAB[P.PROCESS]['z']} z)")
    rows, bad = [], []
    for f, args in ((_plate_scan, (parts["front"],)), (_ledge_scan, (parts["front"],)), (_floor_scan, (parts, ctx)),
                    (_fence_scan, (parts, ctx)), (_speaker_seal, (parts, ctx)), (_wall_scan, (parts, ctx))) + \
            (() if fast else ((_slab_scan, (parts, ctx)),)):
        try:
            f(*args, rows, bad)
        except Exception as ex:
            bad.append(f"{f.__name__} raised {ex!r}")
            rows.append(f"    {f.__name__}: {traceback.format_exc().splitlines()[-1]}  <-- ERROR")
    for r in rows:
        say(r)
    if fast:
        say("    (slab scan skipped: --fast)")
    LAST["e"] = bad
    out.append(("(e) thickness", "FAIL" if bad else "PASS",
                "; ".join(bad[:8]) + (" ..." if len(bad) > 8 else "") if bad else
                "plate, CN1, skin, ledge, walls, floor" + ("" if fast else ", slab scan") + " on target"))


# ================================================================ (f) region table
def _budget():
    """({region: min}, {architecture: total}) from the research budget (stack.json), or ({}, {})."""
    try:
        s = json.load(open(BUDGET_JSON))
    except Exception:
        return {}, {}
    reg = {r["region"].lower(): r["min"] for r in s.get("regions", [])}
    arch = s.get("architectures", {})
    return reg, arch


def _arch_total(arch):
    """the budget's total for this flag set (single-flag variants of A, B, C), or None."""
    f = P.FLAGS
    alt = [k for k, v in (("FRONT", "flush"), ("BACK", "flat"), ("CELL", "LP803860"), ("LCD_PLATE", False))
           if f[k] != v]
    if not alt:
        return arch.get("A_flat_flush", {}).get("total"), "A_flat_flush"
    if alt == ["FRONT"]:
        return arch.get("A_flat_lip", {}).get("total"), "A_flat_lip"
    if alt == ["LCD_PLATE"]:
        return arch.get("A_lcd_plate", {}).get("total"), "A_lcd_plate"
    if alt == ["BACK"]:
        return arch.get("B_hump", {}).get("total_max"), "B_hump"
    if alt == ["CELL"]:
        return arch.get("C_thin_cell", {}).get("total"), "C_thin_cell"
    return None, "+".join(alt)


def check_f(parts, ctx, out, fast=False):
    say(f"\n(f) per-region thickness [{P.VARIANT}] (front face / glass top to the back face; controls left out)")
    bud, arch = _budget()
    at, an = _arch_total(arch)
    bad = []
    if at is not None and abs(at - P.TOTAL) > 0.01:
        bad.append(f"params TOTAL {P.TOTAL:.2f} != the budget's {an} {at:.2f}")
    total = P.TOTAL
    tops = [parts["front"]] + [context_bodies(ctx)[n] for n in ("panel glass", "panel active area", "panel touch",
                                                                "panel LCM")]
    bots = list(back_side_parts(parts, ctx).values())
    X, Y = _grid_rect(P.OUTLINE.x0, P.OUTLINE.y0, P.OUTLINE.x1, P.OUTLINE.y1, REGION_GRID)
    m = _in_outline(X, Y, 0.05)
    X, Y = X[m], Y[m]
    top = np.full(len(X), -np.inf)
    bot = np.full(len(X), np.inf)
    for s in tops:
        r = mesh(s).cast(2, X, Y)
        np.maximum.at(top, r["r"], r["t1"])
    for s in bots:
        r = mesh(s).cast(2, X, Y)
        np.minimum.at(bot, r["r"], r["t0"])
    ok = np.isfinite(top) & np.isfinite(bot)
    t = np.where(ok, top - bot, np.nan)
    ba = np.array([P.back_at(x, y) for x, y in zip(X, Y)])
    # the slab envelope: no part but the controls above the face, below the local back face or past the outline
    for n, p in parts.items():
        if _owner(ctx, n) == "controls":
            continue
        lo, hi = bb(p)
        if lo[0] < P.OUTLINE.x0 - TOL_T or hi[0] > P.OUTLINE.x1 + TOL_T or lo[1] < P.OUTLINE.y0 - TOL_T or \
                hi[1] > P.OUTLINE.y1 + TOL_T:
            bad.append(f"{n} past the outline (x {lo[0]:.2f}..{hi[0]:.2f}, KiCad y {-hi[1]:.2f}..{-lo[1]:.2f})")
        if hi[2] <= P.Z_FACE + TOL_T and lo[2] >= P.Z_BACK - TOL_T:
            continue                                              # inside the slab everywhere
        r = mesh(p).cast(2, X, Y)
        up = r["t1"] > P.Z_FACE + TOL_T
        dn = r["t0"] < ba[r["r"]] - TOL_T
        if up.any():
            k = np.argmax(np.where(up, r["t1"], -np.inf))
            bad.append(f"{n} over the face: z {r['t1'][k]:.2f} at {_k(X[r['r'][k]], Y[r['r'][k]])} ({up.sum()} rays)")
        if dn.any():
            k = np.argmin(np.where(dn, r["t0"] - ba[r["r"]], np.inf))
            i = r["r"][k]
            bad.append(f"{n} under the back face: z {r['t0'][k]:.2f} (local back {ba[i]:.2f}) at {_k(X[i], Y[i])} "
                       f"({dn.sum()} rays)")
    rows = P.region_table(show=False)
    say(f"  architecture total {total:.2f} (params.TOTAL)" + (f"; budget {an} {at:.2f}" if at is not None else
                                                              f"; no budget architecture for {an}")
        + ("" if bud else f"; budget regions not found ({BUDGET_JSON})"))
    say(f"  {'region':20s} {'KiCad x / y':24s} {'budget':>6s} {'needs':>6s} {'model':>6s} {'meas.':>6s} "
        f"{'margin':>6s}  governed by   (budget: research, baseline A; needs / model: params; meas.: these parts)")
    for name, rects, need, model, margin, gov in rows:
        sel = np.zeros(len(X), bool)
        for rk in rects:
            sel |= _in_rect(X, Y, P.Rect.k(*rk))
        sel &= ok
        if not sel.any():
            bad.append(f"{name}: no reading")
            continue
        tmax = float(np.nanmax(t[sel]))
        b = bud.get(name.lower())
        flag = []
        if tmax > total + TOL_T:
            flag.append(f"THICKER THAN {total:.2f}")
            bad.append(f"{name} {tmax:.2f} > total {total:.2f}")
        if tmax < need - TOL_T:
            flag.append(f"THINNER THAN IT NEEDS {need:.2f}")
            bad.append(f"{name} {tmax:.2f} < needs {need:.2f}")
        r0 = rects[0]
        say(f"  {name:20s} {f'{r0[0]:g}-{r0[2]:g} / {r0[1]:g}-{r0[3]:g}':24s} "
            f"{('%6.2f' % b) if b is not None else '     -'} {need:6.2f} {model:6.2f} {tmax:6.2f} "
            f"{tmax - need:+6.2f}  {gov}" + (f"  <-- {', '.join(flag)}" if flag else ""))
    allt = t[ok]
    ctl = [p for n, p in parts.items() if _owner(ctx, n) == "controls"]
    if ctl:
        zt = max(bb(p)[1][2] for p in ctl)
        say(f"  slab max {np.nanmax(allt):.2f} over {ok.sum()} rays; with the controls (top z {zt:.2f}, "
            f"{zt - P.Z_FACE:+.2f} over the face) {zt - P.Z_BACK_MIN:.2f}")
    else:
        say(f"  slab max {np.nanmax(allt):.2f} over {ok.sum()} rays; no controls yet")
    env = [n for n, s in context_bodies(ctx).items() if bb(s)[1][2] > P.Z_FACE + TOL or bb(s)[0][2] < P.Z_BACK_MIN - TOL]
    if env:
        bad.append("context bodies outside the slab: " + ", ".join(env))
        say("  context bodies outside the slab z: " + ", ".join(env) + "  <-- OUT")
    LAST["f"] = bad
    out.append(("(f) region thickness", "FAIL" if bad else "PASS",
                "; ".join(bad[:6]) if bad else
                f"every region <= {total:.2f} and >= its needs; max {np.nanmax(allt):.2f}" +
                (f" (budget {an} {at:.2f})" if at is not None else "")))


# ================================================================ (j) press travel
def _obstacles(parts, ctx, own_refs, skip=()):
    d = {n: s for n, s in context_bodies(ctx).items() if n not in skip and n != "cell (swollen)"
         and not any(n == f"F:{r}" or n == f"B:{r}" for r in own_refs)}
    d.update({n: s for n, s in parts.items()})
    return d


def _press_hits(moved, rest, obst, name):
    hits = []
    for on, o in obst.items():
        if on == name or _bb_ub(moved, o) <= EPS_V:
            continue
        v = vol(moved, o)
        if v > EPS_V or v != v:
            v0 = vol(rest, o)                                # only what the press adds counts ((a) has the rest)
            if v - v0 > EPS_V or v != v:
                hits.append(f"{on} {v - v0:.3f}")
    return hits


# ================================================================ (h) assembly path
def sweep_hits(mover, obst, up, flex=()):
    """[(obstacle, rays, (x, y), z)]: mover sliding along z into its rest pose from far away (up: from below) past
    the obstacles already in place. Blocked wherever an obstacle's material lies in its path: under one of the
    mover's surfaces (up) or over one (down). flex: (lo, hi) boxes where contact is an elastic snap (skipped)."""
    lo, hi = bb(mover)
    X, Y = _grid_rect(lo[0], lo[1], hi[0], hi[1], SWEEP_GRID)
    rm = mesh(mover).cast(2, X, Y)
    M = {}
    for r, a, b in zip(rm["r"], rm["t0"], rm["t1"]):
        M.setdefault(int(r), []).append((a, b))
    if not M:
        return []
    out = []
    for on, o in obst.items():
        olo, ohi = bb(o)
        if ohi[0] < lo[0] or olo[0] > hi[0] or ohi[1] < lo[1] or olo[1] > hi[1]:
            continue
        if (up and olo[2] >= hi[2]) or (not up and ohi[2] <= lo[2]):
            continue                                              # never in the path
        sel = np.nonzero((X >= olo[0] - 1e-6) & (X <= ohi[0] + 1e-6) & (Y >= olo[1] - 1e-6) & (Y <= ohi[1] + 1e-6))[0]
        sel = sel[np.isin(sel, list(M))]
        if not len(sel):
            continue
        ro = mesh(o).cast(2, X[sel], Y[sel])
        cnt, first = 0, None
        for r, a, b in zip(ro["r"], ro["t0"], ro["t1"]):
            i = int(sel[r])
            for m0, m1 in M[i]:
                hit = (b <= m0 + 1e-6) if up else (a >= m1 - 1e-6)
                if hit and not any(fl[0] <= X[i] <= fh[0] and fl[1] <= Y[i] <= fh[1] and fl[2] <= (a + b) / 2 <= fh[2]
                                   for fl, fh in flex):
                    cnt += 1
                    if first is None:
                        first = ((X[i], Y[i]), (a, b))
                    break
        if cnt:
            out.append((on, cnt, first[0], first[1]))
    return out


def check_h(parts, ctx, out, fast=False):
    say("\n(h) assembly path (CONTRACT.md order; every body slides along z into its rest pose past what is in place)")
    cb = context_bodies(ctx)
    bad, steps = [], []
    owner = {n: _owner(ctx, n) for n in parts}
    on_board = {n for n, p in parts.items() if owner[n] == "controls" and abs(bb(p)[0][2] - P.BOARD_TOP) <= TOL_T
                and bb(p)[1][2] <= P.KEYSTAKE_Z + 1.0}            # taped on the board (the D-pad washer)
    ctl = {n: parts[n] for n in parts if owner[n] == "controls" and n not in on_board}
    front_in = {"front": parts["front"]}
    front_in.update({n: p for n, p in parts.items() if owner[n] == "front"})
    other = {n: p for n, p in parts.items() if owner[n] not in ("shell", "front", "back", "controls")}
    panel = {n: cb[n] for n in ("panel LCM", "panel touch", "panel glass", "panel active area") if n in cb}
    board = {n: s for n, s in cb.items() if n == "board" or n.startswith(("F:", "B:")) or n in (
        "usb_c", "jack", "microsd", P.FPC1_KEY)}
    board.update({n: parts[n] for n in on_board})
    backp = {"back": parts["back"]}
    backp.update({n: p for n, p in parts.items() if owner[n] == "back"})
    # GB3_LCD_PLATE=1: the SUS plate (the notch's sharp-cornered buffer outline) cannot rise past the board's R2 notch
    # corners, so it goes in from inside before the board, the cell taped under it (CONTRACT.md has no order for it)
    sus = {n: cb[n] for n in ("lcd plate", "cell") if n in cb} if "lcd plate" in cb else {}
    backp.update({n: cb[n] for n in ("cell", "lra", "speaker", "speaker foam") if n in cb and n not in sus})
    flex = [bb(k) for kn, k in ctx.keepouts.items() if any(f in kn for f in SWEEP_FLEX)]
    lip = P.FRONT == "lip"
    plan = [("1. controls into the face-down front shell, head-first from inside", ctl, True, {**front_in, **other}),
            (f"2. the module {'from inside, up against the lip' if lip else 'from the face onto the ledge tape'}",
             panel, lip, {**front_in, **other, **ctl}),
            ("3. the board (parts, jack, USB-C, microSD, corrected FPC1, washer) onto the boss tips", board, True,
             {**front_in, **other, **ctl, **panel, **sus}),
            ("4. the back shell with " + ("the battery plate, " if P.CELL["floor"] == "fr4" else "")
             + ("the LRA and speaker" if sus else "the cell, LRA and speaker") + ", from below", backp, True,
             {**front_in, **other, **ctl, **panel, **board, **sus})]
    if sus:
        plan.insert(2, ("2b. the SUS plate with the cell taped under it, from inside (before the board: it cannot "
                        "pass the board's notch corners)", sus, True, {**front_in, **other, **ctl, **panel}))
    for title, movers, up, obst in plan:
        hits = []
        for mn, m in movers.items():
            for on, cnt, (x, y), (z0, z1) in sweep_hits(m, {k: v for k, v in obst.items() if k != mn}, up, flex):
                why = SWEEP_ALLOWED.get((mn, on))
                hits.append(f"{mn} x {on}: {cnt} rays, e.g. {_k(x, y)} ({on} z {z0:.2f}..{z1:.2f} "
                            f"{'under' if up else 'over'} it)" + (f"  (allowed: {why})" if why else ""))
                if not why:
                    bad.append(f"step {title[0]}: {mn} x {on} at {_k(x, y)}")
        say(f"  {title}: {len(movers)} bodies {'up' if up else 'down'} past {len(obst)}: "
            + ("clear" if not hits else f"{len(hits)} blocked"))
        for h in hits[:12]:
            say(f"    {h}" + ("" if "(allowed" in h else "  <-- BLOCKED"))
        steps.append(len(hits))
    if flex:
        say(f"  (snap blades flex past: {len(flex)} region(s) from keep-outs named {', '.join(SWEEP_FLEX)})")
    LAST["h"] = bad
    out.append(("(h) assembly path", "FAIL" if bad else "PASS",
                "; ".join(bad[:8]) + (" ..." if len(bad) > 8 else "") if bad else
                f"{len(plan)} steps clear ({sum(len(m) for _, m, _, _ in plan)} bodies)"))


def rock_solve(part, obst, k, u, s_l, d):
    """the D-pad rocked toward u (small angles): every point at s (along u from the centre) moves dz = t - theta * s.
    The pressed legs (s_l) must go down d, so t = theta * s_l - d; every other surface keeps clear of the material
    above and below it (z rays over the D-pad's footprint through it and every obstacle): each gap gives a bound on
    theta. -> dict(theta, t, lo, hi, lo_by, hi_by, need_dn, need_up); theta None when no tilt satisfies them all
    (lo_by / hi_by: (bound, obstacle, s, gap) of the binding constraints; need_*: the extra gap either would need)."""
    cx, cy = k["at"]
    lo, hi = bb(part)
    X, Y = _grid_rect(lo[0] - 0.2, lo[1] - 0.2, hi[0] + 0.2, hi[1] + 0.2, ROCK_GRID)
    S = (X - cx) * u[0] + (Y - cy) * u[1]
    rd = mesh(part).cast(2, X, Y)
    D = [[] for _ in range(len(X))]
    for r, a, b in zip(rd["r"], rd["t0"], rd["t1"]):
        D[r].append((a, b))
    O = [[] for _ in range(len(X))]
    for on, o in obst.items():
        olo, ohi = bb(o)
        if ohi[0] < lo[0] - 0.2 or olo[0] > hi[0] + 0.2 or ohi[1] < lo[1] - 0.2 or olo[1] > hi[1] + 0.2:
            continue
        ro = mesh(o).cast(2, X, Y)
        for r, a, b in zip(ro["r"], ro["t0"], ro["t1"]):
            if D[r]:
                O[r].append((a, b, on))
    lows, highs = [(0.0, "-", 0.0, 0.0)], []
    for i in range(len(X)):
        den = s_l - S[i]
        for a, b in D[i]:
            up = [(o0 - b, on) for o0, o1, on in O[i] if o0 >= b - 1e-6]
            dn = [(a - o1, on) for o0, o1, on in O[i] if o1 <= a + 1e-6]
            if up:
                g, on = min(up)
                if den > 1e-9:
                    highs.append(((g + d) / den, on, S[i], g))
            if dn:
                g, on = min(dn)
                if den > 1e-9:
                    lows.append(((d - g) / den, on, S[i], g))
                elif den < -1e-9:
                    highs.append(((d - g) / den, on, S[i], g))
                elif g < d:
                    highs.append((-1.0, on, S[i], g))
    lb = max(lows)
    ub = min(highs) if highs else (math.inf, "-", 0.0, 0.0)
    out = dict(lo=lb[0], hi=ub[0], lo_by=lb, hi_by=ub, theta=None, t=None, need_dn=0.0, need_up=0.0)
    if lb[0] <= ub[0]:
        th = lb[0] + 0.25 * (ub[0] - lb[0]) if math.isfinite(ub[0]) else lb[0] + 0.005
        out.update(theta=th, t=th * s_l - d)
    else:
        out["need_dn"] = (d - ub[0] * (s_l - lb[2])) - lb[3]       # keep theta at the upper limit
        out["need_up"] = (lb[0] * (s_l - ub[2]) - d) - ub[3]       # keep theta at the lower limit
    return out


def check_j(parts, ctx, out, fast=False):
    from build123d import Axis, Pos
    say(f"\n(j) press travel (each control to {PRESS_MARGIN} short of its switch's stop; nothing else touched)")
    bad, used, n = [], set(), 0
    for name, k in P.KEYS.items():
        cap = find_control(parts, ctx, k["at"], (P.KEYSTAKE_Z, P.Z_FACE), used, prefer=name)
        if cap is None:
            say(f"  {name:8s} no part (see (c))")
            bad.append(f"{name}: no part")
            continue
        used.add(cap)
        part = parts[cap]
        refs = [_switch_at(*c) for c in k["contacts"]]
        obst = _obstacles(parts, ctx, refs)
        tips = {r: _lowest(part, *P.SWITCHES[r]["at"], P.SWITCHES[r]["actuator_d"], P.KEYSTAKE_Z - 1, P.KEYSTAKE_Z + 4)
                for r in refs}
        if any(v is None for v in tips.values()):
            bad.append(f"{name}: no stem")
            continue
        if k["shape"] == "round":
            r = refs[0]
            dz = tips[r] - P.SWITCHES[r]["top_z"] + P.TS_STOP - PRESS_MARGIN
            mv = Pos(0, 0, -dz) * part
            hits = _press_hits(mv, part, obst, cap)
            n += 1
            say(f"  {name:8s} {cap} down {dz:.3f}: " + (", ".join(hits) + "  <-- BINDS" if hits else "clear"))
            if hits:
                bad.append(f"{name} press: " + ", ".join(hits[:3]))
            continue
        cx, cy = k["at"]                                          # D-pad: rock it toward 8 directions
        for phi in range(0, 360, 45):
            u = np.array([math.cos(math.radians(phi)), math.sin(math.radians(phi))])
            pressed = [r for r in refs if np.dot(np.array(P.SWITCHES[r]["at"]) - (cx, cy), u) > 1.0]
            if not pressed:
                continue
            s_l = min(float(np.dot(np.array(P.SWITCHES[r]["at"]) - (cx, cy), u)) for r in pressed)
            d = max(tips[r] - P.SWITCHES[r]["top_z"] + P.TS_STOP - PRESS_MARGIN for r in pressed)
            ob = {o: s_ for o, s_ in _obstacles(parts, ctx, pressed).items() if o != cap}
            rk = rock_solve(part, ob, k, u, s_l, d)
            n += 1
            lab = f"  {name:8s} {cap} toward {'+'.join(pressed):9s}"
            if rk["theta"] is None:
                lb, ub = rk["lo_by"], rk["hi_by"]
                fix = (f"needs {rk['need_dn']:.2f} more under it at the {lb[1]} (s {lb[2]:+.1f}: e.g. a pivot gap) or "
                       f"{rk['need_up']:.2f} more over it at the {ub[1]} (s {ub[2]:+.1f}: e.g. a flange gap)")
                say(f"{lab} JAMMED: the tilt the {lb[1]} forces ({math.degrees(rk['lo']):.2f} deg) is more than the "
                    f"{ub[1]} allows ({math.degrees(rk['hi']):.2f} deg); {fix}  <-- BINDS")
                bad.append(f"D-pad toward {'+'.join(pressed)}: jammed, {fix}")
                continue
            base = Pos(0, 0, rk["t"]) * part.rotate(Axis((cx, cy, k["pivot_z"]), (-u[1], u[0], 0.0)),
                                                    math.degrees(rk["theta"]))
            # the cross is free sideways within its head-in-hole / pivot-in-washer play: the first lateral shift
            # along u that clears everything counts (the z-ray solve fixed the tilt and the height)
            hits, shift = None, None
            for tx in ROCK_SHIFTS:
                mv = Pos(u[0] * tx, u[1] * tx, 0) * base if tx else base
                h = _press_hits(mv, part, ob, cap)
                if hits is None:
                    hits = h
                if not h:
                    hits, shift = [], tx
                    break
            sway = (bb(part)[1][2] - k["pivot_z"]) * math.sin(rk["theta"])
            say(f"{lab} tilt {math.degrees(rk['theta']):.2f} deg (feasible {math.degrees(rk['lo']):.2f}.."
                f"{math.degrees(rk['hi']):.2f}), centre {rk['t']:+.3f}, head sways {sway:.2f} (hole clr "
                f"{k['hole_clr']}): " + (", ".join(hits) + f"  <-- BINDS (at every shift up to "
                                         f"{max(ROCK_SHIFTS):.2f})" if hits else
                                         "clear" + (f" (shifted {shift:+.2f} along the press)" if shift else "")))
            if hits:
                bad.append(f"D-pad toward {'+'.join(pressed)}: " + ", ".join(hits[:3]))
    for group in (P.EDGE_KEYS, P.SHOULDERS):
        for name, e in group.items():
            ax_, sg, plo, phi_, tip = _axis_probe(e)
            probe = tip.copy()
            probe[ax_] += sg * (e["rest_gap"] + 0.3)
            cap = find_control(parts, ctx, probe[:2], (e["axis_z"] - 0.5, e["axis_z"] + 0.5), used, prefer=name)
            if cap is None:
                bad.append(f"{name}: no part")
                say(f"  {name:8s} no part (see (c))")
                continue
            used.add(cap)
            g, _ = edge_gap(parts[cap], e)
            if g is None:
                bad.append(f"{name}: nothing on the axis")
                continue
            d = g + P.SWITCHES[e["switch"]]["travel"] - PRESS_MARGIN
            ux, uy = e["dir"]
            mv = Pos(-ux * d, -uy * d, 0) * parts[cap]
            hits = _press_hits(mv, parts[cap], _obstacles(parts, ctx, [e["switch"]]), cap)
            n += 1
            say(f"  {name:8s} {cap} in {d:.3f} along -dir: " + (", ".join(hits) + "  <-- BINDS" if hits else "clear"))
            if hits:
                bad.append(f"{name} press: " + ", ".join(hits[:3]))
    LAST["j"] = bad
    out.append(("(j) press travel", "FAIL" if bad else "PASS",
                "; ".join(bad[:8]) + (" ..." if len(bad) > 8 else "") if bad else f"{n} presses clear"))


# ================================================================ (x) cross-module cuts
def check_x(parts, ctx, out, fast=False):
    say("\n(x) cross-module cuts")
    bad = []
    for ma, ra in ctx.modules.items():
        for mb, rb in ctx.modules.items():
            if ma == mb:
                continue
            for sh in ("front", "back"):
                for i, c in enumerate(ra[f"{sh}_cut"]):
                    for j, a in enumerate(rb[f"{sh}_add"]):
                        v = vol(c, a)
                        if v > EPS_V or v != v:
                            bad.append(f"{ma} {sh}_cut[{i}] removes {mb} {sh}_add[{j}] {v:.3f}")
                            say(f"  {bad[-1]}  <-- CUT")
    if not bad:
        say(f"  none ({sum(len(r['front_cut']) + len(r['back_cut']) for r in ctx.modules.values())} cuts)")
    LAST["x"] = bad
    out.append(("(x) cross-module cuts", "FAIL" if bad else "PASS", "; ".join(bad) if bad else "none"))


# ================================================================ maps (--maps: PNGs of what the scans measured)
MAPS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "build", "checks")


def maps(parts, ctx, out_dir=MAPS_DIR, step=0.5):
    """build/checks/thickness_map.png (the slab per z ray, the regions with their measured max) and
    build/checks/plate_floor_map.png (the front plate's thickness from the face, the back floor's from the back
    face). Returns the paths."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle
    os.makedirs(out_dir, exist_ok=True)
    o = P.OUTLINE
    xs = np.arange(o.x0 + 0.0123, o.x1, step)
    ys = np.arange(o.y0 + 0.0071, o.y1, step)
    Xg, Yg = np.meshgrid(xs, ys)
    X, Y = Xg.ravel(), Yg.ravel()
    inside = _in_outline(X, Y, 0.02)
    cb = context_bodies(ctx)
    top = np.full(len(X), -np.inf)
    bot = np.full(len(X), np.inf)
    for s_ in [parts["front"]] + [cb[n] for n in ("panel glass", "panel active area", "panel touch", "panel LCM")]:
        r = mesh(s_).cast(2, X, Y)
        np.maximum.at(top, r["r"], r["t1"])
    for s_ in back_side_parts(parts, ctx).values():
        r = mesh(s_).cast(2, X, Y)
        np.minimum.at(bot, r["r"], r["t0"])
    t = np.where(inside & np.isfinite(top) & np.isfinite(bot), top - bot, np.nan).reshape(Xg.shape)
    ext = (o.x0, o.x1, -o.y0, -o.y1)                          # KiCad y down: image row 0 = KiCad y 30 at the top
    paths = []
    fig, ax = plt.subplots(figsize=(15, 7.6), dpi=110)
    im = ax.imshow(t[::-1], extent=(o.x0, o.x1, -o.y0, -o.y1), origin="upper", cmap="viridis",
                   vmin=np.nanmin(t) if np.isfinite(t).any() else 0, vmax=P.TOTAL)
    ax.set_ylim(-o.y0, -o.y1)
    for name, rects, need, model, margin, gov in P.region_table(show=False):
        sel = np.zeros(len(X), bool)
        for rk in rects:
            sel |= _in_rect(X, Y, P.Rect.k(*rk))
        tm = np.nanmax(t.ravel()[sel]) if np.isfinite(t.ravel()[sel]).any() else float("nan")
        for i, rk in enumerate(rects):
            ax.add_patch(Rectangle((rk[0], rk[1]), rk[2] - rk[0], rk[3] - rk[1], fill=False, ec="white", lw=0.8,
                                   ls="--"))
        r0 = rects[0]
        ax.text((r0[0] + r0[2]) / 2, (r0[1] + r0[3]) / 2, f"{name}\n{tm:.2f} (needs {need:.2f})", color="white",
                ha="center", va="center", fontsize=7.5, bbox=dict(fc="black", alpha=0.35, lw=0))
    fig.colorbar(im, ax=ax, fraction=0.025, pad=0.01, label="slab thickness (mm): face / glass top to the back face")
    ax.set_title(f"(f) measured slab thickness [{P.VARIANT}]; architecture total {P.TOTAL:.2f} (KiCad mm)")
    ax.set_xlabel("KiCad x")
    ax.set_ylabel("KiCad y")
    fig.tight_layout()
    pth = os.path.join(out_dir, "thickness_map.png")
    fig.savefig(pth)
    plt.close(fig)
    paths.append(pth)
    # front plate from the face, back floor from the back face
    rf = mesh(parts["front"]).cast(2, X, Y)
    _, last = _ends(rf)
    pt = np.full(len(X), np.nan)
    ok = last >= 0
    li = last[ok]
    face = np.abs(rf["t1"][li] - P.Z_FACE) <= TOL_T
    pt[np.nonzero(ok)[0][face]] = (rf["t1"][li] - rf["t0"][li])[face]
    fl = np.full(len(X), np.nan)
    rb = mesh(parts["back"]).cast(2, X, Y)
    first, _ = _ends(rb)
    ok = first >= 0
    fi = first[ok]
    ba = np.array([P.back_at(x, y) for x, y in zip(X[ok], Y[ok])])
    at_back = np.abs(rb["t0"][fi] - ba) <= TOL_T
    fl[np.nonzero(ok)[0][at_back]] = (rb["t1"][fi] - rb["t0"][fi])[at_back]
    fig, axs = plt.subplots(2, 1, figsize=(14, 13), dpi=110)
    for ax, v, title, vmax in ((axs[0], pt, f"front plate thickness from the face (want >= {P.PLATE}; CN1 >= "
                                f"{P.PLATE_CN1}; fold skin <= {P.FOLD_BAY['skin_max']:.2f}); clipped at 2.0", 2.0),
                               (axs[1], fl, f"back floor thickness from the back face (want >= {P.FLOOR_PR}); "
                                f"clipped at 3.0; blank = window / grille / holes", 3.0)):
        im = ax.imshow(np.clip(v.reshape(Xg.shape), 0, vmax)[::-1], extent=(o.x0, o.x1, -o.y0, -o.y1),
                       origin="upper", cmap="turbo", vmin=0, vmax=vmax)
        ax.set_ylim(-o.y0, -o.y1)
        for rr, lab in ((P.FOLD_BAY["rect"], "fold bay"), (P.CN1["rect"], "CN1"), (P.CELL["rect"], "cell"),
                        (P.POCKET, "pocket")):
            k0, k1, k2, k3 = rr.kicad()
            ax.add_patch(Rectangle((k0, k1), k2 - k0, k3 - k1, fill=False, ec="white", lw=0.7, ls=":"))
            ax.text(k0 + 0.5, k1 + 1.5, lab, color="white", fontsize=7)
        fig.colorbar(im, ax=ax, fraction=0.025, pad=0.01, label="mm")
        ax.set_title(title, fontsize=10)
    fig.suptitle(f"(e) plate / floor ray scan [{P.VARIANT}] (KiCad mm)")
    fig.tight_layout()
    pth = os.path.join(out_dir, "plate_floor_map.png")
    fig.savefig(pth)
    plt.close(fig)
    paths.append(pth)
    return paths


# ================================================================ run
CHECKS = (("a", check_a), ("b", check_b), ("c", check_c), ("d", check_d), ("e", check_e), ("f", check_f),
          ("h", check_h), ("j", check_j), ("x", check_x))
FAST_SKIP = {"h"}        # --fast skips these checks (and (e)'s slab scan)


def run(parts, ctx, fast=False, only=None, verbose=True):
    """[(check, "PASS" | "FAIL", detail)] for every check (only: an iterable of letters)."""
    global VERBOSE
    VERBOSE = verbose
    LAST.clear()
    out = []
    for letter, f in CHECKS:
        if only and letter not in only:
            continue
        if fast and letter in FAST_SKIP:
            say(f"\n({letter}) {f.__name__}: skipped (--fast)")
            continue
        t0 = time.time()
        try:
            f(parts, ctx, out, fast=fast)
        except Exception as ex:
            say(traceback.format_exc())
            out.append((f"({letter}) {f.__name__}", "FAIL", f"raised {ex!r}"))
            LAST[letter] = [f"raised {ex!r}"]
        ctx.timings[f"check {letter}"] = time.time() - t0
    return out


def report(res, ctx=None, t0=None):
    print("\nsummary" + (f" [{P.VARIANT}]" if True else ""))
    for name, verdict, detail in res:
        print(f"  {verdict}  {name}: {detail}")
    fails = sum(v == "FAIL" for _, v, _ in res)
    tim = ""
    if ctx is not None:
        tim = ", ".join(f"{k[6:]} {v:.1f}s" for k, v in ctx.timings.items() if k.startswith("check "))
    print(f"  {len(res) - fails} PASS, {fails} FAIL" + (f" in {time.time() - t0:.1f} s" if t0 else "")
          + (f" (checks: {tim})" if tim else ""))
    return fails


if __name__ == "__main__":
    import enclosure
    t0 = time.time()
    only = None
    if "--only" in sys.argv:
        only = set(sys.argv[sys.argv.index("--only") + 1].replace(",", ""))
    parts, ctx = enclosure.build()
    print(f"built {len(parts)} parts + {len(ctx.context)} context bodies + {len(ctx.keepouts)} keep-outs in "
          f"{time.time() - t0:.1f} s [{P.VARIANT}]")
    res = run(parts, ctx, fast="--fast" in sys.argv, only=only, verbose="--quiet" not in sys.argv)
    fails = report(res, ctx, t0)
    if "--maps" in sys.argv:
        for pth in maps(parts, ctx):
            print(f"wrote {os.path.relpath(pth)}")
    sys.exit(1 if fails else 0)
