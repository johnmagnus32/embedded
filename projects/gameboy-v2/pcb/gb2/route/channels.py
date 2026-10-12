# gb2 signal channels: the reserved corridors every route group has to respect (see route/PLAN.md).
# Each: name -> (layer, polygon in board mm, what runs there). plot.py --channels draws them over the board.
# SOFT: wide bands that routes weave through - plane vias may land in them (decoupling beats a channel slot);
# every other channel is a choke point where a via would cost a slot, so fanout keeps out.
CHANNELS = {
    # west: everything bound for the left grip crosses J3's pin column (both ends are keep-outs) on the BACK,
    # two traces per gap, in the 4 gaps above the FPGA's back-side decaps
    "W1 west bundle":      ("B", [(62.0, 47.4), (84.5, 47.4), (84.5, 49.3), (82.0, 49.3), (82.0, 51.5), (79.0, 54.4),
                                  (60.2, 54.4), (60.2, 56.4), (58.1, 56.4), (58.1, 47.4)],
                            "BT x5, PWR_SW (from the MCU's top-left), I2C x2 (from under the MCU) -> J3 gaps 48.1/50.7/53.2/55.8"),
    # south-west: J3's east side below the FPGA (the board is cut away east of x 62.2 here)
    "W2 LCD control":      ("F", [(58.1, 63.4), (68.6, 63.4), (68.6, 64.6), (61.7, 64.6), (61.7, 92.0), (58.1, 92.0)],
                            "LCD_CS / DC / WR / RST from U2's bottom pins down J3's east side (LCD_LITE on the back)"),
    "W2b LCD_LITE":        ("B", [(61.0, 62.6), (87.0, 62.6), (87.0, 64.6), (61.7, 64.6), (61.7, 78.0), (58.1, 78.0), (58.1, 62.6)],
                            "LCD_LITE from the MCU's bottom pins along the board's bottom edge, under the FPGA"),
    # MCU <-> FPGA, front: the straight PPU lane
    "P  PPU lane":         ("F", [(64.8, 52.9), (81.6, 52.9), (81.6, 57.6), (68.8, 60.1), (64.8, 56.2)],
                            "SPI1 SCK/MOSI/select to U2's config pins (top edge), FPGA_CRESET_B, FPGA_CDONE"),
    # under the MCU body on the back: the hub between its back-side decap rings
    "H  MCU hub":          ("B", [(84.3, 51.8), (92.2, 51.8), (92.2, 59.4), (84.3, 59.4)],
                            "I2C x2 + BT_EN west to W1 (the FPGA config bus left SPI3 on 2026-10-12)"),
    # east: everything for the right grip crosses J4's pin column (both ends keep-outs); two traces per gap per layer
    "E1 east, back":       ("B", [(95.0, 49.4), (131.0, 49.4), (131.0, 58.8), (95.0, 60.0)],
                            "BTN_A, VOL+, VOL-, BTN_R (MCU right side) -> J4 gaps 50.7/53.2/55.8"),
    "E2 east, front":      ("F", [(84.0, 62.2), (131.0, 62.2), (131.0, 64.7), (84.0, 64.7)],
                            "I2S x3, BTN_B (MCU bottom side) under NOR and J8 -> J4 gaps 60.7..63.6 (WS/CK dive to the back at x 118/119.5)"),
    "A  amp feed":         ("B", [(96.6, 44.7), (129.5, 44.7), (133.0, 47.6), (133.0, 48.7), (128.6, 45.7), (96.6, 45.7)],
                            "SYS to the amp: Q1 -> J4 gap 48.1 (0.5 mm neck) -> across the right grip -> amp"),
    # north: under J7 on the back, between its shell-tab rows
    "N  charger signals":  ("B", [(86.6, 34.7), (103.6, 34.7), (103.6, 36.2), (88.0, 36.2), (88.0, 42.8), (86.6, 42.8)],
                            "BAT (sense tap), CHG_PGOOD, CHG_STAT: west under J7, down to the MCU's top pins / R26"),
}

SOFT = {"E1 east, back", "E2 east, front"}
