#!/usr/bin/env python3
"""Art for canvas-racer into src/assets/racer/ (original): car.png (rear-view player car) + tree.png
(a roadside billboard, scaled by distance via eng_tex_column). Run: python3 tools/gen_racer_assets.py
"""
import os
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "src", "assets", "racer")
os.makedirs(OUT, exist_ok=True)
def save(im, n): im.save(os.path.join(OUT, n)); print("wrote", os.path.join("assets/racer", n), im.size)

def car():
    W, H = 88, 52
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    body, dk, glass = (210, 55, 50), (150, 34, 32), (60, 70, 90)
    d.rectangle([6, 40, 20, 50], fill=(25, 25, 28))      # rear wheels
    d.rectangle([68, 40, 82, 50], fill=(25, 25, 28))
    d.rounded_rectangle([10, 20, 78, 46], 6, fill=body)   # body
    d.rounded_rectangle([20, 8, 68, 26], 6, fill=dk)      # cabin
    d.rounded_rectangle([26, 12, 62, 24], 4, fill=glass)  # rear window
    d.rectangle([14, 40, 30, 46], fill=(255, 90, 70))     # tail lights
    d.rectangle([58, 40, 74, 46], fill=(255, 90, 70))
    d.rectangle([10, 45, 78, 49], fill=dk)                # bumper
    return im

def tree():
    W, H = 60, 104
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.rectangle([26, 74, 34, 103], fill=(96, 66, 40))     # trunk
    for (cx, cy, r, c) in [(30, 46, 26, (36, 120, 58)), (18, 40, 18, (48, 140, 70)),
                           (42, 40, 18, (48, 140, 70)), (30, 28, 20, (60, 158, 84))]:
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=c)   # layered canopy
    return im

save(car(), "car.png")
save(tree(), "tree.png")
