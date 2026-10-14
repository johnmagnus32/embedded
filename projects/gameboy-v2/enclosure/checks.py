#!/usr/bin/env python3
"""checks.py — the gb2 enclosure's fit checks (DESIGN.md section 6), on the assembled parts from enclosure.build().

  (a) interference matrix: every printed part vs every other, the board, every part envelope by ref (J7 / SW1 as
      back_openings' real-shape models, not their board.json boxes), module_2090, battery, speaker, LRA
  (b) every module's keepouts() vs every OTHER module's added material (+ the finished front shell)
  (c) rest gaps: face-key / D-pad leg tips to the keystake, edge-cap contact faces to the plunger tips, the slider
      fork round the SW1 lever
  (d) solid count + validity per printed part (+ its print pose on the bed)
  (e) lens pocket floor vs GLASS_WORST, nothing of the front shell over the panel / FPC fold
  (f) wall-to-board minimum, each locator finger's gap to the board edge (+ to its back-shell pocket)
  (h) assembly path (DESIGN.md 2, rev 2.1 order): every part / body swept along +Z into its rest pose past what is
      already in place (controls into the face-down front shell, the speaker, the board pack, the back shell + cell)
  (i) min wall: a ray from every surface triangle inward along its normal; a slab (exit face within SLAB_DEG of
      antiparallel) under MIN_WALL fails (tapers and chamfers end in wedges, which this ignores)
  (j) D-pad rocking: axial / 22.5 deg / diagonal presses with the cross off centre by +-controls.dpad_shift_max(), each
      must take its switch(es) to the 0.3 stop before the head / flange meets the plate or a leg the switch cover
  (g) a PASS / FAIL line per check

Run (from this folder): .venv/bin/python checks.py [--fast]   (--fast skips (h); exit status 1 if any check fails)
"""
import math, sys, time
import numpy as np
from build123d import Pos, Vector, extrude, offset
import params, enclosure, front_screen, front_keys, front_struct, back_openings, controls
from params import (KEYS, EDGE_KEYS, KEYSTAKE_Z, KEY_REST_GAP, DPAD_REST_GAP, EDGE_AXIS_Z, EDGE_REST_GAP, POWER_SLOT,
                    BOARD_LOCATORS, FINGER, PANEL, FPC_BULGE, LENS, TAPE_T, GLASS_WORST, SILHOUETTE, WALL, SPLIT, T,
                    Z_BACK, Z_FRONT, Z_PLATE_IN, PLATE, box, cyl, prism)

# ---------------------------------------------------------------- thresholds
EPS_V = 0.001            # mm3: overlaps above this are reported
TOL = 0.005              # mm: a rest gap counts as on target within this
LENS_GLASS_MIN = 0.5     # lens bottom over the worst-case glass (DESIGN.md 3)
WALL_BOARD_MIN = 0.45    # side wall to the board edge (DESIGN.md 6)
FINGER_POCKET_MIN = 0.1  # choice: finger's outer face to its back-shell pocket (FDM: a 0 fit may not close)
FORK_PLAY_MAX = 0.5      # choice: slider fork play round the lever per side (lost motion of the 1.6 travel)
PANEL_Z0 = 14.17         # the 2090 panel's bottom (module_2090: PCB top)
SWEEP_STEP = 0.25        # (h): mm per step for the bodies swept stepwise (the thinnest moving / fixed feature is >= 0.5)
MIN_WALL = 0.79          # (i): DESIGN.md 6 asks 0.8 (0.01 for the tessellation)
SLAB_DEG = 40.0          # (i): the exit face within this of antiparallel to the ray counts as a slab
DPAD_INFO_SHIFT = 0.5    # (j): also shown (not judged): the cross this far off centre, as without the pivot washer

# (a) overlaps that are intended, with the reason; (pair key = sorted names)
ALLOWED = {}

def _sw1_lever_sweep():
    """the lever part of back_openings._sw1_real(): its swept envelope over both slider ends (2.9 x 1.45, z 1.8..2.8,
    centre 2.675 along footprint +y; numbers as back_openings builds it)."""
    a = math.radians(back_openings.SW1_ROT)
    ox, oy = back_openings.SW1_ORIGIN
    return back_openings._rbox(ox + math.sin(a) * 2.675, oy - math.cos(a) * 2.675, back_openings.SW1_ROT, 2.9, 1.45,
                               SPLIT + 0.2, SPLIT + 1.2)

def _allowed(a, b, parts, ctx):
    """reason an (a) overlap is intended, or None."""
    if tuple(sorted((a, b))) in ALLOWED:
        return ALLOWED[tuple(sorted((a, b)))]
    if {a, b} == {"POWER", "SW1 (real)"}:
        # the real-shape lever is swept over both slider ends, but the fork moves with the lever: allowed if all of the
        # overlap is in the lever sweep (the body is clear); (c) checks the lever at each end
        rest = vol(parts["POWER"], ctx["SW1 (real)"] - _sw1_lever_sweep())
        if rest <= EPS_V:
            return f"all inside SW1's lever sweep (both ends), body overlap {rest:.4f}; the fork carries the lever, see (c)"
    return None

def _bb_hit(a, b, pad=0.0):
    p, q = a.bounding_box(), b.bounding_box()
    return all(lo - pad <= hi2 and lo2 - pad <= hi
               for lo, hi, lo2, hi2 in zip(tuple(p.min), tuple(p.max), tuple(q.min), tuple(q.max)))

def vol(a, b):
    """intersection volume (0 when the bounding boxes miss)."""
    if not _bb_hit(a, b):
        return 0.0
    r = a & b
    return sum(s.volume for s in r.solids()) if r is not None else 0.0

def inside(shape, x, y, z):
    return any(s.is_inside(Vector(x, y, z)) for s in shape.solids())

RESULTS = []             # (check, ok, note)

def verdict(check, ok, note=""):
    RESULTS.append((check, ok, note))
    print(f"  => {check}: {'PASS' if ok else 'FAIL'}{' - ' + note if note else ''}")


# ================================================================ (a) interference matrix
def context_by_ref():
    """{name: solid}: board, every part envelope by ref (J7 / SW1 real shapes), module_2090, battery, speaker, LRA."""
    out = {"board": params.board_solid()}
    for ref, e in sorted(back_openings._envelopes_by_ref().items()):
        if ref == "J7":
            out["J7 (real)"] = back_openings._j7_real()
        elif ref == "SW1":
            out["SW1 (real)"] = back_openings._sw1_real()
        else:
            out[f"env {ref}"] = e
    out.update({"module_2090": params.module_2090(), "battery": params.battery(), "speaker": params.speaker(),
                "lra": params.lra()})
    return out

def check_a(parts, ctx):
    print("\n(a) interference matrix (mm3, > 0.001 reported)")
    names, hits, n = list(parts), [], 0
    for i, a in enumerate(names):
        for b in names[i + 1:] + list(ctx):
            other = parts[b] if b in parts else ctx[b]
            n += 1
            v = vol(parts[a], other)
            if v > EPS_V:
                hits.append((a, b, v))
    bad = 0
    for a, b, v in hits:
        why = _allowed(a, b, parts, ctx)
        bad += why is None
        print(f"  {a} x {b}: {v:.4f}{'  (intended: ' + why + ')' if why else '  <-- INTERFERENCE'}")
    print(f"  {n} pairs ({len(names)} parts x {len(names) - 1} others / 2 + {len(ctx)} context bodies), "
          f"{len(hits)} over {EPS_V}")
    verdict("(a) interference", bad == 0, f"{bad} unexpected overlap(s)" if bad else "")


# ================================================================ (b) keepouts vs other modules' material
def _keepouts():
    """[(owner module, owner control or None, label, solid)]."""
    out = []
    for i, k in enumerate(front_screen.keepouts()):
        out.append(("front_screen", None, ("opening column", "lens pocket", "LED rod", "speaker + 0.3")[i], k))
    own = ["A", "B", "START", "SELECT"] * 2 + ["DPAD"] * 3
    lbl = [f"guide bore {n}" for n in own[:4]] + [f"face hole {n}" for n in own[4:8]] + \
          ["face hole DPAD", "D-pad flange sweep", "D-pad drop column"]
    for k, o, l in zip(front_keys.keepouts(), own, lbl):
        out.append(("front_keys", o, l, k))
    for i, k in enumerate(front_struct.keepouts()):
        out.append(("front_struct", None, "J1 column" if i == 0 else f"insert column {i}", k))
    return out

def _materials(parts):
    """[(module, control or None, label, solid)] of added material."""
    out = []
    for m in (front_screen, front_keys, front_struct):
        for i, s in enumerate(m.adds()):
            out.append((m.__name__, None, f"add {i}", s))
    out.append(("front_struct", None, "j1_cover", parts["j1_cover"]))
    out.append(("back_openings", None, "back shell", parts["back"]))
    for n in controls.all_controls():
        out.append(("controls", "DPAD" if n == "DPAD_WASHER" else n, n, parts[n]))     # the washer is the D-pad's
    return out

# (b) overlaps that are intended: (keepout label, material label) -> reason
KEEPOUT_ALLOWED = {
    ("speaker + 0.3", "back shell"):
        "the back shell's speaker cradle locates the speaker at 0.2 radial (ID 27.4) and carries it on its seat lip, "
        "as front_screen's ring does from the front",
}

def check_b(parts):
    print("\n(b) keepouts vs every other module's added material (mm3, > 0.001 reported)")
    kos, mats = _keepouts(), _materials(parts)
    bad, n = 0, 0
    for ko_mod, ko_ctl, ko_lbl, k in kos:
        for m_mod, m_ctl, m_lbl, s in mats:
            if m_mod == ko_mod or (m_ctl is not None and m_ctl == ko_ctl):
                continue                                        # own module / a control in its own key's keepouts
            n += 1
            v = vol(k, s)
            if v > EPS_V:
                why = KEEPOUT_ALLOWED.get((ko_lbl, m_lbl))
                bad += why is None
                print(f"  {ko_mod} '{ko_lbl}' x {m_mod} '{m_lbl}': {v:.4f}"
                      f"{'  (intended: ' + why + ')' if why else '  <-- IN KEEPOUT'}")
    print(f"  {n} keepout x material pairs")
    # the finished front shell (after every module's cuts): only the plate annulus in each bore keepout (the flange
    # seat between the face hole and the bore, z Z_PLATE_IN..Z_FRONT) may be there
    front = parts["front"]
    for ko_mod, ko_ctl, ko_lbl, k in kos:
        v = vol(k, front)
        exp = 0.0
        if ko_lbl.startswith("guide bore"):
            kk = KEYS[ko_ctl]
            exp = math.pi / 4 * (kk["guide_id"] ** 2 - (kk["d"] + 2 * params.KEY_HOLE_CLR) ** 2) * PLATE
        if abs(v - exp) > EPS_V or v > EPS_V:
            ok = abs(v - exp) <= 0.01
            bad += not ok
            print(f"  finished front shell in {ko_mod} '{ko_lbl}': {v:.4f}"
                  f"{f' (= the plate annulus round the face hole, {exp:.4f}: the flange seat)' if ok else '  <-- IN KEEPOUT'}")
    verdict("(b) keepouts", bad == 0, f"{bad} unexpected overlap(s)" if bad else "")


# ================================================================ (c) rest gaps
def _tip_z(part, x, y, d):
    """lowest z of part within Ø d round (x, y), and the plan centre of that lowest slice."""
    low = part & cyl(x, y, d, 0, KEYSTAKE_Z + 3)
    z = low.bounding_box().min.Z
    sl = (part & cyl(x, y, d, z - 0.01, z + 0.02)).bounding_box()
    return z, ((sl.min.X + sl.max.X) / 2 - x, (sl.min.Y + sl.max.Y) / 2 - y)

def check_c(parts, board):
    print("\n(c) rest gaps")
    ok = True
    for n in ("A", "B", "START", "SELECT", "DPAD"):
        k = KEYS[n]
        want = DPAD_REST_GAP if n == "DPAD" else KEY_REST_GAP
        for cx, cy in k["contacts"]:
            z, (ox, oy) = _tip_z(parts[n], cx, cy, 4.0)
            g = z - KEYSTAKE_Z
            good = abs(g - want) <= TOL and math.hypot(ox, oy) <= 0.05
            ok &= good
            print(f"  {n:6s} tip at ({cx:.2f}, {cy:.2f}): z {z:.3f}, gap to the keystake {g:.3f} (want {want}), "
                  f"centred {math.hypot(ox, oy):.3f}  {'ok' if good else '<-- OFF'}")
    piv = parts["DPAD"].distance_to(board)
    pv = vol(parts["DPAD"], board)
    good = piv < 1e-3 and pv <= EPS_V
    ok &= good
    print(f"  DPAD   pivot leg to the board top: {piv:.4f} (contact), overlap {pv:.4f}  {'ok' if good else '<-- OFF'}")
    w = parts["DPAD_WASHER"]
    dw, dbw = w.distance_to(parts["DPAD"]), w.distance_to(board)
    good = dw > 0 and dbw < 1e-3
    ok &= good
    print(f"  DPAD   pivot washer: to the pivot {dw:.3f} (play {controls.dpad_pivot_play():.3f}), on the board "
          f"{dbw:.4f}  {'ok' if good else '<-- OFF'}")
    for n, e in EDGE_KEYS.items():
        (tx, ty), (ux, uy) = e["tip"], e["dir"]
        tip = Vector(tx, ty, EDGE_AXIS_Z)
        g = parts[n].distance_to(tip)
        along = inside(parts[n], tx + ux * (g + 0.02), ty + uy * (g + 0.02), EDGE_AXIS_Z)   # the face is along dir
        good = abs(g - EDGE_REST_GAP) <= TOL and along
        ok &= good
        print(f"  {n:8s} contact face to the plunger tip along dir: {g:.3f} (want {EDGE_REST_GAP})"
              f"{'' if along else ' NOT along dir'}  {'ok' if good else '<-- OFF'}")
    # slider: at rest (lever at +travel/2) and at the other end; play = how far the lever moves either way along the
    # slot before it meets the fork (bisection), and the clearances to the lever and the switch body
    for sh in (POWER_SLOT["travel"] / 2, -POWER_SLOT["travel"] / 2):
        p = parts["POWER"] if sh == controls.POWER_REST_SHIFT else controls.power_slider(sh)
        lever, body = controls._t_sw1(sh)
        play = []
        for sgn in (1, -1):
            lo, hi = 0.0, 1.5
            for _ in range(14):
                m = (lo + hi) / 2
                lo, hi = (lo, m) if p.distance_to(controls._t_sw1(sh + sgn * m)[0]) < 1e-3 else (m, hi)
            play.append(hi)
        dl, db = p.distance_to(lever), p.distance_to(body)
        vl, vb = vol(p, lever), vol(p, body)
        good = vl <= EPS_V and vb <= EPS_V and dl > 0 and db >= 0.1 and max(play) <= FORK_PLAY_MAX
        ok &= good
        print(f"  POWER at {sh:+.1f}: fork to lever {dl:.3f} (overlap {vl:.4f}), lever play +{play[0]:.3f} / "
              f"-{play[1]:.3f} along the slot (<= {FORK_PLAY_MAX}), to the SW1 body {db:.3f} (overlap {vb:.4f})  "
              f"{'ok' if good else '<-- OFF'}")
    verdict("(c) rest gaps", ok)


# ================================================================ (d) solids + validity
def check_d(parts):
    print("\n(d) solids, validity, print pose")
    ok = True
    for n, p in parts.items():
        pp = enclosure.print_pose(n, p)
        bb = pp.bounding_box()
        good = len(p.solids()) == 1 and p.is_valid and abs(bb.min.Z) < 1e-6 and abs(pp.volume - p.volume) < 0.01
        ok &= good
        print(f"  {n:9s} {len(p.solids())} solid, valid {p.is_valid}, {p.volume:8.1f} mm3; print pose z 0..{bb.max.Z:.2f}"
              f"  {'ok' if good else '<-- BAD'}")
    # the shells' print faces: the front face / back face are the bed
    for n, z_bed in (("front", Z_FRONT), ("back", Z_BACK)):
        pp = enclosure.print_pose(n, parts[n])
        bed = max((f for f in pp.faces() if abs(f.center().Z) < 1e-4 and abs(abs(f.normal_at().Z) - 1) < 1e-6),
                  key=lambda f: f.area, default=None)
        a = bed.area if bed is not None else 0.0
        top = max((f for f in parts[n].faces() if abs(f.center().Z - z_bed) < 1e-4), key=lambda f: f.area,
                  default=None)
        good = bed is not None and top is not None and abs(a - top.area) < 0.01
        ok &= good
        print(f"  {n} print pose: largest bed face {a:.1f} mm2 = its {'front' if n == 'front' else 'back'} face "
              f"{top.area if top else 0:.1f}  {'ok' if good else '<-- BAD'}")
    verdict("(d) solids", ok)


# ================================================================ (e) lens + panel
def check_e(parts):
    print("\n(e) lens pocket, panel")
    front, ok = parts["front"], True
    land = front & prism(front_screen.pocket_face(-0.05), Z_PLATE_IN - 0.5, Z_FRONT + 1)
    floor = land.bounding_box().max.Z
    above = vol(front, prism(front_screen.pocket_face(-0.01), floor + 0.001, Z_FRONT + 1))
    lens_bot = floor + TAPE_T
    margin = lens_bot - GLASS_WORST
    good = margin >= LENS_GLASS_MIN - 1e-6 and above <= EPS_V
    ok &= good
    print(f"  pocket floor z {floor:.3f}; lens bottom (+ tape {TAPE_T}) {lens_bot:.3f}, over GLASS_WORST {GLASS_WORST}: "
          f"{margin:.3f} (>= {LENS_GLASS_MIN}); shell in the pocket above the floor {above:.4f}  "
          f"{'ok' if good else '<-- BAD'}")
    lens = prism(front_screen.lens_outline()[0], lens_bot, lens_bot + LENS["t"])
    lv, ld = vol(front, lens), front.distance_to(lens)
    lt = lens_bot + LENS["t"] - Z_FRONT
    good = lv <= EPS_V and abs(lt) < 1e-6
    ok &= good
    print(f"  lens in its pocket: overlap {lv:.4f}, gap {ld:.3f} (the tape), top {lt:+.3f} to the face (flush)  "
          f"{'ok' if good else '<-- BAD'}")
    cols = {"panel": box((PANEL["x0"] + PANEL["x1"]) / 2, (PANEL["y0"] + PANEL["y1"]) / 2, PANEL["x1"] - PANEL["x0"],
                         PANEL["y1"] - PANEL["y0"], PANEL_Z0, Z_FRONT + 1),
            "FPC fold": box(PANEL["x1"] + FPC_BULGE / 2, (PANEL["y0"] + PANEL["y1"]) / 2, FPC_BULGE,
                            PANEL["y1"] - PANEL["y0"], PANEL_Z0, Z_FRONT + 1)}
    for n, c in cols.items():
        v, d = vol(front, c), front.distance_to(c)
        good = v <= EPS_V and d >= params.CLR - 1e-3
        ok &= good
        print(f"  front shell over the {n} column (z {PANEL_Z0}..face): {v:.4f}, clearance {d:.3f} (>= {params.CLR})  "
              f"{'ok' if good else '<-- BAD'}")
    d = front.distance_to(params.module_2090())
    print(f"  front shell to module_2090 (pins, screw heads, panel): {d:.3f}")
    verdict("(e) lens + panel", ok)


# ================================================================ (f) walls + fingers
def check_f(parts, board):
    print("\n(f) wall to board, locator fingers")
    ok = True
    band = prism(SILHOUETTE, Z_BACK - 1, Z_FRONT + 1) - prism(offset(SILHOUETTE, amount=-(WALL + 0.01)), Z_BACK - 2,
                                                                 Z_FRONT + 2)
    for n in ("front", "back"):
        wall = parts[n] & band
        d = wall.distance_to(board)
        good = d >= WALL_BOARD_MIN
        ok &= good
        print(f"  {n} side wall (outer WALL + 0.01 band) to the board: {d:.3f} (>= {WALL_BOARD_MIN})  "
              f"{'ok' if good else '<-- BAD'}")
    back = parts["back"]
    for k, l in BOARD_LOCATORS.items():
        f = front_struct.finger(l)
        g, gp = f.distance_to(board), f.distance_to(back)
        v = vol(f, back)
        good = abs(g - FINGER["gap"]) <= TOL and v <= EPS_V and gp >= FINGER_POCKET_MIN
        ok &= good
        print(f"  finger {k:11s} to the board edge {g:.3f} (want {FINGER['gap']}), to the back shell {gp:.3f} "
              f"(>= {FINGER_POCKET_MIN}, overlap {v:.4f})  {'ok' if good else '<-- BAD'}")
    verdict("(f) walls + fingers", ok)


# ================================================================ (h) assembly path
def _vertical_only(solid):
    """every face horizontal or vertical (then the downward sweep is the solid + a prism under each bottom face)."""
    for f in solid.faces():
        for u, v in ((0.5, 0.5), (0.1, 0.1), (0.9, 0.9), (0.1, 0.9), (0.9, 0.1)):
            nz = abs(f.normal_at(u, v).Z)
            if 1e-6 < nz < 1 - 1e-6:
                return False
    return True

def _swept(solid, d):
    """solid swept down by d (its positions from rest - d to rest), or None if it isn't vertical-faced."""
    if not _vertical_only(solid):
        return None
    out = solid
    for f in solid.faces():
        if f.normal_at().Z < -0.5:                                  # bottom face
            out = out + Pos(0, 0, -d) * extrude(f, amount=d, dir=(0, 0, 1))
    return out

def _crop(shape, mov, d, pad=0.5):
    b = mov.bounding_box()
    c = box((b.min.X + b.max.X) / 2, (b.min.Y + b.max.Y) / 2, b.size.X + 2 * pad, b.size.Y + 2 * pad,
            b.min.Z - d - pad, b.max.Z + pad)
    r = shape & c
    return r if r is not None and r.solids() else None

def _sweep(name, mov, obstacles, d, step=SWEEP_STEP):
    """[(obstacle, worst mm3, at mm below rest)] for mov swept down d past each obstacle (from 0.05 below rest)."""
    hits = []
    for s in mov.solids():
        sw = _swept(s, d)
        for on, o in obstacles:
            c = _crop(o, s, d)
            if c is None:
                continue
            if sw is not None:
                v = vol(sw, c) - vol(s, c)                          # the rest pose is (a)'s business
                if v > EPS_V:
                    hits.append((on, v, None))
                continue
            worst, at, z = 0.0, 0.0, 0.05
            while z <= d + 1e-9:
                v = vol(Pos(0, 0, -z) * s, c)
                if v > worst:
                    worst, at = v, z
                z += step
            if worst > EPS_V:
                hits.append((on, worst, at))
    return hits

def check_h(parts, ctx):
    print("\n(h) assembly path (DESIGN.md 2, rev 2.1 order; each step along +Z into the rest pose)")
    t0, bad = time.time(), 0
    front = parts["front"]
    keys = ("A", "B", "START", "SELECT", "DPAD")
    edges = tuple(EDGE_KEYS) + ("POWER",)
    board_pack = {"board": ctx["board"], "2090": ctx["module_2090"], "J7 (real)": ctx["J7 (real)"],
                  "SW1 (real)": ctx["SW1 (real)"], "DPAD_WASHER (taped on)": parts["DPAD_WASHER"]}
    for n, e in back_openings._envelopes_by_ref().items():
        if n not in ("J7", "SW1") and e.bounding_box().min.Z >= T - 1e-6:
            board_pack[f"env {n}"] = e
    for n in EDGE_KEYS:
        pl, bd = controls._t_skrt(n)
        board_pack[f"SKRT {n}"] = pl + bd
    steps = [("1. controls into the face-down front shell", [(n, parts[n], 19.0) for n in keys] +
              [(n, parts[n], 6.0) for n in edges], [("front", front)]),
             ("2. speaker into its ring", [("speaker", ctx["speaker"], 14.0)],
              [("front", front)] + [(n, parts[n]) for n in keys]),
             ("3. board (2090 on, washer taped on) onto the boss tips", [(n, b, 22.0) for n, b in board_pack.items()],
              [("front", front)] + [(n, parts[n]) for n in keys + edges] + [("speaker", ctx["speaker"])]),
             ("4. back shell + the cell (plugged into J8, taped in its fence) on", [("back", parts["back"], 18.0),
                                                                                     ("battery", ctx["battery"], 10.0)],
              [("front", front)] + [(n, parts[n]) for n in keys + edges + ("DPAD_WASHER",)] +
              [("speaker", ctx["speaker"])] + list(board_pack.items()))]
    for title, movers, obstacles in steps:
        print(f"  {title}")
        for n, m, d in movers:
            hits = _sweep(n, m, obstacles, d)
            bad += len(hits)
            for on, v, at in hits:
                where = "somewhere on the path (swept prism)" if at is None else f"worst at {at:.2f} below rest"
                print(f"    {n} x {on}: {v:.4f} mm3, {where}  <-- BLOCKED")
        print(f"    {len(movers)} bodies swept past {len(obstacles)} in place: "
              f"{'clear' if not bad else 'see above'}")
    print(f"  ({time.time() - t0:.0f} s)")
    verdict("(h) assembly path", bad == 0, f"{bad} blocked sweep(s)" if bad else "")


# ================================================================ (i) min wall
# thin readings that are intended (part, centre, radius, reason). None today; the back's TR counterbore rim, which
# feathers into the R3 fillet (0.71 on the first layer, cosmetic), is a wedge and so not a slab reading.
THIN_ALLOWED = []

def _thin_scan(shp):
    """[(point, thickness)] of slab readings under MIN_WALL (one per triangle)."""
    from OCP.IntCurvesFace import IntCurvesFace_ShapeIntersector
    from OCP.gp import gp_Lin, gp_Pnt, gp_Dir
    from OCP.BRepAdaptor import BRepAdaptor_Surface
    from OCP.BRepLProp import BRepLProp_SLProps
    verts, tris = shp.tessellate(0.05, 0.3)
    V = np.array([(v.X, v.Y, v.Z) for v in verts])
    Tr = np.array(tris)
    A, B, C = V[Tr[:, 0]], V[Tr[:, 1]], V[Tr[:, 2]]
    N = np.cross(B - A, C - A)
    area = np.linalg.norm(N, axis=1) / 2
    keep = area > 0.002
    A, B, C, N = A[keep], B[keep], C[keep], N[keep]
    N = N / np.linalg.norm(N, axis=1)[:, None]
    P = (A + B + C) / 3
    it = IntCurvesFace_ShapeIntersector()
    it.Load(shp.wrapped, 1e-6)
    cos_slab = math.cos(math.radians(SLAB_DEG))
    out = []
    for p, n in zip(P, N):
        q = p + n * 0.05
        it.Perform(gp_Lin(gp_Pnt(*q), gp_Dir(*(-n))), 0.0, 50.0)
        hits = sorted((it.WParameter(i), i) for i in range(1, it.NbPnt() + 1))
        if len(hits) < 2 or hits[0][0] > 0.15:
            continue
        t = hits[1][0] - hits[0][0]
        if t >= MIN_WALL or t < 1e-4:
            continue
        i = hits[1][1]
        props = BRepLProp_SLProps(BRepAdaptor_Surface(it.Face(i)), it.UParameter(i), it.VParameter(i), 1, 1e-6)
        if not props.IsNormalDefined():
            continue
        m = props.Normal()
        if abs(m.X() * n[0] + m.Y() * n[1] + m.Z() * n[2]) < cos_slab:
            continue                                              # a wedge (taper / chamfer edge), not a slab
        out.append((p, t))
    return out, len(P)

def check_i(parts):
    print(f"\n(i) min wall (slab thickness under {MIN_WALL}; exit face within {SLAB_DEG:.0f} deg of antiparallel)")
    t0, bad = time.time(), 0
    for n, shp in parts.items():
        thin, ntri = _thin_scan(shp)
        clusters = {}
        for p, t in thin:
            k = tuple(np.round(p / 2.0).astype(int))
            if k not in clusters or t < clusters[k][1]:
                clusters[k] = (p, t, clusters.get(k, (0, 0, 0))[2] + 1)
            else:
                clusters[k] = (clusters[k][0], clusters[k][1], clusters[k][2] + 1)
        for p, t, cnt in sorted(clusters.values(), key=lambda r: r[1]):
            why = next((r for pn, c, rad, r in THIN_ALLOWED if pn == n and math.dist(c, p) <= rad), None)
            bad += why is None
            print(f"  {n} {t:.3f} at ({p[0]:.2f}, {p[1]:.2f}, {p[2]:.2f}) [{cnt} tri]"
                  f"{'  (allowed: ' + why + ')' if why else '  <-- THIN'}")
        if not clusters:
            print(f"  {n:11s} {ntri:6d} triangles: none under {MIN_WALL}")
    print(f"  ({time.time() - t0:.0f} s)")
    verdict("(i) min wall", bad == 0, f"{bad} thin spot(s)" if bad else "")


# ================================================================ (j) D-pad rocking
def check_j(parts):
    sm = controls.dpad_shift_max()
    print(f"\n(j) D-pad rocking on its pivot (domed legs; the cross off centre by up to {sm:.3f}: pivot in its washer "
          f"{controls.dpad_pivot_play():.3f} + washer between the switches {controls.WASHER_CLR} + board "
          f"{FINGER['gap']}); hole clr {front_keys.DPAD_HOLE_CLR}")
    print("  press toward  | shift  | 0.3 stop at deg | first obstruction before it | travel | head/flange to plate")
    ok = True
    for phi, lbl in ((0, "SW5 (axial)"), (22.5, "22.5 deg"), (45, "SW5+SW2 diag"), (90, "SW2 (axial)")):
        for sh, judged in ((-sm, True), (0.0, True), (sm, True), (-DPAD_INFO_SHIFT, False), (DPAD_INFO_SHIFT, False)):
            r = controls.dpad_rock(parts["front"], phi, sh, part=parts["DPAD"])
            f = r["first"]
            good = f is None and r["travel"] >= controls.TS_STOP - 1e-3
            if judged:
                ok &= good
            print(f"  {lbl:13s} | {sh:+.3f} | {r['stop_deg']:.2f} | "
                  f"{'none' if f is None else f'{f[0]} at {f[1]:.2f} deg'} | {r['travel']:.3f} | {r['plate_clr']:+.3f}  "
                  f"{('ok' if good else '<-- BINDS') if judged else '(info: no washer)'}")
    verdict("(j) D-pad rocking", ok)


# ================================================================ main
if __name__ == "__main__":
    t0 = time.time()
    parts = enclosure.build()
    ctx = context_by_ref()
    print(f"built {len(parts)} parts + {len(ctx)} context bodies in {time.time() - t0:.1f} s")
    check_a(parts, ctx)
    check_b(parts)
    check_c(parts, ctx["board"])
    check_d(parts)
    check_e(parts)
    check_f(parts, ctx["board"])
    if "--fast" not in sys.argv:
        check_h(parts, ctx)
    check_i(parts)
    check_j(parts)
    print(f"\n(g) summary ({time.time() - t0:.0f} s)")
    for c, good, note in RESULTS:
        print(f"  {'PASS' if good else 'FAIL'}  {c}{' - ' + note if note else ''}")
    print(f"  {'ALL PASS' if all(g for _, g, _ in RESULTS) else 'FAILURES'}")
    sys.exit(0 if all(g for _, g, _ in RESULTS) else 1)
