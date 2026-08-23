#!/usr/bin/env python3
"""Generate the canvas-adventure art + level into ../src/assets/adventure/.

Produces (deterministic — fixed RNG seed):
  tiles.png   32px top-down tileset atlas, 4 cols x 2 rows (grass/path/flower/sand/water/tree/wall/blank)
  hero.png    32px sprite SHEET: 4 rows (down/up/left/right) x 4 frames (stand,stepA,stand,stepB)
  slime.png   32px sheet: 1 row x 2 frames (bob)
  heart.png   24px pickup (also the HUD icon)
  key.png     24px pickup
  chest.png   32px goal
  sword.png   24px slash effect (direction-agnostic)
  level1.tmj  30x20 Tiled map: "bg" ground layer + "solid" collision layer + entity objects

Run:  python3 tools/gen_adventure_assets.py   (from projects/gameboy-v3/)
The engine loads these at runtime under $CANVAS_ASSETS/adventure/ — nothing here ships in a binary.
"""
import json, os, random
from PIL import Image, ImageDraw

random.seed(1337)
HERE = os.path.dirname(os.path.abspath(__file__))
OUT  = os.path.join(HERE, "..", "src", "assets", "adventure")
os.makedirs(OUT, exist_ok=True)

TS = 32   # tile / entity frame size

def save(im, name):
    im.save(os.path.join(OUT, name))
    print(f"  {name:12s} {im.size}")

# ---- tileset atlas ---------------------------------------------------------
# local index -> gid is +1 (Tiled firstgid=1). Order MUST match adventure.c's TILE_* comment.
# DETERMINISTIC 4px dither instead of random speckle: gives subtle texture but is PERIODIC, so
# the whole scrolling framebuffer compresses well (random per-pixel noise defeats zlib -> huge
# frames -> the web sim drops frames over a slow tunnel). Same pattern in every tile instance.
def dither(d, ox, oy, c):
    for y in range(0, TS, 8):
        for x in range(0, TS, 8):
            d.point((ox + x, oy + y), fill=c)

def t_grass(d, ox, oy):
    d.rectangle([ox, oy, ox+TS-1, oy+TS-1], fill=(86, 150, 78))
    dither(d, ox, oy, (98, 164, 88))

def t_path(d, ox, oy):
    d.rectangle([ox, oy, ox+TS-1, oy+TS-1], fill=(196, 170, 120))
    dither(d, ox, oy, (182, 156, 108))

def t_flower(d, ox, oy):
    t_grass(d, ox, oy)
    for _ in range(5):
        x = ox + random.randint(4, TS-5); y = oy + random.randint(4, TS-5)
        c = random.choice([(230,90,120),(240,210,80),(240,240,250)])
        d.ellipse([x-2, y-2, x+2, y+2], fill=c)
        d.point((x, y), fill=(250,240,120))

def t_sand(d, ox, oy):
    d.rectangle([ox, oy, ox+TS-1, oy+TS-1], fill=(220, 204, 152))
    dither(d, ox, oy, (232, 218, 170))

def t_water(d, ox, oy):
    d.rectangle([ox, oy, ox+TS-1, oy+TS-1], fill=(64, 118, 196))
    for k in range(4):
        y = oy + 6 + k*7
        d.line([ox+4, y, ox+TS-6, y], fill=(120,170,224), width=1)

def t_tree(d, ox, oy):
    t_grass(d, ox, oy)
    d.rectangle([ox+14, oy+20, ox+17, oy+28], fill=(96,64,40))      # trunk
    d.ellipse([ox+4, oy+2, ox+27, oy+24], fill=(44,112,52))          # canopy
    d.ellipse([ox+8, oy+5, ox+22, oy+18], fill=(58,132,64))

def t_wall(d, ox, oy):
    d.rectangle([ox, oy, ox+TS-1, oy+TS-1], fill=(120, 120, 132))
    d.line([ox, oy+15, ox+TS-1, oy+15], fill=(92,92,104), width=1)  # mortar rows
    for r,off in ((0,0),(16,16)):
        d.line([ox+16-off, oy+r, ox+16-off, oy+r+15], fill=(92,92,104), width=1)
    dither(d, ox, oy, (134, 134, 146))

TILES = [t_grass, t_path, t_flower, t_sand, t_water, t_tree, t_wall]  # + blank (8th)
COLS = 4
atlas = Image.new("RGBA", (COLS*TS, 2*TS), (0,0,0,0))
ad = ImageDraw.Draw(atlas)
for i, fn in enumerate(TILES):
    fn(ad, (i % COLS)*TS, (i // COLS)*TS)
save(atlas, "tiles.png")

# ---- hero sheet: 4 dirs x 4 frames -----------------------------------------
SKIN=(240,200,160); TUNIC=(70,120,200); TUNIC2=(52,96,168); HAIR=(90,60,40); BOOT=(80,60,44)
def hero_frame(d, ox, oy, dirn, step):
    cx = ox + TS//2
    # legs (step: -1 left-fwd, 0 together, +1 right-fwd)
    lo = {0:0, 1:-3, 2:0, 3:3}[step]
    d.rectangle([cx-6, oy+24, cx-2, oy+30+ (lo if lo<0 else 0)], fill=BOOT)
    d.rectangle([cx+2, oy+24, cx+6, oy+30+ (lo if lo>0 else 0)], fill=BOOT)
    # body
    d.rectangle([cx-7, oy+13, cx+7, oy+25], fill=TUNIC)
    d.rectangle([cx-7, oy+13, cx+7, oy+16], fill=TUNIC2)   # belt/shoulders shade
    # head
    d.ellipse([cx-6, oy+3, cx+6, oy+15], fill=SKIN)
    if dirn == 1:                                          # up = back of head (hair)
        d.chord([cx-6, oy+3, cx+6, oy+15], 180, 360, fill=HAIR)
        d.rectangle([cx-6, oy+3, cx+6, oy+8], fill=HAIR)
    else:
        d.arc([cx-6, oy+1, cx+6, oy+12], 180, 360, fill=HAIR)  # hair fringe
        eyes = {0:[(cx-3,oy+9),(cx+3,oy+9)], 2:[(cx-4,oy+9)], 3:[(cx+4,oy+9)]}[dirn]
        for (ex,ey) in eyes:
            d.rectangle([ex-1, ey-1, ex, ey+1], fill=(30,30,40))
    # arms hint
    d.rectangle([cx-9, oy+14, cx-7, oy+22], fill=SKIN)
    d.rectangle([cx+7, oy+14, cx+9, oy+22], fill=SKIN)

hero = Image.new("RGBA", (4*TS, 4*TS), (0,0,0,0))
hd = ImageDraw.Draw(hero)
for row in range(4):          # 0 down,1 up,2 left,3 right
    for col in range(4):      # 0 stand,1 stepA,2 stand,3 stepB
        hero_frame(hd, col*TS, row*TS, row, col)
save(hero, "hero.png")

# ---- slime sheet: 1 row x 2 frames -----------------------------------------
def slime_frame(d, ox, oy, squash):
    cx = ox + TS//2; base = oy + 26
    w = 11 + squash; h = 11 - squash
    d.ellipse([cx-w, base-2*h, cx+w, base], fill=(96,196,110))
    d.ellipse([cx-w+2, base-2*h+2, cx+w-2, base-2], fill=(120,220,130))
    d.rectangle([cx-4, base-h-2, cx-2, base-h], fill=(30,40,30))   # eyes
    d.rectangle([cx+2, base-h-2, cx+4, base-h], fill=(30,40,30))
slime = Image.new("RGBA", (2*TS, TS), (0,0,0,0))
sd = ImageDraw.Draw(slime)
slime_frame(sd, 0, 0, 0); slime_frame(sd, TS, 0, 3)
save(slime, "slime.png")

# ---- items -----------------------------------------------------------------
def heart():
    im = Image.new("RGBA", (24,24), (0,0,0,0)); d = ImageDraw.Draw(im)
    d.ellipse([3,4,12,13], fill=(224,64,80)); d.ellipse([11,4,20,13], fill=(224,64,80))
    d.polygon([(4,10),(20,10),(12,21)], fill=(224,64,80))
    d.ellipse([6,6,9,9], fill=(255,180,190))
    return im
save(heart(), "heart.png")

def key():
    im = Image.new("RGBA", (24,24), (0,0,0,0)); d = ImageDraw.Draw(im)
    d.ellipse([3,3,13,13], outline=(240,200,70), width=3)
    d.rectangle([11,9,13,20], fill=(240,200,70)); d.rectangle([13,16,18,18], fill=(240,200,70))
    d.rectangle([13,12,17,14], fill=(240,200,70))
    return im
save(key(), "key.png")

def chest():
    im = Image.new("RGBA", (32,32), (0,0,0,0)); d = ImageDraw.Draw(im)
    d.rectangle([5,14,27,28], fill=(150,96,48)); d.rectangle([5,14,27,17], fill=(120,74,36))
    d.rectangle([5,8,27,16], fill=(176,120,60))                       # lid
    d.line([5,16,27,16], fill=(96,60,30), width=1)
    d.rectangle([14,15,18,21], fill=(240,210,90)); d.point((16,18), fill=(120,90,20))  # lock
    return im
save(chest(), "chest.png")

def sword():   # direction-agnostic slash sparkle
    im = Image.new("RGBA", (24,24), (0,0,0,0)); d = ImageDraw.Draw(im)
    d.arc([2,2,21,21], 300, 60, fill=(255,255,255), width=3)
    d.arc([5,5,18,18], 300, 60, fill=(200,230,255), width=2)
    d.line([12,2,12,7], fill=(255,255,255), width=2)
    return im
save(sword(), "sword.png")

# ---- level -----------------------------------------------------------------
W, H = 30, 20
GRASS, PATH, FLOWER, SAND, WATER, TREE, WALL = 1,2,3,4,5,6,7
bg    = [GRASS]*(W*H)
solid = [0]*(W*H)
def setb(c,r,g): bg[r*W+c]=g
def sets(c,r,g): solid[r*W+c]=g

# ground: paths (cross), sand clearing, flowers
for c in range(1, W-1): setb(c, 9, PATH)
for r in range(1, H-1): setb(4, r, PATH)
for r in range(15,18):
    for c in range(22,27): setb(c, r, SAND)
for _ in range(24):
    c = random.randint(1,W-2); r = random.randint(1,H-2)
    if bg[r*W+c]==GRASS: setb(c,r,FLOWER)

# solids: border wall, a pond, scattered trees
for c in range(W): sets(c,0,WALL); sets(c,H-1,WALL)
for r in range(H): sets(0,r,WALL); sets(W-1,r,WALL)
for r in range(3,6):
    for c in range(16,20): sets(c,r,WATER)
for (c,r) in [(8,8),(9,8),(12,3),(20,14),(21,14),(24,11)]: sets(c,r,TREE)

def obj(name, c, r): return {"name":name, "x":float(c*TS+TS//2), "y":float(r*TS+TS//2), "point":True}
objects = [obj("p",4,4), obj("s",11,6), obj("s",15,13), obj("s",23,8),
           obj("h",7,14), obj("k",25,4), obj("g",26,16)]
# sanity: no object sits on a solid tile
for o in objects:
    cc=int(o["x"])//TS; rr=int(o["y"])//TS
    assert solid[rr*W+cc]==0, f"object {o['name']} on solid at {cc},{rr}"

level = {
  "width":W, "height":H, "tilewidth":TS, "tileheight":TS, "orientation":"orthogonal",
  "tilesets":[{"firstgid":1, "name":"tiles", "image":"adventure/tiles.png",
               "imagewidth":COLS*TS, "imageheight":2*TS, "tilewidth":TS, "tileheight":TS,
               "columns":COLS, "tilecount":8}],
  "layers":[
    {"type":"tilelayer","name":"bg",   "width":W,"height":H,"data":bg},
    {"type":"tilelayer","name":"solid","width":W,"height":H,"data":solid},
    {"type":"objectgroup","name":"entities","objects":objects},
  ],
}
with open(os.path.join(OUT, "level1.tmj"), "w") as f:
    json.dump(level, f)
print(f"  level1.tmj   {W}x{H}, {len(objects)} objects")
print("done ->", os.path.relpath(OUT))
