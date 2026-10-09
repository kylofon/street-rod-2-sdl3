"""Street Rod 2: decode Game/SR2_SHP (vector span shapes) -> work/shp/*.png + _sheet.png + dump.txt

  python tools/sr2shp.py [Game/SR2_SHP] [outdir]      (default outdir work/shp)

SR2_SHP is not a bitmap library: 26 "shape sets" of coloured horizontal-span lists, one record per
distance level (LOD).  See FORMATS.md.  Reader: 1a21:7304 (format A) / 1a21:7446 (format B),
renderer shape_draw 1dcb:4154.  Directory = 26 x 8 bytes at DS:80A2 {u32 file offset, u16 size, u16 0}.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from srlib import game_palette, png  # noqa: E402

EXE = 'work/SR2_unp.exe'
DIR_DS = 0x80A2
N_SETS = 26
FIRST_B = 23          # DS:81DA: sets 23..25 are format B (mode 8), loaded by 7446
FILE_BASE_DS = 0x40C80  # file offset of DS:0 in work/SR2_unp.exe


def read_dir():
    d = open(EXE, 'rb').read()
    out = []
    for i in range(N_SETS):
        lo, hi, sz, _ = struct.unpack_from('<4H', d, FILE_BASE_DS + DIR_DS + 8 * i)
        out.append((hi << 16 | lo, sz))
    return out


def parse_records(e, p, n):
    """n 16-byte records at p -> list of (parts[(colour, count)], spans[(colour, x, y, w)], newp)."""
    recs = []
    for _ in range(n):
        r = e[p:p + 16]
        p += 16
        recs.append([(r[2 * k], r[2 * k + 1]) for k in range(6) if r[2 * k + 1]])
    out = []
    for parts in recs:
        spans = []
        for col, cnt in parts:
            for _ in range(cnt):
                x, y, w = struct.unpack_from('<3b', e, p)
                p += 3
                spans.append((col, x, y, w))
        out.append((parts, spans))
    return out, p


def parse_set(e, fmt_b):
    """-> list of groups, each a list of LOD records (parts, spans)."""
    if not fmt_b:
        n = struct.unpack_from('<H', e, 0)[0]
        recs, p = parse_records(e, 6, n)          # +2,+4 are runtime pointers
        groups = [recs]
    else:
        ng, per = struct.unpack_from('<HH', e, 0)  # +4,+6 runtime pointers; then ng far ptrs (runtime)
        recs, p = parse_records(e, 8 + 4 * ng, ng * per)
        groups = [recs[g * per:(g + 1) * per] for g in range(ng)]
    assert p == len(e), (p, len(e))
    return groups


def render(spans, pal, scale=2):
    if not spans:
        return 1, 1, bytearray(3), (0, 0)
    x0 = min(min(x, x + w) for _, x, _, w in spans)
    x1 = max(max(x, x + w) for _, x, _, w in spans)
    y0 = min(y for _, _, y, _ in spans)
    y1 = max(y for _, _, y, _ in spans)
    w, h = (x1 - x0 + 1) * scale, (y1 - y0 + 1) * scale
    img = bytearray(w * h * 3)
    for col, x, y, ww in spans:
        c = pal[col & 15]
        for xx in range(min(x, x + ww), max(x, x + ww) + 1):
            for sy in range(scale):
                for sx in range(scale):
                    o = (((y - y0) * scale + sy) * w + (xx - x0) * scale + sx) * 3
                    img[o:o + 3] = bytes(c)
    return w, h, img, (x0, y0)


def blit(dst, dw, src, sw, sh, ox, oy):
    for y in range(sh):
        dst[((oy + y) * dw + ox) * 3:((oy + y) * dw + ox + sw) * 3] = src[y * sw * 3:(y + 1) * sw * 3]


def sheet(rows, pal_bg=(40, 40, 40), gap=6):
    """rows: list of lists of (w, h, img). Returns (W, H, rgb)."""
    W = max(sum(r[0] for r in row) + gap * (len(row) + 1) for row in rows)
    rh = [max(r[1] for r in row) + gap for row in rows]
    H = sum(rh) + gap
    rgb = bytearray(bytes(pal_bg) * (W * H))
    y = gap
    for row, h in zip(rows, rh):
        x = gap
        for w, hh, img in row:
            blit(rgb, W, img, w, hh, x, y)
            x += w + gap
        y += h
    return W, H, rgb


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else 'Game/SR2_SHP'
    out = sys.argv[2] if len(sys.argv) > 2 else 'work/shp'
    os.makedirs(out, exist_ok=True)
    data = open(src, 'rb').read()
    pal = game_palette(0x1DC8)
    entries = read_dir()
    assert sum(s for _, s in entries) == len(data), 'directory does not cover the file'
    all_rows = []
    dump = []
    for i, (off, sz) in enumerate(entries):
        e = data[off:off + sz]
        groups = parse_set(e, i >= FIRST_B)
        rows = []
        dump.append('set %2d  off %5d size %4d  format %s  groups %d x LODs %d' % (
            i, off, sz, 'B' if i >= FIRST_B else 'A', len(groups), len(groups[0])))
        for g, recs in enumerate(groups):
            cells = []
            for k, (parts, spans) in enumerate(recs):
                w, h, img, org = render(spans, pal)
                cells.append((w, h, img))
                dump.append('   g%d lod%d parts %s spans %d origin %s' % (g, k, parts, len(spans), org))
            rows.append(cells)
        W, H, rgb = sheet(rows)
        png('%s/%02d_%s.png' % (out, i, 'B' if i >= FIRST_B else 'A'), W, H, rgb)
        all_rows.append([c for r in rows[:1] for c in r])      # first group only in the big sheet
    W, H, rgb = sheet(all_rows)
    png(out + '/_sheet.png', W, H, rgb)
    open(out + '/dump.txt', 'w').write('\n'.join(dump) + '\n')
    print('%d sets, %d bytes covered; wrote %s/NN_[AB].png, _sheet.png (%dx%d), dump.txt' % (
        len(entries), len(data), out, W, H))


if __name__ == '__main__':
    main()
