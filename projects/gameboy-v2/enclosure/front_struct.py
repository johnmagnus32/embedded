"""front_struct.py — the front shell's structure (DESIGN.md section 3 Screw bosses / J1 hatch, section 2 Board location):
Screw bosses: Ø BOSS_D columns from the plate down to the board top (z SPLIT) at the 4 SHELL_SCREWS, a heat-set insert
hole INSERT_HOLE_D x INSERT_DEPTH up from each tip. The top two and the off-board bottom-right one are merged into the
wall (they sit < 0.5 from its inner face); the bottom-left one (3 mm off) gets two webs to the wall. The top-right one
is D-cut on its J1 side.
J1 hatch: J1_HATCH is the clear column through the plate (the 1x5 Dupont housing, ~12.7 x 2.5, goes down it onto J1).
The cover sits flush in a face pocket J1_HATCH + HATCH_LEDGE per side, on the 0.8 ledge of plate left round the
opening; two snap tabs hang from it down the opening and catch under two short walls (chimney) under the plate.
A fingernail notch at the west end of the pocket gets under the cover's end; a pad under the plate there keeps its
floor 0.8 thick (rev 2.1: it was one 0.2 layer).
Board locators: six fingers (BOARD_LOCATORS, FINGER) from z FINGER.z0 to SPLIT in the gap round the board edge,
continuing up the wall's inner face as a rib to RIB_Z1, 45-deg lead-in on the tip's inner edge.
No ribs under the plate (nothing found that is clearly worth the risk; see the self-test).
API (front_features): adds(), cuts(), keepouts(), j1_cover().
`.venv/bin/python front_struct.py` runs the self-test against shell.shells() and the context geometry."""
import math
from build123d import Axis, Plane, Pos, extrude, fillet, offset
from params import (SHELL_SCREWS, BOSS_D, INSERT_HOLE_D, INSERT_DEPTH, SILHOUETTE, WALL, SPLIT, Z_FRONT,
                    Z_PLATE_IN, J1_HATCH, PARTS, BOARD_LOCATORS, FINGER, box, cyl, face, prism)

# ---------------------------------------------------------------- choices (this module only)
FUSE = 0.2                  # adds reach this far into the plate / wall so the union fuses solidly
CUT = 0.5                   # cut tools overshoot the faces they go through
WALL_INTO = 1.0             # merges / webs run this far into the 2.0 wall (outer fillet R2 is <= 0.1 deep at z 19.5)
MERGE_Z0 = SPLIT + 0.3      # merge blocks / webs stop 0.3 above the board: only the boss ring clamps it ...
SKIRT_CLR = 0.2             # ... except beyond the board edge + this, where they run down to SPLIT (rev 2.1: the gap
                            # under them met the sloped corner walls nearly tangent at TL / TR, 0.02 slivers)
WEB_T = 1.2                 # bottom-left boss webs: 3 FDM lines
WEB_Z0 = 7.0                # webs over the board start here (grip parts <= 4.95, = GUIDE_BOTTOM_Z)
J1_TOP = SPLIT + PARTS["J1"]["height"]          # 10.1: J1 pin tips (8.5 header, board.json estimate)
D_CUT_X = J1_HATCH["cx"] + J1_HATCH["w"] / 2 + 0.2   # 130.5: top-right boss flat; J1 courtyard ends 129.925 (0.575
                                                     # off), the hatch column 130.3 (0.2), insert-hole wall 1.09

# J1 hatch (all in the assembly frame)
HATCH_LEDGE = 0.8           # ledge width round the clear opening = the face pocket's overhang past J1_HATCH
COVER_T = 0.8               # cover thickness (>= 0.8): pocket depth, so the ledge is PLATE - 0.8 = 0.8 thick (rev 2.1:
                            # 1.0 left a 0.6 x 0.6 ledge, backed by the chimneys along only 6.0 of each 13.4 side)
COVER_CLR = 0.1             # cover in its pocket, per side
POCKET_R = 0.5              # pocket corner radius (cover: POCKET_R - COVER_CLR)
LEDGE_Z = Z_FRONT - COVER_T                     # 19.9: the cover sits here
TAB_W, TAB_T = 4.0, 0.8     # snap tabs (one per long side, centred): width along x, thickness
TAB_CLR = 0.1               # tab's outer face inside the opening's edge
CHIMNEY_W, CHIMNEY_T = 6.0, 1.2                 # short walls under the plate outside each long edge, x-length, y-thickness
CATCH_Z = 16.3              # chimney bottom = the tabs' catch line (tab flex length 20.1 - 16.3 = 3.8)
BARB = 0.3                  # barb out of the tab face: 0.2 past the opening edge; strain 1.5*0.8*0.2/3.8^2 = 1.7 %
BARB_PLAY = 0.05            # barb's upper 45-deg ramp meets the chimney corner this far below seated (light preload)
NOTCH = {"w": 4.0, "out": 1.0, "z0": Z_PLATE_IN + 0.2}  # fingernail notch at the pocket's west end: across y, into the
                                                        # rim past the pocket, floor (under the cover's end, 0.2 plate)
NOTCH_PAD = {"t": 0.8, "margin": 0.5}                   # pad under the plate below the notch: its floor is t thick, the
                                                        # pad margin past the notch on the west / y sides (not into J1)

# board locators
WALL_LINE = 0.5             # the wall's inner face is this far outside the board edge (along n)
RIB_Z1 = 3.0                # finger continues up the wall's inner face as a rib to here


# ---------------------------------------------------------------- helpers
def _rbox(cx, cy, w, h, z0, z1, r=0.0):
    b = box(cx, cy, w, h, z0, z1)
    return fillet(b.edges().filter_by(Axis.Z), r) if r > 0 else b


def _strip(p0, p1, t, z0, z1):
    """a web: XY rectangle from p0 to p1 (centre line), t thick, z0..z1."""
    (x0, y0), (x1, y1) = p0, p1
    L = math.hypot(x1 - x0, y1 - y0); nx, ny = -(y1 - y0) / L * t / 2, (x1 - x0) / L * t / 2
    return prism(face([(x0 + nx, y0 + ny), (x1 + nx, y1 + ny), (x1 - nx, y1 - ny), (x0 - nx, y0 - ny)],
                      board_frame=False), z0, z1)


_OFF_BOARD = None
def _off_board(z0, z1):
    """everything outside the board outline + SKIRT_CLR, z0..z1."""
    global _OFF_BOARD
    if _OFF_BOARD is None:
        from params import D
        _OFF_BOARD = offset(face(D["board"]["outline"]), amount=SKIRT_CLR).faces()[0]
    return box(95, -70, 400, 400, z0, z1) - prism(_OFF_BOARD, z0 - 1, z1 + 1)


_INSIDE = None
def _inside():
    """the outline WALL_INTO short of the outer surface (adds that run into the wall are clipped to it)."""
    global _INSIDE
    if _INSIDE is None:
        _INSIDE = prism(offset(SILHOUETTE, amount=-(WALL - WALL_INTO)), SPLIT - 2, Z_FRONT)
    return _INSIDE


# ---------------------------------------------------------------- screw bosses
TL, TR, BL, BR = SHELL_SCREWS
# merged bosses: unit vector from the screw to the nearest point of the wall's inner face (self-test prints the gaps)
MERGE_DIR = {TL: (0.0, 1.0), TR: (0.0, 1.0), BR: (0.375, -0.927)}
# bottom-left (5.96 from the wall): webs toward the nearest wall point (24.52, -95.55) and straight west
BL_WEBS = [(-0.792, -0.611), (-1.0, 0.0)]


def boss(at):
    x, y = at
    b = cyl(x, y, BOSS_D, SPLIT, Z_PLATE_IN + FUSE)
    if at in MERGE_DIR:
        ux, uy = MERGE_DIR[at]
        far = (x + ux * (BOSS_D / 2 + 3.0), y + uy * (BOSS_D / 2 + 3.0))   # well past the wall; clipped by _inside()
        b += _strip((x, y), far, BOSS_D, MERGE_Z0, Z_PLATE_IN + FUSE) & _inside()
        b += _strip((x, y), far, BOSS_D, SPLIT, MERGE_Z0 + FUSE) & _inside() & _off_board(SPLIT, MERGE_Z0 + FUSE)
    if at == BL:
        for ux, uy in BL_WEBS:
            far = (x + ux * 10.0, y + uy * 10.0)
            b += _strip((x, y), far, WEB_T, WEB_Z0, Z_PLATE_IN + FUSE) & _inside()
    if at == TR:                                                      # D-cut on the J1 side
        b -= box(D_CUT_X - 10, y, 20, 20, SPLIT - 1, Z_FRONT)
    return b


def insert_hole(at):
    return cyl(*at, INSERT_HOLE_D, SPLIT - CUT, SPLIT + INSERT_DEPTH)


# ---------------------------------------------------------------- J1 hatch
H = J1_HATCH
HX0, HX1 = H["cx"] - H["w"] / 2, H["cx"] + H["w"] / 2               # 116.9..130.3
HY0, HY1 = H["cy"] - H["h"] / 2, H["cy"] + H["h"] / 2               # -38.7..-35.3


def hatch_opening():
    return box(H["cx"], H["cy"], H["w"], H["h"], CATCH_Z - CUT, Z_FRONT + CUT)


def hatch_pocket():
    return _rbox(H["cx"], H["cy"], H["w"] + 2 * HATCH_LEDGE, H["h"] + 2 * HATCH_LEDGE, LEDGE_Z, Z_FRONT + CUT, POCKET_R)


def hatch_notch():
    x1 = HX0                                          # notch runs under the cover's end to the opening's edge
    x0 = HX0 - HATCH_LEDGE - NOTCH["out"]
    return box((x0 + x1) / 2, H["cy"], x1 - x0, NOTCH["w"], NOTCH["z0"], Z_FRONT + CUT)


def notch_pad():
    """under the plate below the fingernail notch: notch floor - NOTCH_PAD t .. the plate, x up to the J1 column."""
    x1, m = HX0, NOTCH_PAD["margin"]
    x0 = HX0 - HATCH_LEDGE - NOTCH["out"] - m
    return box((x0 + x1) / 2, H["cy"], x1 - x0, NOTCH["w"] + 2 * m, NOTCH["z0"] - NOTCH_PAD["t"], Z_PLATE_IN + FUSE)


def chimneys():
    out = []
    for sgn, ye in ((1, HY1), (-1, HY0)):
        cy = ye + sgn * CHIMNEY_T / 2
        out.append(box(H["cx"], cy, CHIMNEY_W, CHIMNEY_T, CATCH_Z, Z_PLATE_IN + FUSE))
    return out


def j1_column():
    """the clear column from the face down to J1 (and J1 itself): the hatch footprint, z SPLIT..Z_FRONT."""
    return box(H["cx"], H["cy"], H["w"], H["h"], SPLIT, Z_FRONT)


def _tab(sgn):
    """snap tab on the north (sgn +1) / south (-1) long side: a plate in the (y, z) profile extruded along x."""
    ye = HY1 if sgn > 0 else HY0
    o = ye - sgn * TAB_CLR                             # tab's outer face
    i = o - sgn * TAB_T                                # inner face
    za = CATCH_Z - BARB_PLAY - (BARB - TAB_CLR)        # barb apex: the upper ramp passes the chimney corner at
                                                       # CATCH_Z - BARB_PLAY
    zb = za - BARB                                     # lower ramp foot
    z_top = LEDGE_Z + FUSE
    prof = [(i, z_top), (o, z_top), (o, za + BARB), (o + sgn * BARB, za), (o, zb), (o, zb - 0.2), (i, zb - 0.2)]
    # profile in the plane x = cx - TAB_W/2: local x -> +Y, local y -> +Z, normal +X
    pl = Plane(origin=(H["cx"] - TAB_W / 2, 0, 0), x_dir=(0, 1, 0), z_dir=(1, 0, 0))
    f = face([(y, z) for y, z in prof], board_frame=False)
    return pl.location * extrude(f, amount=TAB_W, dir=(0, 0, 1))


def j1_cover():
    """the printed SWD-hatch cover in the assembly frame (top flush with the face at Z_FRONT; prints face-down)."""
    lid = _rbox(H["cx"], H["cy"], H["w"] + 2 * (HATCH_LEDGE - COVER_CLR), H["h"] + 2 * (HATCH_LEDGE - COVER_CLR),
                LEDGE_Z, Z_FRONT, POCKET_R - COVER_CLR)
    return lid + _tab(1) + _tab(-1)


# ---------------------------------------------------------------- board locators
def finger(loc):
    """FINGER.w along the edge; inner face FINGER.gap off the board edge, outer face pocket_depth into the wall line;
    z FINGER.z0..RIB_Z1, lead_in 45 deg on the tip's inner edge."""
    (ax, ay), (nx, ny) = loc["at"], loc["n"]
    L = math.hypot(nx, ny); nx, ny = nx / L, ny / L
    tx, ty = -ny, nx
    s0, s1, z0, c = FINGER["gap"], WALL_LINE + FINGER["pocket_depth"], FINGER["z0"], FINGER["lead_in"]
    prof = [(s0, RIB_Z1), (s1, RIB_Z1), (s1, z0), (s0 + c, z0), (s0, z0 + c)]       # (along n, z)
    w = FINGER["w"]
    # local x -> n, local y -> +Z, normal -t (so local z extrudes along -t from the +t side)
    pl = Plane(origin=(ax + tx * w / 2, ay + ty * w / 2, 0), x_dir=(nx, ny, 0), z_dir=(-tx, -ty, 0))
    f = face(prof, board_frame=False)
    return pl.location * extrude(f, amount=w, dir=(0, 0, 1))


# ---------------------------------------------------------------- API
def adds():
    return [boss(s) for s in SHELL_SCREWS] + chimneys() + [notch_pad()] + [finger(l) for l in BOARD_LOCATORS.values()]


def cuts():
    return [insert_hole(s) for s in SHELL_SCREWS] + [hatch_opening(), hatch_pocket(), hatch_notch()]


def keepouts():
    return [j1_column()] + [cyl(*s, INSERT_HOLE_D, SPLIT, SPLIT + INSERT_DEPTH) for s in SHELL_SCREWS]


# ================================================================ self-test
if __name__ == "__main__":
    import time
    from build123d import Rot
    from params import (KEYS, GUIDE_BOTTOM_Z, GUIDE_WALL, LENS, LENS_POCKET, LAND_Z0, FRAME_W, LED, SPEAKER_AT,
                        SPK_RING, EDGE_KEYS, EDGE_NOTCH, EDGE_LIP_RECESS, POWER_SLOT, USBC, MECH, T, CLR,
                        board_solid, part_envelopes, module_2090, battery, speaker, lra)
    import shell
    t0 = time.time()
    V = lambda a, b: (a & b).volume
    dist = lambda a, b: a.distance_to(b)

    def report(label, v, ok):
        print(f"  {'ok ' if ok else 'BAD'} {label}: {v}")

    front0, back0 = shell.shells()
    A, C = adds(), cuts()
    front = front0
    for s in A: front = front + s
    for s in C: front = front - s
    print(f"front shell + struct: {len(front.solids())} solid(s), valid {front.is_valid}, {front.volume:.0f} mm3 "
          f"(bare {front0.volume:.0f})")

    top, bot = part_envelopes()
    ctx = {"board": board_solid(), "top envelopes": top, "bottom envelopes": bot, "module_2090": module_2090(),
           "battery": battery(), "speaker": speaker(), "lra": lra()}
    mine = {f"boss {s}": boss(s) - insert_hole(s) for s in SHELL_SCREWS}
    mine.update({f"chimney {i}": c for i, c in enumerate(chimneys())})
    mine["notch pad"] = notch_pad()
    mine.update({f"finger {k}": finger(l) for k, l in BOARD_LOCATORS.items()})
    mine["j1 cover"] = j1_cover()

    print("\n1. intersections with the context geometry (mm3; > 0.001 reported)")
    for mk, m in mine.items():
        for ck, c in ctx.items():
            v = V(m, c)
            if v > 0.001: report(f"{mk} x {ck}", f"{v:.4f}", False)
    for s in SHELL_SCREWS:                                            # intended: boss tips on the board top
        tip = cyl(*s, BOSS_D, SPLIT - 0.01, SPLIT) - cyl(*s, INSERT_HOLE_D, SPLIT - 0.02, SPLIT + 0.01)
        a = V(tip, ctx["board"]) / 0.01
        where = "the board top" if a > 0.1 else "nothing of the board (off-board: the back boss rises to SPLIT)"
        print(f"  intended: boss {s} tip on {where}, contact area {a:.1f} mm2 (volume 0, face contact)")

    print("\n2. boss columns: clearance (want >= 0.3)")
    zones = {}
    for k in ("A", "B", "START", "SELECT"):
        kk = KEYS[k]
        zones[f"key bore {k}"] = cyl(*kk["at"], kk["guide_id"] + 2 * GUIDE_WALL, GUIDE_BOTTOM_Z, Z_PLATE_IN)
    dp = KEYS["DPAD"]
    from front_keys import DPAD_HOLE_CLR
    zones["D-pad sweep"] = cyl(*dp["at"], 2 * (dp["arm"] + DPAD_HOLE_CLR + 1.0 + 0.9), SPLIT, Z_PLATE_IN)
    zones["screen frame zone"] = box(LENS["cx"], LENS["cy"], LENS["w"] + 2 * (LENS_POCKET["clr"] + FRAME_W),
                                     LENS["h"] + 2 * (LENS_POCKET["clr"] + FRAME_W), LAND_Z0, Z_PLATE_IN)
    zones["LED tube"] = cyl(*LED["at"], LED["tube_id"] + 2.0, 2.5, Z_PLATE_IN)
    zones["speaker ring"] = cyl(*SPEAKER_AT, SPK_RING["id"] + 2 * SPK_RING["wall"], Z_PLATE_IN - SPK_RING["depth"],
                                Z_PLATE_IN)
    j1env = prism(face(PARTS["J1"]["courtyard"]), SPLIT, J1_TOP)
    zones["J1 envelope"] = j1env
    zones["J1 column"] = j1_column()
    for name in ("module_2090", "top envelopes", "lra", "speaker"):
        zones[name] = ctx[name]
    for s in SHELL_SCREWS:
        b = boss(s)
        d = {k: dist(b, z) for k, z in zones.items()}
        worst = sorted(d.items(), key=lambda kv: kv[1])[:3]
        bad = [k for k, v in d.items() if v < (0.2 - 1e-6 if k == "J1 column" else 0.3)]
        print(f"  {'BAD' if bad else 'ok '} boss {s}: " + ", ".join(f"{k} {v:.2f}" for k, v in worst))
    print("  (J1 column = the clear hatch opening; the D-cut sits 0.2 back from its edge, the Dupont housing has"
          " another ~0.35 inside it)")
    hole = insert_hole(TR)
    print(f"  top-right D-cut: x {D_CUT_X:.2f}, wall left to the insert hole {(TR[0] - INSERT_HOLE_D / 2) - D_CUT_X:.2f}")

    print("\n3. J1 hatch")
    col = box(H["cx"], H["cy"], H["w"], H["h"], J1_TOP, Z_FRONT)
    v = V(front, col)
    report("front shell solid inside the column plate -> J1 top (z %.1f..%.1f)" % (J1_TOP, Z_FRONT), f"{v:.4f}", v < 1e-3)
    for k, m in mine.items():
        v = V(m, col)
        if v > 1e-3 and k != "j1 cover": report(f"{k} in the J1 column", f"{v:.4f}", False)
    cov = j1_cover()
    v = V(cov, front)
    report("cover x front shell", f"{v:.4f}", v < 1e-3)
    print(f"  cover to front shell min gap {dist(cov, front):.3f} (seated on the ledge: 0 = face contact)")
    for dz in (BARB_PLAY - 0.01, BARB_PLAY + 0.05, 0.3):
        lift = Pos(0, 0, dz) * cov
        print(f"  cover lifted {dz:.2f}: gap {dist(lift, front):.3f}, overlap {V(lift, front):.4f} mm3 "
              f"(barbs vs chimney corners: {BARB_PLAY} play, then they catch)")
    bb = cov.bounding_box()
    print(f"  cover: {len(cov.solids())} solid, valid {cov.is_valid}, {cov.volume:.1f} mm3, lid {COVER_T} thick, "
          f"top z {bb.max.Z:.2f} (face {Z_FRONT}), tabs to z {bb.min.Z:.2f}; J1 top {J1_TOP:.1f}; "
          f"cover x J1 {V(cov, j1env):.4f}")
    print(f"  ledge: {HATCH_LEDGE} wide, {LEDGE_Z - Z_PLATE_IN:.2f} thick; clear opening {H['w']} x {H['h']} "
          f"(Dupont 1x5 ~12.7 x 2.5)")

    print("\n4. board locators (want: board 0.1, everything else >= 0.3, thickness >= 0.8)")
    keep = {}
    for k, e in EDGE_KEYS.items():                                    # notch + lip recess + cap, outer -> tip + 0.5
        (ox, oy), (ux, uy) = e["outer"], e["dir"]
        dep = 2.82 + 0.5
        cx, cy = ox - ux * dep / 2, oy - uy * dep / 2
        a = math.degrees(math.atan2(uy, ux))
        keep[f"edge {k}"] = Pos(cx, cy, (SPLIT + EDGE_LIP_RECESS["z1"]) / 2) * Rot(0, 0, a) * \
            box(0, 0, dep, EDGE_LIP_RECESS["w"], SPLIT, EDGE_LIP_RECESS["z1"]).moved(Pos(0, 0, -(SPLIT + EDGE_LIP_RECESS["z1"]) / 2))
    ps = POWER_SLOT; a = math.radians(ps["angle_deg"]); u = (math.cos(a), math.sin(a)); nin = (-u[1], u[0])
    if nin[1] < 0: nin = (-nin[0], -nin[1])
    dep = WALL + 2.42 + 0.5; L = 7.0 + ps["travel"] + 0.6
    keep["power slider"] = Pos(ps["at"][0] + nin[0] * dep / 2, ps["at"][1] + nin[1] * dep / 2, 0) * \
        Rot(0, 0, ps["angle_deg"]) * box(0, 0, L, dep, ps["z0"], ps["z0"] + ps["h"])
    keep["USB-C"] = box(USBC["x"], -31.0, USBC["relief_w"], 3.0, USBC["z"] - USBC["relief_h"] / 2,
                        USBC["z"] + USBC["relief_h"] / 2)
    for k in ("top envelopes", "bottom envelopes", "module_2090", "battery", "speaker", "lra"):
        keep[k] = ctx[k]
    for k, l in BOARD_LOCATORS.items():
        f = finger(l)
        db = dist(f, ctx["board"])
        d = sorted(((kk, dist(f, z)) for kk, z in keep.items()), key=lambda kv: kv[1])[0]
        (ax, ay), (nx, ny) = l["at"], l["n"]; tx, ty = -ny, nx      # back-shell pocket per DESIGN.md 4
        pd, pc = FINGER["pocket_depth"] + FINGER["pocket_clr"], FINGER["pocket_clr"]    # 0.9 deep (back_openings)
        sm = WALL_LINE + (pd - CUT) / 2
        pk = Pos(ax + nx * sm, ay + ny * sm, 0) * Rot(0, 0, math.degrees(math.atan2(ny, nx))) * \
            box(0, 0, pd + CUT, FINGER["w"] + 2 * pc, FINGER["z0"] - pc, SPLIT + CUT)
        vb = V(f, back0 - pk)
        print(f"  {'ok ' if abs(db - 0.1) < 0.02 and d[1] >= 0.3 else 'BAD'} {k}: board {db:.3f}, nearest {d[0]} "
              f"{d[1]:.2f}; x back shell less its pocket {vb:.4f} mm3")
    s1 = WALL_LINE + FINGER["pocket_depth"]
    print(f"  finger thickness {s1 - FINGER['gap']:.2f} (at the tip, after the lead-in {s1 - FINGER['gap'] - FINGER['lead_in']:.2f})")

    print("\n5. keepouts vs my own adds (only the intended insert-hole wall contacts)")
    for i, ko in enumerate(keepouts()):
        for k, m in mine.items():
            v = V(m, ko)
            if v > 1e-3 and k != "j1 cover": report(f"keepout {i} x {k}", f"{v:.4f}", False)
    print(f"\n{time.time() - t0:.1f} s")
