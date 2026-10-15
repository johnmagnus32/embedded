"""scratch: the thinnest slab along y between S1's counterbore and the outer back fillet (fine ray grid)."""
import os, sys
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
os.environ.setdefault("GB3_QUIET", "1")
import numpy as np
import params as P
import enclosure as E
import checks as K

parts, ctx = E.build()
back = parts["back"]
x0, y0 = P.BOSSES["S1"]["at"]
zb = P.back_at(x0, y0)
xs = np.arange(x0 - 0.6, x0 + 0.6, 0.02)
zs = np.arange(zb + 0.2, zb + 2.4, 0.02)
X, Z = (a.ravel() for a in np.meshgrid(xs, zs))
res = K.mesh(back).cast(1, X, Z)                     # rays along +y: in-plane (x, z)
L = res["t1"] - res["t0"]
yk = -(res["t0"] + res["t1"]) / 2                     # KiCad y of each slab's middle
sel = (yk > -y0 + P.SCREW["cbore_d"] / 2 - 0.05) & (yk < P.OUTLINE.kicad()[3] + 0.1)   # outline side of the cbore
i = np.argsort(L[sel])[:5]
print(f"[{P.VARIANT}] S1 at KiCad ({x0:.1f}, {-y0:.1f}), back face z {zb:+.2f}; thinnest y-slabs on the outline side:")
for k in i:
    r = res["r"][sel][k]
    print(f"  {L[sel][k]:.3f} at x {X[r]:.2f} z {Z[r]:+.2f} (KiCad y {yk[sel][k]:.2f}; entry cos {res['c0'][sel][k]:.2f}, "
          f"exit cos {res['c1'][sel][k]:.2f})")
