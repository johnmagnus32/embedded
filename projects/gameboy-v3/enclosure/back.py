"""back.py - the gb3 back shell's features and the FR4 battery plate (the back module; contract: CONTRACT.md).

build(ctx) -> {"front_cut", "back_add", "back_cut", "parts", "keepouts", "notes"} (enclosure.py applies them);
export(out_dir) -> [paths]: the battery plate DXF (called by enclosure.export_all; build() writes no files).
Every z comes from params names (stack inputs + flags), so GB3_BACK=hump, the thin cells (GB3_CELL) and
GB3_LCD_PLATE=1 rebuild from the same code; the constants below are this module's own choices.

Built here (CONTRACT.md "Ownership: back.py"):
  cell bay   CELL["floor"] == "fr4": a pocket in the back face for the bought 1.0 FR4 battery plate (flush with the
             back face, at CELL["floor_z"]) and a window through the floor over the cell, so the cell's tape sits on
             the plate. The plate reaches PLATE_LAP under the fences and is bonded to their feet (PLATE_BOND): the
             fences are its lips. Where the cell leads run (no fence) the window opens to the pocket's edge, so no
             printed lip thinner than the fences is left. "printed" (thin cells): the cell's tape on the printed floor.
  fences     CELL["fence_t_max"] thick, CELL["fence_clr"] off the cell, in the 2 mm notch gaps, up to the board's
             bottom-face level, with a 45 deg FENCE_ROOT chamfer on the outside; cut back LEAD_CLR from the cell
             leads' route (core keep-out "cell leads": the top end from the PCM exit east, and the riser corner).
  LRA        a locating ring, ID LRA["ring_id"], with a lead gap toward its pads J2.
  speaker    a cup, ID SPEAKER["cup_id"], SPK_CUP_T wall (<= 1.0: the S2 boss is 0.3 off it, so the two are bridged
             rather than left with a 0.3 slot), SPK_CUP_H tall, a lead-in and a lead gap toward J3; a hex grille
             inside SPEAKER["grille_d"] (inside the foam ring's ID).
  USB-C      a stadium round the receptacle shell's design envelope USBC["z"] (USBC_CLR beside it, CLR_B over and
             under it), open up to the split (the back shell closes from below, past the receptacle), plus the
             USBC["relief"] overmold relief on the plug axis (R USBC_RELIEF_R, grown to hold the sharp 13 x 7),
             whose top (above the split) is also cut from the front rim.
  microSD    a slot round the card path through the top wall, and a finger scallop round it in the outer face.
  reset      the RESET["hole_d"] pinhole with a chamfered mouth and a guide tube up to RESET_GAP under SW1's keystake.
  screws     at every params.BOSSES entry: a BOSS_D back boss (H1-H5 stop at back_top_z, BACK_BOSS_GAP under the
             board's bottom face; the seam screws S1 / S2 rise to SPLIT and meet the front boss tips), webbed into
             the wall where it is within WEB_GAP of it, the SCREW["clear_d"] hole and a SCREW["cbore_d"] counterbore
             CBORE_DEPTH deep from the back face.
  posts      under the D-pad pivot and the ABXY centre, BACK_BOSS_GAP short of the board (hard presses; front.py
             has its plate posts over the same spots).
  snap       (SNAP) one releasable hook on the bottom wall at the cable notch, between H5 and S1 (the bottom edge's
             110 mm span with no fastener): a blade free above SNAP_ROOT_Z, a 45 deg catch / 30 deg ramp barb, into
             a pocket this module cuts in the front wall's inner face. With S1 and S2 that makes 3 shell-to-shell
             fasteners along the bottom edge.

Part "battery plate": bought (STEP only; print_pose returns None): a 1.0 mm FR4 blank from JLCPCB with no copper,
black mask both sides. Its outline is written to battery_plate.dxf by the export() hook (enclosure.export_all: out/,
or out/variants/<slug>/ for a variant): the enclosure frame's (x, -y), so KiCad's DXF
import (Y flipped) puts it at its KiCad position over the board's cell notch.

Published keep-outs (other modules keep their material out): "cell bay", "speaker", "lra", "usb_c path",
"microsd path", "battery plate" (fr4 only), "screws", "snap".

`.venv/bin/python back.py` builds this module alone on the bare shells and prints its numbers (no other modules).
"""
import functools, math, os
from build123d import Box, Compound, Cone, Cylinder, ExportDXF, Pos, Rot, Unit, extrude
import params as P
import context as C
import shell

HERE = os.path.dirname(os.path.abspath(__file__))
DXF_PATH = os.path.join(HERE, "out", "battery_plate.dxf")

# ---------------------------------------------------------------- conventions (gb2 back_openings.py)
CUT = 0.2                  # every cutter overshoots the face it opens by this much
CUT_OUT = 1.0              # ... and reaches this far past the outer surface
FUSE = 0.1                 # added solids start this far inside the face they grow from (clean unions)

# ---------------------------------------------------------------- choices (this module only)
FC, FT = P.CELL["fence_clr"], P.CELL["fence_t_max"]     # contract: fences 0.4 off the cell, 1.2 thick
PLATE_LAP = 0.4            # the plate's edge band under the fence feet = the bond band (0.6 of fence foot on the floor)
PLATE_CLR = 0.2            # pocket over the plate, per side (JLC routed-outline tolerance +/-0.2)
PLATE_BOND = 0.05          # 3M 467MP acrylic transfer tape (0.05) between the plate's edge band and the fence feet
FENCE_TOP = P.Z_BOARD_BOT  # the fences stop at the board's bottom-face level (they stand in the notch, 0.4 off it)
FENCE_ROOT = 0.8           # 45 deg root chamfer outside the fences (gb2 STRUCTURAL_REVIEW #9: fence root stress)
LEAD_CLR = 0.3             # the fences stop this far short of the cell-leads route
MIN_GAP = 0.5              # no slot narrower than this between features (MJF fuses / traps powder in narrower ones,
                           # FDM fills them): a fence nearer a wall is joined to it, the cup is bridged to S2, the
                           # fence's root chamfer is trimmed back from the bosses
LRA_RING_T, LRA_RING_H, LRA_GAP_DEG = 1.0, 1.5, 40.0    # LRA ring wall (>= FDM's 2 perimeters 0.86), height, lead gap
SPK_CUP_T = 1.0            # contract: the cup wall <= 1.0 next to S2 (its boss is 0.3 off it)
SPK_CUP_H = 3.0            # cup top over the speaker's lip face (the lower 2.6 of the D27 frame)
SPK_LEAD_IN = 0.5          # 45 deg lead-in on the cup's inner top edge (gb2 rev 2.1)
SPK_GAP_DEG = 30.0         # lead gap toward J3
SPK_BRIDGE_W = 3.0         # the bridge from the cup to a boss nearer than MIN_GAP (S2)
GRILLE_HOLE_D, GRILLE_PITCH = 1.6, 2.6                  # hex-pitch holes: 29 % of the D22 open, a stiff perforated floor
USBC_CLR = 0.28            # beside the 8.94 x 3.36 shell (gb2 openings.md 0.15-0.3); CLR_B over / under: 9.50 x 3.96
USBC_RELIEF_R = 0.5        # overmold relief corner radius; the relief grows by R(1 - 1/sqrt 2) a side so it contains
                           # the core keep-out's sharp 13 x 7 box (gb2 used R2.5 on a plain 13 x 7: corners left in it)
MSD_CLR_W, MSD_CLR_T, MSD_R = 0.45, 0.35, 0.3           # slot 11.9 x 1.7 round the 11.0 x 1.0 card (keep-out 11.5 x 1.3)
MSD_SCALLOP = (15.0, 1.5, 0.6)                          # finger scallop w (x), corner r, depth into the wall
MSD_SCALLOP_TOP = 0.6      # the scallop stops this far under the split (no sliver of wall left at the rim)
RESET_GAP = 0.5            # the guide tube's top under SW1's keystake
RESET_TUBE_T = 1.0         # guide tube wall (>= FDM's 2 perimeters 0.86)
RESET_MOUTH = 0.4          # 45 deg chamfer at the pinhole's mouth
CBORE_RECESS = 0.1         # the screw head sits this far inside the back face
CBORE_DEPTH = float(P.SCREW["head_h"]) + CBORE_RECESS
# FDM: BOSS_D 5.5 round the D4.2 counterbore leaves a 0.65 wall from the floor's inner face up to the head's bearing
# face (fine for MJF's 0.5, under FDM's 2 perimeters 0.86, the thin report), so FDM builds get a collar there:
# D cbore_d + 2 CBORE_WALL up to COLLAR_OVER past the bearing face, a 45 deg step back to BOSS_D, clipped to the outer
# body (S1 / S2 sit in the R3 back fillet)
CBORE_WALL = 0.9 if P.PROCESS == "fdm" else None
COLLAR_D = P.SCREW["cbore_d"] + 2 * CBORE_WALL if CBORE_WALL else None
COLLAR_OVER = 0.3
WEB_GAP = 1.0              # a boss closer than this to the wall's inner face gets a BOSS_D-wide web into the wall
WEB_KEEP = 1.0             # the web stops this far inside the outer surface
POST_D = 4.0               # gb2 rev 2.1 board posts
SCREW_LENGTHS = (4, 5, 6, 8, 10, 12, 16)                # DIN 912 M2 stock lengths
SCREW_TIP_CLR = 0.2        # the screw tip stays this far off the insert hole's bottom
INSERT_LEN = 3.0           # the M2 x 3 heat-set insert (params.INSERT label), set flush with the boss tip
SNAP = True
SNAP_X_K = 81.8            # KiCad x: the cable notch's left end, past the board corner (x <= 79.9 at y 117) and short
                           # of the fold bay (x 84.5) and the display flex (x 83.4, y <= 115.4)
SNAP_W = 2.8               # along the wall
SNAP_T = 0.9               # blade thickness (k ~ 10 N/mm at the barb, 2.4 % strain at full deflection, MJF PA12)
SNAP_SLOT = MIN_GAP        # blade to the walls' inner face (the blade must stay free: not under MIN_GAP)
SNAP_BARB = 0.35           # barb tip past the walls' inner face (= the blade's deflection when snapping in)
SNAP_ROOT_Z = P.SPLIT - 3.0                             # the blade is free above this (fused to the wall below)
SNAP_CATCH_Z = P.SPLIT + 1.4                            # the barb's tip line (its catch below, its ramp above)
SNAP_FLAT, SNAP_RAMP_DEG, SNAP_CATCH_DEG = 0.2, 30.0, 45.0
SNAP_RECESS_CLR = 0.1      # the front-wall pocket round the barb (depth and top; its lower edge is the catch)

COLOURS = {"battery plate": "#1c1c1c"}


# ---------------------------------------------------------------- small helpers
def _rbox(cx, cy, ang_deg, along, across, z0, z1):
    """a box centred (cx, cy), `along` long in direction ang_deg, `across` wide, z0..z1."""
    return Pos(cx, cy, (z0 + z1) / 2) * Rot(0, 0, ang_deg) * Box(along, across, z1 - z0)


def _stadium_y(cx, cz, w, h, y0, y1):
    """stadium section (w along X, h along Z, round ends left / right) swept along Y over y0..y1."""
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


def _sector(cx, cy, r, a_mid, a_w, z0, z1, n=12):
    """a pie slice about (cx, cy): radius r, centred on a_mid deg (CCW from +X), a_w deg wide, z0..z1."""
    pts = [(cx, cy)] + [(cx + r * math.cos(math.radians(a_mid - a_w / 2 + a_w * i / n)),
                         cy + r * math.sin(math.radians(a_mid - a_w / 2 + a_w * i / n))) for i in range(n + 1)]
    return P.prism(P.face(pts, board_frame=False), z0, z1)


def _angle_to(p, q):
    return math.degrees(math.atan2(q[1] - p[1], q[0] - p[0]))


def _part_at(ref):
    return P.P(P.PARTS[ref]["x"], P.PARTS[ref]["y"])


def _one(shapes):
    """fuse a list into one shape (a single solid when it is one)."""
    shapes = [s for s in shapes if s is not None]
    s = shapes[0] if len(shapes) == 1 else shapes[0].fuse(*shapes[1:]).clean()
    return s.solids()[0] if len(s.solids()) == 1 else s


def outline_near(x, y):
    """(distance from (x, y) to the console's outer surface, the outward unit normal there): the analytic R7.1
    rounded rectangle params.OUTLINE (x, y inside it)."""
    o, R = P.OUTLINE, float(P.OUTLINE_R)
    px, py = x - o.cx, y - o.cy
    qx, qy = abs(px) - (o.w / 2 - R), abs(py) - (o.h / 2 - R)
    sx, sy = math.copysign(1.0, px), math.copysign(1.0, py)
    if qx > 0 and qy > 0:                                   # a corner arc
        L = math.hypot(qx, qy)
        return R - L, (sx * qx / L, sy * qy / L)
    if qx > qy:
        return o.w / 2 - abs(px), (sx, 0.0)
    return o.h / 2 - abs(py), (0.0, sy)


def back_flat_rect():
    """the Rect over which the back face is flat at Z_BACK_MIN (the cell bay's level): the outline (flat back) or
    HUMP_RECT (hump) less its back fillet (the plan corners are far from the bay)."""
    if P.HUMP:
        r, h = shell._hump_r_bot(), P.HUMP_RECT
        return P.Rect(h.x0 + r, P.OUTLINE.y0 + r, h.x1 - r, h.y1 - r), r
    r = float(P.R_BACK)
    return P.OUTLINE.grow(-r), r


# ---------------------------------------------------------------- 1. cell bay: fences, plate pocket, window
def lead_rect():
    """the cell leads' route in plan (the core 'cell leads' keep-out's end-gap part + LEAD_CLR): from the PCM exit's
    west limit (cell centre - LEAD_EXIT_TOL) east past the riser corner, from the cell's top end north."""
    c, buf = P.CELL["rect"], P.CELL["buffer"]
    return P.Rect(c.cx - P.LEAD_EXIT_TOL - LEAD_CLR, c.y1 - LEAD_CLR, buf.x1 + 5.0, buf.y1 + 5.0)


def _bay_floor():
    c = P.CELL["rect"]
    return P.floor_in_at(c.cx, c.cy)                        # the floor's inner face in the bay (hump: the hump's)


@functools.lru_cache(maxsize=None)
def fences():
    c = P.CELL["rect"]
    zf = _bay_floor()
    inner = c.grow(FC).prism(zf - 2.0, FENCE_TOP + 1.0, FC)
    ring = c.grow(FC + FT).prism(zf - FUSE, FENCE_TOP, FC + FT) - inner
    ro = FC + FT + FENCE_ROOT + FUSE
    root = Pos(0, 0, zf - FUSE) * extrude(c.grow(ro).face(ro), amount=FENCE_ROOT + FUSE, taper=45)
    root = root - inner
    for n, b in P.BOSSES.items():                           # keep MIN_GAP to the bosses (S1 sits 0.85 off the fence)
        if c.grow(ro + P.BOSS_D / 2 + MIN_GAP + 6.0).contains(*b["at"]):
            root = root - boss_footprint(n, MIN_GAP, zf - 2.0, FENCE_TOP + 1.0)
    lr = lead_rect()
    f = fence_fillers()
    body = ring + root if f is None else ring + root + f
    return _one([body - lr.prism(zf - 2.0, FENCE_TOP + 1.0)])


def fence_fillers():
    """the fences joined to a wall closer than MIN_GAP + its own thickness (no deep narrow slot): the console's
    bottom wall (always 0.1-0.4 off the bottom fence) and, with the hump, the hump's inner walls (0.4 off, up to the
    body floor they meet)."""
    c = P.CELL["rect"]
    zf, o = _bay_floor(), c.grow(FC + FT)
    out = []
    y_in = P.OUTLINE.y0 + P.WALL
    if o.y0 - y_in < MIN_GAP:
        out.append(P.Rect(o.x0, y_in - FUSE, o.x1, o.y0 + FUSE).prism(zf - FUSE, FENCE_TOP))
    if P.HUMP:
        hi, zt = P.HUMP_INNER, P.Z_FLOOR_IN                 # the hump walls rise to the body floor's inner face
        if o.x0 - hi.x0 < MIN_GAP:
            out.append(P.Rect(hi.x0 - FUSE, o.y0, o.x0 + FUSE, o.y1).prism(zf - FUSE, zt))
        if hi.x1 - o.x1 < MIN_GAP:
            out.append(P.Rect(o.x1 - FUSE, o.y0, hi.x1 + FUSE, o.y1).prism(zf - FUSE, zt))
        if hi.y1 - o.y1 < MIN_GAP:
            out.append(P.Rect(o.x0, o.y1 - FUSE, o.x1, hi.y1 + FUSE).prism(zf - FUSE, zt))
    return _one(out) if out else None


def plate_rect():
    return P.CELL["rect"].grow(FC + PLATE_LAP)


def pocket_rect():
    return P.CELL["rect"].grow(FC + PLATE_LAP + PLATE_CLR)


@functools.lru_cache(maxsize=None)
def battery_plate():
    """the bought FR4 plate at rest: flush with the back face (fr4 only)."""
    r = FC + PLATE_LAP
    return P.prism(plate_rect().face(r), *P.CELL["floor_z"])


@functools.lru_cache(maxsize=None)
def plate_cuts():
    """(pocket, window) cutters (fr4 only): the pocket from the back face up to the plate top + PLATE_BOND, the window
    from the plate top through the floor over the cell + FC, and to the pocket's edge where the leads run."""
    c, z1, zf = P.CELL["rect"], P.CELL["floor_z"][1], _bay_floor()
    rk = FC + PLATE_LAP + PLATE_CLR
    pocket = pocket_rect().prism(P.Z_BACK_MIN - CUT, z1 + PLATE_BOND, rk)
    win = c.grow(FC).prism(z1 - CUT, zf + CUT, FC)
    win_lead = pocket_rect().prism(z1 - CUT, zf + CUT, rk) & lead_rect().prism(z1 - CUT, zf + CUT)
    return pocket, _one([win, win_lead])


def write_dxf(path=DXF_PATH):
    """the plate outline for ordering (DXF in mm, layer Edge.Cuts; the enclosure frame = KiCad after import)."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    exp = ExportDXF(unit=Unit.MM)
    exp.add_layer("Edge.Cuts")
    exp.add_shape(plate_rect().face(FC + PLATE_LAP).outer_wire(), layer="Edge.Cuts")
    exp.write(path)
    return path


def plate_edge_rise():
    """{side: rise} where the plate reaches past the back face's flat zone: how far the back surface has curved up
    above Z_BACK_MIN at the plate's edge there (= the plate edge's step out of the surface)."""
    flat, r = back_flat_rect()
    pr = plate_rect()
    out = {}
    for side, ex in (("left", flat.x0 - pr.x0), ("right", pr.x1 - flat.x1), ("bottom", flat.y0 - pr.y0),
                     ("top", pr.y1 - flat.y1)):
        if ex > 1e-9:
            e = min(ex, r)
            out[side] = r - math.sqrt(r * r - e * e)
    return out


# ---------------------------------------------------------------- 2. LRA ring, speaker cup + grille
@functools.lru_cache(maxsize=None)
def lra_ring():
    l = P.LRA
    (x, y), zf = l["at"], P.floor_in_at(*l["at"])
    ri, ro, z1 = l["ring_id"] / 2, l["ring_id"] / 2 + LRA_RING_T, P.floor_in_at(*l["at"]) + LRA_RING_H
    ring = P.cyl(x, y, 2 * ro, zf - FUSE, z1) - P.cyl(x, y, 2 * ri, zf - 1.0, z1 + 1.0)
    a = _angle_to(l["at"], _part_at("J2"))
    return _one([ring - _sector(x, y, ro + 1.0, a, LRA_GAP_DEG, zf - 1.0, z1 + 1.0)])


def speaker_cup_top():
    return P.SPEAKER["z"][0] + SPK_CUP_H


@functools.lru_cache(maxsize=None)
def speaker_cup():
    s = P.SPEAKER
    (x, y), zf = s["at"], P.floor_in_at(*s["at"])
    top, ri = speaker_cup_top(), s["cup_id"] / 2
    ro = ri + SPK_CUP_T
    cup = P.cyl(x, y, 2 * ro, zf - FUSE, top) - P.cyl(x, y, 2 * ri, zf - 1.0, top + 1.0)
    c = SPK_LEAD_IN
    cup = cup - Pos(x, y, top - c + (c + CUT) / 2) * Cone(ri, ri + c + CUT, c + CUT)
    a = _angle_to(s["at"], _part_at("J3"))
    cup = cup - _sector(x, y, ro + 1.0, a, SPK_GAP_DEG, zf - 1.0, top + 1.0)
    for b in P.BOSSES.values():                             # a boss nearer than MIN_GAP (S2: 0.3) is bridged to the cup
        bx, by = b["at"]
        d = math.hypot(bx - x, by - y)
        if d - ro - P.BOSS_D / 2 < MIN_GAP:
            ux, uy = (bx - x) / d, (by - y) / d
            r0, r1 = ro - FUSE, d - P.BOSS_D / 2 + FUSE
            bridge = _rbox(x + ux * (r0 + r1) / 2, y + uy * (r0 + r1) / 2, math.degrees(math.atan2(uy, ux)), r1 - r0,
                           SPK_BRIDGE_W, zf - FUSE, top)
            cup = cup + bridge
    return _one([cup])


@functools.lru_cache(maxsize=None)
def grille():
    """(compound of hole cutters, hole count, open fraction of the D grille_d circle)."""
    s = P.SPEAKER
    (x, y) = s["at"]
    zb, zf = P.back_at(x, y), P.floor_in_at(x, y)
    rmax = s["grille_d"] / 2 - GRILLE_HOLE_D / 2
    holes, n = [], int(rmax / GRILLE_PITCH) + 2
    for j in range(-n, n + 1):
        for i in range(-n, n + 1):
            hx, hy = (i + 0.5 * (j % 2)) * GRILLE_PITCH, j * GRILLE_PITCH * math.sqrt(3) / 2
            if math.hypot(hx, hy) <= rmax + 1e-9:
                holes.append(P.cyl(x + hx, y + hy, GRILLE_HOLE_D, zb - CUT, zf + CUT))
    return Compound(holes), len(holes), len(holes) * GRILLE_HOLE_D ** 2 / s["grille_d"] ** 2


# ---------------------------------------------------------------- 3. wall openings: USB-C, microSD; reset pinhole
def _y_top_out():
    return P.OUTLINE.y1                                     # the top wall's outer face (enclosure y)


@functools.lru_cache(maxsize=None)
def usbc_cuts():
    """(stadium + rim opening, relief): the stadium runs from outside through the wall and its floor fillet; the rim
    opening (full stadium width from the axis up past the split) lets the back shell close past the receptacle."""
    u, yo = P.USBC, _y_top_out()
    w, h, zc = usbc_stadium()
    y0, y1 = yo + CUT_OUT, yo - float(P.R_BACK) - CUT
    stadium = _stadium_y(u["x"], zc, w, h, y0, y1)
    rim = P.box(u["x"], (y0 + y1) / 2, w, y0 - y1, zc, P.SPLIT + CUT)
    rw, rh, rd = relief_size()
    relief = _rrect_y(u["x"], u["axis_z"], rw, rh, USBC_RELIEF_R, y0, yo - rd)
    return _one([stadium, rim]), relief


def usbc_stadium():
    """(w, h, zc) of the stadium round the shell's design envelope USBC["z"] (8.94 x 3.36, centred 0.05 under the plug
    axis): USBC_CLR at the sides, CLR_B over and under it (with the back at -4.86 the floor's inner fillet is right
    under the receptacle, so the B-part clearance governs there)."""
    u = P.USBC
    return u["shell_w"] + 2 * USBC_CLR, u["shell_h"] + 2 * max(USBC_CLR, float(P.CLR_B)), sum(u["z"]) / 2


def relief_size():
    """the relief cut: the contract's w x h grown so its R USBC_RELIEF_R corners still contain the sharp w x h."""
    rw, rh, rd = P.USBC["relief"]
    g = USBC_RELIEF_R * (1 - 1 / math.sqrt(2)) + 0.01
    return rw + 2 * g, rh + 2 * g, rd


def usbc_front_relief():
    """the relief's part above the split (cut from the front rim)."""
    u = P.USBC
    rw, rh, _ = relief_size()
    top = u["axis_z"] + rh / 2
    if top <= P.SPLIT:
        return None
    return usbc_cuts()[1] & P.box(u["x"], _y_top_out(), rw + 2, 2 * CUT_OUT + 4, P.SPLIT - CUT, top + CUT)


@functools.lru_cache(maxsize=None)
def microsd_cuts():
    m, yo = P.MICROSD, _y_top_out()
    zc = (m["card_z"][0] + m["card_z"][1]) / 2
    w, h = m["card_w"] + 2 * MSD_CLR_W, m["card_t"] + 2 * MSD_CLR_T
    slot = _rrect_y(m["x"], zc, w, h, MSD_R, yo + CUT_OUT, yo - P.WALL - CUT)
    sw, sr, sd = MSD_SCALLOP
    sh = 2 * (P.SPLIT - MSD_SCALLOP_TOP - zc)               # centred on the slot, its top MSD_SCALLOP_TOP under the split
    scallop = _rrect_y(m["x"], zc, sw, sh, sr, yo + CUT_OUT, yo - sd)
    return _one([slot, scallop])


@functools.lru_cache(maxsize=None)
def reset_parts():
    """(guide tube, pinhole + mouth chamfer)."""
    r = P.RESET
    (x, y) = r["at"]
    zb, zf = P.back_at(x, y), P.floor_in_at(x, y)
    top = r["keystake_z"] - RESET_GAP
    tube = P.cyl(x, y, r["hole_d"] + 2 * RESET_TUBE_T, zf - FUSE, top) - P.cyl(x, y, r["hole_d"], zf - 1.0, top + 1.0)
    m, rh = RESET_MOUTH, r["hole_d"] / 2
    hole = P.cyl(x, y, r["hole_d"], zb - CUT, top + CUT) + \
        Pos(x, y, zb - CUT + (m + CUT) / 2) * Cone(rh + m + CUT, rh, m + CUT)
    return tube, hole


# ---------------------------------------------------------------- 4. screws: bosses, webs, holes, counterbores
def screw_length(bearing_z):
    """(stock length, tip z, insert engagement) for a screw whose head bears at bearing_z: the longest stock length
    whose tip stays SCREW_TIP_CLR off the front insert hole's bottom."""
    bottom = P.SPLIT + P.INSERT["depth"]
    ok = [L for L in SCREW_LENGTHS if bearing_z + L <= bottom - SCREW_TIP_CLR + 1e-9]
    L = max(ok)
    tip = bearing_z + L
    return L, tip, min(tip, P.SPLIT + INSERT_LEN) - P.SPLIT


def _web_box(x, y, z0, z1, grow=0.0):
    """the web from a boss at (x, y) into the wall (None when the boss is WEB_GAP or more off the wall's inner face)."""
    d, (nx, ny) = outline_near(x, y)
    if d - P.WALL - P.BOSS_D / 2 >= WEB_GAP:
        return None
    return _rbox(x + nx * d / 2, y + ny * d / 2, math.degrees(math.atan2(ny, nx)), d + 2 * grow, P.BOSS_D + 2 * grow,
                 z0, z1)


def boss_footprint(name, grow, z0, z1):
    """a boss (or its FDM counterbore collar, the wider) + its web, grown by `grow` in plan, over z0..z1 (to keep
    other features off it)."""
    x, y = P.BOSSES[name]["at"]
    s = P.cyl(x, y, max(P.BOSS_D, COLLAR_D or 0.0) + 2 * grow, z0, z1)
    w = _web_box(x, y, z0, z1, grow)
    return s if w is None else s + w


@functools.lru_cache(maxsize=None)
def screws():
    """{name: (boss + web, hole + counterbore, info)}."""
    out = {}
    for n, b in P.BOSSES.items():
        x, y = b["at"]
        zf, zb, top = P.floor_in_at(x, y), P.back_at(x, y), b["back_top_z"]
        boss = P.cyl(x, y, P.BOSS_D, zf - FUSE, top)
        d, _ = outline_near(x, y)
        gap = d - P.WALL - P.BOSS_D / 2                     # boss to the wall's inner face (< 0: in the wall)
        web = _web_box(x, y, zf - FUSE, top)
        if web is not None:
            boss = boss + (web & P.prism(P.outline_face(WEB_KEEP), zf - 1.0, top + 1.0))
        bearing = zb + CBORE_DEPTH
        hole = P.cyl(x, y, P.SCREW["clear_d"], P.Z_BACK_MIN - CUT, top + CUT)
        cut = hole + P.cyl(x, y, P.SCREW["cbore_d"], P.Z_BACK_MIN - CUT, bearing)
        if COLLAR_D and COLLAR_D > P.BOSS_D:                # FDM: thicken the counterbore's wall
            z1, step = bearing + COLLAR_OVER, (COLLAR_D - P.BOSS_D) / 2
            collar = P.cyl(x, y, COLLAR_D, zf - FUSE, z1) + \
                Pos(x, y, z1 + step / 2) * Cone(COLLAR_D / 2, P.BOSS_D / 2, step)
            boss = boss + (collar & shell.body())
        boss = boss - cut                                   # bored in the add too: no add relies on a later cut
        L, tip, eng = screw_length(bearing)
        out[n] = (_one([boss]), cut, {"gap": gap, "web": gap < WEB_GAP, "top": top, "bearing": bearing, "L": L,
                                      "tip": tip, "engage": eng, "wall_d": d})
    return out


# ---------------------------------------------------------------- 5. board posts under the keys
def post_points():
    k = P.KEYS
    abxy = [k[n]["at"] for n in ("A", "B", "X", "Y")]
    return {"D-pad pivot": k["DPAD"]["at"],
            "ABXY centre": (sum(p[0] for p in abxy) / 4, sum(p[1] for p in abxy) / 4)}


@functools.lru_cache(maxsize=None)
def posts():
    top = P.Z_BOARD_BOT - P.BACK_BOSS_GAP
    return [P.cyl(x, y, POST_D, P.floor_in_at(x, y) - FUSE, top) for x, y in post_points().values()]


# ---------------------------------------------------------------- 6. snap on the bottom wall
def _snap_geo():
    x = SNAP_X_K
    y_in = P.OUTLINE.y0 + P.WALL                          # the bottom walls' inner face (enclosure; the inside is +y)
    yb0 = y_in + SNAP_SLOT                                # the blade's outer face
    yb1 = yb0 + SNAP_T                                    # its inner face
    y_tip = y_in - SNAP_BARB                              # the barb's tip
    d = yb0 - y_tip                                       # barb depth off the blade
    zc = SNAP_CATCH_Z
    z_catch0 = zc - d * math.tan(math.radians(90 - SNAP_CATCH_DEG))   # the catch face's foot on the blade
    z_ramp1 = zc + SNAP_FLAT + d / math.tan(math.radians(SNAP_RAMP_DEG))
    z_r0 = zc - SNAP_BARB * math.tan(math.radians(90 - SNAP_CATCH_DEG))   # the catch face crosses the wall plane
    z_r1 = zc + SNAP_FLAT + SNAP_BARB / math.tan(math.radians(SNAP_RAMP_DEG)) + SNAP_RECESS_CLR
    return dict(x=x, y_in=y_in, yb0=yb0, yb1=yb1, y_tip=y_tip, d=d, zc=zc, z_catch0=z_catch0, z_ramp1=z_ramp1,
                z_tip=z_ramp1 + 0.1, z_r0=z_r0, z_r1=z_r1, zf=P.floor_in_at(x, y_in + 1.0))


@functools.lru_cache(maxsize=None)
def snap():
    """(hook = root block + blade + barb [back add], front-wall pocket [front cut], sweep keep-out)."""
    g = _snap_geo()
    x, w = g["x"], SNAP_W
    root = P.box(x, (g["y_in"] - FUSE + g["yb1"]) / 2, w, g["yb1"] - g["y_in"] + FUSE, g["zf"] - FUSE, SNAP_ROOT_Z)
    blade = P.box(x, (g["yb0"] + g["yb1"]) / 2, w, SNAP_T, SNAP_ROOT_Z - FUSE, g["z_tip"])
    yb0, yt, zc = g["yb0"], g["y_tip"], g["zc"]
    barb = P.yz_plate([(yb0 + FUSE, g["z_catch0"] - FUSE), (yb0, g["z_catch0"]), (yt, zc), (yt, zc + SNAP_FLAT),
                       (yb0, g["z_ramp1"]), (yb0 + FUSE, g["z_ramp1"] + FUSE)], x - w / 2, x + w / 2)
    hook = _one([root, blade, barb])
    pw = w + 2 * SNAP_RECESS_CLR
    yp0, yp1 = yt - SNAP_RECESS_CLR, g["y_in"] + CUT
    pocket = P.box(x, (yp0 + yp1) / 2, pw, yp1 - yp0, g["z_r0"], g["z_r1"])
    ys1 = g["yb1"] + SNAP_BARB + 0.1                      # the blade's inner face, deflected, + 0.1
    sweep = P.box(x, (g["y_in"] + ys1) / 2, pw, ys1 - g["y_in"], SNAP_ROOT_Z, g["z_tip"] + 0.1)   # inside the walls
    return hook, pocket, sweep


# ---------------------------------------------------------------- keep-outs
@functools.lru_cache(maxsize=None)
def keepouts():
    ko = {}
    c = P.CELL["rect"]
    # the cell, its swell and its drop-in path: inside the fences (cell + FC), from the plate / floor up to the LCM
    ko["cell bay"] = c.grow(FC).prism(P.CELL["floor_z"][1], P.Z_PANEL_LCM[0], FC)
    s = P.SPEAKER
    ko["speaker"] = P.cyl(*s["at"], s["cup_id"], s["foam_z"][0], s["z"][1] + float(P.CLR_B))
    l = P.LRA
    ko["lra"] = P.cyl(*l["at"], l["ring_id"], l["z"][0], l["z"][1] + float(P.CLR_B))
    u = P.USBC
    rw, rh, _ = u["relief"]
    far = _y_top_out() + 10.0
    stad, relief = usbc_cuts()
    # the receptacle's way in (the stadium + its rim opening, up to the split: the front wall over it stays) + the
    # relief (its top is cut from the front rim) + the plug from face_max off the receptacle face outward
    rim = stad & P.box(u["x"], _y_top_out(), rw + 2, 20.0, P.Z_BACK_MIN - 1.0, P.SPLIT)
    plug = P.box(u["x"], (u["mouth_y"] + u["face_max"] + far) / 2, rw, far - u["mouth_y"] - u["face_max"],
                 u["axis_z"] - rh / 2, u["axis_z"] + rh / 2)
    ko["usb_c path"] = _one([rim, relief, plug])
    m = P.MICROSD
    card = P.box(m["x"], (m["face_y"] + far) / 2, m["card_w"] + 0.5, far - m["face_y"], m["card_z"][0] - 0.15,
                 m["card_z"][1] + 0.15)
    ko["microsd path"] = _one([microsd_cuts(), card])
    if P.CELL["floor"] == "fr4":
        ko["battery plate"] = plate_cuts()[0]
    heads = []
    for n, b in P.BOSSES.items():
        x, y = b["at"]
        info = screws()[n][2]
        heads.append(P.cyl(x, y, P.SCREW["cbore_d"], P.Z_BACK_MIN - CUT, info["bearing"]) +
                     P.cyl(x, y, P.SCREW["clear_d"], info["bearing"] - FUSE, P.SPLIT))
    ko["screws"] = Compound(heads)
    if SNAP:
        hook, pocket, sweep = snap()
        ko["snap"] = _one([sweep, pocket])
    for n, k in ko.items():
        k.label = f"back:{n}"
    return ko


# ---------------------------------------------------------------- notes
def notes():
    c, sp, u, m = P.CELL, P.SPEAKER, P.USBC, P.MICROSD
    out = []
    if c["floor"] == "fr4":
        pr = plate_rect()
        rise = plate_edge_rise()
        out.append(f"battery plate: {pr.w:.1f} x {pr.h:.1f} R{FC + PLATE_LAP:.1f} x {P.FLOOR_FR4:.1f} FR4 (JLCPCB, no "
                   f"copper, black mask), outline -> battery_plate.dxf in the export folder; flush in a +{PLATE_CLR} pocket at z "
                   f"{c['floor_z'][0]:+.2f}..{c['floor_z'][1]:+.2f}, bonded ({PLATE_BOND} 3M 467MP) under the fence "
                   f"feet ({PLATE_LAP} lap); no lip where the leads run. The plate's +/-10 % thickness shows as "
                   f"+/-0.1 at the back face" + (", and its edge steps " + ", ".join(f"{v:.3f} out of the R{float(P.R_BACK):g} "
                                                                                    f"fillet at the {k}" for k, v in rise.items())
                                                 if rise else ""))
    else:
        out.append(f"cell bay: {c['name']} on the printed {P.FLOOR_PR} floor (z {c['floor_z'][0]:+.2f}.."
                   f"{c['floor_z'][1]:+.2f}); no plate. The thin report puts a 1.2 printed floor at 1.3-2.3 mm under "
                   f"20 N over the bay, against {P.Z_LCM_BACK - c['swell_z'][1]:.2f} above the swollen cell: stiffen "
                   f"it (2.0 printed, or keep the FR4 plate) before trusting it")
    out.append(f"fences: {FT} thick, {FC} off the cell, z {_bay_floor():+.2f}..{FENCE_TOP:+.2f} "
               f"({FENCE_TOP - c['z'][0]:.2f} of the {c['t']:.2f} cell), {FENCE_ROOT} root chamfer outside; open from "
               f"KiCad x {lead_rect().x0:.2f} east along the cell's top end and in the riser corner (cell leads)")
    _, nh, frac = grille()
    out.append(f"speaker: cup ID {sp['cup_id']} wall {SPK_CUP_T} to z {speaker_cup_top():+.2f}, lead gap toward J3; "
               f"grille {nh} x D{GRILLE_HOLE_D} at {GRILLE_PITCH} inside D{sp['grille_d']} ({100 * frac:.0f} % open). "
               f"LRA ring ID {P.LRA['ring_id']} x {LRA_RING_H} tall, lead gap toward J2")
    rw, rh, rd = relief_size()
    sw, sh, zc = usbc_stadium()
    out.append(f"USB-C: stadium {sw:.2f} x {sh:.2f} round the shell envelope (z {zc - sh / 2:+.2f}..{zc + sh / 2:+.2f}: "
               f"{P.CLR_B} under / over it, {USBC_CLR} beside), open to the rim; relief {rw:.2f} x {rh:.2f} R{USBC_RELIEF_R} x {rd:g} (holds the "
               f"contract's sharp {u['relief'][0]:g} x {u['relief'][1]:g}; z {u['axis_z'] - rh / 2:+.2f}.."
               f"{u['axis_z'] + rh / 2:+.2f}): the receptacle face is {abs(u['mouth_y'] - (_y_top_out() - rd)):.2f} "
               f"behind the relief floor (<= {u['face_max']:.2f}); the relief's top is cut from the front rim")
    reach = abs(m["face_y"] + m["out_latched"] - _y_top_out())       # the latched card sticks out toward +y
    out.append(f"microSD: slot {m['card_w'] + 2 * MSD_CLR_W:.1f} x {m['card_t'] + 2 * MSD_CLR_T:.1f} + a "
               f"{MSD_SCALLOP[2]} scallop; the latched card edge is {reach:.2f} in from the outer face "
               f"({reach - MSD_SCALLOP[2]:.2f} from the scallop floor) and push-push needs ~1 more: a fingernail or a "
               f"card corner. Reset: D{P.RESET['hole_d']} pinhole, guide tube to z "
               f"{P.RESET['keystake_z'] - RESET_GAP:+.2f}")
    rows = screws()
    Ls = sorted({v[2]["L"] for v in rows.values()})
    out.append("screws: " + ", ".join(f"{n} {'web' if v[2]['web'] else 'free'}" for n, v in rows.items()) +
               f"; head bears at z {min(v[2]['bearing'] for v in rows.values()):+.2f}; DIN 912 M2 x "
               + "/".join(str(L) for L in Ls) + f" (tip z {max(v[2]['tip'] for v in rows.values()):+.2f}, "
               f"{min(v[2]['engage'] for v in rows.values()):.1f} in the insert). H1-H5 stop "
               f"{P.BACK_BOSS_GAP} under the board, so nothing clamps it onto the front datum but the screw through "
               f"its hole: fit a 0.3-0.5 PORON / EPDM washer (D5.5 x D2.4) on each back boss (gb2 STRUCTURAL_REVIEW #4)")
    out.append(f"posts: D{POST_D} under the D-pad pivot and the ABXY centre, {P.BACK_BOSS_GAP} under the board")
    if SNAP:
        g = _snap_geo()
        out.append(f"snap: a hook at KiCad x {SNAP_X_K} on the bottom wall (blade {SNAP_T} x {SNAP_W}, free "
                   f"{g['zc'] - SNAP_ROOT_Z:.1f}, barb {SNAP_BARB} at z {g['zc']:+.2f}, 45 deg catch: comes apart "
                   f"with a firm pull) into a pocket cut in the front wall's inner face (z {g['z_r0']:+.2f}.."
                   f"{g['z_r1']:+.2f}, {SNAP_BARB + SNAP_RECESS_CLR:.2f} deep): with S1 and S2, 3 fasteners on the bottom edge")
    if P.HUMP:
        out.append(f"hump: S1 (KiCad x {P.BOSSES['S1']['at'][0]:.1f}) is {P.HUMP_RECT.x0 - P.BOSSES['S1']['at'][0]:.1f} "
                   f"west of the hump, its counterbore notches the hump's side; the USB-C relief's foot "
                   f"(z {u['axis_z'] - rh / 2:+.2f}) is under the body's back face (z {P.Z_BACK:+.2f}): a plugged "
                   f"overmold stands below the back plane")
    if P.LCD_PLATE:
        out.append("LCD plate: nothing here carries the SUS plate (the fences stop at the board's bottom face); it "
                   "needs joggled tabs onto the board's top face (gb2 rev B) - open")
    return out


# ---------------------------------------------------------------- the module result
@functools.lru_cache(maxsize=None)
def _result():
    back_add, back_cut, front_cut = [fences(), lra_ring(), speaker_cup()], [], []
    parts = {}
    if P.CELL["floor"] == "fr4":
        back_cut += list(plate_cuts())
        parts["battery plate"] = battery_plate()
    back_cut.append(grille()[0])
    stad, relief = usbc_cuts()
    back_cut += [stad, relief]
    fr = usbc_front_relief()
    if fr is not None:
        front_cut.append(fr)
    back_cut.append(microsd_cuts())
    # the core 'cell leads' keep-out, cut from the back shell: with the hump, its cavity's R1.5 plan corner sits in
    # the riser corner (elsewhere the cut removes nothing)
    back_cut.append(C.keepouts()["cell leads"])
    tube, hole = reset_parts()
    back_add.append(tube)
    back_cut.append(hole)
    for n, (boss, cut, _) in screws().items():
        back_add.append(boss)
        back_cut.append(cut)
    back_add += posts()
    if SNAP:
        hook, pocket, _ = snap()
        back_add.append(hook)
        front_cut.append(pocket)
    return back_add, back_cut, front_cut, parts


def build(ctx):
    back_add, back_cut, front_cut, parts = _result()
    return {"front_cut": list(front_cut), "back_add": list(back_add), "back_cut": list(back_cut),
            "parts": dict(parts), "keepouts": dict(keepouts()), "notes": notes()}


def export(out_dir):
    """enclosure.export_all() hook: the battery plate's outline DXF into out_dir (FR4 builds only) -> [paths]."""
    if P.CELL["floor"] != "fr4":
        return []
    return [write_dxf(os.path.join(out_dir, "battery_plate.dxf"))]


def print_pose(name, solid):
    """the battery plate is bought (STEP only)."""
    if name == "battery plate":
        return None
    return solid


# ================================================================ self-test (this module alone on the bare shells)
if __name__ == "__main__":
    import time
    import enclosure
    t0 = time.time()
    ctx = enclosure.make_ctx()
    t1 = time.time()
    r = build(ctx)
    t2 = time.time()
    back = ctx.back.fuse(*r["back_add"]).clean().cut(*r["back_cut"]).clean()
    front = ctx.front.cut(*r["front_cut"]).clean() if r["front_cut"] else ctx.front
    t3 = time.time()
    print(f"[{P.VARIANT}] ctx {t1 - t0:.1f} s, module {t2 - t1:.1f} s, apply {t3 - t2:.1f} s")
    for n, s in (("back", back), ("front", front)):
        bb = s.bounding_box()
        print(f"  {n}: {len(s.solids())} solid(s), valid {s.is_valid}, z {bb.min.Z:+.2f}..{bb.max.Z:+.2f}, "
              f"{s.volume:.0f} mm3")
    for n, s in r["parts"].items():
        bb = s.bounding_box()
        print(f"  part {n}: valid {s.is_valid}, {bb.size.X:.2f} x {bb.size.Y:.2f} x {bb.size.Z:.2f}, z "
              f"{bb.min.Z:+.2f}..{bb.max.Z:+.2f}")
    print({k: len(v) for k, v in r.items()})
    for note in r["notes"]:
        print("  note:", note)
