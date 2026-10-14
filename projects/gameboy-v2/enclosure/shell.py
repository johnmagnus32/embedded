"""shell.py — the two shell bodies before any features: the GBA-silhouette slab (Z_BACK..Z_FRONT, rounded outer edges),
hollowed by WALL / FLOOR / PLATE, split at SPLIT into the front shell (prints face-down) and the back shell (prints
back-down). Feature modules cut / add to these (DESIGN.md sections 3-5)."""
from build123d import Axis, Box, Pos, fillet, offset
from params import (SILHOUETTE, Z_FRONT, Z_BACK, SPLIT, WALL, Z_FLOOR_IN, Z_PLATE_IN, R_OUT_FRONT, R_OUT_BACK,
                    prism)

def body():
    b = prism(SILHOUETTE, Z_BACK, Z_FRONT)
    b = fillet(b.edges().group_by(Axis.Z)[-1], R_OUT_FRONT)        # front perimeter
    b = fillet(b.edges().group_by(Axis.Z)[0], R_OUT_BACK)          # back perimeter
    return b

def cavity():
    return prism(offset(SILHOUETTE, amount=-WALL), Z_FLOOR_IN, Z_PLATE_IN)

def shells():
    """(front, back) bare shells."""
    hollow = body() - cavity()
    big = 400
    front = hollow & Pos(95, -70, (SPLIT + Z_FRONT + 5) / 2) * Box(big, big, Z_FRONT + 5 - SPLIT)
    back = hollow & Pos(95, -70, (Z_BACK - 5 + SPLIT) / 2) * Box(big, big, SPLIT - (Z_BACK - 5))
    return front, back

if __name__ == "__main__":
    f, b = shells()
    for n, s in (("front", f), ("back", b)):
        bb = s.bounding_box()
        print(f"{n}: {len(s.solids())} solid(s), z {bb.min.Z:.2f}..{bb.max.Z:.2f}, {bb.size.X:.1f} x {bb.size.Y:.1f}, {s.volume:.0f} mm3")
