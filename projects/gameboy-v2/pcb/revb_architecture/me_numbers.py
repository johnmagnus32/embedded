"""Hand calculations behind the rev B mechanical architecture choice. Run: .venv/bin/python me_numbers.py
Units: mm, N, MPa (= N/mm^2). Assumptions are marked ASSUME; everything else cites a file/page."""
from math import sqrt

print("== 1. Foam in the swell space pushes on whatever the cell's other face touches ==")
cell_face = 38.5 * 62.0                                   # PKCELL p.5 max outline, mm^2
# Rogers PORON 4701-30 product page: CFD @ 25 % = 21 / 35 / 62 kPa for the -15 / -20 / -25 grades
# (thickness ranges 4.78-12.70 / 1.57-3.18 / 0.79-1.14 mm). Only the -25 grade exists at the 0.85-1.0 mm the studies use.
for grade, kpa in (("-15", 21.0), ("-20", 35.0), ("-25", 62.0)):
    print(f"  PORON 4701-30{grade}: {kpa:4.0f} kPa x {cell_face:.0f} mm^2 = {kpa / 1000 * cell_face:5.0f} N at only 25 % compression")
print("  compare: one face key needs 1.6 N (TS-1187A, BOM.md:176). A swollen cell squeezing 0.85-1.0 foam toward"
      " ~100 % compression pushes far harder than the 25 % figure.")

print("\n== 2. Lens stiffness vs lens-to-panel gap ==")
acr_per10N = 0.4                  # 1.5 cast acrylic over the 71 x 51 opening, ~0.4 mm per 10 N (STRUCTURAL_REVIEW.md:21)
glass_per10N = acr_per10N / 8.6   # 1.1 glass is 8.6x stiffer (same line)
for name, k, gap in (("1.5 acrylic, 0.5 worst gap (T/F)", acr_per10N, 0.5),
                     ("1.5 acrylic, 0.5 gasket (R fallback)", acr_per10N, 0.5),
                     ("1.1 glass, 0.3 gasket (R)", glass_per10N, 0.3)):
    print(f"  {name:38s}: {k:5.3f} mm/10 N -> lens touches panel at ~{gap / k * 10:4.0f} N")

print("\n== 3. Lens-to-panel gap tolerance chain ==")
# T: the gap is what is left between two surfaces located by different parts
t_chain = {"panel 4.00 +/-0.20 (MI P.5 note 11)": 0.20, "tape 0.115 (ASSUME +/-0.02)": 0.02,
           "board clamp: shell boss tip (ASSUME FDM +/-0.10)": 0.10, "lens pocket depth (ASSUME FDM +/-0.10)": 0.10,
           "local board flatness under the panel (ASSUME +/-0.10)": 0.10}
r_chain = {"gasket 0.30 (ASSUME +/-15 %)": 0.045}
for name, ch in (("T", t_chain), ("R", r_chain)):
    print(f"  {name}: worst {sum(ch.values()):.3f}, RSS {sqrt(sum(v * v for v in ch.values())):.3f}  <- {', '.join(ch)}")
print("  -> T needs 0.5 worst-case + margin (0.7 nominal, DESIGN.md:60-61); R's gap is the gasket itself, the panel"
      " tolerance moves to the back clearance below.")
back = {"panel +/-0.20": 0.20, "gasket +/-0.045": 0.045, "land-to-boss-tip (FDM, ASSUME +/-0.10)": 0.10,
        "board flatness (ASSUME +/-0.10)": 0.10}
rss = sqrt(sum(v * v for v in back.values()))
print(f"  R back clearance 0.30 nominal: RSS {rss:.2f} -> min {0.30 - rss:+.2f} (RSS), {0.30 - sum(back.values()):+.2f}"
      " worst = flat contact of the panel's back on solder mask, no point load (zero-height zone)")

print("\n== 4. Board bending under a hard face-key press, no post (DESIGN.md:98-99: ~0.2 mm at t = 1.6) ==")
for t in (1.6, 1.2, 1.0):
    print(f"  t = {t}: stiffness {(t / 1.6) ** 3 * 100:3.0f} % of 1.6 -> ~{0.2 * (1.6 / t) ** 3:.2f} mm "
          f"vs key travel 0.2 +/-0.1, stop 0.3 (DESIGN.md:67)")
print("  -> any t < 1.6 needs a back-shell post 0.05 under every face switch (8 posts); then t no longer sets key feel.")

print("\n== 5. Battery plate: stays off the board under a 20 N press on the door (STRUCTURAL_REVIEW #7 load) ==")
# Roark 7th ed. Table 11.4 case 1a (all edges simply supported, uniform q): y = alpha q b^4 / (E t^3), alpha(a/b=1.6)=0.0906
a, b = 63.0, 40.0                 # plate span between its ledges (bay walls + a rib at x ~104), mm  (this study)
alpha = 0.0890                    # a/b = 1.58, interpolated
q = 20.0 / cell_face              # 20 N spread by the (stiff) cell over its face, MPa
mats = {"304 stainless 0.5": (193e3, 0.5, 7.93), "5052 aluminium 0.5": (70e3, 0.5, 2.68),
        "FR4 0.8 (JLC)": (20e3, 0.8, 1.85), "FR4 0.6 (JLC)": (20e3, 0.6, 1.85), "PETG 1.0 printed": (2.0e3, 1.0, 1.27)}
for n, (E, t, rho) in mats.items():   # ASSUME E: steel 193, Al 70, FR4 ~20 (in-plane flexural, typical), PETG 2.0 GPa
    y = alpha * q * b ** 4 / (E * t ** 3)
    gap = max(0.2, round(y + 0.1, 1))
    m = 41 * 66 * t * rho / 1000
    print(f"  {n:20s}: deflection {y:5.2f} mm -> gap {gap:.1f}, z cost {t + gap:4.2f} mm, mass {m:4.1f} g")

print("\n== 6. FPC joggle drop for R (REVB_DESIGN.md §3.3: h = t + 0.065 + 0.88 when taped) ==")
for name, lift, t in (("T t=1.0", 0.0, 1.0), ("T t=1.6", 0.0, 1.6), ("R t=1.2", 0.30, 1.2)):
    h = t + lift + 0.065 + 0.88
    used = h - 0.855              # linear fit to stack.py's 1.09 @ h 1.945 and 1.69 @ h 2.545
    u_end = 25.00 - used          # folded tail end 25.00 from the module edge (MI P.5 folded view)
    print(f"  {name}: drop {h:.3f}, length used {used:.2f}, FPC end u {u_end:.2f} -> x {132.55 - u_end:.2f}")
print("  -> R's J3 origin ~ x 107.80 (between REVB §3.4's t=1.2 and t=1.6 rows); re-run stack.py with lift 0.30.")

print("\n== 7. Metal near the BT830 antenna (EZ-DS-BT830 v3.0 §12.1.1 p.27: 30 mm L/R best, <20 mm 'significantly degrade') ==")
u12_x = 37.78                      # U12 bbox right edge (REVB_DESIGN.md §3.8)
for n, x in (("panel frame", 63.25), ("cell (rev B position)", 65.6), ("cell moved 0.6 left (R)", 65.0),
             ("battery plate left edge (R, kept >= panel edge)", 63.5)):
    print(f"  {n:46s}: {x - u12_x:5.1f} mm")
print("  -> the plate adds no metal closer to U12 than the panel's own frame already is.")

print("\n== 8. Air gap vs optical bonding: Fresnel loss at normal incidence ==")
for n in (1.49, 1.52):
    R = ((n - 1) / (n + 1)) ** 2
    print(f"  n = {n}: {R * 100:.1f} % per air interface -> air gap adds 2 interfaces = {2 * R * 100:.1f} % more ambient glare")

print("\n== 9. Face-plate floor over the tallest front parts (board.json heights) ==")
for opt, fh in (("T", 6.315), ("R", 5.815), ("R + IPS", 4.615), ("R + acrylic 1.5/0.5", 6.415)):
    room = fh - 1.6
    print(f"  {opt:20s}: plate inner face at board top + {room:.3f}; SKRT 3.35 / J7 3.25 / L2 3.1 need >= 3.65 -> "
          f"{'OK' if room >= 3.65 else 'needs local plate thinning to %.2f' % (fh - 3.65)}")
