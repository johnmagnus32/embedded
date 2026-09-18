#!/usr/bin/env python3
"""Art for canvas-flappy into src/assets/flappy/ (all ORIGINAL — not the game's copyrighted art):
  bird.png     a little bird as a 3-frame TILT sheet (up / level / diving); frame chosen by vy.
  pipe.png     the pipe BODY cross-section (74 wide) with cylinder shading; drawn stretched to any
               height via eng_tex_column (sample column x, full tex height mapped onto the span).
  pipecap.png  the wider pipe CAP/lip (fixed height), drawn at each gap end via eng_draw_image.
  ground.png   a 48px-wide ground tile (grass + dirt), tiled horizontally with a scroll offset.
Run: python3 tools/gen_flappy_assets.py
"""
import os, math
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "src", "assets", "flappy")
os.makedirs(OUT, exist_ok=True)
CELL = 40
PIPE_W = 74
CAP_W, CAP_H = 82, 26      # cap overhangs the body by 4px each side; CAP_H must match flappy.c
GROUND_H = 72

def save(im, name):
    im.save(os.path.join(OUT, name)); print("wrote", os.path.join("assets/flappy", name), im.size)

def bird_base():
    im = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    body, belly, wing = (250, 205, 60), (255, 236, 150), (232, 170, 40)
    d.ellipse([7, 11, 33, 33], fill=(120, 96, 30, 255))
    d.ellipse([8, 12, 32, 32], fill=body)
    d.ellipse([12, 20, 28, 31], fill=belly)
    d.ellipse([9, 18, 19, 27], fill=wing)
    d.polygon([(30, 18), (39, 21), (30, 24)], fill=(240, 130, 30, 255))
    d.ellipse([24, 14, 30, 20], fill=(255, 255, 255, 255))
    d.ellipse([26, 16, 29, 19], fill=(30, 25, 20, 255))
    return im

def bird_sheet():
    base = bird_base(); tilts = [20, 0, -30]
    sh = Image.new("RGBA", (CELL * len(tilts), CELL), (0, 0, 0, 0))
    for k, ang in enumerate(tilts):
        sh.paste(base.rotate(ang, resample=Image.BICUBIC, center=(CELL / 2, CELL / 2)), (k * CELL, 0))
    return sh

def tube(t):                       # cylinder shading across the width: bright highlight, dark edges
    hl = math.exp(-((t - 0.32) / 0.24) ** 2)
    b = 0.60 + 0.52 * hl
    base = (72, 176, 94)
    return tuple(min(255, int(v * b)) for v in base)

def pipe_body():                   # 74 x 8, vertically uniform (stretched at draw), shaded per column
    W, H = PIPE_W, 8
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); px = im.load()
    for x in range(W):
        c = (38, 110, 56) if (x < 2 or x > W - 3) else tube(x / (W - 1))   # dark side edges
        for y in range(H): px[x, y] = (c[0], c[1], c[2], 255)
    return im

def pipe_cap():                    # 82 x 26, same shading + a darker rim (the lip) top+bottom
    W, H = CAP_W, CAP_H
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); px = im.load()
    for x in range(W):
        c = (34, 100, 50) if (x < 2 or x > W - 3) else tube(x / (W - 1))
        for y in range(H):
            cc = tuple(int(v * 0.62) for v in c) if (y < 3 or y > H - 4) else c   # rim lip
            px[x, y] = (cc[0], cc[1], cc[2], 255)
    return im

def ground():                      # 48 x 72 tile; features kept off the edges so tiling is seamless
    W, H = 48, GROUND_H
    im = Image.new("RGBA", (W, H), (222, 196, 140, 255)); d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W - 1, 11], fill=(120, 200, 96, 255))    # grass
    d.rectangle([0, 0, W - 1, 2],  fill=(150, 220, 120, 255))   # grass highlight
    d.line([(0, 12), (W - 1, 12)], fill=(90, 150, 70, 255))     # seam
    for (x, y) in [(10, 26), (30, 34), (18, 50), (38, 60), (6, 44), (26, 21)]:
        d.ellipse([x, y, x + 5, y + 4], fill=(200, 172, 118, 255))  # dirt pebbles
    return im

save(bird_sheet(), "bird.png")
save(pipe_body(),  "pipe.png")
save(pipe_cap(),   "pipecap.png")
save(ground(),     "ground.png")
