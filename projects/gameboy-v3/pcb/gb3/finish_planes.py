#!/usr/bin/env python3
"""finish_planes.py — close the via-antipad slots in the planes, then re-pour. Run after fanout_planes.py.

Every via kept a pad on both inner layers and every zone cleared other nets by 0.5 mm, so each
other-net via punched a 1.6 mm hole in both planes. The fanout vias beside U1 sit at a 1.8 mm pitch,
just under the 1.85 mm at which those holes merge, so they fenced U1's east and south sides with
6-8.6 mm slots in the +3V3 plane. Measured on the first routed board: PWR voids over 2 mm went
48 -> 9 and GND 21 -> 5 with both changes below.

  * zone clearance 0.5 -> ZONE_CLR on all four pours (still above every netclass clearance)
  * vias drop their pads on layers where nothing connects, keeping the start/end (outer) pads;
    a GND via keeps its GND-plane connection and a +3V3 via its PWR one, those count as connected.
    Non-functional pad removal is routine at JLCPCB.

Re-runnable: both settings are absolute.

Run:  <kicad python> finish_planes.py [board.kicad_pcb]      (default gb3.kicad_pcb, in place)
"""
import pcbnew, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ZONE_CLR = pcbnew.FromMM(0.3)


def main():
    path = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "gb3.kicad_pcb")
    B = pcbnew.LoadBoard(path)
    zones = [z for z in B.Zones() if not z.GetIsRuleArea()]
    for z in zones:
        z.SetLocalClearance(ZONE_CLR)
    vias = [t for t in B.GetTracks() if t.Type() == pcbnew.PCB_VIA_T]
    for v in vias:
        v.SetRemoveUnconnected(True)
        v.SetKeepStartEnd(True)
    pcbnew.ZONE_FILLER(B).Fill(B.Zones())
    pcbnew.SaveBoard(path, B)
    sys.stderr.write("finish_planes: %d zones at %.2f mm clearance, %d vias drop unused inner pads, refilled\n"
                     % (len(zones), pcbnew.ToMM(ZONE_CLR), len(vias)))
    sys.stderr.flush(); os._exit(0)


if __name__ == "__main__":
    main()
