/* Grudge Night (event 17): the framed tournament screens, port/spec/game_flow.md "Segment 2925" 0522-16c8.
 * Ground truth is the Ghidra decompile (port/decomp/sr2_ds.c 41528-42390), transcribed literally: the wipes
 * (panel_wipe) are cosmetic but are kept step for step. */
#include "game/events.h"
#include "game/ds_events.h"

#include "codeptr.h"
#include "game/flow.h"
#include "game/garage.h"
#include "game/garage_draw.h"
#include "game/records.h"
#include "game/ui.h"
#include "platform/platform.h"
#include "platform/video.h"

#include <stdio.h>
#include <stdlib.h>

/* ============================================================================ frame and colours */

/* 2925:0522 panel_frame_draw: a bevel cut from the border picture around the saved photo / face area `dst` */
void panel_frame_draw(FarPtr dst, FarPtr border)
{
    s16 dw = (s16)desc_w(dst), dh = (s16)desc_h(dst);
    s16 bw = (s16)desc_w(border), bh = (s16)desc_h(border);
    u16 half = (u16)(((((u16)dw >> 9) & 0xFF) << 8 | (((u16)dw >> 1) & 0xFF)) & 0xFFF8);
    Rect r;

    r.w = (s16)half; r.h = 4; r.sx = 0; r.sy = 0; r.dx = 0; r.dy = 0;      /* top, left part */
    drv_blit(border, dst, &r, 1);
    r.w = (s16)(dw - r.w);                                                  /* top, right part */
    r.sx = (s16)(bw - r.w);
    r.dx = (s16)half;
    drv_blit(border, dst, &r, 1);
    r.w = (s16)half; r.sx = 0; r.sy = (s16)(bh - 4); r.dx = 0; r.dy = (s16)(dh - 4);   /* bottom, left */
    drv_blit(border, dst, &r, 1);
    r.w = (s16)(dw - r.w);                                                  /* bottom, right */
    r.sx = (s16)(bw - r.w);
    r.dx = (s16)half;
    drv_blit(border, dst, &r, 1);

    if (bh < dh) {                                                          /* the sides, in three pieces each */
        r.w = 8;
        r.h = (s16)(bh - 8);
        r.sy = 4; r.sx = 0; r.dx = 0;
        r.dy = (s16)((dh - r.h) >> 1);
        drv_blit(border, dst, &r, 1);
        r.h = (s16)(r.dy - 4);
        r.dy = 4;
        drv_blit(border, dst, &r, 1);
        r.sy = (s16)((bh - r.h) - 4);
        r.dy = (s16)((dh - r.h) - 4);
        drv_blit(border, dst, &r, 1);
        r.sx = (s16)(bw - 8);
        r.dx = (s16)(dw - 8);
        r.h = (s16)(bh - 8);
        r.sy = 4;
        r.dy = (s16)((dh - r.h) >> 1);
        drv_blit(border, dst, &r, 1);
        r.h = (s16)(r.dy - 4);
        r.dy = 4;
        drv_blit(border, dst, &r, 1);
        r.sy = (s16)((bh - r.h) - 4);
        r.dy = (s16)((dh - r.h) - 4);
    } else {
        r.w = 8;
        r.h = (s16)(dh - 8);
        r.sx = 0;
        r.sy = (s16)((bh - r.h) >> 1);
        r.dx = 0;
        r.dy = 4;
        drv_blit(border, dst, &r, 1);
        r.sx = (s16)(bw - 8);
        r.dx = (s16)(dw - 8);
    }
    drv_blit(border, dst, &r, 1);
}

/* 2925:0812 bitmap_remap_colors: recolour a 4-plane bitmap in place (pixel == from1 -> to1, else from2 -> to2) */
void bitmap_remap_colors(FarPtr b, s16 from1, u16 to1, s16 from2, u16 to2)
{
    s16 h = (s16)desc_h(b);
    s16 stride = (s16)desc_stride(b);
    s16 size = (s16)(h * stride);
    u8 *p0 = far_mp(desc_planes(b));
    u8 *p1 = p0 + size, *p2 = p1 + size, *p3 = p2 + size;

    for (s16 n = 0; n < size; n++) {
        u8 a = p0[n], bb = p1[n], c = p2[n], d = p3[n];
        bool changed = false;
        for (u8 m = 0x80; m != 0; m >>= 1) {
            s16 col = (s16)(((c & m) ? 4 : 0) + ((d & m) ? 8 : 0) + ((bb & m) ? 2 : 0) + ((a & m) ? 1 : 0));
            u16 to;
            if (col == from1) to = to1;
            else if (col == from2) to = to2;
            else continue;
            a &= (u8)~m; bb &= (u8)~m; c &= (u8)~m;
            if (to & 1) a |= m;
            if (to & 2) bb |= m;
            if (to & 4) c |= m;
            d &= (u8)~m;
            if (to & 8) d |= m;
            changed = true;
        }
        if (changed) { p0[n] = a; p1[n] = bb; p2[n] = c; p3[n] = d; }
    }
}

/* The three text colours the screens set: fg 3 (4 on EGA / Tandy), background 0, fill 0. Saved: DS:AD28..AD2C. */
static void text_colours_enter(u8 save[5])
{
    for (int i = 0; i < 5; i++) save[i] = DSB((u16)(DS_g_text_fg + i));
    DSB(DS_g_text_fg) = (DSS(DS_driver_id) == -2 || DSS(DS_driver_id) == -6) ? 4 : 3;
    DSB(DS_g_text_fg + 2) = 0;                    /* DS:AD2A */
    DSB(DS_g_text_fg + 1) = 0;                    /* DS:AD29 */
}

static void text_colours_leave(const u8 save[5])
{
    for (int i = 0; i < 5; i++) DSB((u16)(DS_g_text_fg + i)) = save[i];
}

/* ========================================================================== build, split, wipe */

/* slot 15 (A0EA, 0dcb:b8c8 pic_shrink_planar, S5.3): shrink_pic(src, mask) -> new picture */
typedef FarPtr (*FnShrink)(FarPtr, u8);

/* 2925:0a0c tournament_screen_build: the framed panel (and with_photo the opponent's photo and face) */
void tournament_screen_build(s16 with_photo)
{
    u16 mirror = DSW(DS_g_mirror);
    FarPtr border = pic_get(with_photo ? 0x4B7 : 0x7F6, 2);     /* LIB2 #207 / LIB3 #38 */
    ds_far_wr(DS_tourn_pic_border, border);
    DSW(DS_g_mirror) = 0;
    screen_fill_rect(0, 0, 0x140, 200, 0);
    u16 fg = DSW(DS_g_text_fg), fill = DSW(DS_g_fill_col);
    u8 frame = DSB(DS_g_frame_style);
    DSW(DS_g_fill_col) = 0x0D09;
    DSB(DS_g_frame_style) = 1;
    draw_frame3(g_back(), 3, 3, 0xD2, 0x9E);
    DSW(DS_g_text_fg) = fg;
    DSW(DS_g_fill_col) = fill;
    DSB(DS_g_frame_style) = frame;
    ds_far_wr(DS_tourn_pic_panel, screen_save_rect(0, 0, 0xD8, 0xA4, far_make(0, 0)));
    if (with_photo) {
        FarPtr photo = screen_save_rect(8, 8, 200, 0x42, far_make(0, 0));
        ds_far_wr(DS_tourn_pic_photo, photo);
        panel_frame_draw(photo, border);
        s16 model = DSS(DS_opp_model);
        FarPtr img;
        car_compose(&img, model, (s16)(s8)DSB((u16)(DSW(DS_opp_rec) + 0xD)), 0,
                    (u16)(DS_TYRE_PICS + 8 * DSS(DS_garage_car_tyre)), 0, 1);
        img = ((FnShrink)codeptr_lookup(ds_far((u16)(DS_drv_vec + 4 * 15))))(img, DSB((u16)(DS_MODELS + MODEL_REC * model + 4)));
        if (DSS(DS_driver_id) == -2) bitmap_remap_colors(img, 6, 0xC, 7, 3);
        bitmap_blit_at(photo, img, (s16)(((s16)(desc_w(photo) - desc_w(img)) >> 1) & 0xFFF8),
                       (s16)((s16)(desc_h(photo) - desc_h(img)) >> 1), 1);
        arena_pop(1);
        arena_pop_low(1);
        FarPtr face = screen_save_rect(8, 8, 0x28, 0x23, far_make(0, 0));
        ds_far_wr(DS_tourn_pic_face, face);
        panel_frame_draw(face, border);
        FarPtr fpic = pic_get(DSS((u16)(DSW(DS_opp_rec) + 4)), 2);   /* the opponent's face */
        bitmap_blit_at(face, fpic, 8, 8, 1);
        arena_pop(1);
    }
    drv_copy_page(desc_planes(g_front()), desc_planes(g_back()));
    DSW(DS_g_mirror) = mirror;
}

/* 2925:0c4c clip_rect_split: pieces of the drawing rect r above / below the band clip (y0 = clip[1], y1 = clip[3]) */
s16 clip_rect_split(const s16 *clip, const s16 *r, s16 *out1, s16 *out2)
{
    s16 n = 0;
    s16 *o = out1;
    if (r[5] < clip[1]) {                                    /* the part above the band */
        for (int i = 0; i < 10; i++) o[i] = r[i];
        if (clip[1] < (s16)(r[1] + r[5])) o[1] = (s16)(clip[1] - r[5]);
        n = 1;
        o = out2;
    }
    if (clip[3] < (s16)(r[1] + r[5])) {                      /* the part below it */
        for (int i = 0; i < 10; i++) o[i] = r[i];
        s16 d = (s16)((clip[3] - r[5]) + 1);
        if (0 < d) {
            o[1] = (s16)(o[1] - d);
            o[3] = (s16)(o[3] + d);
            o[5] = (s16)(o[5] + d);
        }
        if (6 < o[1]) {
            o[3] = (s16)(o[3] + o[1] - 6);
            o[5] = (s16)(o[5] + o[1] - 6);
            o[1] = 6;
        }
        n++;
    }
    return n;
}

/* 2925:0d02 panel_wipe: a saved bitmap slides onto the screen 2 pixels per step. mode 0 from the bottom up, 1 from the
 * right, 2 from the top down (clipped around the band `clip`), 3 grows left to right (the photo); odd modes use the
 * pixel-shifted copies of the bitmap so that odd x positions are byte aligned. */
void panel_wipe(u16 mode, FarPtr bmp, s16 x, s16 y, const s16 *clip)
{
    FarPtr sh[4];                       /* [0] the bitmap, [1..3] shifted by 2, 4, 6 pixels (local_44..) */
    FarPtr cur = bmp;
    s16 idx = 0;                        /* local_74: shifted copy in use */
    s16 rw, rh, rx, ry;                 /* the saved background region (local_30, local_32, local_34, local_46) */
    s16 m[10] = { 0 };                  /* the moving rectangle {w, h, sx, sy, dx, dy} (local_2e..local_24) */
    s16 dxs = 0, dys = 0;               /* local_5c, local_72: step */
    s16 steps = 0;

    sh[0] = bmp;
    if (mode & 1) {
        for (s16 i = 1; i < 4; i++) {
            sh[i] = arena_bitmap_alloc((s16)((1 << DSW(DS_g_px_shift)) + desc_w(bmp)), (s16)desc_h(bmp), 0, 2);
            bitmap_make_shifted(bmp, (s16)(i << 1), sh[i]);
        }
    }
    if (mode == 0) {
        ry = y;
        rh = (s16)(200 - y);
        steps = (s16)((rh - 0x10) >> 1);
        dys = -2;
        rw = (s16)desc_w(bmp);
        m[0] = rw; m[1] = 2; m[2] = 0; m[3] = 0;
        rx = x;
        m[4] = x; m[5] = 0xB7;
    } else if (mode == 1) {
        idx = 3;
        cur = sh[3];
        rx = x;
        rw = (s16)(0x140 - x);
        dxs = -2;
        m[0] = (s16)desc_w(cur); m[1] = (s16)desc_h(cur); m[2] = 0;
        m[4] = 0x13E;
        steps = (s16)(rw >> 1);
        m[3] = 0; dys = 0; m[5] = y; ry = y; rh = m[1];
    } else if (mode == 2) {
        m[1] = (s16)desc_h(bmp);
        rh = (s16)(m[1] + y);
        steps = (s16)(rh >> 1);
        dys = 2;
        rw = (s16)desc_w(bmp);
        m[0] = rw; m[2] = 0; m[3] = 0;
        ry = 0;
        rx = x;
        m[4] = x;
        m[5] = (s16)-(m[1] - 2);
    } else {                            /* mode 3 */
        idx = 1;
        cur = sh[1];
        rw = (s16)(desc_w(cur) + x);
        dxs = 2;
        m[0] = 8; m[1] = (s16)desc_h(cur); m[2] = (s16)(desc_w(cur) - 8);
        rx = 0;
        m[4] = 0x30;
        steps = (s16)((rw - 0x38) >> 1);
        m[3] = 0; dys = 0; m[5] = y; ry = y; rh = m[1];
    }

    FarPtr bg = screen_save_rect(rx, ry, rw, rh, far_make(0, 0));
    Rect r = { rw, rh, 0, 0, rx, ry };
    s16 o1[10] = { 0 }, o2[10] = { 0 };
    s16 n = 0;

    for (s16 step = 0; step < steps; step++) {
        /* restore the strip the bitmap uncovers */
        if (0 < m[4] && (s16)(m[4] + m[0]) < 0x140 && 0 < m[5] && (s16)(m[5] + m[1]) < 200) {
            if (0 < dxs) {
                r.sx = (s16)(m[4] - dxs);
                r.dx = r.sx;
                r.w = 8;
            } else if (dxs < 0) {
                r.dx = (s16)(m[4] + m[0]);
                r.sx = (s16)((r.dx + rw) - 0x140);
                r.w = 8;
            } else if (0 < dys) {
                r.sy = (s16)(m[5] - dys);
                r.dy = r.sy;
                r.h = 2;
            } else if (dys < 0) {
                r.dy = (s16)(m[5] + m[1]);
                r.sy = (s16)((r.dy + rh) - 200);
                r.h = 2;
            }
            drv_blit(bg, g_back(), &r, 1);
        }
        /* draw the bitmap (the band `clip` is left alone) */
        if (clip == NULL) {
            drv_blit(cur, g_back(), (const Rect *)m, 1);
        } else {
            n = clip_rect_split(clip, m, o1, o2);
            if (1 < n) drv_blit(cur, g_back(), (const Rect *)o2, 1);
            if (0 < n) drv_blit(cur, g_back(), (const Rect *)o1, 1);
        }
        /* the sub-byte edges of a horizontally moving bitmap, from the saved background */
        if (dxs != 0) {
            Rect s;
            s.h = (s16)desc_h(cur);
            s.sy = 0;
            s.dy = m[5];
            u16 lo = (u16)(~DSW(DS_g_x_align) & (u16)m[4]);
            if (lo != 0 && 0 < m[4]) {
                s.dx = (s16)(DSW(DS_g_x_align) & (u16)m[4]);
                s.sx = (s16)(s.dx - rx);
                s.w = (s16)lo;
                drv_blit_shifted(bg, g_back(), &s);
            }
            if (0 < dxs) {
                s.w = 3;
                s.sx = 0x30;
                s.dx = 0x30;
                drv_blit_shifted(bg, g_back(), &s);
            }
            u16 u = (u16)(desc_w(sh[0]) + m[4]);
            if (dxs < 0 && (s16)u < 0x140) {
                s.w = (s16)((1 << DSW(DS_g_px_shift)) - lo);
                s.sx = (s16)(u - rx);
                s.dx = (s16)u;
                drv_blit_shifted(bg, g_back(), &s);
            }
        }
        /* show the changed part */
        s16 ays = (s16)abs(dys), axs = (s16)abs(dxs);
        if (clip == NULL) {
            s16 y1 = (s16)(ays + m[5] + m[1]), x1 = (s16)(axs + m[4] + m[0]);
            s16 y0 = (s16)(m[5] - ays), x0 = (s16)(m[4] - axs);
            if (y0 < 0) y0 = 0;
            if (x0 < 0) x0 = 0;
            page_copy_rect(g_back(), g_front(), x0, y0, (s16)(x1 - 1), (s16)(y1 - 1));
        } else {
            if (1 < n) {
                drv_blit(cur, g_back(), (const Rect *)o2, 1);
                s16 y0 = (s16)(o2[5] - ays);
                if (y0 < 0) y0 = 0;
                page_copy_rect(g_back(), g_front(), o2[4], y0, (s16)(o2[4] + o2[0] - 1), (s16)(ays + o2[1] + o2[5] - 1));
            }
            if (0 < n) {
                s16 y1 = (s16)(ays + o1[1] + o1[5]), x1 = (s16)(o1[4] + o1[0]);
                s16 y0 = (s16)(o1[5] - ays);
                if (y0 < 0) y0 = 0;
                page_copy_rect(g_back(), g_front(), o1[4], y0, (s16)(x1 - 1), (s16)(y1 - 1));
            }
        }
        /* next step */
        if (mode == 3) {
            if (m[2] == 0) m[4] = (s16)(m[4] + 2);
            idx++;
            if ((1 << (DSW(DS_g_px_shift) - 1)) - 1 < idx) {
                idx = 0;
                m[2] = (s16)(m[2] + (s16)-(1 << DSW(DS_g_px_shift)));
                if (m[2] < 0) m[2] = 0;
                m[0] = (s16)(m[0] + (1 << DSW(DS_g_px_shift)));
                if ((s16)desc_w(cur) < m[0]) m[0] = (s16)desc_w(cur);
            }
        } else if ((mode & 2) == 0) {
            if (--idx < 0) idx = 3;
        } else {
            if (3 < ++idx) idx = 0;
        }
        if (mode & 1) cur = sh[idx];
        if (mode == 0) {
            s16 h2 = (s16)(m[1] + 2);
            m[1] = (s16)desc_h(cur);
            if (h2 < (s16)desc_h(cur)) m[1] = h2;
        }
        if (mode != 3) m[4] = (s16)(m[4] + dxs);
        m[5] = (s16)(m[5] + dys);
    }
    arena_pop((mode & 1) ? 4 : 1);       /* PORT: the spec says 2 / 4; the code passes 1 (the saved region) or 4 (+ 3 shifted copies) */
}

/* ===================================================================================== text screens */

/* 2925:12f2 tournament_round_header: "ROUND n", the opponent with the dial-in difference, his car */
void tournament_round_header(void)
{
    u8 save[5];
    char buf[80];
    text_colours_enter(save);
    snprintf(buf, sizeof buf, "ROUND %d", DSS(DS_tourn_round) + 1);
    screen_text((s16)((0x140 - font_string_width(buf)) >> 1), 0x52, buf);
    s16 d = (s16)(DSS(DS_tourn_opp_dialin) - DSS(DS_tourn_dialin));
    snprintf(buf, sizeof buf, "%s (%+d.%d)", ds_str(DSW((u16)(DSW(DS_opp_rec) + 6))), d / 10, abs(d) % 10);
    screen_text(0x40, 0x60, buf);
    screen_text(0x48, 0xAD, ds_str(DSW((u16)(DSW(DS_opp_model_ptr) + 0xC))));
    text_colours_leave(save);
}

/* 2925:13d0 tournament_entrants_list: the nine opponents and the player, "Prepare to set your breakout time" */
void tournament_entrants_list(void)
{
    u8 save[5];
    char buf[80];
    s16 x = 0x38, y = 0x61;
    text_colours_enter(save);
    s16 n = 1;
    for (s16 i = 0; i < 9; i++, n++) {
        snprintf(buf, sizeof buf, "%2d %s", n, ds_str(DSW((u16)(DS_people + PERSON_REC * TOURN_ID(i) + 6))));
        screen_text(x, y, buf);
        if (i == 4) { x = 0x8C; y = 0x61; }
        else y = (s16)(y + 10);
    }
    snprintf(buf, sizeof buf, "%2d %s", n, ds_str(DS_player_name));
    screen_text(x, y, buf);
    screen_text(0x40, (s16)(y + 0x14), "Prepare to set your breakout time");
    text_colours_leave(save);
}

/* 2925:14b6 tournament_standings: FINAL STANDINGS, four places with the prizes */
void tournament_standings(const s16 *ids)
{
    u8 save[5];
    char buf[80];
    text_colours_enter(save);
    screen_text(0x38, 0x52, "FINAL STANDINGS:");
    s16 y = 0x5E;
    for (s16 r = 0; r < 4; r++) {
        s16 id = ids[r];
        u16 name;
        s16 model;
        if (id < 0) {
            name = DS_player_name;
            model = DSS((u16)(DSW(DS_cur_car) + CR_MODEL));
        } else {
            name = DSW((u16)(DS_people + PERSON_REC * id + 6));
            model = DSS((u16)(DS_people + PERSON_REC * id + 8));
        }
        u16 car = DSW((u16)(DS_MODELS + MODEL_REC * model + 0xC));
        snprintf(buf, sizeof buf, "%2d.", r + 1);
        drv_draw_text(g_back(), 0x38, y, buf);
        drv_draw_text(g_back(), 0x4C, y, ds_str(name));
        if (r < 3) {
            snprintf(buf, sizeof buf, "$%3d", DSW((u16)(DS_tourn_prizes + 2 * r)));
            drv_draw_text(g_back(), 0xCE, y, buf);
        }
        drv_draw_text(g_back(), 0x56, (s16)(y + 10), ds_str(car));
        y = (s16)(y + 0x14);
    }
    page_copy_rect(g_back(), g_front(), 0x38, 0x5E, 0xFF, y);
    screen_text(0x40, (s16)(y + 2), "Congratulations!");
    text_colours_leave(save);
}

/* 2925:1642 tournament_intro_screen */
void tournament_intro_screen(void)
{
    cursor_ctl(1);
    tournament_screen_build(0);
    screen_put_bitmap(ds_far(DS_tourn_pic_panel), 0x30, 0x18);
    screen_put_bitmap(ds_far(DS_tourn_pic_border), 0x18, 0x1E);
    page_copy_rect(g_back(), g_front(), 0x18, 0x18, 0x11F, 0xBB);
    tournament_entrants_list();
    cursor_ctl(-2);
    wait_ticks(2000);
}

/* 2925:16c8 tournament_round_screen */
void tournament_round_screen(void)
{
    cursor_ctl(-4);
    tournament_screen_build(1);
    panel_wipe(1, ds_far(DS_tourn_pic_border), 0x18, 0x1E, NULL);
    panel_wipe(2, ds_far(DS_tourn_pic_panel), 0x30, 0x18, (const s16 *)mp(DGROUP, DS_tourn_clip));
    opp_palette_set(DSS(DS_garage_car_colour));
    panel_wipe(3, ds_far(DS_tourn_pic_photo), 0x38, 0x69, NULL);
    panel_wipe(0, ds_far(DS_tourn_pic_face), 0xD0, 0x52, NULL);
    arena_pop(4);
    tournament_round_header();
    cursor_ctl(-2);
    wait_ticks(1000);
}
