"""LZEXE 0.91 unpacker (the seven main F-117A EXEs carry the 'LZ91' signature).
  unlzexe.py [FILE ...]      default: every LZ91 EXE in Game/ -> work/unpacked/<NAME>
Writes a plain MZ file (relocations restored) so x86dis.py / rawdis.py / Ghidra see real code.
"""
import glob, os, struct, sys


def unpack(d):
    h = list(struct.unpack_from('<14H', d, 0))
    if d[0x1c:0x20] != b'LZ91':
        raise ValueError('not LZEXE 0.91')
    hdr = h[4] * 16
    inf = hdr + h[11] * 16                       # decompressor segment
    ip, cs, sp, ss, _cpar, _incpar, _dsize = struct.unpack_from('<7H', d, inf)

    # relocation table (0.91 layout: byte spans, 0/word escapes)
    p, relocs, off, seg = inf + 0x158, [], 0, 0
    while True:
        span = d[p]; p += 1
        if span == 0:
            span = struct.unpack_from('<H', d, p)[0]; p += 2
            if span == 0:
                seg += 0x0fff
                continue
            if span == 1:
                break
        off += span
        seg += (off & ~0x0f) >> 4
        off &= 0x0f
        relocs.append((off, seg))

    # LZ bit-stream: 16-bit words, LSB first, reloaded right after the 16th bit is used
    src, out = hdr, bytearray()
    buf = struct.unpack_from('<H', d, src)[0]; src += 2
    cnt = 16

    def bit():
        nonlocal buf, cnt, src
        b = buf & 1
        cnt -= 1
        if cnt == 0:
            buf = struct.unpack_from('<H', d, src)[0]; src += 2
            cnt = 16
        else:
            buf >>= 1
        return b

    while True:
        if bit():
            out.append(d[src]); src += 1
            continue
        if not bit():
            n = (bit() << 1 | bit()) + 2
            span = d[src] - 0x100; src += 1
        else:
            lo, hi = d[src], d[src + 1]; src += 2
            span = (lo | ((hi & 0xf8) << 5) | 0xe000) - 0x10000
            n = (hi & 7) + 2
            if n == 2:
                n = d[src]; src += 1
                if n == 0:
                    break
                if n == 1:
                    continue
                n += 1
        for _ in range(n):
            out.append(out[span])

    # new MZ header: 0x20 bytes fixed + relocs, padded to paragraphs
    hsize = (0x1c + 4 * len(relocs) + 15) // 16 * 16
    hsize = max(hsize, 0x20)
    total = hsize + len(out)
    packed_load = len(d) - hdr
    minalloc = max(0, h[5] - (len(out) - packed_load + 15) // 16)
    head = struct.pack('<14H', 0x5a4d, total % 512, (total + 511) // 512, len(relocs), hsize // 16,
                       minalloc, h[6], ss, sp, 0, ip, cs, 0x1c, 0)
    head += b''.join(struct.pack('<HH', o, s) for o, s in relocs)
    head += b'\0' * (hsize - len(head))
    return head + bytes(out), (cs, ip, ss, sp, len(relocs), len(out))


def main():
    files = sys.argv[1:] or [f for f in sorted(glob.glob('Game/*.EXE'))
                             if open(f, 'rb').read(0x20)[0x1c:0x20] == b'LZ91']
    os.makedirs('work/unpacked', exist_ok=True)
    for f in files:
        data, (cs, ip, ss, sp, nrel, n) = unpack(open(f, 'rb').read())
        dst = os.path.join('work/unpacked', os.path.basename(f).upper())
        open(dst, 'wb').write(data)
        print('%-13s image %6d  relocs %4d  cs:ip %04x:%04x  ss:sp %04x:%04x' % (
            os.path.basename(f), n, nrel, cs, ip, ss, sp))


if __name__ == '__main__':
    main()
