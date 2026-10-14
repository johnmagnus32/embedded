"""controls.py — the printed controls, each one solid placed in the assembly frame at rest (DESIGN.md sections 3, 5):
key(name) for A / B / START / SELECT, dpad() + dpad_washer() (rev 2.1: centres the pivot), edge_cap(name) for
VOL_UP / VOL_DOWN / L / R and power_slider(); print_pose(part) lays any of them on the bed; dpad_rock() rocks the D-pad
on its pivot against the finished front shell (checks.py (j) and the self-test use it). Shapes follow the shared contracts in params.py (KEYS, EDGE_KEYS,
POWER_SLOT); the face holes, guide tubes, wall notches + lip recesses they ride in belong to front_keys / back_openings.
`.venv/bin/python controls.py` runs the fit self-test against the bare shells and the context geometry."""
import functools, math
from build123d import (Axis, Cylinder, Face, Kind, Line, Pos, Rot, Sphere, Vector, Wire, chamfer, extrude, fillet,
                       offset, revolve)
from params import (KEYS, EDGE_KEYS, POWER_SLOT, KEYSTAKE_Z, KEY_REST_GAP, DPAD_REST_GAP, KEY_PROUD, KEY_HOLE_CLR,
                    FLANGE_T, FLANGE_LIP, FIN_Z, EDGE_TIP_INSET, EDGE_REST_GAP, EDGE_PROUD, EDGE_NOTCH,
                    EDGE_LIP_RECESS, FINGER, PARTS, SILHOUETTE, SPLIT, T, WALL, Z_FRONT, Z_PLATE_IN, CLR_SLIDE, box, cyl,
                    face, prism)
from front_keys import DPAD_HOLE_CLR, DPAD_CORNER_R, dpad_hole_face   # the D-pad's face hole (rev 2.1: 1.3; params
                                                                       # says 1.0) + the cross's corners

# ---------------------------------------------------------------- choices (this module only)
FDM_MIN = 0.8                    # thinnest section anywhere (task rule, 0.4 nozzle)
ENV_CLR = 0.1                    # kept from part envelopes everywhere except the intended contacts
TS_STOP = 0.3                    # TS-1187A: keystake stroke to its stainless cover, the hard stop (C318884)
TS_KEYSTAKE_D = 2.0              # TS-1187A keystake (self-test model)
KEY_TIP_L = 0.8                  # length of the flat Ø tip_d tip under a wider stem (A/B): the stem's shoulder stays
                                 # 0.8 over the cover (z 2.80) even at the hard stop, where the tip lands on the cover
FIN_TAPER = 45                   # deg: the guide ring's top slopes in to the stem (it prints head-down: no flat overhang)
FIN_ROOT = 0.3                   # the guide ring starts this far inside the stem surface
KEY_CHAMFER = 0.4                # top edge of every head (comfort; it is the bed edge in print_pose)
PIVOT_TIP_R = 1.4                # pivot leg tip: rounded edge on the Ø3.0 leg (near-hemispherical, Ø0.2 flat at z T)
LEG_DOME_R = 2.5                 # rev 2.1: the four legs' tips are spherical caps (0.37 high on the Ø2.6 leg), so a rocked
                                 # leg still bears on its keystake, never on the switch's cover, up to the 0.3 stop
TS_COVER_TOP = KEYSTAKE_Z - TS_STOP     # 2.80: TS-1187A stainless cover, flush with the keystake at its stop
TS_COVER_HOLE = 2.2              # cover hole round the keystake (not on the drawing: the review's 2.2, the tightest)
TS_BODY = 5.1                    # TS-1187A body, square
# D-pad pivot washer (rev 2.1): a printed disc on the board's GND pour round the pivot (taped, thin double-sided tape),
# its rim WASHER_CLR off the four switch bodies so they centre it - no jig; its hole holds the R1.4 pivot tip, whose
# surface at r 1.45 is 0.08 above the washer top, so the pivot still rests on the board (rest gap unchanged).
WASHER_T = 0.8                   # FDM_MIN
WASHER_HOLE_D = 2.9
WASHER_CLR = 0.1                 # rim to the switch bodies (courtyards = the 5.1 bodies)
DPAD_FLANGE_MARGIN = 0.05        # flange still this far under the plate at the worst-direction hard stop
EDGE_CHAMFER = 0.3               # edge-cap face edges
# MSK12C02-HB (SW1, C431541) lever / body, in the slot frame (n = depth in from the outer surface)
SW1_TIP_N = WALL + 0.97          # lever tip: 0.97 short of the wall's inner face (contract)
SW1_LEVER_W, SW1_LEVER_LEN = 1.3, 1.45   # lever width along the slot, length out of the body face
SW1_LEVER_Z = (1.8, 2.8)
SW1_BODY = (8.0, 2.8, 1.4)       # body along the slot (incl. lugs), deep, tall - self-test model only
POWER_KNOB_PROUD = 1.0           # knob out of the outer surface (choice)
POWER_FORK_PLAY = 0.25           # per side round the lever, so the slot ends stop the slider, not the switch (review)
POWER_LIP = 1.0                  # plate overlap past the slot (at full travel) and above it
POWER_PLATE_IN = 3.4             # plate inner face away from the lever (FDM_MIN behind it, at the fork bottom)
POWER_TINE_W = 0.8
POWER_BODY_CLR = 0.3             # fork tines stop this short of the switch body
POWER_WALL_CLR = 0.03            # plate's outer face off the wall where the wall bends in (west end of the slot)
POWER_REST_SHIFT = POWER_SLOT["travel"] / 2   # rest pose: lever at the east end (+t; OFF per the research's reading
                                              # of the drawing - verify with a meter)


def _solid(shape, label):
    """the single solid of a build result, labelled (print_pose() dispatches on the label)."""
    s = shape.solids()
    assert len(s) == 1, f"{label}: {len(s)} solids"
    s = s[0]
    s.label = label
    return s


def _stem(x, y, d, z_top, tip_d=None, gap=KEY_REST_GAP):
    """a stem of diameter d from z_top down to a flat tip 'gap' above the keystake; a narrower tip_d (if any) over the
    last KEY_TIP_L."""
    tip = KEYSTAKE_Z + gap
    if tip_d is None or tip_d >= d:
        return cyl(x, y, d, tip, z_top)
    return cyl(x, y, d, tip + KEY_TIP_L, z_top) + cyl(x, y, tip_d, tip, tip + KEY_TIP_L)


def _guide_ring(x, y, stem_d, od):
    """rev 2.1 (was FIN_N fins): one full ring on the stem, Ø od over FIN_Z, its top sloping in at FIN_TAPER to the stem
    (self-supporting head-down). B's tube has a 39-deg slot on the speaker side that a 1.0 fin could drop into (then
    the stem could walk 1.55 toward the speaker); a full ring can shift only ~0.2 into it."""
    z0, z1 = FIN_Z
    r_in = stem_d / 2 - FIN_ROOT                               # starts inside the stem (clean union)
    rise = (od / 2 - r_in) / math.tan(math.radians(FIN_TAPER))
    prof = Face(Wire.make_polygon([Vector(r_in, 0, z0), Vector(od / 2, 0, z0), Vector(od / 2, 0, z1),
                                   Vector(r_in, 0, z1 + rise)], close=True))
    return Pos(x, y, 0) * revolve(prof, Axis.Z)


# ---------------------------------------------------------------- face keys
def key(name):
    """A / B / START / SELECT: head (KEY_PROUD over the face) + flange seated under the plate + stem down to a flat
    Ø tip_d tip KEY_REST_GAP above the keystake, with a guide ring (Ø fin_od over FIN_Z, rides the guide bore)."""
    k = KEYS[name]
    x, y = k["at"]
    head = cyl(x, y, k["d"], Z_PLATE_IN, Z_FRONT + KEY_PROUD)
    head = chamfer(head.edges().group_by(Axis.Z)[-1], KEY_CHAMFER)
    flange = cyl(x, y, k["flange_d"], Z_PLATE_IN - FLANGE_T, Z_PLATE_IN)
    stem = _stem(x, y, k["stem_d"], Z_PLATE_IN - FLANGE_T + 0.01, k["tip_d"])
    return _solid(head + flange + stem + _guide_ring(x, y, k["stem_d"], k["fin_od"]), name)


# ---------------------------------------------------------------- D-pad
def _cross():
    """the D-pad head outline at the origin: arms reach 'arm' from the centre, 'w' wide; corners rounded."""
    k = KEYS["DPAD"]
    a, h = k["arm"], k["w"] / 2
    f = face([(h, h), (a, h), (a, -h), (h, -h), (h, -a), (-h, -a), (-h, -h), (-a, -h), (-a, h), (-h, h), (-h, a),
              (h, a)], board_frame=False)
    f = fillet([v for v in f.vertices() if max(abs(v.X), abs(v.Y)) > a - 1e-6], DPAD_CORNER_R[0]).faces()[0]
    return fillet([v for v in f.vertices() if abs(abs(v.X) - h) < 1e-6 and abs(abs(v.Y) - h) < 1e-6],
                  DPAD_CORNER_R[1]).faces()[0]


def dpad_leg_r():
    """centre to each D-pad contact (8.000)."""
    k = KEYS["DPAD"]
    return math.dist(k["at"], k["contacts"][0])


def _flange_face():
    """the flange outline (rev 2.1): the face hole + FLANGE_LIP all round."""
    return dpad_hole_face(FLANGE_LIP)


def dpad_flange_reach():
    """centre to the flange's arm tips: arm + hole clearance + lip."""
    k = KEYS["DPAD"]
    return k["arm"] + DPAD_HOLE_CLR + FLANGE_LIP


def dpad_stop_tilt(phi_deg):
    """tilt (rad) toward plan direction phi (0 = toward a leg) at which a domed leg, rocking on the pivot, takes its
    keystake to the switch's own stop: DPAD_REST_GAP + TS_STOP down at the leg's axis (the dome's contact point moves
    out by LEG_DOME_R x tilt, ~0.18: ignored, so this is the larger tilt)."""
    p = math.radians(phi_deg % 90)
    p = min(p, math.pi / 2 - p)
    return math.asin((DPAD_REST_GAP + TS_STOP) / (dpad_leg_r() * math.cos(p)))


@functools.lru_cache(maxsize=None)
def _flange_pts():
    k = KEYS["DPAD"]
    w = _flange_face().outer_wire()
    return [(p.X - k["at"][0], p.Y - k["at"][1]) for p in (w.position_at(i / 720) for i in range(720))]


def dpad_flange_support(phi_deg):
    """plan reach of the flange along direction phi (support function of its outline, sampled)."""
    c, s_ = math.cos(math.radians(phi_deg)), math.sin(math.radians(phi_deg))
    return max(x * c + y * s_ for x, y in _flange_pts())


def dpad_flange_gap():
    """the D-pad rocks on its pivot, so its flange's far side rises (reach x tilt); at rest it rides this far under the
    plate - room for the tilt that takes a leg to the switch's hard stop, in the worst plan direction (the diagonal:
    the legs are nearer the tilt axis there, so the stop tilt is larger)."""
    lift = max(dpad_flange_support(d) * math.sin(dpad_stop_tilt(d)) for d in range(0, 46))
    return round(lift + DPAD_FLANGE_MARGIN, 2)


def _dome_leg(cx, cy, d, z_top):
    """a Ø d leg from z_top down to a spherical-cap tip (LEG_DOME_R) whose lowest point is DPAD_REST_GAP above the
    keystake."""
    tip = KEYSTAKE_Z + DPAD_REST_GAP
    dome = Pos(cx, cy, tip + LEG_DOME_R) * Rot(90, 0, 0) * Sphere(LEG_DOME_R) + \
        cyl(cx, cy, d + 1, tip + LEG_DOME_R, z_top + 1)                  # (pole off the tip: the erosion test needs it)
    return cyl(cx, cy, d, tip, z_top) & dome


def dpad():
    """the cross: head (KEY_PROUD over the face), flange under the plate, four domed Ø leg_d legs DPAD_REST_GAP above
    SW2-5, centre pivot leg resting its rounded tip on the board top at z T."""
    k = KEYS["DPAD"]
    x, y = k["at"]
    z_fl = Z_PLATE_IN - dpad_flange_gap()                     # flange top at rest
    cross = Pos(x, y, 0) * _cross()
    head = prism(cross, z_fl, Z_FRONT + KEY_PROUD)
    head = chamfer(head.edges().group_by(Axis.Z)[-1], KEY_CHAMFER)
    flange = prism(_flange_face(), z_fl - FLANGE_T, z_fl)
    pivot = cyl(x, y, k["pivot_d"], T, z_fl - FLANGE_T + 0.01)
    pivot = fillet(pivot.edges().group_by(Axis.Z)[0], PIVOT_TIP_R)                 # rounded tip, rocks on the board
    part = head + flange + pivot
    for cx, cy in k["contacts"]:
        part += _dome_leg(cx, cy, k["leg_d"], z_fl - FLANGE_T + 0.01)
    return _solid(part, "DPAD")


def _poly_dist(x, y, pts):
    """distance from (x, y) to the closed polygon pts."""
    best = math.inf
    for (ax, ay), (bx, by) in zip(pts, pts[1:] + pts[:1]):
        dx, dy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy)))
        best = min(best, math.hypot(x - ax - t * dx, y - ay - t * dy))
    return best


def dpad_washer_od():
    """the washer's diameter: WASHER_CLR short of the nearest of the four switch bodies (5.46 from the centre)."""
    k = KEYS["DPAD"]
    refs = [r for r, p in PARTS.items() if r in ("SW2", "SW3", "SW4", "SW5")]
    near = min(_poly_dist(*k["at"], [(px, -py) for px, py in PARTS[r]["courtyard"]]) for r in refs)
    return round(2 * (near - WASHER_CLR), 2)


def dpad_washer():
    """the pivot washer (rev 2.1) on the board top round the pivot: Ø dpad_washer_od() x WASHER_T, Ø WASHER_HOLE_D hole."""
    x, y = KEYS["DPAD"]["at"]
    return _solid(cyl(x, y, dpad_washer_od(), T, T + WASHER_T) - cyl(x, y, WASHER_HOLE_D, T - 0.1, T + WASHER_T + 0.1),
                  "DPAD_WASHER")


def dpad_pivot_play():
    """how far the pivot slides (any direction: the tip is round) before it meets the washer's hole edge: the torus tip
    (tube PIVOT_TIP_R on a ring r pivot_d/2 - PIVOT_TIP_R) against the edge circle at r WASHER_HOLE_D/2, z WASHER_T."""
    r_ring = KEYS["DPAD"]["pivot_d"] / 2 - PIVOT_TIP_R
    dz = PIVOT_TIP_R - WASHER_T
    return WASHER_HOLE_D / 2 - r_ring - math.sqrt(PIVOT_TIP_R ** 2 - dz * dz)


def dpad_shift_max():
    """the cross's worst plan offset in its face hole: pivot in the washer + washer between the switches + the board
    against the front shell's fingers (FINGER gap)."""
    return dpad_pivot_play() + WASHER_CLR + FINGER["gap"]


def _bisect(f, lo, hi, n=14):
    """the value where f flips from False to True (f(lo) False, f(hi) True)."""
    for _ in range(n):
        m = (lo + hi) / 2
        lo, hi = (lo, m) if f(m) else (m, hi)
    return (lo + hi) / 2


def dpad_rock(front, phi, shift=0.0, part=None, max_deg=8.0):
    """rock the D-pad on its pivot toward plan direction phi (deg, enclosure frame: 0 = +X toward SW5, 45 = the SW5 +
    SW2 diagonal) about the pivot's board contact, the whole cross first shifted 'shift' along phi in its hole. Each
    switch: keystake Ø TS_KEYSTAKE_D, top KEYSTAKE_Z; cover top TS_COVER_TOP over the TS_BODY square outside a
    Ø TS_COVER_HOLE hole. Rocks until the most-pressed keystake has gone TS_STOP (the switch's own stop) and reports
    the first thing met before that: 'plate' (head / flange vs the finished front shell) or 'cover'. (The washer is
    not an obstruction: where it meets the pivot tip the tip slides and turns about its own centre, 1.4 higher, which
    sways the head less than this rolling model.)
    -> {"stop_deg", "first": None or (what, deg, keystake travel then), "travel": travel reached (TS_STOP if clear),
        "plate_clr": head / flange to the plate at the stop (negative = overlap) }"""
    k = KEYS["DPAD"]
    part = dpad() if part is None else part
    x, y = k["at"]
    u = (math.cos(math.radians(phi)), math.sin(math.radians(phi)))
    part = Pos(u[0] * shift, u[1] * shift, 0) * part
    ax = Axis((x + u[0] * shift, y + u[1] * shift, T), (-u[1], u[0], 0))        # + angle: the u side goes down
    low = part & box(x, y, 30, 30, T - 0.1, KEYSTAKE_Z + 1.5)                   # leg tips + pivot tip
    top = part & box(x, y, 34, 34, Z_PLATE_IN - 4.0, 30)                        # flange + head
    plate = front & box(x, y, 34, 34, Z_PLATE_IN - 4.0, Z_FRONT + 0.1)
    stakes = [cyl(cx, cy, TS_KEYSTAKE_D, KEYSTAKE_Z - 1.0, KEYSTAKE_Z + 0.5) for cx, cy in k["contacts"]]
    covers = None
    for cx, cy in k["contacts"]:
        c = box(cx, cy, TS_BODY, TS_BODY, TS_COVER_TOP - 0.5, TS_COVER_TOP) - cyl(cx, cy, TS_COVER_HOLE, 1, 4)
        covers = c if covers is None else covers + c
    def travel(th):
        lr = low.rotate(ax, th)
        zs = [(lr & s).bounding_box().min.Z if (lr & s).solids() else 99 for s in stakes]
        return KEYSTAKE_Z - min(zs)
    stop = _bisect(lambda th: travel(th) >= TS_STOP, 0.0, max_deg)
    hits = {"plate": lambda th: top.rotate(ax, th).distance_to(plate) < 1e-3,
            "cover": lambda th: low.rotate(ax, th).distance_to(covers) < 1e-3}
    first = None
    for what, hit in hits.items():
        if not hit(stop):
            continue
        th = 0.0 if hit(0.0) else _bisect(hit, 0.0, stop)
        if first is None or th < first[1]:
            first = (what, th, max(0.0, travel(th)))
    tr = top.rotate(ax, stop)
    clr = tr.distance_to(plate)
    if clr < 1e-6:
        r = tr & plate
        clr = -(r.volume if r.solids() else 0.0)
    return {"stop_deg": stop, "first": first, "travel": TS_STOP if first is None else first[2], "plate_clr": clr}


# ---------------------------------------------------------------- wall-local frames (X along the wall, Y inward)
def _frame_edge(name):
    """(origin, angle_deg) of an edge key's local frame: origin on the outer surface on the plunger axis, local X
    along the wall, local Y inward along -dir, so local y = depth in from the outer surface."""
    u = EDGE_KEYS[name]["dir"]
    return EDGE_KEYS[name]["outer"], math.degrees(math.atan2(u[0], -u[1]))


def _frame_power():
    return POWER_SLOT["at"], POWER_SLOT["angle_deg"]


def _place(local, frame):
    (ox, oy), ang = frame
    return Pos(ox, oy, 0) * Rot(0, 0, ang) * local


@functools.lru_cache(maxsize=None)
def _inner_wire():
    """the wall's inner face in plan (the shell cavity's outline)."""
    return offset(SILHOUETTE, amount=-WALL).faces()[0].outer_wire()


def _inner_wall_n(frame, t):
    """depth (in from the frame origin) of the wall's inner face at local x = t: WALL where the wall is straight."""
    (ox, oy), ang = frame
    c, s = math.cos(math.radians(ang)), math.sin(math.radians(ang))
    p0 = (ox + t * c + s, oy + t * s - c, 0)                  # 1 mm outside the outer surface
    hit = Line(p0, (p0[0] - 8 * s, p0[1] + 8 * c, 0)).intersect(_inner_wire())
    return min((v.X - p0[0]) * -s + (v.Y - p0[1]) * c for v in hit.vertices()) - 1


# ---------------------------------------------------------------- edge caps (Vol+, Vol-, L, R)
def edge_lip_z0(name):
    """bottom of the retention lip: the lip runs the cap's full height (z SPLIT + CLR_SLIDE up); it sits in front of
    the switch (depth < the contact face), so the SKRT envelope (from depth ~2.77) never reaches it."""
    return SPLIT + CLR_SLIDE


def edge_wall_n(name):
    """depth of the wall's inner face across the lip: the real face, ~WALL (the contract's outer points are rounded)."""
    w = EDGE_LIP_RECESS["w"] - 2 * CLR_SLIDE
    return max(_inner_wall_n(_frame_edge(name), t) for t in (-w / 2, 0, w / 2))


def edge_lip_n(name):
    """depth of the lip's back face: on the floor of EDGE_LIP_RECESS (back_openings cuts it flat, WALL - depth in from
    the contract's outer point along dir)."""
    return WALL - EDGE_LIP_RECESS["depth"]


def edge_neck(name):
    """the cap's thinnest section: the lip, recess floor to the contact face (contract: EDGE_TIP_INSET -
    EDGE_REST_GAP - (WALL - recess depth) = 0.97 >= FDM_MIN)."""
    return EDGE_TIP_INSET - EDGE_REST_GAP - edge_lip_n(name)


def edge_cap(name):
    """a cap riding in its EDGE_NOTCH: a button through the notch, face EDGE_PROUD out, and a lip in the
    EDGE_LIP_RECESS behind the wall, its back on the recess floor (the outward stop), its inner face the contact face
    EDGE_REST_GAP short of the plunger tip. The lip overlaps the notch above it and at both sides (the notch is open
    at the split; the back shell closes it)."""
    w = EDGE_NOTCH[0] - 2 * CLR_SLIDE                         # button across the notch
    wl = EDGE_LIP_RECESS["w"] - 2 * CLR_SLIDE                 # lip across the recess
    z0, z1 = SPLIT + CLR_SLIDE, SPLIT + EDGE_NOTCH[1] - CLR_SLIDE
    zl1 = EDGE_LIP_RECESS["z1"] - CLR_SLIDE
    n_in = EDGE_TIP_INSET - EDGE_REST_GAP                     # contact face, depth in from the outer surface
    n_l = edge_lip_n(name)
    button = box(0, (n_l + 0.01 - EDGE_PROUD) / 2, w, n_l + 0.01 + EDGE_PROUD, z0, z1)
    lip = box(0, (n_l + n_in) / 2, wl, n_in - n_l, edge_lip_z0(name), zl1)
    cap = button + lip
    cap = chamfer(cap.edges().group_by(Axis.Y)[0], EDGE_CHAMFER)                # face edges
    return _solid(_place(cap, _frame_edge(name)), name)


# ---------------------------------------------------------------- power slider (SW1)
def _plate_plan(half):
    """the slider plate's plan (local x, depth) polygon: inner face at POWER_PLATE_IN, outer face on the wall's inner
    face - stood off where the wall bends in, so it clears it over the whole travel."""
    frame, tr = _frame_power(), POWER_SLOT["travel"] / 2
    xs = [-half + i * 0.5 for i in range(int(2 * half / 0.5))] + [half]
    outer = []
    for x in xs:
        need = max(_inner_wall_n(frame, x + s) for s in (-tr, -tr / 2, 0, tr / 2, tr))
        outer.append((x, WALL if need <= WALL + 0.005 else need + POWER_WALL_CLR))
    return outer + [(half, POWER_PLATE_IN), (-half, POWER_PLATE_IN)]


def power_slider(shift=POWER_REST_SHIFT):
    """captive slider in POWER_SLOT, slid 'shift' along the slot (+ = east, +-travel/2 = the lever's two ends): knob
    through the slot, plate behind the wall, fork round the SW1 lever (POWER_FORK_PLAY each side, bridged above it)."""
    s = POWER_SLOT
    z0, zk1, zp1 = s["z0"] + CLR_SLIDE, s["z0"] + s["h"] - CLR_SLIDE, s["z0"] + s["h"] + POWER_LIP
    n_k = WALL + 0.3                  # the knob runs 0.3 into the plate (whose face steps off the wall bend)
    knob = box(0, (n_k - POWER_KNOB_PROUD) / 2, s["len"] - s["travel"], n_k + POWER_KNOB_PROUD, z0, zk1)
    knob = chamfer(knob.edges().group_by(Axis.Y)[0], EDGE_CHAMFER)   # knob width = slot - travel: the slot ends stop it
    half = s["len"] / 2 + s["travel"] / 2 + POWER_LIP
    plate = prism(face(_plate_plan(half), board_frame=False), z0, zp1)
    gap = SW1_LEVER_W + 2 * POWER_FORK_PLAY
    n_end = SW1_TIP_N + SW1_LEVER_LEN - POWER_BODY_CLR
    fork = box(0, (POWER_PLATE_IN + n_end) / 2, gap + 2 * POWER_TINE_W, n_end - POWER_PLATE_IN, z0, zp1)
    pocket = box(0, (WALL + FDM_MIN + n_end + 0.2) / 2, gap, n_end + 0.2 - WALL - FDM_MIN,
                 z0 - 0.2, SW1_LEVER_Z[1] + CLR_SLIDE)        # the lever's pocket, open below: the board drops in
    return _solid(_place(Pos(shift, 0, 0) * (knob + plate + fork - pocket), _frame_power()), "POWER")


# ---------------------------------------------------------------- all + print pose
def all_controls():
    """{name: solid} for every printed control at rest."""
    out = {n: key(n) for n in ("A", "B", "START", "SELECT")}
    out["DPAD"] = dpad()
    out["DPAD_WASHER"] = dpad_washer()
    out.update({n: edge_cap(n) for n in EDGE_KEYS})
    out["POWER"] = power_slider()
    return out


def print_pose(part):
    """a control (as built above) laid on the bed, centred on the origin: keys and the D-pad head-down (flat top on
    the bed, stems up); edge caps and the slider as assembled, turned square to X, bottom face down; the D-pad
    washer flat."""
    name = part.label
    if name in KEYS:
        p = Rot(180, 0, 0) * part
    elif name == "DPAD_WASHER":
        p = part
    elif name in EDGE_KEYS or name == "POWER":
        (ox, oy), ang = _frame_edge(name) if name in EDGE_KEYS else _frame_power()
        p = Rot(0, 0, -ang) * Pos(-ox, -oy, 0) * part
    else:
        raise ValueError(f"print_pose: unknown control {name!r}")
    bb = p.bounding_box()
    p = Pos(-(bb.min.X + bb.max.X) / 2, -(bb.min.Y + bb.max.Y) / 2, -bb.min.Z) * p
    p.label = name
    return p


# ---------------------------------------------------------------- self-test (python controls.py)
def _t_vol(a, b):
    if not a.solids() or not b.solids():
        return 0.0
    r = a & b
    return sum(s.volume for s in r.solids()) if r is not None else 0.0


def _t_near(a, b, pad=1.0):
    """bounding boxes within pad of each other."""
    p, q = a.bounding_box(), b.bounding_box()
    return all(lo - pad <= hi2 and lo2 - pad <= hi for lo, hi, lo2, hi2 in
               zip(tuple(p.min), tuple(p.max), tuple(q.min), tuple(q.max)))


def _t_envelope(ref):
    """one part's envelope, built as params.part_envelopes() builds it (None if it has none)."""
    p = PARTS[ref]
    if p["height"] < 0.1:
        return None
    try:
        f = face(p["courtyard"])
    except Exception:
        return None
    return prism(f, T, T + p["height"]) if p["side"] == "top" else prism(f, -p["height"], 0)


def _t_cuts():
    """where front_features / back_openings will cut, from the contracts: face holes through the plate, edge notches +
    their EDGE_LIP_RECESS and the power slot through the wall. A control may meet the bare shells only inside these."""
    out = {}
    for n, k in KEYS.items():
        out[n] = _t_hole(n, DPAD_HOLE_CLR if n == "DPAD" else KEY_HOLE_CLR)
    r = EDGE_LIP_RECESS
    for n in EDGE_KEYS:
        notch = box(0, 1, EDGE_NOTCH[0], 4, SPLIT, SPLIT + EDGE_NOTCH[1])
        recess = box(0, (WALL - r["depth"] + WALL + 0.3) / 2, r["w"], r["depth"] + 0.3, SPLIT, r["z1"])
        out[n] = _place(notch + recess, _frame_edge(n))
    s = POWER_SLOT
    out["POWER"] = _place(box(0, 1, s["len"], 4, s["z0"], s["z0"] + s["h"]), _frame_power())
    return out


def _t_hole(name, clr):
    """a face hole through the plate: the head outline + clr (radial)."""
    k = KEYS[name]
    x, y = k["at"]
    if name == "DPAD":                                       # front_keys' outline (clr = DPAD_HOLE_CLR)
        return prism(dpad_hole_face(), Z_PLATE_IN - 0.01, Z_FRONT + 0.01)
    return cyl(x, y, k["d"] + 2 * clr, Z_PLATE_IN - 0.01, Z_FRONT + 0.01)


def _t_skrt(name):
    """the real SKRTLBE010 at an edge key (C127481), finer than its courtyard box: (Ø2 plunger from its tip to the
    body face, 0.84 long; body 4.5 x 2.56 x 3.3 tall)."""
    face_n, z = EDGE_TIP_INSET + 0.84, SPLIT + 1.7
    plunger = Pos(0, (EDGE_TIP_INSET + face_n) / 2, z) * Rot(90, 0, 0) * Cylinder(1.0, 0.84)
    return _place(plunger, _frame_edge(name)), _place(box(0, face_n + 1.28, 4.5, 2.56, T, T + 3.3), _frame_edge(name))


def _t_sw1(lever_t):
    """the real MSK12C02-HB (C431541), its lever at lever_t along the slot: lever + body."""
    n_face = SW1_TIP_N + SW1_LEVER_LEN
    lever = box(lever_t, (SW1_TIP_N + n_face) / 2, SW1_LEVER_W, SW1_LEVER_LEN, *SW1_LEVER_Z)
    body = box(0, n_face + SW1_BODY[1] / 2, SW1_BODY[0], SW1_BODY[1], T, T + SW1_BODY[2])
    return _place(lever, _frame_power()), _place(body, _frame_power())


def _t_thin(part, r):
    """erode by r: (pieces, faces with no point whose r-deep inward probe survives the erosion). A section under 2r
    either splits the eroded body (a neck) or vanishes from it (a fin / lip): its faces then fail the probe."""
    try:
        er = offset(part, amount=-r, kind=Kind.INTERSECTION)
    except Exception:
        return None, []
    bad = []
    for f in part.faces():
        if f.area < 0.05:
            continue
        ok = False
        for i in range(25):
            pt = f.position_at(0.1 + 0.2 * (i // 5), 0.1 + 0.2 * (i % 5))
            if f.distance_to(pt) < 1e-4 and er.is_inside(pt - f.normal_at(pt) * (r + 0.0005)):
                ok = True
                break
        if not ok:
            bad.append(f)
    return len(er.solids()), bad


def _t_solid_ok(part):
    """(solids, thinnest-section verdict): the FDM_MIN/2 erosion (0.399) leaves one solid behind every face iff no
    section is under FDM_MIN."""
    n0, bad0 = _t_thin(part, FDM_MIN / 2 - 0.001)
    if n0 == 1 and not bad0:
        return len(part.solids()), ">= 0.8"
    n1, bad1 = _t_thin(part, 0.36)
    where = ", ".join(f"z{f.center().Z:.2f}" for f in bad0[:3])
    return len(part.solids()), (("0.72-0.8" if n1 == 1 and not bad1 else "< 0.72" if n1 else "offset failed")
                                + f" ({n0} pieces, {len(bad0)} thin faces {where})")


def _selftest():
    import shell
    from params import board_solid, module_2090, battery, speaker, lra, GUIDE_BOTTOM_Z
    ctl, cuts = all_controls(), _t_cuts()
    front, back = shell.shells()
    ctx = {"board": board_solid(), "2090": module_2090(), "battery": battery(), "speaker": speaker(), "lra": lra()}
    envs = {r: e for r in PARTS if (e := _t_envelope(r)) is not None}
    sw1_real = _t_sw1(POWER_SLOT["travel"] / 2)
    print("control: solids / thinnest section / volume | front shell: total (outside its cut zones) / back | "
          "nearest context (d = distance, v = volume)")
    for n, s in ctl.items():
        k, thin = _t_solid_ok(s)
        vf = _t_vol(s, front)
        out = _t_vol(s, front - cuts[n]) if vf else 0.0
        near = sorted((round(s.distance_to(c), 3), r, round(_t_vol(s, c), 4)) for r, c in {**ctx, **envs}.items()
                      if _t_near(s, c))[:6]
        print(f"  {n}: {k} / {thin} / {s.volume:.1f} | {vf:.2f} ({out:.4f}) / {_t_vol(s, back):.4f} | "
              + ", ".join(f"{r} d{d} v{v}" for d, r, v in near))
    names = list(ctl)
    pairs = [(ctl[a].distance_to(ctl[b]), a, b, _t_vol(ctl[a], ctl[b]) if _t_near(ctl[a], ctl[b], 0.1) else 0.0)
             for i, a in enumerate(names) for b in names[i + 1:]]
    pairs.sort()
    print("controls vs each other: max volume", f"{max(p[3] for p in pairs):.4f};", "closest",
          ", ".join(f"{a}-{b} {d:.2f}" for d, a, b, _ in pairs[:3]))

    print("face keys:")
    for n in ("A", "B", "START", "SELECT"):
        k, s = KEYS[n], ctl[n]
        x, y = k["at"]
        tip = KEYSTAKE_Z + KEY_REST_GAP
        tb = (s & cyl(x, y, 20, tip - 0.1, tip + 0.05)).bounding_box()
        below = s & cyl(x, y, 30, 0, Z_PLATE_IN - FLANGE_T - 0.01)
        past = _t_vol(below, below - cyl(x, y, k["fin_od"] + 0.002, 0, 30))
        fins = s & (cyl(x, y, k["fin_od"] + 1, FIN_Z[0] - 1, Z_PLATE_IN - FLANGE_T - 0.01) - cyl(x, y, k["stem_d"] + 0.02, 0, 30))
        fb = fins.bounding_box()
        print(f"  {n}: tip z {tb.min.Z:.3f} (keystake {KEYSTAKE_Z} + {KEY_REST_GAP}), flat Ø{tb.size.X:.2f} "
              f"(tip_d {k['tip_d']}); flange Ø{k['flange_d']:.1f} seat on the plate (holed) {(front - cuts[n]).distance_to(s):.3f}; "
              f"{len(fins.solids())} guide ring Ø{k['fin_od']:.1f} z {fb.min.Z:.2f}..{fb.max.Z:.2f} (full OD to "
              f"{FIN_Z[1]}), outside Ø fin_od {past:.4f}; bore clearance {(k['guide_id'] - k['fin_od']) / 2:.2f}/side; "
              f"ring at the hard stop z {FIN_Z[0] - KEY_REST_GAP - TS_STOP:.2f} (tube ends {GUIDE_BOTTOM_Z})")

    print("edge caps:")
    for n in EDGE_KEYS:
        plunger, body = _t_skrt(n)
        a = EDGE_KEYS[n]["dir"]
        cut_front = front - cuts[n]
        flush = Pos(-a[0] * EDGE_PROUD, -a[1] * EDGE_PROUD, 0) * ctl[n]
        print(f"  {n}: to plunger tip {ctl[n].distance_to(plunger):.3f}, body {ctl[n].distance_to(body):.3f}; lip on "
              f"the recess floor {cut_front.distance_to(ctl[n]):.3f} (overlap {_t_vol(ctl[n], cut_front):.4f}), "
              f"in the bare wall (the recess zone) {_t_vol(ctl[n], front & cuts[n]):.2f} mm3; section behind the wall "
              f"{edge_neck(n):.3f} (inner face {edge_wall_n(n):.3f}); proud {EDGE_PROUD}; pushed flush (drives the "
              f"plunger {EDGE_PROUD - EDGE_REST_GAP:.2f}, it travels 0.2) still {flush.distance_to(body):.3f} off "
              f"the body")

    dp, wa = ctl["DPAD"], ctl["DPAD_WASHER"]
    k = KEYS["DPAD"]
    tip = KEYSTAKE_Z + DPAD_REST_GAP
    legs = [(dp & cyl(cx, cy, 4, tip - 0.01, tip + 0.05)).bounding_box() for cx, cy in k["contacts"]]
    holed = front - cuts["DPAD"]
    print(f"D-pad: pivot on board {ctx['board'].distance_to(dp):.3f}; domed legs (R{LEG_DOME_R}) lowest at z "
          f"{', '.join(f'{b.min.Z:.3f}' for b in legs)} (keystake {KEYSTAKE_Z} + {DPAD_REST_GAP}); flange "
          f"{dpad_flange_gap()} under the plate (rest: flange to the holed plate "
          f"{holed.distance_to(dp & box(*k['at'], 30, 30, 0, Z_PLATE_IN - 0.01)):.3f}, head to the hole "
          f"{holed.distance_to(dp & box(*k['at'], 30, 30, Z_PLATE_IN, 30)):.3f}, hole clr {DPAD_HOLE_CLR})")
    print(f"  washer Ø{dpad_washer_od()} x {WASHER_T}, hole Ø{WASHER_HOLE_D}: to the pivot {wa.distance_to(dp):.3f}, "
          f"to the switch bodies {min(wa.distance_to(envs[r]) for r in ('SW2', 'SW3', 'SW4', 'SW5')):.3f}, to the board "
          f"{wa.distance_to(ctx['board']):.3f}; pivot play {dpad_pivot_play():.3f}; worst cross offset "
          f"{dpad_shift_max():.3f} (+ washer {WASHER_CLR} + board {FINGER['gap']})")
    print("  press toward | shift | keystake stop at deg (formula) | first obstruction before the 0.3 stop | travel | "
          "head/flange to plate at the stop")
    sm = dpad_shift_max()
    for phi, lbl in ((0, "SW5 (axial)"), (22.5, "22.5 deg"), (45, "SW5+SW2 diag")):
        for sh in (-sm, 0.0, sm):
            r = dpad_rock(holed, phi, sh, part=dp)
            f = r["first"]
            print(f"  {lbl:13s} | {sh:+.2f} | {r['stop_deg']:.2f} ({math.degrees(dpad_stop_tilt(phi)):.2f}) | "
                  f"{'none' if f is None else f'{f[0]} at {f[1]:.2f} deg'} | {r['travel']:.3f} | {r['plate_clr']:.3f}")

    for sh in (-POWER_SLOT["travel"] / 2, POWER_SLOT["travel"] / 2):
        p, (lever, body) = power_slider(sh), _t_sw1(sh)
        print(f"  POWER at {sh:+.1f}: lever {p.distance_to(lever):.3f}, body {p.distance_to(body):.3f}, "
              f"shell outside slot {_t_vol(p, front - cuts['POWER']):.4f}/{_t_vol(p, back):.4f}, in the slot "
              f"{_t_vol(p, front & cuts['POWER']):.2f}, SW1 envelope (courtyard bbox artefact) "
              f"{_t_vol(p, envs['SW1']):.2f} mm3, thinnest {_t_solid_ok(p)[1]}")
    for n, s in ctl.items():
        bb = print_pose(s).bounding_box()
        assert abs(bb.min.Z) < 1e-6 and abs(bb.min.X + bb.max.X) < 1e-6, n
    print("print_pose: all on z 0, centred")


if __name__ == "__main__":
    _selftest()
