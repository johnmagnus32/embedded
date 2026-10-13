#!/usr/bin/env python3
"""export_dsn.py — Specctra DSN out for FreeRouting, with a keepout band along the board edge.

kicad-cli has no DSN export in KiCad 10, but pcbnew.ExportSpecctraDSN does. It needs the netclasses
resynced first (see main), or every net goes out as kicad_default.

FreeRouting keeps only its own 0.2 mm clearance to the outline, not this board's 0.5 mm
min_copper_edge_clearance, and happily runs traces into the margin (BTN_X ran 50 mm along the notch
at y=52.3). So before exporting, add one track/via keepout quad per outline segment, BAND wide on the
inside, overlapping its neighbours so the fillet corners stay covered. The quads exist only in the
DSN; the board is never saved. The band has a gap at USBC1's shell pads, which sit 0.35 mm from the
edge by design and would otherwise be walled in.

Run:  <kicad python> export_dsn.py [board.kicad_pcb] [out.dsn]
      (defaults: gb3.kicad_pcb -> gb3.dsn beside it; the session name FreeRouting writes is the dsn's
       basename, and KiCad's SES parser rejects a bare number, so keep it a word)
"""
import pcbnew, os, sys, math

HERE = os.path.dirname(os.path.abspath(__file__))
MM = pcbnew.FromMM
BAND = MM(0.5)
GAPS = {("USBC1", "2"), ("USBC1", "3")}           # pads the band must not wall in
GAP_R = MM(2.0)


def main():
    board = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "gb3.kicad_pcb")
    out = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else os.path.join(HERE, "gb3.dsn")
    B = pcbnew.LoadBoard(board)
    # Standalone LoadBoard does not apply the project's netclass pattern assignments: without this every
    # net, SYS and +0V9 included, exports as kicad_default (0.2 mm, 0.6/0.3 via) and the router never
    # sees the Power / HighCurrent widths.
    B.SynchronizeNetsAndNetClasses(False)

    ps = pcbnew.SHAPE_POLY_SET(); B.GetBoardPolygonOutlines(ps, True)
    ol = ps.COutline(0)
    P = [(ol.CPoint(k).x, ol.CPoint(k).y) for k in range(ol.PointCount())]
    gaps = [p.GetPosition() for fp in B.GetFootprints() for p in fp.Pads()
            if (fp.GetReference(), p.GetPadName()) in GAPS]
    cu = pcbnew.LSET()
    for l in B.GetEnabledLayers().CuStack(): cu.AddLayer(l)

    n = 0
    for a, b in zip(P, P[1:] + P[:1]):
        dx, dy = b[0] - a[0], b[1] - a[1]
        L = math.hypot(dx, dy)
        if L == 0: continue
        ux, uy = dx / L, dy / L
        nx, ny = -uy, ux                           # a normal; flip it to point into the board
        mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
        if not ps.Contains(pcbnew.VECTOR2I(int(mx + nx * MM(0.05)), int(my + ny * MM(0.05)))):
            nx, ny = -nx, -ny
        if any(math.hypot(q.x - mx, q.y - my) < GAP_R for q in gaps): continue
        quad = [(a[0] - ux * BAND, a[1] - uy * BAND), (b[0] + ux * BAND, b[1] + uy * BAND),
                (b[0] + ux * BAND + nx * BAND, b[1] + uy * BAND + ny * BAND),
                (a[0] - ux * BAND + nx * BAND, a[1] - uy * BAND + ny * BAND)]
        z = pcbnew.ZONE(B)
        z.SetIsRuleArea(True)
        z.SetDoNotAllowTracks(True); z.SetDoNotAllowVias(True)
        z.SetDoNotAllowPads(False); z.SetDoNotAllowZoneFills(False); z.SetDoNotAllowFootprints(False)
        z.SetLayerSet(cu)
        z.Outline().NewOutline()
        for x, y in quad: z.Outline().Append(int(x), int(y))
        B.Add(z); n += 1

    ok = pcbnew.ExportSpecctraDSN(B, out)
    sys.stderr.write("export_dsn: %s -> %s  ok=%s  edge band quads %d\n" % (os.path.basename(board), out, ok, n))
    sys.stderr.flush(); os._exit(0 if ok else 1)


if __name__ == "__main__":
    main()
