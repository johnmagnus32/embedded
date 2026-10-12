#!/usr/bin/env python3
"""route.py — gb2's hand-designed routing, rebuilt from the group files in route/groups/.

Each group file (route/groups/<nn>_<name>.py) is a list of explicit polylines and vias written against pad
positions, so a route survives small part moves and reads like a drawing. Everything a group adds goes into a
KiCad group "route:<name>"; a run first deletes the old "route:*" groups it rebuilds (and only those: thermal vias,
zones and anything hand-drawn stay), then re-adds them and refills the zones.

In a group file:
    P("U9.13")                  -> (x, y) of that pad's centre (board mm)
    T(net, layer, w, pts)       track polyline on "F" / "B" (or a KiCad layer name), width w mm
    VIA(net, x, y, d=0.6, h=0.3)  through via (default = the Power/Rail via; 0.45/0.25 for signals)
    Every 90 deg corner in a T() polyline is mitered automatically (two 45 deg turns); see miter().
    ZONE(net, layer, pts, name)    copper pour (solid pad connection, 0.2 mm clearance, islands removed), filled
    nets are exact board names or, for a sheet-local net, the bare name if unique ("BB_FB" = "/buck-boost/BB_FB")

Run:  <kicad python> route/route.py [--board gb2.kicad_pcb] [group ...]      (no groups = all)
"""
import pcbnew, os, sys, glob, subprocess, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import maze

HERE = os.path.dirname(os.path.abspath(__file__))
MM = pcbnew.FromMM; TO = pcbnew.ToMM
def V(x, y): return pcbnew.VECTOR2I(int(round(MM(x))), int(round(MM(y))))
FORCE = False
MITER, MITER_MIN = 2.5, 0.4    # 90 deg corners become two 45s: chamfer leg 2.5 x track width, at least 0.4 mm


def groups_on_disk():
    return {os.path.basename(f)[3:-3]: f for f in sorted(glob.glob(os.path.join(HERE, "groups", "[0-9][0-9]_*.py")))}


def strip(path, names):
    """child process: delete the route:<name> groups and their items (B.Remove last, as always)."""
    B = pcbnew.LoadBoard(path)
    kill = [g for g in B.Groups() if g.GetName() in {"route:" + n for n in names}]
    items = [it for g in kill for it in g.GetItems()]
    for it in items: B.Remove(it)
    for g in kill: B.Remove(g)
    pcbnew.SaveBoard(path, B); sys.stdout.flush(); os._exit(0)


def build(path, names, files):
    B = pcbnew.LoadBoard(path)
    pads = {}
    for fp in B.GetFootprints():
        for p in fp.Pads():
            pads.setdefault(f"{fp.GetReference()}.{p.GetNumber()}", p)
    nets = [str(k) for k in B.GetNetsByName().keys()]
    report, lint = [], []          # lint: shape rules DRC doesn't check (a failing group is not saved)
    pad_xy = {(round(TO(p.GetPosition().x), 2), round(TO(p.GetPosition().y), 2)) for p in pads.values()}
    def miter(pts, w):
        """chamfer every 90 deg corner to two 45 deg ones: chamfer leg MITER x width (>= MITER_MIN), shortened to fit
        the shorter neighbouring segment. A corner sitting on a pad centre is left (the pad hides it)."""
        out = [pts[0]]
        for i in range(1, len(pts) - 1):
            a, b, c = out[-1], pts[i], pts[i + 1]
            u, v = (b[0] - a[0], b[1] - a[1]), (c[0] - b[0], c[1] - b[1])
            lu, lv = math.hypot(*u), math.hypot(*v)
            if lu < 1e-6 or lv < 1e-6 or abs((u[0] * v[0] + u[1] * v[1]) / (lu * lv)) > 0.02 or \
                    (round(b[0], 2), round(b[1], 2)) in pad_xy:
                out.append(b); continue
            k = min(max(MITER * w, MITER_MIN), 0.45 * lu, 0.45 * lv if i + 1 < len(pts) - 1 else 0.9 * lv)
            if k < 0.1:
                lint_warn.append(f"{name}: unmitered 90 deg corner at ({b[0]:.2f},{b[1]:.2f}) (segments too short)")
                out.append(b); continue
            out.append((b[0] - u[0] / lu * k, b[1] - u[1] / lu * k)); out.append((b[0] + v[0] / lv * k, b[1] + v[1] / lv * k))
        out.append(pts[-1]); return out
    lint_warn = []
    for name in names:
        g = pcbnew.PCB_GROUP(B); g.SetName("route:" + name); B.Add(g)
        stats = {"len": 0.0, "vias": 0}
        def NET(n):                     # exact name, or a unique "/<sheet>/<n>" local net
            ni = B.FindNet(n)
            if ni is None:
                hits = [k for k in nets if k.endswith("/" + n)]
                if len(hits) != 1: sys.exit(f"{name}: no net {n!r} on the board" + (f" (ambiguous: {hits})" if hits else ""))
                ni = B.FindNet(hits[0])
            return ni
        def P(key):
            if key not in pads: sys.exit(f"{name}: no pad {key}")
            q = pads[key].GetPosition(); return (TO(q.x), TO(q.y))
        def T(net, layer, w, pts, miter_=True):
            L = {"F": pcbnew.F_Cu, "B": pcbnew.B_Cu}[layer] if layer in ("F", "B") else B.GetLayerID(layer)
            ni = NET(net)
            if miter_: pts = miter(pts, w)
            # lint: every segment >= 0.6 mm on the 45 deg grid (shorter pad stubs may follow the pad)
            for a, b in zip(pts, pts[1:]):
                L_ = ((b[0] - a[0]) ** 2 + (b[1] - a[1]) ** 2) ** 0.5
                if L_ < 0.6: continue
                ang = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0])) % 45
                if min(ang, 45 - ang) > 0.5:
                    lint.append(f"{name}: {net} {L_:.2f} mm segment {min(ang, 45 - ang):.1f} deg off the 45 grid at ({a[0]:.2f},{a[1]:.2f})")
            # lint: no turn sharper than 90 deg (a doubling-back / overshoot), no zero-length segment
            for a, b, c in zip(pts, pts[1:], pts[2:]):
                u, v = (b[0] - a[0], b[1] - a[1]), (c[0] - b[0], c[1] - b[1])
                lu, lv = (u[0] ** 2 + u[1] ** 2) ** 0.5, (v[0] ** 2 + v[1] ** 2) ** 0.5
                if lu < 0.1 or lv < 0.1: continue           # a sub-0.1 mm jog onto a pad centre sits inside the pad
                if (u[0] * v[0] + u[1] * v[1]) / (lu * lv) < -0.02:     # sharper than ~91 deg
                    lint.append(f"{name}: {net} turns back sharper than 90 deg at ({b[0]:.2f},{b[1]:.2f})")
            for a, b in zip(pts, pts[1:]):
                if abs(a[0] - b[0]) < 1e-4 and abs(a[1] - b[1]) < 1e-4: continue     # a maze leg ending on its own start
                t = pcbnew.PCB_TRACK(B); t.SetStart(V(*a)); t.SetEnd(V(*b)); t.SetWidth(MM(w)); t.SetLayer(L)
                t.SetNet(ni); B.Add(t); g.AddItem(t)
                stats["len"] += ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5
        def VIA(net, x, y, d=0.6, h=0.3):
            v = pcbnew.PCB_VIA(B); v.SetViaType(pcbnew.VIATYPE_THROUGH); v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
            v.SetPosition(V(x, y)); v.SetWidth(MM(d)); v.SetDrill(MM(h)); v.SetNet(NET(net)); B.Add(v); g.AddItem(v)
            stats["vias"] += 1
        def ZONE(net, layer, pts, zname, prio=2):
            z = pcbnew.ZONE(B); z.SetZoneName(zname)
            z.SetLayer({"F": pcbnew.F_Cu, "B": pcbnew.B_Cu}[layer] if layer in ("F", "B") else B.GetLayerID(layer))
            z.SetNet(NET(net)); z.SetAssignedPriority(prio); z.SetLocalClearance(MM(0.2)); z.SetMinThickness(MM(0.2))
            z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL); z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
            ol = z.Outline(); ol.NewOutline()
            for x, y in pts: ol.Append(V(x, y))
            B.Add(z); g.AddItem(z)
        def MAZE(box, layers=("F", "B")):
            return maze.Maze(B, box, layers)
        def AUTOROUTE(m, net, a, b, w=0.15, layers="FB", cost=None, via_cost=60.0, through=()):
            """maze-route a -> [through ...] -> b (each (x, y, "F"/"B")), lay it down as T()/VIA() and mark it for
            later routes. `through` waypoints steer a route the search would not find on its own."""
            net = NET(net).GetNetname()                     # the board's full name ("/mcu/NRST"), as the maze sees it
            pts, legs = [], [a, *through, b]
            for k, (u, v) in enumerate(zip(legs, legs[1:])):
                leg = m.route(net, u, v, w, layers, cost, via_cost, relax=(k == 0, k == len(legs) - 2))
                if leg is None:
                    lint.append(f"{name}: maze found no path for {net} {u} -> {v}"); return None
                pts += leg if not pts else leg[1:]
            pts = maze.chamfer(m, net, pts, w)              # miter inside the maze: clearance-checked, and committed as laid
            run = [pts[0]]
            for p in pts[1:]:
                if p[2] != run[-1][2]:                      # layer change: via at the corner
                    T(net, run[-1][2], w, [(q[0], q[1]) for q in run], miter_=False); VIA(net, p[0], p[1], d=maze.VIA_D, h=maze.VIA_H)
                    run = [p]
                else: run.append(p)
            if len(run) > 1: T(net, run[-1][2], w, [(q[0], q[1]) for q in run], miter_=False)
            m.commit(net, pts, w); return pts
        exec(compile(open(files[name]).read(), files[name], "exec"),
             {"P": P, "T": T, "VIA": VIA, "NET": NET, "ZONE": ZONE, "MAZE": MAZE, "AUTOROUTE": AUTOROUTE, "B": B})
        report.append(f"  {name}: {stats['len']:.1f} mm of track, {stats['vias']} vias")
    if lint and not FORCE:
        print("LINT (not saved):\n  " + "\n  ".join(lint)); sys.stdout.flush(); os._exit(1)
    if lint: print("LINT (saved anyway: --force, for inspection only):\n  " + "\n  ".join(lint))
    if lint_warn: report.append("  lint warnings:\n    " + "\n    ".join(lint_warn))
    pcbnew.ZONE_FILLER(B).Fill(B.Zones())
    pcbnew.SaveBoard(path, B)
    print("\n".join(report)); sys.stdout.flush(); os._exit(0)


def main():
    a = sys.argv[1:]
    if a and a[0] == "--strip": return strip(a[1], a[2:])
    global FORCE
    FORCE = "--force" in a
    if FORCE: a.remove("--force")
    board = os.path.join(HERE, "..", "gb2.kicad_pcb")
    if "--board" in a:
        i = a.index("--board"); board = a[i + 1]; del a[i:i + 2]
    board = os.path.abspath(board)
    files = groups_on_disk(); names = a or list(files)
    bad = [n for n in names if n not in files]
    if bad: sys.exit(f"no group file for {bad}; have {list(files)}")
    # the plane fanout and the pours / stitching are rebuilt after routing (fanout.py, pour.py), so their old copper
    # goes too - routes never dodge stale vias
    subprocess.run([sys.executable, __file__, "--strip", board, *names, "fanout", "pour"], check=True)
    build(board, names, files)


if __name__ == "__main__":
    main()
