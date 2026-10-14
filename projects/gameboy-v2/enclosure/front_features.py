"""front_features.py — the front shell's features (DESIGN.md section 3), from three modules:
  front_screen  screen opening, lens pocket + land frame, LED hole + light-pipe tube, speaker grille + locating ring
  front_keys    face holes for the D-pad / A / B / Start / Select, guide tubes
  front_struct  screw bosses + inserts, J1 hatch, board-locating fingers
Each module gives adds() and cuts() (lists of solids, assembly frame); all adds go on first, then every cut, so no
module's addition can fill another's opening. j1_cover() is the printed SWD-hatch cover."""
import front_screen, front_keys, front_struct

MODULES = (front_screen, front_keys, front_struct)

def apply(front):
    for m in MODULES:
        for s in m.adds():
            front = front + s
    for m in MODULES:
        for s in m.cuts():
            front = front - s
    return front

def j1_cover():
    return front_struct.j1_cover()
