#!/usr/bin/env python3
"""Art for canvas-raycast into src/assets/raycast/: a wall-texture ATLAS (3 tiles of 64x64 laid
out horizontally, so the game samples column = (id-1)*64 + hit), a guard billboard sprite, and a
first-person gun. Deterministic (no RNG); patterns are periodic so the web-sim frames compress.
Run:  python3 tools/gen_raycast_assets.py
"""
import os
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "src", "assets", "raycast")
os.makedirs(OUT, exist_ok=True)

def save(im, name):
    im.save(os.path.join(OUT, name)); print("wrote", os.path.join("assets/raycast", name), im.size)

T = 64

def brick(d, ox):                               # tile 0: red brick with mortar
    d.rectangle([ox, 0, ox+T-1, T-1], fill=(150, 60, 45))
    for r in range(0, T, 16):
        d.line([ox, r, ox+T-1, r], fill=(60, 30, 24))               # mortar rows
        off = 0 if (r // 16) % 2 == 0 else 16
        for c in range(off, T, 32):
            d.line([ox+c, r, ox+c, r+15], fill=(60, 30, 24))        # staggered verticals

def stone(d, ox):                               # tile 1: gray block
    d.rectangle([ox, 0, ox+T-1, T-1], fill=(120, 122, 130))
    for r in range(0, T, 32):
        for c in range(0, T, 32):
            d.rectangle([ox+c+2, r+2, ox+c+29, r+29], outline=(80, 82, 92))

def door(d, ox):                                # tile 2: metal panel with a stripe
    d.rectangle([ox, 0, ox+T-1, T-1], fill=(70, 90, 120))
    d.rectangle([ox+8, 4, ox+T-9, T-5], outline=(160, 190, 220))
    d.rectangle([ox+T//2-3, 8, ox+T//2+2, T-9], fill=(200, 200, 90))  # yellow stripe

def walls():
    im = Image.new("RGBA", (T*3, T), (0, 0, 0, 255)); d = ImageDraw.Draw(im)
    brick(d, 0); stone(d, T); door(d, 2*T)
    return im

def guard():                                    # billboard enemy (transparent bg)
    im = Image.new("RGBA", (T, T), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.ellipse([24, 6, 40, 22], fill=(210, 180, 150))            # head
    d.rectangle([20, 22, 44, 52], fill=(90, 60, 140))           # body
    d.rectangle([14, 26, 20, 46], fill=(90, 60, 140))           # arms
    d.rectangle([44, 26, 50, 46], fill=(90, 60, 140))
    d.rectangle([22, 52, 30, 62], fill=(50, 40, 60)); d.rectangle([34, 52, 42, 62], fill=(50, 40, 60))  # legs
    d.rectangle([44, 34, 60, 40], fill=(40, 40, 46))            # gun
    return im

def gun():                                      # first-person weapon at screen bottom
    W, H = 120, 96
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.rectangle([40, 40, 80, H-1], fill=(70, 72, 80))           # grip
    d.rectangle([48, 10, 72, 44], fill=(50, 52, 60))            # body
    d.rectangle([54, 0, 66, 14], fill=(40, 42, 48))             # barrel
    d.rectangle([50, 12, 70, 18], fill=(90, 92, 104))           # slide highlight
    return im

save(walls(), "walls.png")
save(guard(), "guard.png")
save(gun(),   "gun.png")
