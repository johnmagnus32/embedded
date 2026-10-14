#!/usr/bin/env python3
"""place_moves.py — apply a reviewed placement move list to gb3.kicad_pcb.

Placement changes after the first place.py run are data, like the route groups: one CSV per step in
placement/ (columns ref,x,y,rot,side,why — mm in board coordinates, rot = the footprint orientation
KiCad shows, side F or B). This applies the rows in order, refuses to move the mechanically fixed
parts (FIXED; --allow-fixed to override), refills the zones and saves. Re-runnable: a row that already
matches changes nothing.

Run (KiCad closed):  <kicad python> place_moves.py placement/<step>.csv [board.kicad_pcb] [--dry-run]
"""
import os, sys, csv, math

HERE = os.path.dirname(os.path.abspath(__file__))

# placed by the enclosure / mechanics, not by the electrical layout (confirmed with the user at step 2)
# (SW1 reset, the status LEDs U5/U14 and the LRA/speaker pads J2/J3 may move: user, 2026-10-13)
FIXED = {"P1", "FPC1", "USBC1", "Audio1", "U4", "CN1", "U18", "LRA1", "SPK1"} | {"SW%d" % i for i in range(2, 17)}


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    dry, allow_fixed = "--dry-run" in sys.argv, "--allow-fixed" in sys.argv
    if not args: sys.exit(__doc__)
    moves = [r for r in csv.DictReader(open(args[0], newline="")) if r.get("ref") and not r["ref"].startswith("#")]
    path = os.path.abspath(args[1]) if len(args) > 1 else os.path.join(HERE, "gb3.kicad_pcb")

    import pcbnew
    MM, TO = pcbnew.FromMM, pcbnew.ToMM
    B = pcbnew.LoadBoard(path)
    fps = {fp.GetReference(): fp for fp in B.GetFootprints()}
    bad = [m["ref"] for m in moves if m["ref"] not in fps]
    if bad: sys.exit("not on the board: %s" % ", ".join(bad))
    fixed = [m["ref"] for m in moves if m["ref"] in FIXED]
    if fixed and not allow_fixed: sys.exit("mechanically fixed (use --allow-fixed): %s" % ", ".join(fixed))
    if len({m["ref"] for m in moves}) != len(moves): sys.exit("a ref appears twice in %s" % args[0])

    log, n = [], 0
    for m in moves:
        fp = fps[m["ref"]]
        x, y, rot = float(m["x"]), float(m["y"]), float(m["rot"]) % 360
        layer = {"F": pcbnew.F_Cu, "B": pcbnew.B_Cu}[m["side"].strip().upper()]
        ox, oy = TO(fp.GetPosition().x), TO(fp.GetPosition().y)
        orot, olayer = fp.GetOrientationDegrees() % 360, fp.GetLayer()
        if (abs(ox - x) < 1e-3 and abs(oy - y) < 1e-3 and olayer == layer
                and abs((orot - rot + 180) % 360 - 180) < 0.01):
            continue
        if olayer != layer:
            fp.SetLayerAndFlip(layer)
        fp.SetOrientationDegrees(rot)
        fp.SetPosition(pcbnew.VECTOR2I(int(MM(x)), int(MM(y))))
        n += 1
        log.append("  %-5s %s (%.2f, %.2f) %3.0f° -> %s (%.2f, %.2f) %3.0f°  %.1f mm  %s" % (
            m["ref"], "F" if olayer == pcbnew.F_Cu else "B", ox, oy, orot, m["side"].strip().upper(), x, y, rot,
            math.hypot(x - ox, y - oy), m.get("why", "")))

    if n and not dry:
        pcbnew.ZONE_FILLER(B).Fill(B.Zones())
        pro = os.path.splitext(path)[0] + ".kicad_pro"
        keep = open(pro, "rb").read() if os.path.exists(pro) else None
        pcbnew.SaveBoard(path, B)
        if keep is not None and open(pro, "rb").read() != keep:
            open(pro, "wb").write(keep)                # SaveBoard rewrites the project file; it isn't ours to change
    print("place_moves: %d of %d rows moved a part%s" % (n, len(moves), " (dry run, nothing saved)" if dry else ""))
    print("\n".join(log)); sys.stdout.flush(); os._exit(0)


if __name__ == "__main__":
    main()
