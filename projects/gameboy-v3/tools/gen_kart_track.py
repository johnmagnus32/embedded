#!/usr/bin/env python3
"""Bake each track's ROAD SURFACE into a Wavefront OBJ the engine imports (assets/kart/tracks/<name>.obj).

This is the industry pattern: the drivable road is a modeled mesh loaded at runtime, while the authored
racing-line spline + markers stay in <name>.json (read by the game for physics/AI/laps). The mesh is a
DERIVED artifact — this tool replicates kart.c's build_track (Catmull-Rom spline -> arc-length resample
to NSEG -> width-from-curvature -> terrain drape incl. the ramp mound) plus the draw-time curve
subdivision, so the baked geometry lines up exactly with the spline the game drives on.

IMPORTANT: the terr()/cr1()/width math below MUST stay in sync with kart.c. If you change terrain or the
spline there, re-run this tool. Triangles are emitted in SEGMENT ORDER (TRACK_SUB sub-quads per segment),
so the game draws only the near-window of segments (cheap on the T113) and picks road/boost/ramp textures
per segment itself. Walls stay procedural in the game. Run: python3 tools/gen_kart_track.py"""
import json, math, os

NSEG = 240
TRACK_SUB = 3          # sub-quads per segment baked into the mesh (must match kart.c TRACK_SUB)
HERE = os.path.dirname(os.path.abspath(__file__))
TDIR = os.path.join(HERE, "..", "src", "assets", "kart", "tracks")

def cr1(a, b, c, e, t):
    t2 = t*t; t3 = t2*t
    return 0.5*((2*b) + (-a+c)*t + (2*a-5*b+4*c-e)*t2 + (-a+3*b-3*c+e)*t3)

def sub(a, b): return (a[0]-b[0], a[1]-b[1], a[2]-b[2])
def norm(v):
    l = math.sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]) or 1.0
    return (v[0]/l, v[1]/l, v[2]/l)
def clamp(v, lo, hi): return lo if v < lo else hi if v > hi else v

def build(track):
    WP = track["waypoints"]; nwp = len(WP)
    wide = track.get("width", {}).get("wide", 11.0); narrow = track.get("width", {}).get("narrow", 6.5)
    rp = track.get("ramp", {}); ramp_seg = rp.get("seg", 58); ramp_h = rp.get("height", 5.0); ramp_r = rp.get("radius", 70.0)
    # --- dense Catmull-Rom sample of the waypoint loop ---
    dx = []; dz = []; steps = 2400 // nwp
    for w in range(nwp):
        i0, i1, i2, i3 = (w-1) % nwp, w, (w+1) % nwp, (w+2) % nwp
        for t2 in range(steps):
            t = t2/steps
            dx.append(cr1(WP[i0][0], WP[i1][0], WP[i2][0], WP[i3][0], t))
            dz.append(cr1(WP[i0][1], WP[i1][1], WP[i2][1], WP[i3][1], t))
    n = len(dx)
    cum = [0.0]*(n+1)
    for i in range(n):
        j = (i+1) % n; a = dx[j]-dx[i]; b = dz[j]-dz[i]; cum[i+1] = cum[i] + math.sqrt(a*a+b*b)
    total = cum[n]
    # ramp mound centre (arc-length position of ramp_seg), for terr()
    tg = total*ramp_seg/NSEG; d2 = 0
    while d2 < n-1 and cum[d2+1] < tg: d2 += 1
    sl = cum[d2+1]-cum[d2]; f = (tg-cum[d2])/sl if sl > 1e-4 else 0; j2 = (d2+1) % n
    rampx = dx[d2]+(dx[j2]-dx[d2])*f; rampz = dz[d2]+(dz[j2]-dz[d2])*f

    def terr(x, z):
        h = 6.0*math.sin(x*0.028) + 5.0*math.sin(z*0.023+1.3)
        ddx = x-rampx; ddz = z-rampz; dd = ddx*ddx+ddz*ddz
        if dd < ramp_r*ramp_r:
            tt = 1.0 - math.sqrt(dd)/ramp_r; h += ramp_h*tt*tt*(3.0-2.0*tt)
        return h

    # --- even arc-length resample into NSEG points, draped on terr ---
    P = []; di = 0
    for i in range(NSEG):
        target = total*i/NSEG
        while di < n-1 and cum[di+1] < target: di += 1
        sl = cum[di+1]-cum[di]; f = (target-cum[di])/sl if sl > 1e-4 else 0; j = (di+1) % n
        px = dx[di]+(dx[j]-dx[di])*f; pz = dz[di]+(dz[j]-dz[di])*f
        P.append((px, terr(px, pz)+0.25, pz))
    # --- width from smoothed curvature ---
    turn = [0.0]*NSEG
    for i in range(NSEG):
        a = norm(sub(P[i], P[(i-1) % NSEG])); b = norm(sub(P[(i+1) % NSEG], P[i]))
        turn[i] = math.atan2(a[0]*b[2]-a[2]*b[0], a[0]*b[0]+a[2]*b[2])
    WID = [0.0]*NSEG
    for i in range(NSEG):
        st = sum(turn[(i+k) % NSEG] for k in range(-3, 4))/7.0
        WID[i] = clamp(wide-abs(st)*55.0, narrow, wide)

    def cr_at(s):
        m = s % NSEG; i = int(m); t = m-i
        p0 = P[(i-1) % NSEG]; p1 = P[i]; p2 = P[(i+1) % NSEG]; p3 = P[(i+2) % NSEG]
        return (cr1(p0[0], p1[0], p2[0], p3[0], t), cr1(p0[1], p1[1], p2[1], p3[1], t), cr1(p0[2], p1[2], p2[2], p3[2], t))
    def wid_at(s):
        m = s % NSEG; i = int(m); t = m-i; return WID[i]*(1-t)+WID[(i+1) % NSEG]*t
    def road_lat(s):
        fv = sub(cr_at(s+0.06), cr_at(s-0.06)); return norm((fv[2], 0.0, -fv[0]))
    def drape(c, lat, off):
        p = (c[0]+lat[0]*off, c[1]+lat[1]*off, c[2]+lat[2]*off)
        return (p[0], terr(p[0], p[2]) + (c[1]-terr(c[0], c[2])), p[2])

    verts = []; uvs = []; faces = []
    for i in range(NSEG):
        for u in range(TRACK_SUB):
            s0 = i + u/TRACK_SUB; s1 = i + (u+1)/TRACK_SUB
            c0 = cr_at(s0); c1 = cr_at(s1); l0 = road_lat(s0); l1 = road_lat(s1); w0 = wid_at(s0); w1 = wid_at(s1)
            quad = [(drape(c0, l0,  w0), (0.0, s0)),   # engine flips v (va = 1-TV), so write 1-s to get v=s
                    (drape(c0, l0, -w0), (1.0, s0)),
                    (drape(c1, l1, -w1), (1.0, s1)),
                    (drape(c1, l1,  w1), (0.0, s1))]
            b = len(verts)
            for (p, (uu, vv)) in quad:
                verts.append(p); uvs.append((uu, 1.0-vv))
            faces.append((b+1, b+2, b+3)); faces.append((b+1, b+3, b+4))   # 1-based, match quad_s winding
    return verts, uvs, faces

def main():
    with open(os.path.join(TDIR, "index.json")) as f:
        names = json.load(f)
    for fn in names:
        with open(os.path.join(TDIR, fn)) as f:
            track = json.load(f)
        verts, uvs, faces = build(track)
        base = os.path.splitext(fn)[0]
        outp = os.path.join(TDIR, base + ".obj")
        with open(outp, "w") as o:
            o.write("# %s road surface, baked from %s by tools/gen_kart_track.py (%d segs x %d sub-quads)\n"
                    % (track.get("name", base), fn, NSEG, TRACK_SUB))
            for v in verts: o.write("v %.4f %.4f %.4f\n" % v)
            for t in uvs:   o.write("vt %.4f %.4f\n" % t)
            for a, b, c in faces: o.write("f %d/%d %d/%d %d/%d\n" % (a, a, b, b, c, c))
        print("wrote %s  (%d verts, %d tris)" % (os.path.relpath(outp), len(verts), len(faces)))

if __name__ == "__main__":
    main()
