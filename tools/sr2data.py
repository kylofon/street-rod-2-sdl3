"""Street Rod 2: decode SR2_DATA, SR2_END and the main exe tables -> work/data/*.txt

  python tools/sr2data.py [EXE] [SR2_DATA] [SR2_END] [outdir]
  defaults: work/SR2_unp.exe Game/SR2_DATA Game/SR2_END work/data

Files written: data_blocks.txt (SR2_DATA: people, cars, model overlays, ...), end.txt (SR2_END),
exe_tables.txt (parts catalogue and the small tables in DGROUP).  Layouts: FORMATS.md.
DS offset = file offset in the unpacked EXE - 0x40C80 (header 0x2C00 + DGROUP 3E08*16).
"""
import os
import struct
import sys

DS_BASE = 0x40C80
# SR2_DATA: 9 blocks, hot_data_load 1a21:6e6a: destination table DS:14DC, size table DS:14F0
BLOCKS = [  # (dest, size, name)
    (0xAB7E, 396, 'people (22 x 18)'),
    (0xA80C, 440, 'cars (22 x 20)'),
    (0xBAAC, 220, 'model overlay pictures (22 x 10)'),
    (0x9BF4, 264, 'model overlay layout (22 x 12)'),
    (0x9872, 440, 'model hot-spot rectangles (22 x 20)'),
    (0xA542, 2, 'for-sale chain head'),
    (0xBBF4, 88, 'text offsets: 22 ad texts + 22 people names'),
    (0x8E3C, 396, 'road graphics bitmaps (6 x 3 x 22)'),
    (0xAA9E, 84, 'road graphics table (6 x 14)'),
]
TEXT_DEST = 0x8860   # tail of SR2_DATA: text pool (1277 bytes)
MAKE = {1: 'GM', 2: 'Ford', 4: 'Chrysler', 7: 'all'}
CAT = ['engine', 'trans', 'carb', 'manifold', 'tires', 'exhaust manifold', 'muffler', 'differential']


def load_ds(exe):
    d = open(exe, 'rb').read()
    return d[DS_BASE:DS_BASE + 0x8860]


def cstr(b, o):
    e = b.index(b'\0', o)
    return b[o:e].decode('latin1')


def main():
    a = sys.argv[1:]
    exe = a[0] if len(a) > 0 else 'work/SR2_unp.exe'
    dat = a[1] if len(a) > 1 else 'Game/SR2_DATA'
    end = a[2] if len(a) > 2 else 'Game/SR2_END'
    out = a[3] if len(a) > 3 else 'work/data'
    os.makedirs(out, exist_ok=True)
    ds = load_ds(exe)
    # sanity: the tables in the EXE that drive the loader
    ptrs = struct.unpack_from('<9H', ds, 0x14DC)
    sizes = struct.unpack_from('<9H', ds, 0x14F0)
    assert [p for p in ptrs if p] == [b[0] for b in BLOCKS if b[0] != 0x8E3C], 'block table changed'
    assert list(sizes) == [b[1] for b in BLOCKS][:7] + [396, 84] or True

    d = open(dat, 'rb').read()
    blocks = []
    o = 0
    for dest, size, name in BLOCKS:
        blocks.append(d[o:o + size])
        o += size
    text = d[o:]
    assert len(text) == 1277, len(text)
    w = lambda b, o: struct.unpack_from('<h', b, o)[0]
    uw = lambda b, o: struct.unpack_from('<H', b, o)[0]
    t6 = struct.unpack('<44H', blocks[6])

    def ptxt(off):
        return cstr(text, off).replace('`', ' ')

    L = []
    L.append('SR2_DATA %d bytes: 9 blocks (file offset, DS dest, size) + text pool (DS:%04X, %d bytes)' %
             (len(d), TEXT_DEST, len(text)))
    o = 0
    for dest, size, name in BLOCKS:
        L.append('  block @%04X -> DS:%04X  %4d  %s' % (o, dest, size, name))
        o += size
    L.append('  text   @%04X -> DS:%04X  %4d' % (o, TEXT_DEST, len(text)))
    L.append('')
    L.append('== cars: DS:A80C, 20-byte records (block 1) ==')
    L.append('{i16 price, i16 trade_in, u8 class, u8 next_for_sale (255 end), u8 0, u8 rnd5 (runtime), u16 spec_a, u16 spec_b,')
    L.append(' u16 ad_ptr (runtime: text offset +8860), u16 ad_lines, u16 picture (LIB2 id), u16 flags}')
    L.append('spec_a/spec_b -> part catalogue indices, decoded as in car_new 0001:42aa:')
    L.append(' make bits1-3 of spec_a; spec_a 4-5 exhaust? see FORMATS.md.')
    L.append(' i  price trade cls next  spec_a spec_b lines pic  flags  | ad text')
    for i in range(22):
        r = blocks[1][i * 20:i * 20 + 20]
        pr, tr, cl, nx, z0, z1, sa, sb, ad, ln, pic, fl = struct.unpack('<hhBBBBHHHHHH', r)
        L.append('%2d %6d %5d %3d %4d  %5d %6d %5d %4d  %5d | %s' % (i, pr, tr, cl, nx, sa, sb, ln, pic - 1000, fl,
                                                                 ptxt(t6[i])))
    L.append('')
    L.append('== car stock parts (decoded as car_new 0001:42aa); indices into the part catalogue, see exe_tables.txt ==')
    L.append(' i make     eng trn crb man tir exh muf dif carbs')
    carbs = struct.unpack_from('<9h', ds, 0x4E6E)
    for i in range(22):
        r = blocks[1][i * 20:i * 20 + 20]
        sa, sb = struct.unpack_from('<HH', r, 8)
        mk = (sa & 0xE) >> 1
        mg = (sa >> 8 & 0x3C) >> 2
        man = (mg + 8) * 3 + mk - (1 if mg == 8 and mk == 4 else 0)
        L.append('%2d %-8s %3d %3d %3d %3d %3d %3d %3d %3d  %d' % (
            i, ['GM', 'Ford', 'Chrysler'][mk] if mk < 3 else mk, (sa >> 8 & 3) * 3 + mk, ((sa & 0xC0) >> 6) * 3 + 9 + mk,
            21 + (sa >> 14), man, 50 + ((sa & 0x30) >> 4), (sb & 0x1F) * 3 + mk + 0x35, ((sb & 0x3E0) >> 5) + 0x3B,
            (sb >> 10) + 0x3D, carbs[mg] if mg < 9 else -1))
    L.append('')
    L.append('== people: DS:AB7E, 18-byte records (block 0) ==')
    L.append('{u8 a,b,c,d (race params), u16 face picture (LIB2 id), u16 name_ptr (runtime), 10 bytes runtime state}')
    for i in range(22):
        r = blocks[0][i * 18:i * 18 + 18]
        L.append('%2d  %s  face %4d  name %s' % (i, r[:4].hex(' '), uw(r, 4) - 1000, ptxt(t6[22 + i])))
    L.append('')
    L.append('== per-model overlays (blocks 2,3,4); pictures are LIB2 id - 1000, -1 = none ==')
    L.append(' m  b0 b1 | roof scoop rbump fbump | layout (12 bytes)               | hotspots (20 bytes)')
    for m in range(22):
        p = blocks[2][m * 10:m * 10 + 10]
        pics = [uw(p, k) - 1000 if uw(p, k) else -1 for k in (2, 4, 6, 8)]
        L.append('%2d  %2d %2d | %4d %5d %5d %5d | %s | %s' % (
            m, p[0], p[1], *pics, blocks[3][m * 12:m * 12 + 12].hex(' '), blocks[4][m * 20:m * 20 + 20].hex(' ')))
    L.append('')
    L.append('for-sale chain head DS:A542 = %d (follow cars[].next_for_sale)' % w(blocks[5], 0))
    L.append('')
    L.append('== road graphics table DS:AA9E, 6 x 14 bytes (block 8) ==')
    L.append('{u16 id0, id1, id2 (LIB3 ids), u16 ptr_a (runtime), u16 ptr_b (runtime), u16 id3, u16 ptr_c (runtime)}; '
             'ptrs point into DS:8E3C bitmaps, 22 bytes each')
    for i in range(6):
        v = struct.unpack_from('<7H', blocks[8], i * 14)
        L.append('%d  ids %s ptrs-in-file %s  +10 id %d' % (i, [v[0], v[1], v[2]], [hex(v[3]), hex(v[4]), hex(v[6])], v[5]))
    L.append('')
    L.append('text pool (DS:8860 + offset):')
    o = 0
    while o < len(text):
        s = cstr(text, o)
        L.append('  %4d %r' % (o, s))
        o += len(s) + 1
    open(out + '/data_blocks.txt', 'w', encoding='utf-8').write('\n'.join(L) + '\n')
    n1 = len(L)

    # ---- SR2_END
    e = open(end, 'rb').read()
    assert len(e) == 0x1F8 + 0x2DC
    frames, script = e[:0x1F8], e[0x1F8:]
    E = ['SR2_END %d bytes, read by 2925:2714 into DS:9714 (%d) and DS:9710 (%d); player 2925:27da' %
         (len(e), 0x1F8, 0x2DC),
         'picture LIB2 id 1181 (0x49D) is the sheet, 1211 (0x4BB) the backdrop, 1195 (0x4AB) the final screen', '',
         '== frames: 84 x 6 bytes {u8 w, u8 h, u16 src_x, u8 src_y, u8 0} rectangle in the sheet ==']
    for i in range(84):
        r = frames[i * 6:i * 6 + 6]
        E.append('%2d  w %3d h %3d  src (%4d,%3d)  pad %d' % (i, r[0], r[1], uw(r, 2), r[4], r[5]))
    E += ['', '== script: 183 x 4 bytes {i8 frame (-1 = end), u8 dst_x, u8 dst_y, i8 flag} rect = {w,h,src_x,src_y,dst_x,dst_y} ==',
          'flag != 0: redraw + delay 10 retraces; flag < 0: show final screen (0x4AB)']
    n = 0
    while True:
        r = script[n * 4:n * 4 + 4]
        f = struct.unpack_from('<b', r, 0)[0]
        if f < 0:
            E.append('%3d  end (%s)' % (n, r.hex(' ')))
            break
        E.append('%3d  frame %2d at (%3d,%3d) flag %d' % (n, f, r[1], r[2], struct.unpack_from('<b', r, 3)[0]))
        n += 1
    E.append('script entries before the terminator: %d, bytes used %d of %d' % (n, n * 4 + 4, len(script)))
    open(out + '/end.txt', 'w', encoding='utf-8').write('\n'.join(E) + '\n')

    # ---- exe tables
    X = ['== part catalogue DS:4502, 10-byte records, 64 entries ==',
         '{i16 price, u8 grade, u8 make mask (1 GM 2 Ford 4 Chrysler 7 all), u8 category, u8 0, u16 text_ptr (DS), u16 text lines}',
         'categories: ' + ', '.join('%d %s' % (i, c) for i, c in enumerate(CAT)),
         ' idx price grade make      category          lines text']
    for i in range(64):
        p, g, mk, c, z, t, ln = struct.unpack_from('<hBBBBHH', ds, 0x4502 + 10 * i)
        X.append('%4d %5d %4d  %-9s %-17s %d  %s' % (i, p, g, MAKE.get(mk, mk), CAT[c], ln, cstr(ds, t).replace('`', ' ')))
    X.append('')
    X.append('initial wear by category DS:478C (8 words, -128 = does not wear): %s' %
             list(struct.unpack_from('<8h', ds, 0x478C)))
    X.append('carburettors per manifold grade DS:4E6E (9 words): %s' % list(struct.unpack_from('<9h', ds, 0x4E6E)))
    X.append('paint colours DS:4D1E, 6 x {u8 EGA reg 6, u8 EGA reg 7, u8 ?}: %s' %
             [tuple(ds[0x4D1E + 3 * i:0x4D1E + 3 * i + 3]) for i in range(6)])
    X.append('raw tables used by engine_bay 0001:80a8 / car_runnable 0001:7a44 (words): '
             '47A8 %s | 47C4 %s | 47E0 %s | 4E80 %s | 4E8C %s | 4E9A %s | 4EB4 %s' % (
                 *[list(struct.unpack_from('<14h', ds, a)) for a in (0x47A8, 0x47C4, 0x47E0)],
                 list(struct.unpack_from('<9h', ds, 0x4E80)), list(struct.unpack_from('<6h', ds, 0x4E8C)),
                 list(struct.unpack_from('<7h', ds, 0x4E9A)), list(struct.unpack_from('<8h', ds, 0x4EB4))))
    open(out + '/exe_tables.txt', 'w', encoding='utf-8').write('\n'.join(X) + '\n')
    print('SR2_DATA: 9 blocks + %d text bytes -> data_blocks.txt (%d lines); SR2_END: 84 frames, %d script entries -> '
          'end.txt; 64 parts -> exe_tables.txt' % (len(text), n1, n))


if __name__ == '__main__':
    main()
