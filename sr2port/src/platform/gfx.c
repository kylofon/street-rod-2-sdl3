/* Drawing primitives of code segment 1a21: set-up, clipping, palette, rectangles and lines, page
 * copies, bitmaps (video.md "Desc" and the 1a21 function sections; SR2 descriptors are 0x12 bytes).
 *
 * Transcribed from the disassembly (tools/x86dis.py). Memory accesses to bitmaps go through vrd/vwr so
 * a descriptor pointing into video memory behaves as on the card (latches loaded by every CPU read),
 * and plain RAM bitmaps are mem[]. The driver is called through the drv_* wrappers (vector table
 * DS:A0AE) wherever the original uses `lcall [DS:A0AE+4k]`. */
#include <string.h>

#include "host.h"
#include "platform/ega.h"
#include "platform/platform.h"
#include "platform/vga.h"
#include "platform/video.h"

static inline u16 x_align(void) { return DSW(DS_g_x_align); }       /* FFF8h */
static inline u8  px_shift(void) { return (u8)DSW(DS_g_px_shift); } /* cl = byte [823A] */
static inline bool has_mask(FarPtr d) { FarPtr m = desc_mask(d); return m.off != 0 || m.seg != 0; }

/* ======================================================================== static descriptors */

/* The static page descriptors of segment 371c as the load image holds them (video.md "Desc"): page 0
 * A000:0000, page 1 A200:0000 (type FEh), and at 371c:0024 the CGA/Tandy template (planes 2000:A000?,
 * stride 50h, type FDh; copied over by gfx_init on the VGA path). 3720:0000 is zero. */
static const u8 static_desc[3][DESC_BYTES] = {
    { 0x40, 0x01, 0xC8, 0x00, 0x40, 0x1F, 0x00, 0x00, 0x00, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0xFE, 0x00 },
    { 0x40, 0x01, 0xC8, 0x00, 0x40, 0x1F, 0x00, 0x00, 0x00, 0xA2, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0xFE, 0x00 },
    { 0x40, 0x01, 0xC8, 0x00, 0x00, 0x40, 0x00, 0x20, 0x00, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x50, 0x00, 0xFD, 0x00 },
};

/* PORT: sanity check of the loaded image (like the TD2 port's table checks): the page descriptors the
 * EGA path relies on must hold the values above when gfx_init first runs. */
static void check_static_descriptors(void)
{
    static bool checked;
    if (checked) return;
    checked = true;
    for (int i = 0; i < 3; i++)
        if (memcmp(mp(SEG(0x371C), (u16)(i * DESC_BYTES)), static_desc[i], DESC_BYTES) != 0)
            host_fatal("gfx_init: static descriptor 371c:%04x differs from the expected image", i * DESC_BYTES);
    for (int k = 0; k < DESC_BYTES; k++)
        if (rd8(SEG(0x3720), (u16)k) != 0)
            host_fatal("gfx_init: static descriptor 3720:0000 is not zero");
}

/* ================================================================================= set-up */

/* 1a21:19f0 gfx_init(front, back): page descriptors, mode constants, colour scheme, UI colours, page
 * table. main passes 371c:0000 / 371c:0012. */
void gfx_init(FarPtr a, FarPtr b)
{
    check_static_descriptors();

    ds_far_wr(DS_g_front, a);
    ds_far_wr(DS_g_back, b);
    DSW(DS_g_aux_a) = 0;
    DSW(DS_g_aux_b) = 0;
    DSW(DS_g_mirror) = 1;
    DSS(DS_driver_id) = (s16)(s8)rd8(a.seg, (u16)(a.off + DESC_TYPE));
    s16 drv = DSS(DS_driver_id);
    if (drv == -6) {                                    /* Tandy (parked, constants kept) */
        DSW(DS_g_nplanes) = 1; DSW(DS_g_px_shift) = 1; DSW(DS_g_px_factor) = 4;
        DSW(DS_g_x_align) = 0xFFFE; DSW(DS_g_scheme) = DS_scheme_ega;
    } else if (drv == -2) {
        DSW(DS_g_nplanes) = 4; DSW(DS_g_px_shift) = 3; DSW(DS_g_px_factor) = 1;
        DSW(DS_g_x_align) = 0xFFF8; DSW(DS_g_scheme) = DS_scheme_ega;
        out(0x3D4, 0x0C); out(0x3D5, 0);
        out(0x3D4, 0x0D); out(0x3D5, 0);
    } else {                                            /* CGA / Hercules (parked, constants kept) */
        DSW(DS_g_nplanes) = 1; DSW(DS_g_px_shift) = 2; DSW(DS_g_px_factor) = 2;
        DSW(DS_g_x_align) = 0xFFFC; DSW(DS_g_scheme) = DS_scheme_cga;
        wr8(a.seg, (u16)(a.off + DESC_TYPE), 0xFD);
    }
    /* colours AD28..AD2C = scheme +5..+9 (a word, a word and a byte) */
    memmove(mp(DGROUP, DS_g_text_fg), mp(DGROUP, (u16)(DSW(DS_g_scheme) + 5)), 5);
    /* the UI colour variables (VGA / EGA values; CGA / Hercules override the first four) */
    static const u16 ui[12] = { 0xB, 0xE, 4, 5, 2, 4, 5, 2, 0, 0xF, 5, 0 };
    for (int k = 0; k < 12; k++) DSW((u16)(DS_ui_colours + 2 * k)) = ui[k];
    if (drv != -2 && drv != -6) {
        DSW(DS_ui_colours) = 1; DSW(DS_ui_colours + 2) = 0; DSW(DS_ui_colours + 4) = 1; DSW(DS_ui_colours + 6) = 3;
    }
    drv_pal_normal();                                   /* lcall [DS:A0CE] */
    DSW(DS_menu_depth) = 0;
    DSB(DS_ui_level) = 1;
    FarPtr fa = ds_far(DS_g_front), fb = ds_far(DS_g_back);
    ds_far_wr(DS_g_page, fa);
    ds_far_wr((u16)(DS_g_page + 4), fb);
    ds_far_wr((u16)(DS_g_page + 8), far_make(SEG(0x371C), 0x24));
    ds_far_wr((u16)(DS_g_page + 12), far_make(SEG(0x3720), 0));
    if (DSS(DS_driver_id) == -2) {
        FarPtr p2 = ds_far((u16)(DS_g_page + 8));
        memmove(far_mp(p2), far_mp(fb), DESC_BYTES);    /* 9 words */
        far_wr(p2.seg, (u16)(p2.off + DESC_PLANES), far_make(0xA000, 0x4000));
        FarPtr p3 = ds_far((u16)(DS_g_page + 12));
        memmove(far_mp(p3), far_mp(fb), DESC_BYTES);
        far_wr(p3.seg, (u16)(p3.off + DESC_PLANES), far_make(0xA000, 0x6000));
    }
}

static void crtc_start_zero(void)
{
    out(0x3D4, 0x0C); out(0x3D5, 0);
    out(0x3D4, 0x0D); out(0x3D5, 0);
}

/* 1a21:1048: 0 save the pages, 1 restore + split off + start 0, 2 start 0, 3 single buffer (SR2: no case 4). PORT: the return value is AX as far as it is defined by this function itself (cases 0, 2 and
 * out-of-range n); after a call to a void routine (cases 1, 3, 4) the port returns 0. No caller uses it. */
u16 gfx_screen_mode(s16 n)
{
    u16 ax;
    switch (n) {
    case 0: {
        FarPtr sa = ds_far(DS_g_saved_front);
        ax = (u16)(sa.off | sa.seg);
        if (ax == 0) {
            ds_far_wr(DS_g_saved_front, ds_far(DS_g_front));
            ds_far_wr(DS_g_saved_back, ds_far(DS_g_back));
            ax = DSW(DS_g_back);
        }
        DSW(DS_screen_flag) = 1;
        return ax;
    }
    case 1:
        ds_far_wr(DS_g_front, ds_far(DS_g_saved_front));
        ds_far_wr(DS_g_back, ds_far(DS_g_saved_back));
        if (DSS(DS_driver_id) == -2) {
            out(0x3D4, 0x13); out(0x3D5, 0x14);
            ega_set_split(-1);
            crtc_start_zero();
        }
        DSW(DS_g_mirror) = 1;
        status_init(0xBE);
        return 0;
    case 2:
        ax = 2;
        if (DSS(DS_driver_id) == -2) { crtc_start_zero(); ax = 0; }
        DSW(DS_g_mirror) = 1;
        return ax;
    case 3: {
        FarPtr sa = ds_far(DS_g_saved_front);
        ds_far_wr(DS_g_front, sa);
        ds_far_wr(DS_g_back, sa);
        DSW(DS_g_mirror) = 0;
        status_init(DSS(DS_driver_id) == -2 ? 0x5A : 0xBE);
        if (DSB(DS_racing) != 0) status_label(ds_str(DSW(DS_status_label_text)));   /* a near pointer (push [4846]), not a char[] */
        return 0;
    }
    default:
        return (u16)n;
    }
}

/* ================================================================================ helpers (§4.2) */

/* 1a21:17ee clip_rect: pixel rect -> byte rect clipped to src and dst; 1 if non-empty. */
s16 clip_rect(FarPtr src, FarPtr dst, const Rect *in, Rect *o)
{
    u8 sh = px_shift();
    s16 sx = (s16)(in->sx >> sh), sy = in->sy;
    s16 w = (s16)(in->w >> sh), h = in->h;
    s16 sstride = (s16)desc_stride(src), sh_ = (s16)desc_h(src);
    if ((s16)(sx + w) > sstride) w = (s16)(sstride - sx);
    if ((s16)(sy + h) > sh_) h = (s16)(sh_ - sy);
    s16 dx = (s16)(in->dx >> sh), dy = in->dy;
    if (dy < 0) { h = (s16)(h + dy); sy = (s16)(sy - dy); dy = 0; }
    s16 dh = (s16)desc_h(dst);
    if (dh < (s16)(dy + h)) h = (s16)(dh - dy);
    if (dx < 0) { w = (s16)(w + dx); sx = (s16)(sx - dx); dx = 0; }
    s16 dstride = (s16)desc_stride(dst);
    if (dstride < (s16)(dx + w)) w = (s16)(dstride - dx);
    o->sx = sx; o->sy = sy; o->dx = dx; o->dy = dy; o->w = w; o->h = h;
    return (o->w > 0 && h > 0) ? 1 : 0;
}

/* 1a21:17cc far_memset: byte fill (n = 0: nothing). */
void far_memset(FarPtr p, u8 v, u16 n)
{
    if (n == 0) return;
    for (u16 i = 0; i < n; i++) vwr(p.seg, (u16)(p.off + i), v);
}

/* 1a21:af7a far_memcpy: movedata(src.seg, src.off, dst.seg, dst.off, n). */
void far_memcpy(FarPtr src, FarPtr dst, u16 n)
{
    vmovedata(src.seg, src.off, dst.seg, dst.off, n);
}

/* 1a21:18e2 wait_vretraces: n x (wait while in retrace, then until retrace). PORT: the Hercules
 * branch (DS:AD1C == -4, port 3BAh bit 7) is dropped. */
void wait_vretraces(s16 n)
{
    for (s16 i = n; i > 0; i--) vga_wait_retrace_edge();
}

/* 1a21:1954 font_string_width: sum of the glyph advances (byte 0 of each 9-byte glyph); the font is at
 * segment DS:86EA, glyph c at offset c * 9 - 11Ah (`imul byte`: signed char * 9). */
s16 font_string_width(const char *s)
{
    u16 w = 0;
    u16 fseg = DSW(DS_g_font_seg);
    for (; *s; s++) {
        u16 bx = (u16)((s16)(s8)*s * 9);
        w = (u16)(w + rd8(fseg, (u16)(bx - 0x11A)));
    }
    return (s16)w;
}

/* 1a21:1924 wait_ticks (symbols.csv: pace_ticks): a metronome delay. deadline = (i32)DS:1D02 + ticks;
 * busy-wait until the tick counter DS:1F66 reaches it (signed compare of the high word, then unsigned low
 * word); then DS:1D02 = the counter. The first call after start-up waits against 0 and returns at once. */
void pace_ticks(s16 ticks)
{
    u32 deadline = DSL(DS_wait_last) + (u32)(s32)ticks;
    while (tick_after(deadline, ticks_now())) host_pump();
    DSL(DS_wait_last) = ticks_now();
}

/* ================================================================================ palette (§4.10) */

/* 1a21:1b9a ega_set_palreg: v > 7 (signed byte) gets bit 4; live table DS:1DA6[idx] (unchecked index);
 * wait until in vertical retrace; INT 10h AX=1000h (BL = idx, BH = v) or, for idx > 0Fh, AX=1001h
 * (overscan = BH). PORT: int86x replaced by ega_set_palette_reg (register 16 = overscan); an AX=1000h
 * call with BL > 0Fh (negative idx) is ignored like an invalid register. */
void ega_set_palreg(s16 idx, s16 v)
{
    u8 val = (u8)v;
    if ((s8)val > 7) val |= 0x10;
    DSB((u16)(DS_pal_live + idx)) = val;
    u8 al = idx > 0x0F ? 1 : 0;
    u8 bl = (u8)idx;
    vga_wait_in_retrace();
    if (al == 1) ega_set_palette_reg(16, val);
    else if (bl <= 0x0F) ega_set_palette_reg(bl, val);
}

/* 1a21:1bee ega_set_palette: 16 values at DS:table (signed > 7 gets bit 4) + overscan DS:AD2E,
 * INT 10h AX=1002h. No retrace wait, no shadow update. PORT: ega_set_palette_reg 0..16. */
void ega_set_palette(u16 table)
{
    u8 t[17];
    for (int i = 0; i < 16; i++) {
        u8 c = DSB((u16)(table + i));
        t[i] = (s8)c > 7 ? (u8)(c | 0x10) : c;
    }
    t[16] = DSB(DS_g_overscan);
    for (int i = 0; i < 17; i++) ega_set_palette_reg(i, t[i]);
}

/* 1a21:0da6 (slot 7): the black table DS:1DB8, DS:AD2F = 0. */
void pal_black(void)
{
    ega_set_palette(DS_pal_black_tab);
    DSB(DS_g_pal_visible) = 0;
}

/* 1a21:0db8 (slot 8): the live table DS:1DA6, DS:AD2F = 1. */
void pal_normal(void)
{
    ega_set_palette(DS_pal_live);
    DSB(DS_g_pal_visible) = 1;
}

/* 1a21:198a pal_load: copies the 17 bytes at DS:src into the live table DS:1DA6 (the hardware follows with the
 * next pal_normal / ega_set_palette). SR1's pal_reg12_13 (ega_set_palreg(0Ch, 0Dh)) was a misidentification. */
void pal_load(u16 src)
{
    for (u16 i = 0; i < 17; i++) DSB((u16)(DS_pal_live + i)) = DSB((u16)(src + i));
}

/* 1a21:19aa: pal_load(DS:1DDA), then registers 6, 7, 8 of the live table = 09 01 03. */
void pal_shadow_patch(void)
{
    pal_load(DS_pal_menu);
    DSB(DS_pal_live + 6) = 9;
    DSB(DS_pal_live + 7) = 1;
    DSB(DS_pal_live + 8) = 3;
}

/* 1a21:19c6: swap the 17 bytes DS:1DA6 <-> DS:1DFE. */
void pal_shadow_swap(void)
{
    for (u16 i = 0; i < 0x11; i++) {
        u8 t = DSB((u16)(DS_pal_live + i));
        DSB((u16)(DS_pal_live + i)) = DSB((u16)(DS_pal_alt + i));
        DSB((u16)(DS_pal_alt + i)) = t;
    }
}

/* ============================================================ rectangles, lines, frames (§4.9) */

/* 1a21:3e84: h horizontal lines through slot 12 in colour DS:AD2A (sign-extended), clipped to the
 * descriptor's w/h; the line target DS:9DC2 / DS:9BC0 = the descriptor's planes. */
void fill_rect(FarPtr d, s16 x, s16 y, s16 w, s16 h)
{
    s16 dw = (s16)desc_w(d);
    if ((s16)(x + w) > dw) w = (s16)(dw - x);
    s16 dh = (s16)desc_h(d);
    if ((s16)(y + h) > dh) h = (s16)(dh - y);
    u16 sseg = DSW(DS_g_draw_seg), soff = DSW(DS_g_draw_off);
    FarPtr pl = desc_planes(d);
    DSW(DS_g_draw_seg) = pl.seg;
    DSW(DS_g_draw_off) = pl.off;
    s16 c = (s16)(s8)DSB(DS_g_fill_col);
    s16 x1 = (s16)(x + w - 1);
    for (s16 i = 0; i < h; i++) {
        s16 yy = (s16)(y + i);
        drv_line(x, yy, x1, yy, c);
    }
    DSW(DS_g_draw_seg) = sseg;
    DSW(DS_g_draw_off) = soff;
}

/* 1a21:3f43: vertical line in colour DS:AD2B, clipped to the descriptor's h. */
void vline(FarPtr d, s16 x, s16 y, s16 h)
{
    s16 dh = (s16)desc_h(d);
    if ((s16)(h + y) > dh) h = (s16)(dh - y);
    u16 sseg = DSW(DS_g_draw_seg), soff = DSW(DS_g_draw_off);
    FarPtr pl = desc_planes(d);
    DSW(DS_g_draw_seg) = pl.seg;
    DSW(DS_g_draw_off) = pl.off;
    drv_line(x, y, x, (s16)(y + h - 1), (s16)(s8)DSB(DS_g_line_col));
    DSW(DS_g_draw_seg) = sseg;
    DSW(DS_g_draw_off) = soff;
}

/* 1a21:3fba: fill_rect(d, x, y, w, 1) in the line colour. */
void hline(FarPtr d, s16 x, s16 y, s16 w)
{
    u8 s = DSB(DS_g_fill_col);
    DSB(DS_g_fill_col) = DSB(DS_g_line_col);
    fill_rect(d, x, y, w, 1);
    DSB(DS_g_fill_col) = s;
}

/* 1a21:3ff6: 3-px frame outside the rect, style DS:AD2C. */
void draw_frame3(FarPtr d, s16 x, s16 y, s16 w, s16 h)
{
    const s16 k = 3;
    s16 xr = (s16)(x + w), xl = (s16)(x - k), yb = (s16)(y + h), yt = (s16)(y - k);
    s16 hh = (s16)(h + 2 * k), ww = (s16)(w + 2 * k);
    u8 sv = DSB(DS_g_fill_col);
    if (DSB(DS_g_frame_style) == 3) DSB(DS_g_fill_col) = DSB(DS_g_line_col);
    fill_rect(d, xl, yt, k, hh);
    fill_rect(d, x, yb, w, k);
    fill_rect(d, xr, yt, k, hh);
    fill_rect(d, x, yt, w, k);
    if (DSB(DS_g_frame_style) == 3) { DSB(DS_g_fill_col) = sv; return; }
    s16 si = (s16)(xl + 1), di = (s16)(yt + 1);
    s16 h4 = (s16)(hh - 2), w4 = (s16)(ww - 2);
    vline(d, si, di, h4);
    hline(d, si, (s16)(yb + 1), w4);
    vline(d, (s16)(xr + 1), di, h4);
    hline(d, si, di, w4);
    if (DSB(DS_g_frame_style) == 2) {
        si = (s16)(xl + 2); di = (s16)(yt + 2);
        s16 h2 = (s16)(hh - 4), w2 = (s16)(ww - 4);
        vline(d, si, di, h2);
        hline(d, si, yb, w2);
        vline(d, xr, di, h2);
        hline(d, si, di, w2);
    }
}

/* 1a21:41f0: 1-px outline outside the rect in the line colour. */
void draw_box1(FarPtr d, s16 x, s16 y, s16 w, s16 h)
{
    s16 xl = (s16)(x - 1), xr = (s16)(x + w), yt = (s16)(y - 1), yb = (s16)(y + h);
    s16 hh = (s16)(h + 2);
    vline(d, xl, yt, hh);
    hline(d, x, yb, w);
    vline(d, xr, yt, hh);
    hline(d, x, yt, w);
}

/* 1a21:b028 page_copy_rect: inclusive, byte-aligned rect copy through slot 0 (flags 1); only the upper clamps
 * remain in SR2 (x1 <= 13Fh, y1 <= C7h); the lower ones of SR1 are gone. */
void page_copy_rect(FarPtr s, FarPtr d, s16 x0, s16 y0, s16 x1, s16 y1)
{
    if (x1 > 0x13F) x1 = 0x13F;
    if (y1 > 0xC7) y1 = 0xC7;
    u16 m = x_align();
    Rect r;
    r.w = (s16)((((u16)x1 + (u16)~m + 1) & m) - ((u16)x0 & m));
    r.h = (s16)(y1 - y0 + 1);
    r.sx = x0; r.sy = y0;
    r.dx = x0; r.dy = y0;
    drv_blit(s, d, &r, 1);
}

/* ======================================================================================= bitmaps */

/* 1a21:9a68 arena_bitmap_alloc: header (0x12 + 2) + colour planes (unless flags & 2) + mask (flags != 0)
 * from a work arena - pool -1 (new) is the second arena DS:9E6C (a bump region that is never popped),
 * 0..3 the modes of arena 1 - then bitmap_alloc(w, h, flags, -1, d) fills the header. */
FarPtr arena_bitmap_alloc(s16 w, s16 h, s16 flags, s16 pool)
{
    s16 bpr = (s16)(((s16)(w - 1) >> px_shift()) + 1);
    u16 plane = (u16)(bpr * h);
    u16 size = (u16)((flags ? plane : 0) + 0x14);
    if (!(flags & 2)) size = (u16)(size + (u16)(plane * DSW(DS_g_nplanes)));
    FarPtr d;
    if (pool == -1) {
        d = ds_far(DS_arena2);
        d.off = (u16)(d.off + DSW(DS_arena2_used));
        DSW(DS_arena2_used) = (u16)(DSW(DS_arena2_used) + size);
    } else {
        d = arena_alloc((s16)size, pool);
    }
    far_wr(d.seg, (u16)(d.off + DESC_PLANES), far_make(d.seg, (u16)(d.off + DESC_BYTES)));
    bitmap_alloc(w, h, flags, -1, d);
    return d;
}

/* 0001:a4aa dancer_blit (Bob's Drive-In dancer): the 24x44 sprite whose far pointer is DS:AAFC/AAFE (picture
 * 0x47D, loaded by bob_sign_draw) through driver slot 2 (blit_masked, mode 1) to dst at (290, 87). */
void dancer_blit(FarPtr dst)
{
    Rect r;
    r.w = 0x18; r.h = 0x2C; r.sx = 0; r.sy = 0; r.dx = 0x122; r.dy = 0x57;
    drv_blit_masked(ds_far(DS_dancer_sprite), dst, &r, 1);
}

/* 1a21:ad48: the whole of bitmap s to d at (dx, dy) through slot 0. NB: the destination is the first
 * argument (the header had them the other way round). */
void bitmap_blit_at(FarPtr d, FarPtr s, s16 dx, s16 dy, s16 flags)
{
    Rect r;
    r.w = (s16)desc_w(s);
    r.h = (s16)desc_h(s);
    r.sy = 0; r.sx = 0;
    r.dx = dx; r.dy = dy;
    drv_blit(s, d, &r, (u8)flags);
}

/* 1a21:ade2 bitmap_blit_masked_at (new): like bitmap_blit_at through driver slot 2 (blit_masked, mode = flags). */
void bitmap_blit_masked_at(FarPtr d, FarPtr s, s16 dx, s16 dy, s16 flags)
{
    Rect r;
    r.w = (s16)desc_w(s);
    r.h = (s16)desc_h(s);
    r.sy = 0; r.sx = 0;
    r.dx = dx; r.dy = dy;
    drv_blit_masked(s, d, &r, flags);
}

/* 1a21:b09e bitmap_clear (new): planes := 0 (far_memset over `size`), the mask, if any, := FFh (one plane of
 * `size / planes` bytes: 1 = transparent). */
void bitmap_clear(FarPtr d)
{
    far_memset(desc_planes(d), 0, desc_size(d));
    if (has_mask(d)) far_memset(desc_mask(d), 0xFF, (u16)(desc_size(d) / DSW(DS_g_nplanes)));
}

/* 1a21:af96: new arena bitmap (with a mask if s has one) holding the sub-rect r = {w, h, x, y} of s,
 * copied through slot 0 with flags 3. */
FarPtr bitmap_extract(FarPtr s, const s16 *r, s16 pool)
{
    s16 w = r[0], h = r[1], x = r[2], y = r[3];
    FarPtr b = arena_bitmap_alloc(w, h, has_mask(s) ? 1 : 0, pool);
    Rect rr;
    rr.w = w; rr.h = h; rr.sx = x; rr.sy = y;
    rr.dy = 0; rr.dx = 0;
    drv_blit(s, b, &rr, 3);
    return b;
}

/* 1a21:a59c: d re-initialised with s's size (mask if d has one), then the whole of s copied. */
void bitmap_copy_into(FarPtr s, FarPtr d, s16 flags)
{
    s16 w = (s16)desc_w(s);
    bitmap_alloc(w, (s16)desc_h(s), has_mask(d) ? 1 : 0, -1, d);
    Rect r;
    r.w = w;
    r.h = (s16)desc_h(s);
    r.sy = 0; r.sx = 0; r.dy = 0; r.dx = 0;
    drv_blit(s, d, &r, (u8)flags);
}

/* 1a21:a624: *left / *right = first / last column that is opaque in some row (from the AND of the
 * first and last mask byte of every row; at most 8 transparent pixels are counted on each side). */
void bitmap_opaque_extent(FarPtr b, s16 *left, s16 *right)
{
    u16 stride = desc_stride(b);
    s16 h = (s16)desc_h(b);
    FarPtr m = desc_mask(b);
    FarPtr ml = far_add(m, (u16)(stride - 1));
    u8 accl = 0xFF, accr = 0xFF;
    if (h > 0) {
        for (u16 cx = (u16)h; cx; cx--) {
            accl &= vrdp(m);
            accr &= vrdp(ml);
            m.off = (u16)(m.off + stride);
            ml.off = (u16)(ml.off + stride);
        }
    }
    s16 factor = (s16)DSW(DS_g_px_factor);
    s16 cx = 0;
    u8 dl = 0x80;
    while (accl & dl) {
        cx++;
        dl >>= 1;
        if (cx >= 8) break;
    }
    *left = idiv32_16(cx, factor, NULL);
    cx = 0;
    dl = 1;
    while (accr & dl) {
        cx++;
        dl = (u8)(dl << 1);
        if (cx >= 8) break;
    }
    s16 q = idiv32_16(cx, factor, NULL);
    *right = (s16)((u16)(desc_stride(b) << px_shift()) - 1 - (u16)q);
}

/* 1a21:a758: d = s shifted right by (shift * DS:AD22) & 7 pixels; the width grows by
 * 8 when the last mask column is not transparent in the shifted-out bits; the mask is shifted in with
 * 1s (transparent). shift <= 0: plain copy into d. Returns the descriptor of bitmap_alloc (= d). */
FarPtr bitmap_make_shifted(FarPtr s, s16 shift, FarPtr d)
{
    u8 sh7 = (u8)((u16)(shift * (s16)DSW(DS_g_px_factor)) & 7);
    u8 inv = (u8)(8 - sh7);
    u16 lowmask = (u16)(0xFF >> inv);
    s16 h = (s16)desc_h(s);
    u16 stride = desc_stride(s);

    if (shift <= 0) {
        bitmap_copy_into(s, d, has_mask(s) ? 3 : 1);
        return d;
    }

    u8 acc = 0xFF;
    FarPtr ml = far_add(desc_mask(s), (u16)(stride - 1));
    if (h > 0) {
        for (u16 cx = (u16)h; cx; cx--) {
            acc &= vrdp(ml);
            ml.off = (u16)(ml.off + stride);
        }
    }
    u16 extra = ((acc & lowmask) == lowmask) ? 0 : (u16)(1u << px_shift());
    FarPtr nd = bitmap_alloc((s16)(desc_w(s) + extra), (s16)desc_h(s), has_mask(s) ? 1 : 0, -1, d);

    FarPtr dp = desc_planes(nd);
    FarPtr sp = desc_planes(s);
    s16 rows = (s16)(h * (s16)DSW(DS_g_nplanes));
    if (rows > 0) {
        for (u16 r = (u16)rows; r; r--) {
            u8 carry = 0;
            if ((s16)stride > 0) {
                for (u16 n = stride; n; n--) {
                    u8 b = vrdp(sp);
                    vwrp(dp, (u8)((b >> sh7) | (u8)((unsigned)carry << inv)));
                    dp.off++;
                    carry = (u8)(lowmask & b);
                    sp.off++;
                }
            }
            if (extra) {
                vwrp(dp, (u8)((unsigned)carry << inv));
                dp.off++;
            }
        }
    }

    if (!has_mask(s)) return nd;
    FarPtr dm = desc_mask(nd);
    FarPtr sm = desc_mask(s);
    if (h <= 0) return nd;
    u8 fill = (u8)(0xFF >> sh7);
    for (u16 r = (u16)h; r; r--) {
        u8 carry = 0xFF;
        if ((s16)stride > 0) {
            for (u16 n = stride; n; n--) {
                u8 b = vrdp(sm);
                vwrp(dm, (u8)((b >> sh7) | (u8)((unsigned)carry << inv)));
                dm.off++;
                carry = (u8)(lowmask & b);
                sm.off++;
            }
        }
        if (extra) {
            vwrp(dm, (u8)((u8)((unsigned)carry << inv) | fill));
            dm.off++;
        }
    }
    return nd;
}

/* 1a21:a9ac: transparent blit (slot 2, mode 1) of s at r (pixel x); when r->dx & 7 a
 * shifted copy is made first in arena pool 2 (released at the end). and_mask != 0: the clipped part of
 * the source mask is also ANDed into d's mask. */
void bitmap_blit_masked_shift(FarPtr s, FarPtr d, const Rect *r, s16 and_mask)
{
    Rect lr = *r;                        /* the original copies 20 bytes (0Ah words); only 12 are used */
    s16 shift = (s16)((u16)r->dx & (u16)~x_align());
    FarPtr src;
    if (shift > 0) {
        FarPtr t = arena_bitmap_alloc((s16)(desc_w(s) + 8), (s16)desc_h(s), has_mask(s) ? 1 : 0, 2);
        bitmap_make_shifted(s, shift, t);
        src = t;
        lr.w = (s16)desc_w(t);
    } else {
        src = s;
    }
    drv_blit_masked(src, d, &lr, 1);
    if (and_mask != 0) {
        Rect o;
        if (clip_rect(src, d, &lr, &o)) {
            u16 sstride = desc_stride(src);
            s16 w = o.w, h = o.h;
            u16 sskip = (u16)(sstride - (u16)w);
            u16 dskip = (u16)(desc_stride(d) - (u16)w);
            FarPtr sm = desc_mask(src);
            sm.off = (u16)(sm.off + (u16)(o.sy * (s16)sstride) + (u16)o.sx);
            FarPtr dm = desc_mask(d);
            dm.off = (u16)(dm.off + (u16)((s16)desc_stride(d) * o.dy) + (u16)o.dx);
            if (h > 0) {
                for (u16 rr = (u16)h; rr; rr--) {
                    if (w > 0) {
                        for (u16 n = (u16)w; n; n--) {
                            u8 a = vrdp(sm);
                            vwrp(dm, (u8)(vrdp(dm) & a));
                            dm.off++;
                            sm.off++;
                        }
                    }
                    sm.off = (u16)(sm.off + sskip);
                    dm.off = (u16)(dm.off + dskip);
                }
            }
        }
    }
    if (shift != 0) arena_pop(1);
}

/* 1a21:ab52: draws s behind what d already holds at (x, y): per plane
 * d |= ~smask & s & dmask, then (unless DS:79E2) dmask &= smask. No clipping. A shifted copy is made
 * in arena pool 2 when x & 7. */
void bitmap_composite_behind(FarPtr s, FarPtr d, s16 x, s16 y)
{
    s16 shift = (s16)((u16)x & (u16)~x_align());
    FarPtr src;
    if (shift > 0) {
        FarPtr t = arena_bitmap_alloc((s16)(desc_w(s) + 8), (s16)desc_h(s), has_mask(s) ? 1 : 0, 2);
        bitmap_make_shifted(s, shift, t);
        src = t;
    } else {
        src = s;
    }
    u16 dstride = desc_stride(d);
    u16 sstride = desc_stride(src);
    u16 skip = (u16)(dstride - sstride);
    u16 plane_skip = (u16)((s16)(desc_h(d) - desc_h(src)) * (s16)dstride);
    s16 sh = (s16)desc_h(src);
    u16 off = (u16)((u16)(y * (s16)dstride) + (u16)(x >> px_shift()));
    FarPtr dp = far_add(desc_planes(d), off);
    FarPtr dm0 = far_add(desc_mask(d), off), dm = dm0;
    FarPtr sp = desc_planes(src);
    FarPtr sm0 = desc_mask(src), sm = sm0;

    if ((s16)DSW(DS_g_nplanes) > 0) {
        for (u16 p = DSW(DS_g_nplanes); p; p--) {
            if (sh > 0) {
                for (u16 rr = (u16)sh; rr; rr--) {
                    if ((s16)sstride > 0) {
                        for (u16 n = sstride; n; n--) {
                            u8 al = (u8)~vrdp(sm);
                            al &= vrdp(sp);
                            al &= vrdp(dm);
                            vwrp(dp, (u8)(vrdp(dp) | al));
                            dm.off++; sp.off++; sm.off++; dp.off++;
                        }
                    }
                    dp.off = (u16)(dp.off + skip);
                    dm.off = (u16)(dm.off + skip);
                }
            }
            dp.off = (u16)(dp.off + plane_skip);
            dm = dm0;
            sm = sm0;
        }
    }
    if (DSB(DS_composite_keep_mask) == 0) {
        dm = dm0;
        sm = sm0;
        if (sh > 0) {
            for (u16 rr = (u16)sh; rr; rr--) {
                if ((s16)sstride > 0) {
                    for (u16 n = sstride; n; n--) {
                        u8 a = vrdp(sm);
                        vwrp(dm, (u8)(vrdp(dm) & a));
                        sm.off++; dm.off++;
                    }
                }
                dm.off = (u16)(dm.off + skip);
            }
        }
    }
    if (shift != 0) arena_pop(1);
}

/* 1a21:ad92: picture id (arena pool 2) to d at (x, y) with slot 0 flags 3; DS:9858 = the picture's
 * descriptor (left dangling after the arena_pop, as in the original). */
void pic_blit_at(FarPtr d, s16 id, u8 x, u8 y)
{
    FarPtr b = pic_get(id, 2);
    bitmap_blit_at(d, b, x, y, 3);
    ds_far_wr(DS_pic_last, b);
    arena_pop(1);
}

/* 1a21:ae2c: picture id drawn transparently at (x, y) with bitmap_blit_masked_shift(.., flag). */
void pic_draw_masked(FarPtr d, s16 id, u8 x, u8 y, s16 flag)
{
    FarPtr b = pic_get(id, 2);
    Rect r;
    r.w = (s16)desc_w(b);
    r.h = (s16)desc_h(b);
    r.sy = 0; r.sx = 0;
    r.dx = x; r.dy = y;
    bitmap_blit_masked_shift(b, d, &r, flag);
    arena_pop(1);
}

/* 1a21:ae94: d.mask &= s.mask over the rect r (clipped with clip_rect, byte columns). */
void mask_and_rect(FarPtr s, FarPtr d, const Rect *r)
{
    Rect o;
    if (!clip_rect(s, d, r, &o)) return;
    s16 w = o.w, h = o.h;
    u16 sskip = (u16)(desc_stride(s) - (u16)w);
    u16 dskip = (u16)(desc_stride(d) - (u16)w);
    FarPtr sp = desc_mask(s);
    sp.off = (u16)(sp.off + (u16)((s16)desc_stride(s) * o.sy) + (u16)o.sx);
    FarPtr dp = desc_mask(d);
    dp.off = (u16)(dp.off + (u16)((s16)desc_stride(d) * o.dy) + (u16)o.dx);
    if (h <= 0) return;
    for (u16 rr = (u16)h; rr; rr--) {
        if (w > 0) {
            for (u16 n = (u16)w; n; n--) {
                u8 a = vrdp(sp);
                vwrp(dp, (u8)(vrdp(dp) & a));
                dp.off++;
                sp.off++;
            }
        }
        sp.off = (u16)(sp.off + sskip);
        dp.off = (u16)(dp.off + dskip);
    }
}
