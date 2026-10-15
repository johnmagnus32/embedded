#!/usr/bin/env python3
"""silk.py — reference designators on gb3's silkscreen (idempotent; run after placement or routing changes).

Each part's reference goes to the first clear spot beside it, nearest first: the four sides of its courtyard, a few
offsets out and along each side, upright or turned 90 deg, 1.0 mm text (0.8 mm where 1.0 fits nowhere; 0.15 mm
stroke). A spot is clear when the text box (+0.1 mm) misses every pad on that side (JLC clips silk off exposed
copper), every reference already placed, every other part's courtyard on that side (the text would sit under the
part), and stays >= 0.3 mm inside the board edge. Vias are tented (board setup), so silk may cross them. Big parts go
first. A part with no clear spot keeps its reference on the Fab layer (the assembly drawing), as do the board-only
mounting holes H1-H5. First, footprint silk graphics: a piece over exposed copper, over another part's silk (the
smaller part's piece) or within 0.2 mm of the board edge (the overhanging jack / USB-C outlines) moves to Fab;
footprints linked to a library (the step 1-3 parts) keep their graphics, so the board never drifts from the library.

Run:  <kicad python> route/silk.py [board]
"""
import pcbnew, os, sys, math
HERE = os.path.dirname(os.path.abspath(__file__))
path = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "..", "gb3.kicad_pcb")
B = pcbnew.LoadBoard(path); MM, TO = pcbnew.FromMM, pcbnew.ToMM
V = lambda x, y: pcbnew.VECTOR2I(int(round(MM(x))), int(round(MM(y))))
SIZES = (1.0, 0.8)
STROKE = 0.15
PAD_CLR, TEXT_CLR, EDGE_CLR = 0.1, 0.1, 0.3
GAPS = (0.1, 0.25, 0.45, 0.7, 1.0)
HOLES = {"H1", "H2", "H3", "H4", "H5"}

ps = pcbnew.SHAPE_POLY_SET(); B.GetBoardPolygonOutlines(ps, True)
SIDE = {pcbnew.F_Cu: (pcbnew.F_SilkS, pcbnew.F_CrtYd, pcbnew.F_Fab), pcbnew.B_Cu: (pcbnew.B_SilkS, pcbnew.B_CrtYd, pcbnew.B_Fab)}
def box(bb, m=0.0):
    return (TO(bb.GetLeft()) - m, TO(bb.GetTop()) - m, TO(bb.GetRight()) + m, TO(bb.GetBottom()) + m)
def hit(a, b):
    return a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]

fps = list(B.GetFootprints())
pads = {pcbnew.F_Cu: [], pcbnew.B_Cu: []}
for fp in fps:
    for p in fp.Pads():
        for L in pads:
            if p.IsOnLayer(L): pads[L].append(box(p.GetBoundingBox(), PAD_CLR))
def courtyard(fp):
    L = SIDE[fp.GetLayer()][1]
    try:
        poly = fp.GetCourtyard(L)
        if poly.OutlineCount(): return box(poly.BBox())
    except Exception: pass
    return box(fp.GetBoundingBox(False, False))
yards = {fp.GetReference(): (fp.GetLayer(), courtyard(fp)) for fp in fps}
placed = {pcbnew.F_Cu: [], pcbnew.B_Cu: []}

def inside(b):
    pts = [(b[0], b[1]), (b[2], b[1]), (b[0], b[3]), (b[2], b[3])]
    if not all(ps.Contains(V(x, y)) for x, y in pts): return False
    return all(ps.COutline(0).SquaredDistance(V(x, y), True) >= MM(EDGE_CLR) ** 2 for x, y in pts)   # to the outline itself

def clear(fp, b):
    L = fp.GetLayer(); bi = (b[0] - TEXT_CLR, b[1] - TEXT_CLR, b[2] + TEXT_CLR, b[3] + TEXT_CLR)
    if not inside(b): return False
    if any(hit(bi, q) for q in pads[L]): return False
    if any(hit(bi, q) for q in placed[L]): return False
    me = fp.GetReference()
    return not any(r != me and l == L and hit(b, y) for r, (l, y) in yards.items())

def candidates(fp, t, size):
    """(position, angle) spots around the courtyard, nearest first"""
    x0, y0, x1, y1 = yards[fp.GetReference()][1]; cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    out = []
    n = len(t.GetText())
    for ang in (0, 90):
        w, h = (0.75 * size * n + STROKE, size + STROKE) if ang == 0 else (size + STROKE, 0.75 * size * n + STROKE)
        for g in GAPS:
            for sx in (0, -0.5, 0.5, -1.0, 1.0, -2.0, 2.0):
                out += [((cx + sx, y0 - g - h / 2), ang), ((cx + sx, y1 + g + h / 2), ang)]
            for sy in (0, -0.5, 0.5, -1.0, 1.0, -2.0, 2.0):
                out += [((x0 - g - w / 2, cy + sy), ang), ((x1 + g + w / 2, cy + sy), ang)]
    return sorted(out, key=lambda c: math.hypot(c[0][0] - cx, c[0][1] - cy) + (0.3 if c[1] else 0))

def yard_area(ref):
    x0, y0, x1, y1 = yards[ref][1]; return (x1 - x0) * (y1 - y0)
def area(fp): return yard_area(fp.GetReference())
stats = {"silk": 0, "silk 0.8": 0, "fab": 0, "holes": 0, "outline": 0}

# ---- 1. footprint silk graphics: a piece that runs over exposed copper (any pad on that side), over another part's
# silk, or within the board-edge clearance goes to Fab (JLC clips silk over pads anyway; the overhanging jack / USB-C
# outlines would print off the board)
SILK_PAD_CLR, SILK_CURVE_CLR, SILK_SILK_CLR, SILK_EDGE_CLR = MM(0.08), MM(0.15), 0, MM(0.2)
edge = []
for d in B.GetDrawings():
    if d.GetLayer() == pcbnew.Edge_Cuts: edge.append(d.GetEffectiveShape(pcbnew.Edge_Cuts))
pad_shapes = {pcbnew.F_Cu: [], pcbnew.B_Cu: []}
for fp in fps:
    for p in fp.Pads():
        for L in pad_shapes:
            if p.IsOnLayer(L): pad_shapes[L].append(p.GetEffectiveShape(L))
gfx = {pcbnew.F_SilkS: [], pcbnew.B_SilkS: []}
def lib_linked(fp):        # a footprint with a library nickname is DRC-compared to the library: leave its graphics
    return bool(fp.GetFPID().GetLibNickname().wx_str() if hasattr(fp.GetFPID().GetLibNickname(), "wx_str") else str(fp.GetFPID().GetLibNickname()))
for fp in fps:
    for g in fp.GraphicalItems():
        if g.GetLayer() in gfx and g.GetClass() == "PCB_SHAPE" and not lib_linked(fp):
            gfx[g.GetLayer()].append((fp.GetReference(), g, g.GetEffectiveShape(g.GetLayer()), None))
COPPER = {pcbnew.F_SilkS: pcbnew.F_Cu, pcbnew.B_SilkS: pcbnew.B_Cu}
FAB = {pcbnew.F_SilkS: pcbnew.F_Fab, pcbnew.B_SilkS: pcbnew.B_Fab}
moved = set()
pinned = {fp.GetReference() for fp in fps if lib_linked(fp)}       # their pieces cannot move: the other one does
for fp in fps:
    if fp.GetReference() in pinned:
        for g in fp.GraphicalItems():
            if g.GetLayer() in gfx and g.GetClass() == "PCB_SHAPE":
                gfx[g.GetLayer()].append((fp.GetReference(), g, g.GetEffectiveShape(g.GetLayer()), "pinned"))
for L, items in gfx.items():
    for k, (ref, g, sh, pin) in enumerate(items):
        if pin: continue
        clr = SILK_PAD_CLR if g.GetShape() == pcbnew.SHAPE_T_SEGMENT else SILK_CURVE_CLR    # arcs: KiCad's check is coarser
        bad = any(sh.Collide(ps_, clr) for ps_ in pad_shapes[COPPER[L]]) or any(sh.Collide(e, SILK_EDGE_CLR) for e in edge)
        if not bad:     # over another part's outline: the smaller part's piece goes (always when the other is pinned)
            for ref2, g2, sh2, _ in items:
                if ref2 != ref and id(g2) not in moved and sh.Collide(sh2, SILK_SILK_CLR) and (
                        ref2 in pinned or yard_area(ref) <= yard_area(ref2)):
                    bad = True; break
        if bad:
            g.SetLayer(FAB[L]); moved.add(id(g)); stats["outline"] += 1
# what stays on silk (outlines, polarity / pin-1 marks, footprint texts) is an obstacle for the references
for fp in fps:
    for g in fp.GraphicalItems():
        if g.GetLayer() in COPPER: placed[COPPER[g.GetLayer()]].append(box(g.GetBoundingBox()))

# ---- 2. references, big parts first
for fp in sorted(fps, key=lambda f: -area(f)):
    t = fp.Reference(); silk, crt, fab = SIDE[fp.GetLayer()]
    t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_CENTER); t.SetVertJustify(pcbnew.GR_TEXT_V_ALIGN_CENTER)
    if fp.GetReference() in HOLES:
        t.SetLayer(fab); stats["holes"] += 1; continue
    t.SetLayer(silk); t.SetVisible(True); t.SetMirrored(fp.GetLayer() == pcbnew.B_Cu)
    done = False
    for size in SIZES:
        t.SetTextSize(V(size, size)); t.SetTextThickness(MM(STROKE))
        for (x, y), ang in candidates(fp, t, size):
            t.SetTextAngleDegrees(ang); t.SetPosition(V(x, y))
            b = box(t.GetBoundingBox())
            if clear(fp, b):
                placed[fp.GetLayer()].append(b); stats["silk" if size == SIZES[0] else "silk 0.8"] += 1; done = True; break
        if done: break
    if not done:
        t.SetTextSize(V(SIZES[-1], SIZES[-1])); t.SetTextAngleDegrees(0); t.SetPosition(fp.GetPosition())
        t.SetLayer(fab); t.SetMirrored(fp.GetLayer() == pcbnew.B_Cu); stats["fab"] += 1

pro = os.path.splitext(path)[0] + ".kicad_pro"
keep = open(pro, "rb").read() if os.path.exists(pro) else None
pcbnew.SaveBoard(path, B)
if keep is not None and open(pro, "rb").read() != keep: open(pro, "wb").write(keep)   # SaveBoard rewrites the project file
print("silk: %(outline)d outline pieces to Fab (over pads / other parts' silk / the edge); references: %(silk)d at 1.0 mm, "
      "%(silk 0.8)d at 0.8 mm, %(fab)d on Fab (no clear spot), %(holes)d holes on Fab" % stats)
sys.stdout.flush(); os._exit(0)
