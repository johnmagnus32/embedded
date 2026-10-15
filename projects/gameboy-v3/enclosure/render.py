"""render.py - headless PNGs without a GPU or the OCP viewer (enclosure.render() calls these).

view(items, path, ...)     3D view: a numpy z-buffer rasteriser over build123d tessellations - orthographic, flat
                           Lambert shading, outlines (object boundaries, depth jumps, creases), and one transparent
                           layer (the nearest transparent surface over the opaque image, e.g. shells at alpha 0.35)
section(items, path, ...)  planar section: every body cut by the plane (shape & a large planar face), the cut faces
                           filled in their colours, with key z lines and zoom panels (matplotlib)

items: [(name, shape, colour, alpha)] or [(name, shape, colour, alpha, (dx, dy, dz))] (an offset, e.g. exploded),
colour = any matplotlib colour."""
import math
import numpy as np

_TESS = {}


def tessellate(shape, tol=0.1, ang=0.3):
    """(V (n, 3), T (m, 3)) of a shape, meshed with OCP directly (build123d's tessellate() costs ~0.1 s per call),
    triangles wound outward; cached per shape object."""
    from OCP.BRepMesh import BRepMesh_IncrementalMesh
    from OCP.BRep import BRep_Tool
    from OCP.TopLoc import TopLoc_Location
    from OCP.TopExp import TopExp_Explorer
    from OCP.TopAbs import TopAbs_FACE, TopAbs_REVERSED
    from OCP.TopoDS import TopoDS
    key = (id(shape), tol, ang)
    if key in _TESS:
        return _TESS[key][:2]
    w = shape.wrapped
    BRepMesh_IncrementalMesh(w, tol, False, ang, False)
    Vs, Ts, base = [], [], 0
    ex = TopExp_Explorer(w, TopAbs_FACE)
    while ex.More():
        f = TopoDS.Face(ex.Current())
        loc = TopLoc_Location()
        tri = BRep_Tool.Triangulation_s(f, loc)
        if tri is not None:
            tr = loc.Transformation()
            n, m = tri.NbNodes(), tri.NbTriangles()
            V = np.empty((n, 3))
            for i in range(1, n + 1):
                q = tri.Node(i).Transformed(tr)
                V[i - 1] = (q.X(), q.Y(), q.Z())
            T = np.array([tri.Triangle(i).Get() for i in range(1, m + 1)], np.int64).reshape(-1, 3) - 1
            if f.Orientation() == TopAbs_REVERSED:
                T = T[:, [0, 2, 1]]
            Vs.append(V)
            Ts.append(T + base)
            base += n
        ex.Next()
    V = np.vstack(Vs) if Vs else np.zeros((0, 3))
    T = np.vstack(Ts) if Ts else np.zeros((0, 3), np.int64)
    _TESS[key] = (V, T, shape)                       # keep the shape alive so its id() stays unique
    return V, T


def _rgb(c):
    from matplotlib import colors
    return np.array(colors.to_rgb(c))


def _basis(az, el):
    a, e = math.radians(az), math.radians(el)
    c = np.array([math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e)])   # toward the camera
    r = np.array([-math.sin(a), math.cos(a), 0.0])
    u = np.cross(c, r)
    return r, u, c


def _split(X, Y, D, attr, cap=32.0):
    """bisect triangles along their longest screen edge until every bbox is <= cap px (depth is linear in screen
    space for an orthographic view, so the split is exact); attr rows follow their triangles."""
    while True:
        w = np.maximum(X.max(1) - X.min(1), Y.max(1) - Y.min(1))
        big = w > cap
        if not big.any():
            return X, Y, D, attr
        Xb, Yb, Db, Ab = X[big], Y[big], D[big], attr[big]
        e = np.stack([(Xb[:, 1] - Xb[:, 0]) ** 2 + (Yb[:, 1] - Yb[:, 0]) ** 2,
                      (Xb[:, 2] - Xb[:, 1]) ** 2 + (Yb[:, 2] - Yb[:, 1]) ** 2,
                      (Xb[:, 0] - Xb[:, 2]) ** 2 + (Yb[:, 0] - Yb[:, 2]) ** 2], 1).argmax(1)
        r = np.arange(len(e))
        i0, i1, i2 = e, (e + 1) % 3, (e + 2) % 3                 # split edge i0-i1 at its midpoint, opposite i2
        kids = []
        for A in (Xb, Yb, Db):
            m = (A[r, i0] + A[r, i1]) / 2
            a = np.stack([A[r, i0], m, A[r, i2]], 1)
            b = np.stack([m, A[r, i1], A[r, i2]], 1)
            kids.append(np.vstack([a, b]))
        X = np.vstack([X[~big], kids[0]])
        Y = np.vstack([Y[~big], kids[1]])
        D = np.vstack([D[~big], kids[2]])
        attr = np.concatenate([attr[~big], Ab, Ab])


def _fragments(X, Y, D, W, H, chunk=4_000_000):
    """yield (pix, depth, tri) fragment arrays for triangles with pixel coords X, Y (n, 3) and depths D (n, 3)
    (every bbox <= 32 px after _split)."""
    x0 = np.clip(np.floor(X.min(1)).astype(np.int64), 0, W - 1)
    x1 = np.clip(np.ceil(X.max(1)).astype(np.int64), 0, W - 1)
    y0 = np.clip(np.floor(Y.min(1)).astype(np.int64), 0, H - 1)
    y1 = np.clip(np.ceil(Y.max(1)).astype(np.int64), 0, H - 1)
    area = (X[:, 1] - X[:, 0]) * (Y[:, 2] - Y[:, 0]) - (X[:, 2] - X[:, 0]) * (Y[:, 1] - Y[:, 0])
    ok = (np.abs(area) > 1e-9) & (X.max(1) >= 0) & (X.min(1) < W) & (Y.max(1) >= 0) & (Y.min(1) < H)
    size = np.maximum(x1 - x0, y1 - y0) + 1
    idx_all = np.nonzero(ok)[0]
    s = 1
    while len(idx_all):
        sel = idx_all[size[idx_all] <= s]
        idx_all = idx_all[size[idx_all] > s]
        if len(sel) == 0:
            s *= 2
            continue
        g = np.arange(s)
        gx, gy = np.meshgrid(g, g)
        gx, gy = gx.ravel(), gy.ravel()
        per = max(1, chunk // (s * s))
        for k in range(0, len(sel), per):
            t = sel[k:k + per]
            px = x0[t, None] + gx[None, :]
            py = y0[t, None] + gy[None, :]
            cx, cy = px + 0.5, py + 0.5
            ax, ay, bx, by, qx, qy = X[t, 0, None], Y[t, 0, None], X[t, 1, None], Y[t, 1, None], X[t, 2, None], Y[t, 2, None]
            ar = area[t, None]
            wa = ((bx - cx) * (qy - cy) - (qx - cx) * (by - cy)) / ar
            wb = ((qx - cx) * (ay - cy) - (ax - cx) * (qy - cy)) / ar
            wc = 1.0 - wa - wb
            m = (wa >= -1e-6) & (wb >= -1e-6) & (wc >= -1e-6) & (px <= x1[t, None]) & (py <= y1[t, None])
            if not m.any():
                continue
            dep = wa * D[t, 0, None] + wb * D[t, 1, None] + wc * D[t, 2, None]
            tri = np.broadcast_to(t[:, None], px.shape)
            yield (py[m] * W + px[m]), dep[m], tri[m]
        s *= 2


def _zpass(X, Y, D, W, H, limit=None):
    """nearest depth + triangle per pixel (only fragments nearer than `limit` per pixel, if given)."""
    zb = np.full(W * H, -np.inf)
    tb = np.full(W * H, -1, np.int64)
    for pix, dep, tri in _fragments(X, Y, D, W, H):
        if limit is not None:
            keep = dep > limit[pix] + 1e-4
            pix, dep, tri = pix[keep], dep[keep], tri[keep]
        o = np.lexsort((-dep, pix))
        pix, dep, tri = pix[o], dep[o], tri[o]
        first = np.ones(len(pix), bool)
        first[1:] = pix[1:] != pix[:-1]
        pix, dep, tri = pix[first], dep[first], tri[first]
        better = dep > zb[pix]
        zb[pix[better]], tb[pix[better]] = dep[better], tri[better]
    return zb, tb


@np.errstate(invalid="ignore")
def view(items, path, az=-58.0, el=32.0, width=2000, height=1300, title=None, legend=None, tol=0.12,
         background="#ffffff", margin=0.03):
    """render items [(name, shape, colour, alpha)] to a PNG (orthographic, from azimuth az / elevation el deg)."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    r, u, c = _basis(az, el)
    light = 0.25 * r + 0.45 * u + 0.86 * c                          # camera-relative: lit from views below too
    light /= np.linalg.norm(light)
    layers = {"opaque": [], "clear": []}
    for oid, it in enumerate(items):
        name, shape, col, alpha = it[:4]
        V, T = tessellate(shape, tol)
        if len(T) == 0:
            continue
        if len(it) > 4 and it[4] is not None:
            V = V + np.asarray(it[4], float)
        layers["opaque" if alpha >= 0.999 else "clear"].append((oid, V, T, _rgb(col), alpha))
    allv = np.vstack([V for lay in layers.values() for _, V, _, _, _ in lay])
    sx, sy = allv @ r, allv @ u
    span = max((sx.max() - sx.min()) / (width * (1 - 2 * margin)), (sy.max() - sy.min()) / (height * (1 - 2 * margin)))
    ox = (sx.min() + sx.max()) / 2 - span * width / 2
    oy = (sy.min() + sy.max()) / 2 - span * height / 2

    def prep(lay):
        if not lay:
            return None
        Ps, Ts, cols, oids, alphas = [], [], [], [], []
        base = 0
        for oid, V, T, col, alpha in lay:
            Ps.append(V)
            Ts.append(T + base)
            base += len(V)
            cols.append(np.repeat(col[None, :], len(T), 0))
            oids.append(np.full(len(T), oid))
            alphas.append(np.full(len(T), alpha))
        Vv, Tt = np.vstack(Ps), np.vstack(Ts)
        tri = Vv[Tt]                                                  # (n, 3, 3)
        n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
        n /= np.linalg.norm(n, axis=1)[:, None] + 1e-12
        X = (tri @ r - ox) / span
        Y = height - (tri @ u - oy) / span
        D = tri @ c
        k = 0.30 + 0.70 * np.abs(n @ light)
        shade = np.clip(np.vstack(cols) * k[:, None], 0, 1)
        X, Y, D, src = _split(X, Y, D, np.arange(len(X)))
        return X, Y, D, shade[src], np.concatenate(oids)[src], np.concatenate(alphas)[src], n[src]

    op, cl = prep(layers["opaque"]), prep(layers["clear"])
    img = np.tile(_rgb(background), (width * height, 1))
    zb = np.full(width * height, -np.inf)
    oidb = np.full(width * height, -1)
    nb = np.zeros((width * height, 3))
    if op is not None:
        X, Y, D, shade, oids, _, nrm = op
        zb, tb = _zpass(X, Y, D, width, height)
        hit = tb >= 0
        img[hit] = shade[tb[hit]]
        oidb[hit] = oids[tb[hit]]
        nb[hit] = nrm[tb[hit]]
    # outlines: object boundaries, depth jumps, creases
    Z = zb.reshape(height, width)
    O = oidb.reshape(height, width)
    N = nb.reshape(height, width, 3)
    edge = np.zeros((height, width), bool)
    for dy, dx in ((0, 1), (1, 0)):
        a = (slice(0, height - dy), slice(0, width - dx))
        b = (slice(dy, height), slice(dx, width))
        diff = (O[a] != O[b]) | (np.abs(np.nan_to_num(Z[a] - Z[b], posinf=1e9, neginf=1e9)) > 0.6 * max(span * 3, 0.3))
        crease = (np.einsum("ijk,ijk->ij", N[a], N[b]) < 0.82) & (O[a] >= 0) & (O[b] >= 0)
        e = diff | crease
        edge[a] |= e
    img = img.reshape(height, width, 3)
    img[edge] = img[edge] * 0.25
    img = img.reshape(-1, 3)
    if cl is not None:                                               # one transparent layer over the opaque image
        X, Y, D, shade, oids, alphas, _ = cl
        zt, tt = _zpass(X, Y, D, width, height, limit=zb)
        hit = tt >= 0
        a = alphas[tt[hit]][:, None]
        img[hit] = a * shade[tt[hit]] + (1 - a) * img[hit]
        Ot = np.full(width * height, -1)
        Ot[hit] = oids[tt[hit]]
        Ot = Ot.reshape(height, width)
        Zt = zt.reshape(height, width)
        e2 = np.zeros((height, width), bool)
        for dy, dx in ((0, 1), (1, 0)):
            a_ = (slice(0, height - dy), slice(0, width - dx))
            b_ = (slice(dy, height), slice(dx, width))
            e2[a_] |= (Ot[a_] != Ot[b_]) | (np.abs(np.nan_to_num(Zt[a_] - Zt[b_], posinf=1e9, neginf=1e9))
                                           > 0.6 * max(span * 3, 0.3))
        img = img.reshape(height, width, 3)
        img[e2] = img[e2] * 0.55
        img = img.reshape(-1, 3)
    img = img.reshape(height, width, 3)
    fig = plt.figure(figsize=(width / 100, height / 100 + (0.5 if title else 0) + (0.9 if legend else 0)), dpi=100)
    ax = fig.add_axes([0, (0.9 / (height / 100 + 1.4)) if legend else 0, 1, height / 100 / (height / 100 +
                      (0.5 if title else 0) + (0.9 if legend else 0))])
    ax.imshow(img, interpolation="lanczos")
    ax.set_axis_off()
    if title:
        fig.suptitle(title, fontsize=15, y=0.995)
    if legend:
        from matplotlib.patches import Patch
        fig.legend(handles=[Patch(color=cc, label=ll) for ll, cc in legend], loc="lower center",
                   ncol=min(len(legend), 9), fontsize=10, frameon=False)
    fig.savefig(path, dpi=100)
    plt.close(fig)
    return path


def _spread(zs, gap):
    """label positions for the sorted values zs, at least gap apart, as close to them as possible."""
    y = list(zs)
    for _ in range(200):
        moved = False
        for i in range(1, len(y)):
            d = y[i] - y[i - 1]
            if d < gap - 1e-9:
                push = (gap - d) / 2
                y[i - 1] -= push
                y[i] += push
                moved = True
        if not moved:
            break
    return y


def _section_faces(shape, plane_face):
    try:
        s = shape & plane_face
    except Exception:
        return []
    return [f for f in s.faces()] if s is not None else []


def section(items, path, axis, at, zlines=(), zooms=(), title=None, flip_h=False, tol=0.05):
    """section of items [(name, shape, colour, alpha)] by the plane y = at (axis 'y', enclosure y) or x = at (axis
    'x'); horizontal = x (axis 'y') or KiCad y = -enclosure y (axis 'x'); zooms = [(h0, h1)] extra panels."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection
    from build123d import Face, Plane
    if axis == "y":
        pl = Plane(origin=(117.0, at, 0.0), x_dir=(1, 0, 0), z_dir=(0, 1, 0))
        to2 = lambda p: (p[0], p[2])
    else:
        pl = Plane(origin=(at, -75.0, 0.0), x_dir=(0, 1, 0), z_dir=(1, 0, 0))
        to2 = lambda p: (-p[1], p[2])
    big = Face.make_rect(400, 120, pl)
    cuts = []
    for it in items:
        name, shape, col, alpha = it[:4]
        if len(it) > 4 and it[4] is not None:
            from build123d import Pos
            shape = Pos(*it[4]) * shape
        bb = shape.bounding_box()
        lo, hi = (bb.min.Y, bb.max.Y) if axis == "y" else (bb.min.X, bb.max.X)
        if not (lo - 1e-6 <= at <= hi + 1e-6):
            continue
        for f in _section_faces(shape, big):
            vs, ts = f.tessellate(tol, 0.2)
            V = np.array([to2((v.X, v.Y, v.Z)) for v in vs])
            T = np.array(ts).reshape(-1, 3)
            outl = []
            for e in f.edges():
                n = 2 if e.geom_type.name == "LINE" else 24
                outl.append(np.array([to2(tuple(e.position_at(t))) for t in np.linspace(0, 1, n)]))
            cuts.append((name, col, alpha, V[T] if len(T) else None, outl))
    if not cuts:
        raise RuntimeError(f"section {axis}={at}: nothing cut")
    allp = np.vstack([o for _, _, _, _, ol in cuts for o in ol])
    h0, h1 = allp[:, 0].min() - 2, allp[:, 0].max() + 2
    z0, z1 = allp[:, 1].min() - 1.5, allp[:, 1].max() + 1.5
    panels = [(h0, h1)] + list(zooms)
    full_w = 26.0
    hh = [full_w * (z1 - z0) / (h1 - h0)] + [((full_w - 0.6 * (len(zooms) - 1)) / max(len(zooms), 1)) *
                                             (z1 - z0) / (b - a) for a, b in zooms]
    fig = plt.figure(figsize=(full_w + 1.2, hh[0] + (max(hh[1:]) if zooms else 0) + 2.6), dpi=110)
    axes = [fig.add_axes([0.03, 1 - (hh[0] + 0.9) / (hh[0] + (max(hh[1:]) if zooms else 0) + 2.6),
                          0.88, hh[0] / (hh[0] + (max(hh[1:]) if zooms else 0) + 2.6)])]
    if zooms:
        tot = hh[0] + max(hh[1:]) + 2.6
        wz = 0.97 / len(zooms)
        for i, _ in enumerate(zooms):
            axes.append(fig.add_axes([0.03 + i * wz, 0.35 / tot, wz - 0.075, max(hh[1:]) / tot]))
    for k, (ax, (a, b)) in enumerate(zip(axes, panels)):
        for name, col, alpha, tri, outl in cuts:
            if tri is not None:
                ax.add_collection(PolyCollection(tri, facecolors=col, edgecolors="none", antialiaseds=False,
                                                 alpha=0.85 if alpha < 1 else 1.0))
            for o in outl:
                ax.plot(o[:, 0], o[:, 1], color="black", lw=0.35 if k == 0 else 0.6)
        zs = sorted(zlines)
        ys = _spread([z for z, _ in zs], (z1 - z0) * (0.075 if k == 0 else 0.045))
        for (z, lab), yl in zip(zs, ys):
            ax.axhline(z, color="#555555", lw=0.4, ls=(0, (4, 4)))
            xl = b + (b - a) * (0.004 if k == 0 else 0.012)
            ax.annotate(f"{lab} {z:+.2f}", xy=(b, z), xytext=(xl, yl), fontsize=6.5 if k == 0 else 7,
                        color="#333333", va="center", annotation_clip=False,
                        arrowprops=dict(arrowstyle="-", lw=0.3, color="#888888"))
        ax.set_xlim(a, b)
        ax.set_ylim(z0, z1)
        ax.set_aspect("equal")
        ax.tick_params(labelsize=7)
        if flip_h:
            ax.invert_xaxis()
        ax.grid(alpha=0.25, lw=0.3)
    if title:
        fig.suptitle(title, fontsize=13, y=0.995)
    fig.savefig(path, dpi=110)
    plt.close(fig)
    return path
