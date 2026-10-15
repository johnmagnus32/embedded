#!/usr/bin/env python3
"""tests/checks/test_checks.py - tests for checks.py on synthetic modules (planted faults), independent of the real
front.py / back.py / controls.py (enclosure.load_modules is replaced for each build).

  good      a minimal front (key holes, notches, pocket) + contract-exact controls: every cap / edge cap reads its
            rest gap, the round keys and edge caps press clear, and the D-pad - its flange on the plate and its pivot
            on the board, as params.KEYS["DPAD"] specifies - is caught jammed by (j)
  dpad gap  the same with the D-pad's flange DPAD_DROP under the plate: (j) lets it rock
  faults    one build with a planted fault per check (rest gaps off, a part in an F envelope, a module in another's
            keep-out and in a core one, a cross-module cut, a two-solid part, a thinned plate, a thin fin, a wall cut
            to 0.2, a bump under the back face, a rib 0.1 over an F part, a visor over the glass, a hook under the board, a 2.1 fence, a hole in
            the speaker's foam seat; the fake front cuts no LED windows)
            - each must be reported where it belongs
  strict    the 0.2 wall again with its cut hidden from the attribution: the strict WALL rule must fire
  real      the real modules: every check runs to a verdict (no exceptions)

Run: .venv/bin/python tests/checks/test_checks.py [good|dpad|faults|strict|real ...]   (default: all)
"""
import os, sys, time, types, traceback
ENCL = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
sys.path.insert(0, ENCL)
os.chdir(ENCL)
os.environ.setdefault("GB3_QUIET", "1")
import params as P
import enclosure
import checks as K
from build123d import Box, Pos

RESULTS = []
DPAD_DROP = 1.3          # mm: a D-pad flange this far under the plate rocks to the stop, diagonals too (~1.1)


def expect(label, cond, info=""):
    RESULTS.append((label, bool(cond)))
    print(f"  {'ok ' if cond else 'BAD'}  {label}" + (f"  [{info}]" if info and not cond else ""))


def build_with(mods):
    """enclosure.build() (uncached) with these module objects {name: namespace(build=...)}."""
    enclosure.load_modules = lambda: dict(mods)
    K.reset()
    return enclosure.build.__wrapped__()


def any_has(lst, *subs):
    return any(all(s in x for s in subs) for x in lst)


# ---------------------------------------------------------------- synthetic modules
def fake_front(extra_cuts=(), extra_adds=(), keepouts=None):
    """the front features the controls need: key holes, SKRT / shoulder notches, the panel pocket."""
    def build(ctx):
        cuts = [P.prism(P.key_hole_face(n), P.Z_PLATE_IN - 0.5, P.Z_FACE + 0.5) for n in P.KEYS]
        for g in (P.EDGE_KEYS, P.SHOULDERS):
            for e in g.values():
                (ox, oy), (ux, uy) = e["outer"], e["dir"]
                w, d = e["notch_w"], P.WALL + 0.6
                cx, cy = ox - ux * (d / 2 - 0.5), oy - uy * (d / 2 - 0.5)
                sx, sy = (d, w) if abs(ux) > abs(uy) else (w, d)
                cuts.append(P.box(cx, cy, sx, sy, e["notch_z"][0] - 0.5, e["notch_z"][1]))
        cuts.append(P.POCKET.prism(P.LEDGE_Z1, P.Z_FACE + 0.5))
        return {"front_cut": cuts + list(extra_cuts), "front_add": list(extra_adds), "keepouts": keepouts or {},
                "notes": ["test: fake front"]}
    return types.SimpleNamespace(build=build)


def cap_round(name, gap=None):
    k = P.KEYS[name]
    x, y = k["at"]
    tip = P.KEYSTAKE_Z + (k["rest_gap"] if gap is None else gap)
    s = P.cyl(x, y, k["d"], k["flange_z"][1], k["top_z"]).fuse(P.cyl(x, y, k["flange_d"], *k["flange_z"]),
                                                              P.cyl(x, y, 2.0, tip, k["flange_z"][0] + 0.1))
    return s.solids()[0]


def dpad(drop=0.0):
    k = P.KEYS["DPAD"]
    cx, cy = k["at"]
    fz0, fz1 = k["flange_z"][0] - drop, k["flange_z"][1] - drop
    head = P.prism(P.face([(cx + x, cy + y) for x, y in P._cross_pts(k["arm"], k["w"])], board_frame=False),
                   fz1 - 0.1, k["top_z"])
    fl = P.prism(P.key_flange_face("DPAD"), fz0, fz1)
    legs = [P.cyl(x, y, 2.0, k["tip_z"], fz0 + 0.1) for x, y in k["contacts"]]
    piv = P.cyl(cx, cy, k["pivot_d"], k["pivot_z"], fz0 + 0.1)
    return head.fuse(fl, piv, *legs).solids()[0]


def edge_cap(e, gap=None):
    (tx, ty), (ux, uy) = e["tip"], e["dir"]
    s0 = e["rest_gap"] if gap is None else gap
    s1 = e["inset"] + e["proud"]
    w = min(e["notch_w"] - 0.4, 3.6)
    c = (s0 + s1) / 2
    cx, cy = tx + ux * c, ty + uy * c
    sx, sy = (s1 - s0, w) if abs(ux) > abs(uy) else (w, s1 - s0)
    return P.box(cx, cy, sx, sy, e["axis_z"] - 1.4, e["axis_z"] + 1.4)


def fake_controls(gaps=None, dpad_drop=0.0, edge_gaps=None):
    gaps, edge_gaps = gaps or {}, edge_gaps or {}

    def build(ctx):
        parts = {n: cap_round(n, gaps.get(n)) for n, k in P.KEYS.items() if k["shape"] == "round"}
        parts["DPAD"] = dpad(dpad_drop)
        for g in (P.EDGE_KEYS, P.SHOULDERS):
            for n, e in g.items():
                parts[n] = edge_cap(e, edge_gaps.get(n))
        return {"parts": parts, "notes": ["test: fake controls"]}
    return types.SimpleNamespace(build=build, print_pose=lambda n, s: s)


def run_checks(parts, ctx, only, fast=True):
    t0 = time.time()
    res = K.run(parts, ctx, fast=fast, only=set(only), verbose=False)
    return {name[1]: (v, d) for name, v, d in res}, time.time() - t0


# ---------------------------------------------------------------- tests
def test_good():
    print("\n[good] minimal front + contract-exact controls")
    parts, ctx = build_with({"front": fake_front(), "controls": fake_controls()})
    r, dt = run_checks(parts, ctx, "cj")
    c, j = K.LAST["c"], K.LAST["j"]
    for n in list(P.KEYS) + list(P.EDGE_KEYS) + list(P.SHOULDERS):
        expect(f"(c) {n} on its rest gap", not any(x.startswith(n + " ") or x.startswith(n + ":") for x in c),
               [x for x in c if x.startswith(n)])
    for n in [n for n, k in P.KEYS.items() if k["shape"] == "round"] + list(P.EDGE_KEYS) + list(P.SHOULDERS):
        expect(f"(j) {n} presses clear", not any(x.startswith(n + " ") or x.startswith(n + ":") for x in j),
               [x for x in j if x.startswith(n)])
    expect("(j) the contract D-pad (flange on the plate, pivot on the board) is jammed", any_has(j, "D-pad toward"),
           j)
    expect("(j) ... by the front shell", any_has(j, "D-pad toward", "front"), j)
    print(f"  ({dt:.1f} s)")


def test_dpad_gap():
    print(f"\n[dpad] the D-pad flange {DPAD_DROP} under the plate")
    parts, ctx = build_with({"front": fake_front(), "controls": fake_controls(dpad_drop=DPAD_DROP)})
    r, dt = run_checks(parts, ctx, "cj")
    expect("(c) the D-pad still reads its rest gaps + pivot", not any_has(K.LAST["c"], "DPAD"), K.LAST["c"])
    expect("(j) the D-pad rocks to the stop", not any_has(K.LAST["j"], "D-pad"), K.LAST["j"])
    print(f"  ({dt:.1f} s)")


def faults_modules():
    xg, yg = P.P(45.0, 95.0)                 # left grip, bare plate: KiCad (45, 95)
    u14 = P.LEDS["U14"]
    keep_box = P.box(100.0, -60.0, 6, 6, 4.0, 6.0)
    plate_pocket = P.box(xg, yg, 6, 6, P.Z_PLATE_IN - 1.0, P.Z_FACE - 0.5)            # leaves 0.5 of plate
    fin = P.box(*P.P(40.0, 105.0), 0.3, 6.0, P.Z_PLATE_IN - 3.0, P.Z_PLATE_IN + 0.1)  # a 0.3 fin under the plate
    wall_cut = P.box(P.OUTLINE.x0 + 0.2 + 1.5, -80.0, 3.0, 6.0, P.SPLIT + 2.0, P.SPLIT + 4.0)   # left wall to 0.2
    rib = P.box(*u14["at"], 1.0, 1.0, u14["top_z"] + 0.1, P.Z_PLATE_IN + 0.1)         # 0.1 over U14
    bump = P.box(*P.P(100.0, 90.0), 5, 5, P.Z_BACK - 1.0, P.Z_FLOOR_IN - 0.6)          # 1.0 under the back face
    fold = P.box(*P.P(100.0, 115.0), 2, 2, P.Z_PLATE_IN - 1.0, P.Z_PLATE_IN + 0.1)     # a rib in the fold bay
    m1_cut = P.box(*P.P(30.0, 45.0), 2, 2, P.Z_PLATE_IN - 1.0, P.Z_PLATE_IN + 0.1)     # cuts m2's post
    m2_post = P.box(*P.P(30.0, 45.0), 1, 1, P.Z_PLATE_IN - 2.0, P.Z_PLATE_IN + 0.1)
    two = Pos(60, -80, 2.5) * Box(1, 1, 1) + Pos(64, -80, 2.5) * Box(1, 1, 1)
    c, bf = P.CELL["rect"], P.CELL["buffer"]
    fence = P.box((bf.x0 - 0.5 + c.x0 - 0.4) / 2, c.cy, (c.x0 - 0.4) - (bf.x0 - 0.5), 20.0, P.Z_FLOOR_IN - 0.1,
                  P.Z_BOARD_BOT)                                                         # a 2.1 fence, 0.4 off the cell
    sx, sy = P.SPEAKER["at"]
    leak = P.cyl(sx + 12.5, sy, 0.8, P.Z_BACK - 1.0, P.Z_FLOOR_IN + 1.0)              # a hole in the foam seat
    blob = P.box(*P.LEDS["U14"]["at"], 1.0, 1.0, 1.7, 2.0)                             # inside U14's envelope
    m1 = types.SimpleNamespace(build=lambda ctx: {
        "front_cut": [plate_pocket, wall_cut, m1_cut], "front_add": [fin, rib], "back_add": [bump, fence],
        "back_cut": [leak],
        "keepouts": {"zone": keep_box}, "parts": {"two solids": two}, "notes": ["test: m1"]})
    g = P.PANEL["glass"]
    visor = P.box(g.cx, g.y1 - 1.0, 20.0, 1.0, P.Z_FACE + 0.1, P.Z_FACE + 0.4)        # over the glass border
    hook = P.box(*P.P(30.0, 90.0), 2.0, 2.0, P.Z_BOARD_BOT - 0.8, P.Z_BOARD_BOT - 0.5)  # under the board's face
    m2 = types.SimpleNamespace(build=lambda ctx: {
        "front_add": [P.box(100.0, -60.0, 2, 2, 4.5, P.Z_PLATE_IN + 0.1), fold, m2_post],
        "parts": {"blob": blob, "visor": visor, "hook": hook}, "notes": ["test: m2"]})
    return m1, m2


def test_faults():
    print("\n[faults] one planted fault per check")
    m1, m2 = faults_modules()
    mods = {"front": fake_front(), "back": m1, "controls": fake_controls(
        gaps={"A": 0.30, "B": 0.05}, edge_gaps={"VOL_UP": 0.30, "VOL_DOWN": 0.05})}
    mods["m2"] = m2
    enclosure_modules = enclosure.MODULES
    parts, ctx = build_with(mods)
    r, dt = run_checks(parts, ctx, "abcdefhx", fast=False)
    L = K.LAST
    expect("(a) a part inside an F envelope", any_has(L["a"], "blob x F:U14"), L["a"][:4])
    expect("(a) the allowed pair is not a failure", not any_has(L["a"], "R29"), L["a"][:4])
    expect("(a3) a rib 0.1 over U14 is short of CLR_F", any_has(L["a"], "F:U14 0.100"), L["a"])
    expect("(b) a module's add in another module's keep-out", any_has(L["b"], "m2 front_add[0] in back:zone"),
           L["b"])
    expect("(b) a module's add in a core keep-out", any_has(L["b"], "m2 front_add[1] in core:fold bay"), L["b"])
    expect("(c) cap A 0.30 over SW7", any_has(L["c"], "A over SW7 gap 0.300"), L["c"])
    expect("(c) cap B 0.05 over SW9", any_has(L["c"], "B over SW9 gap 0.050"), L["c"])
    expect("(c) cap X on target", not any_has(L["c"], "X over"), L["c"])
    expect("(c) VOL_UP 0.30 off its plunger", any_has(L["c"], "VOL_UP gap 0.3"), L["c"])
    expect("(c) VOL_DOWN 0.05 off its plunger", any_has(L["c"], "VOL_DOWN gap 0.05"), L["c"])
    expect("(c) POWER on target", not any_has(L["c"], "POWER"), L["c"])
    expect("(d) a two-solid part", any_has(L["d"], "two solids: 2 solid(s)"), L["d"])
    expect("(e) the plate pocketed to 0.5", any_has(L["e"], "front plate 0.500 < 1.20"), L["e"])
    expect("(e) the 0.3 fin in the slab scan", any_has(L["e"], "front slab 0.300 along x"), L["e"])
    expect("(e) the wall cut to 0.2 (judged at the printable minimum: a module cut)",
           any_has(L["e"], "front left wall at an opening / the split / a module cut 0.200"), L["e"])
    expect("(e) the fold-bay skin is too thick (no fold-bay cut)", any_has(L["e"], "fold-bay skin", "> 0.65"),
           L["e"])
    expect("(e) the ledge is missing (no ledge built)", any_has(L["e"], "ledge top:", "missing"), L["e"])
    expect("(f) the bump under the back face", any_has(L["f"], "under the back face"), L["f"])
    expect("(f) the region with the bump is thicker than the total", any_has(L["f"], "> total"), L["f"])
    expect("(x) m1 cuts m2's post", any_has(L["x"], "back front_cut[2] removes m2 front_add[2]"), L["x"])
    expect("(e) a 2.1 cell fence", any_has(L["e"], "fence left: THICK 2.10"), L["e"])
    expect("(e) a hole in the speaker's foam seat", any_has(L["e"], "speaker floor open outside the grille"), L["e"])
    expect("(a3) the LED windows (none cut by the fake front)", any_has(L["a"], "LED U5 window blocked"), L["a"])
    expect("(h) the panel cannot drop in past the visor", any_has(L["h"], "step 2", "x visor"), L["h"])
    expect("(h) the board cannot come up past a hook under its bottom face", any_has(L["h"], "step 3", "board x hook"),
           L["h"])
    expect("(h) a back-module part above the board cannot come up through it", any_has(L["h"], "step 4",
                                                                                      "two solids x board"), L["h"])
    expect("(f) the visor is over the face", any_has(L["f"], "visor over the face"), L["f"])
    print(f"  ({dt:.1f} s)")
    return parts, ctx


def test_strict(parts, ctx):
    print("\n[strict] the 0.2 wall with its cut hidden from the attribution")
    saved = dict(ctx.modules)
    try:
        ctx.modules = {k: dict(v, front_cut=[]) for k, v in saved.items()}
        rows, bad = [], []
        K._wall_scan(parts, ctx, rows, bad)
        expect("(e) the strict WALL rule fires", any_has(bad, "front left wall 0.200 < 1.50"), bad)
    finally:
        ctx.modules = saved


def test_real():
    print("\n[real] the real modules: every check runs")
    enclosure.load_modules = enclosure.__dict__["_load_modules_orig"]
    K.reset()
    parts, ctx = enclosure.build.__wrapped__()
    res = K.run(parts, ctx, fast=False, verbose=False)
    for name, v, d in res:
        expect(f"{name} ran ({v})", "raised" not in d, d[:200])


if __name__ == "__main__":
    enclosure._load_modules_orig = enclosure.load_modules
    want = set(sys.argv[1:]) or {"good", "dpad", "faults", "strict", "real"}
    t0 = time.time()
    try:
        if "good" in want:
            test_good()
        if "dpad" in want:
            test_dpad_gap()
        if "faults" in want or "strict" in want:
            pc = test_faults()
            if "strict" in want:
                test_strict(*pc)
        if "real" in want:
            test_real()
    except Exception:
        traceback.print_exc()
        RESULTS.append(("exception", False))
    bad = [l for l, ok in RESULTS if not ok]
    print(f"\n{len(RESULTS) - len(bad)} ok, {len(bad)} bad in {time.time() - t0:.0f} s" +
          ("" if not bad else ":\n  " + "\n  ".join(bad)))
    sys.exit(1 if bad else 0)
