/* Garage: the drawing helpers of the transmission screen and the engine bay (garage.md, segment 1a21:2e92 -
 * 1a21:3c14), and the "Under the Car" drawing 2925:2b26 - 2ff8.
 *
 * Engine bay drawing: DS:AD18 (far) is the bay bitmap (184 x 181, no background picture: the backdrop 0x41E is
 * drawn by bay_draw_parts, the parts are composed into the bitmap by bay_part_in_anim), shown at (0x20, 4) on page B;
 * DS:AD9C selects a part position (0 engine, 1 manifold, 4..16 carburettor positions of the manifold layouts),
 * DS:ADA0 the side of a bolt pair; DS:2018 = 0 composes without animating. Position tables: symbols.h (DS_ENGINE_SEAT,
 * DS_ENGINE_BOLT, DS_CONNECTOR_POS, DS_CARB_SEAT, DS_CARB_BOLT_OFS, DS_CTX_SLOT, DS_BOLT_RECTS, DS_FIRST_CARB_CTX). */
#include "game/garage_draw.h"

#include "game/ds_draw.h"
#include "game/records.h"
#include "game/snd.h"
#include "platform/platform.h"
#include "platform/video.h"

static inline FarPtr bay_bg(void) { return ds_far(DS_bay_bitmap); }
static inline bool has_mask(FarPtr d) { return !far_is_null(desc_mask(d)); }
static inline s16 slot_pic(s16 i) { return DSS((u16)(DS_bay_slot_pic + 2 * i)); }
static inline s16 grade_of(u16 part) { return parts_grade(DSS((u16)(part + PT_TYPE))); }

/* ================================================================ transmission screen helpers */

/* 1a21:2e92 trans_bolt_draw: bolt `bolt` (y from DS:201A) at tightness `tight` (LIB2 #1081 = 0x439, 16x6 frames);
 * a new bolt (tight 1) first slides in from the top. */
void trans_bolt_draw(s16 tight, s16 bolt)
{
    cursor_ctl(-3);
    FarPtr pic = pic_get(0x439, 2);
    s16 x = 0xB8;
    s16 y = (s16)(DSS((u16)(DS_TRANS_BOLT_Y + 2 * bolt)) + 10);
    if (tight == 1 && DSW(DS_g_mirror) != 0) {
        s16 r[4] = { 0x10, 6, 0x40, 0 };
        FarPtr spr = bitmap_extract(pic, r, 2);
        drv_slide_sprite(0x0E, 0x94, 0xB8, 8, y, spr, FAR_NULL);
        arena_pop(1);
    }
    Rect rc = { 0x10, 6, (s16)(tight << 4), 0, x, y };
    drv_blit(pic, g_back(), &rc, 1);
    cursor_ctl(-1);
    if (DSW(DS_g_mirror) != 0) page_copy_rect(g_back(), g_front(), x, y, (s16)(x + 0xF), (s16)(y + 5));
    arena_pop(1);
}

/* 1a21:2fc4 trans_bolt_remove_anim: the bolt comes out and falls */
void trans_bolt_remove_anim(s16 bolt)
{
    cursor_ctl(-3);
    FarPtr pic = pic_get(0x439, 2);
    s16 y = (s16)(DSS((u16)(DS_TRANS_BOLT_Y + 2 * bolt)) + 10);
    Rect rc = { 0x10, 6, 0, 0, 0xB8, y };
    drv_blit(pic, g_back(), &rc, 1);
    cursor_ctl(-1);
    if (DSW(DS_g_mirror) != 0) {
        page_copy_rect(g_back(), g_front(), 0xB8, y, 0xB8 + 0xF, (s16)(y + 5));
        sound_vec(17);                            /* the thud */
        cursor_ctl(-4);
        s16 r[4] = { 0x10, 6, 0x40, 0 };
        FarPtr spr = bitmap_extract(pic, r, 2);
        drv_slide_sprite(0x0E, 0x94, 0xB8, y, 0x94, spr, FAR_NULL);
        arena_pop(1);
        cursor_ctl(-2);
    }
    arena_pop(1);
}

/* 1a21:30f2 trans_slide_anim: the transmission slides up from below into place (reject: 16 px short of it, SR1 24) */
void trans_slide_anim(s16 id, s16 reject)
{
    cursor_ctl(-4);
    FarPtr p = pic_get(id, 2);
    s16 w = (s16)desc_w(p), h = (s16)desc_h(p);
    s16 x = (s16)(200 - w);
    s16 y = (s16)(0x7D - h);
    if (reject != 0) x = (s16)(0xB8 - w);
    if (DSW(DS_g_mirror) != 0) {
        s16 sx = (s16)(x - 8), sw = (s16)(w + 8);
        FarPtr save = screen_save_rect(sx, y, sw, h, FAR_NULL);
        drv_slide_sprite(0x0E, 0x94, sx, 0x94, y, p, FAR_NULL);
        Rect rc = { sw, h, 0, 0, 8, 0 };
        drv_blit_masked(p, save, &rc, 1);
        cursor_ctl(-2);
        screen_put_bitmap_mirror(save, sx, y);
        arena_pop(1);
    } else {
        drv_slide_sprite(0x0E, 0x94, x, y, y, p, FAR_NULL);
        cursor_ctl(-2);
    }
    arena_pop(1);
}

/* 1a21:322e trans_remove_anim: the empty transmission panel again (LIB2 #1055), the clank first; the original's
 * arguments are unused */
void trans_remove_anim(void)
{
    cursor_ctl(-3);
    FarPtr p = pic_get(0x41F, 2);
    sound_vec(9);                                 /* the clank */
    cursor_ctl(-1);
    screen_put_bitmap_mirror(p, 0x20, 10);
    arena_pop(1);
}

/* 1a21:328c trans_screen_draw: transmission and its two bolts, drawn without mirroring, then mirrored at once.
 * (The original's third argument is the segment of `bolts`: a far pointer.) */
void trans_screen_draw(s16 pic, u16 bolts)
{
    u16 m = DSW(DS_g_mirror);
    if (pic == 0) return;
    DSB(DS_bay_animate) = 0;
    DSW(DS_g_mirror) = 0;
    cursor_ctl(-3);
    trans_slide_anim(pic, 0);
    for (s16 i = 0; i < 2; i++) trans_bolt_draw(DSC((u16)(bolts + i)), i);
    cursor_ctl(-1);
    DSW(DS_g_mirror) = m;
    if (m != 0) page_copy_rect(g_back(), g_front(), 0x20, 10, 0xE7, 0xB8);
    DSB(DS_bay_animate) = 1;
}

/* ================================================================ engine bay helpers */

/* 1a21:332e bay_engine_idx: 0/1/2 for the engine pictures 0x425-0x427, else the picture value - 0x427 (sic) */
s16 bay_engine_idx(void)
{
    s16 v = slot_pic(0);
    if (v == 0x425) return 0;
    if (v == 0x426) return 1;
    if ((s16)(v - 0x427) != 0) return (s16)(v - 0x427);
    return 2;
}

/* 1a21:334a bay_carb_pos: carburettor position of context DS:AD9C (>= 4); returns 0/1/2 for the carburettor
 * picture 0x42D / 0x42E / 0x42F (used as the index into the bolt offsets). */
s16 bay_carb_pos(s16 *x, s16 *y)
{
    s16 r = 0;                                    /* PORT: [bp-4] is uninitialised for any other picture */
    s16 ctx = DSS(DS_bay_ctx);
    s16 v = slot_pic(DSC((u16)(DS_CTX_SLOT + ctx)));
    if (v == 0x42D) r = 0;
    else if (v == 0x42E) r = 1;
    else if (v == 0x42F) r = 2;
    *x = DSS((u16)(DS_CARB_SEAT + 4 * (ctx - 4)));
    *y = DSS((u16)(DS_CARB_SEAT + 2 + 4 * (ctx - 4)));
    return r;
}

/* 1a21:33aa bay_part_pos: where the part of context DS:AD9C goes (bottom-left, bay coordinates) */
void bay_part_pos(s16 *x, s16 *y)
{
    s16 ctx = DSS(DS_bay_ctx);
    if (ctx == 0) {
        s16 i = bay_engine_idx();
        *x = DSS((u16)(DS_ENGINE_SEAT + 4 * i));
        *y = DSS((u16)(DS_ENGINE_SEAT + 2 + 4 * i));
    } else if (ctx == 1) {
        *x = 0x38;
        *y = 0x5B;
    } else {
        bay_carb_pos(x, y);
    }
}

/* 1a21:33fe bay_bolt_pos: position of bolt side DS:ADA0 of the part of context DS:AD9C */
void bay_bolt_pos(s16 *x, s16 *y)
{
    s16 ctx = DSS(DS_bay_ctx);
    if (ctx == 0) {
        s16 i = bay_engine_idx();
        *x = DSS((u16)(DS_ENGINE_BOLT + 4 * i));
        *y = DSS((u16)(DS_ENGINE_BOLT + 2 + 4 * i));
        return;
    }
    if (ctx != 1) {
        s16 i = bay_carb_pos(x, y);
        *x = (s16)(*x + DSS((u16)(DS_CARB_BOLT_OFS + 4 * i)));
        *y = (s16)(*y + DSS((u16)(DS_CARB_BOLT_OFS + 2 + 4 * i)));
        return;
    }
    s16 m = slot_pic(1);
    if (DSS(DS_bay_side) == 1) *x = (m == 0x429 || m == 0x428) ? 0x84 : 0x9A;
    else *x = 0x4B;
    if (m == 0x4B8) *x = (s16)(*x + 1);
    *y = 0x5A;
}

/* 1a21:34a2 bay_bolt_draw: bolt state `tight` (1 loose, 2, 3 tight) at the bolt position. All states are cut from
 * the sprite LIB2 0x4A2 by the rectangles DS:2092 + 20 * tight; the background comes back from the bay bitmap, a
 * new (loose) bolt slides in from the top. */
void bay_bolt_draw(s16 tight)
{
    s16 x, y;
    bay_bolt_pos(&x, &y);                         /* [bp-12h], [bp-14h] */
    cursor_ctl(-4);
    FarPtr P = pic_get(0x4A2, 2);
    FarPtr T = arena_bitmap_alloc(8, 12, 1, 2);
    bitmap_clear(T);
    drv_blit(P, T, (const Rect *)mp(DGROUP, (u16)(DS_BOLT_RECTS + 20 * tight)), 3);
    y = (s16)(y - 12);
    Rect rc = { 8, 12, (s16)(x - 8), (s16)(y - 4), (s16)(x + 0x18), y };
    x = (s16)(x + 0x18);
    drv_blit_shifted(bay_bg(), g_back(), &rc);
    s16 xa = (s16)((u16)~DSW(DS_g_x_align) & (u16)x);
    bool animate = tight == 1 && DSW(DS_g_mirror) != 0;
    if (animate) {
        u16 rec = (u16)(DS_BOLT_RECTS + 20);
        FarPtr T2 = arena_bitmap_alloc(8, 12, 1, 2);
        DSW((u16)(rec + 4)) = (u16)(DSW((u16)(rec + 4)) + 8);
        drv_blit(P, T2, (const Rect *)mp(DGROUP, rec), 3);
        DSW((u16)(rec + 4)) = (u16)(DSW((u16)(rec + 4)) - 8);
        if (xa != 0) {
            FarPtr T3 = arena_bitmap_alloc(0x10, 12, 1, 2);
            bitmap_make_shifted(T2, xa, T3);      /* the result is not used by the original */
        }
    }
    FarPtr spr = T;
    if (xa != 0) spr = bitmap_make_shifted(T, xa, P);
    x = (s16)(x & DSW(DS_g_x_align));
    if (animate) drv_slide_sprite(4, 0xB9, x, -8, y, spr, spr);
    else drv_slide_sprite(4, 0xB9, x, y, y, spr, FAR_NULL);
    cursor_ctl(-2);
    arena_pop(2);
    if (animate) arena_pop((s16)(xa == 0 ? 1 : 2));
}

/* 1a21:369a bay_bolt_remove_anim: the bolt comes out: background restored, the loosened bolt falls */
void bay_bolt_remove_anim(void)
{
    s16 x, y;
    bay_bolt_pos(&x, &y);                         /* [bp-1Ah], [bp-18h] */
    cursor_ctl(-4);
    FarPtr P = pic_get(0x4A2, 2);
    FarPtr T1 = arena_bitmap_alloc(8, 12, 1, 2);
    drv_blit(P, T1, (const Rect *)mp(DGROUP, (u16)(DS_BOLT_RECTS + 20)), 3);
    FarPtr T2 = arena_bitmap_alloc(8, 12, 1, 2);
    u16 rec = (u16)(DS_BOLT_RECTS + 20);
    DSW((u16)(rec + 4)) = (u16)(DSW((u16)(rec + 4)) + 8);
    drv_blit(P, T2, (const Rect *)mp(DGROUP, rec), 3);
    DSW((u16)(rec + 4)) = (u16)(DSW((u16)(rec + 4)) - 8);
    s16 w = (s16)desc_w(T1), h = (s16)desc_h(T1);
    s16 sx = (s16)(x - 8);
    x = (s16)(x + 0x18);
    y = (s16)(y - h);
    Rect rc = { w, h, sx, (s16)(y - 4), x, y };
    drv_blit_shifted(bay_bg(), g_back(), &rc);
    if (DSW(DS_g_mirror) != 0) {
        page_copy_rect(g_back(), g_front(), x, y, (s16)(x + w - 1), (s16)(y + h - 1));
        sound_vec(17);                            /* the thud */
        x = (s16)(x & DSW(DS_g_x_align));
        drv_slide_sprite(4, 0xB8, x, (s16)(y + 3), 0xB9, T1, T2);
    }
    cursor_ctl(-2);
    arena_pop(3);
}

/* 1a21:381e bay_part_in_anim: part `pic` of context DS:AD9C into the bay bitmap (not when rejected) and, with
 * DS:2018, slid in from the top on the screen (a rejected part stops 20 px above its place and is not composed).
 * All parts are bottom-aligned (SR1 excluded the engine pictures). */
void bay_part_in_anim(s16 pic, s16 reject)
{
    s16 x, y;
    bay_part_pos(&x, &y);                         /* [bp-10h], [bp-0Eh] */
    if (pic == 0x4B8) x = (s16)(x - 1);
    if (DSB(DS_bay_animate) != 0) cursor_ctl(-4);
    FarPtr P = pic_get(pic, 2);
    s16 h = (s16)desc_h(P);
    y = (s16)(y - h);
    s16 shift = (s16)((u16)~DSW(DS_g_x_align) & (u16)x);
    FarPtr Q = P;
    if (shift != 0) {
        Q = arena_bitmap_alloc((s16)(desc_w(P) + 8), h, has_mask(P) ? 1 : 0, 2);
        bitmap_make_shifted(P, shift, Q);
    }
    x = (s16)(x & DSW(DS_g_x_align));
    if (reject == 0) {
        Rect rc = { (s16)desc_w(Q), h, 0, 0, (s16)(x - 8), (s16)(y - 4) };
        drv_blit_masked(Q, bay_bg(), &rc, 1);
    } else {
        y = (s16)(y - 0x14);
    }
    if (DSB(DS_bay_animate) != 0) {
        s16 from = y;
        if (DSW(DS_g_mirror) != 0) from = (s16)(4 - h);
        drv_slide_sprite(4, 0xB9, (s16)(x + 0x18), from, y, Q, FAR_NULL);
        cursor_ctl(-2);
    }
    arena_pop(1);
    if (Q.off != P.off || Q.seg != P.seg) arena_pop(1);
}

/* 1a21:39a4 bay_part_out_anim: a rejected part goes away: the strip of the bay bitmap above its place is restored
 * on the back page (and copied to the front page) */
void bay_part_out_anim(void)
{
    s16 x, y;
    bay_part_pos(&x, &y);
    y = (s16)(y - 0x14);
    cursor_ctl(-3);
    FarPtr bg = bay_bg();
    s16 w = (s16)(desc_w(bg) - (x - 8));
    s16 h = (s16)(y - 3);
    Rect rc = { w, h, (s16)(x - 8), 0, (s16)(x + 0x18), 4 };
    drv_blit(bg, g_back(), &rc, 1);
    cursor_ctl(-1);
    if (DSW(DS_g_mirror) != 0)
        page_copy_rect(g_back(), g_front(), (s16)(x + 0x18), 4, (s16)(w + x + 0x18), (s16)(h + 4));
}

/* 1a21:3a5a bay_connector_draw: the engine connector (0x432 disconnected / 0x433 connected; one position pair per
 * engine at DS:2036) */
void bay_connector_draw(void)
{
    cursor_ctl(-3);
    s16 i = bay_engine_idx();
    s16 cx = DSS((u16)(DS_CONNECTOR_POS + 4 * i));
    s16 cy = DSS((u16)(DS_CONNECTOR_POS + 2 + 4 * i));
    Rect rc = { 0x25, 0x17, (s16)(cx - 0x2C), (s16)(cy - 0x1A), (s16)(cx - 0x0C), (s16)(cy - 0x16) };
    drv_blit_shifted(bay_bg(), g_back(), &rc);
    s16 px, py;
    if (DSS(DS_bay_connector_pic) == 0x432) { px = (s16)(cx - 9); py = rc.dy; }
    else { px = rc.dx; py = (s16)(rc.dy + 2); }
    pic_draw_masked(g_back(), DSS(DS_bay_connector_pic), (u8)px, (u8)py, 0);
    cursor_ctl(-1);
    if (DSW(DS_g_mirror) != 0)
        page_copy_rect(g_back(), g_front(), rc.dx, rc.dy, (s16)(cx + 0x18), (s16)(rc.dy + 0x16));
}

/* 1a21:3b34 bay_draw_car: rebuilds the bay bitmap (184 x 181, cleared) with the fitted parts, bottom-up */
void bay_draw_car(u16 car)
{
    s16 save_ctx = DSS(DS_bay_ctx);
    DSB(DS_bay_animate) = 0;
    if (!far_is_null(bay_bg())) arena_reset_stacks();     /* 1a21:9b26 arena_release_levels */
    FarPtr b = arena_bitmap_alloc(0xB8, 0xB5, 0, 1);
    ds_far_wr(DS_bay_bitmap, b);
    bitmap_clear(b);
    s16 pic = slot_pic(0);
    if (pic != 0) {
        DSW(DS_bay_ctx) = 0;
        bay_part_in_anim(pic, 0);
        DSB(DS_FIRST_CARB_CTX) = pic == 0x425 ? 4 : 5;
        pic = slot_pic(1);
        if (pic != 0) {
            DSW(DS_bay_ctx) = 1;
            bay_part_in_anim(pic, 0);
            s16 ctx = DSC((u16)(DS_FIRST_CARB_CTX + grade_of(DSW((u16)(car + CR_MANIFOLD)))));
            for (u16 i = 0; i < 3; i++) {
                DSW(DS_bay_ctx) = (u16)ctx;
                ctx++;
                s16 cp = DSS((u16)(DS_bay_slot_pic + 4 + 2 * i));
                if (cp != 0) bay_part_in_anim(cp, 0);
            }
        }
    }
    DSB(DS_bay_animate) = 1;
    DSW(DS_bay_ctx) = (u16)save_ctx;
}

/* 1a21:3c14 bay_draw_parts: the backdrop 0x41E and the bay bitmap to the screen with the bolts and the connector;
 * own: the tool bar 0x47B at the right (and the copied width is 0x110 instead of 200); clank: sound vector 9. */
void bay_draw_parts(s16 own, s16 clank)
{
    s16 save_side = DSS(DS_bay_side), save_ctx = DSS(DS_bay_ctx);
    u16 m = DSW(DS_g_mirror);
    s16 width = 200;
    DSW(DS_g_mirror) = 0;
    show_picture_at(0x41E, 0x18, 0, -1);
    screen_put_bitmap(bay_bg(), 0x20, 4);
    for (u16 i = 0; i < 6; i++) {
        s16 bp = DSS((u16)(DS_bay_bolt_pic + 2 * i));
        if (bp == 0) continue;
        DSW(DS_bay_ctx) = DSW((u16)(DS_BOLT_CTX + 2 + 2 * i));
        DSW(DS_bay_side) = DSW((u16)(DS_BOLT_SIDE + 2 + 2 * i));
        bay_bolt_draw(bp);
    }
    if (DSS(DS_bay_connector_pic) != 0) bay_connector_draw();
    if (own != 0) {
        show_picture_at(0x47B, 0xE0, 0, -1);
        width = 0x110;
    }
    if (clank != 0) sound_vec(9);
    DSW(DS_g_mirror) = m;
    if (m != 0) page_copy_rect(g_back(), g_front(), 0x18, 0, (s16)(width + 0x17), 0xBC);
    DSW(DS_bay_ctx) = (u16)save_ctx;
    DSW(DS_bay_side) = (u16)save_side;
}

/* ================================================================ "Under the Car" (garage code 14, segment 2925) */

/* 2925:2b26 under_car_slide_in (near): the jack slides in from the right and lifts the car: the backdrop strip, the
 * composed car (placed so that it sits at (0x50 - DS:AD5C, 0x8F - DS:AD5E)) and picture 0x3FC go into a work bitmap W;
 * the jack 0x4AE is pre-shifted by 2, 4, 6 px; x runs 0x50..0x80 in steps of 2, y from 0xAC up to 0x8F. */
void under_car_slide_in(void)
{
    FarPtr P = pic_get(0x3FA, 2);                 /* the garage backdrop */
    s16 r4[4] = { 0x68, 0x28, 0x50, 0x8F };
    FarPtr bgp = bitmap_extract(P, r4, 1);        /* [bp-26h] */
    arena_pop(1);
    FarPtr W = arena_bitmap_alloc((s16)desc_w(bgp), (s16)desc_h(bgp), 1, 1);
    bitmap_clear(W);
    FarPtr frames[1];                             /* [bp-22h] */
    car_compose(frames, DSS(DS_garage_car_model), 0, 0, (u16)(DS_TYRE_PICS + 8 * DSS(DS_garage_car_tyre)), 0, 1);
    Rect rc;
    rc.w = 0x68;
    rc.sx = (s16)(0x50 - DSS(DS_car_x));
    rc.sy = (s16)(0x8F - DSS(DS_car_y));
    rc.h = (s16)(desc_h(frames[0]) - rc.sy);
    rc.dx = 0;
    rc.dy = 0;
    drv_blit(frames[0], W, &rc, 3);
    arena_pop_low(1);
    FarPtr P2 = pic_get(0x3FC, 2);                /* the row limit */
    rc.w = (s16)desc_w(P2);
    rc.h = (s16)(desc_h(P2) - 3);
    rc.sy = 0;
    rc.sx = 0;
    rc.dx = 0x40;
    rc.dy = (s16)(desc_h(W) - rc.h);
    drv_blit(P2, W, &rc, 3);
    arena_pop(1);
    FarPtr jacks[4];                              /* [bp-3Ah]: the jack, then copies shifted by 2, 4, 6 px */
    jacks[0] = pic_get(0x4AE, 1);
    s16 nalign = (s16)~DSW(DS_g_x_align);
    for (s16 k = 2; k < nalign; k += 2) {
        FarPtr jk = arena_bitmap_alloc((s16)((1 << (DSW(DS_g_px_shift) & 0x1F)) + desc_w(jacks[0])), (s16)desc_h(jacks[0]), 1, 1);
        jacks[k >> 1] = jk;
        bitmap_make_shifted(jacks[0], k, jk);
    }
    cursor_ctl(-4);
    s16 x = 0x50, y = 0xAC;
    while (y > 0x8F) {
        rc.dy = y;
        s16 t = (s16)(x - 2);
        if (t < 0x50) t = 0x50;
        t = (s16)(t & (s16)DSW(DS_g_x_align));
        rc.dx = t;
        s16 w = (s16)(0xB7 - t);
        if (w > 0x40) w = 0x40;
        rc.w = w;
        s16 h = (s16)(0xB6 - y);
        if (h > 0xC) h = 0xC;
        rc.h = h;
        rc.sx = (s16)(t - 0x50);
        rc.sy = (s16)(y - 0x8F);
        drv_blit(bgp, g_back(), &rc, 1);
        s16 k = (s16)(~((s16)DSW(DS_g_x_align) >> 1) & (x >> 1));
        bitmap_blit_masked_at(g_back(), jacks[k], x, y, 0);
        drv_blit_masked(W, g_back(), &rc, 1);
        wait_vretraces(2);
        s16 e1 = (s16)(desc_h(jacks[k]) + y + 1);
        if (e1 > 0xB6) e1 = 0xB6;
        s16 e0 = (s16)(desc_w(jacks[k]) + x);
        if (e0 > 0xB7) e0 = 0xB7;
        page_copy_rect(g_back(), g_front(), rc.dx, y, e0, e1);
        if (x < 0x80) x = (s16)(x + 2);
        if (y > 0x8F) y--;
        if (x >= 0x80) break;
    }
    cursor_ctl(-2);
    arena_pop_low((s16)(~((s16)DSW(DS_g_x_align) >> 1) + 3));
}

/* 2925:2e60 under_car_draw (near; the symbol table calls it video_options_menu2): the backdrop 0x4AF (kept in DS:9718 for
 * erasing) on page B at (0x18, 0x15), then the fitted exhaust manifold DS:822A[grade] at (200, 0x50), muffler
 * DS:822E[grade] at (0xA8, 0x5C) and differential DS:8232[grade] at (0x50, 0x3F); grade = the catalogue byte of the part. */
void under_car_draw(void)
{
    FarPtr bd = pic_get(0x4AF, 2);
    ds_far_wr(DS_uc_backdrop, bd);
    cursor_ctl(-3);
    bitmap_blit_at(g_back(), bd, 0x18, 0x15, 1);
    u16 car = DSW(DS_cur_car);
    static const struct { u16 slot, tab; s16 x, y; } part[3] = {
        { CR_EXHAUST, 0, 200, 0x50 }, { CR_MUFFLER, 4, 0xA8, 0x5C }, { CR_DIFF, 8, 0x50, 0x3F },
    };
    for (int i = 0; i < 3; i++) {
        u16 p = DSW((u16)(car + part[i].slot));
        if (p == 0) continue;
        s16 grade = parts_grade(DSS((u16)(p + PT_TYPE)));
        FarPtr pic = pic_get(DSS((u16)(DS_UNDERCAR_PICS + part[i].tab + 2 * grade)), 2);
        bitmap_blit_at(g_back(), pic, part[i].x, part[i].y, 1);
        arena_pop(1);
    }
    cursor_ctl(-1);
    page_copy_rect(g_back(), g_front(), 0x18, 0x15, 0x127, 0xA0);
}

/* 2925:2ff8 under_car_part_pic (near): redraws one part after a change (cat 5 exhaust manifold, 6 muffler, 7
 * differential); type < 0 erases it (the rectangle comes back from the backdrop DS:9718). */
void under_car_part_pic(s16 cat, s16 type)
{
    s16 x0, x1, y0, y1;
    s16 id = 0;
    if (cat == 5) {
        if (type >= 0) id = DSS((u16)(DS_UNDERCAR_PICS + 0 + 2 * parts_grade(type)));
        x0 = 0xB0; x1 = 0xF7; y0 = 0x3B; y1 = 0x4B;
    } else if (cat == 6) {
        if (type >= 0) id = DSS((u16)(DS_UNDERCAR_PICS + 4 + 2 * parts_grade(type)));
        x0 = 0x90; x1 = 0xA7; y0 = 0x47; y1 = 0x51;
    } else if (cat == 7) {
        if (type >= 0) id = DSS((u16)(DS_UNDERCAR_PICS + 8 + 2 * parts_grade(type)));
        x0 = 0x38; x1 = 0x4F; y0 = 0x2A; y1 = 0x3C;
    } else {
        return;                                   /* PORT: the original uses uninitialised locals for any other category */
    }
    s16 rr[4] = { (s16)(x1 - x0 + 1), (s16)(y1 - y0 + 1), x0, y0 };
    FarPtr pic = type < 0 ? bitmap_extract(ds_far(DS_uc_backdrop), rr, 2) : pic_get(id, 2);
    cursor_ctl(-3);
    bitmap_blit_at(g_back(), pic, (s16)(x0 + 0x18), (s16)(y0 + 0x15), 1);
    cursor_ctl(-1);
    /* sic: the original passes the y1 coordinate where the x1 / y1 of the copied corner belong, so the copy runs to the
     * bottom right of the screen (the rectangle is clipped by page_copy_rect) */
    page_copy_rect(g_back(), g_front(), (s16)(x0 + 0x18), (s16)(y0 + 0x15), (s16)(y1 + 0x127), (s16)(y1 + 0xA0));
    arena_pop(1);
}
