#!/usr/bin/env python3
"""tests/back/render_back.py - headless renders of back.py's features (back.py alone on the bare shells), into
build/back/<variant tag>/. Run from the enclosure folder:  .venv/bin/python tests/back/render_back.py [--fast]

  inside.png      the back shell from the front (+z down onto it), plate + cell + LRA + speaker, no board
  inside_iso.png  the same from an angle
  outside.png     the back face (mirrored: KiCad +x to the left)
  close_*.png     clipped close-ups: USB-C + microSD from outside, the cell bay corner + leads gap, the snap, the
                  speaker cup + S2, the reset tube
  sec_*.png       sections through each feature with the context bodies"""
import os, sys, time
HERE = os.path.dirname(os.path.abspath(__file__))
ENC = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, ENC)
os.chdir(ENC)
os.environ.setdefault("GB3_QUIET", "1")
import params as P
import enclosure
import render as R
import back as B

TAG = P.VARIANT.replace(" ", "_").replace("=", "-")
OUT = os.path.join(HERE, TAG)
os.makedirs(OUT, exist_ok=True)


def clip(shape, x0, y0, z0, x1, y1, z1):
    c = P.box((x0 + x1) / 2, (y0 + y1) / 2, x1 - x0, y1 - y0, z1 - z0, 0) if False else None
    b = P.box((x0 + x1) / 2, (y0 + y1) / 2, x1 - x0, y1 - y0, z0, z1)
    try:
        r = shape & b
    except Exception:
        return None
    return r if r is not None and r.solids() else None


def main():
    t0 = time.time()
    ctx = enclosure.make_ctx()
    r = B.build(ctx)
    back = ctx.back.fuse(*r["back_add"]).clean().cut(*r["back_cut"]).clean()
    front = ctx.front.cut(*r["front_cut"]).clean() if r["front_cut"] else ctx.front
    cb = {n: (s, c) for n, (s, c) in ctx.context.items()}
    plate = r["parts"].get("battery plate")
    shell_col, plate_col = "#b9bec4", B.COLOURS["battery plate"]
    print(f"built {time.time() - t0:.1f} s")
    zl = [(P.SPLIT, "split"), (P.Z_BOARD_BOT, "board bottom"), (P.Z_FLOOR_IN, "floor inner"), (P.Z_BACK, "back"),
          (P.CELL["z"][0], "cell"), (P.CELL["floor_z"][1], "plate top" if plate is not None else "floor top")]
    if P.HUMP:
        zl.append((P.Z_BACK_HUMP, "hump"))
    lg = [("back shell", shell_col)] + ([("battery plate (FR4)", plate_col)] if plate is not None else []) + [ ("cell", "#f08a24"), ("LRA", "#7b3fa0"),
          ("speaker", "#d4af37"), ("board", "#1f6f3f"), ("B parts", "#c8a07a")]

    def items(extra=(), with_ctx=("cell", "lra", "speaker", "speaker foam"), alpha_ctx=1.0, back_alpha=1.0):
        it = [("back", back, shell_col, back_alpha)]
        if plate is not None:
            it.append(("plate", plate, plate_col, 1.0))
        for n in with_ctx:
            if n in cb:
                it.append((n, cb[n][0], cb[n][1], alpha_ctx))
        return it + list(extra)

    done = []
    done.append(R.view(items(with_ctx=()), os.path.join(OUT, "inside.png"), az=-90, el=90, width=2200, height=1100,
                       title=f"back shell from the front, no context ({P.VARIANT})", legend=lg[:2]))
    done.append(R.view(items(), os.path.join(OUT, "inside_ctx.png"), az=-90, el=90, width=2200, height=1100,
                       title=f"back shell + plate + cell / LRA / speaker ({P.VARIANT})", legend=lg[:5]))
    done.append(R.view(items(with_ctx=()), os.path.join(OUT, "inside_iso.png"), az=-70, el=38, width=2200,
                       height=1300, title=f"back shell, iso from the front-left ({P.VARIANT})", legend=lg[:2]))
    done.append(R.view(items(with_ctx=()), os.path.join(OUT, "outside.png"), az=90, el=-90, width=2200, height=1100,
                       title=f"back face (mirrored: KiCad +x to the left) ({P.VARIANT})", legend=lg[:2]))
    if "--fast" in sys.argv:
        return done
    # close-ups (clipped), enclosure frame boxes
    u, m = P.USBC, P.MICROSD
    yo = P.OUTLINE.y1
    zb = P.Z_BACK_MIN
    tops = [("USB-C from outside (front rim at alpha 0.45)", "close_usbc.png",
             (u["x"] - 12, yo - 9, zb - 1, u["x"] + 12, yo + 1, P.SPLIT + 2.5), 70, 15, ["usb_c"]),
            ("USB-C from inside", "close_usbc_in.png",
             (u["x"] - 12, yo - 9, zb - 1, u["x"] + 12, yo + 1, P.SPLIT + 2.5), -60, 18, ["usb_c"]),
            ("microSD from outside", "close_microsd.png", (m["x"] - 13, yo - 8, zb - 1, m["x"] + 13, yo + 1,
                                                            P.SPLIT + 2.5), 70, 15, ["microsd card (latched)"]),
            ("cell bay top-right corner + leads gap", "close_cell_corner.png",
             (148, -60, zb - 1, 186, -46, P.SPLIT + 1), -110, 40, ["cell"]),
            ("snap hook (bottom wall at the cable notch)", "close_snap.png",
             (B.SNAP_X_K - 7, P.OUTLINE.y0 - 1, zb - 1, B.SNAP_X_K + 7, P.OUTLINE.y0 + 8, P.SPLIT + 4.0), 60, 25, []),
            ("speaker cup + S2 + grille", "close_speaker.png",
             (180, -120.5, zb - 1, 215.5, -86, P.SPLIT + 1), -70, 40, []),
            ("reset tube + pinhole", "close_reset.png", (76, -42, zb - 1, 88, -30.5, P.SPLIT), -60, 35, ["B:SW1"])]
    for title, fn, bx, az, el, ctxn in tops:
        it = []
        for n, s, col, a in items(with_ctx=()):
            c = clip(s, *bx)
            if c is not None:
                it.append((n, c, col, 1.0))
        if fn in ("close_snap.png", "close_usbc.png"):
            c = clip(front, *bx)
            if c is not None:
                it.append(("front", c, "#ece6cf", 0.45))
        for n in ctxn:
            c = clip(cb[n][0], *bx)
            if c is not None:
                it.append((n, c, cb[n][1], 1.0))
        done.append(R.view(it, os.path.join(OUT, fn), az=az, el=el, width=1600, height=1100,
                           title=f"{title} ({P.VARIANT})"))
    # sections
    sec_items = items(with_ctx=tuple(n for n in cb), back_alpha=1.0) + [("front", front, "#ece6cf", 0.5)]
    c = P.CELL["rect"]
    secs = [("y", -85.0, [(126.0, 146.0), (176.0, 214.7)], "KiCad y 85: LRA ring, cell fences + plate, speaker side",
             "sec_y85.png"),
            ("y", P.SPEAKER["at"][1], [(178.0, 216.0), (138.0, 150.0)], "KiCad y 102.8: speaker cup + grille, fence",
             "sec_y102.png"),
            ("x", c.cx, [(48.0, 60.0), (112.0, 121.0)], "KiCad x 160.45: the cell bay across (leads gap at the top)",
             "sec_x160.png"),
            ("x", u["x"], [(28.5, 40.0)], "KiCad x 118: USB-C stadium + relief", "sec_x118_usbc.png"),
            ("x", m["x"], [(28.5, 40.0)], "KiCad x 56.4: microSD slot + scallop", "sec_x56_microsd.png"),
            ("x", P.RESET["at"][0], [(28.5, 40.0)], "KiCad x 82: reset pinhole + guide tube", "sec_x82_reset.png"),
            ("x", B.SNAP_X_K, [(110.0, 121.0)], f"KiCad x {B.SNAP_X_K}: snap hook in the front-wall pocket",
             "sec_x81_snap.png"),
            ("x", P.BOSSES["H2"]["at"][0], [(29.0, 41.0)], "KiCad x 180.5: H2 boss + web + counterbore",
             "sec_x180_h2.png"),
            ("y", P.BOSSES["S1"]["at"][1], [(128.0, 146.0), (200.0, 216.0)], "KiCad y 116.3: S1 seam boss, the bottom fence",
             "sec_y116_s1.png")]
    for axis, at, zooms, title, fn in secs:
        try:
            done.append(R.section(sec_items, os.path.join(OUT, fn), axis, at, zlines=zl, zooms=zooms,
                                  title=f"{title} - {P.VARIANT}"))
        except Exception as e:
            print(f"section {fn} failed: {e}")
    print(f"rendered {len(done)} in {time.time() - t0:.1f} s -> {OUT}")
    return done


if __name__ == "__main__":
    main()
