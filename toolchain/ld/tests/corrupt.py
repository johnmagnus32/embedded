#!/usr/bin/env python3
"""corrupt.py — craft malformed inputs from valid ones, for ld's fail-loud checks.
  corrupt.py shrink OBJ SECTION SIZE OUT   set SECTION's sh_size (a relocation then lies past its end)
  corrupt.py armap ARCHIVE OUT             the archive index's last name loses its NUL (runs off the index)"""
import struct, sys
def shrink(obj, name, size, out):
    b = bytearray(open(obj, 'rb').read())
    shoff, = struct.unpack_from('<I', b, 0x20); shnum, shstrndx = struct.unpack_from('<HH', b, 0x30)
    stroff, = struct.unpack_from('<I', b, shoff + 40 * shstrndx + 16)
    for i in range(shnum):
        nm, = struct.unpack_from('<I', b, shoff + 40 * i)
        if b[stroff + nm:b.index(0, stroff + nm)].decode() == name: struct.pack_into('<I', b, shoff + 40 * i + 20, int(size))
    open(out, 'wb').write(b)
def armap(ar, out):
    b = bytearray(open(ar, 'rb').read())
    assert b[8:10] == b'/ ', 'no index member first'
    size = int(b[8 + 48:8 + 58]); end = 8 + 60 + size
    k = b.rindex(0, 0, end); b[k] = ord('X')                # the index's final NUL
    open(out, 'wb').write(b)
{'shrink': lambda: shrink(*sys.argv[2:6]), 'armap': lambda: armap(*sys.argv[2:4])}[sys.argv[1]]()
