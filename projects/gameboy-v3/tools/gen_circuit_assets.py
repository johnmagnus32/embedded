#!/usr/bin/env python3
"""NIGHT-themed textures + sprites for canvas-circuit into src/assets/circuit/ (all ORIGINAL,
deterministic — periodic/low-entropy so the web-sim frames still compress):
  road.png     dark night asphalt + bright white edges + glowing yellow dashes (u across, v along)
  grass.png    dark night grass, faint speckle
  barrier.png  NEON guardrail — bright cyan/magenta stripes (drawn emissive, not fogged)
  car.png      the PLAYER car as a 5-frame steering SHEET (rear 3/4 view; red tail-lights; lean per frame)
  skyline.png  a wide panoramic CITY skyline (transparent sky, lit windows) scrolled by heading
  glow.png     a soft radial white glow (tinted at draw) for headlights / rival tail-lights
Run: python3 tools/gen_circuit_assets.py
"""
import os
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "src", "assets", "circuit")
os.makedirs(OUT, exist_ok=True)
T = 128

def save(im, name): im.save(os.path.join(OUT, name)); print("wrote", os.path.join("assets/circuit", name), im.size)

def road():
    im = Image.new("RGBA", (T, T), (30, 30, 38, 255)); d = ImageDraw.Draw(im)
    for y in range(0, T, 6): d.line([(0, y), (T-1, y)], fill=(26, 26, 33, 255))       # asphalt banding
    d.rectangle([0, 0, 7, T-1], fill=(210, 210, 220, 255))                             # left edge (bright)
    d.rectangle([T-8, 0, T-1, T-1], fill=(210, 210, 220, 255))
    for y in range(0, T, 1):
        if (y // 24) % 2 == 0: d.rectangle([T//2-4, y, T//2+3, y], fill=(250, 220, 90, 255))  # glowing dashes
    return im

def grass():
    im = Image.new("RGBA", (T, T), (18, 44, 26, 255)); px = im.load()
    for y in range(T):
        for x in range(T):
            if ((x*7 + y*13) % 23) < 3:  px[x, y] = (14, 36, 22, 255)
            elif ((x*5 + y*3) % 29) < 2: px[x, y] = (26, 58, 34, 255)
    return im

def barrier():                                     # neon cyan/magenta stripes
    im = Image.new("RGBA", (T, T), (40, 220, 255, 255)); d = ImageDraw.Draw(im)
    for x in range(0, T, 64): d.rectangle([x+32, 0, x+63, T-1], fill=(255, 70, 210, 255))
    d.rectangle([0, 0, T-1, 6], fill=(70, 70, 90, 255))                                # dark rails
    d.rectangle([0, T-7, T-1, T-1], fill=(70, 70, 90, 255))
    return im

def car_sheet():                                   # 5 steering frames, rear view
    FW, FH, N = 128, 88, 5
    sh = Image.new("RGBA", (FW*N, FH), (0, 0, 0, 0))
    for i in range(N):
        lean = i - 2                               # -2..+2
        im = Image.new("RGBA", (FW, FH), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
        cx = FW//2; sh_top = lean*7                # roof shifts with lean (banking)
        body, dark, glass = (170, 34, 40), (110, 22, 26), (30, 34, 44)
        d.polygon([(20, FH-6), (FW-20, FH-6), (FW-30, 30), (30, 30)], fill=body)       # rear body
        d.rectangle([16, FH-20, FW-16, FH-6], fill=dark)                               # rear bumper
        d.polygon([(34+sh_top, 30), (FW-34+sh_top, 30), (FW-42+sh_top, 6), (42+sh_top, 6)], fill=glass)  # rear window/roof
        d.rectangle([26, 40, 52, 54], fill=(255, 70, 60)); d.rectangle([FW-52, 40, FW-26, 54], fill=(255, 70, 60))  # tail-lights
        d.rectangle([30, 43, 48, 51], fill=(255, 180, 150)); d.rectangle([FW-48, 43, FW-30, 51], fill=(255, 180, 150))  # tail cores
        d.rectangle([cx-16, 44, cx+16, 50], fill=(90, 16, 18))                         # center strip
        d.rectangle([10, FH-24, 26, FH-4], fill=(20, 20, 24)); d.rectangle([FW-26, FH-24, FW-10, FH-4], fill=(20, 20, 24))  # wheels
        d.polygon([(30+sh_top, 26), (FW-30+sh_top, 26), (FW-30+sh_top, 30), (30+sh_top, 30)], fill=dark)  # spoiler
        sh.paste(im, (i*FW, 0))
    return sh

def skyline():                                     # wide neon city panorama (transparent above buildings)
    W, H = 1600, 160
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    x = 0; seed = 1
    while x < W:
        seed = (seed*1103515245 + 12345) & 0x7fffffff
        bw = 34 + (seed >> 8) % 46
        seed = (seed*1103515245 + 12345) & 0x7fffffff
        bh = 40 + (seed >> 7) % 100
        col = (18 + (seed % 14), 20 + (seed % 18), 40 + (seed % 26))
        d.rectangle([x, H-bh, x+bw-4, H-1], fill=col + (255,))
        for wy in range(H-bh+6, H-6, 12):          # lit windows
            for wx in range(x+4, x+bw-8, 10):
                seed = (seed*1103515245 + 12345) & 0x7fffffff
                if (seed >> 5) % 3:
                    lit = (255, 220, 120) if (seed >> 9) % 4 else (120, 220, 255)
                    d.rectangle([wx, wy, wx+4, wy+6], fill=lit + (255,))
        x += bw
    return im

def glow():                                        # soft radial white glow (alpha falloff)
    S = 176; im = Image.new("RGBA", (S, S), (0, 0, 0, 0)); px = im.load(); c = S/2.0
    for y in range(S):
        for x in range(S):
            dpx = (x-c)/c; dpy = (y-c)/c; dd = (dpx*dpx + dpy*dpy) ** 0.5
            a = max(0.0, 1.0 - dd); a = a*a
            px[x, y] = (255, 255, 255, int(a*255))
    return im

save(road(), "road.png"); save(grass(), "grass.png"); save(barrier(), "barrier.png")
save(car_sheet(), "car.png"); save(skyline(), "skyline.png"); save(glow(), "glow.png")
