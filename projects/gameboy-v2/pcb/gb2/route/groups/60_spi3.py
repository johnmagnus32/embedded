# MCU right side (route/mcu_escape.py): the east-channel escapes' first legs, BOOT0, then SPI3 / NOR / I2C.
# Fixed geometry first (vias in the planned two-column stagger x 95.2 / 96.0, so neighbours' lines pass between
# them), then the maze routes the rest between fixed endpoints, steered by channel costs.
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)) if "__file__" in dir() else ".", ".."))
W = 0.15

# ---- east-channel (E1) escapes: pin -> via in the stagger column -> first back-side leg east (the east group
# extends these legs to J4; laying them now makes later routes here respect E1's rows)
E1 = (("BTN_R", "U1.62", 96.0), ("IMU_INT1", "U1.57", 95.2), ("BTN_VOLUP", "U1.56", 96.0),
      ("BTN_A", "U1.55", 95.2), ("BTN_VOLDOWN", "U1.50", 96.0))
for net, pin, vx in E1:
    (px, py) = P(pin)
    T(net, "F", W, [(px, py), (vx, py)]); VIA(net, vx, py, d=0.45, h=0.25)
    T(net, "B", W, [(vx, py), (100.5, py)])

# ---- BOOT0: pin 60 -> via -> its pull-down R1 (back, under the pin) and TP8
(px, py) = P("U1.60")
T("BOOT0", "F", W, [(px, py), (96.0, py)]); VIA("BOOT0", 96.0, py, d=0.45, h=0.25)
T("BOOT0", "B", W, [(96.0, py), P("R1.1")])
T("BOOT0", "B", W, [(96.0, py), P("TP8.1")])

# ---- SPI3 / NOR / I2C pull-ups (front), maze
m = MAZE((90.0, 50.5, 108.5, 63.5))
AUTOROUTE(m, "I2C1_SCL", (*P("U1.61"), "F"), (*P("R13.1"), "F"), layers="F")
AUTOROUTE(m, "I2C1_SDA", (*P("U1.59"), "F"), (*P("R14.1"), "F"), layers="F")
AUTOROUTE(m, "NOR_CS", (*P("U1.54"), "F"), (*P("R8.1"), "F"), layers="F")
AUTOROUTE(m, "NOR_CS", (*P("R8.1"), "F"), (*P("U4.1"), "F"), layers="F")
AUTOROUTE(m, "SPI3_MISO", (*P("U1.52"), "F"), (*P("U4.2"), "F"), layers="F")
AUTOROUTE(m, "SPI3_SCK", (*P("U1.51"), "F"), (*P("U4.6"), "F"), layers="F")
AUTOROUTE(m, "SPI3_MOSI", (*P("U1.53"), "F"), (*P("U4.5"), "F"), layers="F",
          through=[(92.3, 58.0, "F"), (93.2, 61.0, "F")])          # inside the pin ring, round the corner, under NOR

