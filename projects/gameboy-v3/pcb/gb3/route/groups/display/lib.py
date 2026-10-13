# shared helpers, exec'd first by route.py (poly/via/F/BK/EXTRA are injected by route.py)
import math
R2 = math.sqrt(2.0)
def plen(pts): return sum(math.hypot(b[0]-a[0], b[1]-a[1]) for a, b in zip(pts, pts[1:]))
def solve(f, lo, hi, need, it=40):
    """f(p) -> added length (monotone increasing); returns p in [lo,hi] giving >= need."""
    if need <= 0: return lo
    if f(hi) < need: return hi
    for _ in range(it):
        m = (lo + hi) / 2
        if f(m) >= need: hi = m
        else: lo = m
    return hi
def bump(x, y1, y2, s, W, c=0.3):
    """one U trombone on a northbound vertical at x: out to x+s*W between y1 (south) and y2 (north),
    45-degree corners of size c.  adds 2W - 2.343c of length."""
    return [(x, y1 + c), (x + s * c, y1), (x + s * (W - c), y1), (x + s * W, y1 - c),
            (x + s * W, y2 + c), (x + s * (W - c), y2), (x + s * c, y2), (x, y2 - c)]
