#!/usr/bin/env python3
"""prep_route.py — clear the pre-existing DRC errors that block a clean route, then re-pour copper.

Run this before exporting the Specctra DSN. A stale zone fill is the big one: draw_outline.py pours
copper, but every part moved afterwards, so 686 of the board's 829 baseline violations were pad-vs-zone
against a fill from the OLD positions. Refilling alone clears most of them.

What it changes, all of it re-runnable (absolute values, never deltas):

  1. C33 rotation + position. C33 pad 2 (GND) overlapped R27 pad 2 (SYS) by 0.245 x 0.175 mm -- a
     dead short on the battery rail. C33 was also rot 180, pointing its GND pad at R27 and its hot
     pad away; rot -90 puts the SYS pad toward the shunt where it belongs.
  2. R27 down 0.30 mm. Its pad 1 sat 0.302 mm off the top board edge against a 0.5 mm
     min_copper_edge_clearance rule (severity: error).
  3. The unnamed PTH pads on Audio1/U4/USBC1 -> NPTH. These are connector alignment pegs with the
     hole the same size as the pad, i.e. zero annular ring, so they fail padstack + annular_width +
     hole_clearance and drag the zones into clearance errors too. They are mechanical, not electrical.
  4. Every copper zone's pad connection THERMAL -> FULL. Thermal relief on a 0603 GND pad can only
     resolve one spoke against min_resolved_spokes=2, which is all 76 starved_thermal errors. Solid
     is also the right answer for a plane-grounded handheld, and this board is machine-assembled.
  5. Re-pour all zones.

NOT touched, deliberately:
  * the zone 1 rule area (x 28.20..36.10, y 30.50..39.72) -- that is the BT antenna keepout.
  * U18, whose pad 14 sits 0.13 mm inside that keepout. Whether the module or the antenna cutout is
    in the wrong place is a mechanical call, not a DRC one.
  * USBC1 pads 2/3 at 0.35 mm from the board edge. It is an edge-mount connector that has to line up
    with the case opening; nudging it is a mechanical decision.
  * U10 (11x 0.175 mm) and USBC1 (6x 0.100-0.185 mm) internal pad-to-pad spacing, which is inherent
    to the footprints and inside JLC capability. Needs a clearance override or DRC exclusions.

Run:  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 prep_route.py
"""
import pcbnew, os, sys

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "gb3.kicad_pcb")
MM = pcbnew.FromMM; TO = pcbnew.ToMM
def V(x, y): return pcbnew.VECTOR2I(int(round(MM(x))), int(round(MM(y))))

# ref -> (x, y, rot)  absolute, so re-running is a no-op
MOVES = {
    "C33": (168.80, 44.30, -90),     # hot pad faces R27, GND pad clear of it
    "R27": (168.80, 37.40,  -90),    # +0.30 mm off the top edge
}
NPTH_PARTS = ("Audio1", "U4", "USBC1")   # unnamed zero-annular alignment pegs only


def main():
    B = pcbnew.LoadBoard(PATH)
    log = []

    for ref, (x, y, rot) in MOVES.items():
        fp = B.FindFootprintByReference(ref)
        if not fp:
            log.append("  %-7s MISSING" % ref); continue
        old = (TO(fp.GetPosition().x), TO(fp.GetPosition().y), fp.GetOrientationDegrees())
        fp.SetPosition(V(x, y))
        fp.SetOrientationDegrees(rot)
        log.append("  %-7s (%7.2f,%7.2f) rot%-5.0f -> (%7.2f,%7.2f) rot%-5.0f"
                   % (ref, old[0], old[1], old[2], x, y, rot))

    n_npth = 0
    for ref in NPTH_PARTS:
        fp = B.FindFootprintByReference(ref)
        if not fp: continue
        for p in fp.Pads():
            # an alignment peg: no pad name, no net, plated hole, no annular ring
            if p.GetPadName() or p.GetNetname(): continue
            if p.GetAttribute() != pcbnew.PAD_ATTRIB_PTH: continue
            if p.GetSizeX() > p.GetDrillSizeX() + MM(0.05): continue
            p.SetAttribute(pcbnew.PAD_ATTRIB_NPTH)
            p.SetLayerSet(pcbnew.PAD.UnplatedHoleMask())
            n_npth += 1
            log.append("  %-7s peg @(%.2f,%.2f) drill %.2f -> NPTH"
                       % (ref, TO(p.GetPosition().x), TO(p.GetPosition().y), TO(p.GetDrillSizeX())))

    n_zone = 0
    for z in B.Zones():
        if z.GetIsRuleArea(): continue                 # leave the BT antenna keepout alone
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
        n_zone += 1

    pcbnew.ZONE_FILLER(B).Fill(B.Zones())
    pcbnew.SaveBoard(PATH, B)

    print("prep_route: %d moves, %d pads -> NPTH, %d zones set solid + refilled"
          % (len(MOVES), n_npth, n_zone))
    print("\n".join(log))
    sys.stdout.flush(); os._exit(0)


if __name__ == "__main__":
    main()
