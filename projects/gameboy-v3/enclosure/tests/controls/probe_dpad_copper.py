"""Read-only probe of gb3.kicad_pcb round the D-pad centre (KiCad (36.5, 74.9)): F.Cu tracks, vias, pads and zones
near the pivot. Opens the board file for reading only; writes nothing next to it.
usage: python probe_dpad_copper.py <path to gb3.kicad_pcb>"""
import math, re, sys

CX, CY = 36.5, 74.9
R = 6.5


def tokens(s):
    for m in re.finditer(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()]+', s):
        yield m.group(0)


def parse(s):
    stack, cur = [], []
    for t in tokens(s):
        if t == "(":
            stack.append(cur)
            cur = []
        elif t == ")":
            done = cur
            cur = stack.pop()
            cur.append(done)
        else:
            cur.append(t.strip('"') if t.startswith('"') else t)
    return cur[0]


def find(node, key):
    return [c for c in node if isinstance(c, list) and c and c[0] == key]


def first(node, key):
    f = find(node, key)
    return f[0] if f else None


def seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    L2 = dx * dx + dy * dy
    t = 0.0 if L2 == 0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / L2))
    return math.hypot(px - ax - t * dx, py - ay - t * dy)


def point_in_poly(x, y, pts):
    inside = False
    for i in range(len(pts)):
        (ax, ay), (bx, by) = pts[i], pts[(i + 1) % len(pts)]
        if (ay > y) != (by > y) and x < (bx - ax) * (y - ay) / (by - ay) + ax:
            inside = not inside
    return inside


def main(path):
    with open(path, "r") as fh:                     # read only
        root = parse(fh.read())
    nets = {}
    for n in find(root, "net"):
        if len(n) >= 3:
            nets[n[1]] = n[2]
    print(f"D-pad centre KiCad ({CX}, {CY}); radius {R}")
    print("-- F.Cu / B.Cu tracks within R (distance from the centre, net, width):")
    rows = []
    for s in find(root, "segment"):
        st, en, layer, w, net = first(s, "start"), first(s, "end"), first(s, "layer"), first(s, "width"), first(s, "net")
        ax, ay, bx, by = float(st[1]), float(st[2]), float(en[1]), float(en[2])
        d = seg_dist(CX, CY, ax, ay, bx, by)
        if d < R:
            nn = net[1] if net else "?"
            rows.append((d, layer[1], nets.get(nn, nn), float(w[1])))
    for r in sorted(rows)[:30]:
        print(f"   {r[0]:6.2f}  {r[1]:6s} {r[2]:24s} w {r[3]}")
    print("-- vias within R:")
    for v in find(root, "via"):
        at = first(v, "at")
        x, y = float(at[1]), float(at[2])
        d = math.hypot(x - CX, y - CY)
        if d < R:
            net = first(v, "net")
            nn = net[1] if net else "?"
            size = first(v, "size")
            tent = first(v, "tenting")
            print(f"   {d:6.2f}  at ({x:.3f}, {y:.3f}) d {size[1] if size else '?'} net {nets.get(nn, nn)} tenting {tent}")
    print("-- pads within R (footprint ref, pad, layer, distance to the pad centre, size):")
    for fp in find(root, "footprint"):
        at = first(fp, "at")
        fx, fy = float(at[1]), float(at[2])
        rot = float(at[3]) if len(at) > 3 else 0.0
        ref = None
        for p in find(fp, "property"):
            if p[1] == "Reference":
                ref = p[2]
        for fpt in find(fp, "fp_text"):
            if fpt[1] == "reference":
                ref = fpt[2]
        layer = first(fp, "layer")[1]
        if math.hypot(fx - CX, fy - CY) > R + 6:
            continue
        for pad in find(fp, "pad"):
            pat = first(pad, "at")
            px, py = float(pat[1]), float(pat[2])
            c, s_ = math.cos(math.radians(-rot)), math.sin(math.radians(-rot))
            x, y = fx + px * c - py * s_, fy + px * s_ + py * c
            d = math.hypot(x - CX, y - CY)
            size = first(pad, "size")
            if d < R + 2:
                print(f"   {ref:6s} pad {pad[1]:3s} {layer:5s} d {d:6.2f} at ({x:.3f}, {y:.3f}) size {size[1]} x {size[2]}"
                      f" rot {rot}")
    print("-- zones whose filled polygon contains the centre:")
    for z in find(root, "zone"):
        layer = first(z, "layer")
        layers = first(z, "layers")
        lname = layer[1] if layer else (" ".join(layers[1:]) if layers else "?")
        net = first(z, "net_name")
        for fp in find(z, "filled_polygon"):
            fl = first(fp, "layer")
            pts = [(float(q[1]), float(q[2])) for q in first(fp, "pts") if isinstance(q, list) and q[0] == "xy"]
            if pts and point_in_poly(CX, CY, pts):
                print(f"   zone net {net[1] if net else '?'} layer {fl[1] if fl else lname}: centre inside (filled)")
    print("-- silkscreen / fab text near the centre (F.SilkS):")
    for g in find(root, "gr_text") + find(root, "gr_line"):
        pass


if __name__ == "__main__":
    main(sys.argv[1])
