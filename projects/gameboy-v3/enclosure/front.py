"""front.py - the gb3 front shell's features (Architecture A; the contract is CONTRACT.md "Ownership: front.py").

build(ctx) -> {"front_add", "front_cut", "back_cut", "parts", "keepouts", "notes"} (enclosure.py applies them).

Built here (every z from params names, so the GB3_* flags rebuild it):
  pocket       params.POCKET (121.4 x 76.5) through the face, from the ledge top up. The visible gap round the glass is
               0.65 / 0.55. A relief for the panel's unlabeled right-edge tab (panel report: ~0.8 proud, KiCad
               y 40.4-46.4) is cut 0.7 into the right frame wall below the plate.
               GB3_FRONT=lip: the pocket is cut from inside up to the lip's underside (Z_FACE - LIP), and
               params.LIP_WINDOW (R0.5 corners, 0.4 x 45 deg chamfer at the face) goes through the lip. The module then
               goes in from inside, so there is NO ledge (it would block it). The die-cut gasket under the lip (part
               "lip gasket") holds it, backed by foam pads on bare board (panel report option a).
  frame        1.0 walls round the pocket, from the plate down to LEDGE_Z0 on the top, left and bottom-left (they
               carry the ledge) and on the right over the board's top strip (FRAME_T_RIGHT 1.7: FRAME_T stays
               behind the tab relief).
               Over the cell (the right side, the bottom east of the fold bay) they stop FRAME_CELL_GAP above the
               swollen cell's air / the LCM back (cell_frame_z0(), z 4.00): they stiffen the 5.15 bezel there and
               locate the LCM on all four sides. None in the fold bay or the cell-lead corner (KiCad y 52.3-54.6).
  ledge        params.LEDGE: the top / left / bottom-left strips, z LEDGE_Z0..LEDGE_Z1, reaching LEDGE_W under the LCM.
               Its underside is pocketed over C20 / C94 up to their tops + 2 CLR_F (0.75 of ledge left there). The
               "ledge tape" part (bought die-cut acrylic tape) is the PANEL_TAPE between the ledge and the LCM back.
  fold bay     the plate's inner face over params.FOLD_BAY["rect"] cut up to Z_FOLD_SKIN_IN (skin 0.65); no ribs,
               bosses or frame in the bay. (lip: the plate's inner face is already above Z_FOLD_SKIN_IN: no cut.)
  key holes    params.key_hole_face() through the plate for A / B / X / Y / START / SELECT (round, hole_d), and the
               D-pad cross with its 8 arm-end corners rounded DPAD_HOLE_R. Nothing of ours inside clear_d (round) /
               key_flange_face("DPAD") + KEY_SWEEP_CLR from SPLIT to the plate.
  LEDs         a window over U5 / U14: a light-pipe tube from the plate down to LED_GAP over the LED (stop ring
               LED_STOP_ID at its foot), bore LED_BORE through the plate for a bought D2.0 acrylic rod (parts
               "LED pipe U5" / "LED pipe U14", flush with the face).
  CN1          the plate thinned to PLATE_CN1 over CN1["rect"] + CN1_CLR (only when CN1's top + CLR_F would not clear
               the full plate: the lip variant's plate clears it).
  bosses       D BOSS_D at every params.BOSSES entry, SPLIT up into the plate, M2 heat-set hole INSERT hole_d x depth
               with a 0.3 x 45 deg lead-in. Each on-board boss is merged into a wall by a BOSS_D-wide block (the
               gusset) that stops MERGE_GAP over the board so only the boss tip touches it: the nearest wall, except
               H3 (SUPPORT_DIR: into the right wall, which leaves the top wall free for the R shoulder's yoke);
               S1 / S2 already sit in the wall.
  posts        D POST_D posts from the plate to POST_GAP over the board's top face at free board spots round the keys
               (D-pad diagonals, START sides, the ABXY centre, between X and SELECT, SELECT sides). The plate is 1.2
               PA12 spanning ~35 mm in each grip; a firm press beside a key would otherwise push the key's flange
               (gb2 STRUCTURAL_REVIEW #1). The board backs the posts. POSTS = () turns them off.
  jack         the D hole_d opening on JACK["axis_z"] through the bottom wall, open at the split (U-slot: the barrel
               comes in with the board), and a D(shoulder_d + 2 JACK_SPOT_CLR) spot-face from the outside to
               JACK_SPOT_DEEPER past the barrel tip (the plug shoulder seats on the barrel). The hole and spot-face
               dip under the split, so they are cut from the back shell's rim too ("back_cut").
  notches      EDGE_KEYS (VOL+, VOL-, POWER: right wall) and SHOULDERS (L, R: top wall): notch_w x notch_z through the
               wall, open at the split; the back shell's rim closes them from below.

Published keep-outs (for the other modules; the checks test their material against them):
  "panel"        the pocket column from LEDGE_Z1 to the face (lip: to the lip underside) + the tab relief
  "display flex" the display flex's run under the panel, board top to the LCM back
  "touch flex"   the touch flex strip (+0.3 / +0.5) from the glass exit round the U-turn to the FPC1 lid, + its IC area
  "fold bay"     FOLD_BAY["rect"] extended to the notch edge (KiCad y 111.0), z FOLD_BAY z0..Z_FOLD_SKIN_IN
  "key <name>"   the clearance rings round each cap: flange_d..clear_d from SPLIT to the plate, d..hole_d through the
                 plate (D-pad: the flange outline .. + KEY_SWEEP_CLR, and the hole less the cross + hole_clr / 2). The
                 caps themselves sit inside the rings, so the rings check the cap sizes against the contract.

Assembly (notes() prints it): flush - stand the module on its glass bottom edge in the pocket with the flexes
hanging into the cavity, then swing it flat about that edge onto the ledge tape (swing_in() sweeps it); lip - the
module goes in glass-first from inside onto the lip gasket, its flexes plugged first (panel + board as a book).

`.venv/bin/python front.py [--no-render]` runs the self-test (front shell + this module vs the context, the core
keep-outs, its own keep-outs and the contract; thickness probes; the panel insertion) and writes renders to
build/front/ (build/front/<flags>/ for a variant).
"""
import functools, math
from build123d import Compound, Cone, Cylinder, Pos, Rot, fillet
import params as P
import context as C

# ---------------------------------------------------------------- choices (this module only)
FUSE = 0.2                 # adds run this far into the plate / wall they hang from, so the union fuses solidly
CUT = 0.5                  # cutters overshoot the faces they go through
FRAME_T = 1.0              # pocket frame walls (synth ledge.py: wall 1.0 outside the pocket)
FRAME_CELL_CLR = 0.3       # the right frame wall stops this short of the cell buffer (the cell-lead corner is beyond)
FRAME_CELL_GAP = 0.1       # the frame walls over the cell end this far above the swollen cell's air / the LCM back
FOLD_CLR = 0.3             # the frame over the cell starts this far east of the fold bay
TAB_RELIEF = {"y_k": (40.4, 46.4), "x1_k": 178.5, "clr": 0.25, "margin": 0.3}   # panel report: unlabeled tab
RELIEF_CLR = 0.3           # ledge-underside relief over C20 / C94: part rect + this, ...
RELIEF_DZ = 2 * float(P.CLR_F)   # ... up to the part's top + this
DPAD_HOLE_R = 0.5          # the D-pad hole's 8 arm-end corners (gb2 STRUCTURAL_REVIEW #6: sharp corners are the
                           # face's most stressed spot); diagonal clearance to an R0.5 cap corner stays 0.99
LED_ROD_D = 2.0            # bought acrylic light pipe, flush with the face
LED_BORE = LED_ROD_D + 0.2  # MJF holes +/-0.3 (JLC): 0.1 radial, ream to fit and glue
LED_WALL = 1.0             # >= FDM's 2 perimeters (0.86) too
LED_STOP_ID = 1.4          # stop ring the rod rests on (light aperture)
LED_STOP_T = 0.6
LED_GAP = 0.3              # stop ring over the LED's top
CN1_CLR = 0.5              # the thinned plate pocket round CN1's hull
WALL_INTO = 1.0            # merge blocks run this far into the wall (clipped there: the outer fillets stay intact)
MERGE_GAP = 0.3            # merge blocks / webs stop this far over the board: only the boss tip clamps it
MERGE_MAX = P.BOSS_MERGE_MAX      # bosses closer than this to a wall get a merge block, farther ones two webs
SUPPORT_DIR = P.BOSS_MERGE_DIR    # merge toward this wall instead of the nearest: H3 (3.25 off the top wall) goes to
                                  # the right wall, so the top wall stays free from H2 to the corner for the R
                                  # shoulder's yoke (both are params contract names: controls.yoke_span reads them)
WEB_T = 1.2
INSERT_LEAD = 0.3          # 45 deg lead-in at each insert hole mouth
POST_D = 3.0
POST_GAP = 0.2             # post tips over the board's top face (the board is the datum at the boss tips)
POST_CLR = 0.5             # posts keep this from every F-part hull and every key clearance zone (self-test checks it)
DPAD_POST_OFF = 8.0        # the D-pad posts sit on its diagonals this far from the centre in x and y
JACK_IN = 0.3              # the jack opening reaches this far past the wall's inner face
JACK_SPOT_CLR = 0.3        # spot-face radial clearance round the plug shoulder
JACK_SPOT_DEEPER = 0.1     # spot-face floor this far behind the barrel tip (the shoulder seats on the tip, not on us)
NOTCH_IN = 0.1             # edge notches reach this far past the wall's inner face
LIP_CHAMFER = 0.4          # 45 deg at the lip window's face edge (panel report: helps the viewing angle)
LIP_WINDOW_R = 0.5
FLEX_KO_T = 0.3            # touch-flex keep-out: strip thickness + 2 x this ...
FLEX_KO_W = 0.5            # ... and width + 2 x this
DISP_KO = (82.9, 65.0, 132.9)   # display flex run under the panel: KiCad x0, y0, x1 (flex x 83.4-132.4 + 0.5;
                                # its finger end at P1's insertion stop, y ~66.3 +/-0.7)

LIP = P.FRONT == "lip"
Z_POCKET_TOP = P.Z_FACE - P.LIP if LIP else P.Z_FACE      # the pocket's top: the lip's underside / the face
PK = P.POCKET
NOTCH_EDGE_K = next(n["rect"][1] for n in P.BJ["board"]["notches"] if "cable" in n["name"])   # 111.0 (KiCad y)
TAB_RELIEF_DEPTH = round(TAB_RELIEF["x1_k"] + TAB_RELIEF["clr"] - PK.x1, 6)     # 0.70 into the right frame wall
FRAME_T_RIGHT = round(TAB_RELIEF_DEPTH + FRAME_T, 6)   # 1.70: the right frame wall keeps FRAME_T behind the relief


def _k(x, y):
    """KiCad -> enclosure (P.P)."""
    return P.P(x, y)


# ---------------------------------------------------------------- small helpers
def _strip(p0, p1, t, z0, z1):
    """XY rectangle from p0 to p1 (centre line), t wide, z0..z1 (enclosure frame)."""
    (x0, y0), (x1, y1) = p0, p1
    L = math.hypot(x1 - x0, y1 - y0)
    nx, ny = -(y1 - y0) / L * t / 2, (x1 - x0) / L * t / 2
    return P.prism(P.face([(x0 + nx, y0 + ny), (x1 + nx, y1 + ny), (x1 - nx, y1 - ny), (x0 - nx, y0 - ny)],
                          board_frame=False), z0, z1)


def _cyl_y(x, z, d, y0, y1):
    """a cylinder along Y (enclosure), axis at (x, z), over y0..y1."""
    return Pos(x, (y0 + y1) / 2, z) * Rot(90, 0, 0) * Cylinder(d / 2, abs(y1 - y0))


def _fuse(shapes):
    shapes = [s for s in shapes if s is not None]
    if len(shapes) == 1:
        return shapes[0]
    s = shapes[0].fuse(*shapes[1:]).clean()
    return s.solids()[0] if len(s.solids()) == 1 else s


@functools.lru_cache(maxsize=None)
def _inside():
    """the outline WALL_INTO short of the outer surface: adds that run into the wall are clipped to it."""
    return P.prism(P.outline_face(P.WALL - WALL_INTO), P.SPLIT - 1.0, P.Z_FACE + 1.0)


def wall_nearest(x, y):
    """(nearest point, distance, outward unit) on the walls' inner face (params.WALL_IN, corner OUTLINE_R - WALL) from
    an inside point: WALL_IN is its core rectangle grown by the corner radius (a Minkowski sum)."""
    w, r = P.WALL_IN, P.OUTLINE_R - P.WALL
    c0x, c1x, c0y, c1y = w.x0 + r, w.x1 - r, w.y0 + r, w.y1 - r
    qx, qy = min(max(x, c0x), c1x), min(max(y, c0y), c1y)
    if math.hypot(x - qx, y - qy) > 1e-9:                     # outside the core: radially from its nearest point
        d = math.hypot(x - qx, y - qy)
        ux, uy = (x - qx) / d, (y - qy) / d
        return (qx + r * ux, qy + r * uy), r - d, (ux, uy)
    d, (ux, uy) = min([(x - c0x, (-1.0, 0.0)), (c1x - x, (1.0, 0.0)), (y - c0y, (0.0, -1.0)), (c1y - y, (0.0, 1.0))])
    return (x + (d + r) * ux, y + (d + r) * uy), d + r, (ux, uy)


# ---------------------------------------------------------------- 1. pocket, frame, ledge (+ the lip variant)
@functools.lru_cache(maxsize=None)
def tab_relief():
    """the column cut into the right frame wall for the panel's unlabeled tab (below the plate)."""
    t = TAB_RELIEF
    y0, y1 = t["y_k"]
    r = P.Rect.k(PK.x1 - 0.01, y0 - t["margin"], t["x1_k"] + t["clr"], y1 + t["margin"])
    return r.prism(P.LEDGE_Z1, P.Z_PLATE_IN)


def cell_frame_z0():
    """the bottom of the frame walls over the cell: above the swollen cell + CELL_AIR (the core keep-out "cell
    swell"), the SUS plate + its air (LCD_PLATE) and the LCM back, + FRAME_CELL_GAP."""
    zs = [P.Z_PANEL_LCM[0], P.CELL["swell_z"][1] + (0.0 if P.LCD_PLATE else P.CELL_AIR)]
    if P.SS_Z:
        zs.append(P.SS_Z[1] + P.SS_AIR)
    return round(max(zs) + FRAME_CELL_GAP, 6)


def frame_rects():
    """{name: (enclosure-frame Rect, z0)}: the pocket frame walls, from z0 up into the plate. Down to LEDGE_Z0 on the
    top, left and bottom-left (they carry the ledge) and on the right over the board's top strip; over the cell (the
    right side below the lead corner, the bottom east of the fold bay) only down to cell_frame_z0()."""
    t, tr = FRAME_T, FRAME_T_RIGHT
    buf, cell = P.CELL["buffer"], P.CELL["rect"]
    z0, zc = P.LEDGE_Z0, cell_frame_z0()
    lead_y = cell.y1 - FRAME_CELL_CLR                         # the cell-lead riser corner is above (KiCad y < 54.3)
    return {
        "top": (P.Rect(PK.x0 - t, PK.y1, PK.x1 + tr, PK.y1 + t), z0),
        "left": (P.Rect(PK.x0 - t, PK.y0 - t, PK.x0, PK.y1 + t), z0),
        "bottom_left": (P.Rect(PK.x0 - t, PK.y0 - t, P.LEDGE_BL_X1, PK.y0), z0),
        "right": (P.Rect(PK.x1, buf.y1 + FRAME_CELL_CLR, PK.x1 + tr, PK.y1 + t), z0),
        "right_cell": (P.Rect(PK.x1, PK.y0 - t, PK.x1 + tr, lead_y), zc),
        "bottom_cell": (P.Rect(P.FOLD_BAY["rect"].x1 + FOLD_CLR, PK.y0 - t, PK.x1 + tr, PK.y0), zc),
    }


@functools.lru_cache(maxsize=None)
def frame():
    """the frame walls round the pocket, plate (+FUSE) down to their z0, less the tab relief."""
    walls = [r.prism(z0, P.Z_PLATE_IN + FUSE) for r, z0 in frame_rects().values()]
    return _fuse(walls) - tab_relief()


def ledge_reliefs():
    """underside pockets over C20 / C94 (merged into one where they nearly touch)."""
    rs = [v["rect"].grow(RELIEF_CLR) for v in P.LEDGE["relief"].values()]
    top = max(v["top_z"] for v in P.LEDGE["relief"].values()) + RELIEF_DZ
    r = P.Rect(min(q.x0 for q in rs), min(q.y0 for q in rs), max(q.x1 for q in rs), max(q.y1 for q in rs))
    return r.prism(P.LEDGE_Z0 - CUT, top), top


@functools.lru_cache(maxsize=None)
def ledge():
    """the ledge strips (each run FUSE into its frame wall), less the C20 / C94 relief. None in the lip variant."""
    if LIP:
        return None
    s = P.LEDGE["segments"]
    z0, z1 = P.LEDGE["z"]
    strips = [P.Rect(s["top"].x0, s["top"].y0, s["top"].x1, s["top"].y1 + FUSE).prism(z0, z1),
              P.Rect(s["left"].x0 - FUSE, s["left"].y0, s["left"].x1, s["left"].y1).prism(z0, z1),
              P.Rect(s["bottom_left"].x0, s["bottom_left"].y0 - FUSE, s["bottom_left"].x1,
                     s["bottom_left"].y1).prism(z0, z1)]
    return _fuse(strips) - ledge_reliefs()[0]


@functools.lru_cache(maxsize=None)
def ledge_tape():
    """the bought die-cut tape between the ledge top and the LCM back: the ledge strips under the nominal LCM."""
    if LIP:
        return None
    lcm = P.PANEL["lcm"]
    out = []
    for r in P.LEDGE["segments"].values():
        q = P.Rect(max(r.x0, lcm.x0), max(r.y0, lcm.y0), min(r.x1, lcm.x1), min(r.y1, lcm.y1))
        if q.w > 0 and q.h > 0:
            out.append(q.prism(P.LEDGE_Z1, P.Z_LCM_BACK))
    return _fuse(out)


def pocket_cut():
    return PK.prism(P.LEDGE_Z1, Z_POCKET_TOP + (CUT if not LIP else 0.0))


@functools.lru_cache(maxsize=None)
def lip_window_cut():
    """lip variant: the window through the lip with a LIP_CHAMFER x 45 deg chamfer at the face."""
    w = P.LIP_WINDOW
    f = w.face(LIP_WINDOW_R)
    straight = P.prism(f, Z_POCKET_TOP - CUT, P.Z_FACE - LIP_CHAMFER + 0.01)
    from build123d import extrude
    cham = Pos(0, 0, P.Z_FACE - LIP_CHAMFER) * extrude(f, amount=LIP_CHAMFER + CUT, dir=(0, 0, 1), taper=-45)
    return _fuse([straight, cham])


@functools.lru_cache(maxsize=None)
def lip_gasket():
    """lip variant: the bought 0.1 die-cut gasket on the glass's black border under the lip."""
    if not LIP:
        return None
    g = P.PANEL["glass"]
    ring = g.prism(P.Z_GLASS_MAX, Z_POCKET_TOP) - P.LIP_WINDOW.grow(0.3).prism(P.Z_GLASS_MAX - 1, Z_POCKET_TOP + 1)
    return ring


# ---------------------------------------------------------------- 2. fold bay skin, CN1 thinning
def fold_skin_cut():
    """the plate's inner face over the fold bay up to Z_FOLD_SKIN_IN (None when the plate is already above it)."""
    if P.Z_PLATE_IN >= P.Z_FOLD_SKIN_IN - 1e-9:
        return None
    return P.FOLD_BAY["rect"].prism(P.Z_PLATE_IN - CUT, P.Z_FOLD_SKIN_IN)


def cn1_needs_thinning():
    return P.CN1["top_z"] + P.CLR_F > P.Z_PLATE_IN + 1e-9


def cn1_cut():
    if not cn1_needs_thinning():
        return None
    return P.CN1["rect"].grow(CN1_CLR).prism(P.Z_PLATE_IN - CUT, P.CN1["plate_in_z"])


# ---------------------------------------------------------------- 3. key holes
@functools.lru_cache(maxsize=None)
def dpad_hole_face():
    """params.key_hole_face('DPAD') with its 8 arm-end corners rounded DPAD_HOLE_R."""
    k = P.KEYS["DPAD"]
    f = P.key_hole_face("DPAD")
    cx, cy = k["at"]
    a = k["arm"] + k["hole_clr"]
    vs = [v for v in f.vertices() if max(abs(v.X - cx), abs(v.Y - cy)) > a - 1e-6]
    return fillet(vs, DPAD_HOLE_R).faces()[0]


def hole_face(name):
    return dpad_hole_face() if P.KEYS[name]["shape"] == "cross" else P.key_hole_face(name)


def key_hole_cuts():
    return [P.prism(hole_face(n), P.Z_PLATE_IN - CUT, P.Z_FACE + CUT) for n in P.KEYS]


def _cross_face(grow):
    k = P.KEYS["DPAD"]
    cx, cy = k["at"]
    return P.face([(cx + x, cy + y) for x, y in P._cross_pts(k["arm"], k["w"], grow)], board_frame=False)


def key_clear_zone(name, z0=None, z1=None):
    """the column front.py keeps its material out of: D clear_d (round) / the flange outline + KEY_SWEEP_CLR."""
    k = P.KEYS[name]
    z0 = P.SPLIT if z0 is None else z0
    z1 = P.Z_PLATE_IN if z1 is None else z1
    if k["shape"] == "round":
        return P.cyl(*k["at"], k["clear_d"], z0, z1)
    return P.prism(_cross_face(k["hole_clr"] + k["flange_lip"] + P.KEY_SWEEP_CLR), z0, z1)


@functools.lru_cache(maxsize=None)
def key_keepout(name):
    """the clearance rings round a cap (see the module docstring)."""
    k = P.KEYS[name]
    if k["shape"] == "round":
        x, y = k["at"]
        flange = P.cyl(x, y, k["clear_d"], P.SPLIT, P.Z_PLATE_IN) - \
            P.cyl(x, y, k["flange_d"], P.SPLIT - 1, P.Z_FACE + 1)
        head = P.cyl(x, y, k["hole_d"], P.Z_PLATE_IN, P.Z_FACE) - P.cyl(x, y, k["d"], P.SPLIT, P.Z_FACE + 1)
        return Compound([flange, head])
    fl = k["hole_clr"] + k["flange_lip"]
    flange = P.prism(_cross_face(fl + P.KEY_SWEEP_CLR), P.SPLIT, P.Z_PLATE_IN) - \
        P.prism(_cross_face(fl), P.SPLIT - 1, P.Z_FACE + 1)
    head = P.prism(dpad_hole_face(), P.Z_PLATE_IN, P.Z_FACE) - \
        P.prism(_cross_face(k["hole_clr"] / 2), P.SPLIT, P.Z_FACE + 1)
    return Compound([flange, head])


# ---------------------------------------------------------------- 4. LED light pipes
def _led_z():
    """(stop ring bottom, stop ring top) per LED."""
    out = {}
    for n, l in P.LEDS.items():
        z0 = l["top_z"] + LED_GAP
        out[n] = (round(z0, 6), round(z0 + LED_STOP_T, 6))
    return out


@functools.lru_cache(maxsize=None)
def led_tubes():
    out = []
    for n, l in P.LEDS.items():
        x, y = l["at"]
        z0, z1 = _led_z()[n]
        t = P.cyl(x, y, LED_BORE + 2 * LED_WALL, z0, P.Z_PLATE_IN + FUSE)
        t = t - P.cyl(x, y, LED_BORE, z1, P.Z_FACE + 1) - P.cyl(x, y, LED_STOP_ID, z0 - 1, z1 + 0.01)
        out.append(t)
    return out


def led_cuts():
    out = []
    for n, l in P.LEDS.items():
        x, y = l["at"]
        z0, z1 = _led_z()[n]
        out.append(P.cyl(x, y, LED_BORE, z1, P.Z_FACE + CUT))
        out.append(P.cyl(x, y, LED_STOP_ID, z0 - CUT, z1 + 0.01))
    return out


def led_pipes():
    return {f"LED pipe {n}": P.cyl(*l["at"], LED_ROD_D, _led_z()[n][1], P.Z_FACE) for n, l in P.LEDS.items()}


# ---------------------------------------------------------------- 5. bosses, merge blocks, posts
def wall_along(x, y, ux, uy):
    """distance from an inside point along the unit direction (ux, uy) to the walls' inner face (bisection on the
    rounded rectangle: inside <=> within the corner radius of its core rectangle)."""
    w, r = P.WALL_IN, P.OUTLINE_R - P.WALL
    c0x, c1x, c0y, c1y = w.x0 + r, w.x1 - r, w.y0 + r, w.y1 - r

    def inside(t):
        px, py = x + ux * t, y + uy * t
        return math.hypot(px - min(max(px, c0x), c1x), py - min(max(py, c0y), c1y)) <= r
    lo, hi = 0.0, 500.0
    for _ in range(60):
        mid = (lo + hi) / 2
        lo, hi = (mid, hi) if inside(mid) else (lo, mid)
    return lo


def boss_supports(name):
    """[(kind, p0, p1, width)] for one boss: a merge block into the nearest wall (gap < MERGE_MAX) or the wall along
    SUPPORT_DIR[name], else two webs."""
    b = P.BOSSES[name]
    x, y = b["at"]
    (wx, wy), dist, (ux, uy) = wall_nearest(x, y)
    gap = dist - P.BOSS_D / 2
    if gap <= 0:
        return []                                           # already in the wall (S1, S2)
    if name in SUPPORT_DIR:
        ux, uy = SUPPORT_DIR[name]
        d = wall_along(x, y, ux, uy)
        return [("merge", (x, y), (x + ux * (d + WALL_INTO + 0.5), y + uy * (d + WALL_INTO + 0.5)), P.BOSS_D)]
    far = (x + ux * (dist + WALL_INTO + 0.5), y + uy * (dist + WALL_INTO + 0.5))
    if gap < MERGE_MAX:
        return [("merge", (x, y), far, P.BOSS_D)]
    out = [("web", (x, y), far, WEB_T)]
    # the second web: the nearest point on another side of the wall
    w = P.WALL_IN
    sides = [((w.x0, y), (-1.0, 0.0)), ((w.x1, y), (1.0, 0.0)), ((x, w.y0), (0.0, -1.0)), ((x, w.y1), (0.0, 1.0))]
    sides = sorted((math.hypot(px - x, py - y), (px, py), u) for (px, py), u in sides if u != (ux, uy))
    d2, _, (vx, vy) = sides[0]
    out.append(("web", (x, y), (x + vx * (d2 + WALL_INTO + 0.5), y + vy * (d2 + WALL_INTO + 0.5)), WEB_T))
    return out


@functools.lru_cache(maxsize=None)
def boss(name):
    b = P.BOSSES[name]
    x, y = b["at"]
    z0 = P.SPLIT
    parts = [P.cyl(x, y, P.BOSS_D, z0, P.Z_PLATE_IN + FUSE)]
    zs = P.SPLIT + MERGE_GAP if b["on_board"] else P.SPLIT
    for kind, p0, p1, t in boss_supports(name):
        parts.append(_strip(p0, p1, t, zs, P.Z_PLATE_IN + FUSE) & _inside())
    return _fuse(parts) - insert_hole(name)


def insert_hole(name):
    x, y = P.BOSSES[name]["at"]
    d = P.INSERT["hole_d"]
    hole = P.cyl(x, y, d, P.SPLIT - CUT, P.SPLIT + P.INSERT["depth"])
    lead = Pos(x, y, P.SPLIT - CUT + (CUT + INSERT_LEAD) / 2) * Cone(d / 2 + INSERT_LEAD + CUT, d / 2,
                                                                       CUT + INSERT_LEAD)
    return hole + lead


def _posts_k():
    """plate posts, KiCad (x, y) (derived from the key positions; the self-test checks their clearances)."""
    K = {n: (k["at"][0], -k["at"][1]) for n, k in P.KEYS.items()}
    dx, dy = K["DPAD"]
    s = DPAD_POST_OFF
    out = {f"D-pad {a}": (dx + sx * s, dy + sy * s) for a, (sx, sy) in
           (("NW", (-1, -1)), ("NE", (1, -1)), ("SW", (-1, 1)), ("SE", (1, 1)))}
    sx, sy = K["START"]
    out.update({"START W": (sx - 6.0, sy), "START E": (sx + 6.0, sy)})
    abxy = [K[n] for n in ("A", "B", "X", "Y")]
    out["ABXY centre"] = (sum(p[0] for p in abxy) / 4, sum(p[1] for p in abxy) / 4)
    (xx, xy), (lx, ly) = K["X"], K["SELECT"]
    out["X-SELECT"] = ((xx + lx) / 2, (xy + ly) / 2)
    out["SELECT E"] = (lx + 6.0, ly)
    out["SELECT SW"] = (lx - 5.7, ly + 2.5)                   # CN1's corner is 1.6 from it
    return out


POSTS = tuple(_posts_k().items())


@functools.lru_cache(maxsize=None)
def posts():
    return [P.cyl(*_k(*xy), POST_D, P.SPLIT + POST_GAP, P.Z_PLATE_IN + FUSE) for _, xy in POSTS]


# ---------------------------------------------------------------- 6. jack, notches
def jack_cuts():
    """(front cuts, back cuts) for the jack: the U-slot + spot-face in the front rim, the hole + spot-face below the
    split in the back rim."""
    j = P.JACK
    x, az, ty = j["x"], j["axis_z"], j["tip_y"]
    y_in = P.WALL_IN.y0 + JACK_IN
    y_out = P.OUTLINE.y0 - CUT
    hole = _cyl_y(x, az, j["hole_d"], y_out, y_in)
    slot = P.box(x, (y_in + y_out) / 2, j["hole_d"], y_in - y_out, P.SPLIT - CUT, az)
    spot = _cyl_y(x, az, j["shoulder_d"] + 2 * JACK_SPOT_CLR, y_out, ty + JACK_SPOT_DEEPER)
    return [hole, slot, spot], [hole, spot]


def notch_cut(e):
    (ox, oy), (ux, uy) = e["outer"], e["dir"]
    d0, d1 = -CUT, P.WALL + NOTCH_IN                          # inward distances from the outer surface
    cx, cy = ox - ux * (d0 + d1) / 2, oy - uy * (d0 + d1) / 2
    L, w = d1 - d0, e["notch_w"]
    z0, z1 = e["notch_z"]
    sx, sy = (L, w) if abs(ux) > abs(uy) else (w, L)
    return P.box(cx, cy, sx, sy, z0 - CUT, z1)


def notch_cuts():
    return [notch_cut(e) for e in list(P.EDGE_KEYS.values()) + list(P.SHOULDERS.values())]


# ---------------------------------------------------------------- 7. keep-outs
@functools.lru_cache(maxsize=None)
def keepouts():
    ko = {}
    ko["panel"] = _fuse([PK.prism(P.LEDGE_Z1, Z_POCKET_TOP), tab_relief()])
    x0, y0, x1 = DISP_KO
    ko["display flex"] = P.Rect.k(x0, y0, x1, -P.PANEL["lcm"].y0).prism(P.SPLIT, P.Z_PANEL_LCM[0])
    t = P.FLEX["touch"]
    pieces = [C.strip(t["path"], s0, min(s1, t["path"].s - P.HDGC_INSERT), (xa - FLEX_KO_W, xb + FLEX_KO_W),
                      t["t"] + 2 * FLEX_KO_T) for s0, s1, (xa, xb) in t["widths"]]
    ya, yb = t["ic_y"]
    pieces.append(P.box((t["ic_x"][0] + t["ic_x"][1]) / 2, -(ya + yb) / 2, t["ic_x"][1] - t["ic_x"][0] + 2 * FLEX_KO_W,
                        yb - ya + 2 * FLEX_KO_W, P.Z_BOARD_BOT - t["ic_t"] - FLEX_KO_T, P.Z_BOARD_BOT))
    ko["touch flex"] = Compound(pieces)
    fb = P.FOLD_BAY["rect"]
    ko["fold bay"] = P.Rect(fb.x0, fb.y0, fb.x1, -NOTCH_EDGE_K).prism(P.FOLD_BAY["z"][0], P.FOLD_BAY["z"][1])
    for n in P.KEYS:
        ko[f"key {n}"] = key_keepout(n)
    for n, s in ko.items():
        s.label = f"front:{n}"
    return ko


# ---------------------------------------------------------------- the module result
def adds():
    out = [frame()]
    if ledge() is not None:
        out.append(ledge())
    out += led_tubes()
    out += [boss(n) for n in P.BOSSES]
    out += posts()
    return out


def cuts():
    out = [pocket_cut(), tab_relief()]
    if LIP:
        out.append(lip_window_cut())
    out += [c for c in (fold_skin_cut(), cn1_cut()) if c is not None]
    out += key_hole_cuts()
    out += led_cuts()
    out += [insert_hole(n) for n in P.BOSSES]
    out += jack_cuts()[0]
    out += notch_cuts()
    return out


def parts():
    out = dict(led_pipes())
    if ledge_tape() is not None:
        out["ledge tape"] = ledge_tape()
    if lip_gasket() is not None:
        out["lip gasket"] = lip_gasket()
    return out


COLOURS = {"LED pipe U5": "#bfe3ff", "LED pipe U14": "#bfe3ff", "ledge tape": "#e8e8e8", "lip gasket": "#202020"}
BOUGHT = ("LED pipe U5", "LED pipe U14", "ledge tape", "lip gasket")


def print_pose(name, solid):
    """every front.py part is bought (acrylic rods, die-cut tape / gasket): STEP only."""
    return None if name in BOUGHT else solid


def _dir_label(s):
    (x0, y0), (x1, y1) = s[1], s[2]
    dx, dy = x1 - x0, y1 - y0
    return ("+x" if dx > 0 else "-x") if abs(dx) > abs(dy) else ("top" if dy > 0 else "bottom")


E_PA12 = 1800.0             # MPa, MJF PA12-HP (thin report / JLC): the skin estimate in notes()
SWING_STEP = 2              # deg: the self-test's flush swing-in sweep


def dpad_post_margin():
    """the D-pad diagonal posts' gap to the D-pad flange zone (flange outline + KEY_SWEEP_CLR) at its inner corner
    arm edge."""
    k = P.KEYS["DPAD"]
    return DPAD_POST_OFF - POST_D / 2 - (k["w"] / 2 + k["hole_clr"] + k["flange_lip"] + P.KEY_SWEEP_CLR)


def flex_roots_standing():
    """(display, touch) flex roots' KiCad y with the module standing on its glass bottom edge (the flush swing-in):
    the glass edge less each root's depth under the glass top."""
    g = -P.PANEL["glass"].y0
    top = P.Z_PANEL_GLASS[1]
    return g - (top - (P.Z_PANEL_LCM[0] + P.DISP_EXIT_DZ)), g - P.TOUCH_EXIT_BELOW


def swing_in(front, step=None):
    """flush: the module (LCM + touch + glass) swung 0..90 deg about its glass bottom edge: [(deg, overlap mm3 with
    the front shell)] (the ledge tape it lifts off is left out)."""
    from build123d import Axis
    pn = C.panel_bodies()
    mod = pn["panel LCM"] + pn["panel touch"] + pn["panel glass"]
    ax = Axis((0, P.PANEL["glass"].y0, P.Z_PANEL_GLASS[1]), (1, 0, 0))
    out = []
    for a in range(0, 91, step or SWING_STEP):
        m = mod.rotate(ax, a)
        r = m & front
        out.append((a, sum(s.volume for s in r.solids()) if r is not None else 0.0))
    return out


def skin_estimate(F=10.0, b=10.0):
    """(deflection mm, stress MPa) of the fold-bay skin as a cantilever from its full-thickness root (FOLD_BAY y0) to
    the free pocket edge, F N spread over b mm (hand calculation; plate action spreads it ~1-1.5x wider)."""
    L = PK.y0 - P.FOLD_BAY["rect"].y0
    t = P.Z_FACE - max(P.Z_PLATE_IN, P.Z_FOLD_SKIN_IN)
    return 4 * F * L ** 3 / (E_PA12 * b * t ** 3), 6 * F * L / (b * t ** 2)


def notes():
    k = P.KEYS
    d = k["DPAD"]
    e, s = P.EDGE_KEYS["VOL_UP"], P.SHOULDERS["L"]
    j = P.JACK
    skin_t = P.Z_FACE - max(P.Z_PLATE_IN, P.Z_FOLD_SKIN_IN)
    n = [f"[{P.VARIANT}] pocket {PK.w:.1f} x {PK.h:.1f} through the face"
         + (f", lip window {P.LIP_WINDOW.w:.1f} x {P.LIP_WINDOW.h:.1f} (R{LIP_WINDOW_R}, {LIP_CHAMFER} x 45 at the "
            f"face) through a {P.LIP:.1f} lip; no ledge" if LIP else
            f"; ledge z {P.LEDGE_Z0:.2f}..{P.LEDGE_Z1:.2f} top / left / bottom-left (C20 / C94 relief to z "
            f"{ledge_reliefs()[1]:.2f}) + ledge tape") +
         f"; frame walls {FRAME_T} ({FRAME_T_RIGHT} right) round the pocket, down to z {cell_frame_z0():.2f} over the "
         f"cell; fold-bay skin {skin_t:.2f}" + ("" if fold_skin_cut() is not None else " (full plate, no cut)") +
         "; CN1 plate " + (f"{P.PLATE_CN1} (inner z {P.CN1['plate_in_z']:.2f})" if cn1_needs_thinning() else
                           f"full ({P.Z_PLATE_IN - P.CN1['top_z']:.2f} over CN1)") +
         f"; LED light pipes D{LED_ROD_D} x {P.Z_FACE - _led_z()['U5'][1]:.2f} (bought) over U5 / U14; bosses "
         f"D{P.BOSS_D} + M2 insert holes at {', '.join(P.BOSSES)}; jack U-slot D{j['hole_d']} + spot-face "
         f"D{j['shoulder_d'] + 2 * JACK_SPOT_CLR:.1f} (also cut from the back rim); SKRT / shoulder notches"]
    n.append("for controls: face holes D" + ", ".join(f"{k[x]['hole_d']:g} {x}" for x in
                                                       ("A", "B", "X", "Y", "START", "SELECT")) +
             f" at KEYS[n]['at'] (plate z {P.Z_PLATE_IN:.2f}..{P.Z_FACE:.2f}); D-pad hole = key_hole_face('DPAD') "
             f"with its 8 arm-end corners R{DPAD_HOLE_R}; front keeps clear_d / the D-pad flange + "
             f"{P.KEY_SWEEP_CLR} free from z {P.SPLIT:.2f} to the plate (nearest: the D-pad diagonal posts, "
             f"{dpad_post_margin():.1f} outside it); keep-outs 'front:key <n>' = the clearance rings round each cap "
             f"(flange_d..clear_d, d..hole_d). Edge notches {e['notch_w']:g} x z {e['notch_z'][0]:.2f}.."
             f"{e['notch_z'][1]:.2f} (right wall), shoulders {s['notch_w']:g} (top wall): plain slots, the wall's "
             f"inner face is the outward stop. "
             f"H3 now merges into the RIGHT wall: the top wall is free from H2's block (KiCad x "
             f"{P.BOSSES['H2']['at'][0] + P.BOSS_D / 2:.2f}) to the corner arc (x {P.OUTLINE.x1 - P.OUTLINE_R:.1f}), "
             f"so the R yoke can grow past x {P.BOSSES['H3']['at'][0] - P.BOSS_D / 2:.2f}")
    n.append(f"for controls / back: plate posts D{POST_D:g} ending {POST_GAP} over the board (the board backs the "
             f"plate beside the keys) at KiCad " + ", ".join(f"{nm} ({x:.1f}, {y:.1f})" for nm, (x, y) in POSTS) +
             "; boss gussets (merge blocks, BOSS_D wide, " + f"{MERGE_GAP} over the board): " +
             ", ".join(f"{b} {_dir_label(q)}" for b in P.BOSSES for q in boss_supports(b)[:1]))
    if LIP:
        n.append("assembly (lip): the module goes in glass-first from inside the face-down shell onto the lip "
                 "gasket (tape), its flexes already plugged (panel + board as a book); foam pads on bare board "
                 "behind the LCM hold it; nothing of front.py stops it from below")
    else:
        rd, rt = flex_roots_standing()
        n.append(f"assembly (flush): stand the module on its glass bottom edge in the pocket (glass toward the bottom "
                 f"wall), flexes hanging into the cavity (their roots stand at KiCad y {rd:.1f} / {rt:.1f}, inside the "
                 f"pocket edge {-PK.y0:.2f}), and swing it flat about that edge onto the ledge tape (the self-test "
                 f"sweeps it every {SWING_STEP} deg). Fed straight past the bezel the flexes would not fit "
                 f"({P.PANEL['lcm'].y0 - PK.y0:.2f} / {P.PANEL['glass'].y0 - PK.y0:.2f} from the LCM / glass edge "
                 f"to the pocket)")
    dl, sl = skin_estimate()
    flex_air = P.Z_FOLD_SKIN_IN - (P.FLEX["touch"]["exit_z"] + P.FLEX_T / 2 + P.Z_GLASS_MAX - P.Z_PANEL_GLASS[1])
    fx0, fx1 = P.FLEX["touch"]["widths"][0][2][0] - FLEX_KO_W, P.FLEX["touch"]["widths"][0][2][1] + FLEX_KO_W
    if fold_skin_cut() is not None:
        n.append(f"open: the {skin_t:.2f} fold-bay skin is a {PK.y0 - P.FOLD_BAY['rect'].y0:.2f} cantilever "
                 f"{P.FOLD_BAY['rect'].w:.1f} long with a free edge at the glass: ~{dl:.2f} mm / {sl:.0f} MPa at "
                 f"10 N on 10 mm (hand calc, PA12 48 MPa) and it reaches the touch flex after {flex_air:.2f}"
                 f"; narrowing the core 'fold bay' top band to the touch funnel (KiCad x {fx0:.1f}-{fx1:.1f}) would "
                 f"shorten it to {fx1 - fx0:.0f} mm; the lip variant has a full plate there")
    if P.LCD_PLATE:
        n.append("open (LCD_PLATE=1): nothing holds the SUS plate (its outline is the cell buffer, mostly under the "
                 "LCM); proposal: bond its right / bottom edges under front's frame walls over the cell, lowered to "
                 "the plate top (they stop at z %.2f today), plus gb2-rev-B joggled tabs onto the board's top strip "
                 "along its top edge" % cell_frame_z0())
    if P.PROCESS == "fdm":
        n.append("FDM: " + ("" if LIP else f"the ledge ({P.LEDGE['segments']['left'].w:.2f} into the pocket) and ") +
                 "the LED stop rings overhang in the face-down print: support them" +
                 ("" if LIP else " (or a 45 deg chamfer under the ledge)"))
    n.append(f"posts and merge blocks assume a flat board: a board bowed more than {POST_GAP} toward the face between "
             f"bosses touches the posts (harmless); FR4 bow at IPC 0.75 % is 0.3 over 40 mm")
    return n


def build(ctx):
    return {"front_add": adds(), "front_cut": cuts(), "back_cut": jack_cuts()[1], "parts": parts(),
            "keepouts": keepouts(), "notes": notes()}


# ================================================================ self-test (python front.py [--no-render])
def _probe_z(shape, x, y, z0=None, z1=None, s=0.02):
    """material intervals [(z0, z1)] of shape on the vertical line at enclosure (x, y)."""
    z0 = P.Z_BACK_MIN - 1 if z0 is None else z0
    z1 = P.Z_FACE + 1 if z1 is None else z1
    r = shape & P.box(x, y, s, s, z0, z1)
    if r is None:
        return []
    return sorted((round(q.bounding_box().min.Z, 3), round(q.bounding_box().max.Z, 3)) for q in r.solids())


def _probe_h(shape, axis, a0, a1, other, z, s=0.02):
    """material intervals along a horizontal line: axis 'x' (x a0..a1 at y = other) or 'y' (y a0..a1 at x = other)."""
    if axis == "x":
        b = P.box((a0 + a1) / 2, other, abs(a1 - a0), s, z - s / 2, z + s / 2)
        k = 0
    else:
        b = P.box(other, (a0 + a1) / 2, s, abs(a1 - a0), z - s / 2, z + s / 2)
        k = 1
    r = shape & b
    out = []
    for q in (r.solids() if r is not None else []):
        bb = q.bounding_box()
        out.append((round(tuple(bb.min)[k], 3), round(tuple(bb.max)[k], 3)))
    return sorted(out)


def selftest(render=True):
    import sys, time
    import enclosure, checks
    t0 = time.time()
    ctx = enclosure.make_ctx()
    r = enclosure._validate("front", build(ctx))
    t_build = time.time() - t0
    front = ctx.front.fuse(*r["front_add"]).clean().cut(*r["front_cut"]).clean()
    back = ctx.back.cut(*r["back_cut"]).clean()
    front.label, back.label = "front", "back"
    t_apply = time.time() - t0 - t_build
    print(f"front.py self-test [{P.VARIANT}]: build {t_build:.1f} s, apply {t_apply:.1f} s; "
          f"{len(r['front_add'])} adds, {len(r['front_cut'])} cuts, {len(r['back_cut'])} back cuts")
    bad = []

    def check(label, ok, detail=""):
        print(f"  {'ok  ' if ok else 'FAIL'} {label}" + (f": {detail}" if detail else ""))
        if not ok:
            bad.append(label)

    # 0. solids
    for n, s in (("front", front), ("back", back)):
        check(f"{n} shell: one valid solid", len(s.solids()) == 1 and s.is_valid,
              f"{len(s.solids())} solid(s), valid {s.is_valid}, {s.volume:.0f} mm3")
    for n, p in r["parts"].items():
        check(f"part {n}: one valid solid", len(p.solids()) == 1 and p.is_valid, f"{p.volume:.1f} mm3")

    # 1. the front shell vs every context body (none), the back shell's jack corner
    bodies = {n: s for n, (s, _) in ctx.context.items()}
    ov = checks.overlaps(front, bodies)
    check("front shell vs the context bodies", not ov, ", ".join(f"{n} {v:.3f}" for n, v in sorted(ov.items()))
          or f"{len(bodies)} bodies clear")
    vj = checks.vol(back, bodies["jack"])
    check("back shell vs the jack", vj <= checks.EPS_V, f"{vj:.4f}")
    # 2. shells vs the core keep-outs (usb_c plug: back.py cuts its relief from the front rim; microsd / reset / cell:
    #    back.py's)
    theirs = {"usb_c plug", "microsd card", "reset pin"}
    for kn, k in ctx.core_keepouts.items():
        vf, vb = checks.vol(front, k), checks.vol(back, k)
        if kn in theirs:
            print(f"  info core:{kn}: front {vf:.3f}, back {vb:.3f} mm3 (back.py's)")
            continue
        okb = vb <= checks.EPS_V or kn not in ("jack plug", "fold bay")
        check(f"core:{kn}", vf <= checks.EPS_V and okb, f"front {vf:.4f}, back {vb:.4f}")
    # 3. my adds / parts vs the core keep-outs
    mats = [(f"add[{i}]", s) for i, s in enumerate(r["front_add"])] + list(r["parts"].items())
    hits = [f"{mn} in {kn} {checks.vol(s, k):.3f}" for mn, s in mats for kn, k in ctx.core_keepouts.items()
            if checks.vol(s, k) > checks.EPS_V]
    check("adds + parts vs the core keep-outs", not hits, "; ".join(hits))
    # 4. parts vs the context and the finished shells (contacts only)
    for n, p in r["parts"].items():
        o = checks.overlaps(p, dict(bodies, front=front, back=back))
        check(f"part {n} vs the context + shells", not o, ", ".join(f"{k} {v:.4f}" for k, v in o.items()))
    # 5. key columns clear (round: D clear_d, D-pad: flange + KEY_SWEEP_CLR, SPLIT..plate) and the face holes open
    for n in P.KEYS:
        v = checks.vol(front, key_clear_zone(n))
        hv = checks.vol(front, P.prism(hole_face(n), P.Z_PLATE_IN - 0.01, P.Z_FACE + 0.01))
        check(f"key {n}: clear zone + face hole", v <= checks.EPS_V and hv <= checks.EPS_V,
              f"zone {v:.4f}, hole {hv:.4f}")
    # 6. my keep-outs vs the finished front shell (the other modules get exactly these)
    for kn, k in r["keepouts"].items():
        v = checks.vol(front, k)
        check(f"keep-out front:{kn} vs the finished front shell", v <= checks.EPS_V, f"{v:.4f}")
    # 7. clearances of my adds to the context (distance; the boss tips sit on the board by design)
    groups = {"frame": [frame()], "ledge": [ledge()] if ledge() is not None else [], "LED tubes": led_tubes(),
              "bosses": [boss(n) for n in P.BOSSES], "posts": posts()}
    for g, shapes in groups.items():
        near = []
        for s in shapes:
            for bn, b in bodies.items():
                if checks._bb_hit(s, b, 1.0):
                    near.append((s.distance_to(b), bn))
        near.sort()
        intended = {"bosses": ("board",)}
        worst = [(d, bn) for d, bn in near if bn not in intended.get(g, ())]
        lim = 0.2 if g in ("posts",) else 0.15 if g == "ledge" else 0.3 - 1e-6
        mn = worst[0][0] if worst else None
        check(f"{g}: nearest context body >= {lim}", mn is None or mn >= lim - 1e-6,
              ", ".join(f"{bn} {d:.3f}" for d, bn in near[:4]) or "none within 1.0")
    # 8. posts: lateral clearance to every F part and key zone
    fparts = {n: s for n, s in bodies.items() if n.startswith("F:") or n in ("jack",)}
    for (nm, xy), s in zip(POSTS, posts()):
        dp = min((s.distance_to(b), bn) for bn, b in fparts.items() if checks._bb_hit(s, b, 3.0)) \
            if any(checks._bb_hit(s, b, 3.0) for b in fparts.values()) else (9.9, "-")
        dk = min(s.distance_to(key_clear_zone(k)) for k in P.KEYS)
        x, y = _k(*xy)
        under = _probe_z(bodies["board"], x, y, -1, 3)
        check(f"post {nm}", dp[0] >= POST_CLR - 1e-6 and dk >= POST_CLR - 1e-6 and under == [(0.0, P.BOARD_TOP)],
              f"F part {dp[1]} {dp[0]:.2f}, key zone {dk:.2f}, board under it {under}")
    # 9. thickness probes (material intervals along z / horizontal lines)
    fb = P.FOLD_BAY["rect"]
    probes = [
        ("plate, top strip", _probe_z(front, 100.0, -33.5), [(P.Z_PLATE_IN, P.Z_FACE)]),
        ("fold-bay skin", _probe_z(front, fb.cx, fb.cy - 1.0), [(max(P.Z_PLATE_IN, P.Z_FOLD_SKIN_IN), P.Z_FACE)]),
        ("plate over CN1", _probe_z(front, P.CN1["rect"].cx, P.CN1["rect"].cy),
         [(P.CN1["plate_in_z"] if cn1_needs_thinning() else P.Z_PLATE_IN, P.Z_FACE)]),
    ]
    if not LIP:
        rl = P.LEDGE["relief"]["C20"]["rect"]
        probes += [("ledge, plain (left)", _probe_z(front, 57.5, -60.0, 0, 9), [(P.LEDGE_Z0, P.LEDGE_Z1)]),
                   ("ledge over C20 (relief)", _probe_z(front, 57.6, rl.cy, 0, 9),
                    [(round(ledge_reliefs()[1], 3), P.LEDGE_Z1)]),
                   ("ledge, top", _probe_z(front, 120.0, -37.5, 0, 9), [(P.LEDGE_Z0, P.LEDGE_Z1)])]
    for lab, got, want in probes:
        ok = len(got) == len(want) and all(abs(a - c) < 0.01 and abs(b - d) < 0.01 for (a, b), (c, d) in zip(got, want))
        check(f"probe {lab}", ok, f"{got} (want {[(round(a, 3), round(b, 3)) for a, b in want]})")
    zt = (P.Z_LCM_BACK + P.Z_PLATE_IN) / 2
    tr = _probe_h(front, "x", PK.x1 - 2, PK.x1 + 4, _k(0, sum(TAB_RELIEF["y_k"]) / 2)[1], zt)
    check(f"right frame wall behind the tab relief >= {FRAME_T}",
          len(tr) == 1 and tr[0][1] - tr[0][0] >= FRAME_T - 1e-3, f"x {tr}")
    tw = _probe_h(front, "y", PK.y1 - 2, PK.y1 + 3, 120.0, zt)
    check("top frame wall 1.0", any(abs((b - a) - FRAME_T) < 0.01 for a, b in tw), f"y {tw}")
    # 10. panel fit: the pocket over the LCM max / the glass, per side
    pm, gl = P.PANEL["max"], P.PANEL["glass"]
    gaps = (pm.x0 - PK.x0, PK.x1 - pm.x1, pm.y0 - PK.y0, PK.y1 - pm.y1)
    ggaps = (gl.x0 - PK.x0, PK.x1 - gl.x1, gl.y0 - PK.y0, PK.y1 - gl.y1)
    check("pocket over the LCM max outline >= 0.2 per side", min(gaps) >= 0.2 - 1e-6,
          f"{', '.join(f'{g:.2f}' for g in gaps)}; glass gaps {', '.join(f'{g:.2f}' for g in ggaps)}")
    # 11. jack: barrel in the hole, the plug keep-out clear in both shells
    j = P.JACK
    check("jack hole round the barrel", (j["hole_d"] - j["barrel_d"]) / 2 >= 0.2 - 1e-6,
          f"{(j['hole_d'] - j['barrel_d']) / 2:.2f} radial; spot-face floor {JACK_SPOT_DEEPER} behind the tip")
    # 12. notches open (no front material in the notch box: outer surface to the wall's inner face)
    for n, e in list(P.EDGE_KEYS.items()) + list(P.SHOULDERS.items()):
        (ox, oy), (ux, uy) = e["outer"], e["dir"]
        L = float(P.WALL)                                  # (an Input: build123d cannot deepcopy it)
        cx, cy = ox - ux * L / 2, oy - uy * L / 2
        sx, sy = (L, e["notch_w"]) if abs(ux) > abs(uy) else (e["notch_w"], L)
        v = checks.vol(front, P.box(cx, cy, sx, sy, *e["notch_z"]))
        check(f"notch {n} open", v <= checks.EPS_V, f"{v:.4f}")
    # 13. bosses: tips at SPLIT on the board, insert hole open, supports
    for n, b in P.BOSSES.items():
        x, y = b["at"]
        zs = _probe_z(front, x + P.BOSS_D / 2 - 0.4, y, 0, 9)
        hole = _probe_z(front, x, y, 0, 9)
        sup = ", ".join(k for k, *_ in boss_supports(n)) or "in the wall"
        check(f"boss {n}", zs and abs(zs[0][0] - P.SPLIT) < 1e-3 and hole and abs(hole[0][0] -
                                                                               (P.SPLIT + P.INSERT['depth'])) < 1e-3,
              f"ring from z {zs[0][0] if zs else None}, insert hole to {hole[0][0] if hole else None}, {sup}, "
              f"wall gap {wall_nearest(x, y)[1] - P.BOSS_D / 2:+.2f}")
    # 14. panel insertion: flush - the swing-in about the glass bottom edge; lip - straight up from the split into the
    #     pocket against the lip (the column under its seat clear of the front shell)
    if not LIP:
        sw = swing_in(front)
        worst = max(sw, key=lambda t: t[1])
        rd, rt = flex_roots_standing()
        check("panel swing-in (flush) clear", worst[1] <= checks.EPS_V and max(rd, rt) < -PK.y0,
              f"worst {worst[1]:.4f} mm3 at {worst[0]} deg ({len(sw)} steps); flex roots standing at KiCad y "
              f"{rd:.2f} / {rt:.2f} < the pocket edge {-PK.y0:.2f}")
    else:
        col = P.PANEL["max"].prism(P.SPLIT, P.Z_PANEL0)
        v = checks.vol(front, col)
        check("panel inserted from inside (lip): its column from the split clear", v <= checks.EPS_V, f"{v:.4f}")
    print(f"{len(bad)} FAIL" + (": " + "; ".join(bad) if bad else "") + f" ({time.time() - t0:.1f} s)")
    if render:
        _renders(front, back, r, ctx)
    print(f"done in {time.time() - t0:.1f} s")
    return front, back, r, ctx, bad


def _renders(front, back, r, ctx):
    import os, time
    import render as R
    t0 = time.time()
    default = {"FRONT": "flush", "BACK": "flat", "CELL": "LP803860", "LCD_PLATE": False, "PROCESS": "mjf"}
    slug = "_".join(f"{k.lower()}-{v}" for k, v in P.FLAGS.items() if v != default[k])
    out = os.path.join(P.HERE, "build", "front", slug) if slug else os.path.join(P.HERE, "build", "front")
    os.makedirs(out, exist_ok=True)
    tag = P.VARIANT
    fc = "#e9e2c6"
    mine = [(n, p, COLOURS.get(n, "#888888"), 1.0) for n, p in r["parts"].items()]
    R.view([("front", front, fc, 1.0)] + mine, os.path.join(out, "front_inside.png"), az=90, el=-90, width=2200,
           height=1080, title=f"front shell from inside (mirrored: KiCad +x to the left) - {tag}")
    R.view([("front", front, fc, 1.0)] + mine, os.path.join(out, "front_iso_inside.png"), az=-120, el=-38,
           width=2200, height=1300, title=f"front shell, inside (iso from below) - {tag}")
    R.view([("front", front, fc, 1.0)] + mine, os.path.join(out, "front_iso_inside_2.png"), az=60, el=-38,
           width=2200, height=1300, title=f"front shell, inside (iso from below, other side) - {tag}")
    ctxb = [(n, s, col, 1.0) for n, (s, col) in ctx.context.items()]
    R.view([("front", front, fc, 1.0)] + mine + [it for it in ctxb if it[0].startswith("panel")],
           os.path.join(out, "front_face.png"), az=-90, el=90, width=2200, height=1080,
           title=f"front face with the panel - {tag}")
    R.view([("front", front, fc, 0.35)] + mine + ctxb, os.path.join(out, "front_iso_context.png"), az=-62, el=30,
           width=2200, height=1300, title=f"front shell (alpha 0.35) over the context - {tag}")
    sec = [("front", front, fc, 1.0), ("back", back, "#b9bec4", 1.0)] + mine + ctxb
    zl = [(P.Z_FACE, "face"), (P.Z_GLASS_MAX, "glass max"), (P.Z_PLATE_IN, "plate inner"),
          (P.Z_FOLD_SKIN_IN, "fold skin"), (P.Z_LCM_BACK, "LCM back"), (P.LEDGE_Z0, "ledge"), (P.SPLIT, "split"),
          (P.Z_BOARD_BOT, "board bottom")]
    S = [("x", 110.0, [(29.0, 41.0), (104.0, 120.0)], "KiCad x 110: top strip frame + ledge, fold-bay skin"),
         ("x", P.JACK["x"], [(29.0, 42.0), (44.0, 56.0), (106.0, 121.0)], "KiCad x 37.5: L notch, START, jack"),
         ("x", P.LEDS["U5"]["at"][0], [(29.0, 40.0)], "KiCad x 125.4: U5 light pipe"),
         ("x", P.CN1["rect"].cx, [(29.0, 56.0)], "KiCad x 186.7: CN1 thinning, H2 merge"),
         ("x", 57.6, [(76.0, 88.0), (29.0, 42.0), (106.0, 120.0)], "KiCad x 57.6: left ledge over C20 / C94"),
         ("y", -66.9, [(20.0, 52.0), (176.0, 215.0)], "KiCad y 66.9: D-pad NW / NE posts, X"),
         ("y", -43.4, [(170.0, 215.0)], "KiCad y 43.4: tab relief, right frame wall, VOL+ region"),
         ("y", -116.3, [(126.0, 146.0), (28.0, 46.0)], "KiCad y 116.3: S1 seam boss, jack"),
         ("y", -58.0, [(19.0, 34.0)], "KiCad y 58: H1 merge")]
    for i, (ax, at, zooms, title) in enumerate(S):
        fn = os.path.join(out, f"section_{i}_{ax}{abs(at):.1f}.png")
        try:
            R.section(sec, fn, ax, at, zlines=zl, zooms=zooms, title=f"{title} - {tag}")
        except Exception as ex:
            print(f"  section {ax} {at}: {ex}")
    print(f"renders in build/front/ ({time.time() - t0:.0f} s)")


if __name__ == "__main__":
    import sys
    _f, _b, _r, _c, _bad = selftest(render="--no-render" not in sys.argv)
    sys.exit(1 if _bad else 0)
