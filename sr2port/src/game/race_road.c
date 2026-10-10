/* Road renderer of race_render segment 0dcb (port/spec/race_render.md §1-§3; SR1 race_road.c): segment pointers,
 * projection, the frame (front view, mirror, overlays, page flip, pacing), the advance along the road and the
 * autopilot, edge collisions, sky / horizon / ground, road bands, town walls, hillsides, road-side shapes and
 * pictures, overhead banners, truss bridges, guard rails, barrier walls, cockpit and hood overlays, the graphics
 * loader. Opponent sprite and start props: race_opp.c; Aqueduct (mode 8): race_aq.c; spans / polygons / shapes:
 * race_gfx.c.
 *
 * State stays in mem[]: the far segment pointers DS:9A3C / B16A / A79E / 9DC8 / 9AD8 / 9754 / 9B10 / 9E8E, the
 * eleven row tables (shared by the front view and the mirror), the near row pointers DS:96B6..96E0 and DS:96EE, the
 * far flag pointers DS:96E2 / 96E6 / 96F0 / 96EA, the clip rectangle DS:7E12..7E18. 16-bit arithmetic as the
 * original (imul low word, sar, idiv). */
#include "game/race_int.h"
#include "game/race.h"

#include "host.h"
#include "platform/platform.h"
#include "platform/video.h"

#define MODE   DSS(DS_road_mode)
#define XMIN   DSS(DS_clip_xmin)
#define YMIN   DSS(DS_clip_ymin)
#define YMAX   DSS(DS_clip_ymax)
#define XMAX   DSS(DS_clip_xmax)

/* ================================================================================ helpers */

FarPtr back_page(void)
{
    return DSS(DS_back_page) == 1 ? ds_far(DS_page1_ptr) : ds_far(DS_page2_ptr);
}

/* value at a near row pointer DS:ptr, and its neighbours */
static inline s16 rv(u16 ptr) { return DSS(DSW(ptr)); }
static inline s16 rvk(u16 ptr, s16 k) { return DSS((u16)(DSW(ptr) + 2 * k)); }
/* word at a far pointer DS:fp + 2 idx */
static inline u16 fw(u16 fp, s16 idx) { FarPtr p = ds_far(fp); return rd16(p.seg, (u16)(p.off + 2 * idx)); }
static inline void fadv(u16 fp, s16 bytes) { DSW(fp) = (u16)(DSW(fp) + bytes); }
static inline void pt_set(s16 *p, s16 x, s16 y) { p[0] = x; p[1] = y; }

static void hl(s16 colour, s16 x1, s16 x2, s16 y) { drv_set_colour(colour); drv_hspan(x1, x2, y); }

/* ================================================================================ pointers, projection */

/* 0dcb:056a road_set_seg_ptrs */
void road_set_seg_ptrs(s16 seg)
{
    if (!(MODE & 6)) {
        ds_far_wr(DS_seg_left_ptr, trk_ptr(TRK_L, seg));
        ds_far_wr(DS_seg_right_ptr, trk_ptr(TRK_R, seg));
        ds_far_wr(DS_seg_centre_ptr, trk_ptr(TRK_C, seg));
    } else {                                                /* drag strip */
        ds_far_wr(DS_seg_left_ptr, trk_ptr(TRK_L2, seg));
        ds_far_wr(DS_seg_right_ptr, trk_ptr(TRK_R2, seg));
        ds_far_wr(DS_seg_centre_ptr, trk_ptr(TRK_C2, seg));
        seg = (s16)(seg + 0xC0);
    }
    ds_far_wr(DS_seg_lane_ptr, trk_ptr(TRK_LANE, seg));
    ds_far_wr(DS_seg_hgt_ptr, trk_ptr(TRK_HGT, seg));
    ds_far_wr(DS_seg_yaw_ptr, trk_ptr(TRK_YAW, seg));
    ds_far_wr(DS_seg_bank_l_ptr, trk_ptr(TRK_BL, seg));
    ds_far_wr(DS_seg_bank_r_ptr, trk_ptr(TRK_BR, seg));
}

static s16 cam_x_clamped(void)
{
    s16 cx = DSS(DS_cam_x);
    if (MODE == 8) { if (cx < 0x564) cx = 0x564; else if (cx > 0xD5C) cx = 0xD5C; }
    return cx;
}

/* 0dcb:062c road_proj_x */
s16 road_proj_x(s16 z, s16 x, s16 dx)
{
    s16 cx = cam_x_clamped();
    x = add16(x, (s16)((s16)(z * dx) >> 2));
    s16 t = shl16(sub16(cx, x), 2);
    if (z == 0) z = 1;
    return sub16(0xA0, cdiv16(t, z));
}

/* 0dcb:0684 road_proj_y */
s16 road_proj_y(s16 z, s16 y, s16 dy)
{
    s16 t = add16(shl16((s16)((s16)(z * dy) & 0xFFFE), 1), shl16(sub16(y, DSS(DS_cam_y)), 2));
    if (z == 0) z = 1;
    return add16(add16(cdiv16(t, z), DSS(DS_view_bounce_y)), 0x3C);
}

/* 0dcb:06ba road_proj_x_mirror */
s16 road_proj_x_mirror(s16 z, s16 x, s16 dx)
{
    s16 cx = cam_x_clamped();
    x = add16(x, (s16)((s16)(z * dx) >> 2));
    s16 t = sub16(cx, x);
    if (z == 0) z = 1;
    return sub16(0x118, cdiv16(t, z));
}

/* 0dcb:070e road_proj_y_mirror */
s16 road_proj_y_mirror(s16 z, s16 y, s16 dy)
{
    y = add16(y, (s16)(z * dy));
    s16 t = sub16(y, DSS(DS_cam_y));
    if (z == 0) z = 1;
    return add16(cdiv16(t, z), 0x24);
}

/* 0dcb:0736 road_state_init */
void road_state_init(void)
{
    if (DSB(DS_racing)) DSS(DS_seg) = DSS(DS_start_seg);
    else switch (DSS(DS_location)) {
        case 1: DSS(DS_seg) = 0x0A; break;
        case 2: case 4: DSS(DS_seg) = 0x41; break;
        case 3: DSS(DS_seg) = 0x73; break;
        default: break;
    }
    road_set_seg_ptrs(DSS(DS_seg));
    DSS(DS_edge_left) = DSS(DS_ai_edge_left) = 0x5DC;
    DSS(DS_edge_right) = DSS(DS_ai_edge_right) = 0xCE4;
    DSS(DS_cam_x) = 0xA00;
    DSS(DS_cam_y) = 0x76C;
    DSS(DS_substep) = 0;
    DSS(DS_cam_unused) = 0;
    DSS(DS_cur_height) = 0;
    DSS(DS_heading) = 0;
    DSS(DS_step_per_frame) = 0;
    DSS(DS_ai_side) = 1;
    DSS(DS_ai_recentre) = rnd(2) == 0 ? 1 : 0;
    DSW(DS_bump_allowed) = 1;                           /* A7D6 */
    DSS(DS_lat_last_opp_step) = DSS(DS_cam_x);
    DSW(DS_nudge_left_latch) = 0;
    DSW(DS_nudge_right_latch) = 0;
    DSW(DS_opp_sprite_prev) = 0;
    DSW(DS_opp_sprite) = 0;
    arena_clear();
}

/* the row tables (s16[15] each, row k at index k) */
#define T_XR   DS_row_xr        /* A2E2 */
#define T_XL   DS_row_xl        /* 9A5C */
#define T_Y    DS_row_y         /* 9EF0 */
#define T_XOR  DS_row_xor       /* AB5E */
#define T_XOL  DS_row_xol       /* AB2E */
#define T_YE   DS_row_yeye      /* A300 */
#define T_YK   DS_row_ykerb     /* A7EA */
#define T_YBL  DS_row_ybank_l   /* A5A8 */
#define T_YBR  DS_row_ybank_r   /* A7AE */
#define T_XFL  DS_row_xfar_l    /* ADB2 */
#define T_XFR  DS_row_xfar_r    /* B89E */
#define ROWT(t, k) DSS((u16)((t) + 2 * (k)))

static void edges_of(u16 cm, s16 *xl, s16 *xr)
{
    if (cm == 0)        { *xr = 0xCE4; *xl = 0x5DC; }
    else if (cm & 0x80) { *xr = 0xAF0; *xl = 0x7D0; }
    else if (cm & 0x20) { *xr = 0xCE4; *xl = 0xCE4; }      /* quirk (spec §5): zero-width road */
    else                { *xr = 0x5DC; *xl = 0x5DC; }
}

static s16 far_take(u16 fp, s16 step) { FarPtr p = ds_far(fp); s16 v = (s16)rd16(p.seg, p.off); fadv(fp, step); return v; }

/* 0dcb:07e4 road_project_front */
static void road_project_front(void)
{
    s16 w = MODE == 0x10 ? 200 : 400;
    s16 q = q_shr(DSS(DS_substep), 3);
    s16 z = 4;
    for (s16 k = 0; k < 15; k++) {
        u16 cm = (u16)(far_take(DS_seg_centre_ptr, 2) & 0xE0);
        s16 dy = 0;
        if (!(MODE & 6)) dy = sub16(DSS(DS_cur_height), far_take(DS_seg_hgt_ptr, 2));
        s16 dx = sub16(DSS(DS_heading), far_take(DS_seg_yaw_ptr, 2));
        if (k == 0 && MODE == 0x10) { if (dx > 0x30) dx = 0x30; else if (dx < -0x30) dx = -0x30; }
        if (k == 1) z = (s16)(0x0C - q);
        if (k == 14) z = add16(z, q);
        s16 xl, xr;
        edges_of(cm, &xl, &xr);
        if (k == 0) ROWT(T_Y, 0) = road_proj_y(4, 2000, dy);
        ROWT(T_XR, k) = road_proj_x(z, xr, dx);
        ROWT(T_XL, k) = road_proj_x(z, xl, dx);
        if (k != 0) ROWT(T_Y, k) = road_proj_y(z, 2000, dy);
        if (k == 0) {
            ROWT(T_XOR, 0) = road_proj_x(4, (s16)((w - DSS(DS_cam_x)) + 0x19C8), dx);   /* row 0 quirk */
            ROWT(T_XOL, 0) = road_proj_x(4, (s16)-(w - DSS(DS_cam_x)), dx);
        } else {
            ROWT(T_XOR, k) = road_proj_x(z, (s16)(w + 0xCE4), dx);
            ROWT(T_XOL, k) = road_proj_x(z, (s16)-(w - 0x5DC), dx);
        }
        ROWT(T_YE, k) = road_proj_y(z, 0x76C, dy);
        ROWT(T_YK, k) = road_proj_y(z, 0x79E, dy);
        ROWT(T_YBL, k) = road_proj_y(z, sub16(2000, far_take(DS_seg_bank_l_ptr, 2)), dy);
        ROWT(T_YBR, k) = road_proj_y(z, sub16(2000, far_take(DS_seg_bank_r_ptr, 2)), dy);
        ROWT(T_XFL, k) = road_proj_x(z, (s16)((w - 0x177) * -4), dx);
        ROWT(T_XFR, k) = road_proj_x(z, (s16)((w + 0x339) * 4), dx);
        if (k != 0) z = add16(z, 8);
    }
}

/* 0dcb:0ce4 road_project_mirror: rows 0..7 of the same tables (YK gets two entries per row) */
static void road_project_mirror(void)
{
    s16 w = MODE == 0x10 ? 200 : 400;
    s16 q = q_shr(DSS(DS_substep), 3);
    s16 z = 8;
    for (s16 k = 0; k < 8; k++) {
        u16 cm = (u16)(far_take(DS_seg_centre_ptr, -2) & 0xE0);
        s16 dy = 0;
        if (!(MODE & 6)) dy = sub16(far_take(DS_seg_hgt_ptr, -2), DSS(DS_cur_height));
        s16 dx = sub16(far_take(DS_seg_yaw_ptr, -2), DSS(DS_heading));
        if (k == 0 && (MODE & 0x10)) { if (dx > 0x3C) dx = 0x46; else if (dx < -0x3C) dx = -0x46; }
        if (k == 1) z = add16(q, 8);
        if (k == 5) z = sub16(z, q);
        s16 xl, xr;
        edges_of(cm, &xl, &xr);
        ROWT(T_XR, k) = road_proj_x_mirror(z, xr, dx);
        ROWT(T_XL, k) = road_proj_x_mirror(z, xl, dx);
        ROWT(T_Y, k) = road_proj_y_mirror(z, 2000, dy);
        s16 yk = road_proj_y_mirror(z, 0x79E, dy);
        if (k == 0) ROWT(T_YK, 0) = yk;                     /* quirk: rows >= 1 write YK[2k-1] and YK[2k] */
        else { ROWT(T_YK, 2 * k - 1) = yk; ROWT(T_YK, 2 * k) = yk; }
        ROWT(T_XOR, k) = road_proj_x_mirror(z, (s16)(w + 0xCE4), dx);
        ROWT(T_XOL, k) = road_proj_x_mirror(z, (s16)(0x5DC - w), dx);
        ROWT(T_YE, k) = road_proj_y_mirror(z, 0x76C, dy);
        ROWT(T_YBL, k) = road_proj_y_mirror(z, sub16(2000, far_take(DS_seg_bank_l_ptr, 2)), dy);   /* banks forward (sic) */
        ROWT(T_YBR, k) = road_proj_y_mirror(z, sub16(2000, far_take(DS_seg_bank_r_ptr, 2)), dy);
        ROWT(T_XFL, k) = road_proj_x_mirror(z, (s16)((w - 0x177) * -4), dx);
        ROWT(T_XFR, k) = road_proj_x_mirror(z, (s16)((w + 0x339) * 4), dx);
        if (k != 0) z = add16(z, 8);
    }
}

/* ================================================================================ pages, clip */

/* 0dcb:3516 road_pages_init */
void road_pages_init(void)
{
    if (DSS(DS_driver_id) == -2) {
        DSS(DS_back_page) = 1;
        DSW(DS_draw_seg) = 0xA200;
        DSS(DS_shown_page) = 2;
        ega_show_page(2);
    } else {
        FarPtr p = ds_far(DS_page1_ptr);
        DSW(DS_draw_seg) = rd16(p.seg, (u16)(p.off + 8));
        DSW(DS_draw_off) = rd16(p.seg, (u16)(p.off + 6));
    }
}

/* 0dcb:354e road_page_flip */
void road_page_flip(void)
{
    if (DSS(DS_driver_id) == -2) {
        s16 t = DSS(DS_back_page);
        DSS(DS_back_page) = DSS(DS_shown_page);
        DSS(DS_shown_page) = t;
        DSW(DS_draw_seg) = DSS(DS_back_page) == 1 ? 0xA200 : 0xA400;
        ega_show_page(DSS(DS_shown_page));
    } else {                                                /* other drivers (parked) */
        Rect r;
        r.w = DSS(DS_view_copy_rect + 0);  r.h = DSS(DS_view_copy_rect + 2);
        r.sx = DSS(DS_view_copy_rect + 4); r.sy = DSS(DS_view_copy_rect + 6);
        r.dx = DSS(DS_view_copy_rect + 8); r.dy = DSS(DS_view_copy_rect + 10);
        drv_blit(ds_far(DS_page1_ptr), ds_far(DS_g_front), &r, 1);
        FarPtr p = ds_far(DS_page1_ptr);
        DSW(DS_draw_seg) = rd16(p.seg, (u16)(p.off + 8));
        DSW(DS_draw_off) = rd16(p.seg, (u16)(p.off + 6));
    }
}

/* 0dcb:35c4 blit_clip_y */
void blit_clip_y(Rect *r)
{
    if (r->dy < YMIN) {
        s16 d = sub16(YMIN, r->dy);
        r->h = sub16(r->h, d);
        r->sy = add16(r->sy, d);
        r->dy = YMIN;
    }
    s16 e = add16(r->dy, r->h);
    if (YMAX < e) r->h = sub16(r->h, sub16(e, YMAX));
}

/* 0dcb:3602 blit_clip_xy: inclusive bottom (+1), left side srcx += d + 7 */
void blit_clip_xy(Rect *r)
{
    if (r->dy < YMIN) {
        s16 d = sub16(YMIN, r->dy);
        r->h = sub16(r->h, d);
        r->sy = add16(r->sy, d);
        r->dy = YMIN;
    }
    if (YMAX < add16(r->dy, r->h)) r->h = (s16)(YMAX - r->dy + 1);
    if (r->dx < XMIN) {
        s16 d = sub16(XMIN, r->dx);
        r->w = sub16(r->w, d);
        r->sx = add16(r->sx, add16(d, 7));
        r->dx = XMIN;
    }
}

/* 0dcb:3664 road_vline */
void road_vline(s16 colour, s16 x, s16 y1, s16 y2)
{
    if (y2 < YMIN || y1 > YMAX || x < XMIN || x > XMAX) return;
    if (y1 < YMIN) y1 = YMIN;
    if (y1 > YMAX) y1 = YMAX;
    if (y2 < YMIN) y2 = YMIN;
    if (y2 > YMAX) y2 = YMAX;
    drv_line(x, y1, x, y2, colour);
}

void clip_set_ymin0(void) { DSS(DS_clip_ymin) = 0; }            /* 0dcb:36ce */

/* 0dcb:36d6 clip_set_mirror */
void clip_set_mirror(void)
{
    DSS(DS_clip_xmin) = 0xF0;
    DSS(DS_clip_ymin) = 0x16;
    DSS(DS_clip_xmax) = 0x13F;
    DSS(DS_clip_ymax) = 0x2A;
}

/* 0dcb:36f0 clip_set_view */
void clip_set_view(void)
{
    DSS(DS_clip_xmin) = 0;
    DSS(DS_clip_ymin) = 0x12;
    DSS(DS_clip_ymax) = 0x65;
    DSS(DS_clip_xmax) = 0x13F;
}

/* ================================================================================ graphics */

/* 0dcb:370a mirror_bg_prepare: mirror frame (and sky) pre-blitted into page 2 at (232, 102) */
void mirror_bg_prepare(s16 with_sky)
{
    FarPtr p = pic_get(2005, 2);
    Rect r = { 80, 27, 0, 0, 232, 102 };
    drv_blit(p, ds_far(DS_page2_ptr), &r, 1);
    arena_pop(1);
    if (with_sky) {
        p = pic_get(MODE == 0x10 ? 2037 : 2063, 2);
        r.h = (s16)desc_h(p); r.sx = 0; r.sy = 0; r.dy = 102 + 27;
        drv_blit(p, ds_far(DS_page2_ptr), &r, 1);
        arena_pop(1);
    }
}

/* 0dcb:37ce road_load_graphics */
void road_load_graphics(void)
{
    Rect r;
    FarPtr p;
    if (!DSB(DS_racing) || MODE != 0x10) {
        p = pic_get(2002, 2);                               /* sky */
        r = (Rect){ (s16)desc_w(p), (s16)desc_h(p), 0, 0, 0, 0x66 };
        drv_blit(p, ds_far(DS_page1_ptr), &r, 1);
        arena_pop(1);
        ds_far_wr(DS_horizon_pic, pic_get(2055, 1));        /* horizon panorama */
        if (!DSB(DS_racing)) {                              /* cruising: rooftops */
            ds_far_wr(DS_rooftops_pic, pic_get(2004, 1));
            FarPtr s = ds_far(DS_rooftops_pic);
            r = (Rect){ (s16)desc_w(s), (s16)desc_h(s), 0, 0, 0x2C, 0x0C };
            drv_blit_masked(s, ds_far(DS_horizon_pic), &r, 1);
            arena_pop_low(1);
        }
        p = ds_far(DS_horizon_pic);
        r = (Rect){ (s16)desc_w(p), (s16)desc_h(p), 0, 0, 0, (s16)(200 - desc_h(p)) };
    } else {                                                /* racing on Mulholland drive */
        p = pic_get(2056, 1);
        ds_far_wr(DS_horizon_pic, p);
        r = (Rect){ 320, 98, 0, 0, 0, 0x66 };
    }
    drv_blit(p, ds_far(DS_page1_ptr), &r, 1);
    arena_pop_low(1);
    s16 loc = DSB(DS_racing) ? 0 : DSS(DS_location);
    if (loc == 4) loc = 2;
    p = pic_get(DSS((u16)(DS_sign_sheet_ids + 2 * loc)), 2);
    r = (Rect){ 0x108, 0x2B, 0, 0, 0, 0 };
    FarPtr dst = ds_far(DS_page2_ptr);
    if (DSS(DS_driver_id) == -2) { r.dy = 100; dst = ds_far(DS_page0_ptr); }
    drv_blit(p, dst, &r, 1);
    arena_pop(1);
    if (!DSB(DS_racing)) {
        ds_far_wr(DS_billboard_pics, pic_get(2049, 2));
        ds_far_wr(DS_billboard_pics + 4, pic_get(2050, 2));
    }
    ds_far_wr(DS_pic_hood, pic_get(DSS((u16)(DS_hood_pics + 6 * DSS(DS_dash_type))), 1));
    ds_far_wr(DS_pic_pillar, pic_get(2003, 1));
    mirror_bg_prepare(1);
    if (!DSB(DS_racing)) return;
    s16 cls = OVL_B0(DSS(DS_opp_model));
    u16 e = (u16)(DS_opp_sprite_sets + 14 * cls);
    start_props_load();
    if (DSW(DS_demo) == 0 || !(MODE & 2)) {
        ds_far_wr(DS_opp_sprites + 0, pic_get(DSS(e), 1));
        ds_far_wr(DS_opp_sprites + 4, pic_get(DSS((u16)(e + 2)), 1));
        ds_far_wr(DS_opp_sprites + 8, pic_get(DSS((u16)(e + 4)), 2));
        u16 ra = DSW((u16)(e + 6)), rb = DSW((u16)(e + 8)), rc = DSW((u16)(e + 12));
        for (s16 k = 0; k < 3; k++)
            ds_far_wr((u16)(DS_opp_sprites + 12 + 4 * k), drv_shrink(ds_far(DS_opp_sprites + 8), DSB((u16)(ra + 9 + k))));
        for (s16 k = 0; k < 3; k++)
            ds_far_wr((u16)(DS_opp_sprites + 24 + 4 * k), drv_shrink(ds_far(DS_opp_sprites + 20), DSB((u16)(rb + 9 + k))));
        ds_far_wr(DS_opp_sprites + 36, pic_get(DSS((u16)(e + 10)), 1));
        for (s16 k = 0; k < 3; k++)
            ds_far_wr((u16)(DS_opp_sprites + 40 + 4 * k), drv_shrink(ds_far(DS_opp_sprites + 36), DSB((u16)(rc + 9 + k))));
    }
    u16 pz = DSW(DS_police_sprite_set);
    ds_far_wr(DS_police_sprites + 0, ds_far(DS_opp_sprites));
    ds_far_wr(DS_police_sprites + 4, pic_get(DSS((u16)(pz + 2)), 1));
    ds_far_wr(DS_police_sprites + 8, pic_get(DSS((u16)(pz + 4)), 1));
    {
        FarPtr s = drv_shrink(ds_far(DS_police_sprites + 8), DSB(DSW((u16)(pz + 6))));
        ds_far_wr(DS_police_sprites + 8, s);
        ds_far_wr(DS_police_sprites + 12, s);
    }
    arena_pop_low(1);
    for (s16 k = 0; k < 3; k++) ds_far_wr((u16)(DS_police_sprites + 16 + 4 * k), ds_far(DS_police_sprites + 12));
    ds_far_wr(DS_police_sprites + 36, pic_get(DSS((u16)(pz + 10)), 1));
    u16 prc = DSW((u16)(pz + 12));
    for (s16 k = 0; k < 3; k++)
        ds_far_wr((u16)(DS_police_sprites + 40 + 4 * k), drv_shrink(ds_far(DS_police_sprites + 36), DSB((u16)(prc + k))));
}

/* ================================================================================ sky */

static void sky_scroll(int sh, s16 *a, s16 *b)
{
    s16 h = DSS(DS_heading);
    if (h > 0) {
        s16 m = (s16)(cmod16(shl16(h, sh), 0x140) & 0xFFF8);
        *a = m;  *b = sub16(0x140, m);
    } else {
        s16 m = (s16)(cmod16(shl16(abs16(h), sh), 0x140) & 0xFFF8);
        *b = m;  *a = sub16(0x140, m);
    }
}

/* 0dcb:3e66 road_draw_sky */
static void road_draw_sky(void)
{
    Rect r;
    s16 hy = 0, a = 0, b = 0;
    r.dy = 0x12;
    bool race = DSB(DS_racing) != 0;
    if (!race) {
        r.w = 0x140; r.sy = 0x66; r.dx = 0; r.h = 0x1D;
    } else {
        s16 dy = (MODE & 6) ? 0 : sub16(DSS(DS_cur_height), TRKS(TRK_HGT, DSS(DS_seg) - 1));
        hy = road_proj_y(0xF0, 2000, dy);
        if (hy > 0x51 || hy < 0x13) hy = hy < 0x12 ? 0x12 : 0x52;    /* quirk: 0x12 becomes 0x52 */
        r.h = sub16(hy, 0x1E);
        r.sy = (s16)-(hy - 0xB8);
        sky_scroll(1, &a, &b);
        r.w = b; r.dx = a;
    }
    r.sx = 0;
    drv_blit(ds_far(DS_page1_ptr), back_page(), &r, 1);
    if (race) {
        r.w = a; r.sx = b; r.dx = 0;
        if (hy > 0x12) drv_blit(ds_far(DS_page1_ptr), back_page(), &r, 1);
    }
    r.h = race ? 0x14 : 0x10;
    r.sy = 0xA3;
    if (!race) { r.w = 0x140; r.dx = 0; r.dy = 0x2F; }
    else {
        sky_scroll(2, &a, &b);
        r.w = b; r.dx = a; r.dy = sub16(hy, 0x0C);
    }
    r.sx = 0;
    blit_clip_y(&r);
    drv_blit(ds_far(DS_page1_ptr), back_page(), &r, 1);
    DSS(DS_top_band_row) = 0x0E;
    if (!race) { drv_fill_rows(0x3F, 1); return; }
    r.w = a; r.sx = b; r.dx = 0;
    drv_blit(ds_far(DS_page1_ptr), back_page(), &r, 1);
    DSS(DS_ground_top_y) = add16(hy, 8);
    for (s16 k = 14; k != 0; k--) {
        s16 yk = ROWT(T_Y, k);
        if (yk < DSS(DS_ground_top_y)) { DSS(DS_ground_top_y) = yk; DSS(DS_top_band_row) = k; }
    }
    s16 top = DSS(DS_ground_top_y);
    drv_fill_rows(top < 0x12 ? 0x12 : top, 9);
}

/* 0dcb:3cca road_draw_sky_mulholland */
static void road_draw_sky_mulholland(void)
{
    s16 dy = sub16(DSS(DS_cur_height), TRKS(TRK_HGT, DSS(DS_seg) - 4));
    if (dy > 8) dy = 8;
    else if (dy < -8 && DSS(DS_fall_dir) == 0) dy = -8;
    s16 hy = road_proj_y(0xF0, 2000, dy);
    Rect r;
    r.sy = (s16)(0x6E - dy);
    r.h = (s16)(199 - r.sy);
    if (r.h > 0x53) r.h = 0x53;
    r.dy = 0x12;
    s16 a, b;
    sky_scroll(1, &a, &b);
    r.w = b; r.sx = 0; r.dx = a;
    blit_clip_y(&r);
    drv_blit(ds_far(DS_page1_ptr), back_page(), &r, 1);
    r.w = a; r.sx = b; r.dx = 0;
    drv_blit(ds_far(DS_page1_ptr), back_page(), &r, 1);
    s16 bottom = (s16)(r.h + 0x12);
    DSS(DS_ground_top_y) = (s16)(hy + 8);
    DSS(DS_top_band_row) = 14;
    for (s16 k = 14; k != 0; k--)
        if (ROWT(T_Y, k) < DSS(DS_ground_top_y)) { DSS(DS_ground_top_y) = ROWT(T_Y, k); DSS(DS_top_band_row) = k; }
    if (DSS(DS_fall_dir) != 0 && bottom < 0x65) drv_fill_rows(bottom, 0x0E);
}

/* ================================================================================ polygons */

/* 0dcb:58f4 road_poly_fill (pts12 = 6 points, n = 3, 4 or 6) */
void road_poly_fill(const s16 *pts12, s16 n, s16 colour)
{
    s16 buf[128], cnt;
    if (clip_polygon(pts12, n, buf, &cnt)) {
        poly_fill_pts(buf, cnt, colour, colour);
        road_fillmode_begin();
    }
}

static void poly3(s16 c, s16 x0, s16 y0, s16 x1, s16 y1, s16 x2, s16 y2)
{
    s16 p[12] = { x0, y0, x1, y1, x2, y2, 0, 0, 0, 0, 0, 0 };
    road_poly_fill(p, 3, c);
}
static void poly4(s16 c, s16 x0, s16 y0, s16 x1, s16 y1, s16 x2, s16 y2, s16 x3, s16 y3)
{
    s16 p[12] = { x0, y0, x1, y1, x2, y2, x3, y3, 0, 0, 0, 0 };
    road_poly_fill(p, 4, c);
}
static void poly6(s16 c, s16 x0, s16 y0, s16 x1, s16 y1, s16 x2, s16 y2, s16 x3, s16 y3, s16 x4, s16 y4, s16 x5, s16 y5)
{
    s16 p[12] = { x0, y0, x1, y1, x2, y2, x3, y3, x4, y4, x5, y5 };
    road_poly_fill(p, 6, c);
}

/* 0dcb:4268 slope16 */
static s16 slope16(s16 xa, s16 ya, s16 xb, s16 yb)
{
    if (yb == ya) return 0;
    return cdiv16(shl16(sub16(xb, xa), 4), sub16(yb, ya));
}

/* 0dcb:4294 quad_fill: left side xa (ya1 top .. ya0), right side xb (yb1 .. yb0) */
static void quad_fill(s16 c, s16 xa, s16 xb, s16 ya0, s16 yb0, s16 yb1, s16 ya1)
{
    s16 pts[8] = { xa, ya0, xb, yb0, xb, yb1, xa, ya1 };
    s16 buf[128], cnt;
    if (clip_polygon(pts, 4, buf, &cnt)) poly_fill_pts(buf, cnt, c, c);
    road_fillmode_begin();
}

/* ================================================================================ road bands */

/* 0dcb:42fe sign */
static s16 sgn16(s16 v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

/* PORT: road_draw_band_front compares the remaining line count with a never-written local (SR1 §8) to draw an
 * extra colour-2 line on town shoulders of rows < 6; treated as never equal (as the SR1 port). */
static inline bool band_g_match(s16 n) { (void)n; return false; }

/* 0dcb:4322 road_draw_band_front */
static void road_draw_band_front(s16 row)
{
    s16 xl = rv(DS_rp_xl), xr = rv(DS_rp_xr);
    s16 axl = shl16(xl, 4), axr = shl16(xr, 4);
    s16 y = rv(DS_rp_y), yq = rv(DS_rq_y);
    s16 sl = slope16(rv(DS_rq_xl), yq, xl, y);
    s16 sr = slope16(rv(DS_rq_xr), yq, xr, y);
    u16 C = fw(DS_centre_ptr, 0);
    u8 cb = (u8)C;
    bool outer = (cb & 1) || MODE == 0x10;
    s16 xol = 0, xor_ = 0, axol = 0, axor = 0, sol = 0, sor = 0;
    if (outer) {
        xol = rv(DS_rp_xol); xor_ = rv(DS_rp_xor);
        axol = shl16(xol, 4); axor = shl16(xor_, 4);
        sol = slope16(rv(DS_rq_xol), yq, xol, y);
        sor = slope16(rv(DS_rq_xor), yq, xor_, y);
    }
    s16 w = 0, dashx = 0;
    bool wide = (MODE & 6) && DSS(DS_front_row_seg) < (s16)(DSS(DS_start_seg) + 5);
    if (C & 8) {
        w = wide ? shl16(DSS(DSW(DS_dash_ptr)), 3) : DSS(DSW(DS_dash_ptr));
        dashx = sub16((s16)(add16(xl, xr) >> 1), (s16)(w >> 1));
    }
    if (sgn16(sub16(yq, y)) < 0) return;
    s16 n = sub16(yq, y);
    s16 L16 = 0, kk = 0;   /* PORT: uninitialised in the original for rows <= 9 (then unused) */
    for (; n != 0 && y < 0x66; y++, n--) {
        if (!(cb & 1)) {
            if (MODE == 0x10 && !(fw(DS_centre_ptr, 0) & 0x80) &&
                sub16(TRKS(TRK_HGT, DSS(DS_seg)), 0x0B) <= DSS(DS_cur_height)) {
                hl(2, xol, xl, y);
                drv_hspan(xr, xor_, y);
            }
            drv_set_colour(9);
            if (!(MODE & 0x10)) {
                drv_hspan(0, xl, y);
                drv_hspan(xr, 0x13F, y);
            } else {
                if (row > 9) {                              /* recomputed every line (96F0 net unchanged) */
                    L16 = 0;
                    for (s16 k = 14; k > 9; k--) {
                        s16 s = (s16)(DSS(DS_seg) + k);
                        u16 ck = fw(DS_seg_centre_ptr, k), cm1 = fw(DS_seg_centre_ptr, (s16)(k - 1));
                        (void)s;
                        if ((MODE & 0x10) && !(ck & 0x80) && (cm1 & 0x80)) L16 = 1;
                    }
                    kk = (s16)(0x10 - row);
                }
                s16 ext = DSS((u16)(DS_hill_peak_tab + 4 + 4 * kk));     /* word DS:7E22 + 4 kk */
                if (rv(DS_rq_yt1) == yq) drv_hspan((L16 == 0 || row < 10) ? 0 : (s16)(xl - ext), xl, y);
                if (rv(DS_rq_yt2) == yq) drv_hspan(xr, (L16 == 0 || row < 10) ? 0x13F : (s16)(ext + xr), y);
            }
            u16 cf = fw(DS_centre_ptr, 0);
            bool white = (cf & 0x10) && (MODE != 0x10 || DSS(DS_seg) >= 0x3AD);
            hl(white ? 0x0F : 1, xl, xr, y);
        } else {
            if (fw(DS_centre_ptr, 0) & 4) return;           /* intersection */
            if (!(fw(DS_left_ptr, 0) & 0x80)) {
                hl(4, xol, xl, y);
                if (band_g_match(n) && row < 6) hl(2, xol, xl, y);
            } else {
                s16 m = add16((s16)(sub16(xl, xol) >> 1), xol);
                hl(4, xol, m, y);
                hl(9, m, xl, y);
                if (band_g_match(n) && row < 6) hl(2, xol, m, y);
            }
            if (!(fw(DS_right_ptr, 0) & 0x80)) {
                hl(4, xr, xor_, y);
            } else {
                s16 m = (s16)-(s16)(((s16)(xor_ - xr) >> 1) - xor_);
                hl(4, m, xor_, y);
                hl(9, xr, m, y);
            }
        }
        if (fw(DS_centre_ptr, 0) & 8) {
            drv_set_colour(wide ? 2 : 4);
            if (xl < 0x13F) drv_hspan(dashx, add16(dashx, w), y);
        }
        axl = add16(axl, sl); axr = add16(axr, sr);
        xl = (s16)(axl >> 4); xr = (s16)(axr >> 4);
        if (outer) {
            axol = add16(axol, sol); axor = add16(axor, sor);
            xol = (s16)(axol >> 4); xor_ = (s16)(axor >> 4);
        }
        dashx = sub16((s16)(add16(xl, xr) >> 1), (s16)(w >> 1));
    }
}

/* 0dcb:4852 road_draw_band_mirror */
static void road_draw_band_mirror(void)
{
    s16 xl = rv(DS_rp_xl), xr = rv(DS_rp_xr);
    s16 axl = shl16(xl, 4), axr = shl16(xr, 4);
    s16 y = rv(DS_rp_y), yq = rv(DS_rq_y);
    s16 sl = slope16(rv(DS_rq_xl), yq, xl, y);
    s16 sr = slope16(rv(DS_rq_xr), yq, xr, y);
    u8 cb = (u8)fw(DS_centre_ptr, 0);
    s16 xol = 0, xor_ = 0, axol = 0, axor = 0, sol = 0, sor = 0;
    if ((cb & 1) || (MODE & 0x10)) {
        xol = rv(DS_rp_xol); xor_ = rv(DS_rp_xor);
        axol = shl16(xol, 4); axor = shl16(xor_, 4);
        sol = slope16(rv(DS_rq_xol), yq, xol, y);
        sor = slope16(rv(DS_rq_xor), yq, xor_, y);
    }
    s16 w = 0, dashx = 0;
    bool wide = (MODE & 6) && DSS(DS_mirror_row_seg) < (s16)(DSS(DS_start_seg) + 5);
    if (fw(DS_centre_ptr, 0) & 8) {
        w = wide ? shl16(DSS(DSW(DS_dash_ptr)), 2) : DSS(DSW(DS_dash_ptr));
        dashx = sub16((s16)(add16(xl, xr) >> 1), (s16)(w >> 1));
    }
    if (sgn16(sub16(yq, y)) < 0) return;
    s16 n = sub16(yq, y);
    for (; n != 0 && y < 0x2B; y++, n--) {
        s16 a, b;
        if (!(cb & 1)) {
            if (MODE == 0x10 && !(fw(DS_centre_ptr, 0) & 0x80) &&
                sub16(TRKS(TRK_HGT, DSS(DS_seg)), 0x0B) <= DSS(DS_cur_height)) {
                hl(2, (s16)(xol - 6), xl, y);
                drv_hspan(xr, (s16)(xor_ + 6), y);
            }
            drv_set_colour(9);
            if (!(MODE & 0x10)) {
                drv_hspan(0xF0, xl, y);
                drv_hspan(xr, 0x13F, y);
            } else {
                if (rv(DS_rq_yt1) == yq) drv_hspan(0xF0, xl, y);
                if (rv(DS_rq_yt2) == yq) drv_hspan(xr, 0x13F, y);
            }
            u16 cf = fw(DS_centre_ptr, 0);
            bool white = (cf & 0x10) && ((MODE != 0x10 && DSS(DS_seg) >= 0x169) || (MODE == 0x10 && DSS(DS_seg) >= 0x3AD));
            drv_set_colour(white ? 0x0F : 1);
            a = xl; b = xr;
        } else {
            if (fw(DS_centre_ptr, 0) & 4) return;
            s16 m;
            drv_set_colour(4);
            if (!(fw(DS_left_ptr, 0) & 0x80)) m = xol;
            else {
                m = add16((s16)(sub16(xl, xol) >> 1), xol);
                drv_hspan(xol, m, y);
                drv_set_colour(9);
            }
            drv_hspan(m, xl, y);
            a = xr;
            drv_set_colour(4);
            if (!(fw(DS_right_ptr, 0) & 0x80)) b = xor_;
            else {
                b = (s16)-(s16)(((s16)(xor_ - xr) >> 1) - xor_);
                drv_hspan(b, xor_, y);
                drv_set_colour(9);
            }
        }
        drv_hspan(a, b, y);
        if (fw(DS_centre_ptr, 0) & 8) {
            drv_set_colour(wide ? 2 : 4);
            drv_hspan(dashx, add16(dashx, w), y);
        }
        axl = add16(axl, sl); axr = add16(axr, sr);
        xl = (s16)(axl >> 4); xr = (s16)(axr >> 4);
        if (cb & 1) {
            axol = add16(axol, sol); axor = add16(axor, sor);
            xol = (s16)(axol >> 4); xor_ = (s16)(axor >> 4);
        }
        dashx = sub16((s16)(add16(xl, xr) >> 1), (s16)(w >> 1));
    }
}

/* ================================================================================ town walls */

/* 0dcb:4c1c town_walls_front */
static void town_walls_front(s16 a, s16 b, s16 c, s16 d, s16 e, s16 f, s16 left)
{
    u16 fp = left ? DS_left_ptr : DS_right_ptr;
    u16 F = fw(fp, 0), Fm1 = fw(fp, -1);
    if (fw(DS_centre_ptr, 0) & 4) return;
    quad_fill((F & 0x10) ? 2 : 3, a, b, c, d, e, f);
    if (F & 4) {
        s16 k = (F & 8) ? 1 : 2;
        quad_fill(4, a, b, f, e, (s16)(cdiv16(sub16(e, d), k) + e), (s16)(cdiv16(sub16(f, c), k) + f));
    }
    if ((u8)fw(DS_centre_ptr, -1) & 4) {
        if (left) hl(4, XMIN, a, sub16(c, 1)); else hl(4, b, XMAX, sub16(d, 1));
        drv_set_colour(2);
        if (!left) for (s16 y = e; y < sub16(d, 1); y++) drv_hspan(b, XMAX, y);
        else       for (s16 y = f; y < sub16(c, 1); y++) drv_hspan(XMIN, a, y);
    }
    if (!(F & 4) || (Fm1 & 4)) return;
    drv_set_colour(2);
    if (!(F & 8)) {
        if (!left) { for (s16 y = (s16)-((s16)(sub16(d, e) >> 1) - e); y < e; y++) drv_hspan(add16(b, 1), sub16(add16(b, b), a), y); }
        else       { for (s16 y = (s16)-((s16)(sub16(c, f) >> 1) - f); y < f; y++) drv_hspan(sub16(add16(a, a), b), sub16(a, 1), y); }
    } else {
        if (!left) { for (s16 y = sub16(add16(e, e), d); y < e; y++) drv_hspan(add16(b, 1), sub16(add16(b, b), a), y); }
        else       { for (s16 y = sub16(add16(f, f), c); y < f; y++) drv_hspan(sub16(add16(a, a), b), sub16(a, 1), y); }
    }
}

/* 0dcb:4e70 town_walls_mirror */
static void town_walls_mirror(s16 a, s16 b, s16 c, s16 d, s16 e, s16 f, s16 left)
{
    if (!(fw(DS_centre_ptr, 0) & 1)) return;
    u16 fp = left ? DS_left_ptr : DS_right_ptr;
    u16 F4 = fw(fp, 0) & 4, Fn = fw(fp, 1);
    if (fw(DS_centre_ptr, 0) & 4) return;
    quad_fill((fw(fp, 0) & 0x10) ? 2 : 3, a, b, c, d, e, f);
    if (F4) {
        s16 k = (s16)(2 - (left == 0));
        quad_fill(4, a, b, f, e, (s16)(cdiv16(sub16(e, d), k) + e), (s16)(cdiv16(sub16(f, c), k) + f));
    }
    if ((u8)fw(DS_centre_ptr, 1) & 4) {
        if (left) hl(4, XMIN, a, sub16(c, 1)); else hl(4, b, XMAX, sub16(d, 1));
        drv_set_colour(2);
        if (!left) for (s16 y = e; y < sub16(d, 1); y++) drv_hspan(b, XMAX, y);
        else       for (s16 y = f; y < sub16(c, 1); y++) drv_hspan(XMIN, a, y);
    }
    if (!F4 || (Fn & 4)) return;
    drv_set_colour(2);
    if (!left) { for (s16 y = sub16(add16(e, e), d); y < e; y++) drv_hspan(add16(b, 1), sub16(add16(b, b), a), y); }
    else       { for (s16 y = (s16)-((s16)(sub16(c, f) >> 1) - f); y < f; y++) drv_hspan(sub16(add16(a, a), b), sub16(a, 1), y); }
}

/* 0dcb:504a / 507a / 50a8 / 50c4 / 50f4 / 5122 */
static void town_walls_front_left(void)
{
    town_walls_front(rv(DS_rq_xol), rv(DS_rp_xol), rv(DS_rq_y), rv(DS_rp_y), rv(DS_rp_ye), rv(DS_rq_ye), 1);
}
static void town_walls_front_right(void)
{
    town_walls_front(rv(DS_rp_xor), rv(DS_rq_xor), rv(DS_rp_y), rv(DS_rq_y), rv(DS_rq_ye), rv(DS_rp_ye), 0);
}
static void road_draw_row_front(s16 row)
{
    if (!DSB(DS_racing)) {
        town_walls_front_left();
        town_walls_front_right();
    }
    road_draw_band_front(row);
}
static void town_walls_mirror_left(void)
{
    town_walls_mirror(rv(DS_rq_xol), rv(DS_rp_xol), rv(DS_rq_y), rv(DS_rp_y), rv(DS_rp_ye), rv(DS_rq_ye), 1);
}
static void town_walls_mirror_right(void)
{
    town_walls_mirror(rv(DS_rp_xor), rv(DS_rq_xor), rv(DS_rp_y), rv(DS_rq_y), rv(DS_rq_ye), rv(DS_rp_ye), 0);
}
static void road_draw_row_mirror(void)
{
    town_walls_mirror_left();
    town_walls_mirror_right();
    road_draw_band_mirror();
}

/* ================================================================================ scenery */

/* 0dcb:512e shape_draw_dot: 5x5 cyan disc (UI: steering-wheel knob) */
void shape_draw_dot(s16 x, s16 y)
{
    road_fillmode_begin();
    shape_draw(x, y, 0, ds_ptr(DS_dot_shape), 1);
    road_fillmode_end();
}

/* 0dcb:5158 scenery_draw_shape */
void scenery_draw_shape(s16 x, s16 y, s16 depth, s16 side, s16 type, s16 noclamp)
{
    s16 lod = sub16(9, depth);
    if (lod > 6) lod = 6;
    if (lod < 0) lod = 0;
    if (noclamp == 0 && y > 0x65) y = 0x65;
    shape_draw(x, y, lod, ds_far((u16)(DS_shp_a_ptrs + 4 * type)), side);
}

/* 0dcb:51aa scenery_draw_hydrant (set 2, 8 LODs, no y clamp) */
static void scenery_draw_hydrant(s16 x, s16 y, s16 depth, s16 side)
{
    s16 lod = sub16(9, depth);
    if (lod > 7) lod = 7;
    if (lod < 0) lod = 0;
    shape_draw(x, y, lod, ds_far((u16)(DS_shp_a_ptrs + 4 * 2)), side);
}

/* 0dcb:51e0 scenery_draw_bush_group */
static void scenery_draw_bush_group(s16 x, s16 y, s16 depth, s16 side)
{
    s16 d2 = (s16)(depth + 2);
    if (depth == 0) x = (s16)(x + side * 100);
    if (depth == 1) x = (s16)(x + side * 40);
    if (depth == 2) x = (s16)(x + side * 10);
    s16 W = DSS((u16)(DS_bush_group_hw + 4 * depth + 2)), H = DSS((u16)(DS_bush_group_hw + 4 * depth));
    s16 A = DSS((u16)(DS_bush_group_dx + 4 * depth)), B2 = DSS((u16)(DS_bush_group_dx + 4 * d2 + 2));
    scenery_draw_shape(x, y, depth, side, 8, 1);
    scenery_draw_shape((s16)(W * side + x), y, depth, side, 7, 1);
    scenery_draw_shape((s16)((W + A) * side + x), y, depth, side, 8, 1);
    scenery_draw_shape((s16)((W >> 1) * side + x), (s16)(y - H), d2, side, 7, 1);
    scenery_draw_shape((s16)(((W >> 1) + B2) * side + x), (s16)(y - H), d2, side, 8, 1);
}

/* 0dcb:52e2 scenery_draw_sign_picture: kind 1 location sign sheet, 2 / 3 town billboards */
static void scenery_draw_sign_picture(s16 x, s16 y, s16 depth, s16 align, s16 xclip, s16 kind)
{
    s16 size = sub16(9, depth);
    if (size > 6) size = 6;
    if (size < 0) size = 0;
    u16 e = (u16)(DS_sign_sizes + 6 * size);
    Rect r;
    r.w = DSS((u16)(e + 2));
    r.h = DSS((u16)(e + 4));
    r.sx = DSS(e);
    r.sy = 0;
    FarPtr src = far_make(0, 0);                   /* PORT: other kinds leave the source uninitialised (never used) */
    if (kind == 1) {
        if (DSS(DS_driver_id) == -2) { r.sy = 100; src = ds_far(DS_page0_ptr); }
        else                         { r.sy = 0;   src = ds_far(DS_page2_ptr); }
    } else if (kind == 2) src = ds_far(DS_billboard_pics);
    else if (kind == 3)   src = ds_far(DS_billboard_pics + 4);
    else return;
    if (align == 1)      x = sub16(x, r.w);
    else if (align == 0) x = sub16(x, (s16)(r.w >> 1));
    r.dx = x;
    r.dy = (s16)-(cdiv16(r.h, (s16)(align == 0 ? 2 : 1)) - y);
    if (xclip == 0) blit_clip_y(&r); else blit_clip_xy(&r);
    drv_blit(src, back_page(), &r, 1);
}

/* 0dcb:55ce scenery_draw_banner */
static void scenery_draw_banner(s16 xl, s16 xr, s16 ytop, s16 ybot, s16 depth, s16 xclip, u16 flags)
{
    for (s16 si = add16(DSS(DSW(DS_dash_ptr)), 1); si != 0; si--) {
        road_fillmode_end();
        road_vline(10, sub16(xl, si), ytop, ybot);
        road_vline(10, add16(xr, si), ytop, ybot);
        road_fillmode_begin();
        hl(10, xl, xr, (s16)(si + ytop - 1));
    }
    s16 mid = (s16)(add16(xl, xr) >> 1);
    if (flags & 0x1000) { scenery_draw_shape(mid, ytop, depth, 1, 0x0D, 0); return; }
    s16 kind;
    if (!(flags & 0x2000)) {
        if (!(fw(DS_centre_ptr, 0) & 1)) { scenery_draw_shape(mid, ytop, depth, 1, (flags & 0x4000) ? 0x0C : 0x0E, 0); return; }
        road_fillmode_end();
        kind = (flags & 0x4000) ? 2 : 3;
    } else {
        road_fillmode_end();
        kind = 1;
    }
    scenery_draw_sign_picture(mid, ytop, depth, 0, xclip, kind);
    road_fillmode_begin();
}

/* 0dcb:5404 scenery_draw_truss (Mulholland C & 0x80) */
static void scenery_draw_truss(s16 xl1, s16 xr1, s16 yt1, s16 yb1, s16 xl2, s16 xr2, s16 yt2, s16 yb2, s16 parity, s16 near_flag)
{
    road_fillmode_end();
    for (s16 n = add16(DSS(DSW(DS_dash_ptr)), 1); n != 0; n--) {
        s16 a = sub16(xl1, n), b = add16(xr1, n), c = sub16(xl2, n), e = add16(xr2, n);
        clip_line(a, yt1, a, yb1, 10);
        clip_line(b, yt1, b, yb1, 10);
        clip_line(xl1, (s16)(yt1 + n - 1), xr1, (s16)(yt1 + n - 1), 10);
        if (cmod16(parity, 2) == 0 || near_flag == 0) { clip_line(a, yb1, c, yt2, 10); clip_line(b, yb1, e, yt2, 10); }
        else                                         { clip_line(a, yt1, c, yb2, 10); clip_line(b, yt1, e, yb2, 10); }
        clip_line(a, yt1, c, yt2, 10);
        clip_line(b, yt1, e, yt2, 10);
        clip_line(a, yb1, c, yb2, 10);
        clip_line(b, yb1, e, yb2, 10);
    }
    road_fillmode_begin();
}

/* 0dcb:555e scenery_draw_truss_end */
static void scenery_draw_truss_end(s16 xl, s16 xr, s16 ytop, s16 ybot)
{
    road_fillmode_end();
    for (s16 n = add16(DSS(DSW(DS_dash_ptr)), 1); n != 0; n--) {
        clip_line(sub16(xl, n), ytop, sub16(xl, n), ybot, 10);
        clip_line(add16(xr, n), ytop, add16(xr, n), ybot, 10);
        clip_line(xl, (s16)(ytop + n - 1), xr, (s16)(ytop + n - 1), 10);
    }
    road_fillmode_begin();
}

/* ================================================================================ overlays */

/* 0dcb:56e8 road_draw_cockpit_overlay: the A-pillar */
static void road_draw_cockpit_overlay(void)
{
    FarPtr p = ds_far(DS_pic_pillar);
    Rect r = { (s16)desc_w(p), (s16)desc_h(p), 0, 0, 0, 0x12 };
    drv_blit_masked(p, back_page(), &r, 1);
}

/* 0dcb:573e road_draw_hood_overlay */
static void road_draw_hood_overlay(void)
{
    s16 t = DSS(DS_dash_type);
    FarPtr p = ds_far(DS_pic_hood);
    Rect r = { (s16)desc_w(p), (s16)desc_h(p), 0, 0, DSS((u16)(DS_hood_pics + 6 * t + 2)), DSS((u16)(DS_hood_pics + 6 * t + 4)) };
    drv_blit_masked(p, back_page(), &r, 1);
}

/* 0dcb:57a6 mirror_draw_backdrop */
void mirror_draw_backdrop(void)
{
    Rect r = { 0x10, 2, 0x108, 0x66, 0x110, 0x12 };
    drv_blit(ds_far(DS_page2_ptr), back_page(), &r, 1);
    r = (Rect){ 0x50, 0x19, 0xE8, 0x68, 0xF0, 0x14 };
    drv_blit(ds_far(DS_page2_ptr), back_page(), &r, 1);
    if (MODE != 8) {
        r = (Rect){ 0x50, (s16)(MODE == 0x10 ? 0x14 : 2), 0xE8, 0x81, 0xF0, (s16)(ROWT(T_Y, 7) - (MODE == 0x10 ? 10 : 2)) };
        blit_clip_xy(&r);
        drv_blit(ds_far(DS_page2_ptr), back_page(), &r, 1);
    } else
        aq_mirror_stars();
}

/* 0dcb:597e road_draw_barrier_walls (drag strip) */
static void road_draw_barrier_walls(s16 phase_seg, s16 n, s16 row)
{
    s16 v = (s16)(abs16((s16)(n - row - DSS(DS_seg) + phase_seg)) % 10);
    s16 c = v < 5 ? 0x0B : 4;
    poly4(c, rv(DS_rp_xr), rv(DS_rp_y79e), rv(DS_rp_xr), rv(DS_rp_y), rv(DS_rq_xr), rv(DS_rq_y), rv(DS_rq_xr), rv(DS_rq_y79e));
    poly4(c, rv(DS_rp_xl), rv(DS_rp_y79e), rv(DS_rp_xl), rv(DS_rp_y), rv(DS_rq_xl), rv(DS_rq_y), rv(DS_rq_xl), rv(DS_rq_y79e));
}

/* 0dcb:5a2c mirror_finish: frame lines, mirror centre -> page 0 */
void mirror_finish(void)
{
    clip_set_view();
    road_vline(2, 0xF0, 0x16, 0x2A);
    road_vline(4, 0xF1, 0x16, 0x2A);
    road_vline(4, 0x13E, 0x16, 0x2A);
    road_vline(2, 0x13F, 0x16, 0x2A);
    Rect r = { 0x20, 0x15, 0xF8, 0x16, 8, (s16)((DSS(DS_driver_id) == -2 ? 0 : 100) + 3) };
    drv_blit(back_page(), ds_far(DS_page0_ptr), &r, 1);
}

/* guard rail (6a32 / 5aee inline): right side; m = 1 in the mirror */
static void guard_rail(s16 row, s16 post, s16 n, s16 m)
{
    s16 buf[128], cnt;
    if (cmod16((s16)(DSS(DS_rail_phase_seg) - row - DSS(DS_seg) + n), 2) != 0) {
        s16 x = (s16)(post / (row + 1) + rv(DS_rq_xor));
        s16 v[8] = { x, (s16)(rv(DS_rq_y79e) - 1), x, rv(DS_rq_y), rv(DS_rq_xor), rv(DS_rq_y), rv(DS_rq_xor), (s16)(rv(DS_rq_y79e) - 1) };
        if (clip_polygon(v, 4, buf, &cnt)) poly_fill_pts(buf, cnt, 5, 2);
    }
    s16 w[8] = { rv(DS_rp_xor), (s16)(rv(DS_rp_ye) - m), rv(DS_rp_xor), rv(DS_rp_y79e), rv(DS_rq_xor), rv(DS_rq_y79e),
                 rv(DS_rq_xor), (s16)(rv(DS_rq_ye) - m) };
    if (clip_polygon(w, 4, buf, &cnt)) {
        poly_fill_pts(buf, cnt, 5, 5);
        clip_line(w[0], w[1], w[6], w[7], 2);
        clip_line(w[2], w[3], w[4], w[5], 2);
    }
    road_fillmode_begin();
}

/* ================================================================================ front view */

static void set_ptr(u16 ptr, u16 table, s16 row) { DSW(ptr) = (u16)(table + 2 * row); }

static void row_ptrs_init(s16 row)
{
    set_ptr(DS_rp_xl, T_XL, row);    set_ptr(DS_rq_xl, T_XL, row - 1);
    set_ptr(DS_rp_xr, T_XR, row);    set_ptr(DS_rq_xr, T_XR, row - 1);
    set_ptr(DS_rp_y, T_Y, row);      set_ptr(DS_rq_y, T_Y, row - 1);
    set_ptr(DS_rp_y79e, T_YK, row);  set_ptr(DS_rq_y79e, T_YK, row - 1);
    set_ptr(DS_rp_yt1, T_YBL, row);  set_ptr(DS_rq_yt1, T_YBL, row - 1);
    set_ptr(DS_rp_yt2, T_YBR, row);  set_ptr(DS_rq_yt2, T_YBR, row - 1);
    set_ptr(DS_rp_xfl, T_XFL, row);  set_ptr(DS_rq_xfl, T_XFL, row - 1);
    set_ptr(DS_rp_xfr, T_XFR, row);  set_ptr(DS_rq_xfr, T_XFR, row - 1);
    set_ptr(DS_rp_xol, T_XOL, row);  set_ptr(DS_rq_xol, T_XOL, row - 1);
    set_ptr(DS_rp_xor, T_XOR, row);  set_ptr(DS_rq_xor, T_XOR, row - 1);
    set_ptr(DS_rp_ye, T_YE, row);    set_ptr(DS_rq_ye, T_YE, row - 1);
}

static void row_ptrs_step(void)
{
    static const u16 ptrs[] = { DS_rp_xl, DS_rq_xl, DS_rp_xr, DS_rq_xr, DS_rp_y, DS_rq_y, DS_rp_y79e, DS_rq_y79e,
                                DS_rp_yt1, DS_rq_yt1, DS_rp_yt2, DS_rq_yt2, DS_rp_xfl, DS_rq_xfl, DS_rp_xfr, DS_rq_xfr,
                                DS_rp_xol, DS_rq_xol, DS_rp_xor, DS_rq_xor, DS_rp_ye, DS_rq_ye, DS_dash_ptr };
    for (unsigned i = 0; i < sizeof ptrs / sizeof ptrs[0]; i++) DSW(ptrs[i]) = (u16)(DSW(ptrs[i]) - 2);
}

static void flag_ptrs(s16 bytes)
{
    ds_far_wr(DS_left_ptr, far_add(ds_far(DS_seg_left_ptr), (u16)bytes));
    ds_far_wr(DS_right_ptr, far_add(ds_far(DS_seg_right_ptr), (u16)bytes));
    ds_far_wr(DS_centre_ptr, far_add(ds_far(DS_seg_centre_ptr), (u16)bytes));
    ds_far_wr(DS_x6_ptr, far_add(ds_far(DS_seg_lane_ptr), (u16)bytes));
}

static void flag_step(s16 bytes)
{
    fadv(DS_left_ptr, bytes); fadv(DS_right_ptr, bytes); fadv(DS_centre_ptr, bytes); fadv(DS_x6_ptr, bytes);
}

/* row value shorthands */
#define XLp   rv(DS_rp_xl)
#define XLq   rv(DS_rq_xl)
#define XRp   rv(DS_rp_xr)
#define XRq   rv(DS_rq_xr)
#define Yp    rv(DS_rp_y)
#define Yq    rv(DS_rq_y)
#define XOLp  rv(DS_rp_xol)
#define XOLq  rv(DS_rq_xol)
#define XORp  rv(DS_rp_xor)
#define XORq  rv(DS_rq_xor)
#define YEp   rv(DS_rp_ye)
#define YEq   rv(DS_rq_ye)
#define YT1p  rv(DS_rp_yt1)
#define YT1q  rv(DS_rq_yt1)
#define YT2p  rv(DS_rp_yt2)
#define YT2q  rv(DS_rq_yt2)
#define XFLp  rv(DS_rp_xfl)
#define XFRp  rv(DS_rp_xfr)
#define PEAK(base, k) DSS((u16)((base) + 4 * (k)))   /* DS:7E1E.. words, index k = 15 - row (reads run into 7E52) */

/* left / right hillsides of the front view (Mulholland, rows without C & 0x80) */
static void front_hillsides(s16 row, s16 L16)
{
    s16 o = DSS(DS_fall_dir);
    s16 dyaw = sub16(DSS(DS_heading), TRKS(TRK_YAW, DSS(DS_seg)));
    s16 k = (s16)(15 - row);
    s16 W0 = PEAK(0x7E1E, k), H0 = PEAK(0x7E20, k), W1 = PEAK(0x7E22, k), H1 = PEAK(0x7E24, k), E = PEAK(0x7E3A, k);
    u16 Cm1 = fw(DS_centre_ptr, -1);
    s16 XLq1 = rvk(DS_rq_xl, -1), Yq1 = rvk(DS_rq_y, -1), YT1q1 = rvk(DS_rq_yt1, -1), XRq1 = rvk(DS_rq_xr, -1), YT2q1 = rvk(DS_rq_yt2, -1);
    if (dyaw > -100 && Yq != YT1q) {
        if (L16 && YT1p <= Yp) {
            if (Cm1 & 0x80) {
                poly6(0x0E, XLq1, 0x65, XLq1, Yq1, XLq, Yq, XOLq, Yq, (s16)(XOLq - W1), Yq, E, 0x65);
                if (YT1p < Yp) poly3(0x0E, XOLq, Yq, (s16)(XOLq - W1 / 2), (s16)(Yq - H1), (s16)(XOLq - W1), Yq);
                else poly4(0x0E, XOLq, Yq, (s16)(XOLq - W1), Yq, (s16)(XOLp - W1), Yp, XOLp, Yp);
            }
            if (YT1p < Yp) poly4(0x0E, XOLq, Yq, (s16)(XOLq - W1 / 2), (s16)(Yq - H1), (s16)(XOLp - W0 / 2), (s16)(Yp - H0), XOLp, Yp);
        }
        if (!L16) {
            if (row == 1 || !(Cm1 & 0x80)) {
                if (XOLq + 0x1E < XORq) {
                    poly4(0x0E, 0, YT1q, (s16)(XOLq - o), Yq, (s16)(XOLp + o), Yp, 0, YT1p);
                    if (XOLq > 0 && ((row != 1 && Yq1 == YT1q1) || (row == 1 && Yq > YT1q)))
                        poly3(0x0E, 0, YT1q, XOLq, Yq, 0, Yq);
                }
            } else {
                poly6(0x0E, 0, YT1q, XOLq, Yq, XLq, Yq, XLq1, Yq1, XLq1, 0x65, 0, 0x65);
                poly4(0x0E, 0, YT1q, XOLq, Yq, XOLp, Yp, 0, YT1p);
            }
        }
    }
    if (dyaw < 100 && Yq != YT2q) {
        if (L16 && YT2p <= Yp) {
            if (Cm1 & 0x80) {
                poly6(0x0E, XRq1, 0x65, XRq1, Yq1, XRq, Yq, XORq, Yq, (s16)(W1 + XORq), Yq, (s16)(0x13F - E), 0x65);
                if (YT2p < Yp) poly3(0x0E, XORq, Yq, (s16)(W1 / 2 + XORq), (s16)(Yq - H1), (s16)(W1 + XORq), Yq);
                else poly4(0x0E, XORq, Yq, (s16)(W1 + XORq), Yq, (s16)(W1 + XORp), Yp, XORp, Yp);
            }
            if (YT2p < Yp) poly4(0x0E, XORq, Yq, (s16)(W1 / 2 + XORq), (s16)(Yq - H1), (s16)(W0 / 2 + XORp), (s16)(Yp - H0), XORp, Yp);
        }
        if (!L16) {
            if (row == 1 || !(Cm1 & 0x80)) {
                if (XOLq + 0x1E < XORq) {                              /* sic: the left test */
                    poly4(0x0E, 0x13F, YT2q, (s16)(XORq - o), Yq, (s16)(XORp + o), Yp, 0x140, YT2p);   /* sic 0x140 */
                    if (XORq < 0x13F && ((row != 1 && Yq1 == YT2q1) || (row == 1 && Yq > YT2q)))
                        poly3(0x0E, 0x13F, YT2q, XORq, Yq, 0x13F, Yq);
                }
            } else {
                poly6(0x0E, 0x13F, YT2q, XORq, Yq, XRq, Yq, XRq1, Yq1, XRq1, 0x65, 0x13F, 0x65);
                poly4(0x0E, 0x13F, YT2q, XORq, Yq, XORp, Yp, 0x13F, YT2p);
            }
        }
    }
}

/* 0dcb:69d4 obstacle_hit_test: does [x, x+w) cover a wheel column of the player's car (row 1) */
static s16 obstacle_hit_test(s16 x, s16 w, s16 y)
{
    if (y > 0x57) {
        if ((x < 0x65 && x + w > 99) || (x < 0x83 && x + w > 0x81)) return -1;
        if ((x < 0x140 && x + w > 0x13E) || (x < 0x122 && x + w > 0x120)) return 1;
    }
    return 0;
}

static void front_objects(s16 row)
{
    u16 L = fw(DS_left_ptr, 0), R = fw(DS_right_ptr, 0), C = fw(DS_centre_ptr, 0);
    s16 cross = (s16)(C & 1);
    s16 cpu = DSS(DS_cpu_speed);
    s16 Y = Yp, span = sub16(XRp, XLp);
    s16 t = MODE == 0x10 ? 0x15 : 4, u = MODE == 0x10 ? 0x16 : 0xB;
    if (MODE != 0x10 && (L & 0x20) && cpu > 3) scenery_draw_shape(XLp, Y, row, -1, 0, 0);
    if (L & 0x100) {
        if (cross) scenery_draw_shape(XLp, Y, row, -1, 4, 1);
        else if (!(MODE == 0x10 && Yq != YT1q)) {
            if (L & 0x200) scenery_draw_shape(sub16(shl16(XOLp, 1), XLp), Y, (s16)(row + 2), -1, t, 1);
            else           scenery_draw_shape(XOLp, Y, row, -1, t, 1);
        }
    }
    if (!(MODE & 7) && (L & 0x800))  scenery_draw_shape(XLp, Y, row, 1, 3, 0);
    if (!(MODE & 7) && (L & 0x1000)) scenery_draw_shape(XLp, Y, row, 1, 6, 0);
    if (!(MODE & 6) && (L & 1)) {
        if (!cross) { if (Yq == YT1q) scenery_draw_shape(XLp, Y, row, 1, 1, 1); }
        else { road_fillmode_end(); scenery_draw_sign_picture(XLp, (s16)(add16(Y, YEp) >> 1), row, 1, 0, 1); road_fillmode_begin(); }
    }
    if (MODE == 0x10 && (L & 0x10)) scenery_draw_bush_group(XLp, Y, row, 1);
    if (cpu > 3) {
        if ((MODE & 7) || Yq == YT1q) {
            if (L & 0x40) scenery_draw_shape(XOLp, Y, row, -1, 0xA, 0);
            if (L & 0x80) {
                if (!cross && (L & 2)) scenery_draw_shape((s16)(add16(XFLp, XMIN) >> 1), Y, (s16)(row + 3), -1, u, 0);
                else                   scenery_draw_shape(XOLp, Y, row, -1, u, 0);
            }
        }
        if ((MODE & 7) || Yq == YT2q) {
            if (R & 0x40) scenery_draw_shape(XORp, Y, row, 1, 0xA, 0);
            if (R & 0x80) {
                if (!cross && (R & 2)) scenery_draw_shape((s16)(add16(XFRp, XMAX) >> 1), Y, (s16)(row + 3), 1, u, 0);
                else                   scenery_draw_shape(add16(XORp, (s16)(sub16(XORp, XRp) >> 1)), Y, row, 1, u, 0);
            }
        }
    }
    if (!cross) {
        if (cpu > 3) {
            if ((MODE & 1) || Yq == YT1q) {
                if (L & 4) scenery_draw_shape(sub16(XLp, 2), Y, row, -1, 7, 1);
                if (L & 8) scenery_draw_shape(sub16(XLp, 2), Y, row, -1, 8, 1);
            }
            if ((MODE & 1) || Yq == YT2q) {
                if (R & 4) scenery_draw_shape(add16(XRp, 2), Y, row, 1, 7, 1);
                if (R & 8) scenery_draw_shape(add16(XRp, 2), Y, row, 1, 8, 1);
            }
        }
    } else {
        if (L & 0x40) scenery_draw_hydrant(XLp, Y, row, 1);
        if (R & 0x40) scenery_draw_hydrant(XRp, Y, row, -1);
    }
    if (!cross && (C & 2) && (MODE != 0x10 || Yq == YT2q) && !(MODE & 6))
        scenery_draw_shape((s16)-((span >> 3) - XORp), Y, row, 1, 9, 0);
    if (MODE != 0x10 && (R & 0x20)) scenery_draw_shape(XRp, Y, row, 1, 0, 0);
    if (R & 0x100) {
        if (cross) scenery_draw_shape(XRp, Y, row, 1, 4, 1);
        else if (!(MODE == 0x10 && Yq != YT2q)) {
            if (R & 0x200) scenery_draw_shape(sub16(shl16(XORp, 1), XRp), Y, (s16)(row + 3), 1, t, 1);
            else           scenery_draw_shape(XORp, Y, row, 1, t, 1);
        }
    }
    if (R & 0x2000) scenery_draw_shape(XRp, Y, row, 1, 0x12, 0);
    if (R & 0x4000) scenery_draw_shape(XRp, Y, row, 1, 0x13, 0);
    if (R & 0x800)  scenery_draw_shape(XRp, Y, row, 1, 3, 0);
    if (R & 0x1000) scenery_draw_shape(XRp, Y, row, -1, 6, 0);
    if (!(MODE & 6) && (R & 1)) {
        if (!cross) { if (Yq == YT2q) scenery_draw_shape(XRp, Y, row, -1, 1, 1); }
        else { road_fillmode_end(); scenery_draw_sign_picture(XRp, (s16)(add16(Y, YEp) >> 1), row, -1, 0, 1); road_fillmode_begin(); }
    }
    if (MODE == 0x10 && (R & 0x10)) scenery_draw_bush_group(XRp, Y, row, -1);
    s16 x6 = (s16)fw(DS_x6_ptr, 0);
    if (MODE == 0x10 && (C & 4)) {                          /* rock */
        s16 s = x6 < 9 ? -1 : 1;
        s16 x = (s16)(((s16)(x6 * span) >> 4) + XLp);
        scenery_draw_shape(x, Y, row, s, 7, 1);
        if (s == 1) s = 0;
        if (row == 1) DSS(DS_rock_hit) = obstacle_hit_test((s16)(s * 0x28 + x), 0x28, Y);
    }
    if (MODE == 0x10 && (C & 0x1000)) {
        s16 s = x6 < 9 ? -1 : 1;
        s16 x = (s16)(((s16)(x6 * span) >> 4) + XLp);
        scenery_draw_shape(x, Y, row, s, 0x14, 1);
        if (s == 1) s = 0;
        if (row == 1) DSS(DS_obstacle14_hit) = obstacle_hit_test((s16)(s * 0x32 + x), 0x32, Y);
    }
    if ((C & 0xF000) && !(MODE & 0x16))
        scenery_draw_banner(XLp, XRp, (s16)(YEp * 4 - Y * 3), Y, row, 0, (u16)(C & 0xF000));
    if (C & 0x100) scenery_draw_shape(XRp, Y, row, 1, 5, 0);
    if (C & 0x800) scenery_draw_shape(XRp, Y, row, 1, 0xF, 0);
    if (C & 0x200) scenery_draw_shape(XRp, Y, row, 1, 0x10, 0);
    if (C & 0x400) scenery_draw_shape(XRp, Y, row, 1, 0x11, 0);
    if ((MODE & 6) && sub16(DSS(DS_start_seg), DSS(DS_front_row_seg)) == -4)
        road_draw_start_tree((s16)(add16(XRp, XLp) >> 1), Y, row);
}

static void mulholland_extras_front(s16 row)
{
    u16 C = fw(DS_centre_ptr, 0), R = fw(DS_right_ptr, 0);
    if ((C & 0x80) && (fw(DS_centre_ptr, -1) & 0x80))
        scenery_draw_truss(XLq, XRq, (s16)(YEq * 4 - Yq * 3), Yq, XLp, XRp, (s16)(YEp * 4 - Yp * 3), Yp,
                           (s16)(DSS(DS_seg) - row + 0xE), row < 7);
    if ((C & 0x80) && !(fw(DS_centre_ptr, 1) & 0x80)) scenery_draw_truss_end(XLp, XRp, (s16)(YEp * 4 - Yp * 3), Yp);
    if ((R & 0x8000) && !(C & 0x80)) guard_rail(row, -0x1E, 0xE, 0);
}

/* 0dcb:6a32 road_draw_front */
static void road_draw_front(void)
{
    if (DSW(DS_fallen) != 0) return;
    row_ptrs_init(14);
    DSW(DS_dash_ptr) = (u16)(DS_dash_width + 28);
    flag_ptrs(0x1C);
    if (MODE & 6) DSS(DS_rail_phase_seg) = DSS(DS_start_seg);
    else {
        for (s16 k = 14; k != 0; k--)
            if ((fw(DS_seg_right_ptr, k) & 0x8000) && !(fw(DS_seg_right_ptr, (s16)(k - 1)) & 0x8000)) {
                DSS(DS_rail_phase_seg) = (s16)(DSS(DS_seg) + k);
                break;
            }
    }
    clip_set_view();
    if (MODE == 0x10) road_draw_sky_mulholland(); else road_draw_sky();
    road_fillmode_begin();
    s16 L16 = 0;
    for (s16 k = 14; k > 9; k--)
        if ((MODE & 0x10) && !(fw(DS_seg_centre_ptr, k) & 0x80) && (fw(DS_seg_centre_ptr, (s16)(k - 1)) & 0x80)) L16 = 1;
    for (s16 row = 14; row != 0; row--) {
        DSS(DS_front_row_seg) = (s16)(DSS(DS_seg) + row);
        u16 C = fw(DS_centre_ptr, 0);
        if ((MODE & 0x10) && !(C & 0x80)) front_hillsides(row, L16);
        if (row <= DSS(DS_top_band_row)) road_draw_row_front(row);
        C = fw(DS_centre_ptr, 0);
        if (!((C & 4) && (C & 1))) {
            if (row < 11) front_objects(row);
            if (!(C & 1) && MODE == 0x10) mulholland_extras_front(row);
        }
        if (sub16(DSS(DS_opp_seg), row) == DSS(DS_seg) && DSB(DS_racing)) opponent_draw();
        if (MODE & 6) road_draw_barrier_walls(DSS(DS_rail_phase_seg), 0xE, row);
        row_ptrs_step();
        flag_step(-2);
    }
    if (DSS(DS_seg) == DSS(DS_opp_seg) && DSB(DS_racing)) opponent_draw();
}

/* ================================================================================ mirror */

static void mirror_hillsides(s16 row)
{
    s16 o = DSS(DS_fall_dir);
    u16 Cm1 = fw(DS_centre_ptr, -1);
    s16 XLq1 = rvk(DS_rq_xl, -1), XRq1 = rvk(DS_rq_xr, -1), Yq1 = rvk(DS_rq_y, -1);
    s16 YT1q1 = rvk(DS_rq_yt1, -1), YT2q1 = rvk(DS_rq_yt2, -1);
    if (o != 1 && Yq != YT1q) {
        if (row == 1 || !(Cm1 & 0x80)) {
            poly4(0x0E, XMIN, YT1q, (s16)(XOLq - o), Yq, (s16)(XOLp + o), Yp, XMIN, YT1p);
            if (row == 1 && XOLp > 0xF0)                 poly3(0x0E, XMIN, YT1q, XOLp, Yp, XMIN, Yp);
            else if (Yq1 == YT1q1 && XOLq > 0xF0)        poly3(0x0E, XMIN, YT1q, XOLq, Yq, XMIN, Yq);
        } else poly4(0x0E, XMIN, YT1q, XOLq, Yq, XOLp, Yp, XMIN, YT1p);
        if (row != 1 && (Cm1 & 0x80))
            poly6(0x0E, XMIN, YT1q, XOLq, Yq, XLq, Yq, XLq1, Yq1, XLq1, YMAX, XMIN, YMAX);
    }
    if (o != -1 && Yq != YT2q) {
        if (row == 1 || !(Cm1 & 0x80)) {
            poly4(0x0E, XMAX, YT2q, (s16)(XORq - o), Yq, (s16)(XORp + o), Yp, XMAX, YT2p);
            if (row == 1 && XORp < 0x13F)                poly3(0x0E, XMAX, YT2q, XORp, Yp, XMAX, Yp);
            else if (Yq1 == YT2q1 && XORq < 0x13F)       poly3(0x0E, XMAX, YT2q, XORq, Yq, XMAX, Yq);
        } else poly4(0x0E, XMAX, YT2q, XORq, Yq, XORp, Yp, XMAX, YT2p);
        if (row != 1 && (Cm1 & 0x80))
            poly6(0x0E, XMAX, YT2q, XORq, Yq, XRq, Yq, XRq1, Yq1, XRq1, YMAX, XMAX, YMAX);
    }
}

static void mirror_objects(s16 row, s16 d)
{
    u16 L = fw(DS_left_ptr, 0), R = fw(DS_right_ptr, 0), C = fw(DS_centre_ptr, 0);
    s16 cross = (s16)(C & 1);
    s16 cpu = DSS(DS_cpu_speed);
    s16 Y = Yp, span = sub16(XRp, XLp);
    s16 t = MODE == 0x10 ? 0x15 : 4, u = MODE == 0x10 ? 0x16 : 0xB;
    if (MODE != 0x10 && (L & 0x20) && cpu > 3) scenery_draw_shape(XLp, Y, d, -1, 0, 0);
    if (L & 0x100) {
        if (cross) scenery_draw_shape(XLp, Y, d, -1, 4, 1);
        else if (!(MODE == 0x10 && Yq != YT1q))
            scenery_draw_shape((L & 0x200) ? sub16(shl16(XOLp, 1), XLp) : XOLp, Y, d, -1, t, 1);
    }
    if (!(MODE & 7) && (L & 0x800))  scenery_draw_shape(XLp, Y, d, -1, 3, 0);
    if (!(MODE & 7) && (L & 0x1000)) scenery_draw_shape(XLp, Y, d, 1, 6, 0);
    if (!(MODE & 6) && (L & 1) && Yq == YT1q) scenery_draw_shape(XLp, Y, d, 1, 1, 1);
    if (!(MODE & 6) && (R & 1) && Yq == YT2q) scenery_draw_shape(XRp, Y, d, -1, 1, 1);
    if (cpu > 3) {
        if ((MODE & 7) || Yq == YT1q) {
            if (L & 0x40) scenery_draw_shape(XOLp, Y, d, -1, 0xA, 0);
            if (L & 0x80) scenery_draw_shape((!cross && (L & 2)) ? (s16)(add16(XOLp, XMIN) >> 1) : XOLp, Y, d, -1, u, 0);
        }
        if ((MODE & 7) || Yq == YT2q) {
            if (R & 0x40) scenery_draw_shape(XORp, Y, d, 1, 0xA, 0);
            if (R & 0x80) scenery_draw_shape((!cross && (R & 2)) ? (s16)(add16(XFRp, XMAX) >> 1) : XORp, Y, d, 1, u, 0);
        }
    }
    if ((MODE & 0x10) && (L & 0x10)) scenery_draw_bush_group(XLp, Y, d, 1);
    if ((MODE & 0x10) && (R & 0x10)) scenery_draw_bush_group(XRp, Y, d, -1);
    if (!cross) {
        if (cpu > 3) {
            if ((MODE & 1) || Yq == YT1q) {
                if (L & 4) scenery_draw_shape(XLp, Y, d, -1, 7, 1);
                if (L & 8) scenery_draw_shape(XLp, Y, d, -1, 8, 1);
            }
            if ((MODE & 1) || Yq == YT2q) {
                if (R & 4) scenery_draw_shape(XRp, Y, d, 1, 7, 1);
                if (R & 8) scenery_draw_shape(XRp, Y, d, 1, 8, 1);
            }
        }
    } else {
        if (L & 0x40) scenery_draw_hydrant(XLp, Y, d, -1);
        if (R & 0x40) scenery_draw_hydrant(XRp, Y, d, 1);
    }
    if (!cross && (C & 2) && (MODE != 0x10 || Yq == YT2q) && !(MODE & 6))
        scenery_draw_shape((s16)-((span >> 3) - XORp), Y, d, 1, 9, 0);
    if (MODE != 0x10 && (R & 0x20)) scenery_draw_shape(XRp, Y, d, 1, 0, 0);
    if (R & 0x100) {
        if (cross) scenery_draw_shape(XRp, Y, d, 1, 4, 1);
        else if (!(MODE == 0x10 && Yq != YT2q))
            scenery_draw_shape((R & 0x200) ? sub16(shl16(XORp, 1), XRp) : XORp, Y, d, 1, t, 1);
    }
    if (R & 0x2000) scenery_draw_shape(XRp, Y, d, 1, 0x12, 0);
    if (R & 0x4000) scenery_draw_shape(XRp, Y, d, 1, 0x13, 0);
    if (R & 0x800)  scenery_draw_shape(XRp, Y, d, 1, 3, 0);
    if (R & 0x1000) scenery_draw_shape(XRp, Y, d, -1, 6, 0);
    s16 x6 = (s16)fw(DS_x6_ptr, 0);
    if (!(MODE & 7) && (C & 4))
        scenery_draw_shape((s16)(((s16)(x6 * span) >> 4) + XLp), Y, d, x6 < 9 ? -1 : 1, 7, 1);
    if (MODE == 0x10 && (C & 0x1000))
        scenery_draw_shape((s16)(((s16)(x6 * span) >> 4) + XLp), Y, d, x6 < 9 ? -1 : 1, 0x14, 1);
    if (C & 0x100) scenery_draw_shape(XRp, Y, d, 1, 5, 0);
    if (C & 0x200) scenery_draw_shape(XRp, Y, d, 1, 0x10, 0);
    if (C & 0x400) scenery_draw_shape(XRp, Y, d, 1, 0x11, 0);
    if (C & 0x800) scenery_draw_shape(XRp, Y, d, 1, 0xF, 0);
    if ((MODE & 0x11) && sub16(DSS(DS_seg), row) == DSS(DS_starter_seg))
        starter_draw_mirror((s16)(add16(XRp, XLp) >> 1), Y, (s16)(row - 1));
}

static void mulholland_extras_mirror(s16 row)
{
    u16 C = fw(DS_centre_ptr, 0), R = fw(DS_right_ptr, 0);
    if ((C & 0x80) && (fw(DS_centre_ptr, 1) & 0x80) && (fw(DS_centre_ptr, 2) & 0x80))
        scenery_draw_truss(XLq, XRq, (s16)(YEq * 4 - Yq * 3), Yq, XLp, XRp, (s16)(YEp * 4 - Yp * 3), Yp,
                           sub16(DSS(DS_seg), row), row < 4);
    if ((C & 0x80) && !(fw(DS_centre_ptr, -1) & 0x80)) scenery_draw_truss_end(XLp, XRp, (s16)(YEp * 4 - Yp * 3), Yp);
    if ((R & 0x8000) && !(C & 0x80)) guard_rail(row, -10, 7, 1);
}

/* 0dcb:5aee road_draw_mirror */
static void road_draw_mirror(void)
{
    row_ptrs_init(7);
    DSW(DS_dash_ptr) = (u16)(DS_dash_width + 0x28);         /* 7E7A */
    flag_ptrs(-0x10);
    clip_set_mirror();
    road_fillmode_end();
    mirror_draw_backdrop();
    road_fillmode_begin();
    for (s16 row = 7; row != 0; row--) {
        DSS(DS_mirror_row_seg) = sub16(DSS(DS_seg), row);
        if ((MODE & 0x10) && !(fw(DS_centre_ptr, 0) & 0x80)) mirror_hillsides(row);
        road_draw_row_mirror();
        u16 C = fw(DS_centre_ptr, 0);
        if (!((C & 4) && (C & 1))) {
            if ((C & 0xF000) && !(MODE & 0x16))
                scenery_draw_banner(XLp, XRp, (s16)(YEp * 4 - Yp * 3), Yp, (s16)(row + 5), 1, (u16)(C & 0xF000));
            if (row < 7) mirror_objects(row, (s16)((row >> 1) + 7));
            if (!(C & 1) && MODE == 0x10) mulholland_extras_mirror(row);
        }
        if (sub16(DSS(DS_seg), row) == DSS(DS_opp_seg) && DSB(DS_racing)) opponent_draw();
        if (MODE & 6) road_draw_barrier_walls(DSS(DS_rail_phase_seg), 7, row);
        row_ptrs_step();
        flag_step(2);
    }
    mirror_finish();
}

/* ================================================================================ frame, step */

/* 0dcb:11fc road_frame */
void road_frame(void)
{
    if (MODE == 8) aq_project_front();
    else { road_set_seg_ptrs(DSS(DS_seg)); road_project_front(); }
    if (MODE == 8) {
        DSS(DS_aq_rows) = 14;
        ds_far_wr(DS_aq_flags_ptr, trk_ptr(TRK_AQ, DSS(DS_seg)));
        aq_draw_front();
    } else { road_set_seg_ptrs(DSS(DS_seg)); road_draw_front(); }
    if (MODE == 8) aq_project_mirror();
    else { road_set_seg_ptrs(DSS(DS_seg)); road_project_mirror(); }
    if (MODE == 8) {
        DSS(DS_aq_rows) = 7;
        ds_far_wr(DS_aq_flags_ptr, far_make(SEG(TRK_SEG), (u16)(0x6570 - 2 * DSS(DS_seg))));
        aq_draw_mirror();
    } else { road_set_seg_ptrs(DSS(DS_seg)); road_draw_mirror(); }
    road_fillmode_end();
    road_draw_hood_overlay();
    if (DSB(DS_racing) && !(MODE & 6)) road_draw_starter_slide();
    road_draw_cockpit_overlay();
    road_page_flip();
    pace_ticks(8);
}

/* 0dcb:12ca road_advance_segment */
void road_advance_segment(void)
{
    DSS(DS_seg) = add16(DSS(DS_seg), (s16)(DSS(DS_substep) >> 6));
    DSS(DS_substep) &= 0x3F;
    if (MODE == 8) {
        DSS(DS_ai_edge_left) = DSS(DS_edge_left) = (s16)(DSS(DS_aq_def_top_xl) + 0x438);
        DSS(DS_ai_edge_right) = DSS(DS_edge_right) = (s16)(DSS(DS_aq_def_top_xr) + 0xE88);
    } else {
        road_set_seg_ptrs(DSS(DS_seg));
        if (fw(DS_seg_centre_ptr, 0) & 0xE0) { DSS(DS_edge_left) = 0x7D0; DSS(DS_edge_right) = 0xAF0; }
        else                                  { DSS(DS_edge_left) = 0x5DC; DSS(DS_edge_right) = 0xCE4; }
        if (fw(DS_seg_centre_ptr, 4) & 0x80)  { DSS(DS_ai_edge_left) = 0x7D0; DSS(DS_ai_edge_right) = 0xAF0; }
        else                                  { DSS(DS_ai_edge_left) = 0x5DC; DSS(DS_ai_edge_right) = 0xCE4; }
    }
    DSS(DS_cur_height) = (s16)fw(DS_seg_hgt_ptr, 0);
    if (MODE & 6) return;
    if (!(((DSW(DS_demo) || DSW(DS_auto_drive)) && !DSW(DS_start_active)) || DSS(DS_seg) > DSS(DS_track_end))) return;
    s16 t = 0x960;                                          /* autopilot: demo / auto, or past the finish */
    if (MODE == 8 || !(TRKB(TRK_C, DSS(DS_seg)) & 0x80))
        t = (s16)(t + (DSS(DS_opp_lat) > t ? -0x140 : 0xA0));
    if (MODE == 8) {
        t = (s16)(aq_lane_target(DSS(DS_cam_x), sub16(t, DSS(DS_cam_x)), DSS(DS_seg), DSS(DS_opp_lat), DSS(DS_opp_seg)) + DSS(DS_cam_x));
        DSS(DS_cam_x) = t;
    } else if (abs16(sub16(DSS(DS_cam_x), t)) > 100) DSS(DS_cam_x) = (s16)(t + (DSS(DS_cam_x) > t ? 100 : -100));
    else DSS(DS_cam_x) = t;
    s16 y = (s16)fw(DS_seg_yaw_ptr, 0);
    if (abs16(sub16(y, DSS(DS_heading))) > 0) {
        s16 w = shl16(sub16(DSS(DS_heading), y), 1);
        if (abs16(w) > 0x18) w = w > 0 ? 0x18 : -0x18;
        PHS(PLAYER, PH_WHEEL) = w;
    }
    DSS(DS_heading) = y;
}

/* 0dcb:148a road_edge_collision (every race step from the race ISR) */
s16 road_edge_collision(void)
{
    if (MODE == 8) return aq_wall_collision();
    DSW(DS_on_shoulder) = 0;
    s16 l = sub16(DSS(DS_cam_x), 0x1E), r = add16(DSS(DS_cam_x), 0x46), s = DSS(DS_seg);
    u16 c = TRKW(TRK_C, s + 1), rr = TRKW(TRK_R, s + 1);
    s16 bl = TRKS(TRK_BL, s), br = TRKS(TRK_BR, s);
    s16 w = MODE == 0x10 ? 0x96 : 0x104;
    s16 eL = DSS(DS_edge_left), eR = DSS(DS_edge_right);
    if (bl > 0 && l < sub16(eL, (s16)(w >> 2))) return 1;
    if (br > 0 && r > add16(eR, (s16)(w >> 1))) return 1;
    if (l <= sub16(eL, w) || r >= add16(eR, w)) {
        if (!(MODE & 0x10)) return 1;
        if (DSS(DS_fall_dir)) return 1;
        if (l < eL && bl < 0) DSS(DS_fall_dir) = -1;
        if (r > eR && br < 0) DSS(DS_fall_dir) = 1;
        return 1;
    }
    if ((c & 0x80) && (l < add16(eL, (s16)(w >> 2)) || r > sub16(eR, (s16)(w >> 1)))) return 1;
    s16 shoulder = 0;
    if (bl == 0 && l < eL) shoulder = 1;
    else if (br == 0 && r > eR) { if (c & 2) return 1; shoulder = 1; }
    if (shoulder) { if (rr & 0xC0) return 1; DSW(DS_on_shoulder) = 1; }
    if (MODE == 0x10) {
        if (DSS(DS_rock_hit)) {
            DSS(DS_rock_hit) = 0;
            if (DSS(DS_speedo_shown) > 0x1E) { if (!DSW(DS_demo) && !DSW(DS_auto_drive)) return 1; opp_gear(0x1E, PLAYER); }
        }
        if (DSS(DS_obstacle14_hit)) {
            if (DSS(DS_speedo_shown) > 0x32) DSS(DS_heading) = add16(DSS(DS_heading), (s16)(DSS(DS_obstacle14_hit) * -0x28));
            if (DSS(DS_speedo_shown) > 0x14) PHS(PLAYER, PH_SPEED) = sub16(PHS(PLAYER, PH_SPEED), 10);
            DSS(DS_obstacle14_hit) = 0;
        }
        if ((TRKW(TRK_L, s) & 0x10) && l < 0x79E) return 1;
        if ((TRKW(TRK_R, s) & 0x10) && r > 0xB22) return 1;
        if ((c & 0x60) && DSS(DS_speedo_shown) > 0x3C) { if (!DSW(DS_demo) && !DSW(DS_auto_drive)) return 1; opp_gear(0x3C, PLAYER); }
        if (!(rr & 0x8000) || r < eR) return 0;
        if (DSS(DS_speedo_shown) >= 0x28) return 1;
        sound_vec(SND_SCREECH); sound_vec(SND_SCREECH);
        DSS(DS_cam_x) = sub16(DSS(DS_cam_x), 300);
        DSS(DS_heading) = add16(DSS(DS_heading), 0x1E);
    } else {
        if (!(MODE & 6)) return 0;
        if (r < sub16(eR, 0xB4)) return 0;                  /* drag strip: right wall */
        if (DSS(DS_speedo_shown) >= 0x28) return 1;
        sound_vec(SND_SCREECH); sound_vec(SND_SCREECH);
        DSS(DS_cam_x) = sub16(DSS(DS_cam_x), 200);
        DSS(DS_heading) = add16(DSS(DS_heading), 0x14);
    }
    PHS(PLAYER, PH_SPEED) = PHS(PLAYER, PH_SPEED) > 10 ? (s16)(PHS(PLAYER, PH_SPEED) - 10) : 0;
    sound_vec(SND_SCREECH);
    return 0;
}

/* 0dcb:1758 road_nudge_heading */
static void road_nudge_heading(s16 dir)
{
    if (dir == 1) DSS(DS_heading) = add16(DSS(DS_heading), 3);
    else          DSS(DS_heading) = sub16(DSS(DS_heading), 3);
}

/* 0dcb:1774 road_step: once per frame from race_run */
void road_step(void)
{
    s16 si = PHS(PLAYER, PH_STEER_IN);
    if (si != 0 && PHS(PLAYER, PH_SPEED) != 0 &&
        abs16((s16)(DSS(DS_heading) - (s16)fw(DS_seg_yaw_ptr, 0) - si)) < 200)
        DSS(DS_heading) = sub16(DSS(DS_heading), add16((s16)(si >> 1), 1));
    if (!DSB(DS_racing)) DSS(DS_step_per_frame) = DSW(DS_slow_machine) ? 0x30 : 0x20;
    if (DSS(DS_step_per_frame) > 0) DSS(DS_start_light_pos) = add16(DSS(DS_start_light_pos), 4);
    road_frame();
    if (!DSW(DS_demo) && !DSW(DS_auto_drive)) {
        if (DSB(DS_tires_blown)) DSS(DS_cam_x) = add16(DSS(DS_cam_x), 0x19);
        else if (PHS(PLAYER, PH_SPEED) != 0) {
            s16 t = mul16(add16(q_shr(DSS(DS_step_per_frame), 3), PHS(PLAYER, PH_TIRE_SLIP)),
                          sub16(DSS(DS_heading), (s16)fw(DS_seg_yaw_ptr, 0)));
            DSS(DS_cam_x) = sub16(DSS(DS_cam_x), (s16)(t >> 2));
            if (MODE == 0x10) {
                s16 v = DSS(DS_cam_x);
                if (v < sub16(DSS(DS_edge_left), 0x96)) v = sub16(DSS(DS_edge_left), 0x96);
                if (v > add16(DSS(DS_edge_right), 0x96)) v = add16(DSS(DS_edge_right), 0x96);
                DSS(DS_cam_x) = v;
            }
        }
    }
    if (!DSB(DS_racing)) {
        DSS(DS_substep) = add16(DSS(DS_substep), DSS(DS_step_per_frame));
        if (DSS(DS_substep) >= 0x40) road_advance_segment();
        if ((TRKW(TRK_L, DSS(DS_seg) + 7) & 1) || DSW(DS_nudge_left_latch)) {
            DSW(DS_nudge_left_latch) = 1;
            road_nudge_heading(1);
        }
        if ((TRKW(TRK_R, DSS(DS_seg) + 6) & 1) || DSW(DS_nudge_right_latch)) {
            road_nudge_heading(-1);
            DSW(DS_nudge_right_latch) = 1;
        }
    } else {
        if (DSS(DS_fall_dir)) { road_fall_off_edge(DSS(DS_fall_dir)); DSS(DS_fall_dir) = 0; }
        if (DSS(DS_aq_wall_hit) && MODE == 8) aq_wall_bounce(DSS(DS_aq_wall_hit));
    }
}

/* 0dcb:7da2 road_fall_off_edge (Mulholland): 8 frames of the camera dropping and sliding */
void road_fall_off_edge(s16 dir)
{
    for (s16 k = 1; k < 9; k++) {
        DSS(DS_cur_height) = sub16(DSS(DS_cur_height), k);
        DSS(DS_cam_y) = sub16(DSS(DS_cam_y), k);
        DSS(DS_cam_x) = add16(DSS(DS_cam_x), (s16)(dir * 100));
        road_frame();
    }
    DSW(DS_fallen) = 1;
    DSW(DS_cliff_fall) = 1;
}
