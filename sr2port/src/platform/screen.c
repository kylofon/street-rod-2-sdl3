/* Screen helpers of code segment 1a21: drawing on page B with the mirror to page A, the status line,
 * full-screen pictures and the mouse pointer (video.md, 1a21:1f8e-2b44; cursor_ctl 1a21:2d90 and the
 * status functions 1a21:1cde-1e86 are garage / game_flow functions in SR2 that SR1 kept in this file).
 *
 * "page B" = DS:AD14 (back, drawn into), "page A" = DS:AD10 (visible). HIDE = cursor_ctl(-3),
 * SHOW = cursor_ctl(-1), MIRROR(x0, y0, x1, y1) = if (DS:AD26) page_copy_rect(B, A, ...). */
#include <string.h>

#include "platform/platform.h"
#include "platform/vga.h"
#include "platform/video.h"

#define HIDE() cursor_ctl(-3)
#define SHOW() cursor_ctl(-1)

static void mirror(s16 x0, s16 y0, s16 x1, s16 y1)
{
    if (DSW(DS_g_mirror)) page_copy_rect(g_back(), g_front(), x0, y0, x1, y1);
}

/* Status-line colours: DS:AD28..AD2C = scheme +37h..+3Bh. */
static void status_colours(void)
{
    memmove(mp(DGROUP, DS_g_text_fg), mp(DGROUP, (u16)(DSW(DS_g_scheme) + 0x37)), 5);
}

/* ============================================================================ page B + mirror */

/* 1a21:1f8e: HIDE, fill_rect(page B) in colour c, MIRROR, SHOW (fill colour restored). */
void screen_fill_rect(s16 x, s16 y, s16 w, s16 h, u8 c)
{
    u8 s = DSB(DS_g_fill_col);
    HIDE();
    DSB(DS_g_fill_col) = c;
    fill_rect(g_back(), x, y, w, h);
    mirror(x, y, (s16)(x + w - 1), (s16)(y + h - 1));
    DSB(DS_g_fill_col) = s;
    SHOW();
}

/* 1a21:22c6: text on page B through slot 3, MIRROR(x, y, end x - 1, y + 7) - SR2: only if x < end x.
 * No HIDE. */
void screen_text(s16 x, s16 y, const char *s)
{
    drv_draw_text(g_back(), x, y, s);
    if (DSW(DS_g_mirror) != 0 && x < DSS(DS_g_text_end_x))
        page_copy_rect(g_back(), g_front(), x, y, (s16)(DSS(DS_g_text_end_x) - 1), (s16)(y + 7));
}

/* 1a21:231c: hline on page B, MIRROR. */
void screen_hline(s16 x, s16 y, s16 w)
{
    hline(g_back(), x, y, w);
    mirror(x, y, (s16)(x + w - 1), y);
}

/* 1a21:236e: copies the byte-aligned page-B rect into dst (re-initialised; mask if dst has one) or
 * into a new arena bitmap (pool 2) when dst is null. Returns the bitmap. */
FarPtr screen_save_rect(s16 x, s16 y, s16 w, s16 h, FarPtr dst)
{
    u16 m = DSW(DS_g_x_align);
    s16 x1 = (s16)(((u16)(x + w) + (u16)~m) & m);
    if (x1 > 0x140) x1 = 0x140;
    s16 y1 = (s16)(y + h);
    if (y1 > 0xC8) y1 = 0xC8;
    x = (s16)((u16)x & m);
    FarPtr b;
    if (!far_is_null(dst)) {
        FarPtr dm = desc_mask(dst);
        s16 fl = (dm.off | dm.seg) ? 1 : 0;
        b = bitmap_alloc((s16)(x1 - x), (s16)(y1 - y), fl, -1, dst);
    } else {
        b = arena_bitmap_alloc((s16)(x1 - x), (s16)(y1 - y), 0, 2);
    }
    Rect r;
    r.w = (s16)desc_w(b);
    r.h = (s16)desc_h(b);
    r.sx = x; r.sy = y;
    r.dy = 0; r.dx = 0;
    HIDE();
    drv_blit(g_back(), b, &r, 1);
    SHOW();
    return b;
}

/* 1a21:246c: the whole bitmap to page B at (x & DS:AD24, y), pointer hidden. */
void screen_put_bitmap(FarPtr b, s16 x, s16 y)
{
    Rect r;
    r.w = (s16)desc_w(b);
    r.h = (s16)desc_h(b);
    r.sy = 0; r.sx = 0;
    r.dx = (s16)((u16)x & DSW(DS_g_x_align));
    r.dy = y;
    HIDE();
    drv_blit(b, g_back(), &r, 1);
    SHOW();
}

/* 1a21:24d8: screen_put_bitmap + MIRROR(x, y, x + w - 1, y + h - 1) (x not aligned). */
void screen_put_bitmap_mirror(FarPtr b, s16 x, s16 y)
{
    screen_put_bitmap(b, x, y);
    if (DSW(DS_g_mirror))
        page_copy_rect(g_back(), g_front(), x, y, (s16)(desc_w(b) + x - 1), (s16)(desc_h(b) + y - 1));
}

/* ================================================================================== status line */

/* 1a21:1c5a: status line at row y: message area x 58h, width A0h, text row y + 1; colours = scheme
 * +37h.. (left set); clears 320x10 on page B, MIRROR. */
void status_init(s16 y)
{
    DSS(DS_status_y) = (s16)(y + 1);
    DSW(DS_status_width) = 0xA0;
    DSW(DS_status_x0) = 0x58;
    DSW(DS_status_used) = 0;
    status_colours();
    fill_rect(g_back(), 0, y, 0x140, 0x0A);
    mirror(0, y, 0x13F, (s16)(y + 9));
}

/* 1a21:1cde (game_flow in SR2): message s in the status line. x < 0: centred in w (0 = DS:AD3A) from
 * x = DS:AD36, rounded down to a byte. The area right of x - 1 is cleared first when the last message was
 * not empty. The five colour bytes DS:AD28..AD2C are saved and restored around it. */
void status_print(s16 x, const char *s, s16 w)
{
    u8 saved[5];
    memcpy(saved, mp(DGROUP, DS_g_text_fg), 5);
    status_colours();
    if (w == 0) w = DSS(DS_status_width);
    s16 tx;
    if (x < 0) {
        x = DSS(DS_status_x0);
        s16 d = (s16)(w - font_string_width(s));
        tx = (s16)((u16)((s16)(d / 2) + x) & 0xFFF8);   /* cwd / sub ax,dx / sar ax,1 ; and al,F8h */
    } else {
        tx = x;
    }
    s16 bw = (s16)(0x140 - x);
    s16 ty = DSS(DS_status_y);
    HIDE();
    if (DSW(DS_status_used) != 0) fill_rect(g_back(), (s16)(x - 1), ty, bw, 8);
    drv_draw_text(g_back(), tx, ty, s);
    DSS(DS_status_used) = (s16)(s8)s[0];
    SHOW();
    if (DSW(DS_g_mirror))
        page_copy_rect(g_back(), g_front(), x > 8 ? (s16)(x - 8) : x, ty, (s16)(x + bw), (s16)(ty + 7));
    memcpy(mp(DGROUP, DS_g_text_fg), saved, 5);
}

/* ltoa(value, buf, radix) of the MS C runtime (34b0:16dc itoa / ltoa): '-' only for radix 10 and a negative value,
 * then the digits of the unsigned 32-bit magnitude ('a'.. above 9), NUL-terminated; returns buf.
 * The original's buffer (money_add) is 10 bytes: "-2147483648" would overflow it; the port's is 12. */
static char *crt_ltoa(s32 value, char *buf, u16 radix)
{
    char *p = buf;
    u32 v = (u32)value;
    if (radix == 10 && value < 0) {
        *p++ = '-';
        v = (u32)0 - v;
    }
    char *first = p;
    do {
        u8 c = (u8)(v % radix + '0');
        if (c > '9') c = (u8)(c + 0x27);
        *p++ = (char)c;
        v /= radix;
    } while (v != 0);
    *p = 0;
    for (char *a = first, *b = p - 1; a < b; a++, b--) { char t = *a; *a = *b; *b = t; }
    return buf;
}

/* 1a21:1df4 (game_flow in SR2): bankroll DS:A9FC (s32) += d (16-bit deltas, sign-extended), redisplayed
 * after the label: status_print(DS:AD3E, text) with DS:AD3C forced to 1 so the old amount is cleared;
 * then DS:AD36 = DS:AD32 (end x of the text) and DS:AD3A = 140h - DS:AD32. */
void money_add(s16 d)
{
    s16 saved = DSS(DS_status_used);
    DSW(DS_status_used) = 1;
    char buf[12];
    DSSL(DS_money) = (s32)((u32)DSSL(DS_money) + (u32)(s32)d);
    crt_ltoa(DSSL(DS_money), buf, 10);
    status_print(DSS(DS_status_money_x), buf, 0x30);
    DSW(DS_status_x0) = DSW(DS_g_text_end_x);
    DSW(DS_status_width) = (u16)(0x140 - DSW(DS_g_text_end_x));
    DSS(DS_status_used) = saved;
}

/* 1a21:1e86 (game_flow in SR2): label s at x = 0 of the status line (colours set, not restored), then the bankroll unless
 * s[1] is 'G' or ' '; the mirror is suspended meanwhile and done once at the end. */
void status_label(const char *s)
{
    u16 saved_mirror = DSW(DS_g_mirror);
    status_colours();
    HIDE();
    fill_rect(g_back(), 0, DSS(DS_status_y), (s16)(DSW(DS_status_x0) & 0xFFF8), 8);
    drv_draw_text(g_back(), 0, DSS(DS_status_y), s);
    DSW(DS_status_money_x) = (u16)((DSW(DS_g_text_end_x) + 7) & 0xFFF8);
    DSW(DS_g_mirror) = 0;
    if (s[1] != 'G' && s[1] != ' ') money_add(0);
    DSW(DS_g_mirror) = saved_mirror;
    SHOW();
    s16 y = DSS(DS_status_y);
    mirror(0, y, DSS(DS_status_x0), (s16)(y + 7));
}

/* ========================================================================== full-screen pictures */

/* 1a21:21be: picture id (low stack, pic_get pool 1) to page B at (DS:1E40, DS:1E42), at most DS:1E44 rows;
 * clr >= 0 clears 320 x DS:1E44 in that colour first. MIRROR, position reset to (0, 0), arena_pop_low(1). */
void show_picture(s16 id, s16 clr)
{
    HIDE();
    FarPtr b = pic_get(id, 1);
    if (clr >= 0) screen_fill_rect(0, 0, 0x140, DSS(DS_pic_rows), (u8)clr);
    Rect r;
    r.w = (s16)desc_w(b);
    s16 h = (s16)desc_h(b);
    if (h > DSS(DS_pic_rows)) h = DSS(DS_pic_rows);
    r.h = h;
    r.sy = 0; r.sx = 0;
    r.dx = DSS(DS_pic_x);
    r.dy = DSS(DS_pic_y);
    drv_blit(b, g_back(), &r, 1);
    SHOW();
    if (DSW(DS_g_mirror))
        page_copy_rect(g_back(), g_front(), DSS(DS_pic_x), DSS(DS_pic_y),
                       (s16)(r.dx + r.w - 1), (s16)(r.dy + r.h - 1));
    DSW(DS_pic_y) = 0;
    DSW(DS_pic_x) = 0;
    arena_pop_low(1);
}

/* 1a21:22a8: show_picture at (x, y). */
void show_picture_at(s16 id, s16 x, s16 y, s16 clr)
{
    DSS(DS_pic_x) = x;
    DSS(DS_pic_y) = y;
    show_picture(id, clr);
}

/* ================================================================================ mouse pointer */

/* 1a21:28dc: n * DS:A2D4 (mul: low word). */
u16 cursor_shape_offset(s16 n)
{
    return (u16)((u16)n * DSW(DS_ptr_shape_size));
}

/* 1a21:28e8 - video.md: the pointer bank (far_alloc): a 192x48 bitmap with mask holding the 3 shapes of
 * LIB1 picture 2 (24x48; SR1: 4 shapes of 24x64) at x = 0 and its 7 pre-shifted copies at x = 24, 48, ...;
 * plane stride 0x480; the save-under descriptor DS:A2BA right after the bank's mask (planes A000:1F40 on
 * EGA); mask inverted on EGA; shape 1, pointer at (9Eh, 80h), DS:AD2D = 2. */
void cursor_init(void)
{
    u16 t = 0x480;
    u16 size = (u16)(DSW(DS_g_nplanes) * 0x480 + 0x4A4);              /* bank + planes + mask + save-under descriptors */
    if (DSS(DS_driver_id) != -2)                          /* save-under planes in RAM (non-EGA) */
        size = (u16)(size + (u16)(((s16)0x18 >> DSB(DS_g_px_shift)) << 4));
    FarPtr bank = far_alloc(size, 1);
    far_wr(bank.seg, (u16)(bank.off + DESC_PLANES), far_make(bank.seg, (u16)(bank.off + DESC_BYTES)));
    FarPtr bd = bitmap_alloc((s16)(0x18 << DSB(DS_g_px_shift)), 0x30, 1, -1, bank);
    bitmap_clear(bd);
    FarPtr save = far_add(desc_mask(bd), t);
    ds_far_wr(DS_g_ptr_save, save);
    FarPtr data = DSS(DS_driver_id) == -2 ? far_make(0xA000, 0x1F40) : far_add(save, DESC_BYTES);
    u8 sh = DSB(DS_g_px_shift);
    bitmap_init(save, data, far_make(0, 0), (s16)(((s16)0x18 >> sh) << sh), 0x10,
                DSS(DS_driver_id) == -2 ? -2 : -1);
    memset(mp(DGROUP, DS_ptr_save_rect), 0, 0x0C);
    DSB(DS_ptr_mode) = 0;

    FarPtr pic = pic_get(2, 2);
    if (far_is_null(pic)) out_of_memory();
    Rect r;
    r.w = 0x18; r.h = 0x30;
    r.dy = 0; r.dx = 0; r.sy = 0; r.sx = 0;
    drv_blit(pic, bd, &r, 3);
    s16 si;
    for (si = 1; si <= (s16)(u16)~DSW(DS_g_x_align); si++) {
        FarPtr tmp = arena_bitmap_alloc(0x18, 0x30, 1, 1);
        bitmap_make_shifted(pic, si, tmp);
        r.dx = (s16)(r.dx + 0x18);
        drv_blit(tmp, bd, &r, 3);
        arena_pop_low(1);
    }
    arena_reset();
    ds_far_wr(DS_ptr_bank, desc_planes(bd));
    ds_far_wr(DS_ptr_bank_mask, desc_mask(bd));
    if (DSS(DS_driver_id) == -2) {                        /* EGA: 1 = opaque for the bit mask */
        FarPtr m = desc_mask(bd);
        u16 n = (u16)(desc_size(bd) >> 3);
        while (n--) {
            wr16(m.seg, m.off, (u16)~rd16(m.seg, m.off));
            m.off = (u16)(m.off + 2);
        }
    }
    DSW(DS_ptr_shape_size) = (u16)(desc_stride(bd) << 4);
    DSW(DS_ptr_shape) = 1;
    DSW(DS_ptr_shape_off) = cursor_shape_offset(1);
    DSW(DS_ptr_last_x1) = 0xFFFF;
    DSW(DS_cursor_x) = 0x9E;
    DSW(DS_cursor_y) = 0x80;
    DSW(DS_race_saved_cursor_x) = 0;
    DSW(DS_race_saved_cursor_y) = 0;
    DSB(DS_ui_level) = 2;
}

/* 1a21:2d90 cursor_ctl (garage in SR2): op >= 0 selects shape op (hot spot words DS:200C + 4 * op) and
 * redraws; -1 / -2 show (nesting counter DS:3B3B, byte), -2 also mirrors to page A; -3 / -4 hide (-3
 * leaves the page-A copy, -4 mirrors the erase). */
void cursor_ctl(s16 op)
{
    s16 x = DSS(DS_cursor_x), y = DSS(DS_cursor_y);
    x = x < 0 ? 0 : (x > 0x13F ? 0x13F : x);
    y = y < 0 ? 0 : (y > 0xC7 ? 0xC7 : y);
    if (op >= 0) {
        DSS(DS_ptr_shape) = op;
        DSW(DS_cursor_hot_x) = DSW((u16)(DS_cursor_hot_tab + ((u16)op << 2)));
        DSW(DS_cursor_hot_y) = DSW((u16)(DS_cursor_hot_tab + 2 + ((u16)op << 2)));
        DSW(DS_ptr_shape_off) = cursor_shape_offset(DSS(DS_ptr_shape));
        cursor_redraw(x, y);
        return;
    }
    if (op >= -2) {
        DSB(DS_cursor_visible)++;
        if (DSC(DS_cursor_visible) <= 0) return;
        DSB(DS_ptr_mode) = (u8)(op == -1 ? 2 : 1);
        if (DSS(DS_ptr_last_x1) >= 0) cursor_redraw(DSS(DS_ptr_last_x0), DSS(DS_ptr_last_y0));
        else cursor_redraw(x, y);
        DSB(DS_ptr_mode) = 1;
    } else {
        DSB(DS_cursor_visible)--;
        if (DSC(DS_cursor_visible) < 0) return;
        DSC(DS_ptr_mode) = (s8)(op == -3 ? -2 : -1);
        if (DSS(DS_ptr_last_x1) >= 0) cursor_redraw(DSS(DS_ptr_last_x0), DSS(DS_ptr_last_y0));
        else cursor_redraw(x, y);
        DSB(DS_ptr_mode) = 0;
    }
}

/* The body of cursor_redraw on the page pair (front, back). */
static void cursor_redraw_pair(s16 x, s16 y, FarPtr front, FarPtr back)
{
    u8 sh = DSB(DS_g_px_shift);
    if (DSS(DS_ptr_w) > 0) {
        if (DSS(DS_driver_id) == -2) {
            FarPtr pb = desc_planes(back);
            u16 off = (u16)(pb.off + (u16)(0x28 * DSS(DS_ptr_y)));
            off = (u16)(off + (u16)(DSS(DS_ptr_x) >> 3));
            ega_latch_copy_to40(desc_planes(ds_far(DS_g_ptr_save)), (s16)(DSS(DS_ptr_w) >> 3),
                                DSS(DS_ptr_h), far_make(pb.seg, off), (s16)((s16)0x18 >> sh));
        } else {
            /* PORT: non-EGA path kept for completeness (never taken on the VGA path). */
            Rect r;
            r.w = DSS(DS_ptr_save_rect);      r.h = DSS(DS_ptr_save_rect + 2);
            r.sx = DSS(DS_ptr_save_rect + 4); r.sy = DSS(DS_ptr_save_rect + 6);
            r.dx = DSS(DS_ptr_save_rect + 8); r.dy = DSS(DS_ptr_save_rect + 10);
            drv_blit(ds_far(DS_g_ptr_save), back, &r, 1);
        }
        DSW(DS_ptr_w) = 0;
    } else {
        DSW(DS_ptr_last_y0) = 0x3E8;
        DSW(DS_ptr_last_x0) = 0x3E8;
        DSW(DS_ptr_last_y1) = 0xFFFF;
        DSW(DS_ptr_last_x1) = 0xFFFF;
    }
    if (DSC(DS_ptr_mode) < 0) {
        if (DSS(DS_ptr_last_x1) < 0) return;
        if (DSC(DS_ptr_mode) < -1) return;
        DSB(DS_ptr_mode) = 1;
    } else {
        s16 px = (s16)((u16)x & DSW(DS_g_x_align));
        DSS(DS_ptr_x) = px;
        s16 w = (s16)(0x140 - px);
        DSS(DS_ptr_w) = w > 0x18 ? 0x18 : w;
        s16 h = (s16)(0xC8 - y);
        DSS(DS_ptr_h) = h > 0x10 ? 0x10 : h;
        DSS(DS_ptr_y) = y;
        u16 off = (u16)((u16)((s16)((u16)x & (u16)~DSW(DS_g_x_align)) * (s16)((s16)0x18 >> sh))
                        + DSW(DS_ptr_shape_off));
        FarPtr img = ds_far(DS_ptr_bank), msk = ds_far(DS_ptr_bank_mask);
        drv_draw_cursor(far_make(img.seg, (u16)(off + img.off)), far_make(msk.seg, (u16)(off + msk.off)),
                        desc_planes(ds_far(DS_g_ptr_save)), desc_planes(back),
                        (s16)(DSS(DS_ptr_x) >> sh), y, (s16)(DSS(DS_ptr_w) >> sh), DSS(DS_ptr_h));
    }
    /* mirror */
    s16 x2 = (s16)(x + 0x0F), y2 = (s16)(y + 0x0F);
    if (DSW(DS_g_mirror) != 0 && DSC(DS_ptr_mode) == 1) {
        s16 ly1 = DSS(DS_ptr_last_y1), lx1 = DSS(DS_ptr_last_x1);
        s16 ly0 = DSS(DS_ptr_last_y0), lx0 = DSS(DS_ptr_last_x0);
        page_copy_rect(back, front, lx0 > x ? x : lx0, ly0 > y ? y : ly0,
                       lx1 < x2 ? x2 : lx1, ly1 < y2 ? y2 : ly1);
    }
    DSS(DS_ptr_last_x0) = x;
    DSS(DS_ptr_last_y0) = y;
    DSS(DS_ptr_last_x1) = x2;
    DSS(DS_ptr_last_y1) = y2;
}

/* 1a21:2b44: restore the save-under on page B, then (mode > 0) save + draw the pre-shifted pointer through
 * slot 4, and mirror the union of the old and new rects to page A when mode == 1. SR2: in a race / event
 * screen (DS:B7AE) the pointer lives on the race page pair - VGA: front DS:AD0C, back DS:ADA2; otherwise
 * front DS:AD10, back DS:AD0C - y is clamped to 54h and the mirror flag is forced to 1 for the call. */
void cursor_redraw(s16 x, s16 y)
{
    if (DSB(DS_ptr_mode) == 0) return;
    if (DSB(DS_racing) != 0) {
        FarPtr front, back;
        if (DSS(DS_driver_id) == -2) { front = ds_far(DS_race_front); back = ds_far(DS_race_back); }
        else { front = ds_far(DS_g_front); back = ds_far(DS_race_front); }
        if (y > 0x54) y = 0x54;
        u16 saved_mirror = DSW(DS_g_mirror);
        DSW(DS_g_mirror) = 1;
        cursor_redraw_pair(x, y, front, back);
        DSW(DS_g_mirror) = saved_mirror;
    } else {
        cursor_redraw_pair(x, y, ds_far(DS_g_front), ds_far(DS_g_back));
    }
}
