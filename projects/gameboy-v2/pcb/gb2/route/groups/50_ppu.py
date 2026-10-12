# MCU <-> FPGA (channel P, front). One SPI1 bus does configuration and runtime (2026-10-12): the MCU's left pins
# 20/21/23 (PA4 select, PA5 SCK, PA7 MOSI) go to U2's config pins on its top edge (SPI_SS 16, SPI_SCK 15, SPI_SI 17),
# which the PPU gateware reuses as its command bus after configuration. CRESET_B / CDONE (pins 27/28) run straight
# to U2's right pins 8/7: two parallel lines, one 45 deg drop each. CRESET_B reaches its back-side pull-up R4 through
# one via at the U2 end; CDONE drops to its pull-up R5 and the CDONE LED D2 (front, just below the lane).
# At the FPGA the config pins read MOSI / SS_B / SCK west -> east, while the MCU's SPI1 pins come out SCK above
# MOSI: whichever way the bundle goes, MOSI and SCK cross once - MOSI takes one short hop over the back.
W = 0.15
LANE = (("FPGA_CRESET_B", "U1.27", "U2.8", 69.5), ("FPGA_CDONE", "U1.28", "U2.7", 69.5))
for net, mp, up, x_end in LANE:
    (mx, my), (ux, uy) = P(mp), P(up)
    x_turn = x_end + (uy - my)                    # 45 deg: x + y is constant along the drop
    T(net, "F", W, [(mx, my), (x_turn, my), (x_end, uy), (ux, uy)])
# CRESET_B pull-up (R4, back): via on the line just outside U2's pin 8, east between C22 and TP7, under U3's
# NC pad to R4 (keeps U2.5 / C13's +1V2 via spot free)
cv = (69.0, P("U2.8")[1])
VIA("FPGA_CRESET_B", *cv, d=0.45, h=0.25)
T("FPGA_CRESET_B", "B", W, [cv, (69.17, 59.51), (71.0, 59.51), (71.49, 60.0), (73.2, 60.0), (74.0, 60.8), P("R4.1")])  # C22 / TP7 gap, under U3.4
# CDONE pull-up + LED: straight down from the drop to R5, then along to D2; D2 -> R7 (LED resistor)
cx = 70.0
drop = (cx, P("U2.7")[1] - (cx - 69.5))           # the point on CDONE's 45 deg drop at x = 70.0
T("FPGA_CDONE", "F", W, [drop, (cx, P("R5.1")[1])])
T("FPGA_CDONE", "F", W, [P("R5.1"), P("D2.1")])
T("CDONE_LED_A", "F", W, [P("D2.2"), P("R7.2")])

# ---- the SPI1 bus to U2's config pins
T("FPGA_SS_B", "F", W, [P("U2.16"), P("R6.1")])                  # the slave-boot strap right above its pin
m = MAZE((62.0, 49.5, 84.0, 60.0))
AUTOROUTE(m, "PPU_SPI_SCK", (*P("U1.21"), "F"), (*P("U2.15"), "F"), layers="F")
AUTOROUTE(m, "FPGA_SS_B", (*P("U1.20"), "F"), (*P("R6.1"), "F"), layers="F")
AUTOROUTE(m, "PPU_SPI_MOSI", (*P("U1.23"), "F"), (*P("U2.17"), "F"), layers="FB", via_cost=30.0)
