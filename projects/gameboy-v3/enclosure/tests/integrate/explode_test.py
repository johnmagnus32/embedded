"""scratch: try exploded-view offsets / angles for enclosure.render() (writes build/integrate/explode_*.png)."""
import os, sys, time
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
os.environ.setdefault("GB3_QUIET", "1")
import enclosure as E
import render as R

OUTD = os.path.dirname(os.path.abspath(__file__))
TRIES = {
    "a": ({"back": -44.0, "back-mounted": -22.0, "board": 0.0, "panel": 18.0, "front": 40.0, "controls": 56.0}, -62, 30),
    "b": ({"back": -50.0, "back-mounted": -25.0, "board": 0.0, "panel": 20.0, "front": 46.0, "controls": 64.0}, -62, 24),
}
parts, ctx = E.build()
for k, (ex, az, el) in TRIES.items():
    E.EXPLODE.clear()
    E.EXPLODE.update(ex)
    t0 = time.time()
    R.view(E._items(parts, ctx, explode=True), os.path.join(OUTD, f"explode_{k}.png"), az=az, el=el, height=1500,
           title=f"try {k}: {ex} az {az} el {el}", legend=E._legend(ctx))
    print(k, f"{time.time() - t0:.1f} s")
