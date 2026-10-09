/* Opponent sprite and start props of race_render segment 0dcb (port/spec/race_render.md):
 *
 *   0dcb:a5aa start_props_load     0dcb:a746 road_draw_starter_slide   0dcb:a85c starter_draw_mirror
 *   0dcb:aab0 road_draw_start_tree 0dcb:af98 opponent_draw
 *
 * Distances in 1/64 segment (SR1 1/8); the Aqueduct (mode 8) branches call race_aq.c. */
#include "game/race_int.h"
#include "game/race.h"

#include "platform/platform.h"
#include "platform/video.h"

#define MODE DSS(DS_road_mode)

/* 0dcb:a5aa start_props_load */
void start_props_load(void)
{
    DSS(DS_start_light_pos) = 0;
    DSW(DS_start_active) = 1;
    s16 id = DSS(DS_start_panel_id);
    ds_far_wr(DS_start_panel_pic, pic_get(id, 1));
    if (id == 0x813 || id == 0x816) {
        Rect r = { 0x30, 0x52, 0, 0, 0xB8, 0x66 };
        drv_blit(ds_far(DS_start_panel_pic), ds_far(DS_page2_ptr), &r, 1);
        arena_pop_low(1);
    }
    if (id == 0x813) {
        for (s16 k = 0; k < 4; k++) ds_far_wr((u16)(DS_start_obj_sizes + 4 * k), pic_get((s16)(0x81E + k), -1));
        ds_far_wr(DS_start_pic2, pic_get(0x814, -1));
    } else if (id == 0x816) {
        static const s16 ids[4] = { 2000, 0x7E6, 0x809, 0x80A };
        for (s16 k = 0; k < 4; k++) ds_far_wr((u16)(DS_start_obj_sizes + 4 * k), pic_get(ids[k], -1));
        ds_far_wr(DS_start_pic2, pic_get(0x817, -1));
    } else if (id == 0x815) {
        s16 r[4] = { 0x80, 0x38, 0, 6 };
        ds_far_wr(DS_start_pic2, bitmap_extract(ds_far(DS_start_panel_pic), r, -1));
    }
}

/* 0dcb:a746 road_draw_starter_slide: the starter figure slides out of the view while the car moves */
void road_draw_starter_slide(void)
{
    if (!DSW(DS_start_active)) return;
    s16 d = (s16)((DSS(DS_start_light_pos) & 0xFFFC) * 2);
    if (d > 0x58) { DSW(DS_start_active) = 0; return; }
    Rect r = { (s16)(0x30 - d), 0x52, (s16)(d + 0xB8), 0x66, 8, 0x12 };
    if (r.w < 1) {
        r.w = (s16)(0x60 - d);
        r.sx = (s16)(d - 0x30);
    } else {
        drv_blit(ds_far(DS_page2_ptr), back_page(), &r, 1);
        r.dx = (s16)(r.dx + r.w);
        r.w = 0x30;
        r.sx = 0;
    }
    r.sy = 0;
    if (DSW(DS_light_latch)) r.sx = (s16)(r.sx + 0x30);
    if (DSW(DS_start_lights)) { DSW(DS_light_latch) = 1; r.sx = (s16)(r.sx + 0x30); }
    if (r.w > 0) drv_blit_masked(ds_far(DS_start_pic2), back_page(), &r, 1);
    DSS(DS_starter_seg) = DSS(DS_seg) < DSS(DS_opp_seg) ? (s16)(DSS(DS_seg) + 1) : DSS(DS_opp_seg);
}

/* 0dcb:a85c starter_draw_mirror */
void starter_draw_mirror(s16 x, s16 y, s16 row)
{
    if (DSW(DS_start_active) != 0 || row == 6) return;
    s16 lod = row < 3 ? (s16)(row >> 1) : (s16)(row - 1);
    if (lod > 3) lod = 3;
    FarPtr p = ds_far((u16)(DS_start_obj_sizes + 4 * lod));
    Rect r = { (s16)desc_w(p), (s16)desc_h(p), 0, 0, (s16)(x - (bitmap_visible_width(p) >> 1)), (s16)(y - desc_h(p) - 3) };
    if (y > 0) r.dy = (s16)(r.dy - (y - 0x2A));                 /* quirk (spec §5): pins the figure at y 0x27 - h */
    blit_clip_xy(&r);
    road_fillmode_end();
    bitmap_blit_masked_shift(p, back_page(), &r, 0);
    road_fillmode_begin();
}

/* 0dcb:aab0 road_draw_start_tree: drag-strip "Christmas tree" with the lamp states DS:A7A2 */
void road_draw_start_tree(s16 x, s16 ybase, s16 row)
{
    static Rect r;                                  /* PORT: uninitialised in the original for rows < 0 or > 4 */
    road_fillmode_end();
    if (row >= 0) {
        if (row < 2)                 { r.w = 0x38; r.h = 0x57; r.sx = 0x48; }
        else if (row == 2 || row == 3) { r.w = 0x28; r.h = 0x45; r.sx = 0x20; }
        else if (row == 4)           { r.w = 0x20; r.h = 0x3A; r.sx = 0; }
    }
    u16 l = DSW(DS_start_lights);
    if (l != 0) {
        FarPtr tree = ds_far(DS_start_panel_pic);
        if (l == 0x88 || l == 0x92) { DSS(DS_tree_lamp_left) = -1; DSS(DS_tree_lamp_right) = -1; }
        else {
            Rect c = { 0x80, 0x38, 0, 0, 0, 6 };
            drv_blit(ds_far(DS_start_pic2), tree, &c, 1);
        }
        if (l & 1) DSS(DS_tree_lamp_left) = 0;
        else if (l & 2) DSS(DS_tree_lamp_left) = 1;
        else if (l & 4) DSS(DS_tree_lamp_left) = 2;
        if (l & 8) DSS(DS_tree_lamp_right) = 0;
        else if (l & 0x10) DSS(DS_tree_lamp_right) = 1;
        else if (l & 0x20) { DSW(DS_light_latch) = 1; DSS(DS_tree_lamp_right) = 2; }
        if (DSS(DS_tree_lamp_left) >= 0)
            for (s16 i = 0; i < 3; i++) {
                s16 k = (s16)(DSS(DS_tree_lamp_left) * 3 + i);
                Rect m = { DSS((u16)(DS_tree_lamp_w + 2 * i)), DSS((u16)(DS_tree_lamp_h + 2 * k)), 0x80,
                           DSS((u16)(DS_tree_lamp_sy + 2 * k)), DSS((u16)(DS_tree_lamp_dx + 2 * i)), DSS((u16)(DS_tree_lamp_dy + 2 * k)) };
                drv_blit_masked(tree, tree, &m, 1);
            }
        if (DSS(DS_tree_lamp_right) >= 0)
            for (s16 i = 0; i < 3; i++) {
                s16 k = (s16)(DSS(DS_tree_lamp_right) * 3 + i);
                s16 w = DSS((u16)(DS_tree_lamp_w + 2 * i));
                Rect m = { w, DSS((u16)(DS_tree_lamp_h + 2 * k)), (s16)(w + 0x80), DSS((u16)(DS_tree_lamp_sy + 2 * k)),
                           (s16)(w + DSS((u16)(DS_tree_lamp_dx + 2 * i))), DSS((u16)(DS_tree_lamp_dy + 2 * k)) };
                drv_blit_masked(tree, tree, &m, 1);
            }
    }
    DSW(DS_start_lights) = 0;
    Rect d = r;
    s16 t = (s16)(ybase - d.h + 1);
    if (t < DSS(DS_clip_ymin)) { d.dy = DSS(DS_clip_ymin); d.sy = (s16)(DSS(DS_clip_ymin) - t); d.h = (s16)(d.h - d.sy); }
    else { d.sy = 0; d.dy = t; }
    s16 o = (s16)(d.h - DSS(DS_clip_ymax) + d.dy);
    if (o > 0) {
        if (o > 0x14) d.dy = (s16)(d.dy - 0x14);
        d.h = (s16)(DSS(DS_clip_ymax) - d.dy);
    }
    d.dx = (s16)-((d.w >> 1) - x);
    drv_blit_masked(ds_far(DS_start_panel_pic), back_page(), &d, 1);
    road_fillmode_begin();
}

/* 0dcb:af98 opponent_draw */
void opponent_draw(void)
{
    if (MODE == 2) return;
    s16 oseg = DSS(DS_opp_seg);
    s16 d = (s16)(shl16(sub16(oseg, DSS(DS_seg)), 6) - DSS(DS_substep) + DSS(DS_opp_seg_frac64));
    s16 ox = sub16(DSS(DS_opp_lat), DSS(DS_lat_last_opp_step));
    s16 wy = (s16)(DSS(DS_aq_saved_floor_h) + 2000);
    s16 v = 0;
    if (MODE == 8) {
        v = opp_bank_variant();
        s16 lat = DSS(DS_opp_lat), sxl = DSS(DS_aq_saved_xl), sxr = DSS(DS_aq_saved_xr);
        s16 th = (s16)(DSS(DS_aq_saved_top_h) - DSS(DS_aq_saved_floor_h) + 0x154);
        if (lat < sxl + 0x5C8) {
            s32 num = (s32)(s16)(sxl - lat + 0x5A0) * th;
            s16 den = (s16)(400 - DSS(DS_aq_wall_l_shift) + sxl);
            wy = (s16)(wy - (den ? (s16)(num / den) : 0));
        } else if (lat > sxr + 0xCF8) {
            s32 num = (s32)th * (s16)(lat - sxr - 0xCF8);
            s16 den = (s16)(400 - sxr + DSS(DS_aq_wall_r_shift));
            wy = (s16)(wy - (den ? (s16)(num / den) : 0));
        }
    }
    s16 p = DSS(DS_opp_sprite), s;
    if (d >= 0x3C0) s = -1;
    else if (d >= 0x80) { s = (s16)(((s16)(d + 0x40) >> 7) + 2); if (s > 8) s = 8; }
    else if (d < -0x38) s = (s16)(((s16)(0x80 - d) >> 7) + 8);
    else if (d <= 0x40) {
        if (abs16(ox) < 0xB4) s = p <= 8 ? 2 : 9;
        else s = d < 0 ? 9 : (ox < 0 ? 0 : 2);
    } else s = ox < -0xB4 ? 1 : 3;
    s16 nodraw = 0;
    DSW(DS_opp_contact) = (d >= 0 && d <= 0x40 && DSS(DS_opp_sprite_prev) < 9 && p < 9
                           && abs16(ox) < (DSS(DS_cpu_speed) < 4 ? 100 : 0xB4) && DSS(DS_step_per_frame) > DSS(DS_opp_seg_step)) ? 1 : 0;
    u16 res = PHW(PLAYER, PH_RESULT);
    if (DSW(DS_aq_bounce_flag) == 0 && DSS(DS_cpu_speed) >= 4) {
        if (res == 3 || DSW(DS_opp_contact)) s = 2;
        else if ((MODE & 0x10) && abs16(sub16(d, DSS(DS_opp_last_d))) < 0x28 && p == 9 && s >= 9) { s = 9; d = DSS(DS_opp_last_d); }
        else if (s != p) {
            if (s < 0)            { if (p == 8) return; s = (s16)(p + 1); }
            else if (s > 12)      { if (p == 12) return; s = (s16)(p + 1); }
            else if (s == DSS(DS_opp_sprite_prev)) {}
            else if ((s <= 8 && p > 9) || (s >= 9 && p > 0 && p <= 8 && ox < 0)) s = (s16)(p - 1);
            else if (s >= 9 && p > 0 && p <= 8) nodraw = 1;
            else if (p == 0) { if (s < 9) s = 1; else { s = 9; nodraw = 1; } }
            else if (p == 9) { if (s > 8) s = 10; else { s = ox < 0 ? 0 : 2; nodraw = 1; } }
            else if (p == 1) s = s >= 1 ? 3 : 0;
            else if (p == 3 && s == 1) {}
            else s = s > p ? (s16)(p + 1) : (s16)(p - 1);
        }
    }
    if (s < 0 || s > 12) return;                    /* PORT (spec §5): s = -1 would read DS:B7B2 as a sprite pointer */
    s8 adx = 0, ady = 0;
    if (MODE == 8) {
        if (s == 0 && v != 3) nodraw = 1;
        if (s == 1 && v != 3) s = 2;
        if (v == -1) {
            FarPtr pic = opp_sprite_aq(s, 3);
            DSW(DS_rot_into_scratch) = 1;
            pic = drv_rotate(pic, DSS(DS_opp_bank), -1, &adx, &ady, (s8)DSB((u16)(DS_anchor_x + 7 * s + 3)));
            DSW(DS_rot_into_scratch) = 0;
            ds_far_wr(DS_opp_pic, pic);
        } else if (s != p || v != DSS(DS_opp_variant)) ds_far_wr(DS_opp_pic, opp_sprite_aq(s, v));
    } else ds_far_wr(DS_opp_pic, ds_far((u16)(DS_opp_sprites + 4 * s)));
    DSW(DS_opp_alongside) = (s == 0 || (s == 2 && d <= 0x50)) ? 1 : 0;
    DSW(DS_opp_contact) = (DSW(DS_opp_contact) && s == 2) ? 1 : 0;
    if (v != -1) { adx = (s8)DSB((u16)(DS_anchor_x + 7 * s + v)); ady = (s8)DSB((u16)(DS_anchor_y + 7 * s + v)); }
    DSS(DS_opp_sprite_prev) = p;
    DSS(DS_opp_sprite) = s;
    DSS(DS_opp_variant) = v;
    DSS(DS_opp_last_d) = d;
    if (nodraw && !DSW(DS_opp_contact) && res != 3) return;
    FarPtr pic = ds_far(DS_opp_pic);
    s16 z = cdiv16(d, 8);
    if (s == 0) z = add16(z, 0x10);
    if (z < -2 && s == 2) z = add16(z, 8);
    s16 dy, dx;
    if (MODE == 8) { dy = DSS(DS_aq_dy); dx = DSS(0xBA8E); }
    else {
        dy = MODE == 4 ? 0 : sub16(DSS(DS_cur_height), TRKS(TRK_HGT, oseg));
        dx = sub16(DSS(DS_heading), TRKS(TRK_YAW, oseg));
    }
    s16 sx, sy;
    if (s < 9) {
        z = (s16)(abs16(z) + 4);
        sx = road_proj_x(z, DSS(DS_opp_lat), dx);
        sy = road_proj_y(z, wy, dy);
        if (s == 2 && sx > 0x13F && DSS(DS_opp_last_sx) < 200) sx = (s16)((sx + DSS(DS_opp_last_sx)) / 2);
    } else {
        z = (s16)-z;
        sx = road_proj_x_mirror(z, DSS(DS_opp_lat), (s16)(10 - dx));
        sy = road_proj_y_mirror(z, wy, (s16)-dy);
    }
    s16 row = (s16)((z + 1) >> 3);
    if ((s == 2 || s == 3) && sy > 100) sy = 100;
    Rect r;
    r.w = (s16)desc_w(pic);
    r.h = (s16)desc_h(pic);
    if (MODE == 8) {
        /* PORT (spec §5): the original tests sx + r.w with an uninitialised width; the port uses the picture's */
        if (DSW(DS_opp_contact) || res == 3) { sy = 0x68; if (sx + r.w < 0) sx = (s16)(0x1E - r.w); }
        rotate_about_centre(sx, sy, &sx, &sy);
        sx = (s16)(sx - adx);
        sy = (s16)(sy + ady);
    }
    DSS(DS_opp_last_sx) = sx;
    r.sx = 0; r.sy = 0;
    if (s == 0) {
        r.sx = (s16)(r.w - sx + 0x40);
        if (r.sx < 0) r.sx = 0;
        r.dx = 0;
        r.dy = sy;
    } else if (s == 1) {
        r.dx = (s16)(sx - r.w);
        r.dy = (s16)(sy - r.h + 0x11);
        if (res == 3 || DSW(DS_opp_contact)) {
            r.dy = (s16)(0x66 - r.h);
            if (r.dx + r.w < 0) r.dx = (s16)(0x1E - r.w);
        }
    } else {
        if (MODE == 8) { r.dx = (s16)(sx + DSS(DS_g_px_shift)); r.dy = (s16)(sy - r.h); }
        else {
            s16 base;
            if (s > 3 && s < 9) {
                r.dx = (s16)(sx - (r.w >> 1));
                if (row > 14) base = sy < DSS(DS_ground_top_y) ? DSS(DS_ground_top_y) : sy;
                else { s16 ry = DSS((u16)(DS_row_y + 2 * row)); base = sy < ry ? ry : sy; }
            } else { r.dx = (s16)(sx - (bitmap_visible_width(pic) >> 1)); base = sy; }
            r.dy = (s16)(base - r.h);
            if ((s == 2 || s == 3) && r.h + r.dy >= 0x65) r.dy = (s16)(0x65 - r.h);
        }
        if (res == 3 || DSW(DS_opp_contact)) {
            r.dy = (s16)(0x66 - r.h);
            if (r.dx + r.w < 0)      { r.dx = (s16)(0x14 - r.w); r.dy = (s16)(0x6B - r.h); }
            else if (r.dx >= 0x13F)  { r.dx = 0x12B;             r.dy = (s16)(0x69 - r.h); }
        }
    }
    s16 sw = 0;
    if (s >= 9) { if (DSS(DS_clip_xmin) == 0) { sw = 1; clip_set_mirror(); } }
    else        { if (DSS(DS_clip_xmin) != 0) { sw = 1; clip_set_view(); } }
    if (r.dx + r.w < DSS(DS_clip_xmin) || r.dx >= DSS(DS_clip_xmax)) return;     /* clip not restored (as SR1) */
    if (s >= 9) blit_clip_xy(&r); else blit_clip_y(&r);
    road_fillmode_end();
    if (s >= 9) bitmap_blit_masked_shift(pic, back_page(), &r, 0);
    else drv_blit_masked(pic, back_page(), &r, 1);
    road_fillmode_begin();
    if (sw) { if (DSS(DS_clip_xmin) == 0) clip_set_mirror(); else clip_set_view(); }
}
