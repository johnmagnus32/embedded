# East: MCU -> J4 (crossing between its pins, two traces per gap per layer) -> the right grip.
# The back (E1) carries the MCU right-side escapes laid by the spi3 group; the front (E2, the bottom-edge band)
# carries the MCU bottom-side group. Each line is steered through its J4 slot by a waypoint and the maze finds the
# exact path. (SWD / console no longer come here: J1 / J2 became test pads by the MCU, see 65_debug.)
W = 0.15
J4 = lambda k: P("J4.%d" % k)[1]
gap = lambda k: (J4(k) + J4(k + 1)) / 2           # between J4 pin k and k+1 (pin 20 is the top, pin 1 the bottom)
X4 = P("J4.1")[0]
import math
def slots(k):
    """the two trace positions in the gap between J4 pins k and k+1: as far apart as the gap allows, each >= the maze's
    clearance reach from its pad, snapped INWARD onto the 0.05 mm routing grid (rounding outward would put a trace
    inside its pad's halo)."""
    r = P("J4.1"); pad_r = 0.8; reach = 0.15 + 0.075 + 0.03
    lo = J4(k + 1) + pad_r + reach; hi = J4(k) - pad_r - reach         # pin k+1 is above pin k
    return math.ceil(lo / 0.05 - 1e-6) * 0.05, math.floor(hi / 0.05 + 1e-6) * 0.05
S50, S53, S56, S58, S61, S63, S66, S68 = slots(18), slots(17), slots(16), slots(15), slots(14), slots(13), slots(12), slots(11)
m = MAZE((83.0, 36.0, 166.5, 72.0))
E = lambda pin: (100.5, P(pin)[1], "B")           # the end of the spi3 group's leg
# straight through the J4 gap at y; `late` = the lower trace of a pair that both rise into the gap, which has to
# straighten 0.25 mm later so the two parallel diagonals keep their clearance
thru = lambda y, late=False, L="B": [(X4 - 3.5 + (0.25 if late else 0.0), y, L), (X4 + 1.25, y, L)]
VC = 30.0                                         # via cost (cells) - a hop is cheap, a detour isn't
# ---- back (E1). J4 slots, north -> south: BTN_R | VOL+, BTN_A | VOL-. BTN_R first (north to SW13), then the
# ones running east to the far switches. After J4 the maze may change layer (via cost VC) where a crossing needs it.
AUTOROUTE(m, "BTN_R", E("U1.62"), (*P("SW13.2"), "F"), through=thru(S50[1]) + [(X4 + 1.65, S50[1], "F")], via_cost=VC)
AUTOROUTE(m, "BTN_VOLDOWN", E("U1.50"), (*P("SW11.2"), "F"), through=thru(S56[0]), via_cost=VC)
AUTOROUTE(m, "BTN_A", E("U1.55"), (*P("SW6.1"), "F"), through=thru(S53[1], late=True), via_cost=VC)
AUTOROUTE(m, "BTN_VOLUP", E("U1.56"), (*P("SW10.2"), "F"), through=thru(S53[0]), via_cost=VC)
AUTOROUTE(m, "IMU_INT1", E("U1.57"), (*P("R12.1"), "F"), via_cost=VC)
T("IMU_INT1", "F", W, [P("R12.1"), P("U7.4")])
# I2C on to the IMU: from the pull-ups, over the back under the TS line, up beside U7's west pins
AUTOROUTE(m, "I2C1_SCL", (*P("R13.1"), "F"), (*P("U7.13"), "F"), via_cost=VC)
AUTOROUTE(m, "I2C1_SDA", (*P("R14.1"), "F"), (*P("U7.14"), "F"), via_cost=VC)
