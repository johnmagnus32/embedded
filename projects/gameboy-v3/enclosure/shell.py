"""shell.py - the two bare shells (CONTRACT.md), before any module's features.

The console outline (params.outline_face(): the analytic R7.1 rounded rectangle) as a slab Z_BACK..Z_FACE with
R_FRONT / R_BACK outer fillets, hollowed by WALL (sides), PLATE (front) and FLOOR_PR (back); the cavity's own edges are
filleted R - WALL so the fillet corners stay FLOOR_PR thick. Split at SPLIT (= the board's top face):
  front shell  z SPLIT..Z_FACE   (prints face-down)
  back shell   z Z_BACK_MIN..SPLIT (prints back-down)
GB3_BACK=hump: the back face is Z_BACK outside params.HUMP_RECT and Z_BACK_HUMP over it; the hump's walls are WALL
thick and its floor FLOOR_PR (back.py cuts the cell bay window for the FR4 plate either way).

shells() -> (front, back): build123d Solids labelled "front" / "back", cached.
`.venv/bin/python shell.py` prints both."""
import functools
from build123d import Axis, Pos, Box, fillet
import params as P

R_IN_FRONT = max(P.R_FRONT - P.WALL, 0.0)        # 0.5: keeps the front corner >= PLATE thick
R_IN_BACK = max(P.R_BACK - P.WALL, 0.0)          # 1.5: keeps the back corner >= FLOOR_PR thick
HUMP_PLAN_R = 3.0                                # hump plan corners (the two toward the board)
CUT = 50.0                                       # half-space boxes for the split


def _slab(f, z0, z1, r_top=0.0, r_bot=0.0):
    s = P.prism(f, z0, z1)
    if r_top > 0:
        s = fillet(s.edges().group_by(Axis.Z)[-1], r_top)
    if r_bot > 0:
        s = fillet(s.edges().group_by(Axis.Z)[0], r_bot)
    return s


def _hump_r_bot():
    """the hump's bottom-edge fillet: R_BACK, but no more than its step allows."""
    return min(P.R_BACK, (P.Z_BACK - P.Z_BACK_HUMP) - 0.3)


@functools.lru_cache(maxsize=None)
def body():
    """the outer solid."""
    b = _slab(P.outline_face(), P.Z_BACK, P.Z_FACE, P.R_FRONT, P.R_BACK)
    if P.HUMP:
        r = _hump_r_bot()
        h = P.HUMP_RECT
        # past the outline's bottom edge by HUMP_PLAN_R, so only the two inner plan corners are rounded; trimmed by an
        # outline slab with the same bottom fillet, so the hump's edge along the console's bottom wall is rounded too
        raw = P.Rect(h.x0, h.y0 - HUMP_PLAN_R, h.x1, h.y1).prism(P.Z_BACK_HUMP, P.Z_BACK + P.R_BACK + 0.5, HUMP_PLAN_R)
        raw = fillet(raw.edges().group_by(Axis.Z)[0], r)
        trim = _slab(P.outline_face(), P.Z_BACK_HUMP, P.Z_FACE, 0.0, r)
        b = b.fuse(raw & trim)
    return _one(b, "body")


@functools.lru_cache(maxsize=None)
def cavity():
    """the inside: outline less WALL, from the printed floor's inner face to the plate's (+ the hump's bay)."""
    c = _slab(P.outline_face(P.WALL), P.Z_FLOOR_IN, P.Z_PLATE_IN, R_IN_FRONT, R_IN_BACK)
    if P.HUMP:
        hi = P.HUMP_INNER
        top = P.Z_FLOOR_IN + R_IN_BACK + 0.5                 # past the body cavity's bottom fillet
        c = c.fuse(hi.prism(P.Z_FLOOR_IN_HUMP, top, max(HUMP_PLAN_R - P.WALL, 0.5)))
    return _one(c, "cavity")


def _one(shape, what):
    s = shape.solids()
    assert len(s) == 1, f"shell.{what}: {len(s)} solids"
    return s[0]


@functools.lru_cache(maxsize=None)
def shells():
    """(front, back) bare shells, labelled."""
    hollow = body() - cavity()
    c = P.OUTLINE
    up = Pos(c.cx, c.cy, P.SPLIT + CUT / 2) * Box(c.w + 20, c.h + 20, CUT)
    dn = Pos(c.cx, c.cy, P.SPLIT - CUT / 2) * Box(c.w + 20, c.h + 20, CUT)
    front, back = _one(hollow & up, "front"), _one(hollow & dn, "back")
    front.label, back.label = "front", "back"
    return front, back


if __name__ == "__main__":
    import time
    t0 = time.time()
    f, b = shells()
    for n, s in (("front", f), ("back", b)):
        bb = s.bounding_box()
        print(f"{n}: valid {s.is_valid}, z {bb.min.Z:+.2f}..{bb.max.Z:+.2f}, {bb.size.X:.1f} x {bb.size.Y:.1f}, "
              f"{s.volume:.0f} mm3")
    print(f"{time.time() - t0:.1f} s")
