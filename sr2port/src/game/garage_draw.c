/* Garage: car pictures and their animations (garage.md, segment 1a21): ega_blit_4planes 7f5c, ega_vram_cache_strips
 * 7fc6 (video.md, static here), pump_overlay 8268, car_strip_setup 831c, car_compose 8546, car_draw 89fa, walker_anim
 * 9216, carhop_frames 92f2, carhop_strip 939a, carhop_anim 945a, town_bg_save 98e2.
 *
 * VGA path only (DS:AD1C = -2): the Tandy (-6) and CGA/Hercules branches of these routines (and 1a21:80ec, their strip
 * builder) are not ported - PORT, like the rest of the port (PORTING.md rule 7). The data layouts of the animation
 * records (Anim DS:A544, SpriteRef DS:9D1A, FrameRec DS:9B16, PageNode DS:A5FC) are described in platform/anim_rect.c. */
#include "game/garage_draw.h"

#include <string.h>

#include "game/ds_draw.h"
#include "game/records.h"
#include "game/snd.h"
#include "host.h"
#include "platform/platform.h"
#include "platform/video.h"
#include "platform/vga.h"

#define LOC          DSS(DS_location)             /* DS:1BC8: 1 garage, 2 street, 3 gas station, 5 newspaper, 6 Bob's */
#define YOFF         DS_g_anim_yoff               /* DS:1C08 */

static inline bool has_mask(FarPtr d) { return !far_is_null(desc_mask(d)); }
static inline FarPtr planes_plus(FarPtr d, u16 n) { FarPtr p = desc_planes(d); p.off = (u16)(p.off + n); return p; }

/* ============================================================== 1a21:7f5c, 7fc6 (EGA registers) */

/* 1a21:7f5c ega_blit_4planes: per plane p, map mask 1 << p / read map p, rows_mask_or of the source plane (and its
 * mask) onto the VRAM strip d. */
static void ega_blit_4planes(FarPtr src, FarPtr d, u16 mask_off, s16 wb, s16 rows, s16 sstr, u16 plane_size)
{
    u16 s = src.off;
    for (u16 bit = 1, p = 0; bit < 9; bit <<= 1, p++) {
        out(0x3C4, 2); out(0x3C5, (u8)bit);
        out(0x3CE, 4); out(0x3CF, (u8)p);
        rows_mask_or(d, src.seg, s, mask_off, sstr, wb, rows, wb);
        s = (u16)(s + plane_size);
    }
    out(0x3C4, 2); out(0x3C5, 0x0F);
}

/* 1a21:7fc6 ega_vram_cache_strips: for every frame, the bottom `rows` rows of the car are composited over the
 * background strip cached in VRAM (page 2 from row 46h): the strip's Tail record (tails + 8*i) gets its far pointer, the
 * first row of the car covered by it and the row count. The background comes from bg (a screen copy) or, when bg is
 * null, is a plain fill in `colour` (write mode 2). */
static void ega_vram_cache_strips(u16 tails, const FarPtr *frames, s16 n, s16 yoff, s16 rows, FarPtr bg, s16 colour)
{
    FarPtr pg = g_page(2);
    FarPtr p = planes_plus(pg, (u16)(0x46 * desc_stride(pg)));
    for (s16 i = 0; i < n; i++) {
        u16 t = (u16)(tails + 8 * i);
        FarPtr fr = frames[i];
        far_wr(DGROUP, t, p);
        DSW((u16)(t + 4)) = (u16)(desc_h(fr) - rows - yoff);
        DSW((u16)(t + 6)) = (u16)rows;
        p.off = (u16)(p.off + desc_stride(fr) * rows);
        FarPtr tp = far_rd(DGROUP, t);
        if (!far_is_null(bg)) {
            ega_save_rect(bg, (s16)desc_stride(fr), rows, tp, 0x28);
        } else {
            out(0x3CE, 5); out(0x3CF, 2);
            for (s16 c = 0; (s16)(desc_stride(fr) * rows) > c; c++) vwr(tp.seg, (u16)(tp.off + c), (u8)colour);
            out(0x3CE, 5); out(0x3CF, 0);
        }
        s16 s = (s16)desc_stride(fr);
        u16 off = (u16)(DSS((u16)(t + 4)) * s);
        ega_blit_4planes(planes_plus(fr, off), tp, (u16)(desc_mask(fr).off + off), s, rows, s, (u16)(desc_h(fr) * s));
    }
}

/* ============================================================== 1a21:831c, 8268, 98e2 */

/* 1a21:831c car_strip_setup: the background of a driving car from the strip bitmap `strip` (a descriptor over VRAM page 2):
 * a copy of the descriptor with the planes moved to row top-h and the height h (arena level 1), *out_bgsrc = a new
 * bitmap (strip.w, h) filled from it (masked when there is smoke), *out_bg = the shared descriptor (VGA); with a smoke
 * picture its mask rows are merged into *out_bgsrc and *out_ov = 0x790A (the overlay rect, y1 = bgsrc.h - 1). */
void car_strip_setup(FarPtr strip, FarPtr *out_bgsrc, s16 smoke_pic, FarPtr *out_bg, s16 top, s16 h, u16 *out_ov)
{
    s16 stride = (s16)desc_stride(strip);
    bool with_smoke = smoke_pic != 0;
    FarPtr d = arena_alloc(DESC_BYTES, 1);
    memcpy(far_mp(d), far_mp(strip), DESC_BYTES);
    wr16(d.seg, (u16)(d.off + DESC_PLANES), (u16)(rd16(d.seg, (u16)(d.off + DESC_PLANES)) + (s16)(top - h) * stride));
    wr16(d.seg, (u16)(d.off + DESC_H), (u16)h);
    wr16(d.seg, (u16)(d.off + DESC_SIZE), (u16)(h * DSS(DS_g_nplanes) * stride));
    *out_bgsrc = arena_bitmap_alloc((s16)desc_w(strip), h, with_smoke ? 1 : 0, 1);
    bitmap_copy_into(d, *out_bgsrc, 1);
    *out_bg = d;                                  /* VGA: the shared-plane descriptor itself */
    *out_ov = 0;
    if (!with_smoke) return;
    FarPtr sp = pic_get(smoke_pic, 2);
    u16 si = (u16)(desc_size(*out_bgsrc) / DSW(DS_g_nplanes));
    u16 skip = (u16)(desc_size(sp) / DSW(DS_g_nplanes) - si);
    far_memcpy(far_add(desc_mask(sp), skip), desc_mask(*out_bgsrc), si);
    arena_pop(1);
    *out_ov = DS_strip_ov;
    DSW(DS_strip_ov_y1) = (u16)(desc_h(*out_bgsrc) - 1);
}

/* 1a21:8268 pump_overlay: (gas station, car_draw mode 4) first the part of the view bitmap DS:AD58 (the car picture saved by
 * the last car_draw) that overlaps the pump goes into dst, then the pump slice idx of the sheet DS:B89A (picture 0x3E8). */
void pump_overlay(s16 x, s16 y0, s16 w, s16 h, FarPtr dst, s16 idx)
{
    s16 d = (s16)(x - (DSW(DS_car_x) & 0xFFF8));
    Rect r;
    r.sx = d < 0 ? 0 : d;
    r.dx = (s16)-d < 0 ? 0 : (s16)-d;
    r.w = (s16)(w + d) < w ? (s16)(w + d) : w;
    r.sy = 0;
    s16 dy = (s16)(DSS(DS_car_y) - y0);
    r.dy = dy;
    r.h = (s16)(h - dy);
    drv_blit_masked(ds_far(DS_view_desc), dst, &r, 1);
    Rect r2;
    r2.w = DSS((u16)(DS_PUMP_W + 2 * idx));
    r2.h = 0x3A;
    r2.sx = DSS((u16)(DS_PUMP_SX + 2 * idx));
    r2.sy = r2.dx = r2.dy = 0;
    drv_blit_masked(ds_far(DS_gas_pump_sheet), dst, &r2, 1);
}

/* 1a21:98e2 town_bg_save: the part of the town background behind the car (0x28 x 0x34 from page B at (0x68, 0x52)) into DS:95EE */
void town_bg_save(void)
{
    FarPtr a = arena_bitmap_alloc(0x28, 0x34, 0, 0);
    ds_far_wr(DS_town_bg, a);
    Rect r = { (s16)desc_w(a), (s16)desc_h(a), 0x68, 0x52, 0, 0 };
    drv_blit(g_back(), a, &r, 1);
}

/* ============================================================== 1a21:8546 car_compose */

/* The row of the first opaque pixel in the mask of the last pic_blit_at picture (DS:9858). */
static s16 roof_first_row(void)
{
    FarPtr p = ds_far(DS_pic_last);
    u16 n = (u16)(desc_size(p) / DSW(DS_g_nplanes));
    FarPtr m = desc_mask(p);
    u16 k = 0;
    while ((s16)n > (s16)k && rd8(m.seg, (u16)(m.off + k)) == 0xFF) k++;
    return idiv32_16((s16)k, (s16)desc_stride(p), NULL);
}

/* 1a21:8546 car_compose: frames[0] = the car body of `model` (LIB2, arena pool 1) with the customisations of flags
 * (2 rear bumper stripped, 1 front bumper stripped, 4 chopped roof, 8 scoop), the driver (picture DS:AD74); frames
 * 1..n-1 shifted by 2 px each; the wheels tyre_pics[i] (DS offset of 4 picture ids) and, with smoke, the ground shadow
 * (LIB2 #1138 = 0x472). Also the gas-cap point DS:AD62/AD64 (layout point L10/L11, no sticker any more). */
void car_compose(FarPtr *frames, s16 model, s16 flags, s16 driver, u16 tyre_pics, s16 smoke, s16 n)
{
    u16 P = (u16)(DS_MODEL_OVERLAYS + 10 * model);
    u16 L = (u16)(DS_MODEL_LAYOUT + 12 * model);
    frames[0] = pic_get(model_pic(model), 1);
    DSW(DS_gascap_x) = DSB((u16)(L + 10));
    DSW(DS_gascap_y) = DSB((u16)(L + 11));
    if (flags & 2) pic_blit_at(frames[0], DSS((u16)(P + 6)), 0, DSB((u16)(L + 7)));
    if (flags & 1) pic_blit_at(frames[0], DSS((u16)(P + 8)), DSB((u16)(L + 8)), DSB((u16)(L + 9)));
    s16 roof = 0;                                 /* [bp-8] */
    if (flags & 4) {
        pic_blit_at(frames[0], DSS((u16)(P + 2)), DSB((u16)(L + 4)), 0);
        roof = roof_first_row();
    }
    if ((flags & 8) && model != 0x15)
        pic_draw_masked(frames[0], DSS((u16)(P + 4)), DSB((u16)(L + 5)), DSB((u16)(L + 6)), 1);
    if (driver != 0) {
        FarPtr d = pic_get(DSS(DS_garage_car_driver), 2);
        bitmap_composite_behind(d, frames[0], DSB((u16)(L + 2)),
                                (s16)(DSB((u16)(L + 3)) - desc_h(d) + roof + 1));
        arena_pop(1);
    }
    for (s16 i = 1; i < n; i++)
        frames[i] = arena_bitmap_alloc((s16)(desc_w(frames[0]) + 8), (s16)desc_h(frames[0]), 1, 1);
    for (s16 i = 1; i < n; i++)                   /* VGA: each frame 2 px right of the previous */
        bitmap_make_shifted(frames[i - 1], 2, frames[i]);
    if (tyre_pics != 0) {
        for (s16 i = 0; i < n; i++) {
            FarPtr w = pic_get(DSS((u16)(tyre_pics + 2 * i)), 2);
            s16 dy = (s16)(desc_h(frames[i]) - desc_h(w));
            s16 sh = (s16)(2 * i);
            bitmap_composite_behind(w, frames[i], (s16)(DSB(L) + sh), dy);
            bitmap_composite_behind(w, frames[i], (s16)(DSB((u16)(L + 1)) + sh), dy);
            arena_pop(1);
        }
    }
    if (smoke != 0) {
        FarPtr s = pic_get(0x472, 2);
        s16 left, right;
        bitmap_opaque_extent(frames[0], &left, &right);
        s16 dy = (s16)(desc_h(frames[0]) - desc_h(s));
        for (s16 i = 0; i < n; i++) {
            s16 sh = (s16)(2 * i);
            bitmap_composite_behind(s, frames[i], (s16)(left + sh), dy);
            bitmap_composite_behind(s, frames[i], (s16)(right - desc_w(s) + sh + 1), dy);
        }
        arena_pop(1);
    }
}

/* ============================================================== 1a21:89fa car_draw */

/* 1a21:89fa car_draw. DS:7916 (car_pass) is the Tandy / CGA two-pass state (0, 1 first pass of a two-page driver, 2
 * second pass); on VGA it stays 0 and the loop runs once. */
void car_draw(s16 mode, s16 model, s16 flags, s16 driver, s16 y, u16 tyre_pics)
{
    s16 ytop = (s16)(y - 0x45);                   /* [bp-3Ah] */
    FarPtr frames[4] = { FAR_NULL, FAR_NULL, FAR_NULL, FAR_NULL };   /* [bp-30h] */
    FarPtr bgsrc = FAR_NULL, bg = FAR_NULL;       /* [bp-44h], [bp-3Ah] */
    u16 ov = 0;                                   /* [bp-1Ah] */
    cursor_ctl(-4);
    for (;;) {
        bool stat = mode == 3 || mode == 7 || mode == 8;
        s16 nfr = stat ? 1 : 4;                   /* [bp-2] */
        s16 w, h, masked;
        pic_info(model_pic(model), &w, &h, &masked);
        DSW(YOFF) = DSW((u16)(DS_LOC_YOFF + 2 * LOC));
        s16 colour = (DSS(DS_driver_id) == -2 || DSS(DS_driver_id) == -6)
                         ? DSS((u16)(DS_LOC_COLOUR_EGA + 2 * LOC)) : DSS((u16)(DS_LOC_COLOUR + 2 * LOC));
        s16 rows = stat ? (s16)(h - DSS(YOFF)) : DSS((u16)(DS_LOC_ROWS + 2 * LOC));
        s16 x = (s16)(((s16)(0x140 - w) / 2 + 4) & 0xFFF8);
        if (LOC == 1) {                           /* garage: the car is moved to the left */
            s16 xg = (s16)((0x120 - w) & 0xFFF8);
            if (xg < x) x = xg;
        }
        s16 xend = mode == 2 ? (s16)(0x140 - x) : (s16)(x + w);
        if (DSS(DS_car_pass) != 2) {
            if (mode == 4) {
                DSW(DS_game_over_cache_flag) = 1;
                FarPtr strip = ds_far(DS_car_strip);
                bitmap_init(strip, desc_planes(g_page(2)), FAR_NULL, 0x140, (s16)(0x46 - DSS(YOFF)), -2);
                FarPtr dp = desc_planes(strip);
                dp.off = (u16)(dp.off - (u16)(ytop * 0x28));
                ega_latch_copy(desc_planes(g_back()), dp, 0x28, (s16)desc_h(strip), 0, ytop, 0x28);
                /* PORT: the non-VGA branch (screen_save_rect of the rows into the strip) is not ported */
            }
            if (mode == 4) {
                if (LOC == 3) arena_reset_stacks();
                else arena_reset();
                if (LOC == 2 || LOC == 6) town_bg_save();
            } else {
                arena_reset_stacks();
            }
            car_compose(frames, model, flags, driver, tyre_pics, 1, nfr);
            if (!stat) {
                if (DSS(DS_car_pass) == 1) { DSW(DS_strip_w) = 0x140; DSW(DS_strip_x) = 0; }
                else { DSW(DS_strip_w) = (u16)xend; DSW(DS_strip_x) = (u16)(mode == 2 ? x : 0); }
                car_strip_setup(ds_far(DS_car_strip), &bgsrc, 0, &bg, 0x46, h, &ov);
            }
        }
        DSW(DS_car_x) = (u16)x;
        DSW(DS_car_y) = (u16)(y - desc_h(frames[0]) + 1);
        DSW(DS_car_h) = (u16)(desc_h(frames[0]) - DSS(YOFF));
        DSW(DS_gascap_x) = (u16)(DSS(DS_gascap_x) + x);
        DSW(DS_gascap_y) = (u16)(DSS(DS_gascap_y) + DSS(DS_car_y));
        s16 ax_x = (mode == 1 || mode == 4) ? (s16)(x - xend) : x;   /* [bp-22h] */
        if (DSS(DS_car_pass) != 2) {
            if (DSS(DS_driver_id) == -2) {
                FarPtr bgp = FAR_NULL;            /* [bp-4Ch] */
                if (mode == 3 || mode == 8) {
                    FarPtr s = ds_far(DS_car_strip);
                    s16 xb = (s16)(x / 8);        /* abs / sar 3 / sign: truncation toward zero */
                    bgp = desc_planes(s);
                    bgp.off = (u16)((u16)((desc_h(s) - rows) * desc_stride(s)) + (u16)xb + bgp.off);
                }
                ega_vram_cache_strips(DS_car_tails, frames, nfr, DSS(YOFF), rows, bgp, colour);
            }
            /* PORT: the Tandy / CGA / Hercules strips (1a21:80ec, driver slot 0 copies) are not ported */
            if (mode == 3) {
                FarPtr d = desc_planes(g_back());
                d.off = (u16)((u16)((y - h) * 0x28) + (u16)(x >> 3) + d.off + 0x28);
                ega_latch_copy_to40(ds_far(DS_car_tails), (s16)desc_stride(frames[0]), (s16)(desc_h(frames[0]) - DSS(YOFF)), d,
                                    (s16)desc_stride(frames[0]));
                present_back_page();
                break;
            }
            if (mode == 7 || mode == 8) {
                FarPtr d = desc_planes(g_front());
                d.off = (u16)((u16)((y - h) * 0x28) + (u16)(x >> 3) + d.off + 0x28);
                ega_latch_copy_to40(ds_far(DS_car_tails), (s16)desc_stride(frames[0]), (s16)(h - DSS(YOFF)), d,
                                    (s16)desc_stride(frames[0]));
                page_copy_rect(g_front(), g_back(), x, (s16)(y - h + 1), (s16)(x + w - 1), y);
                break;
            }
        }
        u16 anim = sprite_scene_setup(ax_x, y, frames, DS_car_tails, nfr, bg, bgsrc, ov);
        anim_script(anim, mode, xend);
        if (mode == 6) memcpy(mp(DGROUP, (u16)(DSW((u16)(anim + 0x12)) + 4)), mp(DGROUP, DS_car_dirty), 8);
        if (mode == 4 || mode == 1 || mode == 2 || mode == 6) {
            DSW(DS_car_running) = 0;
            DSW(DS_has_muffler) = (flags & 0x20) == 0;      /* 1 = the car has a muffler (engine sound variant) */
            DSW(DS_anim_player) = 1;                        /* DS:1BCA: animation running */
            if (car_runnable(DSW(DS_cur_car), 1) == 0) sound_vec(10, 1);   /* a car that cannot run makes the chug noise */
            else DSW(DS_car_running) = 1;
            if (DSS(DS_car_pass) != 2 && DSW(DS_car_running) != 0) race_isr_enter();
            while (drv_anim_step(anim) != 0) ;
            if (DSS(DS_car_pass) != 1 && DSW(DS_car_running) != 0) race_isr_leave();
            ds_far_wr(DS_view_desc, ds_far(DS_anim_view));
            if (DSW(DS_car_running) == 0) sound_vec(10, 0);
            DSW(DS_car_running) = 0;
            DSW(DS_anim_player) = 0;
            memcpy(mp(DGROUP, DS_car_dirty), mp(DGROUP, (u16)(DSW((u16)(anim + 0x12)) + 4)), 8);
        }
        if (LOC == 3 && mode == 4) {              /* gas station: the pump bitmaps over the car view */
            for (s16 i = 0; i < 2; i++) {
                s16 pw = DSS((u16)(DS_PUMP_FRAME_W + 2 * i));
                pump_overlay(DSS((u16)(DS_PUMP_X + 2 * i)), 0x7B, pw, 0x3A, far_rd(DGROUP, (u16)(DS_pump_save_a + 4 * i)), (s16)(i + 1));
                pump_overlay(DSS((u16)(DS_PUMP_X + 2 * i)), 0x7B, pw, 0x3A, far_rd(DGROUP, (u16)(DS_pump_save_b + 4 * i)), (s16)(i + 4));
            }
        }
        gfx_screen_mode(2);
        if (DSS(DS_driver_id) != -2) drv_copy_page(desc_planes(g_front()), desc_planes(g_back()));
        if (DSS(DS_car_pass) != 1) break;
        mode = 2;
        DSW(DS_car_pass) = 2;
    }
    DSW(DS_strip_x) = 0;
    DSW(DS_strip_w) = 0;
    DSW(YOFF) = 0;
    arena_reset_stacks();
    DSW(DS_car_pass) = 0;
    cursor_ctl(-2);
}

/* ============================================================== 1a21:9216 - 1a21:945a (Bob's car-hop) */

/* 1a21:9216 walker_anim (gas_leave, car-hop): rows y .. y+h-1 of picture `pic` (source sx, sy, width w) into the car strip
 * DS:7906 (top row base - 0x45) at x dx, clipped to the strip; at Bob's (DS:1BC8 = 6) the 7 background columns DS:5070
 * (14-byte records, width at +0, x at +4) are refreshed from the visible page. */
void walker_anim(FarPtr pic, s16 sx, s16 sy, s16 w, s16 h, s16 dx, s16 y, s16 base)
{
    FarPtr strip = ds_far(DS_car_strip);
    s16 top = (s16)(base - 0x45);                 /* [bp-4] */
    s16 y0 = y < top ? top : y;
    s16 bot = (s16)(top + desc_h(strip) - 1);
    s16 y1 = (s16)(y + h - 1);
    if (bot < y1) y1 = bot;
    Rect r;
    if (y0 <= y1) {
        r.w = w;
        r.h = (s16)(1 - (y0 - y1));
        r.sx = sx;
        r.sy = (s16)(y0 - y + sy);
        r.dx = dx;
        r.dy = (s16)(y0 - top);
        drv_blit(pic, strip, &r, 1);
    }
    if (LOC == 6) {
        r.h = (s16)desc_h(strip);
        r.sy = (s16)(base - 0x45);
        r.dy = 0;
        for (u16 k = 0; k < 7; k++) {
            r.w = DSS((u16)(DS_CARHOP_COLS + 0xE * k));
            r.dx = r.sx = DSS((u16)(DS_CARHOP_COLS + 4 + 0xE * k));
            drv_blit(g_front(), strip, &r, 1);
        }
    }
}

/* 1a21:92f2 carhop_frames (near): the car-hop's frames of `mode` (DS:7918 + 6*mode: picture, columns, count), cut from a
 * sheet of one or two rows (two when the columns differ from the count) */
static void carhop_frames(FarPtr *frames, s16 mode)
{
    u16 t = (u16)(DS_CARHOP_FRAMES + 6 * mode);
    s16 cols = DSS((u16)(t + 2)), n = DSS((u16)(t + 4));
    FarPtr p = pic_get(DSS(t), 2);
    s16 r[4] = { idiv32_16((s16)desc_w(p), cols, NULL), (s16)desc_h(p), 0, 0 };
    if (cols != n) r[1] = (s16)(r[1] >> 1);
    for (s16 i = 0; i < n; i++) {
        if (i == cols) { r[2] = 0; r[3] = r[1]; }
        frames[i] = bitmap_extract(p, r, 2);
        r[2] = (s16)(r[2] + r[0]);
    }
}

/* 1a21:939a carhop_strip (near): descriptor d over the VRAM strip (page 2 row 46h) of picture 0x458's size, with the
 * visible page's rows from 52h copied in */
static void carhop_strip(FarPtr d)
{
    s16 w, h, m;
    pic_info(0x458, &w, &h, &m);
    FarPtr pg = g_page(2);
    bitmap_init(d, planes_plus(pg, (u16)(desc_stride(pg) * 0x46)), FAR_NULL, w, h, DSS(DS_driver_id) == -2 ? -2 : -1);
    FarPtr dp = desc_planes(d);
    dp.off = (u16)(dp.off - 0xCD0);
    ega_latch_copy(desc_planes(g_front()), dp, (s16)desc_stride(d), (s16)desc_h(d), 0, 0x52, 0x28);
    /* PORT: the non-VGA branch (screen_save_rect(0, 0x52, w, h, d)) is not ported */
}

/* PORT: the original keeps this descriptor on its stack (SS = DGROUP). It goes to an otherwise unused paragraph above
 * 1 MB of mem[]; the overlay rect and the frame script of the Anim, which the animation step reads through DGROUP
 * offsets, go to the tyre-change scratch rect DS:960A and the default script area DS:9B16. */
#define CARHOP_DESC far_make(0xFFFF, 0x0010)

/* 1a21:945a carhop_anim: the car-hop at Bob's comes in (mode 0, from the left) or leaves (mode 1, to the right) behind
 * the opponent's car (opp != 0: the car picture DS:AD58 in front of her). */
void carhop_anim(s16 mode, s16 opp)
{
    FarPtr D = CARHOP_DESC;                       /* [bp-48h] */
    FarPtr ov_pic = FAR_NULL;                     /* [bp-4Ch]: the opponent's car cut out */
    Rect r = { 0, 0, 0, 0, 0, 0 };                /* [bp-2Ch] */
    arena_reset_stacks();
    carhop_strip(D);
    if (mode == 1) {
        Rect q = { 0x28, 0x34, 0, 0, 0x68, 0 };
        drv_blit(ds_far(DS_town_bg), D, &q, 1);
    }
    if (opp != 0) {
        s16 c[4];
        if (rect_clip_to_view(0, 0x52, (s16)desc_w(D), (s16)desc_h(D), c) != NULL) {
            s16 e[4] = { (s16)(c[2] - c[0]), (s16)(c[3] - c[1] + 1), (s16)(c[0] - DSS(DS_car_x)), (s16)(c[1] - DSS(DS_car_y)) };
            ov_pic = bitmap_extract(ds_far(DS_view_desc), e, 2);
            r.w = e[0]; r.h = e[1]; r.sx = 0; r.sy = 0; r.dx = c[0];
            r.dy = (s16)(desc_h(D) - desc_h(ov_pic));
            drv_blit_masked(ov_pic, D, &r, 1);
        }
    }
    FarPtr bgc, bgs;                              /* [bp-0B8h] copy with mask, [bp-80h] shared */
    u16 ovx;
    car_strip_setup(D, &bgc, 0x458, &bgs, 0x34, 0x34, &ovx);
    FarPtr a = FAR_NULL;                          /* [bp-6Ah] */
    if (mode == 0) {
        a = arena_bitmap_alloc(0x28, 0x34, 1, 1);
        Rect q = { 0x28, 0x34, 0, 0, 0, 0 };
        drv_blit(ds_far(DS_town_bg), a, &q, 1);
        q.sx = 0x68;
        drv_blit(bgc, a, &q, 2);
    }
    if (!far_is_null(ov_pic)) {
        mask_and_rect(ov_pic, bgc, &r);
        arena_pop(1);
    }
    DSW(DS_tc_rect) = 0;
    DSW((u16)(DS_tc_rect + 2)) = 0;
    DSW((u16)(DS_tc_rect + 4)) = (u16)(desc_w(bgc) - 1);
    DSW((u16)(DS_tc_rect + 6)) = (u16)(desc_h(bgc) - 1);
    FarPtr frames[14];
    carhop_frames(frames, mode);
    FarPtr fb[1] = { arena_bitmap_alloc(0x28, 0x34, 1, 2) };   /* [bp-66h] */
    u16 anim = sprite_scene_setup((s16)((mode == 0 ? 0xD8 : 0) + 0x68), 0x85, fb, 0, 1, bgs, bgc, DS_tc_rect);
    memcpy(mp(DGROUP, DS_anim_script), mp(DGROUP, mode == 0 ? DS_CARHOP_SCRIPT0 : DS_CARHOP_SCRIPT1), 16);
    DSW((u16)(anim + 0x10)) = DS_anim_script;
    if (mode == 1) {
        u16 nd = DSW((u16)(anim + 0x12));
        memcpy(mp(DGROUP, (u16)(nd + 4)), mp(DGROUP, DS_hop_dirty), 8);
        memcpy(mp(DGROUP, (u16)(DSW((u16)(nd + 0xE)) + 4)), mp(DGROUP, DS_hop_dirty), 8);
    }
    DSW(YOFF) = 0;
    u16 idx = mode == 0 ? DS_CARHOP_IDX0 : DS_CARHOP_IDX1;
    for (u16 si = 0; si < 0x4D; si++) {
        s16 x = DSS((u16)(anim + 0xC));
        s16 fi = DSC((u16)(idx + si));
        bitmap_make_shifted(frames[fi - 1], (s16)(x & (s16)~DSW(DS_g_x_align)), fb[0]);
        u32 until = DSL(DS_ticks) + 5;
        drv_anim_step(anim);
        while (tick_after(until, ticks_now())) host_pump();
    }
    drv_anim_step(anim);
    memcpy(mp(DGROUP, DS_hop_dirty), mp(DGROUP, (u16)(DSW((u16)(anim + 0x12)) + 4)), 8);
    if (mode == 1) {
        walker_anim(ds_far(DS_town_bg), 0, 0, 0x28, 0x34, 0x68, 0x52, 0xA8);
    } else {
        s16 w = (s16)desc_w(fb[0]);
        FarPtr t = arena_bitmap_alloc(w, 0x34, 1, 2);
        Rect q = { w, 0x34, 0, 0, 0, 0 };
        drv_blit(ds_far(DS_town_bg), t, &q, 1);
        drv_blit_masked(fb[0], t, &q, 1);
        drv_blit_masked(a, t, &q, 0);
        walker_anim(t, 0, 0, (s16)desc_w(t), 0x34, 0x68, 0x52, 0xA8);
    }
    if (DSS(DS_driver_id) != -2) drv_copy_page(desc_planes(g_front()), desc_planes(g_back()));
    arena_reset_stacks();
}
