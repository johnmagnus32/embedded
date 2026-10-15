#!/usr/bin/env python3
"""tests/back/test_back.py - the back module's own tests (back.py alone on the bare shells; front.py's keep-outs are
read if front.py imports). Run from the enclosure folder:  .venv/bin/python tests/back/test_back.py [--json out.json]

  1. the finished back shell / front rim / battery plate vs every context body (overlaps > EPS_V)
  2. back.py's adds + plate vs the core keep-outs and front.py's / controls.py's keep-outs; the finished back shell vs
     the core keep-outs (the jack plug is front.py's back_cut: reported, not counted)
  3. clearances (exact distances) for every back feature
  4. probes: points that must be void (openings) or solid (the walls round them)
Prints PASS / FAIL lines; exit 1 on any FAIL."""
import json, math, os, sys, time
HERE = os.path.dirname(os.path.abspath(__file__))
ENC = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, ENC)
os.chdir(ENC)
os.environ.setdefault("GB3_QUIET", "1")
from build123d import Vector
import params as P
import enclosure
import back as B

EPS_V = 0.001
TOL = 0.005
RES = []


def rec(name, ok, detail=""):
    RES.append((name, "PASS" if ok else "FAIL", detail))
    print(f"{'PASS' if ok else 'FAIL'}  {name}: {detail}")


def bbh(a, b, pad=0.0):
    p, q = a.bounding_box(), b.bounding_box()
    return all(lo - pad <= hi2 and lo2 - pad <= hi for lo, hi, lo2, hi2 in
               zip(tuple(p.min), tuple(p.max), tuple(q.min), tuple(q.max)))


def vol(a, b):
    if not bbh(a, b):
        return 0.0
    try:
        r = a & b
    except Exception:
        return float("nan")
    return sum(s.volume for s in r.solids()) if r is not None else 0.0


def dist(a, b):
    return a.distance_to(b)


def void(shape, x, y, z):
    return not any(s.is_inside(Vector(x, y, z)) for s in shape.solids())


def main():
    t0 = time.time()
    ctx = enclosure.make_ctx()
    r = B.build(ctx)
    back = ctx.back.fuse(*r["back_add"]).clean().cut(*r["back_cut"]).clean()
    back = back.solids()[0] if len(back.solids()) == 1 else back
    front = ctx.front.cut(*r["front_cut"]).clean() if r["front_cut"] else ctx.front
    front = front.solids()[0] if len(front.solids()) == 1 else front
    print(f"[{P.VARIANT}] built in {time.time() - t0:.1f} s")
    rec("(d) back shell one valid solid", len(back.solids()) == 1 and back.is_valid, f"{len(back.solids())} solids")
    rec("(d) front shell (with back.py's cuts) one valid solid", len(front.solids()) == 1 and front.is_valid,
        f"{len(front.solids())} solids")
    for n, p in r["parts"].items():
        rec(f"(d) part {n}", len(p.solids()) == 1 and p.is_valid, f"{len(p.solids())} solids")

    # ---- 1. vs context
    bodies = {n: s for n, (s, _) in ctx.context.items()}
    hits = []
    for label, s in [("back", back), ("front", front)] + [(f"part {n}", p) for n, p in r["parts"].items()]:
        for n, b in bodies.items():
            v = vol(s, b)
            if v > EPS_V or v != v:
                hits.append((label, n, v))
    expected = {("back", "jack"), ("back", "panel LCM"), ("front", "jack"), ("front", "panel glass"),
                ("front", "panel touch"), ("front", "panel active area"), ("front", "touch flex"),
                ("front", "display flex"), ("front", "panel LCM")}      # front.py's openings (not applied here)
    mine = [h for h in hits if (h[0], h[1]) not in expected]
    rec("(a) back features vs context", not mine,
        "; ".join(f"{a} x {b} {v:.4f}" for a, b, v in mine) or
        f"clear (front.py's openings, not applied here: {', '.join(f'{a} x {b}' for a, b, v in hits) or 'none'})")

    # ---- 2. keep-outs
    kos = {f"core:{n}": s for n, s in ctx.core_keepouts.items()}
    try:
        import front as F
        kos.update({f"front:{n}": s for n, s in F.keepouts().items()})
    except Exception as e:
        print(f"  (front.py keep-outs not read: {e})")
    try:
        import controls as K
        rk = K.build(ctx).get("keepouts", {})
        kos.update({f"controls:{n}": s for n, s in rk.items()})
    except Exception as e:
        print(f"  (controls.py keep-outs not read: {e})")
    bad = []
    mats = [(f"back_add[{i}]", s) for i, s in enumerate(r["back_add"])] + [(f"part {n}", s) for n, s in
                                                                             r["parts"].items()]
    for kn, k in kos.items():
        for mn, s in mats:
            v = vol(s, k)
            if v > EPS_V:
                bad.append(f"{mn} in {kn} {v:.4f}")
    rec("(k) back.py material vs core / front / controls keep-outs", not bad, "; ".join(bad) or f"{len(kos)} clear")
    sh = []
    for kn, k in kos.items():
        if not kn.startswith("core:"):
            continue
        v = vol(back, k)
        if v > EPS_V:
            sh.append(f"back in {kn} {v:.4f}")
    rest = [s for s in sh if "jack plug" not in s]
    rec("(k) finished back shell vs the core keep-outs", not rest,
        "; ".join(sh) + (" (jack plug: front.py's back_cut)" if sh else "") if sh else "clear")
    # back.py's cuts must not remove another module's adds: cut volume vs front.py adds
    try:
        import front as F
        fadds = F.adds()
        xs = []
        for i, c in enumerate(r["front_cut"]):
            for j, a in enumerate(fadds):
                v = vol(c, a)
                if v > EPS_V:
                    xs.append(f"front_cut[{i}] x front_add[{j}] {v:.4f}")
        rec("(x) back.py front cuts vs front.py adds", not xs, "; ".join(xs) or "none")
    except Exception as e:
        print(f"  (front.py adds not read: {e})")

    # ---- 3. clearances
    c = P.CELL
    board = bodies["board"]
    fen = B.fences()
    rows = []

    def need(label, a, b, req, cmp=">="):
        g = dist(a, b)
        ok = g >= req - TOL if cmp == ">=" else abs(g - req) <= TOL
        rows.append((label, g, req, ok))
        rec(f"(c) {label}", ok, f"{g:.3f} {'/' if cmp == '>=' else '=='} {req:.3f}")
        return g

    need("fence to the cell", fen, bodies["cell"], c["fence_clr"])
    need("fence to the board", fen, board, 0.3)
    need("fence to the LRA", fen, bodies["lra"], 0.3)
    for n in ("H1", "H2", "H3", "H4", "H5", "S1", "S2"):
        bos = B.screws()[n][0]
        bp = [s for k, s in bodies.items() if k.startswith("B:") and bbh(s, bos, 1.0)]
        if bp:
            g = min(dist(bos, s) for s in bp)
            rec(f"(c) {n} boss to the nearest B part", g >= 0.3 - TOL, f"{g:.3f} / 0.300")
        if P.BOSSES[n]["on_board"]:
            need(f"{n} boss to the board (BACK_BOSS_GAP)", bos, board, P.BACK_BOSS_GAP, "==")
    for p, (nm, xy) in zip(B.posts(), B.post_points().items()):
        need(f"post {nm} to the board", p, board, P.BACK_BOSS_GAP, "==")
        bp = [s for k, s in bodies.items() if k.startswith("B:") and bbh(s, p, 1.0)]
        if bp:
            g = min(dist(p, s) for s in bp)
            rec(f"(c) post {nm} to the nearest B part", g >= 0.3 - TOL, f"{g:.3f}")
    need("speaker cup to the speaker", B.speaker_cup(), bodies["speaker"], (P.SPEAKER["cup_id"] - P.SPEAKER["d"]) / 2)
    need("speaker cup bridged to the S2 boss (gap 0.3 < MIN_GAP)", B.speaker_cup(), B.screws()["S2"][0], 0.0, "==")
    if not P.HUMP:                                      # (hump: S1 and the fence both fuse into the hump wall)
        need("fence (root chamfer) to the S1 boss", fen, B.screws()["S1"][0], B.MIN_GAP)
    if B.SNAP:
        g = B._snap_geo()
        rec("(c) snap slot >= MIN_GAP", g["yb0"] - g["y_in"] >= B.MIN_GAP - TOL, f"{g['yb0'] - g['y_in']:.3f}")
    need("speaker cup to the board", B.speaker_cup(), board, 0.3)
    need("speaker cup to the fence", B.speaker_cup(), fen, 0.3)
    need("LRA ring to the LRA", B.lra_ring(), bodies["lra"], (P.LRA["ring_id"] - P.LRA["d"]) / 2)
    tube, hole = B.reset_parts()
    need("reset tube to SW1", tube, bodies["B:SW1"], B.RESET_GAP - (P.RESET["keystake_z"] - P.PARTS["SW1"]["z_design"][0]))
    if c["floor"] == "fr4":
        pl = r["parts"]["battery plate"]
        need("battery plate to the back shell", pl, back, B.PLATE_BOND)
        need("battery plate to the cell", pl, bodies["cell"], P.CELL_TAPE)
        bb = pl.bounding_box()
        rec("(c) battery plate flush with the back face", abs(bb.min.Z - P.Z_BACK_MIN) <= TOL,
            f"plate bottom {bb.min.Z:+.3f}, back face {P.Z_BACK_MIN:+.3f}")
    if B.SNAP:
        hook, pocket, sweep = B.snap()
        need("snap hook to the board", hook, board, 0.3)
        need("snap hook to the display flex", hook, bodies["display flex"], 0.5)
        need("snap sweep to the display flex", sweep, bodies["display flex"], 0.2)
        need("snap hook to the front shell (rest: barb on the catch edge)", hook, front, 0.0, "==")
        v = vol(hook, front)
        rec("(c) snap hook vs the front shell at rest", v <= EPS_V, f"overlap {v:.5f}")
        fb = ctx.core_keepouts["fold bay"]
        need("snap sweep to the fold bay keep-out", sweep, fb, 0.3)

    # ---- 4. probes (void / solid)
    u, m = P.USBC, P.MICROSD
    yo = P.OUTLINE.y1
    ymid = yo - P.WALL / 2
    pr = []

    def probe(label, shape, x, y, z, want_void):
        v = void(shape, x, y, z)
        pr.append((label, v == want_void))
        rec(f"(p) {label}", v == want_void, f"({x:.2f}, KiCad y {-y:.2f}, z {z:+.2f}) {'void' if v else 'solid'}")

    probe("USB-C axis mid-wall void", back, u["x"], ymid, u["axis_z"], True)
    probe("USB-C rim opening void (z 1.2)", back, u["x"], ymid, P.SPLIT - 0.4, True)
    w = B.usbc_stadium()[0] / 2
    need("USB-C receptacle to the back shell", bodies["usb_c"], back, min(B.USBC_CLR, float(P.CLR_B)))
    probe("USB-C wall beside the stadium solid", back, u["x"] + w + 0.4, yo - u["relief"][2] - 0.4, u["axis_z"], False)
    probe("USB-C relief void (0.15 in)", back, u["x"] + w + 0.4, yo - 0.15, u["axis_z"], True)
    probe("USB-C behind the relief solid", back, u["x"] + w + 0.4, yo - 0.5, u["axis_z"], False)
    rw, rh, rd = B.relief_size()
    if u["axis_z"] + rh / 2 > P.SPLIT:
        top = u["axis_z"] + rh / 2
        probe("USB-C relief cut from the front rim", front, u["x"] + 2.0, yo - 0.15, (P.SPLIT + top) / 2, True)
        probe("front rim behind the relief solid", front, u["x"] + 2.0, yo - 0.6, (P.SPLIT + top) / 2, False)
        probe("front rim above the relief solid", front, u["x"] + 2.0, yo - 0.15, top + 0.2, False)
    probe("USB-C relief corner void (13 x 7 box corner)", back, u["x"] + u["relief"][0] / 2 - 0.02, yo - 0.1,
          u["axis_z"] - u["relief"][1] / 2 + 0.02 if u["axis_z"] - u["relief"][1] / 2 + 0.02 > P.back_at(u["x"], yo - 3) + 3
          else u["axis_z"] + 1.5, True)
    zc = sum(m["card_z"]) / 2
    probe("microSD slot void", back, m["x"], ymid, zc, True)
    probe("microSD wall above the scallop solid", back, m["x"], yo - 0.2, P.SPLIT - 0.3, False)
    probe("microSD wall behind the scallop solid", back, m["x"] + 6.5, yo - B.MSD_SCALLOP[2] - 0.2, zc + 1.2, False)
    probe("microSD scallop void", back, m["x"] + 6.0, yo - 0.3, zc + 1.5, True)
    rx, ry = P.RESET["at"]
    probe("reset pinhole void (in the floor)", back, rx, ry, P.back_at(rx, ry) + 0.6, True)
    probe("reset tube wall solid", back, rx + 1.2, ry, P.floor_in_at(rx, ry) + 1.0, False)
    for n, b in P.BOSSES.items():
        x, y = b["at"]
        probe(f"{n} counterbore void", back, x + 1.6, y, P.back_at(x, y) + 1.0, True)
        probe(f"{n} hole void", back, x, y, b["back_top_z"] - 0.5, True)
        probe(f"{n} boss solid", back, x + 2.3, y, b["back_top_z"] - 0.5, False)
    if c["floor"] == "fr4":
        probe("cell window void (over the plate)", back, c["rect"].cx, c["rect"].cy, c["floor_z"][1] + 0.1, True)
        probe("plate pocket void", back, c["rect"].cx, c["rect"].cy, c["floor_z"][0] + 0.5, True)
        pk = B.pocket_rect()
        probe("lip under the left fence solid", back, c["rect"].x0 - FCm(), c["rect"].cy, c["floor_z"][1] + 0.1, False)
        probe("floor beyond the pocket solid", back, pk.x0 - 0.3, c["rect"].cy, c["floor_z"][0] + 0.5, False)
    else:
        probe("printed floor under the cell solid", back, c["rect"].cx, c["rect"].cy, c["floor_z"][1] - 0.5, False)
    s = P.SPEAKER
    probe("grille hole void", back, s["at"][0], s["at"][1], P.back_at(*s["at"]) + 0.6, True)
    probe("floor between grille holes solid", back, s["at"][0] + B.GRILLE_PITCH / 2, s["at"][1],
          P.back_at(*s["at"]) + 0.6, False)
    if B.SNAP:
        g = B._snap_geo()
        probe("snap slot void (blade free)", back, g["x"], (g["y_in"] + g["yb0"]) / 2, B.SNAP_ROOT_Z + 0.5, True)
        probe("snap blade solid", back, g["x"], (g["yb0"] + g["yb1"]) / 2, P.SPLIT + 0.5, False)
        probe("snap pocket in the front wall void", front, g["x"], g["y_in"] - 0.2, g["zc"] + 0.1, True)
        probe("front wall behind the pocket solid", front, g["x"], g["y_tip"] - 0.3, g["zc"], False)

    # ---- numbers
    print("\nnumbers")
    for n, (_, _, info) in B.screws().items():
        print(f"  {n}: wall gap {info['gap']:+.2f} ({'web' if info['web'] else 'free'}), top z {info['top']:+.2f}, "
              f"head bears z {info['bearing']:+.2f}, M2 x {info['L']} tip z {info['tip']:+.2f} ({info['engage']:.1f} in "
              f"the insert)")
    print(f"  plate edge rise: {B.plate_edge_rise()}")
    fails = sum(v == "FAIL" for _, v, _ in RES)
    print(f"\n{len(RES) - fails} PASS, {fails} FAIL in {time.time() - t0:.1f} s [{P.VARIANT}]")
    if "--json" in sys.argv:
        json.dump({"variant": P.VARIANT, "results": RES}, open(sys.argv[sys.argv.index("--json") + 1], "w"), indent=1)
    return fails


def FCm():
    return P.CELL["fence_clr"] + 0.2                     # a point under the fence's inner part (over the lap band)


if __name__ == "__main__":
    sys.exit(1 if main() else 0)
