# U1 (STM32F411, LQFP-64) escape plan: for every pin, which way it leaves, on which layer, where its via goes (if
# any) and which channel it joins. Drawn by `plot.py --escape`; the MCU route groups implement it.
#
# The back-side decap ring (C1/C8/C7/C2 west, FB1/C10/C9/C6/C5 north, C4/R1/TP8/C3 east) fences the area under the
# body ("hub"). Its openings: west C8|C7 (y 54.5-57.5, 3.1 mm), north C9|C6 (x 86.2-88.3), east C4|R1 (52.2-53.3),
# R1|TP8 (54.2-55.1), TP8|C3 (56.6-59.4), and the whole south side.
#
# side: N/E/S/W = exit direction off the pin. layer: F = stays front; F>B = front stub, via, then back.
# via: (x, y) or None. ch: channel (route/channels.py) or local destination.
ESCAPE = {
    # ---- top row (pins 16..1, x 84.25 .. 91.75), all leave north on the front
    16: ("USART2_TX",   "N", "F>B", (84.25, 48.55), "W1", "BT830"),
    15: ("USART2_RTS",  "N", "F>B", (84.75, 47.95), "W1", "BT830 (staggered via)"),
    14: ("USART2_CTS",  "N", "F>B", (85.25, 48.55), "W1", "BT830"),
    13: ("+3V3A",       "N", "F>B", (85.75, 47.95), "local", "C9 / C10 / FB1 under the row"),
    12: ("GND",         "N", "F>B", None, "plane", "fanout, straight out"),
    11: ("BB_PSYNC",    "N", "F>B", (84.4, 46.0), "local", "north to y 46, west, back-side column x 84.4 up to R25 / U10.13 (PC3, swapped with BT_EN)"),
    10: ("CHG_PGOOD",   "N", "F>B", (87.2, 41.3), "N", "routed (charger group)"),
    9:  ("CHG_STAT",    "N", "F>B", (87.9, 42.2), "N", "routed (charger group)"),
    8:  ("BAT_SENSE",   "N", "F>B", (88.25, 47.5), "local", "routed; via moves 0.25 north to let PWR_SW pass"),
    7:  ("NRST",        "N", "F>B", (88.75, 48.55), "local", "C6 under the row (inward via, round C6's GND pad), then east to its pad TP16"),
    6:  ("HSE_OUT_MCU", "N", "F", None, "crystal", "R3 straight above"),
    5:  ("HSE_IN",      "N", "F", None, "crystal", "Y1.1 / C11 - nothing else crosses the crystal area"),
    1:  ("+3V3",        "N", "F>B", None, "plane", "fanout"),
    # ---- right row (pins 64..49, y 51.76 .. 59.26): vias alternate x 95.2 / 96.0 so neighbours' lines pass
    64: ("+3V3",        "E", "F>B", None, "plane", "fanout"),
    63: ("GND",         "E", "F>B", None, "plane", "fanout"),
    62: ("BTN_R",       "E", "F>B", (96.0, 52.76), "E1", "J4 gap -> front -> SW13"),
    61: ("I2C1_SCL",    "E", "F",   (95.2, 53.26), "hub->W1", "front east to R13 / U7; west branch through the via, hub C4|R1 gap"),
    60: ("BOOT0",       "E", "F>B", (96.0, 53.76), "local", "R1 (back, under the pin) + TP8"),
    59: ("I2C1_SDA",    "E", "F",   (95.2, 54.26), "hub->W1", "front east to R14 / U7; west branch R1|TP8 gap"),
    58: ("PB6",         "E", "F",   None, "spare", "spare since 2026-10-12 (FPGA_SS_B moved to PA4)"),
    57: ("IMU_INT1",    "E", "F>B", (95.2, 55.26), "E1", "U7 / R12"),
    56: ("BTN_VOLUP",   "E", "F>B", (96.0, 55.76), "E1", "SW10"),
    55: ("BTN_A",       "E", "F>B", (95.2, 56.26), "E1", "SW6"),
    54: ("NOR_CS",      "E", "F",   None, "local", "U4.1 / R8 straight east"),
    53: ("SPI3_MOSI",   "E", "F",   None, "local", "NOR only: inside the pin ring, round the corner, under U4 to pin 5"),
    52: ("SPI3_MISO",   "E", "F",   None, "local", "NOR only"),
    51: ("SPI3_SCK",    "E", "F",   None, "local", "NOR only, between U4 pins 2|3 to pin 6"),
    50: ("BTN_VOLDOWN", "E", "F>B", (96.0, 58.76), "E1", "SW11"),
    49: ("SWCLK",       "E", "F>B", (95.2, 59.26), "local", "down the back to its pad TP14 (bottom-edge debug row)"),
    # ---- bottom row (pins 33..48, x 84.25 .. 91.75), all leave south on the front
    33: ("I2S2_WS",     "S", "F", None, "E2", "amp"),
    34: ("I2S2_CK",     "S", "F", None, "E2", "amp"),
    35: ("BTN_B",       "S", "F", None, "E2", "SW7"),
    36: ("I2S2_SD",     "S", "F", None, "E2", "amp"),
    37: ("LCD_LITE",    "S", "F>B", (86.25, 62.4), "W2b", "routed (display group)"),
    38: ("BT_EN",       "S", "F>B", (86.75, 62.95), "hub->W1", "north through the hub's south side, out its west gap (PC7, swapped with BB_PSYNC)"),
    40: ("MCO2",        "S", "F>B", (88.0, 62.4), "local", "TP9 on the back, under the pin"),
    42: ("USART1_TX",   "S", "F>B", (88.75, 62.35), "local", "console pad TP11 (bottom-edge debug row)"),
    43: ("USART1_RX",   "S", "F>B", None, "local", "console pad TP12"),
    46: ("SWDIO",       "S", "F>B", None, "local", "SWD pad TP13"),
    47: ("GND",         "S", "F>B", None, "plane", "fanout"),
    48: ("+3V3",        "S", "F>B", None, "plane", "fanout"),
    # ---- left row (pins 17..32, y 51.76 .. 59.26)
    17: ("USART2_RX",   "W", "F>B", (80.9, 51.76), "W1", "BT830"),
    18: ("GND",         "W", "F>B", None, "plane", "fanout"),
    19: ("+3V3",        "W", "F>B", None, "plane", "fanout"),
    20: ("FPGA_SS_B",   "W", "F", None, "P", "SPI1 select -> U2 SPI_SS (16) / R6 (config strap; active-high runtime select)"),
    21: ("PPU_SPI_SCK", "W", "F", None, "P", "-> U2 SPI_SCK (15)"),
    23: ("PPU_SPI_MOSI", "W", "F>B", None, "P", "-> U2 SPI_SI (17), one hop under SCK"),
    27: ("FPGA_CRESET_B", "W", "F", None, "P", "routed"),
    28: ("FPGA_CDONE",  "W", "F", None, "P", "routed"),
    30: ("VCAP1",       "W", "F>B", (81.2, 58.1), "local", "C7 under the pin"),
    31: ("GND",         "W", "F>B", None, "plane", "fanout"),
    32: ("+3V3",        "W", "F>B", None, "plane", "fanout"),
}
# the hub's west exit (C8|C7, y 54.5-57.5) carries BT_EN, SCL, SDA (since 2026-10-12, when the FPGA config bus
# moved from SPI3 to SPI1 - it no longer crosses under the MCU). W1 at J3's gaps, north -> south: PWR_SW, USART2
# TX / RTS / CTS / RX, BT_EN, SCL, SDA (2 per gap).
HUB_WEST = ["BT_EN", "I2C1_SCL", "I2C1_SDA"]
PIN_SWAPS = {"BB_PSYNC": ("pin 38 PC7", "pin 11 PC3"), "BT_EN": ("pin 11 PC3", "pin 38 PC7")}
