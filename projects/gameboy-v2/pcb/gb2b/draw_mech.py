#!/usr/bin/env python3
"""draw_mech.py — draw gb2b's mechanical reference outlines, to scale, inside a Game Boy Advance outline
so the form factor can be judged before placement starts.

Layers (each is cleared and redrawn, so this is re-runnable):
  * User.2        — Adafruit 2090 TFT module: I-shaped PCB, 4 mounting holes, JP1/JP2 headers (pin 1
                    marked), panel outline, touch viewing area, LCD active area, FPC slot/pocket.
  * User.Drawings — battery (PKCELL LP-803860), speaker (CES-2704), LRA (VG0832022D), IMU (LSM6DSOX),
                    each with its handling buffer, same buffers as gb3.
  * User.Eco1     — Game Boy Advance (AGB-001) front view: silhouette, screen lens + active area,
                    D-pad / A / B / Start / Select, L/R shoulders, speaker grille, LED, battery door.
  * User.Comments — fit notes.
  * Edge.Cuts     — the board: the widened GBA outline inset by BOARD_INSET (printed wall + clearance), minus
                    the battery and speaker buffers (through-cutouts / notches), slivers removed, inside corners CUT_R.
(User.Eco2 is also cleared: it used to carry the GBA motherboard outline.)

Geometry and sources live in mech_ref.json (every item: source + confidence). Placement concept: the
2090 module is centred in the GBA outline, JP1 (the 8-bit bus header) on the left. The GBA outline is
widened by cutting it at its vertical centre line and pushing the halves apart — the D-pad / Start /
Select / L on the left and A / B / R / grille on the right move out with their half; the screen lens,
battery door and anything centred stay put. The widening is the smallest (0.5 mm steps) that gives
every front control CONTROL_CLEARANCE to the module outline. Our parts sit where the GBA has its
equivalents (cell in the battery bay, speaker at the grille, LRA in the left grip, IMU at the centre).
Nothing here is a decision — it's a starting sketch.

Run:  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 draw_mech.py
"""
import pcbnew, os, sys, json, math

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(HERE, "gb2b.kicad_pcb")
REF = json.load(open(os.path.join(HERE, "mech_ref.json"), encoding="utf-8"))
MM = pcbnew.FromMM
def V(x, y): return pcbnew.VECTOR2I(int(round(MM(x))), int(round(MM(y))))

GBA_ORIGIN = (20.0, 30.0)           # board mm of the (widened) GBA bounding box's top-left corner
GBA_W, GBA_H = 144.5, 82.0          # AGB-001 official
XC = GBA_W / 2                      # GBA centre line (the cut for the widening)
TFT_FLIP = False                    # False: JP1 (8-bit bus) on the left. True mirrors the module left/right.
CENTER_ON = "module"                # "module": the 2090 PCB is centred (its active area then sits 2.64 mm toward
                                    # JP1); "active-area": the pixels are centred instead (PCB shifts 2.64 toward JP2)
CONTROL_CLEARANCE = 3.0             # mm from each front control to the module outline (shell wall + keycap gap)
GBA_STRETCH = None                  # mm added at the centre line; None = the smallest giving CONTROL_CLEARANCE

# Our parts (body w, h / diameter; buffer) — same cells/transducers and buffers as gb3 (datasheet-verified bodies).
BATTERY = (62.0, 38.5, 2.0)         # Adafruit 2011 = PKCELL LP-803860 (2000 mAh), max 62 x 38.5 x 8.5 incl. PCM (landscape), +2 buffer
SPEAKER = (27.0, 1.5)               # CES-2704-088L050 Ø27 ±0.2 x 4.9, +1.5 buffer
SPK_OPENING = 23.4                  # Ø23.4 ±0.1 acoustic opening (front) — sizes the grille
LRA     = (8.0, 1.0)                # VG0832022D Φ8 x 3.2, +1.0 buffer
IMU     = (3.0, 2.5, 5.0)           # LSM6DSOX LGA-14 2.5 x 3.0 x 0.83; 5 mm quiet-zone guideline (see label)

# Where our parts go, in (unwidened) GBA frame mm (centres). Left/right of XC moves with its half.
# The battery and speaker are start points: fit() nudges them (smallest move) so their buffers stay FIT_MARGIN
# inside the widened outline, and the speaker's buffer clears the module by SPEAKER_MODULE_GAP.
BATTERY_AT = (XC, 59.5)             # GBA 2xAA battery-door centre (back side) — centred; fit() moves it UP only
SPEAKER_AT = (120.7, 56.6)          # GBA speaker-grille centre; fit() moves it right first, then up/down if needed
SHELL_WALL = 2.0                    # mm: 3D-printed shell wall; the GBA outline is the shell's OUTER surface
FIT_MARGIN = SHELL_WALL             # mm a part's buffer must stay inside the outline (i.e. clear of the wall's inner face)
BOARD_INSET = SHELL_WALL + 0.5      # mm: board edge (Edge.Cuts) inside the outline = wall + 0.5 clearance
# The battery and speaker sit in through-cutouts of the board (their buffers included), so the cell is beside the
# board, not behind it. Any board strip narrower than 2*SLIVER left between a cutout and the edge is removed (the
# cutout becomes a notch open to the edge); inside corners get CUT_R for the router bit.
SLIVER = 2.5
STRIP_BELOW_HOLE = 3.0              # mm of board kept below the 2090 standoff hole that sits in the J4 strip (r 1.5 + 1.5)
CUT_R = 1.0
SPEAKER_MODULE_GAP = 0.5            # mm between the speaker buffer and the 2090 module outline
LRA_AT     = (8.5, 52.0)            # left grip, below the D-pad, out toward the edge (buffer ~0.9 mm inside the board
                                    # edge, ~5 mm clear of the bottom-left shell screw) so the driver block beside it
                                    # gets room between it and Start/Select
IMU_AT     = (XC, 41.0)             # device centre, on the mainboard under the display
# GBA shell-screw positions NOT used by our (3D-printed) shell — the shell is our own design, so the GBA's six are
# only a reference. Dropped 2026-10-10: the mid-left one sat in the pocket up-right of the D-pad that the button
# expander needs; the four corner screws remain.
SCREWS_DROPPED = {"Shell screw / board hole, mid-left",
                  "Shell screw / board hole, mid-right"}      # mid-right: sat on the console header J2 (2026-10-10)
# Screws moved off their GBA spot (board mm). Bottom-right: the GBA's lands on our speaker (moved right in fit()), so
# its boss goes below-left of the speaker — off the PCB (in the cutout region), 3.9 mm clear of the speaker buffer,
# 5.4 mm inside the shell outline — no board hole needed.
SCREWS_MOVED = {"Shell screw / board hole, bottom-right": (140.5, 100.5)}
SCREW_HOLE_D = 2.4                  # mm: board hole for an M2 shell screw (only where a kept screw lands on the PCB)
SCREW_KEEPOUT_R = 3.0               # mm: copper keep-out radius around it (Ø5 screw head / boss + margin)
# 2090 screen-module standoffs: a plain hole at each of the module's four Ø3.0 mounting holes (for M2.5 standoffs /
# screws), so the screen can't rock on its headers when the buttons around it are pressed (2026-10-10)
STANDOFF_HOLE_D = 3.2               # mm: M2.5 clearance
STANDOFF_KEEPOUT_R = 3.0            # mm: copper keep-out radius (5 mm hex standoff corners reach r 2.9); J3/J4's end
                                    # pins are 3.55 mm from the hole centres, so they stay clear


def standoffs(S):
    """[(name, board x, board y)] of the 2090's four mounting holes (= our standoff holes)."""
    gx, gy = GBA_ORIGIN; _, _, (mcx, mcy), fl = layout(S)
    return [(it["name"], gx + mcx + fl * it["x"], gy + mcy + it["y"]) for it in REF["tft"]["items"]
            if it["name"].startswith("mounting_hole U$")]


def board_holes(S):
    """Every board hole with its copper keep-out: [(name, x, y, hole dia, keep-out r)] (shell screws + standoffs)."""
    return ([(n, x, y, SCREW_HOLE_D, SCREW_KEEPOUT_R) for n, x, y in screws(S)] +
            [("2090 standoff " + ("top" if y < sum(q[2] for q in standoffs(S)) / 4 else "bottom") + "-" +
              ("left" if x < sum(q[1] for q in standoffs(S)) / 4 else "right"), x, y, STANDOFF_HOLE_D, STANDOFF_KEEPOUT_R)
             for n, x, y in standoffs(S)])


def screws(S):
    """[(name, board x, board y)] of the shell screws the design keeps (the GBA's minus SCREWS_DROPPED)."""
    side = layout(S)[0]; gx, gy = GBA_ORIGIN
    return [(it["name"], *SCREWS_MOVED.get(it["name"], (gx + side(it["x"]), gy + it["y"]))) for it in REF["gba"]["items"]
            if it["name"].startswith("Shell screw") and it["name"] not in SCREWS_DROPPED]

# Front controls checked against the module outline: name -> (GBA x, y, radius). The D-pad cross is taken
# as its Ø18.8 circle (slightly conservative).
CONTROLS = {"D-pad": (19.95, 34.09, 9.4), "Start": (29.49, 53.82, 2.0), "Select": (29.44, 61.82, 2.0),
            "B": (117.94, 36.25, 4.7), "A": (130.88, 31.92, 4.7)}

# 2090 module outline as rects in its frame: centre body + the two header-strip "ears" (ignoring the R2.54 ends)
MODULE_RECTS = [(-35.56, -25.4, 35.56, 25.4), (-40.64, -31.242, -35.56, 31.242), (35.56, -31.242, 40.64, 31.242)]


def item(sec, prefix):
    for it in REF[sec]["items"]:
        if it["name"].startswith(prefix):
            return it
    sys.exit(f"mech_ref.json: no {sec} item starting with {prefix!r}")


def rect_dist(px, py, r):             # point to axis-aligned rect (0 inside)
    x0, y0, x1, y1 = r
    return math.hypot(max(x0 - px, 0, px - x1), max(y0 - py, 0, py - y1))


def rect_rect_dist(a, b):
    return math.hypot(max(b[0] - a[2], 0, a[0] - b[2]), max(b[1] - a[3], 0, a[1] - b[3]))


def layout(S):
    """Everything in (widened) GBA-frame mm for stretch S. Returns (side mapper, centred mapper, module centre)."""
    side = lambda x: x if x < XC else (x + S if x > XC else x + S / 2)
    mid = lambda x: x + S / 2
    aa_t = item("tft", "LCD active area 57.6 x 43.2")
    sx = -1 if TFT_FLIP else 1
    mcx = XC + S / 2 - (sx * aa_t["x"] if CENTER_ON == "active-area" else 0.0)
    mcy = GBA_H / 2 - (aa_t["y"] if CENTER_ON == "active-area" else 0.0)
    return side, mid, (mcx, mcy), sx


def module_rects(S):
    side, mid, (mcx, mcy), sx = layout(S)
    return [(mcx + min(sx * a, sx * c), mcy + b, mcx + max(sx * a, sx * c), mcy + d) for a, b, c, d in MODULE_RECTS]


def silhouette(S):
    """The widened GBA outline in (widened) GBA-frame mm. A vertex on the cut splits into two, in path order."""
    side = layout(S)[0]; pts = item("gba", "GBA front silhouette")["points"]; out = []
    for i, (x, y) in enumerate(pts):
        if x == XC:
            px, nx = pts[i - 1][0], pts[(i + 1) % len(pts)][0]
            out += [(x - 1e-9, y), (x + 1e-9, y)] if px < nx else [(x + 1e-9, y), (x - 1e-9, y)]
        else:
            out.append((x, y))
    return [(side(x), y) for x, y in out]


def inside(p, poly):                  # ray casting
    x, y = p; c = False
    for (x0, y0), (x1, y1) in zip(poly, poly[1:] + poly[:1]):
        if (y0 > y) != (y1 > y) and x < x0 + (y - y0) * (x1 - x0) / (y1 - y0):
            c = not c
    return c


def edge_dist(p, poly):
    best = 1e9
    for (x0, y0), (x1, y1) in zip(poly, poly[1:] + poly[:1]):
        dx, dy = x1 - x0, y1 - y0; L2 = dx * dx + dy * dy
        t = 0 if L2 == 0 else max(0, min(1, ((p[0] - x0) * dx + (p[1] - y0) * dy) / L2))
        best = min(best, math.hypot(p[0] - x0 - t * dx, p[1] - y0 - t * dy))
    return best


def rect_in(cx, cy, w, h, poly, margin):
    pts = [(cx - w / 2 + w * i / 40, cy - h / 2) for i in range(41)] + [(cx - w / 2 + w * i / 40, cy + h / 2) for i in range(41)] \
        + [(cx - w / 2, cy - h / 2 + h * i / 20) for i in range(21)] + [(cx + w / 2, cy - h / 2 + h * i / 20) for i in range(21)]
    return all(inside(p, poly) and edge_dist(p, poly) >= margin for p in pts)


def stretch():
    S = GBA_STRETCH
    if S is None:
        S = 0.0
        while min(clearances(S)[k] for k in CONTROLS) < CONTROL_CLEARANCE:
            S += 0.5
    return S


def fit(S):
    """Smallest moves that put the battery/speaker buffers inside the outline (and the speaker clear of the module).
    Returns {'battery': (x, y), 'speaker': (x, y)} in widened GBA-frame mm."""
    side, mid, _, _ = layout(S); poly = silhouette(S); mod = module_rects(S)
    bw, bh, bb = BATTERY; bx, by = mid(BATTERY_AT[0]), BATTERY_AT[1]
    dy = 0.0
    while not rect_in(bx, by - dy, bw + 2 * bb, bh + 2 * bb, poly, FIT_MARGIN):
        dy += 0.25
        if dy > 30: sys.exit("fit(): battery buffer can't fit inside the outline")
    R = SPEAKER[0] / 2 + SPEAKER[1]; sx0, sy0 = side(SPEAKER_AT[0]), SPEAKER_AT[1]
    def ok(x, y):
        return (inside((x, y), poly) and edge_dist((x, y), poly) >= R + FIT_MARGIN
                and min(rect_dist(x, y, m) for m in mod) >= R + SPEAKER_MODULE_GAP)
    for ady in [i * 0.25 for i in range(0, 61)]:            # prefer a pure move right; then the least up/down
        for sdy in ((0.0,) if ady == 0 else (-ady, ady)):
            for i in range(0, 121):
                if ok(sx0 + i * 0.25, sy0 + sdy):
                    return {"battery": (bx, by - dy), "speaker": (sx0 + i * 0.25, sy0 + sdy)}
    sys.exit("fit(): no speaker position clears the module inside the outline")


def clearances(S, spk=None):
    side, mid, (mcx, mcy), sx = layout(S)
    mod = module_rects(S)
    out = {nm: min(rect_dist(side(x), y, m) for m in mod) - r for nm, (x, y, r) in CONTROLS.items()}
    for nm in ("L shoulder", "R shoulder"):
        s_ = item("gba", nm); x0, x1 = s_["x"] - s_["w"] / 2, s_["x"] + s_["w"] / 2
        rr = (side(x0), s_["y"] - s_["h"] / 2, side(x1), s_["y"] + s_["h"] / 2)
        out[nm] = min(rect_rect_dist(rr, m) for m in mod)
    spx, spy = spk if spk else (side(SPEAKER_AT[0]), SPEAKER_AT[1])
    out["speaker (+buffer)"] = min(rect_dist(spx, spy, m) for m in mod) - (SPEAKER[0] / 2 + SPEAKER[1])
    return out


def board_shape(S, pos):
    """The board (Edge.Cuts) as a SHAPE_POLY_SET in board mm: the widened GBA outline inset by BOARD_INSET, minus the
    battery and speaker buffers, with slivers removed and inside corners rounded."""
    gx, gy = GBA_ORIGIN; ce = pcbnew.CORNER_STRATEGY_ROUND_ALL_CORNERS; err = int(MM(0.005))
    P = lambda x, y: (int(round(MM(x))), int(round(MM(y))))
    b = pcbnew.SHAPE_POLY_SET(); b.NewOutline()
    for x, y in silhouette(S): b.Append(*P(gx + x, gy + y))
    b.Simplify(); b.Inflate(-int(MM(BOARD_INSET)), ce, err)
    bw, bh, bb = BATTERY; bx, by = gx + pos["battery"][0], gy + pos["battery"][1]
    cut = pcbnew.SHAPE_POLY_SET(); cut.NewOutline()
    for x, y in ((bx - bw / 2 - bb, by - bh / 2 - bb), (bx + bw / 2 + bb, by - bh / 2 - bb), (bx + bw / 2 + bb, by + bh / 2 + bb), (bx - bw / 2 - bb, by + bh / 2 + bb)):
        cut.Append(*P(x, y))
    sx, sy = gx + pos["speaker"][0], gy + pos["speaker"][1]; sr = SPEAKER[0] / 2 + SPEAKER[1]
    cut.NewOutline()
    for i in range(96):
        a = 2 * math.pi * i / 96; cut.Append(*P(sx + sr * math.cos(a), sy + sr * math.sin(a)), 1)
    # below the speaker the board stops at a straight vertical edge dropped from the buffer's leftmost point — the
    # corner beyond it would be unusable board
    cut.NewOutline()
    for x, y in ((sx - sr, sy), (sx + sr + 60, sy), (sx + sr + 60, sy + 60), (sx - sr, sy + 60)):
        cut.Append(*P(x, y), 2)
    # right of the speaker: the edge runs straight across from the buffer's top point to the board edge (the corner
    # region below that line and right of the speaker would be unusable board)
    cut.NewOutline()
    for x, y in ((sx, sy - sr), (sx + sr + 60, sy - sr), (sx + sr + 60, sy + 60), (sx, sy + 60)):
        cut.Append(*P(x, y), 3)
    # the strip between the battery notch and the speaker (it carries J4) ends STRIP_BELOW_HOLE under the 2090's
    # mounting hole there (the mainboard's standoff hole), instead of running on to the bottom outline
    _, _, (mcx, mcy), fl = layout(S)
    holes = [(gx + mcx + fl * it["x"], gy + mcy + it["y"]) for it in REF["tft"]["items"] if it["name"].startswith("mounting_hole U$")]
    x0s, x1s = bx + bw / 2 + bb, sx - sr
    inside_strip = [h for h in holes if x0s < h[0] < x1s and h[1] > by - bh / 2]
    if inside_strip:
        ycut = max(h[1] for h in inside_strip) + STRIP_BELOW_HOLE
        cut.NewOutline()
        for x, y in ((x0s - 1, ycut), (x1s + 1, ycut), (x1s + 1, ycut + 60), (x0s - 1, ycut + 60)):
            cut.Append(*P(x, y), 4)
    cut.Simplify()
    b.BooleanSubtract(cut)
    b.Inflate(-int(MM(SLIVER)), ce, err); b.Inflate(int(MM(SLIVER)), ce, err)      # opening: drop strips < 2*SLIVER
    b.Inflate(int(MM(CUT_R)), ce, err); b.Inflate(-int(MM(CUT_R)), ce, err)        # closing: inside corners >= CUT_R
    b.Simplify()
    # holes, punched last so they stay exact circles: the kept shell screws that land on the board + the four 2090
    # standoffs
    holes = pcbnew.SHAPE_POLY_SET()
    for _, hx, hy, hole_d, _r in board_holes(S):
        if not b.Contains(pcbnew.VECTOR2I(int(MM(hx)), int(MM(hy)))): continue
        holes.NewOutline()
        for i in range(48):
            a_ = 2 * math.pi * i / 48
            holes.Append(int(MM(hx + hole_d / 2 * math.cos(a_))), int(MM(hy + hole_d / 2 * math.sin(a_))))
    if holes.OutlineCount():
        b.BooleanSubtract(holes); b.Simplify()
    return b


EDGE_MIN_SEG = 0.05                 # mm: outline points closer than this to the last kept one are dropped

def clean_ring(pts):
    """Drop the micro-segments the opening/closing Inflate passes leave behind (some < 0.01 mm, a few doubling back
    by a fraction of a micron — KiCad's DRC flags those as an invalid outline once zones are filled), then drop
    points that are collinear with their neighbours (within 1 um)."""
    keep = []
    for p in pts:
        if not keep or math.hypot(p[0] - keep[-1][0], p[1] - keep[-1][1]) >= EDGE_MIN_SEG: keep.append(p)
    while len(keep) > 3 and math.hypot(keep[0][0] - keep[-1][0], keep[0][1] - keep[-1][1]) < EDGE_MIN_SEG: keep.pop()
    changed = True
    while changed and len(keep) > 3:
        changed = False
        for i in range(len(keep)):
            a, b, c = keep[i - 1], keep[i], keep[(i + 1) % len(keep)]
            lab = math.hypot(c[0] - a[0], c[1] - a[1])
            if lab > 0 and abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) / lab < 0.001:
                keep.pop(i); changed = True; break
    return keep

def board_polys(S, pos):
    """[(outline pts, [hole pts, ...]), ...] in board mm (micro-segments cleaned out)."""
    b = board_shape(S, pos); out = []
    for i in range(b.OutlineCount()):
        o = b.Outline(i); pts = clean_ring([(pcbnew.ToMM(o.CPoint(k).x), pcbnew.ToMM(o.CPoint(k).y)) for k in range(o.PointCount())])
        holes = []
        for h in range(b.HoleCount(i)):
            hh = b.Hole(i, h); holes.append(clean_ring([(pcbnew.ToMM(hh.CPoint(k).x), pcbnew.ToMM(hh.CPoint(k).y)) for k in range(hh.PointCount())]))
        out.append((pts, holes))
    return out


def main():
    S = stretch()
    side, mid, (mcx, mcy), sx = layout(S)
    pos = fit(S)
    clr = clearances(S, pos["speaker"])

    # Board outline = the widened GBA outline inset by BOARD_INSET (KiCad's rounded polygon offset). Computed
    # BEFORE LoadBoard/B.Remove: after a Remove, KiCad 10's SWIG hands back untyped objects (Outline() breaks).
    gx, gy = GBA_ORIGIN
    board = board_polys(S, pos)

    B = pcbnew.LoadBoard(PATH)
    L_TFT, L_PART, L_GBA, L_OLD, L_NOTE = (B.GetLayerID(n) for n in
                                             ("User.2", "User.Drawings", "User.Eco1", "User.Eco2", "User.Comments"))
    # snapshot before mutating (GetDrawings is unreliable after Add/Remove), then clear our layers
    for d in [d for d in B.GetDrawings() if d.GetLayer() in (L_TFT, L_PART, L_GBA, L_OLD, L_NOTE, pcbnew.Edge_Cuts)]:
        B.Remove(d)

    # KiCad 10's SWIG bindings don't expose the LINE_STYLE enum, so dashed lines are drawn as short segments.
    DASH, GAP = 1.2, 0.8
    def shape(kind, layer, w, a, b):
        s = pcbnew.PCB_SHAPE(B); s.SetShape(kind); s.SetLayer(layer); s.SetWidth(MM(w))
        s.SetStart(V(*a)); s.SetEnd(V(*b)); B.Add(s)
    def seg(layer, a, b, w=0.15, dash=False):
        if not dash:
            return shape(pcbnew.SHAPE_T_SEGMENT, layer, w, a, b)
        L = math.hypot(b[0] - a[0], b[1] - a[1]); t = 0.0
        while t < L:
            t1 = min(t + DASH, L); f0, f1 = t / L, t1 / L
            shape(pcbnew.SHAPE_T_SEGMENT, layer, w, (a[0] + (b[0] - a[0]) * f0, a[1] + (b[1] - a[1]) * f0),
                  (a[0] + (b[0] - a[0]) * f1, a[1] + (b[1] - a[1]) * f1))
            t = t1 + GAP
    def rect(layer, cx, cy, w_, h_, w=0.15, dash=False):
        x0, y0, x1, y1 = cx - w_ / 2, cy - h_ / 2, cx + w_ / 2, cy + h_ / 2
        if not dash:
            return shape(pcbnew.SHAPE_T_RECT, layer, w, (x0, y0), (x1, y1))
        for a, b in (((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))):
            seg(layer, a, b, w, dash=True)
    def circ(layer, cx, cy, r, w=0.15, dash=False):
        if not dash:
            return shape(pcbnew.SHAPE_T_CIRCLE, layer, w, (cx, cy), (cx + r, cy))
        n = max(8, int(2 * math.pi * r / (DASH + GAP)))
        for i in range(n):
            a0 = 2 * math.pi * i / n; a1 = a0 + 2 * math.pi / n * DASH / (DASH + GAP)
            shape(pcbnew.SHAPE_T_SEGMENT, layer, w, (cx + r * math.cos(a0), cy + r * math.sin(a0)),
                  (cx + r * math.cos(a1), cy + r * math.sin(a1)))
    def poly(layer, pts, w=0.15, closed=True):
        for a, b in zip(pts, pts[1:] + (pts[:1] if closed else [])):
            if a != b: seg(layer, a, b, w)
    def text(layer, x, y, s, sz=1.2):
        t = pcbnew.PCB_TEXT(B); t.SetText(s); t.SetLayer(layer); t.SetPosition(V(x, y))
        t.SetTextSize(V(sz, sz)); t.SetTextThickness(MM(sz * 0.12)); t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT)
        B.Add(t)

    gx, gy = GBA_ORIGIN
    Gs = lambda x, y: (gx + side(x), gy + y)          # features that move with their half
    Gc = lambda x, y: (gx + mid(x), gy + y)           # centred features (unstretched, re-centred)

    # ---------------- GBA (User.Eco1) ----------------
    poly(L_GBA, [(gx + x, gy + y) for x, y in silhouette(S)], 0.3)

    # ---------------- board outline (Edge.Cuts): the GBA outline inset by BOARD_INSET (computed before any Remove) ----------------
    for outline, holes in board:
        poly(pcbnew.Edge_Cuts, outline, 0.1)
        for h in holes: poly(pcbnew.Edge_Cuts, h, 0.1)
    poly(L_GBA, [Gc(*p) for p in item("gba", "Screen lens / bezel outer")["points"]], 0.12)
    aa = item("gba", "LCD active area 240x160"); rect(L_GBA, *Gc(aa["x"], aa["y"]), aa["w"], aa["h"], 0.1, dash=True)
    poly(L_GBA, [Gs(*p) for p in item("gba", "D-pad cross cap")["points"]], 0.15)
    for nm in ("A button", "B button", "START", "SELECT", "Power LED"):
        c = item("gba", nm); circ(L_GBA, *Gs(c["x"], c["y"]), c["r"], 0.15)
    for nm in ("L shoulder", "R shoulder"):
        r_ = item("gba", nm); rect(L_GBA, *Gs(r_["x"], r_["y"]), r_["w"], r_["h"], 0.1, dash=True)
    for i in range(1, 6):
        s_ = item("gba", f"Speaker grille slot {i}")["points"]; seg(L_GBA, Gs(*s_[0]), Gs(*s_[1]), 0.3)
    bd = item("gba", "2xAA battery door"); rect(L_GBA, *Gc(bd["x"], bd["y"]), bd["w"], bd["h"], 0.1, dash=True)
    for it in REF["gba"]["items"]:
        if it["name"].startswith("Shell screw"):
            if it["name"] in SCREWS_DROPPED or it["name"] in SCREWS_MOVED:   # reference only: dashed ring + note
                circ(L_GBA, *Gs(it["x"], it["y"]), 0.8, 0.05, dash=True)
                text(L_GBA, Gs(it["x"], it["y"])[0] + 1.2, Gs(it["x"], it["y"])[1],
                     "GBA screw (moved)" if it["name"] in SCREWS_MOVED else "GBA screw (not used)", 0.6)
                if it["name"] in SCREWS_MOVED:                                # our boss: full ring + boss outline
                    mx, my = SCREWS_MOVED[it["name"]]
                    circ(L_GBA, mx, my, 0.8, 0.1); circ(L_GBA, mx, my, SCREW_KEEPOUT_R - 0.5, 0.05, dash=True)
                    text(L_GBA, mx + 3.0, my, "shell screw (off PCB)", 0.6)
            else:
                circ(L_GBA, *Gs(it["x"], it["y"]), 0.8, 0.1)
    if S:
        seg(L_GBA, (gx + XC, gy - 2), (gx + XC, gy + GBA_H + 2), 0.05, dash=True)
        seg(L_GBA, (gx + XC + S, gy - 2), (gx + XC + S, gy + GBA_H + 2), 0.05, dash=True)
    text(L_GBA, gx, gy - 6.0, "GAME BOY ADVANCE (AGB-001) 144.5 x 82 x 24.5%s — outline from US D452,280; controls from AGB-CPU-10 scan"
         % (f", WIDENED +{S:g} mm at the centre line -> {GBA_W + S:g} x {GBA_H:g}" if S else ""), 1.4)

    # ---------------- 2090 TFT module (User.2) ----------------
    tcx, tcy = gx + mcx, gy + mcy
    T = lambda x, y: (tcx + sx * x, tcy + y)
    aa_t = item("tft", "LCD active area 57.6 x 43.2")
    poly(L_TFT, [T(*p) for p in item("tft", "module_pcb_outline")["points"]], 0.25)
    poly(L_TFT, [T(*p) for p in item("tft", "module PCB internal cutout")["points"]], 0.1)
    for it in REF["tft"]["items"]:
        if it["name"].startswith("mounting_hole U$"):
            circ(L_TFT, *T(it["x"], it["y"]), it["r"], 0.15); circ(L_TFT, *T(it["x"], it["y"]), 2.2, 0.08, dash=True)
    for hdr in ("JP1 header", "JP2 header"):
        h = item("tft", hdr); rect(L_TFT, *T(h["x"], h["y"]), h["w"], h["h"], 0.12)
        p1 = item("tft", hdr.split()[0] + " pin 1"); circ(L_TFT, *T(p1["x"], p1["y"]), 0.6, 0.2)
    for nm, w in (("TFT panel outline", 0.2), ("CTP viewing area", 0.1)):
        r_ = item("tft", nm); rect(L_TFT, *T(r_["x"], r_["y"]), r_["w"], r_["h"], w)
    rect(L_TFT, *T(aa_t["x"], aa_t["y"]), aa_t["w"], aa_t["h"], 0.1, dash=True)
    text(L_TFT, gx, gy - 3.3, "ADAFRUIT 2090 2.8\" TFT — PCB 81.3 x 62.5 (I-shape), holes 76.2 x 57.15, glass 16.7 above mainboard", 1.1)
    for hdr, lab in (("JP1 header", "JP1 8-bit (pin1 o)"), ("JP2 header", "JP2 mech")):
        h = item("tft", hdr); x_, y_ = T(h["x"], h["y"] + 34.5); text(L_TFT, x_ - 3.5, y_, lab, 0.9)

    # ---------------- our parts (User.Drawings) ----------------
    bw, bh, bb = BATTERY; bx, by = gx + pos["battery"][0], gy + pos["battery"][1]
    rect(L_PART, bx, by, bw, bh, 0.2); rect(L_PART, bx, by, bw + 2 * bb, bh + 2 * bb, 0.1, dash=True)
    text(L_PART, bx - bw / 2, by + bh / 2 + bb + 4.5, "BATTERY Adafruit 2011 (PKCELL LP-803860) 62 x 38.5 x 8.5 (+2 buffer)", 1.0)
    sd, sb = SPEAKER; spx, spy = gx + pos["speaker"][0], gy + pos["speaker"][1]
    circ(L_PART, spx, spy, sd / 2, 0.2); circ(L_PART, spx, spy, sd / 2 + sb, 0.1, dash=True)
    circ(L_PART, spx, spy, SPK_OPENING / 2, 0.08, dash=True)
    text(L_PART, spx - 14, spy + sd / 2 + sb + 2.0, "SPEAKER CES-2704 Ø27 x 4.9 (+1.5), opening Ø23.4", 1.0)
    ld, lb = LRA; lx, ly = Gs(*LRA_AT)
    circ(L_PART, lx, ly, ld / 2, 0.2); circ(L_PART, lx, ly, ld / 2 + lb, 0.1, dash=True)
    text(L_PART, lx - 5, ly + ld / 2 + lb + 2.0, "LRA Ø8 x 3.2 (+1)", 1.0)
    iw, ih, iq = IMU; ix, iy = Gc(*IMU_AT)
    rect(L_PART, ix, iy, iw, ih, 0.15); circ(L_PART, ix, iy, iq, 0.08, dash=True)
    text(L_PART, ix - 8, iy + iq + 1.8, "IMU LSM6DSOX 2.5x3", 0.9)
    text(L_PART, ix - 8, iy + iq + 3.2, "(quiet zone: no holes/connectors/LRA)", 0.7)

    # ---------------- fit notes ----------------
    notes = [f"{nm}: {'clear %.1f' % d if d >= 0 else 'OVERLAPS %.1f' % -d}" for nm, d in clr.items()]
    for n in notes: print("  fit:", n)
    ny = gy + 92
    text(L_NOTE, gx, ny - 2.5, "KEY: User.2 = Adafruit 2090 module · User.Drawings = our battery/speaker/LRA/IMU · User.Eco1 = GBA shell, "
                             "screen (dashed = 61.2 x 40.8 active), controls, grille, 2xAA door (dashed) · dashed rings = handling buffers", 1.0)
    text(L_NOTE, gx, ny, f"FIT (2090 {'PCB' if CENTER_ON == 'module' else 'active area'} centred; controls need "
                         f">= {CONTROL_CLEARANCE:g} mm to the module outline, mm): " + "; ".join(notes), 1.0)
    text(L_NOTE, gx, ny + 2.5, "DEPTH: cell sits IN the board cutout under the 2090 (module back is 11 above the board; 8.5 cell + 2.65 module parts fit) "
                             "-> 2090 glass 16.7 above board + 1.6 PCB + ~1 back-side parts + ~3 shells = ~22 mm vs GBA 24.5", 1.0)

    print("  moved: battery up %.2f mm; speaker right %.2f / down %.2f mm (buffers >= %.1f inside the outline)"
          % (BATTERY_AT[1] - pos["battery"][1], pos["speaker"][0] - side(SPEAKER_AT[0]),
             pos["speaker"][1] - SPEAKER_AT[1], FIT_MARGIN))
    pcbnew.SaveBoard(PATH, B)
    print("drawn: GBA widened +%.1f mm -> %.1f x %.1f; 2090 centre (%.2f,%.2f) board mm, centred on %s%s"
          % (S, GBA_W + S, GBA_H, tcx, tcy, CENTER_ON, ", flipped" if TFT_FLIP else ""))
    sys.stdout.flush(); os._exit(0)


if __name__ == "__main__":
    main()
