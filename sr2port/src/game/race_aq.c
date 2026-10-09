/* The Aqueduct race (road mode 8) of race_render (port/spec/race_render.md §1.6, 0dcb:18ec-1cd2, 8288-c342): the
 * concrete channel renderer with the roll rotation, the wall bounce, the obstacle probe, the rotated opponent
 * sprites, the night sky and the lane AI (0001:d2a8).
 *
 * PORT: S5.3 stub — the channel renderer (aq_project_front / mirror, aq_draw_front / mirror and their helpers), the
 * picture rotator (slot 16) and the rotated sprite loader 0dcb:8840 are not ported yet: the Aqueduct race runs on the
 * stand-ins below (no view drawn, the race logic, AI and results work). */
#include "game/race_int.h"
#include "game/race.h"

#include "platform/platform.h"
#include "platform/video.h"

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
    if (DSS(0x9A58) >= 0xDED) { DSS(DS_aq_wall_hit) = -1; return 1; }
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

/* ---------------------------------------------------------------- stand-ins (PORT: S5.3 stub) */

void aq_project_front(void) {}                                 /* PORT: S5.3 stub (0dcb:18ec) */
void aq_project_mirror(void) {}                                /* PORT: S5.3 stub (0dcb:1b02) */
void aq_draw_front(void) {}                                    /* PORT: S5.3 stub (0dcb:a094) */
void aq_draw_mirror(void) {}                                   /* PORT: S5.3 stub (0dcb:9ce8) */
void aq_wall_bounce(s16 side) { (void)side; DSS(DS_aq_wall_hit) = 0; }   /* PORT: S5.3 stub (0dcb:a1c0) */
s16  aq_collide_probe(s16 flags) { (void)flags; return 0; }    /* PORT: S5.3 stub (0dcb:a2d8) */
void aq_mirror_stars(void) {}                                  /* PORT: S5.3 stub (0dcb:85a0) */
void starter_draw_mirror_aq(s16 row) { (void)row; }            /* PORT: S5.3 stub (0dcb:a92a) */

/* 0dcb:ad40 opp_sprite_aq. PORT: S5.3 stub — without the rotated sprite sets of 0dcb:8840 the plain sprites are used */
FarPtr opp_sprite_aq(s16 s, s16 v)
{
    (void)v;
    return ds_far((u16)(DS_opp_sprites + 4 * s));
}

/* 0dcb:8840 race_load_graphics_rot. PORT: S5.3 stub — loads the normal race graphics (sky, sprites) instead */
void race_load_graphics_rot(void)
{
    road_load_graphics();
}

/* 0dcb:bd12 pic_rotate_planar (slot 16). PORT: S5.3 stub — returns the picture unrotated */
FarPtr pic_rotate_planar(FarPtr pic, s16 angle, s16 mode, s8 *ax, s8 *ay, s16 pivot)
{
    (void)angle; (void)mode; (void)pivot;
    *ax = 0;
    *ay = 0;
    return pic;
}
