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
    ZONE(net, layer, pts, name)    copper pour (solid pad connection, 0.2 mm clearance, islands removed), filled
    nets are exact board names or, for a sheet-local net, the bare name if unique ("BB_FB" = "/buck-boost/BB_FB")

Run:  <kicad python> route/route.py [--board gb2.kicad_pcb] [group ...]      (no groups = all)
"""
import pcbnew, os, sys, glob, subprocess, math

HERE = os.path.dirname(os.path.abspath(__file__))
MM = pcbnew.FromMM; TO = pcbnew.ToMM
def V(x, y): return pcbnew.VECTOR2I(int(round(MM(x))), int(round(MM(y))))


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
        def T(net, layer, w, pts):
            L = {"F": pcbnew.F_Cu, "B": pcbnew.B_Cu}[layer] if layer in ("F", "B") else B.GetLayerID(layer)
            ni = NET(net)
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
                if lu < 1e-6 or lv < 1e-6: continue
                if (u[0] * v[0] + u[1] * v[1]) / (lu * lv) < -0.02:     # sharper than ~91 deg
                    lint.append(f"{name}: {net} turns back sharper than 90 deg at ({b[0]:.2f},{b[1]:.2f})")
            for a, b in zip(pts, pts[1:]):
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
        exec(compile(open(files[name]).read(), files[name], "exec"), {"P": P, "T": T, "VIA": VIA, "NET": NET, "ZONE": ZONE})
        report.append(f"  {name}: {stats['len']:.1f} mm of track, {stats['vias']} vias")
    if lint:
        print("LINT (not saved):\n  " + "\n  ".join(lint)); sys.stdout.flush(); os._exit(1)
    pcbnew.ZONE_FILLER(B).Fill(B.Zones())
    pcbnew.SaveBoard(path, B)
    print("\n".join(report)); sys.stdout.flush(); os._exit(0)


def main():
    a = sys.argv[1:]
    if a and a[0] == "--strip": return strip(a[1], a[2:])
    board = os.path.join(HERE, "..", "gb2.kicad_pcb")
    if "--board" in a:
        i = a.index("--board"); board = a[i + 1]; del a[i:i + 2]
    board = os.path.abspath(board)
    files = groups_on_disk(); names = a or list(files)
    bad = [n for n in names if n not in files]
    if bad: sys.exit(f"no group file for {bad}; have {list(files)}")
    subprocess.run([sys.executable, __file__, "--strip", board, *names], check=True)
    build(board, names, files)


if __name__ == "__main__":
    main()
