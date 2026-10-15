"""controls.py - the gb3 printed controls (CONTRACT.md "controls.py"; gb2's controls.py ported to the thin stack).

build(ctx) -> {"parts", "keepouts", "notes"}: every control is one solid AT REST in the assembly frame, named after its
contract key, so checks.py finds it by name:

  A B X Y         params.KEYS round caps: head D d standing KEY_PROUD over the face in its D hole_d face hole, flange
  START SELECT    D flange_d x FLANGE_T at flange_z (seated under the plate), stem D KEY_STEM_D, flat D KEY_TIP_D tip
                  rest_gap (0.15) over the TS-1187A keystake (z 3.10). Head-first from inside (gb2 style).
  DPAD            the cross (arm 9.4, w 6.6; its 8 arm-end corners corner_r, its concave corners sharp like the hole's)
                  in key_hole_face('DPAD'); flange = key_flange_face with its 8 outer corners DPAD_FLANGE_R, riding
                  dpad_flange_gap() UNDER the plate so the cross can rock a switch to its hard stop in any direction,
                  whether it rolls on its tip or turns about the washer edge (DPAD_FLANGE_Z); four D DPAD_LEG_D legs with
                  LEG_DOME_R domed tips rest_gap (0.10) over SW2-SW5; the centre pivot (D pivot_d, near-hemispherical
                  PIVOT_TIP_R tip) rests on the board top at pivot_z, on the F.Cu GND pour (see notes).
  DPAD washer     printed WASHER_T locating washer taped on the board round the pivot (gb2 rev 2.1): y-located by the
                  SW4 / SW5 bodies, x by the SW2 / SW3 pads, all WASHER_CLR off; its D WASHER_HOLE_D hole holds the tip.
  VOL_UP VOL_DOWN params.EDGE_KEYS SKRT edge caps: a button through front.py's notch (EDGE_CLR per side), proud 0.5,
  POWER           and a lip EDGE_LIP past the notch (sides + top) on the wall's inner face = the outward stop; a contact
                  pad PAD_W square over the D2 plunger, rest_gap 0.15 off its tip.
  L R             params.SHOULDERS bumpers: the same face, but a YOKE_T yoke along the wall's inner face as long as the
                  wall allows (L x 27.6-56.4; R 183.6-201.0 between front.py's H2 / H3 merge blocks), and a stem across
                  the 4.7 from the wall to the SW15 / SW16 plunger. The yoke's far end is the fulcrum of an off-centre
                  press: a long yoke keeps the press travel near the plunger's (shoulder_press()).

Published for front.py / checks.py (import controls; every value derived from params):
  KEY_HOLES       {key: hole outline + the clear zone controls needs under the plate}
  EDGE_NOTCHES    {edge key / shoulder: the notch, the cap's button / lip / pad sizes, the yoke span}
  DPAD_FLANGE_Z   the D-pad flange's z at rest (lowered from FLANGE_Z by the rocking lift)
  DPAD_SWEEP_CLR  the D-pad's lateral excursion below the plate (its hole clearance): front keeps its material this
                  far off the flange outline (the contract's KEY_SWEEP_CLR 0.3 is less)
  keepouts        "controls:<part> sweep": each control swept over its travel (to its switch's hard stop / flush) and
                  its lateral play; "controls:DPAD sweep" covers the washer too

print_pose(): keys and the D-pad head-down (flat top on the bed; FDM needs support under the flange seat), the washer
flat, edge caps and bumpers on their flat bottom (as assembled). COLOURS: dark caps, light washer.
`.venv/bin/python controls.py` runs the self-test (fit against the finished shells, rest gaps, D-pad rocking, press
travel, keep-outs, print poses) and writes renders to build/controls/.
"""
import functools, math
from build123d import Axis, Compound, Kind, Pos, Rot, Sphere, chamfer, fillet, offset
import params as P

# ---------------------------------------------------------------- choices (this module only)
FUSE = 0.1                 # the pieces of one part overlap this much, so the union is one solid
KEY_STEM_D = 3.0           # face-key stem under the flange
KEY_TIP_D = 2.4            # flat tip: covers the D2.0 keystake and the cover hole round it
KEY_TIP_L = 0.8            # the tip's length: the stem's shoulder stays this far over the TS cover at the hard stop
KEY_CHAMFER = 0.4          # head top edge (comfort; it is the bed edge in the print pose)
DPAD_LEG_D = 2.6           # D-pad legs (gb2 rev 2.1)
LEG_DOME_R = 2.5           # domed leg tips (0.37 high on D2.6): a rocked leg still bears on its keystake
PIVOT_TIP_R = 1.49         # the pivot leg's tip: near-hemispherical on D3.0 (a D0.02 flat), so it rolls on the board
DPAD_INNER_R = 0.0         # the cross's 4 concave corners stay sharp, like the hole's (key_hole_face): a fillet r
                           # would eat 0.41 r of the 0.99 diagonal clearance there, which the lip variant needs all of
DPAD_FLANGE_R = 1.5        # the D-pad flange's 8 outer corners (inside key_flange_face; less lift on a diagonal)
DPAD_FLANGE_MARGIN = 0.05  # the flange is still this far under the plate at the worst-direction hard stop
WASHER_T = 0.6             # D-pad washer (FDM 3 layers, the (e) minimum; MJF min 0.5): its hole's edge sits low, so
                           # the rolling tip moves only ~0.04 sideways there at the diagonal stop
WASHER_HOLE_D = 2.5        # holds the pivot tip with dpad_pivot_play() 0.045 (> that 0.04: no contact while rocking)
WASHER_CLR = 0.1           # washer edges to the switch bodies (y) and the SW2 / SW3 pads (x)
TS_SOLDER = 0.05           # the solder / copper step at a pad's edge the taped washer stops against (x location)
WASHER_R = 1.0             # washer plan corners
TS_PAD_IN = 0.25           # the TS-1187A pads run this far past the terminal toes toward the D-pad centre
                           # (gb3.kicad_pcb SW2-SW5: 1.0 x 0.75 pads at +-3.0 from the switch, toes at +-3.25)
BOARD_PLAY = 0.1           # the board's lateral location: M2 screws in D2.2 holes (CONTRACT.md)
EDGE_CLR = 0.2             # edge caps / bumpers in their notches, per side, along the wall and in z (= KEY_HOLE_CLR)
EDGE_LIP = 0.8             # the retention lip past the notch (both sides and the top) on the wall's inner face
PAD_W = 2.4                # contact pad: square PAD_W over the D2 plunger (z from the cap bottom to axis + PAD_W / 2)
PAD_H = 0.5                # contact pad depth along the press axis (keeps the lip clear of the SKRT body)
EDGE_CHAMFER = 0.3         # the outer face's four edges
YOKE_T = 2.0               # shoulder yoke depth behind the wall
YOKE_MAX = 14.4            # shoulder yoke reach along the wall from the plunger axis (each way)
YOKE_CLR = 0.3             # yoke ends to front.py's boss merge blocks and the wall's corner arcs
MERGE_REACH = P.BOSS_MERGE_MAX   # front.py merges a boss into a wall when it is nearer than this (params contract;
                                 # P.BOSS_MERGE_DIR names the bosses merged into another wall than this one, e.g.
                                 # H3 into the right wall); the self-test measures the real yoke-end clearances
                                 # against the finished front shell
STEM_W = 3.0               # shoulder stem width
SWEEP_MARGIN = 0.01        # keep-outs stay this far inside the openings they sweep through (no coincident faces)

K = P.KEYS
DP = K["DPAD"]
TRAVEL = {"key": K["A"]["rest_gap"] + P.TS_STOP, "dpad": DP["rest_gap"] + P.TS_STOP}   # rest to the TS hard stop


def _solid(shape, label):
    """the single solid of a build result, labelled."""
    s = shape.solids()
    assert len(s) == 1, f"{label}: {len(s)} solids"
    s = s[0]
    s.label = label
    return s


def _fuse(pieces):
    s = pieces[0].fuse(*pieces[1:]).clean() if len(pieces) > 1 else pieces[0]
    return s.solids()[0] if len(s.solids()) == 1 else s


# ================================================================ face keys
def key(name):
    """A / B / X / Y / START / SELECT: head + flange (seated under the plate) + stem down to the flat tip."""
    k = K[name]
    x, y = k["at"]
    z0, z1 = k["flange_z"]
    tip = k["tip_z"]
    head = P.cyl(x, y, k["d"], z1 - FUSE, k["top_z"])
    head = chamfer(head.edges().group_by(Axis.Z)[-1], KEY_CHAMFER)
    flange = P.cyl(x, y, k["flange_d"], z0, z1)
    stem = P.cyl(x, y, KEY_STEM_D, tip + KEY_TIP_L, z0 + FUSE)
    tipc = P.cyl(x, y, KEY_TIP_D, tip, tip + KEY_TIP_L + FUSE)
    return _solid(_fuse([head, flange, stem, tipc]), name)


# ================================================================ D-pad
def _cross_face(grow=0.0):
    cx, cy = DP["at"]
    return P.face([(cx + x, cy + y) for x, y in P._cross_pts(DP["arm"], DP["w"], grow)], board_frame=False)


@functools.lru_cache(maxsize=None)
def dpad_head_face():
    """the cross: its 8 arm-end corners corner_r (front.py rounds the hole's the same), its 4 concave corners
    DPAD_INNER_R."""
    f = _cross_face()
    cx, cy = DP["at"]
    outer = [v for v in f.vertices() if max(abs(v.X - cx), abs(v.Y - cy)) > DP["arm"] - 1e-6]
    f = fillet(outer, DP["corner_r"]).faces()[0]
    if DPAD_INNER_R <= 0:
        return f
    inner = [v for v in f.vertices() if abs(abs(v.X - cx) - DP["w"] / 2) < 1e-6 and abs(abs(v.Y - cy) - DP["w"] / 2)
             < 1e-6]
    return fillet(inner, DPAD_INNER_R).faces()[0]


@functools.lru_cache(maxsize=None)
def dpad_flange_face():
    """params.key_flange_face('DPAD') with its 8 outer corners rounded DPAD_FLANGE_R (convex: inside the contract)."""
    f = P.key_flange_face("DPAD")
    cx, cy = DP["at"]
    reach = DP["arm"] + DP["hole_clr"] + DP["flange_lip"]
    vs = [v for v in f.vertices() if max(abs(v.X - cx), abs(v.Y - cy)) > reach - 1e-6]
    return fillet(vs, DPAD_FLANGE_R).faces()[0]


def dpad_leg_r():
    """centre to each D-pad contact (8.000)."""
    return math.dist(DP["at"], DP["contacts"][0])


def dpad_stop_tilt(phi_deg, travel=None):
    """tilt (rad) toward plan direction phi (0 = toward a leg) at which a leg, the cross rolling on its pivot's domed tip
    (about the centre), takes its keystake `travel` (default: rest gap + TS_STOP = the switch's hard stop) down."""
    travel = TRAVEL["dpad"] if travel is None else travel
    p = math.radians(phi_deg % 90)
    p = min(p, math.pi / 2 - p)
    return math.asin(travel / (dpad_leg_r() * math.cos(p)))


@functools.lru_cache(maxsize=None)
def _flange_pts():
    cx, cy = DP["at"]
    w = dpad_flange_face().outer_wire()
    return [(q.X - cx, q.Y - cy) for q in (w.position_at(i / 1440) for i in range(1440))]


def dpad_flange_support(phi_deg):
    """plan reach of the flange from the centre along direction phi (its outline's support function)."""
    c, s = math.cos(math.radians(phi_deg)), math.sin(math.radians(phi_deg))
    return max(x * c + y * s for x, y in _flange_pts())


def _fulcrum(about):
    """(r, z) of the D-pad's fulcrum on the pressed side: "centre" = the rolling tip's contact (the design basis),
    "edge" = the washer hole's top edge (the tip already against it), "rim" = checks.py (j)'s pivot rim."""
    return {"centre": (0.0, DP["pivot_z"]), "edge": (WASHER_HOLE_D / 2, P.BOARD_TOP + WASHER_T),
            "rim": (DP["pivot_d"] / 2, DP["pivot_z"])}[about]


def dpad_tilt(phi_deg, about="centre", travel=None):
    """the tilt (rad) toward phi at which the first pressed leg has taken its keystake `travel` (default the hard stop)
    down, the cross turning about the fulcrum `about`."""
    tr = TRAVEL["dpad"] if travel is None else travel
    r, _ = _fulcrum(about)
    cx, cy = DP["at"]
    u = (math.cos(math.radians(phi_deg)), math.sin(math.radians(phi_deg)))
    arms = [(c[0] - cx) * u[0] + (c[1] - cy) * u[1] for c in DP["contacts"]]
    return min(math.atan2(tr, a - r) for a in arms if a > 1.0)


def dpad_flange_lift(about="centre"):
    """the most the flange's far side rises (its reach beyond the fulcrum x the tilt) over the press directions."""
    r, _ = _fulcrum(about)
    return max((dpad_flange_support(d / 4 + 180) + r) * math.sin(dpad_tilt(d / 4, about)) for d in range(0, 181))


@functools.lru_cache(maxsize=None)
def dpad_flange_gap():
    """how far the flange rides under the plate at rest: the far side of the flange rises as the cross rocks a leg to
    the hard stop, most on a diagonal (the legs are nearer the tilt axis there). The cross rolls on its tip (lift
    "centre"), but a tip already against the washer's hole edge turns about that edge (lift "edge", larger): the gap
    is the larger + DPAD_FLANGE_MARGIN, rounded up to 0.01 (it also clears checks.py (j)'s pivot-rim model)."""
    lift = max(dpad_flange_lift("centre"), dpad_flange_lift("edge"))
    return math.ceil((lift + DPAD_FLANGE_MARGIN) * 100 - 1e-6) / 100


def _dpad_flange_z():
    z1 = round(P.Z_PLATE_IN - dpad_flange_gap(), 6)
    return (round(z1 - P.FLANGE_T, 6), z1)


DPAD_FLANGE_Z = _dpad_flange_z()                 # the D-pad flange at rest (FLANGE_Z less dpad_flange_gap())
DPAD_SWEEP_CLR = round(DP["hole_clr"] - 2 * SWEEP_MARGIN, 6)   # the D-pad's lateral excursion below the plate


def _dome_leg(cx, cy, d, z_top):
    """a D d leg from z_top down to a spherical-cap tip (LEG_DOME_R) whose lowest point is the D-pad's tip_z."""
    tip = DP["tip_z"]
    dome = Pos(cx, cy, tip + LEG_DOME_R) * Rot(90, 0, 0) * Sphere(LEG_DOME_R) + \
        P.cyl(cx, cy, d + 1, tip + LEG_DOME_R, z_top + 1)                  # (pole off the tip: a clean apex)
    return P.cyl(cx, cy, d, tip, z_top) & dome


def dpad():
    """the cross: head (KEY_PROUD over the face), flange under the plate, four domed legs over SW2-SW5, the centre
    pivot resting its rounded tip on the board top."""
    x, y = DP["at"]
    z0, z1 = DPAD_FLANGE_Z
    head = P.prism(dpad_head_face(), z1 - FUSE, DP["top_z"])
    head = chamfer(head.edges().group_by(Axis.Z)[-1], KEY_CHAMFER)
    flange = P.prism(dpad_flange_face(), z0, z1)
    pivot = P.cyl(x, y, DP["pivot_d"], DP["pivot_z"], z0 + FUSE)
    pivot = fillet(pivot.edges().group_by(Axis.Z)[0], PIVOT_TIP_R)
    legs = [_dome_leg(cx, cy, DPAD_LEG_D, z0 + FUSE) for cx, cy in DP["contacts"]]
    return _solid(_fuse([head, flange, pivot] + legs), "DPAD")


def _dpad_switches():
    """{ref: (axis 0 = x / 1 = y, sign)} of the four D-pad switches round the centre."""
    out = {}
    cx, cy = DP["at"]
    for ref, s in P.SWITCHES.items():
        if "top_z" in s and any(math.dist(s["at"], c) < 1e-3 for c in DP["contacts"]):
            dx, dy = s["at"][0] - cx, s["at"][1] - cy
            out[ref] = (0, math.copysign(1, dx)) if abs(dx) > abs(dy) else (1, math.copysign(1, dy))
    return out


@functools.lru_cache(maxsize=None)
def dpad_washer_rect():
    """the washer's plan (a Rect round the D-pad centre): WASHER_CLR off the SW2 / SW3 pads in x (their edges toward
    the centre are TS_PAD_IN past the hulls' terminal toes) and off the SW4 / SW5 hulls (their bodies) in y."""
    cx, cy = DP["at"]
    hx, hy = [], []
    for ref, (ax, sg) in _dpad_switches().items():
        r = P.part_rect(ref)
        if ax == 0:
            hx.append((r.x0 - cx if sg > 0 else cx - r.x1) - TS_PAD_IN - WASHER_CLR)
        else:
            hy.append((r.y0 - cy if sg > 0 else cy - r.y1) - WASHER_CLR)
    return P.Rect.centred(cx, cy, 2 * min(hx), 2 * min(hy))


def dpad_washer():
    """the pivot washer on the board top (taped with thin transfer tape, <= 0.05: not modelled)."""
    x, y = DP["at"]
    z0 = P.BOARD_TOP
    w = dpad_washer_rect().prism(z0, z0 + WASHER_T, WASHER_R)
    return _solid(w - P.cyl(x, y, WASHER_HOLE_D, z0 - 0.1, z0 + WASHER_T + 0.1), "DPAD washer")


def dpad_pivot_play():
    """how far the pivot slides (any way: the tip is round) before the torus tip (tube PIVOT_TIP_R on a ring
    pivot_d / 2 - PIVOT_TIP_R) meets the washer hole's top edge (r WASHER_HOLE_D / 2, WASHER_T over the board)."""
    r_ring = DP["pivot_d"] / 2 - PIVOT_TIP_R
    dz = PIVOT_TIP_R - WASHER_T
    return WASHER_HOLE_D / 2 - r_ring - math.sqrt(PIVOT_TIP_R ** 2 - dz * dz)


def dpad_offcentre():
    """(x, y): the cross's worst plan offset from its hole's centre: the washer on the board (x: to the SW2 / SW3 pad
    edges + TS_SOLDER; y: to the SW4 / SW5 bodies) + the pivot in the washer + the board in the shell."""
    p = dpad_pivot_play()
    return (WASHER_CLR + TS_SOLDER + p + BOARD_PLAY, WASHER_CLR + p + BOARD_PLAY)


def dpad_sway(phi_deg=0.0, z=None, d=None):
    """the head's sideways move where it crosses height z (default: the face, the top of its hole) at the hard-stop
    tilt, for head material at plan distance d along the press from the centre (default: the arm side, w / 2): on the
    pressed side the material that reaches z comes from higher up, so it has swayed more."""
    z = P.Z_FACE if z is None else z
    d = DP["w"] / 2 if d is None else d
    t = dpad_stop_tilt(phi_deg)
    h = (z - DP["pivot_z"] + d * math.sin(t)) / math.cos(t)          # the original height above the pivot
    return d * math.cos(t) + h * math.sin(t) - d


def dpad_rock(part, phi_deg, shift=(0.0, 0.0), tilt_deg=None, about="centre", travel=None):
    """the D-pad `part` rocked toward plan direction phi (deg, enclosure frame: 0 = +x = SW3 RIGHT, 90 = +y = SW4 UP)
    to tilt_deg (default: the tilt at which the first pressed leg has taken its keystake `travel`, default rest gap +
    TS_STOP = the hard stop), the whole cross first shifted by the plan vector `shift`. The fulcrum:
      "centre"  the rolling model, the design basis: the near-hemispherical tip rolls on the board (its contact stays
                within PIVOT_TIP_R x tilt ~0.1 of the centre)
      "edge"    the tip already bearing on the washer hole's top edge on the pressed side (r WASHER_HOLE_D / 2, z the
                washer top): the cross turns about that edge
      "rim"     checks.py (j)'s model: about the pivot's rim pivot_d / 2 out on the pressed side at the board top (a
                flat-bottomed pivot)
    -> (moved part, tilt_deg)."""
    cx, cy = DP["at"]
    u = (math.cos(math.radians(phi_deg)), math.sin(math.radians(phi_deg)))
    part = Pos(shift[0], shift[1], 0) * part
    r, z = _fulcrum(about)
    if tilt_deg is None:
        tilt_deg = math.degrees(dpad_tilt(phi_deg, about, travel))
    org = (cx + shift[0] + u[0] * r, cy + shift[1] + u[1] * r, z)
    return part.rotate(Axis(org, (-u[1], u[0], 0.0)), tilt_deg), tilt_deg       # z x u: the pressed side goes down


# ================================================================ edge caps + shoulder bumpers (wall-local frame)
def _frame(e):
    """(origin, angle_deg): origin on the outer surface on the plunger axis, local x along the wall, local y = n, the
    depth in from the outer surface (along -dir)."""
    ux, uy = e["dir"]
    return e["outer"], math.degrees(math.atan2(ux, -uy))


def _place(local, e):
    (ox, oy), ang = _frame(e)
    return Pos(ox, oy, 0) * Rot(0, 0, ang) * local


def _lbox(u0, u1, n0, n1, z0, z1):
    """a box in the wall-local frame (u along the wall, n inward)."""
    return P.box((u0 + u1) / 2, (n0 + n1) / 2, u1 - u0, n1 - n0, z0, z1)


def _to_u(e, x, y):
    """global (x, y) -> local u (along the wall)."""
    (ox, oy), ang = _frame(e)
    return (x - ox) * math.cos(math.radians(ang)) + (y - oy) * math.sin(math.radians(ang))


def _cap_dims(e):
    """the cap's local numbers: button half-width, z0, z1; lip half-width, lip top; contact face depth n_c."""
    return {"w": e["notch_w"] / 2 - EDGE_CLR, "z0": e["notch_z"][0] + EDGE_CLR, "z1": e["notch_z"][1] - EDGE_CLR,
            "wl": e["notch_w"] / 2 + EDGE_LIP, "zl": e["notch_z"][1] + EDGE_LIP, "n_c": e["inset"] - e["rest_gap"],
            "zp": e["axis_z"] + PAD_W / 2}


@functools.lru_cache(maxsize=None)
def yoke_span(name):
    """(u0, u1): a shoulder yoke's extent along the wall (local u) - YOKE_MAX each way from the plunger axis, YOKE_CLR
    short of the wall's corner arcs and of every boss column in its way: a boss front.py merges into this wall (a
    BOSS_D wide block from the boss to the wall, for a boss within MERGE_REACH of it, unless P.BOSS_MERGE_DIR sends
    its block to another wall), or a boss whose own column reaches into the yoke's swept depth (YOKE_T + proud)."""
    e = P.SHOULDERS[name]
    (ux, uy) = e["dir"]
    (ox, oy) = e["outer"]
    horiz = abs(uy) > abs(ux)                                 # the wall runs along x (top / bottom wall)
    a = ox if horiz else oy                                   # the axis' position along the wall
    lo, hi = a - YOKE_MAX, a + YOKE_MAX
    if horiz:
        lo, hi = max(lo, P.OUTLINE.x0 + P.OUTLINE_R + YOKE_CLR), min(hi, P.OUTLINE.x1 - P.OUTLINE_R - YOKE_CLR)
    else:
        lo, hi = max(lo, P.OUTLINE.y0 + P.OUTLINE_R + YOKE_CLR), min(hi, P.OUTLINE.y1 - P.OUTLINE_R - YOKE_CLR)
    wall_in = (oy - uy * P.WALL) if horiz else (ox - ux * P.WALL)   # the wall's inner face (y or x)
    depth = YOKE_T + e["proud"] + YOKE_CLR                    # the yoke pushed in to flush, + its clearance
    for bn, b in P.BOSSES.items():
        bx, by = b["at"]
        gap = abs((by if horiz else bx) - wall_in) - P.BOSS_D / 2
        md = P.BOSS_MERGE_DIR.get(bn)
        into_this = md is None or math.dist(md, (ux, uy)) < 1e-9   # its block goes into this wall
        if not ((into_this and gap <= MERGE_REACH) or gap < depth):
            continue
        c = bx if horiz else by
        b0, b1 = c - P.BOSS_D / 2 - YOKE_CLR, c + P.BOSS_D / 2 + YOKE_CLR
        if b1 <= a:
            lo = max(lo, b1)
        elif b0 >= a:
            hi = min(hi, b0)
    # global span along the wall -> local u
    if horiz:
        us = sorted((_to_u(e, lo, oy), _to_u(e, hi, oy)))
    else:
        us = sorted((_to_u(e, ox, lo), _to_u(e, ox, hi)))
    return tuple(round(u, 6) for u in us)


def _cap_pieces(e, yoke=None):
    """{piece: local box} of an edge cap (yoke None) or a shoulder bumper (yoke = (u0, u1))."""
    d = _cap_dims(e)
    w, z0, z1, n_c = d["w"], d["z0"], d["z1"], d["n_c"]
    out = {"button": _lbox(-w, w, -e["proud"], P.WALL + FUSE, z0, z1)}
    if yoke is None:
        out["lip"] = _lbox(-d["wl"], d["wl"], P.WALL, n_c - PAD_H, z0, d["zl"])
    else:
        u0, u1 = yoke
        out["lip"] = _lbox(u0, u1, P.WALL, P.WALL + YOKE_T, z0, d["zl"])
        out["stem"] = _lbox(-STEM_W / 2, STEM_W / 2, P.WALL + YOKE_T - FUSE, n_c - PAD_H + FUSE, z0, d["zp"] + 0.2)
    out["pad"] = _lbox(-PAD_W / 2, PAD_W / 2, n_c - PAD_H - FUSE, n_c, z0, d["zp"])
    return out


def _cap(e, name, yoke=None):
    pc = _cap_pieces(e, yoke)
    b = pc.pop("button")
    b = chamfer(b.edges().group_by(Axis.Y)[0], EDGE_CHAMFER)       # the outer face's four edges
    return _solid(_place(_fuse([b] + list(pc.values())), e), name)


def edge_cap(name):
    """VOL_UP / VOL_DOWN / POWER."""
    return _cap(P.EDGE_KEYS[name], name)


def shoulder(name):
    """L / R."""
    return _cap(P.SHOULDERS[name], name, yoke_span(name))


def press_travel(e, u_p, fulcrum, travel=None):
    """face travel needed to actuate when pressed at u_p (along the wall, from the plunger axis): the cap rests out on
    the wall, so an off-centre press rocks it about the far end of its lip / yoke (at `fulcrum` = |u| of that end) and
    the contact moves fulcrum / (|u_p| + fulcrum) of the press. travel: rest gap + the SKRT travel (0.2; 0.3 worst)."""
    travel = e["rest_gap"] + P.SWITCHES[e["switch"]]["travel"] if travel is None else travel
    return travel * (abs(u_p) + fulcrum) / fulcrum


def shoulder_press(name, worst=False):
    """[(u_p, face travel needed)] at five points across the face of a shoulder bumper or an edge cap (u_p from the
    plunger axis; worst: the SKRT's 0.3 worst-case travel instead of 0.2)."""
    e = P.SHOULDERS[name] if name in P.SHOULDERS else P.EDGE_KEYS[name]
    u0, u1 = yoke_span(name) if name in P.SHOULDERS else (-_cap_dims(e)["wl"], _cap_dims(e)["wl"])
    w = _cap_dims(e)["w"]
    tr = e["rest_gap"] + P.SWITCHES[e["switch"]]["travel"] + (0.1 if worst else 0.0)
    rows = []
    for u in (-w, -w / 2, 0.0, w / 2, w):
        far = u1 if u < 0 else -u0                            # the press at -u rocks about the +u end
        rows.append((round(u, 2), round(press_travel(e, u, far, tr), 3)))
    return rows


# ================================================================ keep-outs (each control swept over its travel)
def _key_sweep(name):
    k = K[name]
    x, y = k["at"]
    dz = TRAVEL["key"]
    lat = P.KEY_HOLE_CLR                                      # the head's lateral play in its hole
    tilt = (k["flange_z"][0] - k["tip_z"]) * k["rest_gap"] / (k["flange_d"] / 2)   # the stem's sway at its tip
    head = P.cyl(x, y, k["hole_d"] - 2 * SWEEP_MARGIN, P.Z_PLATE_IN, k["top_z"] + SWEEP_MARGIN)
    flange = P.cyl(x, y, k["flange_d"] + 2 * lat, k["flange_z"][0] - dz, P.Z_PLATE_IN)
    stem = P.cyl(x, y, KEY_STEM_D + 2 * (lat + tilt), k["tip_z"] - dz, k["flange_z"][0] - dz + FUSE)
    return _fuse([head, flange, stem])


def _dpad_sweep():
    head = P.prism(offset(dpad_head_face(), DPAD_SWEEP_CLR, kind=Kind.ARC).faces()[0], P.Z_PLATE_IN,
                   DP["top_z"] + SWEEP_MARGIN)
    below = P.prism(offset(dpad_flange_face(), DPAD_SWEEP_CLR, kind=Kind.ARC).faces()[0], DP["pivot_z"], P.Z_PLATE_IN)
    return _fuse([head, below])


def _cap_sweep(e, yoke=None):
    """the cap pieces pushed in to flush (proud) and grown by their lateral play (EDGE_CLR) in u and z."""
    d = _cap_dims(e)
    pr, g, m = e["proud"], EDGE_CLR - SWEEP_MARGIN, SWEEP_MARGIN
    n0z, n1z = e["notch_z"]
    out = [_lbox(-e["notch_w"] / 2 + m, e["notch_w"] / 2 - m, -pr, P.WALL + pr, n0z + m, n1z - m)]   # in the notch
    zl0 = d["z0"] - g
    if yoke is None:
        out.append(_lbox(-d["wl"] - g, d["wl"] + g, P.WALL, d["n_c"] - PAD_H + pr, zl0, d["zl"] + g))
    else:
        u0, u1 = yoke
        out.append(_lbox(u0 - g, u1 + g, P.WALL, P.WALL + YOKE_T + pr, zl0, d["zl"] + g))
        out.append(_lbox(-STEM_W / 2 - g, STEM_W / 2 + g, P.WALL + YOKE_T, d["n_c"] - PAD_H + pr, zl0, d["zp"] + 0.2 + g))
    out.append(_lbox(-PAD_W / 2 - g, PAD_W / 2 + g, d["n_c"] - PAD_H, d["n_c"] + pr, zl0, d["zp"] + g))
    return _place(_fuse(out), e)


def keepouts():
    ko = {f"{n} sweep": _key_sweep(n) for n in K if K[n]["shape"] == "round"}
    ko["DPAD sweep"] = _dpad_sweep()
    ko.update({f"{n} sweep": _cap_sweep(e) for n, e in P.EDGE_KEYS.items()})
    ko.update({f"{n} sweep": _cap_sweep(e, yoke_span(n)) for n, e in P.SHOULDERS.items()})
    for n, s in ko.items():
        s.label = f"controls:{n}"
    return ko


# ================================================================ published constants (front.py / checks.py)
def _key_holes():
    out = {}
    for n, k in K.items():
        if k["shape"] == "round":
            out[n] = {"shape": "round", "at": k["at"], "hole_d": k["hole_d"], "head_d": k["d"],
                      "flange_d": k["flange_d"], "flange_z": k["flange_z"], "sweep_d": k["flange_d"] + 2 * P.KEY_HOLE_CLR,
                      "sweep_z": (round(k["flange_z"][0] - TRAVEL["key"], 6), P.Z_PLATE_IN), "clear_d": k["clear_d"]}
        else:
            out[n] = {"shape": "cross", "at": k["at"], "hole": "params.key_hole_face('DPAD') (front.py rounds its 8 "
                      "arm-end corners R0.5)", "hole_clr": k["hole_clr"], "head_corner_r": k["corner_r"],
                      "head_inner_r": DPAD_INNER_R,
                      "flange": "key_flange_face('DPAD'), 8 outer corners R%g" % DPAD_FLANGE_R,
                      "flange_z": DPAD_FLANGE_Z, "sweep_clr": DPAD_SWEEP_CLR, "sweep_z": (DP["pivot_z"], P.Z_PLATE_IN),
                      "washer": dpad_washer_rect().kicad(), "washer_z": (P.BOARD_TOP, P.BOARD_TOP + WASHER_T)}
    return out


def _edge_notches():
    out = {}
    for group in (P.EDGE_KEYS, P.SHOULDERS):
        for n, e in group.items():
            d = _cap_dims(e)
            out[n] = {"switch": e["switch"], "outer": e["outer"], "dir": e["dir"], "notch_w": e["notch_w"],
                      "notch_z": e["notch_z"], "button_w": round(2 * d["w"], 6), "button_z": (d["z0"], d["z1"]),
                      "lip_w": round(2 * d["wl"], 6), "lip_z": (d["z0"], d["zl"]), "contact_n": round(d["n_c"], 6),
                      "clr": EDGE_CLR, "yoke_u": yoke_span(n) if n in P.SHOULDERS else None,
                      "yoke_kicad_x": tuple(sorted(round(e["outer"][0] - u, 3) for u in yoke_span(n)))
                      if n in P.SHOULDERS else None}
    return out


KEY_HOLES = _key_holes()
EDGE_NOTCHES = _edge_notches()


# ================================================================ hooks + build
COLOURS = {"DPAD": "#2a2c30", "A": "#3a3d43", "B": "#3a3d43", "X": "#3a3d43", "Y": "#3a3d43", "START": "#55595f",
           "SELECT": "#55595f", "VOL_UP": "#4a4e55", "VOL_DOWN": "#4a4e55", "POWER": "#7a2e2e", "L": "#4a4e55",
           "R": "#4a4e55", "DPAD washer": "#e6e2d6"}


def print_pose(name, solid):
    """keys and the D-pad head-down (their flat tops on the bed); the washer, edge caps and bumpers as assembled (on
    their flat bottoms). enclosure.print_pose() then drops the result onto the bed and centres it."""
    if name in K:
        return Rot(180, 0, 0) * solid
    return solid


def parts():
    out = {n: key(n) for n in K if K[n]["shape"] == "round"}
    out["DPAD"] = dpad()
    out["DPAD washer"] = dpad_washer()
    out.update({n: edge_cap(n) for n in P.EDGE_KEYS})
    out.update({n: shoulder(n) for n in P.SHOULDERS})
    return out


def notes():
    k, a = K["A"], K["START"]
    ox, oy = dpad_offcentre()
    sw0 = dpad_sway(0.0, d=DP["arm"])
    n = [f"[{P.VARIANT}] caps {k['tip_z']:.2f}..{k['top_z']:.2f} ({k['top_z'] - k['tip_z']:.2f} long, {P.KEY_PROUD} "
         f"proud): head D{k['d']:g} / D{a['d']:g} in D{k['hole_d']:g} / D{a['hole_d']:g}, flange D{k['flange_d']:g} / "
         f"D{a['flange_d']:g} x {P.FLANGE_T} at z {k['flange_z'][0]:.2f}..{k['flange_z'][1]:.2f}, stem D{KEY_STEM_D}, "
         f"tip D{KEY_TIP_D} x {KEY_TIP_L} at z {k['tip_z']:.2f} ({k['rest_gap']} over the keystake; "
         f"{TRAVEL['key']:.2f} to the TS stop)"]
    n.append(f"DPAD: legs D{DPAD_LEG_D} R{LEG_DOME_R}-domed at z {DP['tip_z']:.2f}, pivot D{DP['pivot_d']} R"
             f"{PIVOT_TIP_R} on the board at KiCad ({DP['at'][0]:.2f}, {-DP['at'][1]:.2f}): no F part within 4.75 "
             f"(board.json), F.Cu GND pour with its fill edge 1.03 away (BTN_LEFT track at 1.31, tented GND via at "
             f"1.10; gb3.kicad_pcb read-only). The flange rides {dpad_flange_gap():.2f} under the plate (z "
             f"{DPAD_FLANGE_Z[0]:.2f}..{DPAD_FLANGE_Z[1]:.2f}, not FLANGE_Z): rolling on its tip the cross reaches "
             f"a hard stop at {math.degrees(dpad_stop_tilt(0)):.2f} deg axial / {math.degrees(dpad_stop_tilt(45)):.2f}"
             f" diagonal (flange lift {dpad_flange_lift('centre'):.2f}); turning about the washer edge "
             f"{math.degrees(dpad_tilt(45, 'edge')):.2f} deg (lift {dpad_flange_lift('edge'):.2f}). So the cross "
             f"can lift {dpad_flange_gap():.2f} (rattle: a 1.2 PORON ring on the flange would preload it)")
    wr = dpad_washer_rect()
    n.append(f"DPAD washer {wr.w:.2f} x {wr.h:.2f} x {WASHER_T} (hole D{WASHER_HOLE_D}): pivot play "
             f"{dpad_pivot_play():.3f}; the cross sits up to {ox:.2f} (x) / {oy:.2f} (y) off its hole's centre and sways "
             f"{sw0:.2f} at the face at the axial stop -> {ox + sw0:.2f} / {oy + sw0:.2f} of the {DP['hole_clr']} hole "
             f"clearance (x includes TS_SOLDER at the SW2 / SW3 pads). Bond it with <= 0.05 transfer tape (3M 467MP, "
             f"as back.py's plate): 0.05 cuts the play to ~0.01, thicker tape would lift the tip off the board")
    n.append(f"rest gaps hang on printed lengths: the front boss plate-to-tip {P.Z_PLATE_IN - P.SPLIT:.2f} and the cap "
             f"flange-seat-to-tip {k['flange_z'][1] - k['tip_z']:.2f} (MJF +/-0.1-0.15 each) against the "
             f"{k['rest_gap']} gap and the TS 0.2 +/-0.1 travel: check on the first print (sand the flange seat to "
             f"shorten a pre-pressed key)")
    e, s = P.EDGE_KEYS["VOL_UP"], P.SHOULDERS["L"]
    de, ds = _cap_dims(e), _cap_dims(s)
    n.append(f"edge caps: button {2 * de['w']:.1f} x {de['z1'] - de['z0']:.1f} in the {e['notch_w']:g} x "
             f"{e['notch_z'][1] - e['notch_z'][0]:.1f} notch ({EDGE_CLR}/side), {e['proud']} proud, lip "
             f"{2 * de['wl']:.1f} wide to z {de['zl']:.2f} on the wall's inner face (the outward stop), pad "
             f"{PAD_W} x {PAD_H} at {e['rest_gap']} off the plunger tip; no over-travel stop (the SKRT bottoms at "
             f"{e['rest_gap'] + 0.2:.2f}, {e['proud'] - e['rest_gap'] - 0.2:.2f} still proud)")
    sh = []
    for nm in ("L", "R"):
        e_ = P.SHOULDERS[nm]
        u0, u1 = yoke_span(nm)
        xa, xb = sorted((e_["outer"][0] - u0, e_["outer"][0] - u1))     # top wall: local u = -(x - x_axis)
        rows = dict(shoulder_press(nm))
        w = _cap_dims(e_)["w"]
        ends = sorted(((e_["outer"][0] + w, rows[-round(w, 2)]), (e_["outer"][0] - w, rows[round(w, 2)])))
        sh.append(f"{nm} yoke KiCad x {xa:.1f}..{xb:.1f}: face ends x {ends[0][0]:.1f} / {ends[1][0]:.1f} need "
                  f"{ends[0][1]:.2f} / {ends[1][1]:.2f}")
    n.append(f"shoulders: face {2 * ds['w']:.1f} x {ds['z1'] - ds['z0']:.1f} {s['proud']} proud, yoke {YOKE_T} deep "
             f"on the wall's inner face, stem {STEM_W} wide over the {s['inset'] - P.WALL:.2f} to the plunger; an "
             f"off-centre press rocks about the yoke's far end, so the face travel to actuate (rest gap + SKRT 0.2 = "
             f"0.35 at the centre) grows toward the ends: " + "; ".join(sh) + f" (R's -x yoke end stops at front's "
             f"H2 merge block, its +x end at the corner arc: H3 merges into the right wall, params.BOSS_MERGE_DIR; "
             f"> {s['proud']} = no actuation there; see open issues)")
    n.append(f"for front: published controls.KEY_HOLES / EDGE_NOTCHES / DPAD_FLANGE_Z / DPAD_SWEEP_CLR; the D-pad "
             f"moves up to {DPAD_SWEEP_CLR:.2f} sideways below the plate (front keeps its material KEY_SWEEP_CLR "
             f"{P.KEY_SWEEP_CLR} off its flange: today the nearest is the diagonal posts at 1.7); keep-outs "
             f"'controls:<part> sweep'; no adds or cuts of ours in the shells")
    return n


def build(ctx):
    return {"parts": parts(), "keepouts": keepouts(), "notes": notes()}


# ================================================================ self-test (python controls.py [--no-render])
def _t_vol(a, b):
    """overlap volume (0 when the bounding boxes miss or either is empty)."""
    if not a.solids() or not b.solids():
        return 0.0
    p, q = a.bounding_box(), b.bounding_box()
    if not all(lo <= hi2 and lo2 <= hi for lo, hi, lo2, hi2 in zip(tuple(p.min), tuple(p.max), tuple(q.min),
                                                                     tuple(q.max))):
        return 0.0
    r = a & b
    return sum(s.volume for s in r.solids()) if r is not None else 0.0


def _t_near(a, b, pad=1.0):
    p, q = a.bounding_box(), b.bounding_box()
    return all(lo - pad <= hi2 and lo2 - pad <= hi for lo, hi, lo2, hi2 in zip(tuple(p.min), tuple(p.max),
                                                                               tuple(q.min), tuple(q.max)))


def _t_crop(shape, around, pad=2.0):
    """shape cut to the bounding box of `around` grown by pad (speeds up distance / boolean queries); an empty
    Compound when nothing is there."""
    b = around.bounding_box()
    c = shape & P.box((b.min.X + b.max.X) / 2, (b.min.Y + b.max.Y) / 2, b.size.X + 2 * pad, b.size.Y + 2 * pad,
                      b.min.Z - pad, b.max.Z + pad)
    return c if c is not None and c.solids() else Compound([])


def _t_lowest(part, x, y, d, z0, z1):
    c = part & P.cyl(x, y, d, z0, z1)
    return c.bounding_box().min.Z if c is not None and c.solids() else None


def _t_ts_pads(ref):
    """the four TS-1187A pads of a switch as 1.0 x 0.75 boxes running TS_PAD_IN past the model's terminal toes (away
    from the switch centre; the pitch is P3.70), z 1.6..1.7 (pad + solder foot)."""
    s = P.SWITCHES[ref]
    r = P.part_rect(ref)
    x, y = s["at"]
    out = []
    for sx in (-1, 1):
        xt = r.x1 if sx > 0 else r.x0                          # the toe
        xc = xt + sx * (TS_PAD_IN - 0.5)
        for sy in (-1, 1):
            out.append(P.box(xc, y + sy * 1.85, 1.0, 0.75, P.BOARD_TOP, P.BOARD_TOP + 0.1))
    return out


def selftest(render=True):
    import os, time
    import enclosure
    t0 = time.time()
    allparts, ctx = enclosure.build()
    mine = {n: s for n, s in allparts.items() if ctx.part_owner.get(n) == "controls"}
    front, back = allparts["front"], allparts["back"]
    cb = {n: s for n, (s, _) in ctx.context.items()}
    print(f"controls self-test [{P.VARIANT}]: enclosure.build() {time.time() - t0:.1f} s, {len(mine)} controls")
    bad = []

    # 1. every control at rest vs the finished shells, the context and each other
    print("\n1. at rest: overlaps (mm3) with the finished shells / context / other parts; nearest neighbours (mm)")
    for n, s in mine.items():
        hits = []
        for on, o in [("front", front), ("back", back)] + list(cb.items()) + \
                [(m, q) for m, q in allparts.items() if m not in ("front", "back", n)]:
            if not _t_near(s, o, 0.0):
                continue
            v = _t_vol(s, o)
            if v > 1e-3:
                hits.append(f"{on} {v:.4f}")
        near = sorted((round(s.distance_to(o), 3), on) for on, o in list(cb.items()) +
                      [(m, q) for m, q in allparts.items() if m not in ("front", "back", n)] if _t_near(s, o, 1.0))[:4]
        df = _t_crop(front, s, 1.5)
        dfront = s.distance_to(df) if df.solids() else float("inf")
        print(f"  {n:12s} {len(s.solids())} solid, valid {s.is_valid}, {s.volume:7.1f} mm3 | to front {dfront:.3f} | "
              + (("OVERLAPS " + ", ".join(hits) + "  <-- BAD") if hits else "no overlaps") + " | nearest "
              + ", ".join(f"{on} {d}" for d, on in near))
        if hits:
            bad.append(f"{n} overlaps " + ", ".join(hits))

    # 2. rest gaps (as checks.py (c) reads them)
    print("\n2. rest gaps")
    for n, k in K.items():
        for cx, cy in k["contacts"]:
            ref = next(r for r, sw in P.SWITCHES.items() if "top_z" in sw and math.dist(sw["at"], (cx, cy)) < 1e-3)
            z = _t_lowest(mine[n], cx, cy, 2.0, k["tip_z"] - 1, k["tip_z"] + 3)
            g = z - P.SWITCHES[ref]["top_z"]
            ok = abs(g - k["rest_gap"]) < 0.005
            print(f"  {n:6s} over {ref}: tip z {z:.3f}, gap {g:.3f} (contract {k['rest_gap']}), vs max keystake "
                  f"{z - P.SWITCHES[ref]['top_z_max']:.3f}" + ("" if ok else "  <-- OFF"))
            bad += [] if ok else [f"{n} gap {g:.3f}"]
    z = _t_lowest(mine["DPAD"], *DP["at"], 0.5, P.BOARD_TOP - 1, P.KEYSTAKE_Z)
    print(f"  DPAD pivot lowest z {z:.4f} (board top {P.BOARD_TOP}); to the board {mine['DPAD'].distance_to(cb['board']):.4f}")
    for group in (P.EDGE_KEYS, P.SHOULDERS):
        for n, e in group.items():
            (tx, ty), (ux, uy) = e["tip"], e["dir"]
            probe = P.box(tx + ux * 2, ty + uy * 2, 4.0 if abs(ux) > 0.5 else 2.0, 4.0 if abs(uy) > 0.5 else 2.0,
                          e["axis_z"] - 1, e["axis_z"] + 1)
            c = (mine[n] & probe).bounding_box()
            g = (c.min.X - tx) if ux > 0.5 else (tx - c.max.X) if ux < -0.5 else (c.min.Y - ty) if uy > 0.5 else \
                (ty - c.max.Y)
            bbn = mine[n].bounding_box()
            proud = (bbn.max.X - e["outer"][0]) if ux > 0.5 else (bbn.max.Y - e["outer"][1])
            ok = abs(g - e["rest_gap"]) < 0.005 and abs(proud - e["proud"]) < 0.005
            print(f"  {n:8s} contact to the {e['switch']} tip {g:.3f} (contract {e['rest_gap']}), proud {proud:.3f}"
                  + ("" if ok else "  <-- OFF"))
            bad += [] if ok else [f"{n} gap {g:.3f} proud {proud:.3f}"]

    # 3. the D-pad: washer fit, off-centre budget, rocking (rolling model and checks.py's rim model)
    print("\n3. D-pad")
    wa, dp = mine["DPAD washer"], mine["DPAD"]
    hull = {r: cb[f"F:{r}"] for r in _dpad_switches()}
    pads = [b for r in _dpad_switches() for b in _t_ts_pads(r)]
    dh = min(wa.distance_to(h) for h in hull.values())
    dpd = min(wa.distance_to(b) for b in pads)
    print(f"  washer {dpad_washer_rect().w:.2f} x {dpad_washer_rect().h:.2f}: to the switch hulls {dh:.3f}, to the "
          f"TS pads (modelled) {dpd:.3f}, to the D-pad {wa.distance_to(dp):.3f}, on the board "
          f"{wa.distance_to(cb['board']):.4f}; pivot play {dpad_pivot_play():.3f}; off-centre x / y "
          f"{dpad_offcentre()[0]:.3f} / {dpad_offcentre()[1]:.3f}")
    bad += [] if dh >= WASHER_CLR - 0.005 and dpd >= WASHER_CLR - 0.005 else ["washer clearance"]
    fb = _t_crop(front, P.box(*DP["at"], 30, 30, P.BOARD_TOP, P.Z_FACE + 1), 0.0)
    near = {"washer": wa}
    near.update({n: s for n, s in cb.items() if n.startswith("F:") and _t_near(s, dp, 1.0)})
    sw_of = {0: "SW3", 90: "SW4", 180: "SW2", 270: "SW5"}
    ox, oy = dpad_offcentre()
    print("  plate clearance (head + flange to the finished front shell) at the stop tilt, the cross shifted to the "
          "off-centre budget's corner away from / toward the press (the board, switches and washer move with it: only "
          "the front shell is fixed); 'others' = the washer + the switch envelopes, unshifted (the pressed ones left "
          "out; the washer may touch: the rolling tip ends on its hole edge, the locating contact)")
    print("  toward       |  tilt deg  | sway at face | plate clr: -shift / 0 / +shift | others (centre) | edge fulcrum "
          "tilt, plate clr | checks (j) rim model tilt, plate clr")
    for phi in (0.0, 22.5, 45.0, 90.0, 135.0, 180.0, 225.0, 270.0, 315.0):
        u = (math.cos(math.radians(phi)), math.sin(math.radians(phi)))
        corner = (math.copysign(ox, u[0]) if abs(u[0]) > 1e-9 else 0.0,
                  math.copysign(oy, u[1]) if abs(u[1]) > 1e-9 else 0.0)        # the budget box's corner toward phi
        pressed = {r for ang, r in sw_of.items() if math.cos(math.radians(phi - ang)) > 0.1}
        clr = []
        for sh in ((-corner[0], -corner[1]), (0.0, 0.0), corner):
            mv, tilt = dpad_rock(dp, phi, sh)
            top = mv & P.box(*DP["at"], 34, 34, DPAD_FLANGE_Z[0] - 1.2, P.Z_FACE + 2)
            v = _t_vol(mv, fb)
            clr.append(-v if v > 1e-3 else (top.distance_to(fb) if fb.solids() else float("inf")))
            if sh == (0.0, 0.0):
                hits = [f"{on} {vv:.3f}" for on, o in near.items() if on[2:] not in pressed
                        and (vv := _t_vol(mv, o) - _t_vol(dp, o)) > 1e-3]
                t0_ = tilt
        sway = (P.Z_FACE - DP["pivot_z"]) * math.sin(math.radians(t0_))
        info = []
        for about in ("edge", "rim"):
            tr = None if about == "edge" else DP["rest_gap"] + P.TS_STOP - 0.02      # checks.py's PRESS_MARGIN
            if about == "rim" and phi % 45:
                info.append("-")
                continue
            mv, tilt = dpad_rock(dp, phi, about=about, travel=tr)
            v = _t_vol(mv, fb)
            top = mv & P.box(*DP["at"], 34, 34, DPAD_FLANGE_Z[0] - 1.2, P.Z_FACE + 2)
            info.append(f"{tilt:.2f}, " + (f"OVERLAP {v:.3f}" if v > 1e-3 else f"{top.distance_to(fb):.3f}"))
        ok = all(c > 1e-3 for c in clr) and all(h.startswith("washer") and float(h.split()[-1]) <= 0.01 for h in hits)
        print(f"  {phi:5.1f} {'+'.join(sorted(pressed)):7s}| {t0_:5.2f}      | {sway:.3f}        | "
              + " / ".join(("OVERLAP %.3f" % -c) if c < 0 else "%.3f" % c for c in clr)
              + f" | {', '.join(hits) or 'none'} | {info[0]} | {info[1]}" + ("" if ok else "  <-- BINDS"))
        if not ok:
            bad.append(f"D-pad rock {phi}: plate clr {clr}, {hits}")
    sd = dpad_sway(45, d=DP["w"] / 2 * math.sqrt(2))                # at the concave corner
    print(f"  hole budget per axis: off-centre x {ox:.3f} / y {oy:.3f} + axial sway {dpad_sway(0, d=DP['arm']):.3f} = "
          f"{ox + dpad_sway(0, d=DP['arm']):.3f} / {oy + dpad_sway(0, d=DP['arm']):.3f} of hole_clr {DP['hole_clr']}; "
          f"diagonal: {(ox + oy) / math.sqrt(2) + sd:.3f} of the "
          f"{DP['hole_clr'] * math.sqrt(2) - 0.414 * DPAD_INNER_R:.3f} to the hole's sharp concave corners")

    # 4. press travel: keys to the TS stop, edge caps / bumpers through the SKRT travel (0.2) and to flush
    print("\n4. press travel")
    for n, k in K.items():
        if k["shape"] != "round":
            continue
        mv = Pos(0, 0, -(TRAVEL["key"] - 0.02)) * mine[n]
        fc = _t_crop(front, mine[n], 1.0)
        v = _t_vol(mv, fc)
        print(f"  {n:6s} down {TRAVEL['key'] - 0.02:.2f}: front {v:.4f}" + ("" if v <= 1e-3 else "  <-- BINDS"))
        bad += [] if v <= 1e-3 else [f"{n} press binds"]
    for group in (P.EDGE_KEYS, P.SHOULDERS):
        for n, e in group.items():
            ux, uy = e["dir"]
            fc = _t_crop(front, mine[n], 1.0)
            env = cb[f"F:{e['switch']}"]
            for d, what in ((e["rest_gap"] + P.SWITCHES[e["switch"]]["travel"] - 0.02, "SKRT travel"),
                            (e["proud"], "flush")):
                mv = Pos(-ux * d, -uy * d, 0) * mine[n]
                v = _t_vol(mv, fc)
                vb = _t_vol(mv, back)
                print(f"  {n:8s} in {d:.2f} ({what}): front {v:.4f}, back {vb:.4f}, its SKRT hull {_t_vol(mv, env):.3f}"
                      + ("" if v <= 1e-3 and vb <= 1e-3 else "  <-- BINDS"))
                bad += [] if v <= 1e-3 and vb <= 1e-3 else [f"{n} press {what} binds"]
    print("  off-centre presses (face travel to actuate; nominal SKRT 0.2 / worst 0.3; u from the plunger axis):")
    for n in list(P.EDGE_KEYS)[:1] + list(P.SHOULDERS):
        rows, worst = shoulder_press(n), shoulder_press(n, worst=True)
        e = P.SHOULDERS.get(n) or P.EDGE_KEYS[n]
        print(f"    {n:7s} " + "  ".join(f"u {u:+.1f}: {t:.2f}/{w:.2f}" for (u, t), (_, w) in zip(rows, worst))
              + f"  (proud {e['proud']})")

    # 5. keep-outs: ours vs the finished shells (we add nothing to them) and the other modules' material; our parts vs
    # the other modules' keep-outs and the core ones
    print("\n5. keep-outs")
    ko = {k_: s for k_, s in ctx.keepouts.items() if k_.startswith("controls:")}
    for kn, s in ko.items():
        hits = [f"{sh} {v:.4f}" for sh, o in (("front", front), ("back", back)) if (v := _t_vol(s, o)) > 1e-3]
        for mod, r in ctx.modules.items():
            if mod == "controls":
                continue
            for pn, q in r["parts"].items():
                if (v := _t_vol(s, q)) > 1e-3:
                    hits.append(f"{mod} part {pn} {v:.4f}")
        print(f"  {kn:24s} {s.volume:7.1f} mm3: " + (", ".join(hits) + "  <-- BAD" if hits else "clear"))
        bad += [] if not hits else [f"{kn}: " + ", ".join(hits)]
    for n, s in mine.items():
        hits = [f"{kn} {v:.4f}" for kn, k_ in ctx.keepouts.items() if not kn.startswith("controls:")
                and (v := _t_vol(s, k_)) > 1e-3]
        if hits:
            print(f"  part {n} in " + ", ".join(hits) + "  <-- BAD")
            bad.append(f"{n} in " + ", ".join(hits))
    print(f"  our {len(mine)} parts vs {sum(not k_.startswith('controls:') for k_ in ctx.keepouts)} other keep-outs: "
          + ("see above" if any("in " in b for b in bad) else "clear"))

    # 6. print poses
    print("\n6. print poses")
    for n, s in mine.items():
        pp = enclosure.print_pose(n, s, ctx)
        b = pp.bounding_box()
        ok = abs(b.min.Z) < 1e-6 and abs(pp.volume - s.volume) < 1e-3
        bed = sum(f.area for f in pp.faces() if abs(f.center().Z) < 1e-4 and abs(abs(f.normal_at().Z) - 1) < 1e-6)
        print(f"  {n:12s} z 0..{b.max.Z:.2f}, bed faces {bed:6.1f} mm2" + ("" if ok else "  <-- BAD"))
        bad += [] if ok else [f"{n} print pose"]
    print(f"\nself-test: {len(bad)} problem(s) in {time.time() - t0:.0f} s" + ("" if not bad else ": " + "; ".join(bad)))
    if render:
        for pth in _t_render(allparts, ctx):
            print(f"rendered {os.path.relpath(pth, os.path.dirname(os.path.abspath(__file__)))}")
    return bad


def _t_render(allparts, ctx):
    """close-up PNGs of the controls into build/controls/ (headless, render.py)."""
    import os
    import render as R
    import enclosure
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "build", "controls")
    os.makedirs(out_dir, exist_ok=True)
    cb = ctx.context
    done = []

    def items(region, shells_alpha=0.3, skip=()):
        """parts + context bodies whose bounding box meets region (x0, y0, x1, y1 enclosure), the shells cropped."""
        x0, y0, x1, y1 = region
        box = P.box((x0 + x1) / 2, (y0 + y1) / 2, x1 - x0, y1 - y0, P.Z_BACK_MIN - 1, P.Z_FACE + 3)
        out = []
        src = [(n, s, enclosure.part_colour(n, ctx)) for n, s in allparts.items()] + \
            [(n, s, col) for n, (s, col) in cb.items()]
        for n, s, col in src:
            if n in skip:
                continue
            b = s.bounding_box()
            if b.max.X < x0 or b.min.X > x1 or b.max.Y < y0 or b.min.Y > y1:
                continue
            if b.min.X < x0 or b.max.X > x1 or b.min.Y < y0 or b.max.Y > y1:     # sticks out: crop it
                s = s & box
                if s is None or not s.solids():
                    continue
            out.append((n, s, col, shells_alpha if n in ("front", "back") else 1.0))
        return out

    lg = [("front shell", enclosure.SHELL_COLOURS["front"]), ("back shell", enclosure.SHELL_COLOURS["back"]),
          ("controls", COLOURS["DPAD"]), ("D-pad washer", COLOURS["DPAD washer"]), ("power cap", COLOURS["POWER"]),
          ("board", "#1f6f3f"), ("F parts", "#8fa8c8")]
    left = (P.OUTLINE.x0 - 1, P.OUTLINE.y1 - 60, 60.0, P.OUTLINE.y1 + 1)        # D-pad, START, L
    right = (178.0, P.OUTLINE.y1 - 60, P.OUTLINE.x1 + 1, P.OUTLINE.y1 + 1)      # ABXY, SELECT, R, VOL / POWER
    done.append(R.view(items(left), os.path.join(out_dir, "left_top.png"), az=-90, el=90, width=1500, height=1400,
                       title=f"left grip from the front: D-pad + washer, START, L (front shell alpha 0.3) - {P.VARIANT}",
                       legend=lg))
    done.append(R.view(items(right), os.path.join(out_dir, "right_top.png"), az=-90, el=90, width=1500, height=1400,
                       title=f"right grip from the front: A B X Y, SELECT, R, VOL+ VOL- POWER - {P.VARIANT}",
                       legend=lg))
    dp_reg = (DP["at"][0] - 14, DP["at"][1] - 14, DP["at"][0] + 14, DP["at"][1] + 14)
    done.append(R.view(items(dp_reg, skip=("front", "back")), os.path.join(out_dir, "dpad_iso.png"), az=-60, el=28,
                       width=1500, height=1100, title="D-pad, its washer and SW2-SW5 on the board (shells hidden)",
                       legend=lg))
    reg_r = (176.0, P.OUTLINE.y1 - 40, P.OUTLINE.x1 + 1, P.OUTLINE.y1 + 1)
    done.append(R.view(items(reg_r, shells_alpha=1.0), os.path.join(out_dir, "right_corner_outside.png"), az=40,
                       el=22, width=1500, height=1100, title="top-right corner from outside: the R bumper and VOL+ / "
                       "VOL- / POWER standing 0.5 proud in their notches (shells opaque)", legend=lg))
    done.append(R.view(items(reg_r, skip=("front",)), os.path.join(out_dir, "right_corner_inside.png"), az=-130, el=40,
                       width=1500, height=1100, title="the same corner from inside, front shell hidden: R's yoke on "
                       "the wall between the H2 / H3 boss columns, stem over SW16; the edge caps' lips and pads",
                       legend=lg))
    reg_l = (P.OUTLINE.x0 - 1, P.OUTLINE.y1 - 30, 62.0, P.OUTLINE.y1 + 1)
    done.append(R.view(items(reg_l, shells_alpha=1.0), os.path.join(out_dir, "left_corner_outside.png"), az=120, el=22,
                       width=1500, height=1000, title="top-left corner from outside: the L bumper 0.5 proud in its "
                       "12-wide notch (shells opaque)", legend=lg))
    done.append(R.view(items(reg_l, skip=("front",)), os.path.join(out_dir, "left_shoulder_inside.png"), az=-60, el=40,
                       width=1500, height=1000, title="L bumper from inside, front shell hidden: the face, the yoke "
                       "along the top wall's inner face, the stem over the board edge to SW15", legend=lg))
    sec = [(n, s, c, a) for n, s, c, a in items((P.OUTLINE.x0 - 1, P.OUTLINE.y0 - 1, P.OUTLINE.x1 + 1,
                                                  P.OUTLINE.y1 + 1))]
    zl = [(P.Z_FACE, "face"), (P.Z_PLATE_IN, "plate inner"), (P.KEYSTAKE_Z, "keystake"), (P.SPLIT, "split"),
          (DPAD_FLANGE_Z[1], "D-pad flange top")]
    yk = -DP["at"][1]
    done.append(R.section(sec, os.path.join(out_dir, "section_y_dpad.png"), "y", DP["at"][1], zlines=zl,
                          zooms=[(DP["at"][0] - 13, DP["at"][0] + 13), (K["Y"]["at"][0] - 7, P.OUTLINE.x1 + 1)],
                          title=f"section at KiCad y {yk:.1f}: D-pad (SW2 / SW3 legs + pivot + washer), Y and A caps"))
    done.append(R.section(sec, os.path.join(out_dir, "section_x_dpad.png"), "x", DP["at"][0], zlines=zl,
                          zooms=[(-P.OUTLINE.y1 - 1, -K["START"]["at"][1] + 6), (yk - 13, yk + 13)],
                          title=f"section at KiCad x {DP['at'][0]:.1f}: START, the D-pad over SW4 / SW5"))
    for n in ("L", "R"):
        e = P.SHOULDERS[n]
        done.append(R.section(sec, os.path.join(out_dir, f"section_x_{n}.png"), "x", e["tip"][0], zlines=zl,
                              zooms=[(-P.OUTLINE.y1 - 1.5, -e["tip"][1] + 5)],
                              title=f"section at KiCad x {e['tip'][0]:.2f}: the {n} bumper over its "
                                    f"{e['switch']} plunger (contact {e['rest_gap']} off the tip)"))
    for n in ("VOL_UP", "POWER"):
        e = P.EDGE_KEYS[n]
        done.append(R.section(sec, os.path.join(out_dir, f"section_y_{n}.png"), "y", e["tip"][1], zlines=zl,
                              zooms=[(e["tip"][0] - 5, P.OUTLINE.x1 + 1.5)],
                              title=f"section at KiCad y {-e['tip'][1]:.2f}: the {n} cap in its notch over "
                                    f"{e['switch']}"))
    return done


if __name__ == "__main__":
    import sys
    sys.exit(1 if selftest(render="--no-render" not in sys.argv) else 0)
