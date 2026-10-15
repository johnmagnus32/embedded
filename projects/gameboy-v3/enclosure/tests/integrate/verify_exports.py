"""scratch: verify the exports in out/ (or out/variants/<slug>/ via GB3_* flags): every STL / 3MF mesh watertight,
consistently wound, its volume equal to the part's solid volume, on the bed; the STEP read back (labels + solids).
Run: .venv/bin/python tests/integrate/verify_exports.py [--no-step]"""
import os, sys, time
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
os.environ.setdefault("GB3_QUIET", "1")
import zipfile
import xml.etree.ElementTree as ET
import numpy as np
import trimesh
import enclosure as E


def load_3mf(path):
    """one trimesh from a 3MF (stdlib zip + XML: every <mesh> in 3D/3dmodel.model, transforms ignored)."""
    with zipfile.ZipFile(path) as z:
        name = next(n for n in z.namelist() if n.lower().endswith(".model"))
        root = ET.fromstring(z.read(name))
    V, F, base = [], [], 0
    for mesh in root.iter():
        if not mesh.tag.endswith("}mesh"):
            continue
        vs = [(float(v.get("x")), float(v.get("y")), float(v.get("z"))) for v in mesh.iter() if v.tag.endswith("}vertex")]
        ts = [(int(t.get("v1")), int(t.get("v2")), int(t.get("v3"))) for t in mesh.iter() if t.tag.endswith("}triangle")]
        V += vs
        F += [(a + base, b + base, c + base) for a, b, c in ts]
        base += len(vs)
    return trimesh.Trimesh(np.array(V), np.array(F), process=True)

parts, ctx = E.build()
bad = []
print(f"exports in {os.path.relpath(E.OUT, E.HERE)}")
for n, p in parts.items():
    pp = E.print_pose(n, p, ctx)
    fn = n.replace(" ", "_").replace("/", "_")
    stl, tmf = os.path.join(E.OUT, f"{fn}.stl"), os.path.join(E.OUT, f"{fn}.3mf")
    if pp is None:
        if os.path.exists(stl):
            bad.append(f"{n}: bought but has an STL")
        print(f"  {n:14s} bought (STEP only)")
        continue
    m = trimesh.load(stl, force="mesh")
    s3 = load_3mf(tmf)
    rel = abs(m.volume - p.volume) / p.volume
    ok = m.is_watertight and m.is_winding_consistent and rel < 0.01 and abs(m.bounds[0][2]) < 1e-3 and \
        s3.is_watertight and abs(s3.volume - p.volume) / p.volume < 0.01
    print(f"  {n:14s} STL {len(m.faces):7d} tris, watertight {m.is_watertight}, wound {m.is_winding_consistent}, "
          f"vol {m.volume:9.1f} vs solid {p.volume:9.1f} ({100 * rel:.2f} %), z {m.bounds[0][2]:+.3f}..{m.bounds[1][2]:.2f}"
          f" | 3MF watertight {s3.is_watertight}, vol {s3.volume:9.1f}" + ("" if ok else "  <-- BAD"))
    if not ok:
        bad.append(n)
extra = [f for f in os.listdir(E.OUT) if f.endswith((".stl", ".3mf"))
         and f.rsplit(".", 1)[0] not in {n.replace(" ", "_").replace("/", "_") for n in parts}]
if extra:
    print(f"  stale files not from this build: {extra}")
    bad.append(f"stale {extra}")
if "--no-step" not in sys.argv:
    from build123d import import_step
    t0 = time.time()
    a = import_step(os.path.join(E.OUT, "assembly.step"))
    labels = [c.label for c in a.children]
    print(f"  assembly.step: {os.path.getsize(os.path.join(E.OUT, 'assembly.step')) / 1e6:.1f} MB read back in "
          f"{time.time() - t0:.1f} s: {len(a.solids())} solids, {len(labels)} top-level children")
    want = set(parts) | {n for n in ctx.context if n[:2] not in ("F:", "B:")} | {"F parts", "B parts"}
    import re
    san = lambda t: re.sub(r"[^A-Za-z0-9]", "_", t)          # build123d's import_step sanitises labels like this
    got = {san(l) for l in labels}
    miss = sorted(w for w in want if san(w) not in got)
    print(f"  missing top-level labels: {miss or 'none'}")
    if miss:
        bad.append(f"STEP missing {miss}")
print(f"{len(bad)} problem(s)" + (": " + "; ".join(bad) if bad else ""))
sys.exit(1 if bad else 0)
