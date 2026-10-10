/* The Aqueduct race (road mode 8) of race_render (port/spec/race_render.md §1.6, §1.7, 0dcb:18ec-1cd2, 8288-c342):
 * the concrete channel renderer with the roll rotation, the wall bounce, the obstacle probe, the rotated opponent
 * sprites, the night sky and the lane AI (0001:d2a8).
 *
 *   0001:d2a8 aq_lane_target      0dcb:18ec aq_project_front   0dcb:1b02 aq_project_mirror  0dcb:1cd2 aq_wall_collision
 *   0dcb:8482 aq_draw_row_shape   0dcb:84c4 aq_draw_night_sky  0dcb:85a0 aq_draw_mirror_stars
 *   0dcb:862a poly_straddles_bottom                            0dcb:8674 poly_bottom_hit_bits
 *   0dcb:86d2 anchor_scale_rows   0dcb:8774 rot_shrink_max_size 0dcb:8840 race_load_graphics_rot
 *   0dcb:8b16 aq_draw_sky         0dcb:8b5c aq_draw_box        0dcb:8ece aq_wall_opening_quad
 *   0dcb:905c aq_draw_scene       0dcb:9ce8 aq_draw_mirror     0dcb:9d9c aq_climb_left      0dcb:9e4e aq_climb_right
 *   0dcb:9f08 aq_trough_bump      0dcb:9f9a aq_roll_update     0dcb:a094 aq_draw_front      0dcb:a1c0 aq_wall_bounce
 *   0dcb:a2d8 aq_collide_probe    0dcb:a92a starter_draw_mirror_aq                          0dcb:ad40 opp_sprite_aq
 *   0dcb:ae9c opp_bank_variant    0dcb:bb22 rot_alloc          0dcb:bd12 pic_rotate_planar (slot 16)
 *   1a21:9b96 heap_free           1a21:9bac bitmap_bytes       (platform helpers only used here)
 *
 * Row arrays (15 words, row 0 nearest) and the reversed rotated arrays live at their DGROUP addresses; the drawers
 * write physics state (player x DS:A564, heading DS:A56A, speed DS:A100, hit bits DS:A7D2) exactly as the original.
 * The CGA / Tandy rotators (c09a, c342) are not ported (VGA only). */
#include "game/race_int.h"
#include "game/race.h"

#include "platform/platform.h"
#include "platform/video.h"

#define MODE  DSS(DS_road_mode)
#define NROWS DSS(DS_aq_rows)
#define XMIN  DSS(DS_clip_xmin)
#define YMAX  DSS(DS_clip_ymax)
#define XMAX  DSS(DS_clip_xmax)
#define ROLL  DSS(DS_rot_enable)                  /* 9E70: roll angle in degrees (0 = level) */

/* DGROUP without a symbols.h name (race_render.md §1.6, §2) */
#define DS_aq_dx             0xBA8E   /* heading used as dx by the projection */
#define DS_aq_car_xr         0x9A58   /* car right edge world x */
#define DS_aq_car_sxr        0xA55A   /* car right edge screen x (near row) */
#define DS_aq_trough_near_xr 0xB872
#define DS_aq_trough_far_xr  0xB8D8
#define DS_aq_trough_far_y   0xB162
#define DS_aq_roll_prev      0xB868
#define DS_aq_pitch_prev     0x9E68
#define DS_aq_side_l         0x9758   /* car left edge vs trough: -1 / 0 / 1 */
#define DS_aq_side_r         0x9868
#define DS_aq_side_l_prev    0xA7E8
#define DS_aq_side_r_prev    0xAAF2
#define DS_aq_7fcc           0x7FCC   /* heading creep on the right wall top (set by race init) */
#define DS_star_colour_a     0xAD48   /* plane masks of the stars (gfx_init) */
#define DS_star_colour_b     0xAD4A
#define DS_star_colour_c     0xAD54
#define DS_rot_scratch_pic   0xAB24   /* Pic far* reused as the rotation scratch when DS:80A0 (= DS_start_pic2) */

/* raw row arrays */
#define A_Y    DS_row_y        /* 9EF0 floor y */
#define A_XL   DS_row_xl       /* 9A5C floor left x */
#define A_XR   DS_row_xr       /* A2E2 floor right x */
#define A_XC   DS_row_ybank_l  /* A5A8 channel "centre" x */
#define A_YT   DS_row_yeye     /* A300 wall-top y */
#define A_XTL  DS_row_xol      /* AB2E wall-top left x */
#define A_XTR  DS_row_xor      /* AB5E wall-top right x */
#define RW(a, i) DSS((u16)((a) + 2 * (i)))
/* rotated arrays (k = 0 farthest) */
#define R_LX   DS_aq_rot_floor_lx  /* 9732 */
#define R_LY   0x9760
#define R_RX   DS_aq_rot_floor_rx  /* B170 */
#define R_RY   0xB1A0
#define R_TLX  DS_aq_rot_top_lx    /* 9BC6 */
#define R_TLY  0x9CFC
#define R_TRX  DS_aq_rot_top_rx    /* 9DCE */
#define R_TRY  0x9F0E

typedef struct { s16 x, y; } Pt;
static inline Pt rp(u16 ax, u16 ay, s16 k) { Pt p = { RW(ax, k), RW(ay, k) }; return p; }
static inline Pt rot(s16 x, s16 y) { Pt p; rotate_about_centre(x, y, &p.x, &p.y); return p; }
static inline void q4(s16 *q, Pt a, Pt b, Pt c, Pt d)
{
    q[0] = a.x; q[1] = a.y; q[2] = b.x; q[3] = b.y; q[4] = c.x; q[5] = c.y; q[6] = d.x; q[7] = d.y;
}
/* poly(q): if (poly_clip(q, 4, o, &n)) A0F6(o, n, fill, edge) */
static void poly4(const s16 *q, s16 fill, s16 edge)
{
    s16 o[128], n;
    if (clip_polygon(q, 4, o, &n)) poly_fill_pts(o, n, fill, edge);
}
/* segment flags word of row k: far word [DS:96F6 - 2k] */
static u16 aq_flag(s16 k)
{
    FarPtr p = ds_far(DS_aq_flag_ptr);
    return rd16(p.seg, (u16)(p.off - 2 * k));
}
static inline bool user_drives(void) { return DSW(DS_demo) == 0 && DSW(DS_auto_drive) == 0; }

/* ================================================================================ lane AI, collisions */

/* 0001:d2a8 aq_lane_target: Aqueduct lane choice of the autopilot and the opponent; returns a lateral step */
s16 aq_lane_target(s16 x, s16 dx, s16 seg, s16 ox, s16 oseg)
{
    s16 target = (s16)(dx + x);
    s16 a = (s16)(DSS(DS_aq_wall_l_shift) + 0x438), b = (s16)(DSS(DS_aq_saved_xl) + 0x5C8);
    s16 c = (s16)(DSS(DS_aq_wall_r_shift) + 0xE88), e = (s16)(DSS(DS_aq_saved_xr) + 0xCF8);
    s16 lane[5];
    if (DSW(DS_demo) || DSW(DS_auto_drive)) {
        static const s16 fixed[5] = { 0x514, 0x6A4, 0x884, 0xA3C, 0xBCC };
        for (int i = 0; i < 5; i++) lane[i] = fixed[i];
    } else {
        lane[0] = (s16)(cdiv16((s16)(b - a), 3) + a);
        lane[1] = (s16)(b + q_shr((s16)(e - b), 2));
        s16 m2 = cdiv16((s16)(b + e), 2);
        lane[2] = (s16)(m2 - 0x1E);
        lane[3] = cdiv16((s16)(m2 + e), 2);
        lane[4] = cdiv16((s16)(c + e), 2);
    }
    s16 mid = (s16)((s16)(b + e) >> 1);
    s16 o_rel = (s16)(ox - mid), p_rel = (s16)(x - mid), d = (s16)(seg - oseg);
    s16 mode = d > 2 ? 1 : d < 0 ? 8 : (ox > x ? 2 : 4);
    s16 i, idx = 0;
    bool found = false;
    for (i = 0; i < 8; i++) {
        u8 f = TRKB(TRK_AQ, seg + i);
        if (f & 0x10) { idx = mode == 2 ? 0 : mode == 4 ? 4 : (p_rel < 0 ? 0 : 4); found = true; break; }
        if (f & 0x80) { idx = mode == 1 ? 2 : mode == 2 ? 0 : mode == 4 ? 4 : (p_rel < 0 ? 0 : 4); found = true; break; }
    }
    if (found) target = lane[idx];
    else if (mode == 1) target = lane[o_rel < 0 ? 1 : 3];
    else if (mode == 8) target = lane[o_rel < 0 ? 3 : 1];
    if (i > 0) i--;
    if (i > 0) i--;
    return cdiv16(cdiv16((s16)((target - x) * 6), (s16)(i + 1)), 6);
}

/* 0dcb:1cd2 aq_wall_collision */
s16 aq_wall_collision(void)
{
    DSW(DS_on_shoulder) = 0;
    if (DSS(DS_aq_car_xl) < 0x439) { DSS(DS_aq_wall_hit) = 1; return 1; }
    if (DSS(DS_aq_car_xr) >= 0xDED) { DSS(DS_aq_wall_hit) = -1; return 1; }
    return DSW(DS_aq_hit_bits) ? 1 : 0;
}

/* 0dcb:ae9c opp_bank_variant: opponent bank angle DS:A55C and the sprite variant (-1 = a rotated bitmap) */
s16 opp_bank_variant(void)
{
    s16 lat = DSS(DS_opp_lat), sxl = DSS(DS_aq_saved_xl), sxr = DSS(DS_aq_saved_xr);
    s16 e;
    if (lat < (s16)(sxl + 0x5FA)) e = (s16)(sxl + 0x5FA - lat);
    else if (lat > (s16)(sxr + 0xCC6)) e = (s16)(lat - sxr - 0xCC6);
    else e = 0;
    s16 bank = e >= 100 ? 0x2D : e < 1 ? 0 : cdiv16((s16)(e * 0x2D), 100);
    if ((s16)(lat - ((s16)(sxr + sxl + 0x12C0) >> 1)) < 0) bank = (s16)-bank;
    s16 roll = DSS(DS_rot_enable);
    bank = (s16)(bank + roll);
    if (roll < 0 && roll > -0x5A) bank = (s16)(bank + (roll >> 2));
    if (bank > 0x167) bank = (s16)(bank - 0x168);
    if (bank < -0x167) bank = (s16)(bank + 0x168);
    DSS(DS_opp_bank) = bank;
    s16 a = abs16(bank);
    s16 v = a >= 0x38 ? -1 : a >= 0x25 ? 0 : a >= 0x17 ? 1 : a >= 0xD ? 2 : 3;
    if (bank > 0 && v != -1) v = (s16)(6 - v);
    return v;
}

/* ================================================================================ projection */

/* 0dcb:18ec aq_project_front: the 15 front rows; inputs = the channel deformation of the previous a094 */
void aq_project_front(void)
{
    DSS(DS_aq_dy) = sub16(DSS(DS_cur_height), DSS(DS_aq_pitch));
    DSS(DS_aq_dx) = DSS(DS_heading);
    s16 dy = DSS(DS_aq_dy), dx = DSS(DS_aq_dx);
    s16 xl = DSS(DS_aq_def_xl), xr = DSS(DS_aq_def_xr);
    s16 mid = (s16)((s16)(xr + xl + 0x12C0) >> 1);
    DSS(DS_aq_trough_near_xl) = road_proj_x(4, (s16)(mid + xl - 0x28), dx);
    DSS(DS_aq_trough_near_xr) = road_proj_x(4, (s16)(xr + mid + 0x28), DSS(DS_aq_dx));
    DSS(DS_aq_car_xl) = sub16(DSS(DS_cam_x), 0x6E);
    DSS(DS_aq_car_xr) = add16(DSS(DS_cam_x), 0x208);
    DSS(DS_aq_car_sxl) = road_proj_x(4, DSS(DS_aq_car_xl), DSS(DS_aq_dx));
    DSS(DS_aq_car_sxr) = road_proj_x(4, DSS(DS_aq_car_xr), DSS(DS_aq_dx));
    s16 f = q_shr(DSS(DS_substep), 3), z = 4;
    for (s16 i = 0; i < 15; i++) {                 /* z: 4, 12-f, +8 ...; row 12 adds f -> rows 12..14 = 100,108,116 */
        if (i == 1) z = (s16)(12 - f);
        if (i == 12) z = (s16)(z + f);
        dy = DSS(DS_aq_dy); dx = DSS(DS_aq_dx);
        RW(A_Y, i)   = road_proj_y(z, (s16)(DSS(DS_aq_def_floor_h) + 2000), dy);
        RW(A_XL, i)  = road_proj_x(z, (s16)(DSS(DS_aq_def_xl) + 0x5C8), dx);
        RW(A_XR, i)  = road_proj_x(z, (s16)(DSS(DS_aq_def_xr) + 0xCF8), dx);
        RW(A_XC, i)  = road_proj_x(z, (s16)(mid + DSS(DS_aq_def_xr)), dx);
        RW(A_YT, i)  = road_proj_y(z, (s16)(DSS(DS_aq_def_top_h) + 0x67C), dy);
        RW(A_XTL, i) = road_proj_x(z, (s16)(DSS(DS_aq_def_top_xl) + 0x438), dx);
        RW(A_XTR, i) = road_proj_x(z, (s16)(DSS(DS_aq_def_top_xr) + 0xE88), dx);
        z = (s16)(z + 8);
    }
    z = (s16)(z - 8);
    DSS(DS_aq_trough_far_xl) = road_proj_x(z, (s16)(mid + DSS(DS_aq_def_xl) - 0x28), DSS(DS_aq_dx));
    DSS(DS_aq_trough_far_xr) = road_proj_x(z, (s16)(mid + DSS(DS_aq_def_xr) + 0x28), DSS(DS_aq_dx));
    DSS(DS_aq_trough_near_y) = RW(A_Y, 0);
    DSS(DS_aq_trough_far_y) = RW(A_Y, 14);
}

/* 0dcb:1b02 aq_project_mirror: 8 mirror rows from the values a094 saved (AA9C, B8BE, BB8A, B8E6) */
void aq_project_mirror(void)
{
    s16 dy = (s16)-DSS(DS_aq_dy), dx = (s16)-DSS(DS_aq_dx);
    s16 xl = DSS(DS_aq_saved_xl), xr = DSS(DS_aq_saved_xr);
    s16 mid = (s16)((s16)(xr + xl + 0x12C0) >> 1);
    DSS(DS_aq_trough_near_xl) = road_proj_x_mirror(2, (s16)(mid + xl - 0x28), dx);
    DSS(DS_aq_trough_near_xr) = road_proj_x_mirror(2, (s16)(xr + mid + 0x28), dx);
    s16 f = q_shr(DSS(DS_substep), 3), z = 2;
    for (s16 i = 0; i < 8; i++) {                  /* z: 2, 8+f, +8 ...; row 5 subtracts f -> rows 5..7 = 40,48,56 */
        if (i == 1) z = (s16)(f + 8);
        if (i == 5) z = (s16)(z - f);
        RW(A_Y, i)   = road_proj_y_mirror(z, (s16)(DSS(DS_aq_saved_floor_h) + 2000), dy);
        RW(A_XL, i)  = road_proj_x_mirror(z, (s16)(DSS(DS_aq_saved_xl) + 0x5C8), dx);
        RW(A_XR, i)  = road_proj_x_mirror(z, (s16)(DSS(DS_aq_saved_xr) + 0xCF8), dx);
        RW(A_XC, i)  = road_proj_x_mirror(z, (s16)(mid + DSS(DS_aq_saved_xr)), DSS(DS_aq_dx));   /* quirk: + not - */
        RW(A_YT, i)  = road_proj_y_mirror(z, (s16)(DSS(DS_aq_saved_top_h) + 0x67C), dy);
        RW(A_XTL, i) = road_proj_x_mirror(z, (s16)(DSS(DS_aq_wall_l_shift) + 0x438), dx);
        RW(A_XTR, i) = road_proj_x_mirror(z, (s16)(DSS(DS_aq_wall_r_shift) + 0xE88), dx);
        z = (s16)(z + 8);
    }
    z = (s16)(z - 8);
    DSS(DS_aq_trough_far_xl) = road_proj_x_mirror(z, (s16)(mid + DSS(DS_aq_saved_xl) - 0x28), dx);
    DSS(DS_aq_trough_far_xr) = road_proj_x_mirror(z, (s16)(mid + DSS(DS_aq_saved_xr) + 0x28), dx);
    DSS(DS_aq_trough_near_y) = RW(A_Y, 0);
    DSS(DS_aq_trough_far_y) = RW(A_Y, 7);
}

/* ================================================================================ sky */

/* 0dcb:8482 aq_draw_row_shape: group of SR2_SHP set 23 + set */
static void aq_draw_row_shape(s16 x, s16 y, s16 row, s16 set)
{
    s16 group = NROWS == 14 ? (s16)((s16)(row + 1) >> 1) : (s16)((row >> 1) + 6);
    shp_b_draw(x, y, group, ds_far((u16)(DS_shp_b_ptrs + 4 * set)));
}

/* 0dcb:84c4 aq_draw_night_sky: stars and three sky shapes (front view) */
static void aq_draw_night_sky(void)
{
    s16 sc = DSS(DS_aq_star_scroll);
    drv_plot_points(desc_planes(back_page()), (s16)(sc >> 1), 0x7F2E, 18, DSS(DS_star_colour_a));
    drv_plot_points(desc_planes(back_page()), (s16)(sc >> 1), 0x7F76, 6, DSS(DS_star_colour_b));
    shp_b_draw((s16)(DSS(DS_aq_star_scroll) + 0x5C), 0x20, 0, ds_far(DS_shp_b_ptrs));
    shp_b_draw((s16)(DSS(DS_aq_star_scroll) + 0xD2), 0x2D, 0, ds_far(DS_shp_b_ptrs + 4));
    shp_b_draw((s16)(DSS(DS_aq_star_scroll) + 0xB4), 0x16, 0, ds_far(DS_shp_b_ptrs + 4));
}

/* 0dcb:85a0 aq_draw_mirror_stars (from the mirror backdrop 57a6 in mode 8) */
void aq_mirror_stars(void)
{
    s16 xo = (s16)-(s16)(DSS(DS_aq_star_scroll) >> 2);
    drv_plot_points(desc_planes(back_page()), xo, 0x7F8E, 9, DSS(DS_star_colour_a));
    xo = (s16)-(s16)(DSS(DS_aq_star_scroll) >> 2);
    drv_plot_points(desc_planes(back_page()), xo, 0x7FB2, 3, DSS(DS_star_colour_c));
}

/* 0dcb:8b16 aq_draw_sky */
static void aq_draw_sky(void)
{
    s16 save = YMAX;
    if (ROLL == 0) YMAX = DSS(DS_rot_cy);           /* no roll: sky only down to the far floor y */
    drv_fill_rows(0x12, 1);
    YMAX = save;
    DSS(DS_aq_star_scroll) = cmod16(shl16(DSS(DS_heading), 1), 0x140);
    aq_draw_night_sky();
    DSS(DS_ground_top_y) = 0;
    DSS(DS_top_band_row) = 0xE;
}

/* ================================================================================ hit tests, boxes, openings */

/* 0dcb:862a poly_straddles_bottom */
static s16 poly_straddles_bottom(s16 x, const s16 *pts, s16 n)
{
    s16 l = 0, r = 0;
    for (s16 i = 0; i < n; i++) {
        if (pts[2 * i + 1] != 0x65) continue;
        if (pts[2 * i] < x) l = 1;
        else if (pts[2 * i] > x) r = 1;
    }
    return (s16)(r & l);
}

/* 0dcb:8674 poly_bottom_hit_bits: obstacle bits for DS:A7D2 from the points on the bottom view row */
static u16 poly_bottom_hit_bits(const s16 *pts, s16 n)
{
    s16 nl = 0, nr = 0, any = 0;
    for (s16 i = 0; i < n; i++) {
        if (pts[2 * i + 1] != 0x65) continue;
        if (pts[2 * i] < 0x50) nl++;
        if (pts[2 * i] > 0x13F) nr++;
        any = 1;
    }
    u16 r = 0;
    if (nl & 1) r = 0x20;
    if (nr & 1) r |= 0x40;
    if (any && nr == nl) r |= 0x60;
    return r;
}

static inline s16 thin(Pt a, Pt b) { return (abs16(sub16(a.x, b.x)) <= 2 || abs16(sub16(a.y, b.y)) <= 2) ? 2 : 0; }

/* 0dcb:8b5c aq_draw_box: P1/P2 far bottom L/R, P3/P4 far top, P5/P6 near bottom, P7/P8 near top (unrotated) */
static void aq_draw_box(const s16 *raw, s16 solid)
{
    Pt p[9];
    for (int k = 0; k < 8; k++) p[k + 1] = rot(raw[2 * k], raw[2 * k + 1]);
    s16 q[8], o[128], n;
    if (p[5].x > p[1].x) {                          /* left side */
        q4(q, p[1], p[3], p[7], p[5]);
        s16 e = thin(p[1], p[7]);
        if (clip_polygon(q, 4, o, &n)) poly_fill_pts(o, n, 2, e);
    }
    if (p[6].x < p[2].x) {                          /* right side */
        q4(q, p[2], p[4], p[8], p[6]);
        s16 e = thin(p[2], p[8]);
        if (clip_polygon(q, 4, o, &n)) poly_fill_pts(o, n, 2, e);
    }
    if (p[3].y < p[7].y) {                          /* top */
        q4(q, p[3], p[4], p[8], p[7]);
        s16 e = thin(p[3], p[8]);
        if (clip_polygon(q, 4, o, &n)) {
            if (solid == 0 && NROWS == 14) DSW(DS_aq_hit_bits) |= poly_bottom_hit_bits(o, n);
            poly_fill_pts(o, n, 5, e);
        }
    }
    q4(q, p[6], p[5], p[7], p[8]);                  /* near face */
    s16 e = thin(p[6], p[7]);
    if (clip_polygon(q, 4, o, &n)) poly_fill_pts(o, n, 2, e);
    if (solid == 1) {                               /* footprint, not drawn */
        q4(q, p[1], p[2], p[6], p[5]);
        if (clip_polygon(q, 4, o, &n) && NROWS == 14) DSW(DS_aq_hit_bits) |= poly_bottom_hit_bits(o, n);
    }
}

/* 0dcb:8ece aq_wall_opening_quad: q = {bottom k, top k, top k+1, bottom k+1}; 0 = none */
static s16 aq_wall_opening_quad(s16 k, u16 flags, s16 *q, s16 enable)
{
    if (!enable) return 0;
    u16 xb, yb, xt, yt;
    bool any = false;
    if (flags & 0x104) { xb = R_LX; yb = R_LY; xt = R_TLX; yt = R_TLY; any = true; }
    if (flags & 0x208) { xb = R_RX; yb = R_RY; xt = R_TRX; yt = R_TRY; any = true; }
    if (!any) return 0;
    s16 b0 = RW(xb, k), b1 = RW(xb, k + 1), t0 = RW(xt, k), t1 = RW(xt, k + 1);
    s16 by0 = RW(yb, k), by1 = RW(yb, k + 1), ty0 = RW(yt, k), ty1 = RW(yt, k + 1);
    s16 d0 = q_shr(sub16(b0, t0), 2), d1 = q_shr(sub16(b1, t1), 2);
    s16 q0x, q0y, q3x, q3y;
    if (flags & 0x0C) {
        q0x = sub16(b0, d0); q0y = line_y_at_x(b0, by0, t0, ty0, q0x);
        q3x = sub16(b1, d1); q3y = line_y_at_x(b1, by1, t1, ty1, q3x);
    } else {
        q0x = b0; q0y = by0;
        q3x = b1; q3y = by1;
    }
    s16 q1x = add16(t0, d0), q1y = line_y_at_x(b0, by0, t0, ty0, q1x);
    s16 q2x = add16(t1, d1), q2y = line_y_at_x(b1, by1, t1, ty1, q2x);
    q[0] = q0x; q[1] = q0y; q[2] = q1x; q[3] = q1y; q[4] = q2x; q[5] = q2y; q[6] = q3x; q[7] = q3y;
    return 1;
}

/* 0dcb:a92a starter_draw_mirror_aq: the starter figure in the mirror, on the trough's left line, rotated with the roll */
void starter_draw_mirror_aq(s16 row)
{
    if (DSW(DS_start_active) != 0 || row == 6) return;
    s16 sw = 0;
    if (XMIN == 0) { sw = 1; clip_set_mirror(); }
    s16 lod = row < 3 ? (s16)(row >> 1) : (s16)(row - 1);
    if (lod > 3) lod = 3;
    FarPtr p = ds_far((u16)(DS_start_obj_sizes + 4 * lod));
    s16 y = RW(A_Y, row);
    s16 x = (s16)(line_x_at_y(DSS(DS_aq_trough_far_xl), DSS(DS_aq_trough_far_y), DSS(DS_aq_trough_near_xl),
                              DSS(DS_aq_trough_near_y), y) + 1);
    s16 hx = (s16)(bitmap_visible_width(p) >> 1), oy = 0;
    if (DSS(DS_aq_roll_abs) > 5) {
        s8 ox8, oy8;
        DSW(DS_rot_into_scratch) = 1;
        p = drv_rotate(p, ROLL, -1, &ox8, &oy8, hx);
        DSW(DS_rot_into_scratch) = 0;
        hx = ox8;
        oy = oy8;
    }
    if (ROLL != 0) rotate_about_centre(x, y, &x, &y);
    Rect r = { (s16)desc_w(p), (s16)desc_h(p), 0, 0, (s16)(x - hx), (s16)(y - (s16)desc_h(p) + oy) };
    blit_clip_xy(&r);
    road_fillmode_end();
    bitmap_blit_masked_shift(p, back_page(), &r, 0);
    road_fillmode_begin();
    if (sw) clip_set_view();
}

/* ================================================================================ scene */

/* 0dcb:905c aq_draw_scene: channel, trough, features and the opponent of the current view (N = DS:B194) */
static void aq_draw_scene(void)
{
    s16 N = NROWS;
    s16 q[8], o[128], n;
    Pt Lf = rp(R_LX, R_LY, 0), Rf = rp(R_RX, R_RY, 0), TLf = rp(R_TLX, R_TLY, 0), TRf = rp(R_TRX, R_TRY, 0);
    Pt Ln = rp(R_LX, R_LY, N), Rn = rp(R_RX, R_RY, N), TLn = rp(R_TLX, R_TLY, N), TRn = rp(R_TRX, R_TRY, N);
    /* 1. walls and floor, colour 5 */
    if (ROLL != 0) {                                  /* extend the near points: 4 near - 3 far */
        Ln.x = sub16(shl16(Ln.x, 2), (s16)(Lf.x * 3));    Ln.y = sub16(shl16(Ln.y, 2), (s16)(Lf.y * 3));
        Rn.x = sub16(shl16(Rn.x, 2), (s16)(Rf.x * 3));    Rn.y = sub16(shl16(Rn.y, 2), (s16)(Rf.y * 3));
        TLn.x = sub16(shl16(TLn.x, 2), (s16)(TLf.x * 3)); TLn.y = sub16(shl16(TLn.y, 2), (s16)(TLf.y * 3));
        TRn.x = sub16(shl16(TRn.x, 2), (s16)(TRf.x * 3)); TRn.y = sub16(shl16(TRn.y, 2), (s16)(TRf.y * 3));
        q4(q, TLn, TLf, Lf, Ln); poly4(q, 5, 5);
        q4(q, TRn, TRf, Rf, Rn); poly4(q, 5, 5);
        q4(q, Lf, Rf, Rn, Ln);   poly4(q, 5, 5);
    } else {
        if (N == 14) { drv_fill_rows(RW(A_Y, 14), 5); road_fillmode_begin(); }
        else {
            drv_set_colour(5);
            for (s16 y = RW(A_Y, 7); y <= YMAX; y++) drv_hspan(XMIN, XMAX, y);
        }
        Pt l0 = { 0, Lf.y }, r0 = { 0x13F, Rf.y };
        q4(q, TLn, TLf, Lf, l0); poly4(q, 5, 5);
        q4(q, TRn, TRf, Rf, r0); poly4(q, 5, 5);
    }
    clip_line(Lf.x, Lf.y, Ln.x, Ln.y, 0);             /* floor edges black */
    clip_line(Rf.x, Rf.y, Rn.x, Rn.y, 0);
    clip_line(TLf.x, TLf.y, TLn.x, TLn.y, 0xF);       /* wall tops white */
    clip_line(TRf.x, TRf.y, TRn.x, TRn.y, 0xF);
    Pt tfl = rot(DSS(DS_aq_trough_far_xl), DSS(DS_aq_trough_far_y));
    Pt tfr = rot(DSS(DS_aq_trough_far_xr), DSS(DS_aq_trough_far_y));
    Pt tnl = rot(DSS(DS_aq_trough_near_xl), DSS(DS_aq_trough_near_y));
    Pt tnr = rot(DSS(DS_aq_trough_near_xr), DSS(DS_aq_trough_near_y));

    /* 2. per row band, far to near */
    for (s16 k = 0; k < N; k++) {
        u16 w = aq_flag(k);
        if (w & 0x400) {                              /* white floor band */
            q4(q, rp(R_LX, R_LY, k), rp(R_RX, R_RY, k), rp(R_RX, R_RY, k + 1), rp(R_LX, R_LY, k + 1));
            poly4(q, 0xF, 0xF);
        }
        if ((w & 1) && RW(A_Y, N - k) > RW(A_Y, N)) { /* joint lines */
            Pt l = rp(R_LX, R_LY, k), r = rp(R_RX, R_RY, k), tl = rp(R_TLX, R_TLY, k), tr = rp(R_TRX, R_TRY, k);
            clip_line(l.x, l.y, r.x, r.y, 0);
            clip_line(l.x, l.y, tl.x, tl.y, 0);
            clip_line(r.x, r.y, tr.x, tr.y, 0);
        }
        s16 hit = aq_wall_opening_quad(k, w, q, k > 1 ? 1 : 0);
        if (hit && clip_polygon(q, 4, o, &n)) {
            if (N == 14) {
                if (poly_straddles_bottom(0x5A, o, n)) DSW(DS_aq_hit_bits) |= (u16)hit;
                if (poly_straddles_bottom(0x13E, o, n)) DSW(DS_aq_hit_bits) |= (u16)(hit << 2);
            }
            poly_fill_pts(o, n, 2, 0);
        }
        s16 i = (s16)(N - k);
        if ((w & 0x20) && k > 0)
            aq_draw_row_shape((s16)(RW(A_XL, i) + (sub16(RW(A_XR, i), RW(A_XL, i)) >> 2)), RW(A_Y, i), (s16)(i - 1), 2);
        if ((w & 0x40) && k > 0)
            aq_draw_row_shape(sub16(RW(A_XR, i), (s16)(sub16(RW(A_XR, i), RW(A_XL, i)) >> 3)), RW(A_Y, i), (s16)(i - 1), 2);
    }

    /* 3. centre trough, interrupted where the flags have bit 2 */
    s16 a = -1, b;
    for (s16 k = 0; k < N; k++) if (aq_flag(k) & 2) { a = k; break; }
    for (b = (s16)(a + 1); b < N && (aq_flag(b) & 2); b++) {}
    if (b < 2) { q4(q, tfl, tnl, tnr, tfr); poly4(q, 2, 0); }
    else {
        if (a > 0) {
            s16 y = RW(A_Y, N - a);
            Pt pl = rot(line_x_at_y(DSS(DS_aq_trough_far_xl), DSS(DS_aq_trough_far_y), DSS(DS_aq_trough_near_xl), DSS(DS_aq_trough_near_y), y), y);
            Pt pr = rot(line_x_at_y(DSS(DS_aq_trough_far_xr), DSS(DS_aq_trough_far_y), DSS(DS_aq_trough_near_xr), DSS(DS_aq_trough_near_y), y), y);
            q4(q, tfl, pl, pr, tfr); poly4(q, 2, 0);
        }
        if (b < N - 1) {
            s16 y = RW(A_Y, N - b);
            Pt pl = rot(line_x_at_y(DSS(DS_aq_trough_far_xl), DSS(DS_aq_trough_far_y), DSS(DS_aq_trough_near_xl), DSS(DS_aq_trough_near_y), y), y);
            Pt pr = rot(line_x_at_y(DSS(DS_aq_trough_far_xr), DSS(DS_aq_trough_far_y), DSS(DS_aq_trough_near_xr), DSS(DS_aq_trough_near_y), y), y);
            q4(q, pl, tnl, tnr, pr); poly4(q, 2, 0);
        }
    }

    /* 4. boxes, pillars, starter, opponent: rows far to near on the raw arrays */
    for (s16 i = N; i > 0; i--) {
        u16 w = aq_flag((s16)(N - i));
        if (w & 0x10) {                               /* low box over the middle third */
            s16 x0 = (s16)(RW(A_XL, i) + cdiv16(sub16(RW(A_XC, i), RW(A_XL, i)), 3));
            s16 x1 = (s16)(RW(A_XR, i) + cdiv16(sub16(RW(A_XC, i), RW(A_XR, i)), 3));
            s16 y0 = RW(A_Y, i), yt = (s16)(y0 + cdiv16(sub16(RW(A_YT, i), y0), 7));
            s16 n0 = (s16)(RW(A_XL, i - 1) + cdiv16(sub16(RW(A_XC, i - 1), RW(A_XL, i - 1)), 3));
            s16 n1 = (s16)(RW(A_XR, i - 1) + cdiv16(sub16(RW(A_XC, i - 1), RW(A_XR, i - 1)), 3));
            s16 ny = RW(A_Y, i - 1), nt = sub16(ny, cdiv16(sub16(ny, RW(A_YT, i - 1)), 7));
            s16 p[16] = { x0, y0, x1, y0, x0, yt, x1, yt, n0, ny, n1, ny, n0, nt, n1, nt };
            aq_draw_box(p, 0);
        }
        if (w & 0x80) {                               /* two thin full-height pillars */
            s16 d = cdiv16(sub16(RW(A_XR, i), RW(A_XL, i)), 7), e = cdiv16(sub16(RW(A_XR, i - 1), RW(A_XL, i - 1)), 7);
            s16 y0 = RW(A_Y, i), yt = RW(A_YT, i), ny = RW(A_Y, i - 1), nt = RW(A_YT, i - 1);
            s16 l0 = (s16)(RW(A_XL, i) + d), l1 = (s16)(l0 + (d >> 3));
            s16 m0 = (s16)(RW(A_XL, i - 1) + e), m1 = (s16)(m0 + (e >> 3));
            s16 p[16] = { l0, y0, l1, y0, l0, yt, l1, yt, m0, ny, m1, ny, m0, nt, m1, nt };
            aq_draw_box(p, 1);
            s16 r0 = (s16)(RW(A_XR, i) - (d >> 3) - d), r1 = (s16)(r0 + (d >> 3));
            s16 s0 = (s16)(RW(A_XR, i - 1) - (e >> 3) - e), s1 = (s16)(s0 + (e >> 3));
            s16 p2[16] = { r0, y0, r1, y0, r0, yt, r1, yt, s0, ny, s1, ny, s0, nt, s1, nt };
            aq_draw_box(p2, 1);
        }
        if (N == 14) {
            if (sub16(DSS(DS_opp_seg), DSS(DS_seg)) == i) opponent_draw();
        } else {
            if ((s16)(i - N - DSS(DS_starter_seg) + DSS(DS_seg) - 1) == 0) starter_draw_mirror_aq((s16)(N - i));
            if (sub16(DSS(DS_seg), DSS(DS_opp_seg)) == i) opponent_draw();
        }
    }
    if (N == 14 && DSS(DS_opp_seg) == DSS(DS_seg)) opponent_draw();
}

/* ================================================================================ roll */

/* 0dcb:9d9c aq_climb_left: car on the left wall: roll and next frame's channel deformation (writes player x) */
static void aq_climb_left(void)
{
    DSS(DS_rot_cx) = RW(A_XL, 14);
    s16 e = DSS(DS_aq_car_xl) < 0x514 ? 0xB4 : sub16(0x5C8, DSS(DS_aq_car_xl));
    ROLL = cdiv16((s16)(e * 0x6C), 400);
    s16 q = (s16)(e >> 2);
    DSS(DS_aq_def_xl) = q;
    DSS(DS_aq_def_floor_h) = (s16)(e - (e >> 3));
    DSS(DS_aq_def_xr) = q;
    DSS(DS_aq_def_top_xr) = q;
    if (user_drives()) DSS(DS_cam_x) = add16(DSS(DS_cam_x), 0xF);
    if (DSS(DS_aq_car_xl) < 0x514) {                 /* over the wall top */
        if (user_drives()) DSS(DS_cam_x) = add16(DSS(DS_cam_x), 0x19);
        s16 t = sub16(0x514, DSS(DS_aq_car_xl));
        DSS(DS_aq_def_top_xl) = t > 200 ? 200 : t;
        DSS(DS_aq_def_xl) = add16(DSS(DS_aq_def_xl), DSS(DS_aq_def_top_xl));
        DSS(DS_aq_def_floor_h) = add16(DSS(DS_aq_def_floor_h), DSS(DS_aq_def_top_xl));
        DSS(DS_aq_def_top_h) = t > 200 ? 200 : t;
    }
}

/* 0dcb:9e4e aq_climb_right: right wall (asymmetric to 9d9c by design) */
static void aq_climb_right(void)
{
    DSS(DS_rot_cx) = RW(A_XR, 14);
    s16 e = DSS(DS_aq_car_xr) > 0xD80 ? 0x88 : sub16(DSS(DS_aq_car_xr), 0xCF8);
    ROLL = cdiv16((s16)(e * -0x5A), 400);
    DSS(DS_aq_def_xr) = (s16)-(s16)(e * 3);
    s16 q = (s16)(e >> 2);
    DSS(DS_aq_def_floor_h) = (s16)(e - q);
    DSS(DS_aq_def_top_xr) = (s16)(e - q);
    DSS(DS_aq_def_top_h) = (s16)(q - e);
    DSS(DS_aq_def_xl) = sub16(DSS(DS_aq_def_xl), shl16(e, 1));
    DSS(DS_aq_def_top_xl) = sub16(DSS(DS_aq_def_top_xl), shl16(e, 1));
    if (user_drives()) DSS(DS_cam_x) = sub16(DSS(DS_cam_x), 0xF);
    if (DSS(DS_aq_car_xr) > 0xD80) {
        if (user_drives()) DSS(DS_cam_x) = sub16(DSS(DS_cam_x), 0x19);
        if (DSW(DS_aq_7fcc)) DSS(DS_heading) = add16(DSS(DS_heading), 1);
        DSS(DS_aq_def_top_xr) = add16(DSS(DS_aq_def_top_xr), sub16(0xD80, DSS(DS_aq_car_xr)));
        DSS(DS_aq_def_xr) = add16(DSS(DS_aq_def_xr), sub16(0xD80, DSS(DS_aq_car_xr)));
        DSS(DS_aq_def_top_h) = add16(DSS(DS_aq_def_top_h), sub16(DSS(DS_aq_car_xr), 0xD80));
        DSS(DS_aq_def_floor_h) = add16(DSS(DS_aq_def_floor_h), sub16(DSS(DS_aq_car_xr), 0xD80));
    }
}

/* 0dcb:9f08 aq_trough_bump: roll kick and speed penalty on the centre trough (writes player speed) */
static void aq_trough_bump(void)
{
    if (aq_flag(14) & 2) return;                     /* byte [96F6 - 1Ch]: current segment has no trough */
    s16 sl = DSS(DS_aq_car_sxl), sr = DSS(DS_aq_car_sxr);
    s16 tl = DSS(DS_aq_trough_near_xl), tr = DSS(DS_aq_trough_near_xr);
    DSS(DS_aq_side_l) = sl < tl ? -1 : sl > tr ? 1 : 0;
    DSS(DS_aq_side_r) = sr < tl ? -1 : sr > tr ? 1 : 0;
    if (DSS(DS_aq_side_l) == 0 || add16(DSS(DS_aq_side_l_prev), DSS(DS_aq_side_l)) == 0) ROLL = -4;
    if (DSS(DS_aq_side_r) == 0 || add16(DSS(DS_aq_side_r_prev), DSS(DS_aq_side_r)) == 0) ROLL = 4;
    if (ROLL != 0 && PHS(PLAYER, PH_SPEED) > 10) PHS(PLAYER, PH_SPEED) = (s16)(PHS(PLAYER, PH_SPEED) - 2);
}

/* 0dcb:9f9a aq_roll_update: wrap, shape variant, pitch from the roll rate */
static void aq_roll_update(void)
{
    ROLL = add16(ROLL, DSS(DS_wall_hit_shift));
    if (ROLL > 0x167) ROLL = (s16)(ROLL - 0x168);
    if (ROLL < -0x167) ROLL = (s16)(ROLL + 0x168);
    s16 a = abs16(ROLL);
    DSS(DS_aq_roll_abs) = a;
    s16 v = a > 0x1E ? 0 : a > 0xF ? 1 : a > 7 ? 2 : 3;
    if (ROLL < 0) v = (s16)(6 - v);
    DSS(DS_aq_shape_variant) = v;
    s16 d = sub16(ROLL, DSS(DS_aq_roll_prev));
    s16 p = d > 7 ? 3 : d > 5 ? 2 : d > 3 ? 1 : d < -7 ? -3 : d < -5 ? -2 : d < -3 ? -1 : 0;
    if (ROLL < 0) p = (s16)-p;
    if (abs16(sub16(p, DSS(DS_aq_pitch_prev))) > 1) p = (s16)((s16)(DSS(DS_aq_pitch_prev) + p) >> 1);
    DSS(DS_aq_pitch) = p;
    DSS(DS_aq_roll_prev) = ROLL;
    DSS(DS_aq_pitch_prev) = p;
    DSS(DS_aq_side_l_prev) = DSS(DS_aq_side_l);
    DSS(DS_aq_side_r_prev) = DSS(DS_aq_side_r);
}

/* rotate rows 0..n of the raw arrays into the reversed rotated arrays (a094 / 9ce8) */
static void aq_rotate_rows(s16 n)
{
    for (s16 i = 0; i <= n; i++) {
        s16 k = (s16)(n - i), x, y;
        rotate_about_centre(RW(A_XL, i), RW(A_Y, i), &x, &y);   RW(R_LX, k) = x;  RW(R_LY, k) = y;
        rotate_about_centre(RW(A_XR, i), RW(A_Y, i), &x, &y);   RW(R_RX, k) = x;  RW(R_RY, k) = y;
        rotate_about_centre(RW(A_XTL, i), RW(A_YT, i), &x, &y); RW(R_TLX, k) = x; RW(R_TLY, k) = y;
        rotate_about_centre(RW(A_XTR, i), RW(A_YT, i), &x, &y); RW(R_TRX, k) = x; RW(R_TRY, k) = y;
    }
}

/* 0dcb:a094 aq_draw_front (road_frame set B194 = 14 and DS:9A44 = AQ + 2 seg) */
void aq_draw_front(void)
{
    FarPtr fp = ds_far(DS_aq_flags_ptr);
    fp.off = (u16)(fp.off + 0x1C);                  /* k = 0 -> C[seg + 14] */
    ds_far_wr(DS_aq_flag_ptr, fp);
    DSS(DS_rot_cx) = (s16)((s16)(RW(A_XL, 14) + RW(A_XR, 14)) >> 1);
    DSS(DS_rot_cy) = RW(A_Y, 14);
    DSS(DS_aq_saved_floor_h) = DSS(DS_aq_def_floor_h);
    DSS(DS_aq_saved_xl) = DSS(DS_aq_def_xl);
    DSS(DS_aq_saved_xr) = DSS(DS_aq_def_xr);
    DSS(DS_aq_saved_top_h) = DSS(DS_aq_def_top_h);
    DSS(DS_aq_def_floor_h) = DSS(DS_aq_def_xl) = DSS(DS_aq_def_xr) = 0;
    DSS(DS_aq_def_top_h) = DSS(DS_aq_def_top_xl) = DSS(DS_aq_def_top_xr) = 0;
    if (DSS(DS_aq_car_xl) > 0x5C8 && DSS(DS_aq_car_xr) < 0xCF8) ROLL = 0;
    else if (DSS(DS_aq_car_xl) <= 0x5C8) aq_climb_left();
    else if (DSS(DS_aq_car_xr) >= 0xCF8) aq_climb_right();
    aq_trough_bump();
    aq_roll_update();
    DSS(DS_rot_sin) = sin256(ROLL);
    DSS(DS_rot_cos) = sin256((s16)(ROLL + 0x5A));
    aq_rotate_rows(14);
    clip_set_view();
    aq_draw_sky();
    road_fillmode_begin();
    aq_draw_scene();
}

/* 0dcb:9ce8 aq_draw_mirror (road_frame set B194 = 7 and DS:9A44 = 0x6570 - 2 seg); this frame's roll sin/cos */
void aq_draw_mirror(void)
{
    FarPtr fp = ds_far(DS_aq_flags_ptr);
    fp.off = (u16)(fp.off + 0x10);                  /* k = 0 -> C[seg - 8] */
    ds_far_wr(DS_aq_flag_ptr, fp);
    DSS(DS_rot_cx) = (s16)((s16)(RW(A_XL, 7) + RW(A_XR, 7)) >> 1);
    DSS(DS_rot_cy) = RW(A_Y, 7);
    aq_rotate_rows(7);
    clip_set_mirror();
    road_fillmode_end();
    mirror_draw_backdrop();
    road_fillmode_begin();
    aq_draw_scene();
    mirror_finish();
}

/* ================================================================================ wall bounce, obstacle probe */

/* 0dcb:a1c0 aq_wall_bounce: 8 (fast) or 1 frame(s) of the car bouncing off a wall (road_step, DS:B8C8 set) */
void aq_wall_bounce(s16 dir)
{
    DSW(DS_aq_bounce_flag) = 1;
    s16 d = (s16)(shl16(sub16(DSS(DS_opp_seg), DSS(DS_seg)), 3) + q_shr(sub16(DSS(DS_opp_seg_frac64), DSS(DS_substep)), 3));
    s16 k = d >= 0x10 ? (s16)(((s16)(d + 8) >> 4) + 2) : -1;
    s16 sp = PHS(PLAYER, PH_SPEED);
    if (sp > 0x1E) {
        s16 s6 = cdiv16(sp, 6), h6 = cdiv16(DSS(DS_heading), 6);
        s16 db = dir > 0 ? 0x20 : -0x22, dxc = dir > 0 ? 0x28 : -0x28;
        for (s16 i = 0; i < 8; i++) {
            if (k > 1) DSS(DS_opp_seg) = add16(DSS(DS_opp_seg), 2);   /* the opponent pulls away */
            DSS(DS_wall_hit_shift) = add16(DSS(DS_wall_hit_shift), db);
            DSS(DS_heading) = sub16(DSS(DS_heading), h6);
            DSS(DS_cam_x) = add16(DSS(DS_cam_x), dxc);
            DSS(DS_view_bounce_y) = mul16((s16)(i < 4 ? i : 8 - i), s6);
            race_step();
            road_frame();
        }
        DSS(DS_wall_hit_shift) = 0;                  /* DS:9A56 keeps its last value */
    } else {
        DSS(DS_wall_hit_shift) = sub16(DSS(DS_wall_hit_shift), (s16)(dir > 0 ? 6 : -6));
        DSS(DS_view_bounce_y) = 10;
    }
    race_step();
    road_frame();
    DSS(DS_wall_hit_shift) = 0;
    PHS(PLAYER, PH_SPEED) = sp;
}

/* 0dcb:a2d8 aq_collide_probe (player_advance 0001:cee0): projects rows 0-1 (overwrites them) and tests the floor
 * obstacle of the flags against the bottom view row; returns 0, 20h, 40h or 60h. No rotation. */
s16 aq_collide_probe(s16 flags)
{
    s16 xl = DSS(DS_aq_saved_xl), xr = DSS(DS_aq_saved_xr);
    s16 mid = (s16)((s16)(xr + xl + 0x12C0) >> 1);
    s16 z = (s16)(12 - q_shr(DSS(DS_substep), 3));
    for (s16 i = 0; i < 2; i++) {
        s16 dy = DSS(DS_aq_dy), dx = DSS(DS_aq_dx);
        RW(A_Y, i)   = road_proj_y(z, (s16)(DSS(DS_aq_saved_floor_h) + 2000), dy);
        RW(A_XL, i)  = road_proj_x(z, (s16)(DSS(DS_aq_saved_xl) + 0x5C8), dx);
        RW(A_XR, i)  = road_proj_x(z, (s16)(DSS(DS_aq_saved_xr) + 0xCF8), dx);
        RW(A_XC, i)  = road_proj_x(z, (s16)(DSS(DS_aq_saved_xr) + mid), dx);
        RW(A_YT, i)  = road_proj_y(z, (s16)(DSS(DS_aq_saved_top_h) + 0x67C), DSS(DS_aq_dy));
        RW(A_XTL, i) = road_proj_x(z, (s16)(DSS(DS_aq_wall_l_shift) + 0x438), DSS(DS_aq_dx));
        RW(A_XTR, i) = road_proj_x(z, (s16)(DSS(DS_aq_wall_r_shift) + 0xE88), DSS(DS_aq_dx));
        z = (s16)(z + 8);
    }
    s16 q[8], o[128], n;
    u16 r = 0;
    if (flags & 0x10) {                              /* box footprint, point order as in the exe (bow-tie) */
        s16 y1 = (s16)(RW(A_Y, 1) + cdiv16(sub16(RW(A_YT, 1), RW(A_Y, 1)), 7));
        s16 y0 = (s16)(RW(A_Y, 0) + cdiv16(sub16(RW(A_YT, 0), RW(A_Y, 0)), 7));
        q[0] = (s16)(RW(A_XL, 1) + cdiv16(sub16(RW(A_XC, 1), RW(A_XL, 1)), 3)); q[1] = y1;
        q[2] = (s16)(RW(A_XR, 1) + cdiv16(sub16(RW(A_XC, 1), RW(A_XR, 1)), 3)); q[3] = y1;
        q[4] = (s16)(RW(A_XL, 0) + cdiv16(sub16(RW(A_XC, 0), RW(A_XL, 0)), 3)); q[5] = y0;
        q[6] = (s16)(RW(A_XR, 0) + cdiv16(sub16(RW(A_XC, 0), RW(A_XR, 0)), 3)); q[7] = y0;
        if (clip_polygon(q, 4, o, &n)) r = poly_bottom_hit_bits(o, n);
    } else if (flags & 0x80) {                       /* pillar footprints */
        s16 d1 = cdiv16(sub16(RW(A_XR, 1), RW(A_XL, 1)), 7), d0 = cdiv16(sub16(RW(A_XR, 0), RW(A_XL, 0)), 7);
        s16 y1 = RW(A_Y, 1), y0 = RW(A_Y, 0);
        q[0] = (s16)(RW(A_XL, 1) + d1);               q[1] = y1;
        q[2] = (s16)(q[0] + (d1 >> 3));               q[3] = y1;
        q[6] = (s16)(RW(A_XL, 0) + d0);               q[7] = y0;
        q[4] = (s16)(q[6] + (d0 >> 3));               q[5] = y0;
        if (clip_polygon(q, 4, o, &n)) r = poly_bottom_hit_bits(o, n);
        q[0] = (s16)(RW(A_XR, 1) - (d1 >> 3) - d1);   q[1] = y1;
        q[2] = (s16)(q[0] + (d1 >> 3));               q[3] = y1;
        q[6] = (s16)(RW(A_XR, 0) - (d0 >> 3) - d0);   q[7] = y0;
        q[4] = (s16)(q[6] + (d0 >> 3));               q[5] = y0;
        if (clip_polygon(q, 4, o, &n)) r |= poly_bottom_hit_bits(o, n);
    }
    return (s16)r;
}

/* ================================================================================ rotated sprites */

/* 1a21:9b96 heap_free: bytes between the high and the low mark stack tops of the work arena */
static u16 heap_free(void)
{
    return (u16)(DSW((u16)(DS_arena_w + 2 * DSW(DS_arena_top_index))) - DSW((u16)(DS_arena_w + 2 * DSW(DS_arena_low_depth))));
}

/* 1a21:9bac bitmap_bytes: what arena_bitmap_alloc would take */
static u16 bitmap_bytes(s16 w, s16 h, u16 flags)
{
    u16 plane = (u16)((s16)(((s16)(w - 1) >> (DSW(DS_g_px_shift) & 0x1F)) + 1) * h);
    return (u16)(0x14 + (flags ? plane : 0) + ((flags & 2) ? 0 : (u16)(plane * DSW(DS_g_nplanes))));
}

/* 0dcb:bb22 rot_alloc: destination of a rotation (bbox of the rotated corners) and the inverse-mapping start */
static FarPtr rot_alloc(s16 angle, FarPtr src, s16 mode, s16 *u0, s16 *v0, s8 *ax, s8 *ay, s16 pivot, s16 setup)
{
    if (setup) {
        DSS(DS_rot_saved + 0) = DSS(DS_rot_sin);
        DSS(DS_rot_saved + 6) = DSS(DS_rot_cos);
        DSS(DS_rot_saved + 2) = DSS(DS_rot_cx);
        DSS(DS_rot_saved + 4) = DSS(DS_rot_cy);
        DSS(DS_rot_sin) = sin256(angle);
        DSS(DS_rot_cos) = sin256((s16)(angle + 0x5A));
        DSS(DS_rot_cx) = DSS(DS_rot_cy) = 0;
    }
    s16 w = (s16)desc_w(src), h = (s16)desc_h(src);
    s16 x0, y0, x1, y1, x2, y2;
    rotate_about_centre(0, (s16)(h - 1), &x0, &y0);
    rotate_about_centre((s16)(w - 1), (s16)(h - 1), &x1, &y1);
    rotate_about_centre((s16)(w - 1), 0, &x2, &y2);
    s16 W = abs16(sub16(x2, x0));
    if (W < abs16(x1)) W = abs16(x1);
    s16 nw = (s16)((u16)(~DSW(DS_g_x_align) + W + 1) & DSW(DS_g_x_align));
    s16 H = abs16(sub16(y2, y0));
    if (H < abs16(y1)) H = abs16(y1);
    H++;
    if (!(heap_free() > bitmap_bytes(nw, H, 1))) mode = -1;      /* fall back to the SR2_SHP arena */
    FarPtr d = DSW(DS_rot_into_scratch) ? bitmap_alloc(nw, H, 1, -1, ds_far(DS_rot_scratch_pic))
                                         : arena_bitmap_alloc(nw, H, 1, mode);
    bitmap_clear(d);
    s16 minx = x0 > 0 ? 0 : x0;
    if (minx > x1) minx = x1;
    if (minx > x2) minx = x2;
    s16 miny = y0 > 0 ? 0 : y0;
    if (miny > y1) miny = y1;
    if (miny > y2) miny = y2;
    if (setup) {
        s16 px, py;
        rotate_about_centre(pivot, (s16)(h - 1), &px, &py);
        px = sub16(px, minx);
        py = sub16((s16)desc_h(d), sub16(py, miny));
        *ax = (s8)px;
        *ay = (s8)py;
        s16 s = DSS(DS_rot_sin), c = DSS(DS_rot_cos);
        *u0 = sub16(mul16(c, minx), mul16(s, miny));
        *v0 = add16(mul16(s, minx), mul16(c, miny));
    }
    return d;
}

/* 0dcb:bd12 pic_rotate_planar (slot 16): nearest-neighbour rotation by angle, anchor of the bottom pivot in *ax, *ay */
FarPtr pic_rotate_planar(FarPtr src, s16 angle, s16 mode, s8 *ax, s8 *ay, s16 pivot)
{
    s16 u0, v0;
    FarPtr d = rot_alloc(angle, src, mode, &u0, &v0, ax, ay, pivot, 1);
    u16 rowoff[256];                                /* PORT: the original's stack table has 60 entries */
    s16 sw = (s16)desc_w(src), sh = (s16)desc_h(src);
    u16 sbpr = desc_stride(src);
    for (s16 y = 0; y < sh && y < 256; y++) rowoff[y] = (u16)(y * sbpr);
    u16 splane = (u16)(desc_size(src) >> 2), dplane = (u16)(desc_size(d) >> 2);
    FarPtr sp = desc_planes(src), dp = desc_planes(d);
    u16 dbpr = desc_stride(d), doff = 0;
    s16 c = DSS(DS_rot_cos), s = DSS(DS_rot_sin);
    for (s16 rows = (s16)desc_h(d); rows != 0; rows--) {
        bool hit = false, stop = false;
        s16 u = u0, v = v0;
        u16 cnt = dbpr;
        while (cnt != 0) {
            if (stop) { doff = (u16)(doff + cnt); break; }
            u8 p[4] = { 0, 0, 0, 0 };
            for (s16 j = 0; j <= 7; j++) {
                s16 sy = (s8)(u8)((u16)v >> 8), sx = (s8)(u8)((u16)u >> 8);
                if (sx >= 0 && sx < sw && sy >= 0 && sy < sh) {
                    hit = true;
                    u8 bit = (u8)(0x80 >> (sx & 7));
                    u16 o = (u16)((sx >> 3) + rowoff[sy & 0xFF]);
                    s16 shf = (s16)(j - (sx & 7));
                    for (int k = 0; k < 4; k++) {
                        u8 b = (u8)(rd8(sp.seg, (u16)(sp.off + k * splane + o)) & bit);
                        p[k] |= shf > 0 ? (u8)(b >> shf) : (u8)(b << -shf);
                    }
                } else if (hit) { stop = true; break; }
                u = add16(u, c);
                v = add16(v, s);
            }
            for (int k = 0; k < 4; k++) wr8(dp.seg, (u16)(dp.off + k * dplane + doff), p[k]);
            cnt--;
            doff++;
        }
        u0 = sub16(u0, s);
        v0 = add16(v0, c);
    }
    make_mask_plane(desc_planes(d), desc_mask(d), desc_size(d));
    DSS(DS_rot_sin) = DSS(DS_rot_saved + 0);
    DSS(DS_rot_cos) = DSS(DS_rot_saved + 6);
    DSS(DS_rot_cx) = DSS(DS_rot_saved + 2);
    DSS(DS_rot_cy) = DSS(DS_rot_saved + 4);
    return d;
}

/* 0dcb:86d2 anchor_scale_rows: anchors of the shrunk levels row+1..row+n from those of level row, all 7 angles */
static void anchor_scale_rows(s16 row, s16 n, u16 masks)
{
    for (s16 k = 1; k <= n; k++) {
        s16 f = popcount8(DSB((u16)(masks + k - 1)));
        for (s16 c = 0; c < 7; c++) {
            DSC((u16)(DS_anchor_x + 7 * (row + k) + c)) = (s8)((s16)(DSC((u16)(DS_anchor_x + 7 * row + c)) * f) >> 3);
            DSC((u16)(DS_anchor_y + 7 * (row + k) + c)) = (s8)((s16)(DSC((u16)(DS_anchor_y + 7 * row + c)) * f) >> 3);
        }
    }
}

/* 0dcb:8774 rot_shrink_max_size: worst-case bytes of "rotate then shrink" for -45..0 degrees (leaves the angle-0 sin/cos) */
static s16 rot_shrink_max_size(FarPtr pic, u8 mask)
{
    s16 best = 0, du, dv;
    s8 c0, c1;
    for (s16 i = 0; i < 4; i++) {
        s16 a = DSS((u16)(DS_rot_angles + 2 * i));
        DSS(DS_rot_sin) = sin256(a);
        DSS(DS_rot_cos) = sin256((s16)(a + 0x5A));
        DSS(DS_rot_cx) = DSS(DS_rot_cy) = 0;
        FarPtr t = rot_alloc(0, pic, 2, &du, &dv, &c0, &c1, 0, 0);   /* angle / pivot unused (stack garbage) */
        arena_pop(1);
        FarPtr s = pic_alloc_shrunk((s16)desc_w(t), (s16)desc_h(t), 1, mask, mask);
        arena_pop(1);
        s16 b = (s16)bitmap_bytes((s16)desc_w(s), (s16)desc_h(s), 1);
        if (b > best) best = b;
    }
    return best;
}

/* 0dcb:8840 race_load_graphics_rot: race_run's loader for mode 8 (no sky / horizon / signs / police) */
void race_load_graphics_rot(void)
{
    ds_far_wr(DS_pic_hood, pic_get(DSS((u16)(DS_hood_pics + 6 * DSS(DS_dash_type))), 1));
    ds_far_wr(DS_pic_pillar, pic_get(2003, 1));
    mirror_bg_prepare(0);
    start_props_load();
    DSW(DS_start_active) = 1;
    u16 e = (u16)(DS_opp_sprite_sets + 14 * OVL_B0(DSS(DS_opp_model)));
    DSS(DS_start_light_pos) = 0;
    for (u16 k = 0; k < 13 * 7; k++) { DSB((u16)(DS_anchor_x + k)) = 0; DSB((u16)(DS_anchor_y + k)) = 0; }
    ds_far_wr(DS_rot_side, pic_get(DSS(e), -1));
    ds_far_wr(DS_rot_r34, pic_get(DSS((u16)(e + 2)), -1));
    ds_far_wr(DS_rot_rear + 12, pic_get(DSS((u16)(e + 4)), -1));                 /* 9E7E = 9E72[3] */
    u16 ra = DSW((u16)(e + 6)), rb = DSW((u16)(e + 8)), rc = DSW((u16)(e + 12));
    ds_far_wr(DS_rot_rear5 + 12, drv_shrink(ds_far(DS_rot_rear + 12), DSB((u16)(ra + 11))));   /* A1D2, level-5 base */
    ds_far_wr(DS_rot_front + 12, pic_get(DSS((u16)(e + 10)), -1));               /* A32C */
    DSC((u16)(DS_anchor_x + 7 * 2 + 3)) = (s8)(bitmap_visible_width(ds_far(DS_rot_rear + 12)) >> 1);
    DSC((u16)(DS_anchor_x + 7 * 5 + 3)) = (s8)(bitmap_visible_width(ds_far(DS_rot_rear5 + 12)) >> 1);
    DSC((u16)(DS_anchor_x + 7 * 9 + 3)) = (s8)(bitmap_visible_width(ds_far(DS_rot_front + 12)) >> 1);
    arena_alloc(rot_shrink_max_size(ds_far(DS_rot_rear + 12), DSB((u16)(ra + 9))), 1);   /* reserve on the low stack */
    for (s16 i = 0; i < 7; i++) {
        if (i == 3) continue;                       /* 0 degrees = the originals */
        s16 a = DSS((u16)(DS_rot_angles + 2 * i));
        ds_far_wr((u16)(DS_rot_rear + 4 * i),
                  drv_rotate(ds_far(DS_rot_rear + 12), a, 2, (s8 *)mp(DGROUP, (u16)(DS_anchor_x + 7 * 2 + i)),
                             (s8 *)mp(DGROUP, (u16)(DS_anchor_y + 7 * 2 + i)), DSC((u16)(DS_anchor_x + 7 * 2 + 3))));
        ds_far_wr((u16)(DS_rot_rear5 + 4 * i),
                  drv_rotate(ds_far(DS_rot_rear5 + 12), a, 2, (s8 *)mp(DGROUP, (u16)(DS_anchor_x + 7 * 5 + i)),
                             (s8 *)mp(DGROUP, (u16)(DS_anchor_y + 7 * 5 + i)), DSC((u16)(DS_anchor_x + 7 * 5 + 3))));
        ds_far_wr((u16)(DS_rot_front + 4 * i),
                  drv_rotate(ds_far(DS_rot_front + 12), a, 2, (s8 *)mp(DGROUP, (u16)(DS_anchor_x + 7 * 9 + i)),
                             (s8 *)mp(DGROUP, (u16)(DS_anchor_y + 7 * 9 + i)), DSC((u16)(DS_anchor_x + 7 * 9 + 3))));
    }
    arena_pop_low(1);                               /* free the reserve: room for ad40's per-frame shrinks */
    anchor_scale_rows(2, 2, ra);
    anchor_scale_rows(5, 3, rb);
    anchor_scale_rows(9, 3, rc);
    DSS(DS_opp_variant) = 4;
    ds_far_wr(DS_opp_pic, ds_far(DS_rot_side));
    ds_far_wr(DS_seg_hgt_ptr, far_make(SEG(0x3722), 0));    /* the scratch pad: flat, straight */
    ds_far_wr(DS_seg_yaw_ptr, far_make(SEG(0x3722), 2));
    DSS(DS_rot_cos) = 0xFF;
    DSS(DS_opp_sprite) = 0;
    DSW(DS_rot_tmp_live) = 0;
    DSS(DS_aq_pitch_prev) = DSS(DS_aq_roll_prev) = DSS(DS_aq_side_l_prev) = DSS(DS_aq_side_r_prev) = 0;
    DSS(DS_aq_pitch) = DSS(DS_rot_cx) = DSS(DS_rot_cy) = DSS(DS_rot_sin) = 0;
    ROLL = 0;
    DSS(DS_wall_hit_shift) = DSS(DS_aq_wall_hit) = 0;
    DSW(DS_aq_hit_bits) = 0;
}

/* 0dcb:ad40 opp_sprite_aq: sprite s (0..12) for bank variant v (0..6, 3 = level); 3/4, 6..8, 10..12 shrunk on the fly
 * onto the high stack (popped on the next call) */
FarPtr opp_sprite_aq(s16 s, s16 v)
{
    s16 c = OVL_B0(DSS(DS_opp_model));
    if (DSW(DS_rot_tmp_live)) arena_pop(1);
    DSW(DS_rot_tmp_live) = 0;
    switch (s) {
    case 0: return ds_far(DS_rot_side);
    case 1: return ds_far(DS_rot_r34);
    case 2: return ds_far((u16)(DS_rot_rear + 4 * v));
    case 5: return ds_far((u16)(DS_rot_rear5 + 4 * v));
    case 9: return ds_far((u16)(DS_rot_front + 4 * v));
    default: break;
    }
    DSW(DS_rot_tmp_live) = 1;
    u16 row = (u16)(DS_opp_sprite_sets + 14 * c);
    if (s >= 3 && s <= 4)
        return drv_shrink(ds_far((u16)(DS_rot_rear + 4 * v)), DSB((u16)(DSW((u16)(row + 6)) + 3 * v + s - 3)));
    if (s >= 6 && s <= 8)
        return drv_shrink(ds_far((u16)(DS_rot_rear5 + 4 * v)), DSB((u16)(DSW((u16)(row + 8)) + 3 * v + s - 6)));
    return drv_shrink(ds_far((u16)(DS_rot_front + 4 * v)), DSB((u16)(DSW((u16)(row + 12)) + 3 * v + s - 10)));
}
