#!/usr/bin/env python3
"""Textured pine tree as data: src/assets/kart/tree.obj (+ tree.mtl -> bark_tex/leaf_tex).
Trunk box + 3 cone tiers, UV-mapped. Run: python3 tools/gen_kart_tree.py"""
import os, math
OUT=os.path.join(os.path.dirname(os.path.abspath(__file__)),"..","src","assets","kart"); PI=math.pi
verts=[]; uvs=[]; faces=[]
def V(p,uv): verts.append(p); uvs.append(uv); return len(verts)
def box(lo,hi,mtl):
    (x0,y0,z0),(x1,y1,z1)=lo,hi
    C=[(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),(x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)]
    UV=[(0,0),(1,0),(1,1),(0,1)]
    for face in [(0,1,2,3),(5,4,7,6),(4,0,3,7),(1,5,6,2),(4,5,1,0),(3,2,6,7)]:
        idx=[V(C[k],UV[n]) for n,k in enumerate(face)]; faces.append((mtl,[(i,i) for i in idx]))
def cone(base,r,h,segs,mtl):
    bx,by,bz=base; apex=V((bx,by+h,bz),(0.5,1.0))
    ring=[V((bx+math.cos(2*PI*j/segs)*r, by, bz+math.sin(2*PI*j/segs)*r),(j/segs,0.0)) for j in range(segs+1)]
    for j in range(segs): faces.append((mtl,[(apex,apex),(ring[j+1],ring[j+1]),(ring[j],ring[j])]))
box((-0.22,0,-0.22),(0.22,1.5,0.22),"bark")
cone((0,1.3,0),1.7,1.9,10,"foliage"); cone((0,2.5,0),1.3,1.8,10,"foliage"); cone((0,3.7,0),0.85,1.7,10,"foliage")
with open(os.path.join(OUT,"tree.obj"),"w") as f:
    f.write("# tree (tools/gen_kart_tree.py)\nmtllib tree.mtl\n")
    for v in verts: f.write("v %.4f %.4f %.4f\n"%v)
    for uv in uvs: f.write("vt %.4f %.4f\n"%uv)
    cur=None
    for mtl,ft in faces:
        if mtl!=cur: f.write("usemtl %s\n"%mtl); cur=mtl
        f.write("f "+" ".join("%d/%d"%(v,t) for v,t in ft)+"\n")
open(os.path.join(OUT,"tree.mtl"),"w").write("newmtl bark\nKd 1 1 1\nmap_Kd bark_tex.png\nnewmtl foliage\nKd 1 1 1\nmap_Kd leaf_tex.png\n")
print("wrote tree.obj/.mtl (%d v)"%len(verts))
