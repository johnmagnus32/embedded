#!/usr/bin/env python3
"""relcmp.py A.o B.o — are two relocatable objects (ET_REL) the same object, structurally? For `ld -r` against GNU
ld -r: section ORDER may differ (GNU follows its -r script), so everything is compared by name —
  sections     type, flags, size, alignment, the section a SHF_LINK_ORDER one describes, the bytes;
  symbols      every named symbol: binding, type, visibility, size, its section and offset (locals as a multiset);
  relocations  per section: (offset, type, target) — the target a symbol name, or a section name for section symbols.
.ARM.attributes is compared by presence only (GNU merges attribute sets; we keep the inputs' common one).
Exit 0 = same; else prints the first differences."""
import struct, sys
from collections import Counter

def load(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'\x7fELF' and d[4] == 1, path
    (e_type, _m, _v, _entry, _ph, shoff, _fl, _eh, _phe, _phn, _she, shnum, shstrndx) = struct.unpack_from('<HHIIIIIHHHHHH', d, 16)
    assert e_type == 1, f'{path}: not ET_REL'
    sh = [struct.unpack_from('<IIIIIIIIII', d, shoff + 40 * i) for i in range(shnum)]
    def cstr(off):
        return d[off:d.index(b'\0', off)].decode()
    shstr = sh[shstrndx][4]
    names = [cstr(shstr + s[0]) for s in sh]
    symtab = next(i for i, s in enumerate(sh) if s[1] == 2)
    strtab = sh[symtab][6]
    syms = []
    for k in range(sh[symtab][5] // 16):
        st_name, st_value, st_size, st_info, st_other, st_shndx = struct.unpack_from('<IIIBBH', d, sh[symtab][4] + 16 * k)
        syms.append((cstr(sh[strtab][4] + st_name) if st_name else '', st_value, st_size, st_info, st_other, st_shndx))
    def secname(x):
        return names[x] if 0 < x < 0xff00 else {0: 'UND', 0xfff1: 'ABS', 0xfff2: 'COM'}.get(x, hex(x))
    secs = {}
    for i, s in enumerate(sh):
        if s[1] in (0, 2, 3, 9): continue   # null, symtab, strtab, REL
        nm = names[i]
        link = names[s[6]] if s[2] & 0x80 and s[6] else ''
        body = b'' if s[1] == 8 or s[1] == 0x70000003 else d[s[4]:s[4] + s[5]]
        assert nm not in secs, f'{path}: two sections named {nm}'
        secs[nm] = (s[1], s[2] & ~0x40, s[5] if s[1] != 0x70000003 else 0, s[8] if s[5] else 0, link, body)
    symset = Counter()
    for (n, v, sz, info, other, shndx) in syms:
        if not n or info & 0xf == 3: continue   # section symbols: implied by the sections
        if info & 0xf == 4: v = 0               # STT_FILE
        symset[(n, info >> 4, info & 0xf, other & 3, sz, secname(shndx), v)] += 1
    rels = {}
    for i, s in enumerate(sh):
        if s[1] != 9: continue
        tgt = names[s[7]]; out = rels.setdefault(tgt, Counter())
        for k in range(s[5] // 8):
            off, info = struct.unpack_from('<II', d, s[4] + 8 * k)
            sn, _v, _sz, sinfo, _o, shndx = syms[info >> 8]
            key = ('sec', secname(shndx)) if sinfo & 0xf == 3 else ('sym', sn)
            out[(off, info & 0xff, key)] += 1
    return secs, symset, rels

def main():
    a, b = load(sys.argv[1]), load(sys.argv[2])
    bad = 0
    for nm in sorted(set(a[0]) | set(b[0])):
        if a[0].get(nm) != b[0].get(nm):
            bad += 1
            if bad <= 8:
                x, y = a[0].get(nm), b[0].get(nm)
                print(f'section {nm}: ' + ('missing in A' if not x else 'missing in B' if not y else
                      f'type/flags/size/align/link A={x[:5]} B={y[:5]}' + (' (bytes differ)' if x[:5] == y[:5] else '')))
    for k in sorted((a[1] - b[1]) | (b[1] - a[1]))[:8]:
        bad += 1; print('symbol', k, 'A' if a[1][k] > b[1][k] else 'B', 'only')
    for nm in sorted(set(a[2]) | set(b[2])):
        x, y = a[2].get(nm, Counter()), b[2].get(nm, Counter())
        if x != y:
            bad += 1
            print(f'relocations of {nm}: {sum((x - y).values())} only in A, {sum((y - x).values())} only in B; e.g.', sorted((x - y) | (y - x))[:3])
    print('relcmp:', 'same' if not bad else f'{bad} difference(s)')
    sys.exit(1 if bad else 0)

main()
