#!/usr/bin/env python3
"""export_board.py - gb3: the bridge from the KiCad board to the enclosure CAD. Writes build/board.json.

Run with KiCad's python (needs pcbnew; also calls kicad-cli for the GLB cross-check and the STEP models):
    /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 export_board.py \
        [--board <path to gb3.kicad_pcb or a copy>] [--no-glb] [--keep-glb DIR] [--out build/board.json]

Read-only on the board: pcbnew.LoadBoard writes nothing. kicad-cli is only ever run on a temporary copy (it writes a
.kicad_prl next to the board it opens), with ${KIPRJMOD} replaced by the REPO board directory, so the model paths
resolve the same way whether --board is the repo board or a scratch copy.

Frames (identical to gb2's board.json, so a gb2-style params.py works unchanged):
  xy  board mm, +X right, +Y DOWN as in KiCad. The enclosure frame is (x, -y) (+Y up): gb2's params.flip / P().
  z   board bottom face z = 0, top face (F, the screen side) z = 1.6, +z toward the screen.
      F parts sit on z = 1.6 and grow up; B parts hang from z = 0 and grow down.

board.json
  board     outline (the outer polygon; the cut-outs are notches of it, there are no inner holes) and outline_yup (the
            same, already flipped), bbox, edges (the straight outline runs), notches (the cut-outs as rectangles: cable
            notch, cell + LRA notch, speaker cut-out), holes (H1-H5, M2; gb2 shape {name, x, y, d} + keepout_r),
            other_holes (connector pegs, USB-C shell slots), thickness
  mech      panel (ATM0500D27 outline from User.2 + datasheet stack), flex (tails + folded landings), cell (LP-803860 at
            the notch), speaker, lra (datasheet envelopes; their footprint models are mis-oriented, see model_issues),
            console_outline (the Eco1.User sketch)
  switches  every switch: role, press direction, actuator position / z, travel (datasheet), tip-to-board-edge
  edge_features / connectors   USB-C mouth, jack barrel, microSD slot vs the board edge; CN1 / P1 / FPC1 mating sides
  rule_areas  the board's 'datasheet keep-out' areas (BT830 antenna, M2 hole keep-outs, ...)
  parts     every footprint: ref, value, side ('top'/'bottom'), x, y, rot, courtyard polygon (or a box, and says so),
            hull (XY convex hull of the 3D model), z = [z0, z1] and height (body height off its face, gb2's 'height')
            from its 3D model with the model offset / rotation / scale and the footprint placement applied, or a
            datasheet fallback (height_source says which); ds {nominal, max, src} where a datasheet was checked, then
            height_design / z_design = max(model, datasheet max); model_shift where the model was re-registered; notes
  regions   the tallest F and B parts per enclosure region (left grip, right grip, top strip, centre, notch, jack
            corner, USB-C: a partition by courtyard centre) and under_panel (overlap with the panel outline)
  model_issues / fallbacks   every 3D-model oddity found, every part whose height is not from a usable 3D model

3D models: VRML (.wrl, 0.1 inch units) are transformed here exactly as KiCad's 3D viewer does
(render_3d_opengl.cpp: T(offset) Rz(-rz) Ry(-ry) Rx(-rx) S(scale), then T(pos) Rz(orientation) [Rx(180) if on B]).
STEP models (LRA1, SPK1) come from kicad-cli's GLB export, which also cross-checks every VRML part (they agree to
0.01 mm). A model whose locating pegs / shell legs miss its own footprint holes is shifted onto them (registration).
"""
import os, sys, re, json, math, argparse, shutil, struct, subprocess, tempfile, hashlib, datetime

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
BRD_DIR = os.path.join(REPO, "projects", "gameboy-v3", "pcb", "gb3")        # KIPRJMOD for the model paths
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
T_BOARD = 1.6

ap = argparse.ArgumentParser()
ap.add_argument("--board", default=os.path.join(BRD_DIR, "gb3.kicad_pcb"))
ap.add_argument("--out", default=os.path.join(HERE, "build", "board.json"))
ap.add_argument("--no-glb", action="store_true", help="skip kicad-cli (no STEP models, no cross-check)")
ap.add_argument("--keep-glb", default="", help="keep the GLB (and its temp board) in this directory")
args = ap.parse_args()
if os.path.abspath(args.out).startswith(os.path.join(REPO, "projects", "gameboy-v3", "pcb")):
    sys.exit("refusing to write under projects/gameboy-v3/pcb (KiCad has the board open)")

import pcbnew
TO = pcbnew.ToMM
def log(*a): print(*a, file=sys.stderr); sys.stderr.flush()
r3 = lambda v: round(v, 3)

# ====================================================================== datasheet table
# Body height off the face the part sits on: nominal, max, source (file + page). Used as the FALLBACK when a part has no
# usable 3D model, and as a cross-check of the model (height_design = max(model, ds max)). Parts not listed (0402/0603/
# 0805 passives, small ICs) use their model only: generic EasyEDA chip models (0805 1.3, 0603 0.8, 0402 0.5) are within
# ~0.15 of the MLCC maxima.
DS = [
    (["SW1", "SW2", "SW3", "SW4", "SW5", "SW6", "SW7", "SW8", "SW9", "SW10", "SW11"], 1.50, 1.55,
     "cad/docs/C318884.pdf p1 (TS-1187A, height code A: H 1.5 +/-0.05; plunger D2.0, travel 0.2 +/-0.1 / 0.25)"),
    (["SW12", "SW13", "SW14", "SW15", "SW16"], 3.30, 3.40,
     "cad/docs/C127481.pdf p2 (SKRTLBE010: 4.5 W x 3.4 D x 3.3 H, plunger D2 on an axis 1.7 up, 0.84 out of the body; "
     "no tolerance printed, +0.1 assumed); p1 travel 0.2"),
    (["Audio1"], 5.00, 5.15,
     "cad/docs/C145813.pdf p1 (PJ-327C: body 6.00 W x 5.00 H x 11.60 L + 2.50 barrel, barrel axis 2.50 up, D5.0 boss / "
     "D3.6 hole; 5-10 mm class tolerance +/-0.15)"),
    (["CN1"], 5.50, 5.60,
     "cad/docs/C295747.pdf p3 (S2B-PH-SM4-TB SMT side entry: 5.5 high, B 7.9; no tolerance printed, +0.1 assumed)"),
    (["USBC1"], 3.26, 3.36,
     "cad/docs/C165948.pdf p1 (TYPE-C-31-M-12: shell 8.94 x 7.35 x 3.26, opening 8.34 x 2.56; legs 1.01; ~5 mm "
     "class tolerance +/-0.10)"),
    (["U4"], 1.85, 1.90,
     "cad/docs/C22467599.pdf p1 (A-MicroTF-1.85A: 1.85 +/-0.05, 1.90 over the shell; card sticks out 1.30-5.30 REF)"),
    (["P1"], 2.00, 2.10,
     "cad/docs/C9160.pdf p1 (1013A0 0.5 mm FPC, 2.0 +/-0.1 closed, 3.6 with the actuator open; FPC 0.3 +/-0.03)"),
    (["FPC1"], 1.53, 1.68,
     "cad/docs/C2919568.pdf p1 (1.0K-1.5-nPWB: 1.53 +/-0.15, lid opens to 2.9 REF)"),
    (["U18"], 1.60, 1.75,
     "cad/docs/EZ-DS-BT830_v3_0.pdf p7 (8.5 x 13 x 1.6) and p25 (side view 1.75; module size +/-0.15)"),
    (["U3"], 1.95, 2.16,
     "cad/docs/W25Q128JVSIQ-datasheet.md sec 10.1 (SOIC-8 208-mil: A 1.75 / 1.95 / 2.16)"),
    (["L3"], 2.80, 3.00,
     "cad/docs/C408407.pdf p2 (MWSA0503S: 5.2 x 5.4 x C 2.8 +/-0.2)"),
    (["L1", "L2"], 2.00, 2.20,
     "LCSC C135262 datasheet p2 (SMNR4020: 4.0 x 4.0 x C 2.0 +/-0.2; downloaded 2026-10-15, not in cad/docs)"),
    (["D3"], 2.15, 2.45,
     "LCSC C2481 datasheet p1 (MDD SS16, DO-214AC/SMA side view: height 1.85-2.45; downloaded 2026-10-15, not in "
     "cad/docs)"),
    (["Q1"], 1.00, 1.15,
     "LCSC C8545 datasheet p4 (JSCJ 2N7002, SOT-23 outline: A 0.90-1.15; downloaded 2026-10-15, not in cad/docs)"),
    (["U1"], 1.40, 1.60,
     "cad/docs/T113-S3-datasheet.md line 52 ('eLQFP128, 14 mm x 14 mm x 1.4 mm' body); 1.60 overall is the JEDEC "
     "MS-026 maximum for a 1.40 body (estimate, the package figure is not transcribed)"),
]
DS_BY_REF = {r: (nom, mx, src) for refs, nom, mx, src in DS for r in refs}

# parts with no 3D model at all: what goes in their place (estimates carry an error bar)
NO_MODEL = {
    "H": (0.0, "mounting hole (NPTH), no body; the M2 screw head / insert belongs to the enclosure"),
    "TP": (0.0, "bare 1.5 mm test pad, no body (probe access only)"),
    "J2": (0.8, "estimate +/-0.3: LRA leads (AWG32, ~0.4 OD) soldered flat on 1.6 x 2.4 pads, fillet ~0.3"),
    "J3": (0.8, "estimate +/-0.3: speaker leads (UL1571 32 AWG) soldered flat on 1.6 x 2.4 pads, fillet ~0.3"),
}

# ====================================================================== board
B = pcbnew.LoadBoard(args.board)
board_md5 = hashlib.md5(open(args.board, "rb").read()).hexdigest()
ps = pcbnew.SHAPE_POLY_SET(); B.GetBoardPolygonOutlines(ps, True)
if ps.OutlineCount() != 1: log("WARNING: %d board outlines" % ps.OutlineCount())
o = ps.COutline(0)
OUTLINE = [(r3(TO(o.CPoint(i).x)), r3(TO(o.CPoint(i).y))) for i in range(o.PointCount())]
BX0, BY0 = min(p[0] for p in OUTLINE), min(p[1] for p in OUTLINE)
BX1, BY1 = max(p[0] for p in OUTLINE), max(p[1] for p in OUTLINE)
THICK = TO(B.GetDesignSettings().GetBoardThickness())

def inside(x, y, poly=OUTLINE):
    c = False; n = len(poly)
    for i in range(n):
        (x1, y1), (x2, y2) = poly[i], poly[(i + 1) % n]
        if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1): c = not c
    return c

def seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay; L2 = dx * dx + dy * dy
    t = 0 if L2 == 0 else max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / L2))
    return math.hypot(px - ax - t * dx, py - ay - t * dy)

def edge_dist(x, y, poly=OUTLINE):
    return min(seg_dist(x, y, *poly[i], *poly[(i + 1) % len(poly)]) for i in range(len(poly)))

def ray_to_edge(x, y, ux, uy, poly=OUTLINE):
    """distance from (x, y) along (ux, uy) to the first outline crossing (None if it never crosses)."""
    best = None
    for i in range(len(poly)):
        (ax, ay), (bx, by) = poly[i], poly[(i + 1) % len(poly)]
        ex, ey = bx - ax, by - ay; den = ux * ey - uy * ex
        if abs(den) < 1e-12: continue
        t = ((ax - x) * ey - (ay - y) * ex) / den; s = ((ax - x) * uy - (ay - y) * ux) / den
        if t >= -1e-9 and -1e-9 <= s <= 1 + 1e-9 and (best is None or t < best): best = t
    return best

# straight outline runs (>= 1 mm; the fillets are polygonized in 0.2 mm steps), and the cut-outs as rectangles from them
EDGES = []
for i in range(len(OUTLINE)):
    a, b = OUTLINE[i], OUTLINE[(i + 1) % len(OUTLINE)]
    if math.hypot(b[0] - a[0], b[1] - a[1]) >= 1.0: EDGES.append({"from": a, "to": b, "len": r3(math.hypot(b[0] - a[0], b[1] - a[1]))})
xs = sorted({e["from"][0] for e in EDGES if abs(e["from"][0] - e["to"][0]) < 1e-6} | {BX0, BX1})
ys = sorted({e["from"][1] for e in EDGES if abs(e["from"][1] - e["to"][1]) < 1e-6} | {BY0, BY1})
cols = []
for xa, xb in zip(xs, xs[1:]):
    run = None
    for ya, yb in zip(ys, ys[1:]):
        if not inside((xa + xb) / 2, (ya + yb) / 2):
            run = [xa, run[1] if run else ya, xb, yb]
        elif run: cols.append(run); run = None
    if run: cols.append(run)
NOTCH_RECTS = [[r3(v) for v in c] for c in cols]

# ---------------------------------------------------------------------- drawings: panel, cell, transducers, flex, console
def layer_items(name):
    lid = B.GetLayerID(name); return [d for d in B.GetDrawings() if d.GetLayer() == lid]
def rect_of(d): s, e = d.GetStart(), d.GetEnd(); return [r3(min(TO(s.x), TO(e.x))), r3(min(TO(s.y), TO(e.y))), r3(max(TO(s.x), TO(e.x))), r3(max(TO(s.y), TO(e.y)))]
def circ_of(d): s, e = d.GetStart(), d.GetEnd(); return (r3(TO(s.x)), r3(TO(s.y)), r3(math.hypot(TO(e.x - s.x), TO(e.y - s.y))))
def segs(name):
    return [(r3(TO(d.GetStart().x)), r3(TO(d.GetStart().y)), r3(TO(d.GetEnd().x)), r3(TO(d.GetEnd().y)))
            for d in layer_items(name) if d.GetClass() == "PCB_SHAPE" and d.GetShape() == pcbnew.SHAPE_T_SEGMENT]
def seg_bbox(ss):
    return [min(min(s[0], s[2]) for s in ss), min(min(s[1], s[3]) for s in ss), max(max(s[0], s[2]) for s in ss), max(max(s[1], s[3]) for s in ss)] if ss else None

panel_rects = [rect_of(d) for d in layer_items("User.2") if d.GetClass() == "PCB_SHAPE" and d.GetShape() == pcbnew.SHAPE_T_RECT]
dw_rects = [rect_of(d) for d in layer_items("Dwgs.User") if d.GetClass() == "PCB_SHAPE" and d.GetShape() == pcbnew.SHAPE_T_RECT]
dw_circs = [circ_of(d) for d in layer_items("Dwgs.User") if d.GetClass() == "PCB_SHAPE" and d.GetShape() == pcbnew.SHAPE_T_CIRCLE]
cell_body = next((r for r in dw_rects if abs((r[2] - r[0]) - 38.5) < 0.2 and abs((r[3] - r[1]) - 62.0) < 0.2), None)
cell_buf = next((r for r in dw_rects if abs((r[2] - r[0]) - 42.5) < 0.2 and abs((r[3] - r[1]) - 65.5) < 0.3), None)
console = seg_bbox(segs("Eco1.User"))
landings = seg_bbox(segs("Cmts.User"))

# ====================================================================== 3D models
WRL_CACHE = {}
def wrl_points(path):
    """all Coordinate points of a VRML file, in mm (KiCad's VRML unit is 0.1 inch)."""
    if path not in WRL_CACHE:
        t = open(path, errors="ignore").read(); pts = []
        for blk in re.findall(r"point\s*\[([^\]]*)\]", t):
            v = [float(q) for q in re.split(r"[\s,]+", blk.strip()) if q]
            pts += [(v[i] * 2.54, v[i + 1] * 2.54, v[i + 2] * 2.54) for i in range(0, len(v) - 2, 3)]
        WRL_CACHE[path] = sorted(set(pts))
    return WRL_CACHE[path]

def resolve(fn):
    p = fn.replace("${KIPRJMOD}", BRD_DIR)
    for k, v in os.environ.items(): p = p.replace("${%s}" % k, v)
    if not os.path.isabs(p): p = os.path.join(BRD_DIR, p)
    return os.path.normpath(p)

def mmul(A, Bm): return [[sum(A[i][k] * Bm[k][j] for k in range(4)) for j in range(4)] for i in range(4)]
def rx(a): c, s = math.cos(a), math.sin(a); return [[1, 0, 0, 0], [0, c, -s, 0], [0, s, c, 0], [0, 0, 0, 1]]
def ry(a): c, s = math.cos(a), math.sin(a); return [[c, 0, s, 0], [0, 1, 0, 0], [-s, 0, c, 0], [0, 0, 0, 1]]
def rz(a): c, s = math.cos(a), math.sin(a); return [[c, -s, 0, 0], [s, c, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]
def tr(x, y, z): return [[1, 0, 0, x], [0, 1, 0, y], [0, 0, 1, z], [0, 0, 0, 1]]
def sc(x, y, z): return [[x, 0, 0, 0], [0, y, 0, 0], [0, 0, z, 0], [0, 0, 0, 1]]

def model_matrix(fp, m):
    """model mm -> world (x, -y_kicad, z in the enclosure frame), as KiCad's 3D viewer composes it."""
    off, rot, s = m.m_Offset, m.m_Rotation, m.m_Scale
    M = mmul(mmul(mmul(mmul(tr(off.x, off.y, off.z), rz(math.radians(-rot.z))), ry(math.radians(-rot.y))),
                  rx(math.radians(-rot.x))), sc(s.x, s.y, s.z))
    p = fp.GetPosition(); back = fp.IsFlipped()
    F = mmul(tr(TO(p.x), -TO(p.y), 0.0 if back else T_BOARD), rz(math.radians(fp.GetOrientationDegrees())))
    if back: F = mmul(F, rx(math.pi))
    return mmul(F, M)

def apply(M, pts):
    a, b, c, d = M[0]; e, f, g, h = M[1]; i, j, k, l = M[2]
    return [(a * x + b * y + c * z + d, -(e * x + f * y + g * z + h), i * x + j * y + k * z + l) for x, y, z in pts]

# ---------------------------------------------------------------------- kicad-cli GLB (STEP models + cross-check)
GLB, GLB_WARN = {}, []
def glb_export():
    tmp = tempfile.mkdtemp(prefix="gb3-board-glb-")
    try:
        txt = open(args.board, encoding="utf-8").read().replace("${KIPRJMOD}", BRD_DIR)
        brd = os.path.join(tmp, "gb3.kicad_pcb"); open(brd, "w", encoding="utf-8").write(txt)
        out = os.path.join(tmp, "gb3.glb")
        r = subprocess.run([KICAD_CLI, "pcb", "export", "glb", "-f", "-o", out, brd], capture_output=True, text=True, timeout=600)
        GLB_WARN.extend(l.strip() for l in (r.stdout + r.stderr).splitlines() if "Could not add" in l or "not found" in l.lower())
        if not os.path.exists(out): log("kicad-cli GLB export failed:", r.stdout[-400:], r.stderr[-400:]); return
        f = open(out, "rb").read()
        l0 = struct.unpack("<I", f[12:16])[0]; js = json.loads(f[20:20 + l0])
        q = 20 + l0; l1 = struct.unpack("<I", f[q:q + 4])[0]; BIN = f[q + 8:q + 8 + l1]
        def local(n):
            if "matrix" in n:
                m = n["matrix"]; return [[m[0], m[4], m[8], m[12]], [m[1], m[5], m[9], m[13]], [m[2], m[6], m[10], m[14]], [0, 0, 0, 1]]
            x, y, z, w = n.get("rotation", [0, 0, 0, 1]); s = n.get("scale", [1, 1, 1]); t = n.get("translation", [0, 0, 0])
            R = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)], [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                 [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
            return [[R[i][0] * s[0], R[i][1] * s[1], R[i][2] * s[2], t[i]] for i in range(3)] + [[0, 0, 0, 1]]
        def positions(ai):
            a = js["accessors"][ai]; bv = js["bufferViews"][a["bufferView"]]; n = a["count"]
            off = bv.get("byteOffset", 0) + a.get("byteOffset", 0); st = bv.get("byteStride", 12)
            if st == 12: v = struct.unpack_from("<%df" % (3 * n), BIN, off); return [v[i:i + 3] for i in range(0, 3 * n, 3)]
            return [struct.unpack_from("<3f", BIN, off + i * st) for i in range(n)]
        def walk(ni, M, out, name):
            n = js["nodes"][ni]; M = mmul(M, local(n))
            if "mesh" in n:
                for pr in js["meshes"][n["mesh"]]["primitives"]:
                    a, b, c, d = M[0]; e, ff, g, h = M[1]; i, j, k, l = M[2]
                    out.setdefault(name, []).extend(((a * x + b * y + c * z + d) * 1e3, (i * x + j * y + k * z + l) * 1e3,
                                                      (e * x + ff * y + g * z + h) * 1e3) for x, y, z in positions(pr["attributes"]["POSITION"]))
            for ch in n.get("children", []): walk(ch, M, out, js["nodes"][ch].get("name") or name)
        I4 = [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]
        for rt in js["scenes"][js.get("scene", 0)]["nodes"]:
            R0 = local(js["nodes"][rt])
            for ch in js["nodes"][rt].get("children", []):
                nd = js["nodes"][ch]; ref = nd.get("name", "")
                if not ref or ref.startswith("=>"): continue                       # the board body
                seat = nd.get("translation", [0, 0, 0])[1] * 1e3                   # GLB z of the model's z = 0 plane
                out = {}; walk(ch, mmul(I4, R0), out, ref)
                fp = B.FindFootprintByReference(ref)
                if fp is None: continue
                base = 0.0 if fp.IsFlipped() else T_BOARD                          # GLB stack offsets -> nominal faces
                GLB[ref] = {"seat_glb": seat, "subs": {k: [(x, y, base + z - seat) for x, y, z in v] for k, v in out.items()}}
        log("GLB: %d parts with models from kicad-cli%s" % (len(GLB), ("; " + "; ".join(GLB_WARN)) if GLB_WARN else ""))
    finally:
        if args.keep_glb:
            os.makedirs(args.keep_glb, exist_ok=True)
            for fn in os.listdir(tmp): shutil.copy(os.path.join(tmp, fn), args.keep_glb)
        shutil.rmtree(tmp, ignore_errors=True)
if not args.no_glb: glb_export()

# ====================================================================== parts
def bbox3(pts): return [min(p[0] for p in pts), min(p[1] for p in pts), min(p[2] for p in pts), max(p[0] for p in pts), max(p[1] for p in pts), max(p[2] for p in pts)]

def hull(pts):
    P = sorted(set((round(x, 3), round(y, 3)) for x, y, _ in pts))
    if len(P) < 3: return P
    def cross(o_, a, b): return (a[0] - o_[0]) * (b[1] - o_[1]) - (a[1] - o_[1]) * (b[0] - o_[0])
    lo, up = [], []
    for p in P:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], p) <= 0: lo.pop()
        lo.append(p)
    for p in reversed(P):
        while len(up) >= 2 and cross(up[-2], up[-1], p) <= 0: up.pop()
        up.append(p)
    return lo[:-1] + up[:-1]

def clusters(pts, cell=0.5):
    cells = {}
    for p in pts: cells.setdefault((round(p[0] / cell), round(p[1] / cell)), []).append(p)
    seen, out = set(), []
    for k in cells:
        if k in seen: continue
        st, comp = [k], []; seen.add(k)
        while st:
            c = st.pop(); comp += cells[c]
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    n = (c[0] + dx, c[1] + dy)
                    if n in cells and n not in seen: seen.add(n); st.append(n)
        x0, y0, x1, y1 = min(p[0] for p in comp), min(p[1] for p in comp), max(p[0] for p in comp), max(p[1] for p in comp)
        out.append(((x0 + x1) / 2, (y0 + y1) / 2))
    return out

def registration(pts, holes, back):
    """the XY shift that puts the model's in-board features (pegs, shell legs) onto the footprint's holes."""
    inb = [p for p in pts if (p[2] > 0.05 if back else p[2] < T_BOARD - 0.05)]
    if not inb or not holes: return None
    cs = clusters(inb)
    best = None
    for cx, cy in cs:
        for hx, hy, _ in holes:
            dx, dy = hx - cx, hy - cy; pairs = []
            for qx, qy in cs:
                h = min(holes, key=lambda hh: math.hypot(qx + dx - hh[0], qy + dy - hh[1]))
                if math.hypot(qx + dx - h[0], qy + dy - h[1]) <= 0.35: pairs.append((h[0] - qx, h[1] - qy))
            key = (len(pairs), -math.hypot(dx, dy))
            if best is None or key > best[0]: best = (key, pairs)
    n, pairs = best[0][0], best[1]
    if n == 0: return {"clusters": len(cs), "holes": len(holes), "matched": 0}
    mx, my = sum(p[0] for p in pairs) / n, sum(p[1] for p in pairs) / n
    spread = max(math.hypot(p[0] - mx, p[1] - my) for p in pairs)
    return {"clusters": len(cs), "holes": len(holes), "matched": n, "shift": [r3(mx), r3(my)], "spread": r3(spread)}

def pad_hits(fp, pts):
    low = [p for p in pts if (-0.3 <= p[2] <= 0.05) or (T_BOARD - 0.05 <= p[2] <= T_BOARD + 0.3)]
    pads = [pd for pd in fp.Pads() if pd.GetAttribute() != pcbnew.PAD_ATTRIB_NPTH]
    hit = 0
    for pd in pads:
        bb = pd.GetBoundingBox(); l, t, r, b = TO(bb.GetLeft()) - 0.05, TO(bb.GetTop()) - 0.05, TO(bb.GetRight()) + 0.05, TO(bb.GetBottom()) + 0.05
        if any(l <= p[0] <= r and t <= p[1] <= b for p in low): hit += 1
    return hit, len(pads)

parts, model_issues, fallbacks = [], [], []
for fp in sorted(B.GetFootprints(), key=lambda f: (re.sub(r"\d", "", f.GetReference()), int(re.sub(r"\D", "", f.GetReference()) or 0))):
    ref = fp.GetReference(); back = fp.IsFlipped(); q = fp.GetPosition(); attr = fp.GetAttributes()
    side = "bottom" if back else "top"
    pads = list(fp.Pads())
    holes = []
    for pd in pads:
        if pd.HasHole():
            pp, ds_ = pd.GetPosition(), pd.GetDrillSize()
            holes.append((TO(pp.x), TO(pp.y), (TO(ds_.x) + TO(ds_.y)) / 2))
    nets = sorted({pd.GetNetname() for pd in pads if pd.GetNetname() and "GND" not in pd.GetNetname().upper() and not pd.GetNetname().startswith("unconnected")})
    # courtyard
    cy = fp.GetCourtyard(pcbnew.B_CrtYd if back else pcbnew.F_CrtYd)
    crt, crt_src = None, None
    if cy.OutlineCount():
        k = max(range(cy.OutlineCount()), key=lambda i: abs(cy.COutline(i).Area()))
        oc = cy.COutline(k); crt = [(r3(TO(oc.CPoint(i).x)), r3(TO(oc.CPoint(i).y))) for i in range(oc.PointCount())]
        crt_src = "B.CrtYd" if back else "F.CrtYd"
    # 3D models
    rec = {"ref": ref, "value": fp.GetValue(), "footprint": fp.GetFPIDAsString(), "side": side, "layer": "B" if back else "F",
           "x": r3(TO(q.x)), "y": r3(TO(q.y)), "rot": round(fp.GetOrientationDegrees(), 2),
           "tht": bool(attr & pcbnew.FP_THROUGH_HOLE), "smd": bool(attr & pcbnew.FP_SMD),
           "board_only": bool(attr & pcbnew.FP_BOARD_ONLY), "nets": nets[:6], "notes": []}
    models, pts, own, files = [], [], [], []
    for m in fp.Models():
        if not m.m_Show: continue
        path = resolve(m.m_Filename); ext = os.path.splitext(path)[1].lower()
        mi = {"file": os.path.relpath(path, REPO) if path.startswith(REPO) else path, "found": os.path.exists(path),
              "offset": [r3(m.m_Offset.x), r3(m.m_Offset.y), r3(m.m_Offset.z)],
              "rotate": [r3(m.m_Rotation.x), r3(m.m_Rotation.y), r3(m.m_Rotation.z)],
              "scale": [r3(m.m_Scale.x), r3(m.m_Scale.y), r3(m.m_Scale.z)]}
        models.append(mi); files.append(os.path.basename(path))
        if mi["found"] and ext in (".wrl",):
            own += apply(model_matrix(fp, m), wrl_points(path))
    rec["models"] = models
    glb = GLB.get(ref)
    glb_pts = [p for v in glb["subs"].values() for p in v] if glb else []
    if own: pts, src = own, "vrml"
    elif glb_pts: pts, src = glb_pts, "glb"
    else: pts, src = [], None
    if own and glb_pts:                                      # cross-check our transform against KiCad's exporter
        a, b = bbox3(own), bbox3(glb_pts); d = max(abs(u - v) for u, v in zip(a, b))
        rec["glb_check"] = "agree %.3f" % d if d <= 0.02 else "DIFFER %.3f" % d
        if d > 0.02: model_issues.append({"ref": ref, "issue": "own VRML transform and KiCad's GLB export differ by %.3f mm" % d})
    elif models and not pts:
        rec["notes"].append("3D model file not found: " + ", ".join(m_["file"] for m_ in models if not m_["found"]))
    # registration against the footprint's own holes
    shift = None
    if pts and holes:
        rg = registration(pts, holes, back)
        if rg: rec["registration"] = rg
        if rg and rg.get("matched", 0) >= 2 and rg["spread"] <= 0.1 and math.hypot(*rg["shift"]) > 0.15:
            shift = rg["shift"]; pts = [(x + shift[0], y + shift[1], z) for x, y, z in pts]
            rec["model_shift"] = shift
            model_issues.append({"ref": ref, "issue": "3D model mis-registered: its %d in-board features (pegs / shell legs) sit "
                                 "(%+.2f, %+.2f) mm off the footprint's own holes; exported shifted onto the holes (the KiCad 3D "
                                 "view shows the unshifted model)" % (rg["matched"], -shift[0], -shift[1])})
    if pts:
        h_, n_ = pad_hits(fp, pts); rec["pad_hits"] = "%d/%d" % (h_, n_)
    # ---------------------------------------------------------- z range / height
    dsr = DS_BY_REF.get(ref)
    if ref in ("LRA1", "SPK1"): pts_used = []                  # mis-oriented models: datasheet envelope (mech section)
    else: pts_used = pts
    if pts_used:
        bb = bbox3(pts_used)
        z0, z1 = bb[2], bb[5]
        h = (z1 - T_BOARD) if not back else (0.0 - z0)
        rec["z"] = [r3(z0), r3(z1)]
        rec["height"] = r3(h)
        rec["height_source"] = "3D model (%s, %s)" % (", ".join(files), "own VRML transform" if src == "vrml" else "kicad-cli GLB")
        rec["model_bbox"] = [r3(bb[0]), r3(bb[1]), r3(bb[3]), r3(bb[4])]
        if back and z1 > T_BOARD + 0.01: rec["notes"].append("model pokes %.2f above the F face (through the board)" % (z1 - T_BOARD))
        if not back and z0 < -0.01: rec["notes"].append("model pokes %.2f below the B face (through the board)" % (-z0))
        into = (z1 - 0.0) if back else (T_BOARD - z0)
        if into > 0.05: rec["into_board"] = r3(into)                 # pegs / legs into the board (informational)
        # model height named in the file (EasyEDA '-H1.65' etc.) vs measured
        mh = re.search(r"[-_]H(\d+(?:\.\d+)?)", files[0]) if files else None
        if mh and abs(float(mh.group(1)) - h) > 0.15:
            rec["notes"].append("model file says H%s, model measures %.2f" % (mh.group(1), h))
    else:
        pref = re.sub(r"\d", "", ref)
        if dsr:
            nom, mx, s_ = dsr; h = mx
            rec["height_source"] = "datasheet fallback (max): " + s_
            why = "3D model file missing (%s)" % ", ".join(m_["file"] for m_ in models) if models else "no 3D model"
            fallbacks.append({"ref": ref, "height": h, "why": why, "source": s_})
        elif ref in ("LRA1", "SPK1"):
            h = 3.53 if ref == "LRA1" else 4.9
            rec["height_source"] = "datasheet envelope (mech.%s); the footprint's STEP model is mis-oriented" % ("lra" if ref == "LRA1" else "speaker")
            rec["z_free"] = True
            rec["notes"].append("in a board cut-out: z is the enclosure's choice; z here = hanging from the board plane like a B part")
            fallbacks.append({"ref": ref, "height": h, "why": "3D model mis-oriented (see model_issues)", "source": rec["height_source"]})
        elif models:                                       # a model is named but unreadable and there is no datasheet entry
            h = 1.0
            rec["height_source"] = "MISSING 3D model file and no datasheet entry: 1.0 is a placeholder, add a DS row"
            fallbacks.append({"ref": ref, "height": h, "why": "3D model file missing, no datasheet entry", "source": "placeholder"})
            log("WARNING %s: 3D model file missing (%s) and no datasheet entry" % (ref, ", ".join(m_["file"] for m_ in models)))
        else:
            h, why = NO_MODEL.get(ref, NO_MODEL.get(pref, (0.0, "no 3D model")))
            rec["height_source"] = "no 3D model: " + why
            fallbacks.append({"ref": ref, "height": h, "why": "no 3D model", "source": why})
        rec["height"] = r3(h)
        rec["z"] = [r3(-h), 0.0] if back else [T_BOARD, r3(T_BOARD + h)]
    if dsr:
        nom, mx, s_ = dsr
        rec["ds"] = {"nominal": nom, "max": mx, "src": s_}
        rec["height_design"] = r3(max(rec["height"], mx))
        if mx > rec["height"] + 0.05 and pts_used:
            rec["notes"].append("datasheet max %.2f exceeds the model's %.2f by %.2f (use height_design)" % (mx, rec["height"], mx - rec["height"]))
            if mx > rec["height"] + 0.2:
                model_issues.append({"ref": ref, "issue": "3D model %.2f tall but the datasheet allows %.2f (+%.2f): %s"
                                     % (rec["height"], mx, mx - rec["height"], s_)})
    else:
        rec["height_design"] = rec["height"]
    rec["z_design"] = [r3(-rec["height_design"]), 0.0] if back else [T_BOARD, r3(T_BOARD + rec["height_design"])]
    if crt is None:                                            # no courtyard: model box, else pad box (and say so)
        if rec.get("model_bbox"):
            x0, y0, x1, y1 = rec["model_bbox"]; crt_src = "model bbox (no courtyard)"
        elif pads:
            bbs = [pd.GetBoundingBox() for pd in pads]
            x0, y0 = min(TO(b.GetLeft()) for b in bbs) - 0.25, min(TO(b.GetTop()) for b in bbs) - 0.25
            x1, y1 = max(TO(b.GetRight()) for b in bbs) + 0.25, max(TO(b.GetBottom()) for b in bbs) + 0.25; crt_src = "pad bbox + 0.25 (no courtyard)"
        else:
            x0 = y0 = x1 = y1 = None
        if ref in ("LRA1", "SPK1"):
            R = 4.0 if ref == "LRA1" else 13.5
            crt = [(r3(TO(q.x) + R * math.cos(2 * math.pi * i / 48)), r3(TO(q.y) + R * math.sin(2 * math.pi * i / 48))) for i in range(48)]
            crt_src = "datasheet circle D%.0f at the footprint (no courtyard)" % (2 * R)
        elif x0 is not None:
            crt = [(r3(x0), r3(y0)), (r3(x1), r3(y0)), (r3(x1), r3(y1)), (r3(x0), r3(y1))]
    rec["courtyard"] = crt; rec["courtyard_source"] = crt_src
    # overhang past the board outline (convex hull of the model)
    if pts_used:
        out = [(x, y) for x, y in hull(pts_used) if not inside(x, y)]
        if out:
            d = max(edge_dist(x, y) for x, y in out)
            if d > 0.05: rec["overhang"] = r3(d)
    if pts_used: rec["hull"] = [(r3(x), r3(y)) for x, y in hull(pts_used)]
    rec["_pts"] = pts_used if ref.startswith("SW") or ref in ("USBC1", "Audio1", "U4", "CN1") else None
    rec["_glb_subs"] = {k: bbox3(v) for k, v in glb["subs"].items()} if glb and ref in ("LRA1", "SPK1") else None
    parts.append(rec)

P = {p["ref"]: p for p in parts}

# ---------------------------------------------------------------------- LRA1 / SPK1: the models as KiCad places them
for ref in ("LRA1", "SPK1"):
    p = P.get(ref)
    if not p or not p["_glb_subs"]: continue
    bb_all = bbox3([(b[0], b[1], b[2]) for b in p["_glb_subs"].values()] + [(b[3], b[4], b[5]) for b in p["_glb_subs"].values()])
    p["model_as_placed"] = {"bbox": [r3(v) for v in bb_all], "solids": {k: [r3(v) for v in b] for k, b in p["_glb_subs"].items()}}
    msg =("%s: the STEP model is mis-oriented and off-centre: as placed it spans x %.1f..%.1f, y %.1f..%.1f, z %.1f..%.1f "
           "(the part is a D%s disc at (%.1f, %.1f) whose axis should be z)" % (ref, bb_all[0], bb_all[3], bb_all[1], bb_all[4],
           bb_all[2], bb_all[5], "8" if ref == "LRA1" else "27", p["x"], p["y"]))
    if ref == "LRA1":
        leads = [b for k, b in p["_glb_subs"].items() if b[4] - b[1] > 50]
        if leads:
            msg += ("; it includes two %.0f mm lead wires that run to y %.1f, %.1f mm past the board's top edge (y %.1f)"
                    % (leads[0][4] - leads[0][1], min(b[1] for b in leads), BY0 - min(b[1] for b in leads), BY0))
    model_issues.append({"ref": ref, "issue": msg})

# ====================================================================== switches (actuator positions)
ROLE = {"SW1": "RESET", "SW2": "D-pad LEFT", "SW3": "D-pad RIGHT", "SW4": "D-pad UP", "SW5": "D-pad DOWN", "SW6": "Y",
        "SW7": "A", "SW8": "X", "SW9": "B", "SW10": "START", "SW11": "SELECT", "SW12": "VOL+", "SW13": "VOL-",
        "SW14": "POWER (/PB)", "SW15": "L", "SW16": "R"}
switches = []
for p in parts:
    if not p["ref"].startswith("SW"): continue
    pts = p["_pts"] or []
    s = {"ref": p["ref"], "role": ROLE.get(p["ref"]), "nets": p["nets"], "value": p["value"], "side": p["side"], "x": p["x"], "y": p["y"]}
    if p["value"].startswith("TS-1187A"):
        top = max(pts, key=lambda t: abs(t[2] - (0 if p["side"] == "bottom" else T_BOARD))) if pts else None
        ext = abs(top[2] - (0 if p["side"] == "bottom" else T_BOARD)) if top else None
        stem = [t for t in pts if abs(abs(t[2] - (0 if p["side"] == "bottom" else T_BOARD)) - ext) < 0.02] if pts else []
        sx = (min(t[0] for t in stem) + max(t[0] for t in stem)) / 2 if stem else p["x"]
        sy = (min(t[1] for t in stem) + max(t[1] for t in stem)) / 2 if stem else p["y"]
        s.update({"kind": "TS-1187A top push (5.1 x 5.1)", "press": [0, 0, 1 if p["side"] == "bottom" else -1],
                  "actuator_xy": [r3(sx), r3(sy)], "actuator_d": 2.0,
                  "top_z_ds": r3(T_BOARD + 1.50) if p["side"] == "top" else -1.50, "top_z_ds_max": r3(T_BOARD + 1.55) if p["side"] == "top" else -1.55,
                  "top_z_model": r3(p["z"][1] if p["side"] == "top" else p["z"][0]), "travel": 0.25,
                  "src": "cad/docs/C318884.pdf p1 (H 1.5 +/-0.05, D2.0, travel 0.2 +/-0.1 drawing / 0.25 spec)"})
    else:   # ALPS SKRT side push: plunger along +y (KiCad) at rot 0 (place.py; checked against the model below)
        th = math.radians(p["rot"]); ux, uy = math.sin(th), math.cos(th)     # screen-CCW rotation of (0, +1)
        tipd = max(t[0] * ux + t[1] * uy for t in pts)                       # the plunger end face, along the axis
        tipv = [t for t in pts if t[0] * ux + t[1] * uy > tipd - 0.25]       # the plunger's end (within 0.25 of it)
        az = (min(t[2] for t in tipv) + max(t[2] for t in tipv)) / 2
        nx, ny = -uy, ux                                                     # across the axis
        sp = (min(t[0] * nx + t[1] * ny for t in tipv) + max(t[0] * nx + t[1] * ny for t in tipv)) / 2
        tx, ty = tipd * ux + sp * nx, tipd * uy + sp * ny                    # end-face centre on the press axis
        d_edge = ray_to_edge(tx, ty, ux, uy)
        s.update({"kind": "ALPS SKRTLBE010 side push (4.5 x 3.4 x 3.3)", "press": [r3(-ux), r3(-uy), 0], "out_dir": [r3(ux), r3(uy)],
                  "tip_xy": [r3(tx), r3(ty)], "axis_z_model": r3(az), "axis_z_ds": r3(T_BOARD + 1.7), "plunger_d": 2.0,
                  "plunger_out_of_body": 0.84, "travel": 0.2, "tip_to_board_edge": r3(d_edge) if d_edge is not None else None,
                  "src": "cad/docs/C127481.pdf p2 (plunger D2, axis 1.7 above the board, 0.84 out of the 2.56 body); p1 travel 0.2, 1.6 N"})
    switches.append(s)

# ====================================================================== edge features (where connectors meet the board edge)
edge_features = {}
if P.get("USBC1", {}).get("model_bbox"):
    x0, y0, x1, y1 = P["USBC1"]["model_bbox"]
    edge_features["usb_c"] = {"ref": "USBC1", "edge": "top (y %.1f)" % BY0, "mouth_y": y0, "past_edge": r3(BY0 - y0), "x_centre": r3((x0 + x1) / 2),
                              "shell_w": r3(x1 - x0), "z": P["USBC1"]["z_design"], "axis_z": r3(-3.26 / 2), "opening": [8.34, 2.56],
                              "src": "corrected 3D model + cad/docs/C165948.pdf p1 (shell 8.94 x 3.26, opening 8.34 x 2.56; axis assumed centred in the shell)"}
if P.get("Audio1", {}).get("model_bbox"):
    x0, y0, x1, y1 = P["Audio1"]["model_bbox"]
    edge_features["jack"] = {"ref": "Audio1", "edge": "bottom (y %.1f)" % BY1, "barrel_tip_y": y1, "past_edge": r3(y1 - BY1),
                             "x_axis": r3(P["Audio1"]["x"]), "axis_z": r3(T_BOARD + 2.5), "boss_d": 5.0, "hole_d": 3.6,
                             "src": "corrected 3D model + cad/docs/C145813.pdf p1 (barrel D5.0 boss 2.50 long, D3.6 hole, axis 2.50 above the board)"}
if P.get("U4", {}).get("model_bbox"):
    x0, y0, x1, y1 = P["U4"]["model_bbox"]
    edge_features["microsd"] = {"ref": "U4", "edge": "top (y %.1f)" % BY0, "slot_face_y": y0, "inside_edge": r3(y0 - BY0), "x_centre": r3((x0 + x1) / 2),
                                "z": P["U4"]["z_design"], "card_out": "1.30-5.30 REF past the socket face (latched .. ejected)",
                                "src": "3D model + cad/docs/C22467599.pdf p1"}

# ====================================================================== mech (context bodies)
pan = panel_rects[0] if panel_rects else [57.0, 37.0, 177.7, 112.8]
mech = {
    "panel": {"rect": pan, "size": [r3(pan[2] - pan[0]), r3(pan[3] - pan[1])], "thickness": 4.25, "thickness_tol": 0.3,
              "lcd_thickness": 3.0, "lcd_tol": 0.2, "tp_window": [108.8, 66.2], "active_area": [108.0, 64.8],
              "rect_src": "board User.2 rect; place.py comment 'Screen outline is X[57..177.7], Y[37..112.8]'",
              "src": "cad/docs/ATM0500D27-CT.pdf p17 (mechanical drawing: 120.7 +/-0.3 x 75.8 +/-0.3 LCM, *4.25 +/-0.3 total, "
                     "*3.0 +/-0.2 LCM, TP window 108.8 x 66.2, A.A. 108.0 x 64.8) and p4 item 8 (120.7 x 75.8 x 4.25)",
              "z": "not fixed by the board: its back must clear the F parts under it and the folded display flex (regions.centre)"},
    "flex": {"display_tail_from_panel_left": 26.4, "touch_tail_from_panel_right": 55.8, "tail_len": 53.2, "tail_len_tol": 0.5,
             "fpc_t": 0.3, "stiffener_t": 0.3, "component_area_h_max": 1.2,
             "landings_bbox": landings,       # User.4 also carries the hand sketch of the cut-outs: keep the tail (below the panel)
             "unfolded_display": seg_bbox([g for g in segs("User.4") if min(g[1], g[3]) >= pan[3] - 0.01]),
             "unfolded_touch": seg_bbox([g for g in segs("User.3") if min(g[1], g[3]) >= pan[3] - 0.01]),
             "src": "cad/docs/ATM0500D27-CT.pdf p17 (tails 53.2 +/-0.5, component area H 1.2 max, FPC 0.3 +/-0.05, stiffener 0.3 +/-0.03); "
                    "board Cmts.User (fold_cables.py landings), User.3 / User.4 (unfolded tails); place.py comments",
             "note": "both tails exit the panel's bottom edge (y %.1f) and fold behind it: display onto P1 (F side), touch round the "
                     "board's bottom edge (cable notch) onto FPC1 (B side)" % pan[3]},
    "cell": {"rect": cell_body, "buffer": cell_buf, "size": [38.5, 62.0], "t": 8.5, "z": "free: it sits in the through-notch",
             "leads": "UL1007 24 AWG red/black, 100 mm, out of the PCM short end; mate CN1 (JST PH) at (%.1f, %.1f)" % (P["CN1"]["x"], P["CN1"]["y"]) if "CN1" in P else "",
             "src": "cad/docs/LiIon2000mAh37V.pdf p5 (max 62 x 38.5 x 8.5, leads 100); BOM.md #73; board Dwgs.User rects (place.py BAT_OUTLINE / BAT_BUFFER)"},
    "speaker": {"x": P["SPK1"]["x"] if "SPK1" in P else None, "y": P["SPK1"]["y"] if "SPK1" in P else None, "d": 27.0, "d_tol": 0.2, "t": 4.9, "t_tol": 0.2,
                "front_lip_d": 23.4, "buffer_r": 15.0, "leads": "UL1571 32 AWG, 50 +/-3 mm, from the rim; solder pads J3",
                "z": "free: it sits in the speaker cut-out (place.py: back-firing through a back-cover grille)",
                "src": "cad/docs/ces-2704-088l050.pdf p2 (D27 +/-0.2 x 4.9 +/-0.2, lip D23.4 +/-0.1, leads 50 +/-3); board Dwgs.User circles r 13.5 / 15.0"},
    "lra": {"x": P["LRA1"]["x"] if "LRA1" in P else None, "y": P["LRA1"]["y"] if "LRA1" in P else None, "d": 8.0, "t": 3.25, "t_with_adhesive_model": 3.53,
            "buffer_r": 5.0, "leads": "AWG32 UL3302, ~100 mm (the model's leads are 101 mm); solder pads J2",
            "z": "free: it sits in the cell notch, glue it to a shell (vibrates along its own axis, <= 0.5 kgf)",
            "src": "cad/docs/Vybronics-VG0932022D-datasheet.md lines 174 (D8, 3.2t) and 516-526 (outline 8, 3.25, leads 100, tolerances "
                   "truncated; the PDF's images do not render); VG0832022D.step measured (case 3.25 + 0.25 adhesive pad = 3.53, D8.00)"},
    "console_outline": {"bbox": console, "note": "Eco1.User sketch 'CONSOLE outline ~195 x 90 (screen centred)', corner r ~7.1"},
}
mech["dwgs_rects"] = [r for r in dw_rects if r not in (cell_body, cell_buf)]
for key, rb in (("speaker", 13.5), ("lra", 4.0)):          # place.py's drawn body circle vs the board-only footprint
    c = next((c for c in dw_circs if abs(c[2] - rb) < 0.05), None)
    mech[key]["drawn_circle"] = c
    if c and mech[key]["x"] is not None and math.hypot(c[0] - mech[key]["x"], c[1] - mech[key]["y"]) > 0.05:
        model_issues.append({"ref": key, "issue": "Dwgs.User body circle at (%.2f, %.2f) but the footprint at (%.2f, %.2f)"
                             % (c[0], c[1], mech[key]["x"], mech[key]["y"])})

# ====================================================================== regions
REGIONS = [   # (name, rect x0, y0, x1, y1 in KiCad mm), first match wins
    ("usb_c", (108.0, BY0, 127.5, 47.0), "USB-C area: USBC1 + D4 / CC resistors"),
    ("jack_corner", (BX0, 100.0, 57.0, BY1), "headphone-jack corner (bottom-left)"),
    ("left_grip", (BX0, BY0, 57.0, 100.0), "left of the panel (x < 57): D-pad, START, L, BT830, microSD"),
    ("centre", (57.0, BY0, 127.5, BY1), "under the panel, left of the cell notch (SoC, display/touch connectors)"),
    ("top_strip", (127.5, BY0, 181.7, 52.3), "the strip over the cell notch: charger, buck-boost, gauge, INA226"),
    ("notch", (127.5, 52.3, 181.7, BY1), "the cell + LRA notch (no board)"),
    ("right_grip", (181.7, BY0, BX1, BY1), "right of the notch: ABXY, SELECT, R, VOL+/-, POWER, CN1, amp, speaker cut-out"),
]
def region_of(p):
    if p["courtyard"]:
        xs_ = [c[0] for c in p["courtyard"]]; ys_ = [c[1] for c in p["courtyard"]]; cx, cy_ = (min(xs_) + max(xs_)) / 2, (min(ys_) + max(ys_)) / 2
    else: cx, cy_ = p["x"], p["y"]
    for name, (x0, y0, x1, y1), _ in REGIONS:
        if x0 <= cx <= x1 and y0 <= cy_ <= y1: return name
    return "off-board"
regions = {}
for name, rect, desc in REGIONS:
    regions[name] = {"rect": [r3(v) for v in rect], "desc": desc, "top": [], "bottom": [], "free": []}
for p in parts:
    p["region"] = region_of(p)
    if p["region"] in regions and p["height"] > 0:
        regions[p["region"]]["free" if p.get("z_free") else p["side"]].append(p)
for name, R in regions.items():
    for side in ("top", "bottom", "free"):
        L = sorted(R[side], key=lambda p: -p["height_design"])
        R[side] = [{"ref": p["ref"], "value": p["value"], "z": p["z"], "z_design": p["z_design"], "height": p["height"],
                    "height_design": p["height_design"], "src": p["height_source"][:60]} for p in L[:4]]
    R["n_top"] = sum(1 for p in parts if p["region"] == name and p["side"] == "top")
    R["n_bottom"] = sum(1 for p in parts if p["region"] == name and p["side"] == "bottom")

def xy_box(p):
    poly = p.get("hull") or p["courtyard"] or [(p["x"], p["y"])]
    return min(q[0] for q in poly), min(q[1] for q in poly), max(q[0] for q in poly), max(q[1] for q in poly)
pr_ = mech["panel"]["rect"]
under = [p for p in parts if not p.get("z_free") and p["height"] > 0 and (lambda b: b[0] < pr_[2] and b[2] > pr_[0] and b[1] < pr_[3] and b[3] > pr_[1])(xy_box(p))]
regions["under_panel"] = {"rect": pr_, "desc": "every part whose model hull / courtyard overlaps the panel outline (overlap, not a partition)",
                          "n_top": sum(1 for p in under if p["side"] == "top"), "n_bottom": sum(1 for p in under if p["side"] == "bottom"), "free": []}
for side in ("top", "bottom"):
    L = sorted((p for p in under if p["side"] == side), key=lambda p: -p["height_design"])
    regions["under_panel"][side] = [{"ref": p["ref"], "value": p["value"], "z": p["z"], "z_design": p["z_design"], "height": p["height"],
                                     "height_design": p["height_design"], "src": p["height_source"][:60]} for p in L[:6]]

# ====================================================================== holes
holes, other_holes = [], []
for p in parts:
    fp = B.FindFootprintByReference(p["ref"])
    for pd in fp.Pads():
        if not pd.HasHole(): continue
        pp, ds_ = pd.GetPosition(), pd.GetDrillSize()
        h = {"x": r3(TO(pp.x)), "y": r3(TO(pp.y)), "d": r3(min(TO(ds_.x), TO(ds_.y)))}
        if p["ref"].startswith("H"):
            h.update({"name": p["ref"], "ref": p["ref"], "kind": "M2 mounting hole (NPTH 2.2)", "keepout_r": 2.75,
                      "keepout_src": "rule area 'datasheet keep-out: %s M2 mounting hole (r 2.75 mm, F+B)'" % p["ref"]})
            holes.append(h)
        else:
            h.update({"ref": p["ref"], "pad": pd.GetNumber() or "", "plated": pd.GetAttribute() == pcbnew.PAD_ATTRIB_PTH,
                      "slot": [r3(TO(ds_.x)), r3(TO(ds_.y))] if abs(ds_.x - ds_.y) > 1000 else None})
            other_holes.append(h)

rule_areas = []
for z in B.Zones():
    if not z.GetIsRuleArea(): continue
    nm = z.GetZoneName()
    if not nm.startswith("datasheet keep-out"): continue                       # skip the DRC-only 'pins:' areas
    bb = z.GetBoundingBox()
    rule_areas.append({"name": nm, "bbox": [r3(TO(bb.GetLeft())), r3(TO(bb.GetTop())), r3(TO(bb.GetRight())), r3(TO(bb.GetBottom()))],
                       "layers": [B.GetLayerName(l) for l in z.GetLayerSet().Seq()]})
# connector mating directions (KiCad frame), from pads vs housing (checked against the models / datasheets)
connectors = {}
if "CN1" in P:
    connectors["CN1"] = {"what": "cell JST PH plug (PHR-2) mates here", "mouth_dir": [0, 1], "mouth_face_y": P["CN1"]["model_bbox"][3],
                         "why": "contact tails (pads 1 BAT / 2 GND) at y 42.67 = rear, solder tabs (pads 3/4) at y 48.53 = front; housing "
                                "front face at the model's max y", "src": "cad/docs/C295747.pdf p3 (SMT side entry) + the 3D model"}
for ref, who in (("P1", "display flex (40P, contacts down; F side)"), ("FPC1", "touch flex (6P, wraps the board's bottom edge; B side)")):
    if ref in P:
        connectors[ref] = {"what": who, "flex_from": "+y (the panel's bottom edge, folded back: mech.flex.landings_bbox)"}

board = {"outline": OUTLINE, "outline_yup": [(x, r3(-y)) for x, y in OUTLINE], "bbox": [BX0, BY0, BX1, BY1], "size": [r3(BX1 - BX0), r3(BY1 - BY0)], "thickness": THICK,
         "holes": holes, "other_holes": other_holes, "edges": EDGES,
         "notches": [{"rect": r, "name": next((n for n, test in (
             ("speaker cut-out", lambda r: "SPK1" in P and r[0] <= P["SPK1"]["x"] <= r[2] and r[1] <= P["SPK1"]["y"] <= r[3]),
             ("cell + LRA notch", lambda r: cell_body and r[0] <= (cell_body[0] + cell_body[2]) / 2 <= r[2] and r[1] <= (cell_body[1] + cell_body[3]) / 2 <= r[3]),
             ("cable notch (display/touch flex fold)", lambda r: True)) if test(r)), "")} for r in NOTCH_RECTS],
         "notches_note": "the rectangles between the outline's straight runs; the real corners are R2 (concave fillets bulge into "
                         "the cut-out), R8 at the four outer corners (draw_outline.py)"}

out = {"source": os.path.relpath(os.path.abspath(args.board), REPO) if os.path.abspath(args.board).startswith(REPO) else args.board,
       "board_md5": board_md5, "generated": datetime.datetime.now().isoformat(timespec="seconds"),
       "frame": {"xy": "KiCad board mm, +X right, +Y DOWN (as gb2's board.json); enclosure XY = (x, -y), as gb2's params.flip / P()",
                 "z": "board bottom face z = 0, top face (F, screen side) z = 1.6, +z toward the screen; F parts grow up from 1.6, B parts down from 0",
                 "height": "body height off the face the part sits on (gb2 meaning); z = full model range; z_design / height_design add the datasheet max"},
       "board": board, "mech": mech, "switches": switches, "edge_features": edge_features, "connectors": connectors,
       "rule_areas": rule_areas, "regions": regions,
       "model_issues": model_issues, "fallbacks": fallbacks, "glb_warnings": GLB_WARN,
       "parts": [{k: v for k, v in p.items() if not k.startswith("_")} for p in parts]}
os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
json.dump(out, open(args.out, "w"), indent=1)

# ====================================================================== summary
log("board.json: %d parts, outline %d pts %.1f x %.1f, %d notches, %d M2 holes, %d other holes -> %s"
    % (len(parts), len(OUTLINE), BX1 - BX0, BY1 - BY0, len(NOTCH_RECTS), len(holes), len(other_holes), os.path.relpath(args.out, HERE)))
nchk = [p for p in parts if "glb_check" in p]
log("VRML transform vs kicad-cli GLB: %d checked, %d agree" % (len(nchk), sum(1 for p in nchk if p["glb_check"].startswith("agree"))))
for name, R in regions.items():
    f = ", ".join("%s %.2f..%.2f" % (q["ref"], *q["z_design"]) for q in R["top"][:3]) or "-"
    b = ", ".join("%s %.2f..%.2f" % (q["ref"], *q["z_design"]) for q in R["bottom"][:3]) or "-"
    log("  %-11s F: %s | B: %s" % (name, f, b))
log("model issues: %d, fallbacks: %d (%s)" % (len(model_issues), len(fallbacks), ", ".join(f["ref"] for f in fallbacks)))
sys.stderr.flush(); sys.stdout.flush()
os._exit(0)
