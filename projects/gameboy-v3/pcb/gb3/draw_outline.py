#!/usr/bin/env python3
"""draw_outline.py — generate the board's Edge.Cuts outline from a declarative, filleted vertex list.

The outline is the original rounded rect MINUS the cutouts the mechanical sketch calls for (drawn by
hand on User.4 and transcribed here):

  * region A — battery + LRA: X 127.5..181.7 from Y52.3 down to the bottom edge
                speaker:      X 181.7..212.7 from Y87.8 down to the bottom edge
  * region B — cable notch:   X  80.3..127.5 from Y111.0 down to the bottom edge, so the display/touch
                              flexes can fold over the bottom edge instead of wrapping a full 6.8mm of FR4

A and B share the X=127.5 wall, so the result is ONE simply-connected polygon (no holes) — the remaining
board is a left block (full height), a middle block (cut at the bottom), a top strip, and a right block,
all joined.

Every corner is filleted: R18 on the outer corners (step 8, concentric with the console's Switch-like R20; was R8), R2 on
the eight new ones. Concave
corners fillet outward (material bulges into the cutout) exactly as a router bit leaves them, so R2 is
also the min internal radius a 4mm bit needs.

Re-runnable: replaces whatever is on Edge.Cuts, then re-fills the copper zones for the new shape and
audits every pad against the new outline.

Run:  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 draw_outline.py
"""
import pcbnew, os, sys, math

PATH = os.path.join(os.path.dirname(__file__), "gb3.kicad_pcb")
MM = pcbnew.FromMM; TO = pcbnew.ToMM
def V(x, y): return pcbnew.VECTOR2I(int(round(x)), int(round(y)))

R_OUT, R_CUT = 18.0, 2.0    # outer corners (step 8: R18 = the console's R20 less the 2.0 wall + clearance; was R8) / cutout corners
SEG_PER_90 = 16             # arc polygonization (matches the existing R8 corners)

# Clockwise (screen coords, +Y down). (x, y, fillet radius). See the module docstring for the regions.
OUTLINE = [
    ( 22.0,  32.0, R_OUT),   # top-left      (original)
    (212.7,  32.0, R_OUT),   # top-right     (original)
    (212.7,  87.8, R_CUT),   # right edge stops at the speaker cutout      convex
    (181.7,  87.8, R_CUT),   # speaker cutout top -> battery cutout wall   concave
    (181.7,  52.3, R_CUT),   # battery cutout right wall -> top           concave
    (127.5,  52.3, R_CUT),   # battery/LRA cutout top -> left wall        concave
    (127.5, 111.0, R_CUT),   # cutout wall meets the cable notch          convex
    ( 80.3, 111.0, R_CUT),   # cable notch left end                       concave
    ( 80.3, 117.8, R_CUT),   # notch wall meets the bottom edge           convex
    ( 22.0, 117.8, R_OUT),   # bottom-left   (original)
]


def fillet(outline, seg_per_90=SEG_PER_90):
    """Expand a [(x, y, r)] corner list into a dense point list, replacing each corner with a tangent arc.

    For a corner P between neighbours A and B: with phi the angle between the rays P->A and P->B, the arc
    of radius r is tangent to both edges at distance r/tan(phi/2) from P, centred r/sin(phi/2) along the
    bisector. The same formula handles convex and concave corners — for a concave one the bisector points
    out of the material, so the fillet bulges into the cutout (what a router bit actually cuts).
    """
    n = len(outline); pts = []
    for i, (px, py, r) in enumerate(outline):
        ax, ay, _ = outline[(i - 1) % n]
        bx, by, _ = outline[(i + 1) % n]
        ux, uy = ax - px, ay - py; la = math.hypot(ux, uy); ux, uy = ux / la, uy / la
        vx, vy = bx - px, by - py; lb = math.hypot(vx, vy); vx, vy = vx / lb, vy / lb
        phi = math.acos(max(-1.0, min(1.0, ux * vx + uy * vy)))
        if r <= 0 or phi < 1e-6 or abs(phi - math.pi) < 1e-6:
            pts.append((px, py)); continue
        t = r / math.tan(phi / 2)
        t = min(t, la / 2, lb / 2)                     # never eat more than half an edge
        r_eff = t * math.tan(phi / 2)
        bxv, byv = ux + vx, uy + vy; lbis = math.hypot(bxv, byv)
        if lbis < 1e-9:
            pts.append((px, py)); continue
        cx = px + bxv / lbis * (r_eff / math.sin(phi / 2))
        cy = py + byv / lbis * (r_eff / math.sin(phi / 2))
        t1 = (px + ux * t, py + uy * t)                # tangent point on the P->A edge
        t2 = (px + vx * t, py + vy * t)                # tangent point on the P->B edge
        a1 = math.atan2(t1[1] - cy, t1[0] - cx)
        a2 = math.atan2(t2[1] - cy, t2[0] - cx)
        d = (a2 - a1 + math.pi) % (2 * math.pi) - math.pi    # sweep the short way
        steps = max(2, int(round(abs(d) / (math.pi / 2) * seg_per_90)))
        for k in range(steps + 1):                     # t1 -> t2 keeps the walk direction
            a = a1 + d * k / steps
            pts.append((cx + r_eff * math.cos(a), cy + r_eff * math.sin(a)))
    return pts


def main():
    B = pcbnew.LoadBoard(PATH)
    pts = fillet(OUTLINE)

    ch = pcbnew.SHAPE_LINE_CHAIN()
    for x, y in pts: ch.Append(MM(x), MM(y))
    ch.SetClosed(True)
    poly = pcbnew.SHAPE_POLY_SET(); poly.AddOutline(ch)

    # --- read the pad geometry BEFORE mutating (Pads() corrupts after edits) ---
    pads = []
    for fp in B.GetFootprints():
        for pad in fp.Pads():
            p = pad.GetPosition(); s = pad.GetSize()
            pads.append((fp.GetReference(), pad.GetPadName(),
                         p.x - s.x // 2, p.y - s.y // 2, p.x + s.x // 2, p.y + s.y // 2))

    for d in list(B.GetDrawings()):                    # ONE pre-mutation pass
        if d.GetLayer() == pcbnew.Edge_Cuts: B.Remove(d)
    s = pcbnew.PCB_SHAPE(B); s.SetShape(pcbnew.SHAPE_T_POLY)
    s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(MM(0.1)); s.SetPolyShape(poly); B.Add(s)

    pcbnew.ZONE_FILLER(B).Fill(B.Zones())              # re-pour copper for the new shape
    pcbnew.SaveBoard(PATH, B)

    # --- audit: any pad corner outside the new outline ---
    # Re-read the saved board and use GetBoardPolygonOutlines: a raw SHAPE_POLY_SET built by AddOutline
    # is not normalized, and its Contains() answers True everywhere (it silently passes every pad).
    B2 = pcbnew.LoadBoard(PATH)
    bp = pcbnew.SHAPE_POLY_SET(); B2.GetBoardPolygonOutlines(bp, True)
    bad = {}
    for ref, name, l, t, r, b in pads:
        out = sum(0 if bp.Contains(pcbnew.VECTOR2I(int(x), int(y))) else 1
                  for x, y in ((l, t), (r, t), (l, b), (r, b)))
        if out: bad.setdefault(ref, []).append(f"{name}({out}/4)")
    area = bp.Outline(0).Area() / 1e12                 # nm^2 -> mm^2
    print("Edge.Cuts: %d points, board area %.1f mm2" % (len(pts), abs(area)))
    if bad:
        print("PADS OFF BOARD (%d parts):" % len(bad))
        for ref in sorted(bad): print("   %-7s %s" % (ref, " ".join(bad[ref])))
    else:
        print("all pads inside the outline")
    sys.stdout.flush(); sys.stderr.flush()
    os._exit(0)


if __name__ == "__main__":
    main()
