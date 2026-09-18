#!/usr/bin/env python3
"""Generate art for canvas-asteroids into src/assets/asteroids/: a ship (nose points +x, so
node->rot aligns the nose with the heading), one rock (scaled to 3 sizes in-game via node->scale),
and a bullet. Flat shapes / thin outlines only — low entropy so the web-sim frames compress well
(see the adventure speckle->dither lesson). Deterministic (no RNG). Run:
    python3 tools/gen_asteroids_assets.py
"""
import os, math
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "src", "assets", "asteroids")
os.makedirs(OUT, exist_ok=True)

def save(im, name):
    im.save(os.path.join(OUT, name))
    print("wrote", os.path.join("assets/asteroids", name), im.size)

def ship():
    """A triangle whose nose points +x (right). rot=0 => pointing right; the game starts it at
    -pi/2 so it points up. Dark fill + bright outline (classic vector look, but filled)."""
    S = 32
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    nose, top, tail, bot = (30, 16), (5, 5), (12, 16), (5, 27)
    d.polygon([nose, top, tail, bot], fill=(38, 58, 90), outline=(150, 220, 255))
    return im

def rock():
    """An irregular flat gray polygon; one sprite, scaled to big/med/small in-game."""
    S = 40
    c = S / 2.0
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    radii = [0.92, 0.70, 0.96, 0.74, 0.90, 0.66, 0.98, 0.72, 0.86]   # fixed lumpiness (deterministic)
    pts = []
    for i, rr in enumerate(radii):
        a = 2 * math.pi * i / len(radii)
        r = (S / 2.0 - 2) * rr
        pts.append((c + r * math.cos(a), c + r * math.sin(a)))
    d.polygon(pts, fill=(120, 120, 130), outline=(205, 205, 215))
    return im

def bullet():
    S = 8
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse([1, 1, 6, 6], fill=(255, 238, 140), outline=(255, 255, 255))
    return im

save(ship(),   "ship.png")
save(rock(),   "rock.png")
save(bullet(), "bullet.png")
