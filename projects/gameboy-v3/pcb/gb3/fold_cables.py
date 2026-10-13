#!/usr/bin/env python3
"""fold_cables.py — fold the display/touch flex outlines (User.4 / User.3) back over the screen to
show where their contacts LAND after the 180 deg fold onto the PCB.

Model: mirror each cable about the screen's bottom edge (User.2 bottom, Y=SB), then account for the
length consumed by the fold by shifting the mirror DOWN by `loss` (contacts land 53.2-loss above the
edge). Finally clip off the part that falls below the edge (the fold excursion / "chop the bottom").

  loss = (cable-exit -> back of screen) + (screen-bottom -> PCB-top gap) + (bend)
  display (folds to PCB TOP/front): 1.5 + 2.0 + 2.0 = 5.5 mm
  touch   (wraps to PCB BACK):      3.0 + 2.0 + 2.0 = 7.0 mm     (bend = estimate, tune freely)

Folded landings are drawn on Cmts.User (re-runnable: clears Cmts.User first).
Run:  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 fold_cables.py
"""
import pcbnew, os, sys

PATH = os.path.join(os.path.dirname(__file__), "gb3.kicad_pcb")
MM = pcbnew.FromMM; TO = pcbnew.ToMM
def V(x, y): return pcbnew.VECTOR2I(int(round(x)), int(round(y)))

BEND = 2.0                 # 180 deg fold length loss (estimate), mm
LOSS = {'User.4': 1.5 + 2.0 + BEND,   # display -> PCB top/front
        'User.3': 3.0 + 2.0 + BEND}   # touch   -> PCB back


def main():
    B = pcbnew.LoadBoard(PATH)
    src = {n: B.GetLayerID(n) for n in ('User.3', 'User.4')}
    dst = B.GetLayerID('Cmts.User')
    u2 = B.GetLayerID('User.2')

    # read source cable segments + the screen (User.2) bottom edge up front (mm)
    segs = {n: [] for n in src}
    SB = None
    for d in B.GetDrawings():
        if d.GetLayer() == u2 and d.GetClass() == 'PCB_SHAPE' and d.GetShape() == pcbnew.SHAPE_T_RECT:
            SB = max(TO(d.GetStart().y), TO(d.GetEnd().y))     # screen bottom edge = fold line
        for n, lid in src.items():
            if d.GetLayer() == lid and d.GetClass() == 'PCB_SHAPE' and d.GetShape() == pcbnew.SHAPE_T_SEGMENT:
                a, b = d.GetStart(), d.GetEnd()
                segs[n].append((TO(a.x), TO(a.y), TO(b.x), TO(b.y)))
    if SB is None:
        sys.exit("no User.2 screen rect found")

    # clear prior folded landings on Cmts.User
    for d in list(B.GetDrawings()):
        if d.GetLayer() == dst:
            B.Remove(d)

    def clip(x1, y1, x2, y2, ymax):          # keep the y<=ymax part of a segment (chop below the edge)
        if y1 <= ymax and y2 <= ymax: return (x1, y1, x2, y2)
        if y1 > ymax and y2 > ymax: return None
        t = (ymax - y1) / (y2 - y1); xi = x1 + t * (x2 - x1)
        return (x1, y1, xi, ymax) if y1 <= ymax else (xi, ymax, x2, y2)

    def seg(x1, y1, x2, y2, w=0.15):
        s = pcbnew.PCB_SHAPE(B); s.SetShape(pcbnew.SHAPE_T_SEGMENT); s.SetLayer(dst); s.SetWidth(MM(w))
        s.SetStart(V(MM(x1), MM(y1))); s.SetEnd(V(MM(x2), MM(y2))); B.Add(s)

    tops = {}
    for n in ('User.4', 'User.3'):
        loss = LOSS[n]; ytop = 9e9
        for (x1, y1, x2, y2) in segs[n]:
            # mirror about SB, then shift down by loss:  y -> 2*SB - y + loss
            m = (x1, 2 * SB - y1 + loss, x2, 2 * SB - y2 + loss)
            c = clip(*m, SB)
            if c:
                seg(*c); ytop = min(ytop, c[1], c[3])
        tops[n] = ytop

    def label(x, y, s):
        t = pcbnew.PCB_TEXT(B); t.SetText(s); t.SetLayer(dst)
        t.SetPosition(V(MM(x), MM(y))); t.SetTextSize(V(MM(1.5), MM(1.5))); t.SetTextThickness(MM(0.2))
        t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT); B.Add(t)
    label(60, tops['User.4'] - 3, "DISPLAY cable LANDING (folded to PCB top, loss %.1f)" % LOSS['User.4'])
    label(126, tops['User.3'] - 3, "TOUCH LANDING (folded to PCB back, loss %.1f)" % LOSS['User.3'])

    pcbnew.SaveBoard(PATH, B)
    print("folded on Cmts.User | display contacts top Y=%.1f (%.1f above edge) | touch top Y=%.1f (%.1f above edge)"
          % (tops['User.4'], SB - tops['User.4'], tops['User.3'], SB - tops['User.3']))
    sys.stdout.flush(); os._exit(0)


if __name__ == "__main__":
    main()
