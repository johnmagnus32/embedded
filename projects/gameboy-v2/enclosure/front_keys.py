"""front_keys.py — the front shell's face-key features (DESIGN.md section 3, Keycaps / D-pad):
A / B / START / SELECT: a face hole (d + 2 KEY_HOLE_CLR) through the plate and a guide tube from the plate down to
GUIDE_BOTTOM_Z whose bore (guide_id) passes the cap's flange, so the cap goes in head-first from inside and its flange
seats on the plate's inner face in the annulus between the face hole and the bore. B's tube wall is cut back where it
would come within SPK_CLR of the speaker (the bore stays open there).
D-pad: the cross hole (dpad_hole_face(): the sharp cross offset DPAD_HOLE_CLR, R0.3 corners) through the plate;
nothing of ours under the plate round it.
API (front_features): adds(), cuts(), keepouts(), plus guide_bore(name) and face_hole(name).
`.venv/bin/python front_keys.py` runs the self-test against shell.shells() and the context geometry."""
import math
from build123d import Cone, Kind, Pos, Rot, fillet, offset
from params import (KEYS, KEY_HOLE_CLR, KEY_PROUD, KEY_REST_GAP, KEYSTAKE_Z, FLANGE_T, FLANGE_LIP, GUIDE_BOTTOM_Z,
                    GUIDE_WALL, SPEAKER_AT, MECH, LENS, LENS_POCKET, LAND_Z0, FRAME_W, EDGE_KEYS, EDGE_TIP_INSET,
                    EDGE_LIP_RECESS, POWER_SLOT, SPLIT, T, Z_FRONT, Z_PLATE_IN, box, cyl, face, prism)

# ---------------------------------------------------------------- choices (this module only)
ROUND = ("A", "B", "START", "SELECT")
FUSE = 0.2                        # tubes run this far up into the plate so the union fuses solidly
CUT = 0.5                         # cut tools overshoot faces by this much
ENTRY_CHAMFER = 0.4               # 45-degree lead-in at each bore mouth (z GUIDE_BOTTOM_Z), the head goes in there
SPK_R = MECH["speaker"]["d"] / 2  # 13.5: the speaker can (params.speaker, z 14.1..19.0)
SPK_CLR = 0.3                     # B's wall stays this far off the speaker cylinder ...
SPK_CUT_Z = GUIDE_BOTTOM_Z        # ... for z >= this. The brief said 13.6 (speaker bottom 14.1 - 0.5), but the speaker
                                  # drops into its ring from inside (DESIGN.md 2) through the column r 13.5 below it, and
                                  # B's wall reaches r 13.19 of the speaker axis: so the cut runs the tube's full height
                                  # (also no bridge over the gap when the front shell prints face-down)
DPAD_HOLE_CLR = KEYS["DPAD"]["hole_clr"]   # 1.3 (rev 2.1): the cross sways 0.97 axial / 1.36
                                  # diagonal at the face (z 20.9) when its domed legs take the switches to the 0.3 stop,
                                  # and sits up to 0.285 off centre (controls.dpad_shift_max: pivot in its washer 0.085,
                                  # washer between the switches 0.1, board on the fingers 0.1) -> axial 1.25 needed
DPAD_HOLE_R = 0.3                 # the hole's outer corners: near-sharp (an R2.3 arc offset gives only 1.3 on the
                                  # diagonal, sharp gives 1.3 x 1.41 = 1.84 at the faces; 1.76 at the inner corners)
DPAD_CORNER_R = (1.0, 0.2)        # cross corners, convex / concave (controls imports these for the cap; rev 2.1: concave
                                  # 0.5 -> 0.2, its bulge into the hole's sharp inner corner set the diagonal limit)
DPAD_SWEEP_MARGIN = 0.9           # flange sweep: cross + hole clr + FLANGE_LIP + this (DESIGN.md 3) ...
DPAD_SWEEP_Z = (16.5, Z_PLATE_IN)  # ... over this z range (rev 2.1: the domed legs tilt 4.1 deg on the diagonal, so the
                                   # flange's near side dips to ~16.9)
DPAD_DROP_MARGIN = 0.5            # legs / pivot drop-in column: cross + this, z SPLIT..Z_PLATE_IN


# ---------------------------------------------------------------- geometry
def _hole_d(name):
    return KEYS[name]["d"] + 2 * KEY_HOLE_CLR                      # 9.8 / 4.4


def _tube_od(name):
    return KEYS[name]["guide_id"] + 2 * GUIDE_WALL                 # 14.2 / 8.8


def _cross(grow=0.0):
    """the D-pad cross (arm, w; convex R1.0 / concave R0.5, as the cap), offset by grow, placed at the D-pad centre."""
    k = KEYS["DPAD"]
    a, h = k["arm"], k["w"] / 2
    f = face([(h, h), (a, h), (a, -h), (h, -h), (h, -a), (-h, -a), (-h, -h), (-a, -h), (-a, h), (-h, h), (-h, a),
              (h, a)], board_frame=False)
    f = fillet([v for v in f.vertices() if max(abs(v.X), abs(v.Y)) > a - 1e-6], DPAD_CORNER_R[0]).faces()[0]
    f = fillet([v for v in f.vertices() if abs(abs(v.X) - h) < 1e-6 and abs(abs(v.Y) - h) < 1e-6],
               DPAD_CORNER_R[1]).faces()[0]
    if grow:
        f = offset(f, amount=grow, kind=Kind.ARC).faces()[0]
    return Pos(*k["at"], 0) * f


def dpad_hole_face(grow=0.0):
    """the D-pad's face-hole outline at the D-pad centre: the unrounded cross offset DPAD_HOLE_CLR with sharp joins, its
    eight outer corners rounded DPAD_HOLE_R (inner corners sharp), then offset by grow (round joins)."""
    k = KEYS["DPAD"]
    a, h = k["arm"], k["w"] / 2
    f = face([(h, h), (a, h), (a, -h), (h, -h), (h, -a), (-h, -a), (-h, -h), (-a, -h), (-a, h), (-h, h), (-h, a),
              (h, a)], board_frame=False)
    f = offset(f, amount=DPAD_HOLE_CLR, kind=Kind.INTERSECTION).faces()[0]
    f = fillet([v for v in f.vertices() if max(abs(v.X), abs(v.Y)) > a + 1e-6], DPAD_HOLE_R).faces()[0]
    if grow:
        f = offset(f, amount=grow, kind=Kind.ARC).faces()[0]
    return Pos(*k["at"], 0) * f


def _sector(cx, cy, r, a_mid, half, z0, z1):
    """pie slice about (cx, cy), radius r, angles a_mid +- half (rad), z0..z1."""
    n = 16
    pts = [(cx, cy)] + [(cx + r * math.cos(a_mid - half + 2 * half * i / n),
                         cy + r * math.sin(a_mid - half + 2 * half * i / n)) for i in range(n + 1)]
    return prism(face(pts, board_frame=False), z0, z1)


def b_speaker_relief():
    """(centre angle, half angle) in rad about B's axis of the wall sector cut for the speaker: where the tube's outer
    circle comes within SPK_R + SPK_CLR of the speaker axis (law of cosines)."""
    (bx, by), (sx, sy) = KEYS["B"]["at"], SPEAKER_AT
    d, ro, rs = math.dist((bx, by), (sx, sy)), _tube_od("B") / 2, SPK_R + SPK_CLR
    c = (d * d + ro * ro - rs * rs) / (2 * d * ro)
    half = math.acos(max(-1.0, min(1.0, c)))
    return math.atan2(sy - by, sx - bx), half


def _tube(name):
    """outer cylinder of one guide tube, GUIDE_BOTTOM_Z up into the plate (bore cut later); B minus its speaker sector."""
    x, y = KEYS[name]["at"]
    t = cyl(x, y, _tube_od(name), GUIDE_BOTTOM_Z, Z_PLATE_IN + FUSE)
    if name == "B":
        a, half = b_speaker_relief()
        t -= _sector(x, y, _tube_od(name) / 2 + 1.0, a, half, SPK_CUT_Z, Z_PLATE_IN)
    return t


def guide_bore(name):
    """the bore column of a round key: guide_id, z GUIDE_BOTTOM_Z..Z_FRONT (through the plate too, as a keep-out; the
    cut stops at Z_PLATE_IN so the flange seats on the plate in the annulus outside the face hole)."""
    x, y = KEYS[name]["at"]
    return cyl(x, y, KEYS[name]["guide_id"], GUIDE_BOTTOM_Z, Z_FRONT)


def face_hole(name):
    """the hole through the plate: round keys d + 2 KEY_HOLE_CLR, the D-pad cross + hole_clr."""
    if name == "DPAD":
        return prism(dpad_hole_face(), Z_PLATE_IN, Z_FRONT)
    x, y = KEYS[name]["at"]
    return cyl(x, y, _hole_d(name), Z_PLATE_IN, Z_FRONT)


def _entry_cone(name):
    """45-degree lead-in at the bore mouth (ENTRY_CHAMFER), plus CUT below the tube end."""
    x, y = KEYS[name]["at"]
    r = KEYS[name]["guide_id"] / 2
    h = ENTRY_CHAMFER + CUT
    return Pos(x, y, GUIDE_BOTTOM_Z - CUT + h / 2) * Cone(r + ENTRY_CHAMFER + CUT, r, h)


def dpad_flange_sweep():
    k = KEYS["DPAD"]
    return prism(dpad_hole_face(FLANGE_LIP + DPAD_SWEEP_MARGIN), *DPAD_SWEEP_Z)


def dpad_drop_column():
    return prism(_cross(DPAD_DROP_MARGIN), SPLIT, Z_PLATE_IN)


# ---------------------------------------------------------------- API
def _hollow(names):
    """merged outer tubes minus every bore + entry cone (so our adds never enter our own keep-outs either)."""
    t = _tube(names[0]) + _tube(names[1])
    for n in names:
        x, y = KEYS[n]["at"]
        t = t - cyl(x, y, KEYS[n]["guide_id"], GUIDE_BOTTOM_Z - CUT, Z_FRONT) - _entry_cone(n)
    return t


def adds():
    """the guide tubes, merged where they touch (A+B, START+SELECT), bores already open (cuts() reopens them after
    other modules' adds)."""
    return [_hollow(("A", "B")), _hollow(("START", "SELECT"))]


def cuts():
    out = []
    for n in ROUND:
        x, y = KEYS[n]["at"]
        out.append(cyl(x, y, KEYS[n]["guide_id"], GUIDE_BOTTOM_Z - CUT, Z_PLATE_IN))     # bore: flange passes
        out.append(cyl(x, y, _hole_d(n), Z_PLATE_IN - CUT, Z_FRONT + CUT))              # face hole
        out.append(_entry_cone(n))
    out.append(prism(dpad_hole_face(), Z_PLATE_IN - CUT, Z_FRONT + CUT))               # D-pad cross hole
    return out


def keepouts():
    out = [guide_bore(n) for n in ROUND] + [face_hole(n) for n in ROUND]
    out += [face_hole("DPAD"), dpad_flange_sweep(), dpad_drop_column()]
    return out


# ================================================================ self-test
def _ctx():
    from params import board_solid, part_envelopes, module_2090, battery, speaker, lra
    top, bot = part_envelopes()
    return {"board": board_solid(), "parts_top": top, "parts_bot": bot, "module_2090": module_2090(),
            "battery": battery(), "speaker": speaker(), "lra": lra()}


def _frame_zone():
    """the screen frame zone: LENS pocket outline + clr + FRAME_W, z LAND_Z0 - 0.3 .. Z_PLATE_IN."""
    g = LENS_POCKET["clr"] + FRAME_W
    w, h = LENS["w"] + 2 * g, LENS["h"] + 2 * g
    b = box(LENS["cx"], LENS["cy"], w, h, LAND_Z0 - 0.3, Z_PLATE_IN)
    return fillet(b.edges().filter_by(lambda e: abs(e.tangent_at(0).Z) > 0.99), LENS["r"] + g)


def _edge_keepouts():
    """stand-ins for the edge keys and power slot (DESIGN.md 5): along 'dir' from the plunger tip - 1 to the outer
    surface + 2, EDGE_LIP_RECESS w wide, z SPLIT..EDGE_LIP_RECESS z1; the slot len + travel + 2, z SPLIT..z0+h+1.5."""
    out = []
    for k in EDGE_KEYS.values():
        (tx, ty), (ux, uy) = k["tip"], k["dir"]
        L = EDGE_TIP_INSET + 3.0
        cx, cy = tx + ux * (L / 2 - 1.0), ty + uy * (L / 2 - 1.0)
        a = math.degrees(math.atan2(uy, ux))
        out.append(Pos(cx, cy, 0) * Rot(0, 0, a) * box(0, 0, L, EDGE_LIP_RECESS["w"], SPLIT, EDGE_LIP_RECESS["z1"]))
    s = POWER_SLOT
    a = s["angle_deg"]
    out.append(Pos(*s["at"], 0) * Rot(0, 0, a) * box(0, 0, s["len"] + s["travel"] + 2, 8.0, s["z0"],
                                                       s["z0"] + s["h"] + 1.5))
    return out


def _vol(a, b):
    try:
        return (a & b).volume
    except Exception:
        return float("nan")


def _caps():
    """contract cap envelopes, grown by 0.15 radially (the clearance we must leave): round keys head d over the face,
    flange_d x FLANGE_T under the plate (and passing the bore all the way down: the insertion path), stem + fins
    (fin_od = flange_d over FIN_Z, stem_d below to the tip). The D-pad head + flange sweep + legs column."""
    g = 0.15
    out = {}
    for n in ROUND:
        k = KEYS[n]; x, y = k["at"]
        head = cyl(x, y, k["d"] + 2 * g, Z_PLATE_IN, Z_FRONT + KEY_PROUD)
        path = cyl(x, y, k["flange_d"] + 2 * g, GUIDE_BOTTOM_Z, Z_PLATE_IN - 0.001)      # flange / fins travel path
        tip = KEYSTAKE_Z + KEY_REST_GAP
        stem = cyl(x, y, k["stem_d"] + 2 * g, tip, Z_PLATE_IN - FLANGE_T)
        out[n] = head + path + stem
    k = KEYS["DPAD"]
    out["DPAD"] = (prism(_cross(g), Z_PLATE_IN, Z_FRONT + KEY_PROUD)
                   + prism(dpad_hole_face(FLANGE_LIP + g), DPAD_SWEEP_Z[0], Z_PLATE_IN - 0.001)
                   + prism(_cross(g), T, Z_PLATE_IN - 0.001))
    return out


if __name__ == "__main__":
    import shell
    front, _ = shell.shells()
    f = front
    for s in adds():
        f = f + s
    for s in cuts():
        f = f - s
    sol = f.solids()
    print(f"front shell after keys: {len(sol)} solid(s), valid {f.is_valid}, volume {f.volume:.0f} "
          f"(bare {front.volume:.0f})")

    a, half = b_speaker_relief()
    print(f"B speaker relief: sector {math.degrees(2 * half):.1f} deg of 360 ({100 * half / math.pi:.1f}% of the "
          f"circumference) centred {math.degrees(a):.1f} deg, z {SPK_CUT_Z}..{Z_PLATE_IN}")
    mine = adds()
    allmine = mine[0] + mine[1]
    print(f"tubes: A+B {len(mine[0].solids())} solid, START+SELECT {len(mine[1].solids())} solid")
    spk = cyl(*SPEAKER_AT, 2 * (SPK_R + SPK_CLR), SPLIT, Z_PLATE_IN)
    print(f"  B wall vs speaker drop-in column r {SPK_R + SPK_CLR} z {SPLIT}..{Z_PLATE_IN}: {_vol(mine[0], spk):.4f} mm3; "
          f"tubes to speaker {allmine.distance_to(_ctx()['speaker']):.3f}")

    # context
    ctx = _ctx()
    for n, c in ctx.items():
        print(f"  tubes vs {n:12s}: {_vol(allmine, c):.4f} mm3, distance {allmine.distance_to(c):.3f}")
    print(f"  tubes vs screen frame zone: {_vol(allmine, _frame_zone()):.4f} mm3, distance "
          f"{allmine.distance_to(_frame_zone()):.3f}")
    for i, e in enumerate(_edge_keepouts()):
        print(f"  tubes vs edge keep-out {i}: {_vol(allmine, e):.4f} mm3, distance {allmine.distance_to(e):.3f}")

    # caps
    caps = _caps()
    for n, c in caps.items():
        print(f"  cap envelope + 0.15 {n:6s} vs shell: {_vol(f, c):.4f} mm3")
    for n in ROUND:
        x, y = KEYS[n]["at"]
        print(f"  bore {n:6s} (z {GUIDE_BOTTOM_Z}..{Z_PLATE_IN}) vs shell: "
              f"{_vol(f, cyl(x, y, KEYS[n]['guide_id'], GUIDE_BOTTOM_Z, Z_PLATE_IN - 0.001)):.4f} mm3")
    names = [f"bore {n}" for n in ROUND] + [f"hole {n}" for n in ROUND] + ["hole DPAD", "DPAD flange sweep",
                                                                         "DPAD drop column"]
    for nm, k in zip(names, keepouts()):
        print(f"  keepout {nm:18s} vs our adds {_vol(allmine, k):.4f} mm3, vs shell after {_vol(f, k):.4f} mm3")
    t = mine[0]
    print(f"  A+B tube: z {t.bounding_box().min.Z:.2f}..{t.bounding_box().max.Z:.2f}; START+SELECT valid "
          f"{mine[1].is_valid}; A+B valid {t.is_valid}")
