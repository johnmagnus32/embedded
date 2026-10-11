#!/usr/bin/env python3
"""nor_fit.py — does a bundle fit the board's SPI-NOR? Called by flash.sh BEFORE anything is written.

    nor_fit.py <nor-size> <table-off> <kernel-off> <kernel-size> <dtb-off> <dtb-size> <initrd-off> <initrd-size>

Every component must fit the part and none may overlap another. A kernel that outgrows its region is
the failure this exists to catch: the dtb and initramfs are written AFTER it, at fixed offsets, so an
over-long kernel is silently corrupted by the next write and the board hangs with no clue why.
Exit 0 = fits; 1 = prints each problem and the offsets to change.
"""
import sys

TABLE_BYTES = 0x2c   # struct nor_table (bootloader/nor_layout.h)


def main(argv):
    if len(argv) != 9:
        print(__doc__.strip())
        return 2
    cap = int(argv[1], 0)
    parts = [
        ("table",     int(argv[2], 0), TABLE_BYTES),
        ("kernel",    int(argv[3], 0), int(argv[4])),
        ("dtb",       int(argv[5], 0), int(argv[6])),
        ("initramfs", int(argv[7], 0), int(argv[8])),
    ]
    bad = []
    for name, off, size in parts:
        if off + size > cap:
            bad.append("%s 0x%x + %d bytes ends at 0x%x, past the %d MiB part"
                       % (name, off, size, off + size, cap // (1 << 20)))
    for i, (n1, o1, s1) in enumerate(parts):
        for n2, o2, s2 in parts[i + 1:]:
            if o1 < o2 + s2 and o2 < o1 + s1:
                bad.append("%s 0x%x + %d overlaps %s 0x%x + %d (move %s to 0x%x or later)"
                           % (n1, o1, s1, n2, o2, s2, n2, (o1 + s1 + 0xffff) & ~0xffff))
    for line in bad:
        print("  " + line)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
