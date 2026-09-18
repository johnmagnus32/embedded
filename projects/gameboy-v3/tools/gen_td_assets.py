#!/usr/bin/env python3
"""Art for canvas-td (Balloon TD) into src/assets/td/. Original sprites + textures (not copied
from any game). Layout: a 620x480 PLAY field on the left, a 180px shop PANEL on the right.

  balloon.png  glossy GREYSCALE balloon (recolored per layer via eng_draw_image tint).
  monkey.png   shaded dart-monkey (also used, tinted, as the per-defender-type icon/body).
  bg.png       620x480 textured GRASS + a baked dirt PATH (edge + fill + a subtle gravel dither).
  panel.png    180x480 wood shop panel (planks + grain); buttons/text drawn over it in-game.

Everything is deterministic and PERIODIC (tiled grass, block-dither dirt/grain) so the web-sim's
full-frame deflate stays small — NO random speckle (that would inflate every frame).

NB: PATH / PLAY_W / PATH_HALF here MUST match games/td/td.c. Run:  python3 tools/gen_td_assets.py
"""
import os, math
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "src", "assets", "td")
os.makedirs(OUT, exist_ok=True)

def save(im, name):
    im.save(os.path.join(OUT, name)); print("wrote", os.path.join("assets/td", name), im.size)

# ---- layout (keep in sync with td.c) ------------------------------------------------------
PLAY_W, H, PANEL_W = 620, 480, 180
PATH_HALF = 16
PATH = [(-40, 90), (470, 90), (470, 180), (90, 180),
        (90, 290), (500, 290), (500, 390), (-40, 390)]

# ---- palette ------------------------------------------------------------------------------
GRASS, GRASS_HI, GRASS_LO = (60, 120, 66), (78, 144, 84), (50, 106, 58)
DIRT, DIRT_HI, DIRT_LO, DIRT_EDGE = (196, 168, 120), (210, 184, 138), (168, 140, 96), (120, 96, 62)
WOOD, WOOD_HI, WOOD_GROOVE, WOOD_EDGE = (100, 73, 50), (112, 82, 56), (72, 52, 34), (52, 37, 24)

def balloon():
    """28x36 greyscale glossy balloon; tint-modulate recolors per layer."""
    W, Hh = 28, 36
    im = Image.new("RGBA", (W, Hh), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.polygon([(11, 27), (17, 27), (14, 35)], fill=(120, 120, 120, 255))   # knot
    d.ellipse([1, 1, 26, 29], fill=(120, 120, 120, 255))                   # dark rim
    d.ellipse([3, 3, 24, 27], fill=(200, 200, 200, 255))                   # body
    d.ellipse([5, 4, 22, 21], fill=(236, 236, 236, 255))                   # upper
    d.ellipse([8, 6, 15, 14], fill=(252, 252, 252, 255))                   # shine
    return im

def monkey():
    """34x34 shaded dart-monkey; drawn ENG_WHITE for the natural type, or tinted for others."""
    S = 34
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    fur, rim, tan = (122, 86, 56), (92, 64, 42), (224, 194, 152)
    d.ellipse([1, 7, 12, 19], fill=fur);  d.ellipse([22, 7, 33, 19], fill=fur)
    d.ellipse([3, 9, 10, 16], fill=tan);  d.ellipse([24, 9, 31, 16], fill=tan)
    d.ellipse([3, 3, 31, 31], fill=rim)
    d.ellipse([5, 5, 29, 29], fill=fur)
    d.ellipse([9, 14, 25, 29], fill=tan)
    d.ellipse([10, 9, 16, 16], fill=(248, 248, 248)); d.ellipse([18, 9, 24, 16], fill=(248, 248, 248))
    d.ellipse([12, 11, 15, 15], fill=(42, 32, 26));   d.ellipse([20, 11, 23, 15], fill=(42, 32, 26))
    d.ellipse([15, 19, 19, 23], fill=(70, 50, 40))
    return im

def pop_sheet(N=6, C=40):
    """A 'pow' hit burst as a horizontal N-frame strip (each C x C). Frames grow + fade: a jagged
    10-point star (yellow -> orange) with a white-hot core early. Drawn as flat RGBA (per-frame
    alpha), so the game plays cell=t/POP_LIFE*N via eng_draw_image_cell."""
    im = Image.new("RGBA", (N * C, C), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    spikes, cx, cy = 10, C / 2, C / 2
    for f in range(N):
        ox = f * C
        k = f / (N - 1)                                   # 0..1 over the life
        outer, inner = 6 + 13 * k, (6 + 13 * k) * 0.42
        alpha = int(255 * (1.0 - 0.80 * k))               # fade out
        col = (255, int(230 - 90 * k), max(0, int(90 - 80 * k)), alpha)   # yellow -> orange
        pts = []
        for i in range(2 * spikes):
            a = math.pi * i / spikes - math.pi / 2
            r = outer * (0.85 + 0.30 * ((i // 2) % 2)) if i % 2 == 0 else inner   # jagged spikes
            pts.append((ox + cx + math.cos(a) * r, cy + math.sin(a) * r))
        d.polygon(pts, fill=col)
        if k < 0.55:                                      # white-hot core, first frames only
            cr = 4 * (1 - k) + 2
            d.ellipse([ox + cx - cr, cy - cr, ox + cx + cr, cy + cr],
                      fill=(255, 255, 255, int(255 * (1 - k / 0.55))))
    return im

def grass_tile(T=40):
    im = Image.new("RGBA", (T, T), GRASS + (255,)); d = ImageDraw.Draw(im)
    blades = [(5, 31, GRASS_HI), (12, 12, GRASS_LO), (20, 35, GRASS_HI), (27, 17, GRASS_LO),
              (33, 29, GRASS_HI), (8, 22, GRASS_LO), (24, 7, GRASS_HI), (36, 14, GRASS_LO),
              (16, 25, GRASS_HI), (2, 9, GRASS_LO)]
    for x, y, c in blades:
        d.line([(x, y), (x, y - 4)], fill=c + (255,))     # a short blade
    return im

def bg():
    im = Image.new("RGBA", (PLAY_W, H), GRASS + (255,))
    tile = grass_tile()
    for yy in range(0, H, tile.height):
        for xx in range(0, PLAY_W, tile.width):
            im.paste(tile, (xx, yy))
    d = ImageDraw.Draw(im)

    def stroke(width, color):                              # thick polyline + rounded joints
        for i in range(len(PATH) - 1):
            d.line([PATH[i], PATH[i + 1]], fill=color, width=width)
        for i in range(1, len(PATH) - 1):
            r = width // 2
            d.ellipse([PATH[i][0] - r, PATH[i][1] - r, PATH[i][0] + r, PATH[i][1] + r], fill=color)

    stroke(2 * PATH_HALF + 8, DIRT_EDGE + (255,))          # darker edge / kerb
    stroke(2 * PATH_HALF,     DIRT + (255,))               # dirt fill

    mask = Image.new("L", (PLAY_W, H), 0); md = ImageDraw.Draw(mask)  # dirt-fill area only
    for i in range(len(PATH) - 1):
        md.line([PATH[i], PATH[i + 1]], fill=255, width=2 * PATH_HALF)
    for i in range(1, len(PATH) - 1):
        r = PATH_HALF
        md.ellipse([PATH[i][0] - r, PATH[i][1] - r, PATH[i][0] + r, PATH[i][1] + r], fill=255)

    px, mp = im.load(), mask.load()                        # periodic 3-tone gravel dither
    for y in range(H):
        for x in range(PLAY_W):
            if mp[x, y]:
                k = (x // 6 + y // 6) % 3
                if k == 0: px[x, y] = DIRT_HI + (255,)
                elif k == 1: px[x, y] = DIRT_LO + (255,)
    return im

def panel():
    im = Image.new("RGBA", (PANEL_W, H), WOOD + (255,)); d = ImageDraw.Draw(im)
    plank = 46
    for x in range(0, PANEL_W, plank):
        d.rectangle([x, 0, x + plank - 3, H], fill=WOOD_HI + (255,))
        d.line([(x + plank - 2, 0), (x + plank - 2, H)], fill=WOOD_GROOVE + (255,), width=2)
    px = im.load()                                         # periodic horizontal grain
    for y in range(H):
        if (y // 5) % 4 == 0:
            for x in range(PANEL_W):
                r, g, b, a = px[x, y]; px[x, y] = (max(0, r - 8), max(0, g - 6), max(0, b - 4), 255)
    d.rectangle([0, 0, 3, H], fill=WOOD_EDGE + (255,))     # left drop-shadow
    return im

save(balloon(), "balloon.png")
save(monkey(),  "monkey.png")
save(bg(),        "bg.png")
save(panel(),     "panel.png")
save(pop_sheet(), "pop.png")
