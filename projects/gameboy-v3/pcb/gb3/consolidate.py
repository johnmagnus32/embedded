#!/usr/bin/env python3
"""consolidate.py — one-time cleanup: move the screen+cables sketch (User.2/3/4) UP to wrap the
components (board frame), and delete the clutter (Nintendo Switch ref, old "OUR DEVICE" console grips,
the old/duplicate screen rect, screen_ref remnants). Keeps button circles + place.py's battery/speaker/LRA.

Run AFTER this: place.py, fold_cables.py, draw_console.py (they regenerate battery/speaker/LRA, folds, console
in the board frame). Run once.
"""
import pcbnew, os, sys

PATH = os.path.join(os.path.dirname(__file__), "gb3.kicad_pcb")
TO = pcbnew.ToMM; MM = pcbnew.FromMM
def V(x, y): return pcbnew.VECTOR2I(int(x), int(y))
OFF_X, OFF_Y = MM(-2.1), MM(-100.7)     # sketch centre -> board centre


def poly_pts(d):
    oc = d.GetPolyShape().Outline(0)
    return [(TO(oc.CPoint(i).x), TO(oc.CPoint(i).y)) for i in range(oc.PointCount())]


def main():
    B = pcbnew.LoadBoard(PATH)
    move_ids = {B.GetLayerID(n) for n in ('User.2', 'User.3', 'User.4')}
    dwg = pcbnew.Dwgs_User
    moved = deleted = 0
    for d in list(B.GetDrawings()):
        ln = d.GetLayer(); cls = d.GetClass(); sh = d.GetShape() if cls == 'PCB_SHAPE' else None
        if ln in move_ids:                                   # move screen + raw cables to the board frame
            if cls == 'PCB_SHAPE':
                s, e = d.GetStart(), d.GetEnd()
                d.SetStart(V(s.x + OFF_X, s.y + OFF_Y)); d.SetEnd(V(e.x + OFF_X, e.y + OFF_Y))
            else:
                p = d.GetPosition(); d.SetPosition(V(p.x + OFF_X, p.y + OFF_Y))
            moved += 1
            continue
        if ln != dwg:
            continue
        # representative X / min-Y
        if sh == pcbnew.SHAPE_T_POLY:
            pts = poly_pts(d); maxx = max(p[0] for p in pts); miny = min(p[1] for p in pts)
        elif cls == 'PCB_SHAPE':
            maxx = max(TO(d.GetStart().x), TO(d.GetEnd().x)); miny = min(TO(d.GetStart().y), TO(d.GetEnd().y))
        else:
            maxx = TO(d.GetPosition().x); miny = TO(d.GetPosition().y)
        kill = False
        if maxx > 230:                                       # Nintendo Switch reference (X>230)
            kill = True
        elif sh == pcbnew.SHAPE_T_POLY:                      # old "OUR DEVICE" console grips (only Dwgs POLYs left)
            kill = True
        elif sh == pcbnew.SHAPE_T_RECT and 118 < abs(TO(d.GetEnd().x) - TO(d.GetStart().x)) < 123:
            kill = True                                      # old/duplicate screen rect (~120.7 wide)
        elif miny > 119:                                     # screen_ref remnants below the board
            kill = True
        elif cls == 'PCB_TEXT' and any(k in d.GetText().upper() for k in ('DEVICE', 'SWITCH', 'NINTENDO', 'CONSOLE')):
            kill = True
        if kill:
            B.Remove(d); deleted += 1
    pcbnew.SaveBoard(PATH, B)
    print(f"consolidate: moved {moved} screen/cable shapes, deleted {deleted} clutter shapes")
    sys.stdout.flush(); os._exit(0)


if __name__ == "__main__":
    main()
