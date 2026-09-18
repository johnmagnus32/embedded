#!/usr/bin/env python3
"""ORIGINAL item models as data (generic objects, not any specific franchise art):
  shell.obj  — a domed turtle shell: recolourable 'shell_tint' dome + cream belly + dark rim lip
               (tinted green=straight / red=homing / blue=anti-leader at draw time)
  banana.obj — a curved banana (the drop trap)
Run: python3 tools/gen_kart_items.py"""
import os, math
OUT="src/assets/kart"; PI=math.pi
def writeobj(fname, mtllib, verts, groups, uvs=None):
    with open(os.path.join(OUT,fname),"w") as f:
        f.write("# %s (tools/gen_kart_items.py)\nmtllib %s\n"%(fname,mtllib))
        for v in verts: f.write("v %.4f %.4f %.4f\n"%(v[0],v[1],v[2]))
        for uv in (uvs or []): f.write("vt %.4f %.4f\n"%(uv[0],uv[1]))
        for mtl,fs in groups:
            f.write("usemtl %s\n"%mtl)
            for q in fs: f.write("f "+" ".join((("%d/%d"%t) if isinstance(t,tuple) else str(t)) for t in q)+"\n")

# ---------- shell: hemisphere dome (UV-mapped for the scute texture) + belly + rim lip ----------
verts=[]; uvs=[]; dome=[]; belly=[]; rim=[]
R=0.5; SQ=0.66; rings=5; segs=14; grid=[]
for i in range(rings+1):
    th=(PI/2)*i/rings; row=[]
    for j in range(segs+1):
        ph=2*PI*j/segs
        verts.append((R*math.sin(th)*math.cos(ph), R*math.cos(th)*SQ, R*math.sin(th)*math.sin(ph)))
        uvs.append((2.0*j/segs, 1.0-i/rings)); row.append(len(verts))
    grid.append(row)
for i in range(rings):
    for j in range(segs): dome.append([(grid[i][j],grid[i][j]),(grid[i][j+1],grid[i][j+1]),(grid[i+1][j+1],grid[i+1][j+1]),(grid[i+1][j],grid[i+1][j])])
base=grid[rings]
outer=[]
for j in range(segs+1):
    ph=2*PI*j/segs; verts.append((R*1.14*math.cos(ph), -0.02, R*1.14*math.sin(ph))); uvs.append((0.5,0.5)); outer.append(len(verts))
for j in range(segs): rim.append([(base[j],base[j]),(base[j+1],base[j+1]),(outer[j+1],outer[j+1]),(outer[j],outer[j])])
verts.append((0.0,-0.02,0.0)); uvs.append((0.5,0.5)); cbot=len(verts)
for j in range(segs): belly.append([(cbot,cbot),(outer[j+1],outer[j+1]),(outer[j],outer[j])])
writeobj("shell.obj","shell.mtl",verts,[("shell_tint",dome),("shell_rim",rim),("shell_belly",belly)],uvs)
open(os.path.join(OUT,"shell.mtl"),"w").write(
    "newmtl shell_tint\nKd 1 1 1\nmap_Kd shell_tex.png\nnewmtl shell_rim\nKd 0.30 0.26 0.18\nnewmtl shell_belly\nKd 0.90 0.84 0.62\n")

# ---------- banana: a tapered tube swept along an arc lying on the ground (XZ) ----------
verts=[]; body=[]
A=1.15; bend=0.62; Rmax=0.14; N=9; M=7; grid=[]
for i in range(N+1):
    u=i/N; t=-A+2*A*u
    P=(bend*math.sin(t), 0.0, bend*(math.cos(t)-math.cos(A)))
    Nn=(math.sin(t),0.0,math.cos(t))                       # in-plane normal (XZ)
    r=Rmax*(math.sin(PI*u)**0.5)                           # taper to points at the tips
    row=[]
    for j in range(M+1):
        ph=2*PI*j/M
        verts.append((P[0]+r*(math.cos(ph)*Nn[0]), P[1]+r*math.sin(ph), P[2]+r*(math.cos(ph)*Nn[2]))); row.append(len(verts))
    grid.append(row)
for i in range(N):
    for j in range(M): body.append((grid[i][j],grid[i][j+1],grid[i+1][j+1],grid[i+1][j]))
miny=min(verts[k][1] for k in range(len(verts)))
verts=[(x,y-miny,z) for (x,y,z) in verts]                  # sit on the ground
writeobj("banana.obj","banana.mtl",verts,[("banana",body)])
open(os.path.join(OUT,"banana.mtl"),"w").write("newmtl banana\nKd 0.95 0.82 0.20\n")
print("wrote shell.obj/.mtl + banana.obj/.mtl")
