# PPU lane (channel P, front): MCU left pins 20/21/23/27/28 -> U2 right pins 12/11/10/8/7 - PPU_SPI CS/SCK/MOSI,
# FPGA_CRESET_B, FPGA_CDONE. The pin order is the same at both ends, so it is five parallel lines: west off the MCU,
# one 45 deg drop each (the drops are parallel, >= 0.35 mm apart), west into U2. Equal-length to within 1 mm, no
# vias on the SPI lines. CRESET_B reaches its back-side pull-up R4 through one via at the U2 end; CDONE drops to its
# pull-up R5 and the CDONE LED D2 (front, just below the lane).
W = 0.15
LANE = (("PPU_SPI_CS", "U1.20", "U2.12", 70.0), ("PPU_SPI_SCK", "U1.21", "U2.11", 70.0),
        ("PPU_SPI_MOSI", "U1.23", "U2.10", 70.0), ("FPGA_CRESET_B", "U1.27", "U2.8", 69.5),
        ("FPGA_CDONE", "U1.28", "U2.7", 69.5))
for net, mp, up, x_end in LANE:
    (mx, my), (ux, uy) = P(mp), P(up)
    x_turn = x_end + (uy - my)                    # 45 deg: x + y is constant along the drop
    T(net, "F", W, [(mx, my), (x_turn, my), (x_end, uy), (ux, uy)])
# CRESET_B pull-up (R4, back): via on the line just outside U2's pin 8, east between C22 and TP7, under U3's
# NC pad to R4 (keeps U2.5 / C13's +1V2 via spot free)
cv = (69.0, P("U2.8")[1])
VIA("FPGA_CRESET_B", *cv, d=0.45, h=0.25)
T("FPGA_CRESET_B", "B", W, [cv, (69.17, 59.51), (71.0, 59.51), (71.5, 60.0), (73.2, 60.0), P("R4.1")])  # C22 / TP7 gap, under U3.4
# CDONE pull-up + LED: straight down from the drop to R5, then along to D2; D2 -> R7 (LED resistor)
cx = 70.0
drop = (cx, P("U2.7")[1] - (cx - 69.5))           # the point on CDONE's 45 deg drop at x = 70.0
T("FPGA_CDONE", "F", W, [drop, (cx, P("R5.1")[1])])
T("FPGA_CDONE", "F", W, [P("R5.1"), P("D2.1")])
T("CDONE_LED_A", "F", W, [P("D2.2"), P("R7.2")])
