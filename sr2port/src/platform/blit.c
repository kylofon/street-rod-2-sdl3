/* Blit helper segments of the EGA driver (video.md): the mouse pointer 26c0:0175, the picture unpacker
 * 26b3:000c, the latched copies 3298, the plane row helpers 32a5 / 3345 / 33e5 (SR1: 0e6c, 0e92, 24e7,
 * 24f4 / 2595 / 2634). The EGA span routines of SR1's segment 2645 are race_render's 0dcb:043c / 04e6 in
 * SR2 (S5.3).
 *
 * Transcribed from the disassembly (tools/x86dis.py) on top of the VGA register model (vga.h): every
 * memory access goes through vrd/vwr (VRAM through the latches, RAM bitmaps in mem[]); x86 word
 * accesses are vrd16/vwr16 (two byte cycles, low byte first). The unrolled / self-modifying helpers
 * (32a5:0002, 3345:000c) are written as loops with the same access order.
 *
 * PORT: the Tandy helpers (0e6c:0002/00c1, 2462, 2487, 24be, 24d8) are not ported (VGA path only). */
#include "platform/vga.h"
#include "platform/video.h"

/* ======================================================================= pointer (§4.7, §4.10) */

/* 26c0:0175 (slot 4) - video.md, = SR1 0e6c:0175 with the pointer bank plane stride 0x480: save the 4 planes under the pointer through the latches (bit
 * mask 0), then write the 4 image planes with the pointer mask as bit mask. The mask is read in the
 * image's segment; the sequencer index is never written (2 assumed). */
void ega_draw_cursor(FarPtr img, FarPtr mask, FarPtr save, FarPtr scr, s16 xb, s16 y, s16 wb, s16 rows)
{
    u16 o = (u16)(scr.off + (u16)(0x28 * (u8)y) + (u16)xb);   /* mul bl: 8-bit y */
    u16 s = img.off, m = mask.off, v = save.off;
    out(0x3CE, 5); out(0x3CF, 0);                              /* write mode 0 */
    out(0x3CE, 8);                                             /* GC index 8 from here on */
    u16 rows16 = (u16)rows;
    for (;;) {
        u16 n = (u16)wb;                                       /* loop: wb = 0 runs 65536 times */
        do {
            out(0x3CF, 0);                                     /* bit mask 0 */
            out(0x3C5, 0x0F);
            vwr(save.seg, v, vrd(scr.seg, o));                 /* stores the 4 latched planes */
            v++;
            out(0x3CF, vrd(img.seg, m));                       /* bit mask = pointer mask */
            m++;
            out(0x3C5, 1); vwr(scr.seg, o, vrd(img.seg, s));
            out(0x3C5, 2); vwr(scr.seg, o, vrd(img.seg, (u16)(s + 0x480)));
            out(0x3C5, 4); vwr(scr.seg, o, vrd(img.seg, (u16)(s + 0x900)));
            out(0x3C5, 8); vwr(scr.seg, o, vrd(img.seg, (u16)(s + 0xD80)));
            o++;
            s++;
        } while (--n);
        if (--rows16 == 0) break;
        s = (u16)(s + 0x18 - wb);
        m = (u16)(m + 0x18 - wb);
        v = (u16)(v + 3 - wb);
        o = (u16)(o + 0x28 - wb);
    }
    out(0x3C5, 0x0F);
    out(0x3CF, 0xFF);
}

/* ====================================================================== picture unpacker */

/* 26b3:000c pic_unpack - FORMATS.md: bytes 00 and FF are followed by a count c: c+1 copies of themselves,
 * or, new in SR2, for c == 0 ONE byte: tokens[0] for 00 and tokens[1] for FF (the escape of a literal token
 * byte); a byte equal to token k (1..ntok) gives count[k-1] copies of value[k-1] (tables DS:4B72 / DS:4B82; a
 * count of 0 gives 65536 bytes, dec cx / jne); any other byte is a literal. src is read through vrd (a
 * picture parked in the VRAM cache is read with the read map select set by the caller), dst written through
 * vwr. SR2 returns nothing. */
void pic_unpack(FarPtr src, FarPtr dst, u16 n, const u8 *tokens, u16 ntok)
{
    u8 token_of[256];                        /* byte -> token number, 0 = literal */
    for (int i = 0; i < 256; i++) token_of[i] = 0;
    for (s16 k = 0; k < (s16)ntok; k++) token_of[tokens[k]] = (u8)(k + 1);
    u8 esc0 = tokens[0], esc1 = tokens[1];
    u8 tab[32];                              /* counts, values */
    for (u16 i = 0; i < 16; i++) {
        tab[i] = DSB(DS_run_counts + i);
        tab[16 + i] = DSB(DS_run_values + i);
    }
    s16 left = (s16)n;
    u16 si = src.off, di = dst.off;
    while (left > 0) {
        left--;
        u8 b = vrd(src.seg, si++);
        u16 cnt;
        if (b == 0x00 || b == 0xFF) {
            left--;
            u8 c = vrd(src.seg, si++);
            if (c == 0) { vwr(dst.seg, di++, b == 0x00 ? esc0 : esc1); continue; }
            cnt = (u16)(c + 1);
        } else {
            u8 k = token_of[b];
            if (k == 0) { vwr(dst.seg, di++, b); continue; }
            /* PORT: tokens beyond 16 read the original's stack frame past the tables; 0 here. */
            b = (unsigned)(k + 15) < 32 ? tab[k + 15] : 0;
            cnt = (unsigned)(k - 1) < 32 ? tab[k - 1] : 0;
        }
        do vwr(dst.seg, di++, b); while (--cnt);
    }
}

/* ===================================================================== latched copies (§4.4) */

/* 3298:0000 — video.md §4.4: VRAM -> VRAM rect copy in write mode 1, same stride both sides */
void ega_latch_copy(FarPtr s, FarPtr d, s16 wb, s16 h, s16 xb, s16 y, s16 stride)
{
    if (wb < 1 || h < 1) return;
    out(0x3CE, 5); out(0x3CF, 1);
    u16 o = (u16)((u16)((u8)y * (u8)stride) + (u16)xb);       /* mul dl: 8-bit operands */
    s.off = (u16)(s.off + o);
    d.off = (u16)(d.off + o);
    if (wb == 1) {
        u16 dx = (u16)(stride - 1);
        u16 cx = (u16)h;
        do {
            vwr(d.seg, d.off++, vrd(s.seg, s.off++));
            s.off = (u16)(s.off + dx);
            d.off = (u16)(d.off + dx);
        } while (--cx);
    } else if (wb == stride) {
        vmovedata(s.seg, s.off, d.seg, d.off, (u16)((u8)h * (u8)stride));
    } else {
        u16 bx = (u16)h, dx = (u16)(stride - wb);
        do {
            vmovedata(s.seg, s.off, d.seg, d.off, (u16)wb);
            s.off = (u16)(s.off + wb + dx);
            d.off = (u16)(d.off + wb + dx);
        } while (--bx);
    }
    out(0x3CE, 5); out(0x3CF, 0);
}

/* 3298:0083 — video.md §4.4: VRAM -> VRAM copy, destination stride 28h. 8-bit row counter; the
 * destination advances by the byte 28h - wb (zero-extended: add di,cx with ch = 0). */
void ega_latch_copy_to40(FarPtr s, s16 wb, s16 h, FarPtr d, s16 sstride)
{
    if (h < 1 || wb < 1) return;
    out(0x3CE, 5); out(0x3CF, 1);
    u8 bl = (u8)h;
    u16 dx = (u16)(sstride - wb);
    u8 bh = (u8)(0x28 - (u8)wb);
    do {
        vmovedata(s.seg, s.off, d.seg, d.off, (u16)wb);
        s.off = (u16)(s.off + wb + dx);
        d.off = (u16)(d.off + wb + bh);
    } while (--bl);
    out(0x3CE, 5); out(0x3CF, 0);
}

/* ================================================================= plane row helpers (§4.4) */

/* 32a5:0002 — one plane: dst = (mask & bg) | src, words from offset wb-2 down to (wb & 1), then the
 * byte at 0 if wb is odd (unrolled, wb <= A0h). Source, mask and background in segment seg. */
void rows_composite(FarPtr d, u16 seg, u16 s, u16 m, u16 bg, s16 sstr, s16 wb, s16 h, s16 bgstr, s16 dstr)
{
    if (h <= 0 || wb <= 0) return;
    u16 cx = (u16)h;
    do {
        for (s16 i = (s16)(wb - 2); i >= (wb & 1); i -= 2) {
            u16 ax = vrd16(seg, (u16)(m + i));
            ax &= vrd16(seg, (u16)(bg + i));
            ax |= vrd16(seg, (u16)(s + i));
            vwr16(d.seg, (u16)(d.off + i), ax);
        }
        if (wb & 1) {
            u8 al = vrd(seg, m);
            al &= vrd(seg, bg);
            al |= vrd(seg, s);
            vwr(d.seg, d.off, al);
        }
        s = (u16)(s + sstr);
        m = (u16)(m + sstr);
        bg = (u16)(bg + bgstr);
        d.off = (u16)(d.off + dstr);
    } while (--cx);
}

/* 3345:000c — one plane: dst = (mask & dst) | src, same unrolled order as 32a5:0002 */
void rows_mask_or(FarPtr d, u16 seg, u16 s, u16 m, s16 sstr, s16 wb, s16 h, s16 dstr)
{
    if (h <= 0 || wb <= 0) return;
    u16 cx = (u16)h;
    do {
        for (s16 i = (s16)(wb - 2); i >= (wb & 1); i -= 2) {
            u16 ax = vrd16(seg, (u16)(m + i));
            ax &= vrd16(d.seg, (u16)(d.off + i));             /* loads the latches */
            ax |= vrd16(seg, (u16)(s + i));
            vwr16(d.seg, (u16)(d.off + i), ax);
        }
        if (wb & 1) {
            u8 al = vrd(seg, m);
            al &= vrd(d.seg, d.off);
            al |= vrd(seg, s);
            vwr(d.seg, d.off, al);
        }
        s = (u16)(s + sstr);
        m = (u16)(m + sstr);
        d.off = (u16)(d.off + dstr);
    } while (--cx);
}

/* 33e5:0002 — one plane: dst = (dst & mask) | (src & ~mask), left to right. Row counter = low byte
 * of h (dec cl); the destination row step only reduces the low byte of dstr by wb & FEh (sub dl,ch),
 * so a borrow adds 100h. With no whole word (wb & FEh == 0) the odd byte is processed regardless. */
void rows_mask_blend(FarPtr d, u16 seg, u16 s, u16 m, s16 sstr, s16 wb, s16 h, s16 dstr)
{
    if (h <= 0 || wb <= 0) return;
    u8 cl = (u8)h;
    u8 ch = (u8)wb;
    u8 odd = ch & 1;                                          /* cs:[2Bh] */
    ch &= 0xFE;
    u16 sadd = (u16)(sstr - ch);                              /* cs:[28h] */
    u16 dadd = (u16)(((u16)dstr & 0xFF00) | (u8)((u8)dstr - ch));
    u8 words = (u8)(ch >> 1);                                 /* cs:[2Ah] */
    u16 bp = m, si = s, di = d.off;
    do {
        u8 n = words;
        if (n != 0) {
            do {
                u16 ax = vrd16(seg, bp);
                u16 bx = vrd16(d.seg, di);
                bx &= ax;
                ax = (u16)~ax;
                ax &= vrd16(seg, si);
                ax |= bx;
                vwr16(d.seg, di, ax);
                bp = (u16)(bp + 2); si = (u16)(si + 2); di = (u16)(di + 2);
            } while (--n);
        }
        if (words == 0 || odd) {
            u8 al = vrd(seg, bp);
            u8 bl = vrd(d.seg, di);
            bl &= al;
            al = (u8)~al;
            al &= vrd(seg, si);
            al |= bl;
            vwr(d.seg, di, al);
        }
        bp = (u16)(bp + sadd);
        si = (u16)(si + sadd);
        di = (u16)(di + dadd);
    } while (--cl);
}

/* 33e5:00b9 — mask = ~(p0 | p1 | p2 | p3), n = total / 4 bytes per plane (odd byte first, then words;
 * n == 0 would run 32 K words as the original) */
void make_mask_plane(FarPtr planes, FarPtr mask, u16 total)
{
    u16 n = (u16)(total >> 2);
    u16 cx = n, si = planes.off, di = mask.off;
    u16 ps = planes.seg;
    if (n & 1) {
        u8 al = vrd(ps, si);
        al |= vrd(ps, (u16)(si + n));
        al |= vrd(ps, (u16)(si + 2 * n));
        al |= vrd(ps, (u16)(si + 3 * n));
        vwr(mask.seg, di, (u8)~al);
        if (--cx == 0) return;
        di++;
        si++;
    }
    do {
        u16 ax = vrd16(ps, si);
        ax |= vrd16(ps, (u16)(si + n));
        ax |= vrd16(ps, (u16)(si + 2 * n));
        ax |= vrd16(ps, (u16)(si + 3 * n));
        vwr16(mask.seg, di, (u16)~ax);
        di = (u16)(di + 2);
        si = (u16)(si + 2);
        cx--;
    } while (--cx);                                           /* dec cx; loop */
}
