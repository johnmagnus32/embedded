#!/usr/bin/env python3
"""post_route_fixes.py — small verified fixes from the routed-board audits. Run after finish_planes.py.

  1. U12 (TPS63021) pin 2 is the control / logic ground and the router left it floating. A 0.25 mm B.Cu
     stub straight down 0.9 mm lands it in the device's own exposed PGND pad, as the datasheet wants
     (control ground tied to PGND at the part). Checked clean against the routed copper.
  2. U18 (BT830) pads carried a 0.102 mm solder-mask margin: 0.096 mm mask webs between pads (JLC wants
     0.10), 12 via holes 0.063-0.078 mm from pad openings, and at the 0.15 mm netclass clearance three
     tracks only 0.050 mm from a pad opening (JLC wants 0.09). Margin 0 fixes all of it.
  3. U4 (microSD socket) is all-SMD apart from two NPTH pegs but was typed through-hole, which drops it
     from an SMD-only placement file.

Re-runnable: the stub is only added if it is not already there, the rest are absolute settings.

Run:  <kicad python> post_route_fixes.py [board.kicad_pcb]      (default gb3.kicad_pcb, in place)
"""
import pcbnew, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
MM = pcbnew.FromMM; TO = pcbnew.ToMM
def V(x, y): return pcbnew.VECTOR2I(int(round(MM(x))), int(round(MM(y))))

U12_STUB = ((137.00, 38.75), (137.00, 39.65), 0.25)     # U12.2 -> into U12.15 (EP), B.Cu


def main():
    path = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "gb3.kicad_pcb")
    B = pcbnew.LoadBoard(path)
    log = []

    u12 = B.FindFootprintByReference("U12")
    pin2 = [p for p in u12.Pads() if p.GetPadName() == "2"][0]
    (x0, y0), (x1, y1), w = U12_STUB
    a, b = V(x0, y0), V(x1, y1)
    exists = any(t.Type() == pcbnew.PCB_TRACE_T and t.GetNetCode() == pin2.GetNetCode()
                 and {(t.GetStart().x, t.GetStart().y), (t.GetEnd().x, t.GetEnd().y)} == {(a.x, a.y), (b.x, b.y)}
                 for t in B.GetTracks())
    if not pin2.HitTest(a):
        log.append("  U12 stub: pin 2 is no longer at (%.2f, %.2f), skipped" % (x0, y0))
    elif exists:
        log.append("  U12 stub: already there")
    else:
        t = pcbnew.PCB_TRACK(B)
        t.SetStart(a); t.SetEnd(b); t.SetWidth(MM(w)); t.SetLayer(pcbnew.B_Cu); t.SetNet(pin2.GetNet())
        B.Add(t)
        log.append("  U12 stub: pin 2 -> EP, %.2f mm B.Cu, %.2f mm long" % (w, TO(t.GetLength())))

    u18 = B.FindFootprintByReference("U18")
    n = 0
    for p in u18.Pads():
        p.SetLocalSolderMaskMargin(0); n += 1
    log.append("  U18: solder-mask margin 0 on %d pads" % n)

    u4 = B.FindFootprintByReference("U4")
    attr = u4.GetAttributes()
    u4.SetAttributes((attr & ~pcbnew.FP_THROUGH_HOLE) | pcbnew.FP_SMD)
    log.append("  U4: attributes 0x%x -> 0x%x (SMD)" % (attr, u4.GetAttributes()))

    pcbnew.ZONE_FILLER(B).Fill(B.Zones())
    pcbnew.SaveBoard(path, B)
    sys.stderr.write("post_route_fixes:\n" + "\n".join(log) + "\n"); sys.stderr.flush(); os._exit(0)


if __name__ == "__main__":
    main()
