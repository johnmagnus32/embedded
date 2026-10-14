"""back_openings.py — the back shell's features (DESIGN.md section 4) and the openings in the shell rims for the edge
controls and USB-C (section 5).

apply_back(back)            -> back:  4 screw bosses (+ webs into the wall where they touch it), Ø2.5 holes and Ø4.2
                                       counterbores from the back face, 0.5 mm floor reliefs under the 2090 standoff
                                       screw heads, the battery fence ring (lead gap toward J8, bottom stop), the
                                       speaker cradle (3 arc fingers with a seat lip and a lead-in), the six
                                       board-locator pockets in the wall's inner face (rev 2), two board-support
                                       posts under A / B (rev 2.1).
apply_front_openings(front) -> front: rim notches for Vol+ / Vol- / L / R + the cap-lip recess round each (rev 2),
                                       the power-slider slot, the USB-C stadium (rim-open below its centre line so
                                       J7 can come in along +Z, rev 2.1) + its outer overmold relief.
apply_back_openings(back)   -> back:  the USB-C stadium (z 1.35) and relief (z -0.25) dip below SPLIT, so the back
                                       shell's rim gets the matching notch. Apply it to the back shell as well.

Frame, helpers and the shared contracts: params.py. `python back_openings.py` runs the fit self-test.
"""
import math
from build123d import Axis, Box, Cone, Cylinder, Face, Pos, Rot, Vector, Wire, offset, revolve
from params import (D, MECH, P, T, SPLIT, WALL, Z_BACK, Z_FLOOR_IN, SILHOUETTE, BOSS_D, SCREW_CLEAR_D, CBORE_D,
                    CBORE_DEPTH, SHELL_SCREWS, STANDOFFS, EDGE_KEYS, EDGE_NOTCH, EDGE_LIP_RECESS, POWER_SLOT, USBC,
                    BOARD_LOCATORS, FINGER, face, prism, cyl, speaker, part_xy)

# ---------------------------------------------------------------- conventions
CUT = 0.2            # every cutter overshoots the face it opens by this much (no coplanar faces)
CUT_OUT = 1.0        # ... and reaches this far past the outer surface
FUSE = 0.1           # added solids start this far inside the face they grow from (clean unions)

# ---------------------------------------------------------------- outline queries (enclosure frame)
SIL_PTS = [P(x, y) for x, y in MECH["silhouette"]]               # shell outer surface
BOARD_PTS = [P(x, y) for x, y in D["board"]["outline"]]
BOARD_HOLES = [P(h["x"], h["y"]) for h in D["board"]["holes"]]
Y_TOP_OUT = max(y for _, y in SIL_PTS)                           # the flat top wall's outer face (y -30.0)

def _closest(x, y, pts):
    """closest point on the closed polygon pts to (x, y) -> (distance, (qx, qy))."""
    best = (math.inf, None)
    for (ax, ay), (bx, by) in zip(pts, pts[1:] + pts[:1]):
        dx, dy = bx - ax, by - ay
        L2 = dx * dx + dy * dy
        t = 0.0 if L2 == 0 else max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / L2))
        q = (ax + t * dx, ay + t * dy)
        d = math.hypot(x - q[0], y - q[1])
        if d < best[0]: best = (d, q)
    return best

def _rbox(cx, cy, ang_deg, along, across, z0, z1):
    """box centred (cx, cy), `along` long in direction ang_deg, `across` wide, spanning z0..z1."""
    return Pos(cx, cy, (z0 + z1) / 2) * Rot(0, 0, ang_deg) * Box(along, across, z1 - z0)

def _wall_cut(ox, oy, ux, uy, across, z0, z1, depth=WALL):
    """cutter through the wall at outer-surface point (ox, oy) along outward unit (ux, uy): `depth` + CUT inward,
    CUT_OUT outward, `across` wide along the wall, z0..z1."""
    L = depth + CUT + CUT_OUT
    mid = (CUT_OUT - depth - CUT) / 2                                 # box centre, along u from the outer point
    return _rbox(ox + ux * mid, oy + uy * mid, math.degrees(math.atan2(uy, ux)), L, across, z0, z1)

def _inner_pocket(ox, oy, ux, uy, across, depth, z0, z1):
    """pocket in the wall's inner face (WALL in from outer-surface point (ox, oy) along -(ux, uy)): `depth` into the
    wall, CUT out into the cavity, `across` wide along the wall, z0..z1. All walls here are straight: the inner face
    is 2.000 +-0.006 in from every contract point (EDGE_KEYS outer, BOARD_LOCATORS at + 2.5 n)."""
    n0, n1 = WALL - depth, WALL + CUT                                 # depth range in from the outer point
    m = (n0 + n1) / 2
    return _rbox(ox - ux * m, oy - uy * m, math.degrees(math.atan2(uy, ux)), n1 - n0, across, z0, z1)

# ================================================================ section 4: back shell
# Screw bosses: Ø BOSS_D from the floor up to the board bottom (z 0; the board is clamped between it and the front boss
# tip at z SPLIT); the off-board bottom-right screw's boss rises to SPLIT to meet the front boss tip. Back-face
# counterbore CBORE_D x CBORE_DEPTH (leaves 1.6 of plastic under the M2 head), SCREW_CLEAR_D through.
BOSS_TOP_ON_BOARD = 0.0
BOSS_TOP_OFF_BOARD = SPLIT
WEB_GAP = 1.0        # choice: a boss closer than this to the inner wall gets a BOSS_D-wide web into the wall
WEB_KEEP = 1.0       # the web stops this far inside the outer surface (the R3 back fillet is 0.35 deep at z -2.0)
HOLE_MATCH = 0.05    # a shell screw within this of a board hole goes through the board

def _on_board(x, y):
    return any(math.hypot(x - hx, y - hy) < HOLE_MATCH for hx, hy in BOARD_HOLES)

def _boss_top(x, y):
    return BOSS_TOP_ON_BOARD if _on_board(x, y) else BOSS_TOP_OFF_BOARD

def _web(x, y, z0, z1):
    """BOSS_D-wide block from the boss centre toward the nearest outer-surface point, clipped WEB_KEEP inside it."""
    d, (qx, qy) = _closest(x, y, SIL_PTS)
    if d - WALL - BOSS_D / 2 >= WEB_GAP: return None
    ux, uy = (qx - x) / d, (qy - y) / d
    blk = _rbox(x + ux * d / 2, y + uy * d / 2, math.degrees(math.atan2(uy, ux)), d, BOSS_D, z0, z1)
    return blk & prism(offset(SILHOUETTE, amount=-WEB_KEEP), z0, z1)

def screw_bosses():
    out = None
    for x, y in SHELL_SCREWS:
        z0, z1 = Z_FLOOR_IN - FUSE, _boss_top(x, y)
        b = cyl(x, y, BOSS_D, z0, z1)
        w = _web(x, y, z0, z1)
        if w is not None: b = b + w
        out = b if out is None else out + b
    return out

def screw_cuts():
    """clearance holes up through each boss + counterbores from the back face."""
    out = None
    for x, y in SHELL_SCREWS:
        c = cyl(x, y, SCREW_CLEAR_D, Z_BACK - CUT, _boss_top(x, y) + CUT) + \
            cyl(x, y, CBORE_D, Z_BACK - CUT, Z_BACK + CBORE_DEPTH)
        out = c if out is None else out + c
    return out

# Floor reliefs under the four 2090 standoff screw heads (pan heads reach z -1.75, 0.25 above the floor).
RELIEF_D, RELIEF_DEPTH = 6.0, 0.5

def standoff_reliefs():
    out = None
    for x, y in STANDOFFS:
        r = cyl(x, y, RELIEF_D, Z_FLOOR_IN - RELIEF_DEPTH, Z_FLOOR_IN + CUT)
        out = r if out is None else out + r
    return out

# Battery: the cell (params.battery(), on the floor) sits in the board's cut-out with a 2.0 gap left / top / right.
# One fence ring FENCE_T thick at FENCE_CLR from the cell: its left, top and right sides stand in that gap (0.4 to the
# board edge too, so they also locate the board), its bottom side is the bottom stop (0.8-2.6 short of the inner
# wall). The right side has a LEAD_GAP opening where the PCM leads leave the cell's east end (centre) for J8.
CELL = MECH["battery"]
CELL_AT = P(CELL["x"], CELL["y"])

def _cell_gap():
    """cut-out edge to the cell: board outline distance from the cell's left / top / right edge midpoints (2.0)."""
    (cx, cy), w, h = CELL_AT, CELL["w"] / 2, CELL["h"] / 2
    return min(_closest(x, y, BOARD_PTS)[0] for x, y in ((cx - w, cy), (cx, cy + h), (cx + w, cy)))

BATT_GAP = _cell_gap()                       # 2.0 (cut-out x 62.25..128.25, top y -65.25)
FENCE_T = 1.2
FENCE_CLR = (BATT_GAP - FENCE_T) / 2         # 0.4 each side
FENCE_TOP = 4.0      # choice: 6.0 of the 8.5 cell; 6 mm under the 2090's lowest back parts (z 9.95)
LEAD_GAP = 10.0      # choice: two 24 AWG leads leave the PCM "near the middle" of the 38.5 short end

def _cell_rect():
    (cx, cy), w, h = CELL_AT, CELL["w"] / 2, CELL["h"] / 2
    return face([(cx - w, cy - h), (cx + w, cy - h), (cx + w, cy + h), (cx - w, cy + h)], board_frame=False)

def battery_fence():
    (cx, cy), z0, z1 = CELL_AT, Z_FLOOR_IN - FUSE, FENCE_TOP
    ring = prism(offset(_cell_rect(), amount=FENCE_CLR + FENCE_T), z0, z1) - \
           prism(offset(_cell_rect(), amount=FENCE_CLR), z0 - CUT, z1 + CUT)
    gap_x = cx + CELL["w"] / 2 + FENCE_CLR + FENCE_T / 2
    return ring - _rbox(gap_x, cy, 0, FENCE_T + 2 * CUT, LEAD_GAP, z0 - CUT, z1 + CUT)

# Speaker cradle: three arc fingers (ID = speaker + 0.4, wall SPK_WALL) rise from the floor through the board's
# speaker cut-out (>= SPK_CUT_CLR to its edge) to the speaker's bottom, where an inward seat lip carries its back rim
# up against the front seal; above the seat they continue SPK_LOCATE_H to centre it. The gaps between fingers face J5
# (the leads, N), the SW and the SE.
SPK = MECH["speaker"]
SPK_AT = P(SPK["x"], SPK["y"])
SPK_ID = SPK["d"] + 0.4                      # 27.4: 0.2 radial play around the Ø27 +-0.2 frame
SPK_WALL = 1.0
SPK_CUT_CLR = 0.3                            # to the board's speaker cut-out
# the board's cut-out is a polygonised r 15.0 arc whose chords sag to r 14.991: cap the fingers' outer radius by the
# real outline, so the wall is 0.991 there (1.0 asked) and the clearance holds at 0.3
SPK_R_OUT = min(SPK_ID / 2 + SPK_WALL, _closest(*SPK_AT, BOARD_PTS)[0] - SPK_CUT_CLR)
SPK_FINGER_DEG = 40.0
SPK_FINGER_AT = (30.0, 150.0, 270.0)        # finger centres, deg CCW from +X (gap centres 90 = J5, 210, 330)
SPK_SEAT_ID = 25.6                           # choice: lip bears on the outer 0.7 of the back rim (>= 0.3 for a
                                             # Ø26.8 frame pushed fully off-centre)
SPK_SEAT_T = 1.0                             # flat lip thickness; a 45-degree chamfer under it prints unsupported
SPK_LOCATE_H = 1.5                           # choice: finger height past the seat (the rear 1.5 of the 4.9 frame)
SPK_LEAD_IN = 0.5                            # 45-deg chamfer on the fingers' top inner edge (rev 2.1): they meet the
                                             # speaker rim ~1.5 before the finger / pocket locators engage (1.3)

def _speaker_bottom():
    return speaker().bounding_box().min.Z     # 14.1: sealed to the plate through 0.3 foam (params.speaker)

def _seat_lip(z_top):
    """seat ring revolved about Z through the origin (place it with Pos): flat top at z_top, 45-degree chamfer
    underneath, overlapping the finger wall by FUSE."""
    ri, rs = SPK_ID / 2 + FUSE, SPK_SEAT_ID / 2
    w = ri - rs
    pts = [(ri, 0, z_top - SPK_SEAT_T - w), (rs, 0, z_top - SPK_SEAT_T), (rs, 0, z_top), (ri, 0, z_top)]
    return revolve(Face(Wire.make_polygon([Vector(*p) for p in pts], close=True)), Axis.Z)

def _sector(r, a0, a1, z0, z1, n=16):
    """pie slice about the speaker axis (radius r, angles a0..a1 deg), z0..z1."""
    cx, cy = SPK_AT
    arc = [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
            cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]
    return prism(face([(cx, cy)] + arc, board_frame=False), z0, z1)

def speaker_cradle():
    cx, cy = SPK_AT
    z0, z_seat = Z_FLOOR_IN - FUSE, _speaker_bottom()
    z1 = z_seat + SPK_LOCATE_H
    ring = cyl(cx, cy, 2 * SPK_R_OUT, z0, z1) - cyl(cx, cy, SPK_ID, z0 - CUT, z1 + CUT)
    r, c = SPK_ID / 2, SPK_LEAD_IN
    ring = ring - Pos(cx, cy, z1 - c + (c + CUT) / 2) * Cone(r, r + c + CUT, c + CUT)     # lead-in, CUT over the top
    ring = ring + Pos(cx, cy, 0) * _seat_lip(z_seat)
    out = None
    for a in SPK_FINGER_AT:
        f = ring & _sector(SPK_R_OUT + 1.0, a - SPK_FINGER_DEG / 2, a + SPK_FINGER_DEG / 2, z0 - CUT, z1 + CUT)
        out = f if out is None else out + f
    return out

# Board-locator pockets (rev 2): the front shell's six fingers hang below the split into the 0.5 gap round the board
# edge; behind each the back wall gets a pocket in its inner face (the wall line, LOC_EDGE_TO_WALL outside the board
# edge 'at'), LOC_POCKET_DEPTH deep along n, the finger's width + 2 pocket_clr along the edge, from pocket_clr under
# the finger's tip up to SPLIT (open at the rim). front_struct puts the finger's outer face pocket_depth (0.7) into the
# wall, so the pocket is pocket_depth + pocket_clr = 0.9 deep: 0.2 behind the finger, as round its sides and tip (rev 2.1;
# 0.7 left a zero fit). Wall left behind it: 2.0 - 0.9 = 1.1 (>= LOC_MIN_WALL). DESIGN.md 4 still says 0.7.
LOC_EDGE_TO_WALL = 0.5           # board edge -> wall's inner face (DESIGN.md: the board edge is 2.5 inside the outer face)
LOC_EDGE_TO_OUT = LOC_EDGE_TO_WALL + WALL
LOC_MIN_WALL = 1.0               # task rule: wall left behind a pocket
LOC_POCKET_DEPTH = round(FINGER["pocket_depth"] + FINGER["pocket_clr"], 3)      # 0.9

def locator_pockets():
    f = FINGER
    w, z0 = f["w"] + 2 * f["pocket_clr"], f["z0"] - f["pocket_clr"]
    assert WALL - LOC_POCKET_DEPTH >= LOC_MIN_WALL
    out = None
    for l in BOARD_LOCATORS.values():
        (ax, ay), (nx, ny) = l["at"], l["n"]
        p = _inner_pocket(ax + nx * LOC_EDGE_TO_OUT, ay + ny * LOC_EDGE_TO_OUT, nx, ny, w, LOC_POCKET_DEPTH,
                          z0, SPLIT + CUT)
        out = p if out is None else out + p
    return out

# Board-support posts (rev 2.1): nothing else carries the board's underside on the right grip, so a hard A / B press
# bent ~35 mm of FR4 into the J4 / 2090 header and the switch joints. A post under each switch, POST_GAP short of the
# board's bottom face (no preload against the boss clamp; it only stops the bending). No bottom-side parts at x > 136;
# J5's through-hole tails (150.30 / 152.84, -68.30) are >= 6.7 from the post axes.
POST_AT = (part_xy("SW6"), part_xy("SW7"))       # A (156.88, -61.92), B (143.94, -66.25)
POST_D = 4.0
POST_GAP = 0.05

def board_posts():
    out = None
    for x, y in POST_AT:
        p = cyl(x, y, POST_D, Z_FLOOR_IN - FUSE, -POST_GAP)
        out = p if out is None else out + p
    return out

def apply_back(back):
    """DESIGN.md section 4 on the bare back shell (add the rim notch with apply_back_openings)."""
    back = back + screw_bosses() + battery_fence() + speaker_cradle() + board_posts()
    return back - screw_cuts() - standoff_reliefs() - locator_pockets()

# ================================================================ section 5: openings in the rims
# Edge keys (SKRT side-push, axis z 3.3): an EDGE_NOTCH notch per key, centred on its axis, open at the rim (z SPLIT),
# through the wall along the key's outward 'dir' (each axis is normal to its wall segment).
def edge_notches():
    w, h = EDGE_NOTCH
    out = None
    for e in EDGE_KEYS.values():
        (ox, oy), (ux, uy) = e["outer"], e["dir"]
        c = _wall_cut(ox, oy, ux, uy, w, SPLIT - CUT, SPLIT + h)
        out = c if out is None else out + c
    return out

# Edge-cap lip recess (rev 2): round each notch a pocket in the wall's inner face, EDGE_LIP_RECESS depth along the
# key's dir, w wide along the wall, from the split (open at the rim) up to z1; the cap's lip sits in it so the cap is
# >= 0.8 thick behind the wall. Wall left behind it: 2.0 - 0.3 = 1.7.
def edge_lip_recesses():
    r = EDGE_LIP_RECESS
    out = None
    for e in EDGE_KEYS.values():
        c = _inner_pocket(*e["outer"], *e["dir"], r["w"], r["depth"], SPLIT - CUT, r["z1"])
        out = c if out is None else out + c
    return out

# Power slide (SW1): POWER_SLOT len is the whole slot along the wall (2.0 slider stem + 1.6 travel + 0.4 play,
# openings.md), so the slider's travel is inside it; z z0..z0+h, open at the rim.
def power_slot():
    s = POWER_SLOT; a = math.radians(s["angle_deg"])
    nx, ny = math.sin(a), -math.cos(a)                    # outward normal of the bottom-left wall (down-left)
    assert s["len"] > s["travel"]
    return _wall_cut(*s["at"], nx, ny, s["len"], s["z0"] - CUT, s["z0"] + s["h"])

# USB-C (J7): a w x h stadium through the top wall at x, centred z (z 1.35..5.15, 0.25-0.29 around the 8.94 x 3.26
# shell), plus the overmold relief on the outside, relief_d deep = flush with the receptacle face (1.09 inside).
USBC_RELIEF_R = 2.5  # choice: relief corner radius (openings.md R2.5-3)

def _stadium_y(cx, cz, w, h, y0, y1):
    """stadium section (w along X, h along Z, round ends) swept along Y over y0..y1."""
    L, ym, r = abs(y1 - y0), (y0 + y1) / 2, h / 2
    s = Pos(cx, ym, cz) * Box(w - h, L, h)
    for sx in (-1, 1):
        s = s + Pos(cx + sx * (w / 2 - r), ym, cz) * Rot(90, 0, 0) * Cylinder(r, L)
    return s

def _rrect_y(cx, cz, w, h, r, y0, y1):
    """rounded-rectangle section (w along X, h along Z, corner r) swept along Y over y0..y1."""
    L, ym = abs(y1 - y0), (y0 + y1) / 2
    s = Pos(cx, ym, cz) * Box(w - 2 * r, L, h) + Pos(cx, ym, cz) * Box(w, L, h - 2 * r)
    for sx in (-1, 1):
        for sz in (-1, 1):
            s = s + Pos(cx + sx * (w / 2 - r), ym, cz + sz * (h / 2 - r)) * Rot(90, 0, 0) * Cylinder(r, L)
    return s

def usbc_cutters():
    """stadium + relief, and (rev 2.1) the stadium's full width w from its centre line down to its bottom (z 1.35,
    past SPLIT): J7's shell overhangs 0.91 into the wall and comes in along +Z with the board, and the round ends were
    only +-3.79 wide at the rim against its +-4.47 (and left 0.06 knife edges at the split). Cut from both shells."""
    u, y = USBC, Y_TOP_OUT
    pocket = _stadium_y(u["x"], u["z"], u["w"], u["h"], y + CUT_OUT, y - WALL - CUT)
    rim = Pos(u["x"], y + (CUT_OUT - WALL - CUT) / 2, u["z"] - u["h"] / 4) * Box(u["w"], WALL + CUT + CUT_OUT, u["h"] / 2)
    relief = _rrect_y(u["x"], u["z"], u["relief_w"], u["relief_h"], USBC_RELIEF_R, y + CUT_OUT, y - u["relief_d"])
    return pocket + rim + relief

def apply_front_openings(front):
    return front - edge_notches() - edge_lip_recesses() - power_slot() - usbc_cutters()

def apply_back_openings(back):
    """the USB-C stadium / relief below SPLIT. Edge notches and the power slot start at SPLIT: nothing to cut there."""
    return back - usbc_cutters()

# ================================================================ self-test (python back_openings.py)
J7_SHELL = (8.94, 3.26, 7.35)    # TYPE-C-31-M-12 shell: stadium W x H on the board top, length (C165948)
J7_FACE_Y = -31.09               # receptacle face (pads / courtyard; the 3D model is 2.22 off)
SW1_ORIGIN, SW1_ROT = P(43.4356, 97.5653), -13.36    # footprint origin and rotation (controls.md)
EPS_V = 0.001                    # report intersection volumes above this (mm3)
BOX_ARTEFACTS = {                # board.json envelopes that are boxes where the real part is not (checked below)
    "J7": "courtyard box corners vs the stadium's round ends; the real stadium shell clears",
    "SW1": "axis-aligned bounding-box courtyard (no footprint courtyard); the real body + lever clear",
}

def _j7_real():
    w, h, L = J7_SHELL
    return _stadium_y(USBC["x"], SPLIT + h / 2, w, h, J7_FACE_Y, J7_FACE_Y - L)

def _sw1_real():
    """MSK12C02-HB body 8 x 2.8 x 1.40 + the lever's swept envelope (both positions), from the datasheet."""
    a = math.radians(SW1_ROT); ox, oy = SW1_ORIGIN
    ey = (math.sin(a), -math.cos(a))                     # footprint +y (toward the lever) in the enclosure frame
    at = lambda ly: (ox + ey[0] * ly, oy + ey[1] * ly)
    return _rbox(*at(0.55), SW1_ROT, 8.0, 2.8, SPLIT, SPLIT + 1.40) + \
           _rbox(*at(2.675), SW1_ROT, 2.9, 1.45, SPLIT + 0.2, SPLIT + 1.2)

def _envelopes_by_ref():
    """params.part_envelopes(), keyed by reference (same rules)."""
    out = {}
    for p in D["parts"]:
        if p["height"] < 0.1: continue
        try: f = face(p["courtyard"])
        except Exception: continue
        out[p["ref"]] = prism(f, T, T + p["height"]) if p["side"] == "top" else prism(f, -p["height"], 0)
    return out

def _iv(a, b):
    r = a & b
    return r.volume if r is not None else 0.0

def _void(shape, x, y, z):
    return not any(s.is_inside(Vector(x, y, z)) for s in shape.solids())

def _wall_depth(ox, oy, ux, uy, t, inner=True):
    """depth of the wall's inner (or outer) face in from the point (ox, oy) along -(ux, uy), t along the wall."""
    from build123d import Line
    p0 = (ox - uy * t + ux, oy + ux * t + uy, 0)                         # 1 mm outside
    wire = (offset(SILHOUETTE, amount=-WALL) if inner else SILHOUETTE).faces()[0].outer_wire()
    hit = Line(p0, (p0[0] - 8 * ux, p0[1] - 8 * uy, 0)).intersect(wire)
    return min((p0[0] - v.X) * ux + (p0[1] - v.Y) * uy for v in hit.vertices()) - 1

def _fingers(n_out):
    """the front shell's six locating fingers as plain boxes (front_struct owns the real ones): FINGER gap .. n_out
    out from the board edge along n, FINGER w wide, z FINGER z0..SPLIT."""
    f, out = FINGER, None
    for l in BOARD_LOCATORS.values():
        (ax, ay), (nx, ny) = l["at"], l["n"]
        m = (f["gap"] + n_out) / 2
        b = _rbox(ax + nx * m, ay + ny * m, math.degrees(math.atan2(ny, nx)), n_out - f["gap"], f["w"], f["z0"], SPLIT)
        out = b if out is None else out + b
    return out

def _iv_self(c):
    """overlap between a compound's solids (sum over pairs)."""
    s = c.solids()
    return sum(_iv(a, b) for i, a in enumerate(s) for b in s[i + 1:])

def _selftest():
    import time
    from params import board_solid, part_envelopes, module_2090, battery, lra
    from shell import shells
    t0 = time.time()
    f0, b0 = shells()
    front = apply_front_openings(f0)
    back = apply_back_openings(apply_back(b0))
    print(f"built in {time.time() - t0:.1f} s")
    for n, s0, s in (("front", f0, front), ("back", b0, back)):
        bb = s.bounding_box()
        print(f"{n}: {len(s.solids())} solid(s), valid {s.is_valid}, z {bb.min.Z:.2f}..{bb.max.Z:.2f}, "
              f"{s0.volume:.0f} -> {s.volume:.0f} mm3")

    top, bot = part_envelopes()
    ctx = {"board": board_solid(), "parts top": top, "parts bottom": bot, "2090": module_2090(),
           "battery": battery(), "speaker": speaker(), "lra": lra()}
    print("\nintersections (mm3): bare shell -> with features")
    flagged = []
    for n, s0, s in (("front", f0, front), ("back", b0, back)):
        for c, g in ctx.items():
            v0, v = _iv(s0, g), _iv(s, g)
            mark = "  <-- CHECK" if v > EPS_V else ""
            print(f"  {n:5s} x {c:12s} {v0:9.4f} -> {v:9.4f}{mark}")
            if v > EPS_V and c.startswith("parts"): flagged.append((n, s0, s))
    if flagged:
        env = _envelopes_by_ref()
        for n, s0, s in flagged:
            for ref, e in env.items():
                v = _iv(s, e)
                if v > EPS_V:
                    print(f"    {n} x {ref}: {_iv(s0, e):.4f} -> {v:.4f} mm3 "
                          f"({BOX_ARTEFACTS.get(ref, 'NOT EXPECTED')})")
    print(f"  front x back (after features): {_iv(front, back):.4f}")

    print("\nreal-shape checks (the board.json envelopes are boxes)")
    j7 = _j7_real()
    print(f"  J7 stadium shell x front {_iv(front, j7):.4f}, x back {_iv(back, j7):.4f}; "
          f"clearance to front {front.distance_to(j7):.3f}, to back {back.distance_to(j7):.3f}")
    sw1 = _sw1_real()
    print(f"  SW1 body+lever x front {_iv(front, sw1):.4f}; clearance to front {front.distance_to(sw1):.3f}")

    print("\nclearances (mm)")
    fingers, fence, bosses = speaker_cradle(), battery_fence(), screw_bosses()
    print(f"  speaker fingers: board {fingers.distance_to(ctx['board']):.3f} (>= {SPK_CUT_CLR}), "
          f"front shell {fingers.distance_to(front):.3f}, 2090 {fingers.distance_to(ctx['2090']):.3f}, "
          f"parts top {fingers.distance_to(top):.3f}, speaker {fingers.distance_to(ctx['speaker']):.3f} (seat contact)")
    sb = speaker().bounding_box()
    print(f"  speaker z {sb.min.Z:.2f}..{sb.max.Z:.2f}, fingers r {SPK_ID / 2:.3f}..{SPK_R_OUT:.3f} "
          f"(wall {SPK_R_OUT - SPK_ID / 2:.3f}), seat lip to r {SPK_SEAT_ID / 2:.2f}, "
          f"top z {fingers.bounding_box().max.Z:.2f}")
    print(f"  battery fence: board {fence.distance_to(ctx['board']):.3f}, "
          f"cell {fence.distance_to(ctx['battery']):.3f}, "
          f"front shell {fence.distance_to(front):.3f}, parts top {fence.distance_to(top):.3f}, "
          f"parts bottom {fence.distance_to(bot):.3f}, 2090 {fence.distance_to(ctx['2090']):.3f}")
    print(f"  screw bosses: parts bottom {bosses.distance_to(bot):.3f}, board {bosses.distance_to(ctx['board']):.3f} "
          f"(0 = clamping contact), battery {bosses.distance_to(ctx['battery']):.3f}")
    print(f"  floor reliefs: parts bottom {standoff_reliefs().distance_to(bot):.3f}")
    posts = board_posts()
    print(f"  board posts (rev 2.1): to the board {posts.distance_to(ctx['board']):.3f} (= POST_GAP {POST_GAP}), "
          f"parts bottom {posts.distance_to(bot):.3f}, J5 envelope {posts.distance_to(_envelopes_by_ref()['J5']):.3f}, "
          f"cradle {posts.distance_to(speaker_cradle()):.3f}, battery {posts.distance_to(ctx['battery']):.3f}")
    c = speaker_cradle()
    print(f"  cradle lead-in (rev 2.1): fingers to the speaker {c.distance_to(ctx['speaker']):.3f}, top z "
          f"{c.bounding_box().max.Z:.2f}, {len(c.solids())} solids")

    print("\nopenings: a point on each axis at mid-wall must be void, one 1 mm past the opening's side solid")
    def probe(name, shape, ox, oy, ux, uy, z, half_w):
        mx, my = ox - ux * WALL / 2, oy - uy * WALL / 2
        s = half_w + 1.0
        print(f"  {name:9s} axis void {_void(shape, mx, my, z)}, "
              f"beside solid {not _void(shape, mx - uy * s, my + ux * s, z)}")
    for k, e in EDGE_KEYS.items():
        probe(k, front, *e["outer"], *e["dir"], 3.3, EDGE_NOTCH[0] / 2)
    a = math.radians(POWER_SLOT["angle_deg"])
    probe("POWER", front, *POWER_SLOT["at"], math.sin(a), -math.cos(a), 2.3, POWER_SLOT["len"] / 2)
    probe("USB-C", front, USBC["x"], Y_TOP_OUT, 0, 1, USBC["z"], USBC["relief_w"] / 2)
    yb = Y_TOP_OUT - (USBC["relief_d"] + WALL) / 2                      # mid-way through the wall behind the relief
    print(f"  USB-C wall behind the relief, 0.5 beside the stadium: solid "
          f"{not _void(front, USBC['x'] + USBC['w'] / 2 + 0.5, yb, USBC['z'])}")
    print(f"  USB-C notch in the back rim: z 1.5 void {_void(back, USBC['x'], Y_TOP_OUT - 1.0, 1.5)}, "
          f"relief z 0 void {_void(back, USBC['x'], Y_TOP_OUT - 0.5, 0.0)}, behind relief z 0 solid "
          f"{not _void(back, USBC['x'], Y_TOP_OUT - 1.5, 0.0)}")
    sep = math.dist(EDGE_KEYS["VOL_UP"]["outer"], EDGE_KEYS["VOL_DOWN"]["outer"]) - EDGE_NOTCH[0]
    print(f"  wall left between the Vol+ / Vol- notches {sep:.2f}; "
          f"behind the USB-C relief {WALL - USBC['relief_d']:.2f}")

    print("\nrev 2: edge-cap lip recesses (front)")
    rec = edge_lip_recesses()
    r = EDGE_LIP_RECESS
    for k, e in EDGE_KEYS.items():
        (ox, oy), (ux, uy) = e["outer"], e["dir"]
        nw, no = _wall_depth(ox, oy, ux, uy, 0.0), _wall_depth(ox, oy, ux, uy, 0.0, inner=False)
        at = lambda n, s, z: _void(front, ox - ux * n - uy * s, oy - uy * n + ux * s, z)
        print(f"  {k:9s} depth {nw - (WALL - r['depth']):.3f}, wall behind {WALL - r['depth'] - no:.3f}; recess void "
              f"(z 6.0 {at(nw - r['depth'] / 2, 0, 6.0)}, 0.2 in from its side {at(nw - r['depth'] / 2, r['w'] / 2 - 0.2, 6.0)}), "
              f"solid above z1 {not at(nw - r['depth'] / 2, 0, r['z1'] + 0.2)}, "
              f"beside {not at(nw - r['depth'] / 2, r['w'] / 2 + 0.3, 3.3)}, behind {not at(nw - r['depth'] - 0.1, 0, 6.0)}")
    fingers_nom = _fingers(LOC_EDGE_TO_WALL)
    fo = f0 - edge_notches() - power_slot() - usbc_cutters()
    print(f"  material removed {_iv(fo, rec):.3f} mm3 (4 x 0.3 x (4.8 x 5.2 - notch 4.0 x 3.6) = "
          f"{4 * r['depth'] * (r['w'] * (r['z1'] - SPLIT) - EDGE_NOTCH[0] * EDGE_NOTCH[1]):.3f}; shell volumes above "
          f"are only good to ~10 mm3)")
    print(f"  recesses x power slot {_iv(rec, power_slot()):.4f}, x USB-C {_iv(rec, usbc_cutters()):.4f}; "
          f"to the nearest front finger (nominal) {rec.distance_to(fingers_nom):.3f}")

    print("\nrev 2: board-locator pockets (back)")
    pk = locator_pockets()
    f = FINGER
    band = prism(SILHOUETTE, -1.0, SPLIT + 1.0) - prism(offset(SILHOUETTE, amount=-LOC_MIN_WALL), -2.0, SPLIT + 2.0)
    for k, l in BOARD_LOCATORS.items():
        (ax, ay), (nx, ny) = l["at"], l["n"]
        ox, oy = ax + nx * LOC_EDGE_TO_OUT, ay + ny * LOC_EDGE_TO_OUT
        hw = f["w"] / 2 + f["pocket_clr"]
        no = max(_wall_depth(ox, oy, nx, ny, t, inner=False) for t in (-hw, 0.0, hw))
        p = _inner_pocket(ox, oy, nx, ny, 2 * hw, LOC_POCKET_DEPTH, f["z0"] - f["pocket_clr"], SPLIT + CUT)
        at = lambda d, s, z: _void(back, ax + nx * d - ny * s, ay + ny * d + nx * s, z)
        dm = LOC_EDGE_TO_WALL + LOC_POCKET_DEPTH / 2
        print(f"  {k:11s} wall behind {WALL - LOC_POCKET_DEPTH - no:.3f} (>= {LOC_MIN_WALL}: x 1.0 band {_iv(p, band):.4f}); "
              f"void z 1.0 {at(dm, 0, 1.0)} / 0.1 in from its side {at(dm, hw - 0.1, 1.0)}; solid under its floor "
              f"{not at(dm, 0, 0.0)}, beside {not at(dm, hw + 0.3, 1.0)}, behind {not at(LOC_EDGE_TO_WALL + LOC_POCKET_DEPTH + 0.1, 0, 1.0)}")
    bo = b0 + screw_bosses() + battery_fence() + speaker_cradle()
    print(f"  material removed {_iv(bo, pk):.3f} mm3 (6 x {LOC_POCKET_DEPTH} x {2 * hw:.1f} x "
          f"{SPLIT - f['z0'] + f['pocket_clr']:.1f} = {6 * LOC_POCKET_DEPTH * 2 * hw * (SPLIT - f['z0'] + f['pocket_clr']):.3f})")
    others = {"bosses": screw_bosses(), "screw cuts": screw_cuts(), "fence": battery_fence(), "cradle": speaker_cradle(),
              "reliefs": standoff_reliefs(), "USB-C": usbc_cutters()}
    print("  pockets vs other back features (vol / gap): " +
          ", ".join(f"{n} {_iv(pk, g):.4f} / {pk.distance_to(g):.2f}" for n, g in others.items()))
    print(f"  pockets vs each other {_iv_self(pk):.4f}; vs board {_iv(pk, ctx['board']):.4f} / {pk.distance_to(ctx['board']):.3f}")
    fingers_max = _fingers(LOC_EDGE_TO_WALL + f["pocket_depth"])          # out to the real finger's outer face
    print(f"  finger model n {f['gap']}..{LOC_EDGE_TO_WALL} (nominal) x back {_iv(fingers_nom, back):.4f}, gap "
          f"{fingers_nom.distance_to(back):.3f}, to board {fingers_nom.distance_to(ctx['board']):.3f} (= gap {f['gap']}); "
          f"finger out to its real outer face (n ..{LOC_EDGE_TO_WALL + f['pocket_depth']:.1f}) x back "
          f"{_iv(fingers_max, back):.4f}, gap {fingers_max.distance_to(back):.3f} (= pocket_clr {f['pocket_clr']})")
    print(f"total {time.time() - t0:.1f} s")

if __name__ == "__main__":
    _selftest()
