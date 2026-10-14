"""battery_door.py — the printed battery door (rev 3, DESIGN.md section 4): the cell (params.battery()) hangs through the
back floor's opening, and this shallow tub closes it from outside - the bump on the back, like the GBA's battery cover.

  tub        rounded rectangle, top face on the back face (Z_BACK), outer face at DOOR_Z0, BATTERY_DOOR floor / walls,
             outer edges rounded r_edge; the cell sits in it on a cell_gap foam pad
  hooks      two at the west end: a stem up through the floor's hook slot (back_openings.hook_slots) and a tongue over
             the floor's inner face, play (one print layer) above it, a lead_in chamfer on its lower west edge; the
             door's top face laps west_lap past the slots' west wall, so no slit shows beside the stems
  screws     two ISO 10642 M2 countersunk screws at the east end, either side of the cell's lead gap (flush in the outer
             face), up into the heat-set inserts in back_openings.door_boss()
Fitting (the reverse takes it off): the door up into place `slide` east of home - the hooks rise through their slots,
the tub's west wall stays cell_margin off the cell, the pad pushes the cell home - then slid west onto the hooks (the
pad's PET face slides under the cell), then the screws. fit_path() gives the poses; checks.py (h) sweeps them. Prints
outer-face-down (as built), a support enforcer under each tongue (0.2 top z gap).
API: door(), screw() (both), fit_path(), print_pose(). `.venv/bin/python battery_door.py` runs the self-test."""
import math
from build123d import Axis, Box, Cone, Pos, Rot, fillet, offset
from params import (BATTERY_DOOR as BD, DOOR_Z0, Z_BACK, Z_FLOOR_IN, R_OUT_BACK, SILHOUETTE, box, cyl, prism, face)
import back_openings as bo

CUT = 0.2                    # cutters overshoot the faces they open
FUSE = 0.1                   # hooks start this far inside the door's top face

# ---------------------------------------------------------------- outline (enclosure frame)
def _xy():
    """door + tub interior extents: (x0, x1, y0, y1) the tub, (x0, x1, y0, y1) its interior, (x1, y0, y1) the screw ear
    (the tub's east end stays inside the back face's flat zone; the ear carries the two screws past it, under the
    board)."""
    (cx, cy), w, h = bo.CELL_AT, bo.CELL["w"] / 2, bo.CELL["h"] / 2
    o = bo.FENCE_CLR                                              # the floor opening is the cell + this
    sx, _ = bo.hook_slot_x()
    ix0 = cx - w - BD["cell_margin"] - BD["slide"]                # the west wall stays cell_margin off the cell on the
    ix1, iy0, iy1 = cx + w + o, cy - h - o, cy + h + o            # fitting path (it comes in slide east of home)
    x0 = sx - BD["west_lap"]                                      # past the slots' west wall: covers them at home
    (scx, ys), (_, yn) = bo.door_screw_ats()
    e = BD["csk_d"] / 2 + BD["edge_margin"] + BD["r_edge"]        # the countersink stays off the edge rounding
    return (x0, ix1 + BD["wall"], iy0 - BD["wall"], iy1 + BD["wall"]), (ix0, ix1, iy0, iy1), (scx + e, ys - e, yn + e)

def _rrect(x0, x1, y0, y1, r, r_east=None):
    """rounded rectangle face at z 0, corner radius r (r_east on the two east corners, if given)."""
    from build123d import fillet as fil
    f = face([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], board_frame=False)
    re = r if r_east is None else r_east
    f = fil([v for v in f.vertices() if abs(v.X - x0) < 1e-6], r).faces()[0]
    return fil([v for v in f.vertices() if abs(v.X - x1) < 1e-6], re).faces()[0]

EAR_BLEND = 2.0              # plan radius where the ear meets the tub

def outline():
    """the door's plan outline (face at z 0): the tub's rounded rectangle + the screw ear (a strip along the east end
    whose two round ends are concentric with the screws), blended EAR_BLEND where they meet."""
    from build123d import Circle, Kind
    (x0, x1, y0, y1), _, (ex1, ey0, ey1) = _xy()
    (scx, ys), (_, yn) = bo.door_screw_ats()
    ear = face([(x1 - BD["r_plan"], ey0), (scx, ey0), (scx, ey1), (x1 - BD["r_plan"], ey1)], board_frame=False) + \
        face([(scx, ys), (ex1, ys), (ex1, yn), (scx, yn)], board_frame=False)
    for y in (ys, yn):
        ear = ear + (Pos(scx, y) * Circle(ex1 - scx)).faces()[0]
    r_e = bo.FENCE_CLR + BD["wall"]                               # east corners: concentric with the interior's (1.6)
    f = (_rrect(x0, x1, y0, y1, BD["r_plan"], r_e) + ear).faces()[0]
    f = offset(f, amount=EAR_BLEND, kind=Kind.ARC).faces()[0]                    # closing: rounds the two concave
    f = offset(f, amount=-EAR_BLEND, kind=Kind.ARC).faces()[0]                   # corners EAR_BLEND, keeps the rest
    assert f.bounding_box().max.X > ex1 - 1e-3 and len(f.inner_wires()) == 0
    return f

# ---------------------------------------------------------------- parts
def hooks():
    """the two hooks: stem (stem_t, from the top face up) + tongue (west over the floor, play above it, its lower west
    edge chamfered lead_in x 45 deg)."""
    sx, _ = bo.hook_slot_x()
    xs0 = sx + BD["slot_clr"]; xs1 = xs0 + BD["stem_t"]           # stem, home
    zt0 = Z_FLOOR_IN + BD["play"]; zt1 = zt0 + BD["tongue_t"]
    xt0 = sx - BD["reach"]
    c = BD["lead_in"] * math.sqrt(2)                              # a square this wide, 45 deg, centred on the edge
    out = None
    for y in bo.hook_ys():
        h = box((xs0 + xs1) / 2, y, xs1 - xs0, BD["hook_w"], Z_BACK - FUSE, zt1) + \
            box((xt0 + xs1) / 2, y, xs1 - xt0, BD["hook_w"], zt0, zt1)
        h = h - Pos(xt0, y, zt0) * Rot(0, 45, 0) * Box(c, BD["hook_w"] + 2 * CUT, c)
        out = h if out is None else out + h
    return out

def _screw_cutter():
    dz = (BD["csk_d"] - BD["hole_d"]) / 2                         # 90 deg countersink depth
    out = None
    for x, y in bo.door_screw_ats():
        c = cyl(x, y, BD["hole_d"], DOOR_Z0 - CUT, Z_BACK + CUT) + \
            Pos(x, y, DOOR_Z0 - CUT + (dz + CUT) / 2) * Cone(BD["csk_d"] / 2 + CUT, BD["hole_d"] / 2, dz + CUT)
        out = c if out is None else out + c
    return out

def door():
    """the battery door at home in the assembly frame (labelled 'battery_door')."""
    _, (ix0, ix1, iy0, iy1), _ = _xy()
    tub = prism(outline(), DOOR_Z0, Z_BACK)
    tub = fillet(tub.edges().group_by(Axis.Z)[0], BD["r_edge"])                   # rounded outer edges
    tub = tub - prism(_rrect(ix0, ix1, iy0, iy1, bo.FENCE_CLR), DOOR_Z0 + BD["floor"], Z_BACK + CUT)
    d = (tub + hooks()) - _screw_cutter()
    s = d.solids()
    assert len(s) == 1, f"battery door: {len(s)} solids"
    s = s[0]
    s.label = "battery_door"
    return s

def screw():
    """the two M2 x screw_l countersunk screws, home (heads flush in the outer face) - context for the checks."""
    k = (BD["csk_d"] - BD["screw_d"]) / 2                          # 90 deg head height
    out = None
    for x, y in bo.door_screw_ats():
        sc = Pos(x, y, DOOR_Z0 + k / 2) * Cone(BD["csk_d"] / 2, BD["screw_d"] / 2, k) + \
            cyl(x, y, BD["screw_d"], DOOR_Z0 + k - 0.01, DOOR_Z0 + BD["screw_l"])
        out = sc if out is None else out + sc
    return out

def fit_path(step_z=0.25, step_x=0.125):
    """[(dx, dz)] offsets from home along the fitting path: up from fit_drop() below (dx = slide), then west to home."""
    s, H = BD["slide"], fit_drop()
    n, m = max(1, math.ceil(H / step_z)), max(1, math.ceil(s / step_x))
    return [(s, -H + H * i / n) for i in range(n + 1)] + [(s - s * j / m, 0.0) for j in range(1, m + 1)]

def fit_drop():
    """how far below home the door starts: its hooks' tops 0.5 under the back face."""
    return (Z_FLOOR_IN + BD["play"] + BD["tongue_t"]) - Z_BACK + 0.5

def print_pose(part):
    """outer face down = as built; centred on the origin, on z 0."""
    bb = part.bounding_box()
    p = Pos(-(bb.min.X + bb.max.X) / 2, -(bb.min.Y + bb.max.Y) / 2, -bb.min.Z) * part
    p.label = part.label
    return p

def fillet_overhang():
    """how far the door's outline reaches past the back face's flat zone (the R_OUT_BACK fillet starts there), and
    the gap under its edge there (the fillet's sagitta)."""
    flat = offset(SILHOUETTE, amount=-R_OUT_BACK).faces()[0]
    ow = outline().outer_wire()
    worst = 0.0
    for i in range(400):
        p = ow.position_at(i / 400)
        if not flat.is_inside(p):
            worst = max(worst, flat.outer_wire().distance_to(p))
    return worst, R_OUT_BACK - math.sqrt(R_OUT_BACK ** 2 - min(worst, R_OUT_BACK) ** 2)

# ================================================================ self-test
if __name__ == "__main__":
    import time
    import params as p, shell
    t0 = time.time()
    d, sc = door(), screw()
    _, b0 = shell.shells()
    back = bo.apply_back_openings(bo.apply_back(b0))
    bb = d.bounding_box()
    (x0, x1, y0, y1), (ix0, ix1, iy0, iy1), (ex1, ey0, ey1) = _xy()
    print(f"door: valid {d.is_valid}, {d.volume:.0f} mm3, x {x0:.2f}..{x1:.2f} y {y0:.2f}..{y1:.2f} ({x1 - x0:.1f} x "
          f"{y1 - y0:.1f}; ear to x {ex1:.2f}, y {ey0:.2f}..{ey1:.2f}), z {bb.min.Z:.2f}..{bb.max.Z:.2f}; bump {Z_BACK - DOOR_Z0:.2f} past the back face; "
          f"interior x {ix0:.2f}..{ix1:.2f} y {iy0:.2f}..{iy1:.2f}")
    print(f"back shell: valid {back.is_valid}, {len(back.solids())} solid")
    top, bot = p.part_envelopes()
    cell, board, mod = p.battery(), p.board_solid(), p.module_2090()
    V = lambda a, c: getattr(a & c, "volume", 0.0)
    for n, c in (("back shell", back), ("cell", cell), ("board", board), ("parts bottom", bot), ("module_2090", mod),
                 ("screw", sc)):
        print(f"  door vs {n:12s} overlap {V(d, c):.4f}, gap {d.distance_to(c):.3f}")
    for n, c in (("back shell", back), ("board", board), ("cell", cell)):
        print(f"  screw vs {n:10s} overlap {V(sc, c):.4f}, gap {sc.distance_to(c):.3f}")
    hk = hooks()
    print(f"  hooks: to the back shell {hk.distance_to(back):.3f} (play {BD['play']}), to the board "
          f"{hk.distance_to(board):.3f}, to the module (standoff screw heads under the board) {hk.distance_to(mod):.3f}")
    print(f"  cell: over the door floor {cell.distance_to(d):.3f} (cell_gap {BD['cell_gap']}), "
          f"z {cell.bounding_box().min.Z:.2f}..{cell.bounding_box().max.Z:.2f}")
    print(f"  bosses: to the board {bo.door_boss().distance_to(board):.3f}, to parts bottom (incl. THT tails) "
          f"{bo.door_boss().distance_to(bot):.3f}, to the cell {bo.door_boss().distance_to(cell):.3f}; screws at "
          + ", ".join(f"({x:.2f}, {y:.2f})" for x, y in bo.door_screw_ats()) + f", tip z {sc.bounding_box().max.Z:.2f}")
    zp = lambda z: z - DOOR_Z0
    print(f"  print layers ({p.PRINT_LAYER}): top face z_print {zp(Z_BACK):.3f}, tongue underside "
          f"{zp(Z_FLOOR_IN + BD['play']):.3f}, tongue top {zp(Z_FLOOR_IN + BD['play'] + BD['tongue_t']):.3f}; floor "
          f"{BD['floor']} (>= {BD['floor_min']})")
    sx, _ = bo.hook_slot_x()
    print(f"  west edge x {x0:.2f}: {sx - x0:.2f} past the slots' west wall (x {sx:.2f}); at +slide {x0 + BD['slide']:.2f}")
    ov, sag = fillet_overhang()
    print(f"  outline past the back face's flat zone: {ov:.3f} (gap under its edge {sag:.3f})")
    worst = 0.0
    for dx, dz in fit_path():
        m = Pos(dx, 0, dz) * d
        for n, c in (("back", back), ("cell", cell), ("board", board), ("module", mod)):
            v = V(m, c)
            if v > 0.001:
                print(f"  fit path ({dx:.3f}, {dz:.3f}) x {n}: {v:.4f}")
            worst = max(worst, v)
    print(f"  fit path ({len(fit_path())} poses, up {fit_drop():.2f} then west {BD['slide']}): worst overlap {worst:.4f}")
    pp = print_pose(d)
    print(f"  print pose z 0..{pp.bounding_box().max.Z:.2f}; {time.time() - t0:.1f} s")
