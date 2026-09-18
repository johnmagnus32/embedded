#!/usr/bin/env python3
"""Generate real ROCK shapes as data: src/assets/kart/rock{1,2,3}.obj (+ rocks.mtl).
A sphere with its radius modulated by direction-periodic sinusoids (seam-consistent), low-res so it
reads as an angular boulder, not a ball. Faceted by the engine's crease shading. Normalized so the
lowest vertex is y=0 (sits on the ground). Run: python3 tools/gen_kart_rocks.py"""
import os, math, random
OUT=os.path.join(os.path.dirname(os.path.abspath(__file__)),"..","src","assets","kart")
os.makedirs(OUT,exist_ok=True)

def gen(seed, squash, mtl, fname):
    rnd=random.Random(seed); p=[rnd.uniform(0,6.283) for _ in range(4)]
    rings, segs = 7, 12; verts=[]; uvs=[]; faces=[]; grid=[]
    for i in range(rings+1):
        th=math.pi*i/rings; row=[]
        for j in range(segs+1):
            ph=2*math.pi*j/segs
            rr=(1.0 + 0.22*math.sin(3*ph+p[0])*math.sin(2*th) + 0.17*math.cos(5*ph+p[1])
                    + 0.14*math.sin(4*th+p[2]) + 0.10*math.sin(7*ph+p[3])*math.cos(3*th))
            rr=max(0.55,rr)
            verts.append([rr*math.sin(th)*math.cos(ph), rr*math.cos(th)*squash, rr*math.sin(th)*math.sin(ph)])
            uvs.append((ph/(2*math.pi), th/math.pi))
            row.append(len(verts))
        grid.append(row)
    miny=min(v[1] for v in verts)
    for v in verts: v[1]-=miny            # sit on ground (min y = 0)
    for i in range(rings):
        for j in range(segs):
            faces.append((grid[i][j],grid[i][j+1],grid[i+1][j+1],grid[i+1][j]))
    with open(os.path.join(OUT,fname),"w") as f:
        f.write("# procedural rock (tools/gen_kart_rocks.py)\nmtllib rocks.mtl\n")
        for v in verts: f.write("v %.4f %.4f %.4f\n"%(v[0],v[1],v[2]))
        for uv in uvs: f.write("vt %.4f %.4f\n"%uv)
        f.write("usemtl %s\n"%mtl)
        for q in faces: f.write("f %d/%d %d/%d %d/%d %d/%d\n"%(q[0],q[0],q[1],q[1],q[2],q[2],q[3],q[3]))
    print("wrote assets/kart/%s (%d v)"%(fname,len(verts)))

with open(os.path.join(OUT,"rocks.mtl"),"w") as f:
    f.write("# rock materials\n")
    for name,kd in [("rock_gray",(0.44,0.43,0.41)),("rock_tan",(0.50,0.44,0.34)),("rock_dark",(0.34,0.34,0.36))]:
        f.write("newmtl %s\nKd %s %s %s\nmap_Kd rock_tex.png\n"%(name,kd[0],kd[1],kd[2]))
gen(1, 0.75, "rock_gray", "rock1.obj")
gen(2, 0.92, "rock_tan",  "rock2.obj")
gen(3, 0.62, "rock_dark", "rock3.obj")
