#!/usr/bin/env python3
"""Art for canvas-doom into src/assets/doom/ (all ORIGINAL — not id Software's art):
  walls.png  a 4-tile ATLAS of 64x64 wall textures laid out horizontally (the game samples
             column = wtex*64 + (dist along wall & 63)). Tiles: 0 tech, 1 brick, 2 step-ledge, 3 panel.
  floor.png  a 64x64 TILING floor texture (perspective-cast by eng_floor_column, wrapped).
  ceil.png   a 64x64 tiling ceiling texture.
  imp.png    a 64x64 monster BILLBOARD (transparent bg), floor-aligned + depth-tested.
  gun.png    a first-person shotgun at the screen bottom.
Deterministic (no RNG) so the web-sim frames still compress. Run: python3 tools/gen_doom_assets.py
"""
import os
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "src", "assets", "doom")
os.makedirs(OUT, exist_ok=True)
T = 64

def save(im, name):
    im.save(os.path.join(OUT, name)); print("wrote", os.path.join("assets/doom", name), im.size)

def tech(d, ox):                                   # tile 0: green tech paneling with rivets
    d.rectangle([ox, 0, ox+T-1, T-1], fill=(46, 78, 60))
    for r in range(0, T, 16):
        d.line([ox, r, ox+T-1, r], fill=(28, 50, 38))
    for c in range(0, T, 16):
        d.line([ox+c, 0, ox+c, T-1], fill=(30, 54, 40))
    for r in range(8, T, 16):
        for c in range(8, T, 16):
            d.ellipse([ox+c-2, r-2, ox+c+2, r+2], fill=(120, 170, 130))   # rivet

def brick(d, ox):                                  # tile 1: brown brick with mortar
    d.rectangle([ox, 0, ox+T-1, T-1], fill=(126, 74, 48))
    for r in range(0, T, 16):
        d.line([ox, r, ox+T-1, r], fill=(58, 34, 22))
        off = 0 if (r // 16) % 2 == 0 else 16
        for c in range(off, T, 32):
            d.line([ox+c, r, ox+c, r+15], fill=(58, 34, 22))

def ledge(d, ox):                                  # tile 2: metal step/ledge, horizontal ribs
    d.rectangle([ox, 0, ox+T-1, T-1], fill=(96, 100, 112))
    for r in range(0, T, 8):
        d.line([ox, r, ox+T-1, r], fill=(64, 68, 78))
    d.rectangle([ox+2, 2, ox+T-3, T-3], outline=(150, 156, 168))
    d.rectangle([ox+T//2-8, 0, ox+T//2+7, T-1], fill=(196, 176, 70))       # hazard stripe

def panel(d, ox):                                  # tile 3: dark corridor panel
    d.rectangle([ox, 0, ox+T-1, T-1], fill=(58, 60, 70))
    d.rectangle([ox+6, 6, ox+T-7, T-7], outline=(96, 100, 116))
    d.rectangle([ox+T//2-2, 10, ox+T//2+2, T-11], fill=(40, 42, 50))

def walls():
    im = Image.new("RGBA", (T*4, T), (0, 0, 0, 255)); d = ImageDraw.Draw(im)
    tech(d, 0); brick(d, T); ledge(d, 2*T); panel(d, 3*T)
    return im

def floor():                                       # cracked flagstone
    im = Image.new("RGBA", (T, T), (74, 66, 58, 255)); d = ImageDraw.Draw(im)
    for r in range(0, T, 32):
        for c in range(0, T, 32):
            d.rectangle([c+2, r+2, c+29, r+29], outline=(52, 46, 40))
    for (x, y) in [(12, 20), (40, 10), (22, 48), (52, 40), (8, 54)]:
        d.point((x, y), fill=(96, 88, 78))
    return im

def ceil():                                        # dark riveted metal
    im = Image.new("RGBA", (T, T), (40, 44, 54, 255)); d = ImageDraw.Draw(im)
    for r in range(0, T, 16):
        for c in range(0, T, 16):
            d.rectangle([c, r, c+15, r+15], outline=(30, 34, 42))
            d.ellipse([c+6, r+6, c+9, r+9], fill=(64, 70, 84))
    return im

def imp():                                         # original squat clawed monster billboard
    im = Image.new("RGBA", (T, T), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    body, dark, eye = (150, 66, 48), (96, 40, 30), (255, 230, 90)
    d.ellipse([18, 8, 46, 34], fill=body)                       # head
    d.polygon([(18, 14), (10, 2), (22, 12)], fill=dark)         # horns
    d.polygon([(46, 14), (54, 2), (42, 12)], fill=dark)
    d.ellipse([24, 18, 30, 24], fill=eye); d.ellipse([34, 18, 40, 24], fill=eye)
    d.polygon([(26, 28), (32, 34), (38, 28)], fill=(60, 20, 16))  # mouth
    d.rectangle([20, 32, 44, 56], fill=body)                    # torso
    d.polygon([(20, 36), (8, 44), (14, 30)], fill=dark)         # arms/claws
    d.polygon([(44, 36), (56, 44), (50, 30)], fill=dark)
    d.rectangle([24, 56, 30, 64], fill=dark); d.rectangle([34, 56, 40, 64], fill=dark)  # legs
    return im

def gun():                                         # first-person pump shotgun
    Wd, Hd = 150, 104
    im = Image.new("RGBA", (Wd, Hd), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.rectangle([54, 44, 96, Hd-1], fill=(74, 54, 40))          # stock/grip
    d.rectangle([48, 20, 102, 48], fill=(58, 60, 70))           # receiver
    d.rectangle([40, 8, 110, 22], fill=(44, 46, 54))            # barrel + pump
    d.rectangle([44, 24, 106, 30], fill=(96, 100, 116))         # highlight
    d.ellipse([66, 2, 84, 20], fill=(30, 30, 36))               # muzzle
    return im

save(walls(), "walls.png")
save(floor(), "floor.png")
save(ceil(),  "ceil.png")
save(imp(),   "imp.png")
save(gun(),   "gun.png")
