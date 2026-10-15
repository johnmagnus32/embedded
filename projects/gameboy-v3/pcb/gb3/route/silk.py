#!/usr/bin/env python3
"""silk.py — reference designators on gb3's silkscreen (idempotent; run after placement or routing changes).

Each part's reference goes to the first clear spot beside it, nearest first: the four sides of its courtyard, a few
offsets out and along each side, upright or turned 90 deg, 1.0 mm text (0.8 mm where 1.0 fits nowhere; 0.15 mm
stroke). A spot is clear when the text box (+0.1 mm) misses every pad on that side (JLC clips silk off exposed
copper), every reference already placed, every other part's courtyard on that side (the text would sit under the
part), and stays >= 0.3 mm inside the board edge. Vias are tented (board setup), so silk may cross them. Big parts go
first. A part with no clear spot keeps its reference on the Fab layer (the assembly drawing), as do the board-only
mounting holes H1-H5. Step 0 adds the board-level marks (pin-1 / polarity where the footprint has none that prints, and
USBC1's pin-in-paste apertures) and moves wrong-end footprint marks to Fab. First, footprint silk graphics: a piece over exposed copper, over another part's silk (the
smaller part's piece) or within 0.2 mm of the board edge (the overhanging jack / USB-C outlines) moves to Fab;
footprints linked to a library (the step 1-3 parts) keep their graphics, so the board never drifts from the library.

Run:  <kicad python> route/silk.py [board]
"""
import pcbnew, os, sys, math
HERE = os.path.dirname(os.path.abspath(__file__))
path = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "..", "gb3.kicad_pcb")
import subprocess        # strip the previous run's marks in a child process (B.Remove breaks this process's SWIG casts)
subprocess.run([sys.executable, os.path.join(HERE, "route.py"), "--strip", path, "marks"], check=True)
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

# ---- 0. board-level marks (pre-fab, fab review 2026-10-15): a readable pin-1 / polarity mark where the footprint's own is
# missing (moved to Fab here as it sat on a pad), too small to print or at the wrong end, USBC1's pin-in-paste apertures,
# the F.Mask openings of the LCM ground contacts
# (its four shell legs are the receptacle's only anchors; JLC prints paste only where the B.Paste layer has it). Rebuilt
# every run in the group "route:marks"; the marks are obstacles for the references below.
MARK_D = 0.5
MG = pcbnew.PCB_GROUP(B); MG.SetName("route:marks"); B.Add(MG)      # the old group went in the child process above
marks = []                                   # (copper side, box) obstacles
PADBOX = {(fp.GetReference(), p.GetNumber()): box(p.GetBoundingBox()) for fp in fps for p in fp.Pads()}
def pad_box(ref, num):
    return PADBOX[(ref, num)]
def _add(item, side):
    B.Add(item); MG.AddItem(item)
    if side is not None: marks.append((side, box(item.GetBoundingBox(), 0.05)))
def dot(x, y, side):                         # filled pin-1 dot
    c = pcbnew.PCB_SHAPE(B); c.SetShape(pcbnew.SHAPE_T_CIRCLE); c.SetLayer(SIDE[side][0]); c.SetFilled(True)
    c.SetCenter(V(x, y)); c.SetEnd(V(x + MARK_D / 2, y)); c.SetWidth(MM(0.1)); _add(c, side)
def label(x, y, t, side):                    # polarity text
    tx = pcbnew.PCB_TEXT(B); tx.SetText(t); tx.SetLayer(SIDE[side][0]); tx.SetPosition(V(x, y))
    tx.SetTextSize(V(1.0, 1.0)); tx.SetTextThickness(MM(STROKE)); tx.SetMirrored(side == pcbnew.B_Cu)
    tx.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_CENTER); tx.SetVertJustify(pcbnew.GR_TEXT_V_ALIGN_CENTER); _add(tx, side)
def paste(x0, y0, x1, y1, layer):            # a paste aperture
    r = pcbnew.PCB_SHAPE(B); r.SetShape(pcbnew.SHAPE_T_RECT); r.SetLayer(layer); r.SetFilled(True)
    r.SetStart(V(x0, y0)); r.SetEnd(V(x1, y1)); r.SetWidth(0); _add(r, None)
F_, B_ = pcbnew.F_Cu, pcbnew.B_Cu
def spot(ref, num, side, prefer):            # a mark's place beside a pad: clear of every pad and silk graphic on that side
    b = pad_box(ref, num); cx, cy = (b[0] + b[2]) / 2, (b[1] + b[3]) / 2; r = MARK_D / 2
    L = SIDE[side][0]
    obst = [box(g.GetBoundingBox()) for f in fps for g in f.GraphicalItems() if g.GetLayer() == L and g.GetClass() == "PCB_SHAPE"]
    obst += [bb for sd, bb in marks if sd == side]
    for gap in (0.35, 0.5, 0.7, 1.0):
        for dx, dy in prefer:
            x = b[2] + gap + r if dx > 0 else b[0] - gap - r if dx < 0 else cx
            y = b[3] + gap + r if dy > 0 else b[1] - gap - r if dy < 0 else cy
            bi = (x - r - 0.1, y - r - 0.1, x + r + 0.1, y + r + 0.1)
            if inside(bi) and not any(hit(bi, q) for q in pads[side]) and not any(hit(bi, q) for q in obst): return x, y
    sys.exit(f"silk: no clear spot for the {ref} pin-1 mark")
b = pad_box("P1", "1"); t41 = pad_box("P1", "41")
dot(t41[0] - 0.45, b[1] + 0.25, F_)                         # P1: the flex's pin 1 (VLED-), west end (footprint's is at pad 40)
b = pad_box("U18", "1"); dot((b[0] + b[2]) / 2, b[3] + 0.65, B_)   # BT830 pin 1 (footprint's dot is at the antenna end)
dot(*spot("U13", "1", B_, ((1, 0), (1, -1), (0, -1), (1, 1))), B_)     # MAX17048 pin 1 (its dot sat on the pad)
dot(*spot("U17", "1", B_, ((0, 1), (-1, 1), (-1, 0), (1, 1))), B_)    # STM6601 pin 1 (likewise)
for num, t in (("1", "+"), ("2", "-")):                             # CN1: BAT on pad 1 (JST-PH, red lead = +)
    b = pad_box("CN1", num); label((b[0] + b[2]) / 2, b[1] - 0.9, t, F_)
for num in ("1", "2", "3", "4"):                                    # USBC1 shell legs: pin-in-paste on B, 0.2 over the pad
    b = pad_box("USBC1", num); front = num in ("2", "3")              # (pads 2 / 3 sit 0.35 from the edge: no overprint there)
    paste(b[0] - 0.2, b[1] - (0.0 if front else 0.2), b[2] + 0.2, b[3] + 0.2, pcbnew.B_Paste)
for z in B.Zones():                         # the LCM ground contacts (footprint_keepouts.py): F pour exposed 0.3 inside each area
    if z.GetIsRuleArea() and "LCM ground contact" in z.GetZoneName():
        x0, y0, x1, y1 = box(z.GetBoundingBox(), -0.3)
        paste(x0, y0, x1, y1, pcbnew.F_Mask); marks.append((pcbnew.F_Cu, (x0 - 0.1, y0 - 0.1, x1 + 0.1, y1 + 0.1)))
WRONG = 0
for fp in fps:                              # footprint marks that point the wrong way -> Fab
    ref = fp.GetReference()
    for g in fp.GraphicalItems():
        L = g.GetLayer()
        if L not in (pcbnew.F_SilkS, pcbnew.B_SilkS): continue
        circ = g.GetClass() == "PCB_SHAPE" and g.GetShape() == pcbnew.SHAPE_T_CIRCLE
        poly = g.GetClass() == "PCB_SHAPE" and g.GetShape() == pcbnew.SHAPE_T_POLY
        if ((ref == "P1" and ((circ and TO(g.GetRadius()) > 0.3) or poly)) or        # its pin-1 circle + triangle: pad-40 end
                (ref == "U18" and circ and math.hypot(TO(g.GetCenter().x) - sum(PADBOX[("U18", "1")][0::2]) / 2,
                                                      TO(g.GetCenter().y) - sum(PADBOX[("U18", "1")][1::2]) / 2) > 3.0) or
                (ref == "J2" and g.GetClass() == "PCB_TEXT" and g.GetText() == "+")):     # an LRA has no polarity
            g.SetLayer(pcbnew.F_Fab if L == pcbnew.F_SilkS else pcbnew.B_Fab); WRONG += 1

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
for side, bb in marks: placed[side].append(bb)

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
print("silk: %d board-level marks, %d wrong-end footprint marks to Fab" % (len(MG.GetItems()), WRONG))
print("silk: %(outline)d outline pieces to Fab (over pads / other parts' silk / the edge); references: %(silk)d at 1.0 mm, "
      "%(silk 0.8)d at 0.8 mm, %(fab)d on Fab (no clear spot), %(holes)d holes on Fab" % stats)
sys.stdout.flush(); os._exit(0)
