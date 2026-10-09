/* Garage: the tyre change animation (garage.md, "Change tires"): tc_wheel_strip 1a21:b16e, tc_wheel_swap 1a21:b358,
 * tire_change_anim 1a21:b9bc. The jack comes up under the car, the car is lifted 7 steps, each wheel is swapped (the old
 * one goes down with a hand, rolls away to the left, the new one rolls in and goes up), the car is lowered 9 steps and
 * the jack goes away.
 *
 * VGA path only (DS:AD1C = -2): the Tandy branches (DS:79E4 != 0) are not ported, so the y base DS:79E4 is always 0.
 * Scratch state lives in DGROUP at the original addresses (ds_draw.h, DS_tc_*). */
#include "game/garage_draw.h"

#include <string.h>

#include "game/ds_draw.h"
#include "game/records.h"
#include "game/snd.h"
#include "platform/platform.h"
#include "platform/video.h"

#define TC_Y      DSS(DS_tc_ybase)                /* 0 on VGA */
#define TC_X      DSS(DS_tc_x)                    /* x of the car */

static inline bool has_mask(FarPtr d) { return !far_is_null(desc_mask(d)); }
static inline s16 x_align(void) { return (s16)DSW(DS_g_x_align); }
static inline FarPtr slot(s16 i) { return ds_far((u16)(DS_tc_frames + 4 * i)); }
static inline void slot_set(s16 i, FarPtr p) { ds_far_wr((u16)(DS_tc_frames + 4 * i), p); }
static inline FarPtr planes_plus(FarPtr d, u16 n) { FarPtr p = desc_planes(d); p.off = (u16)(p.off + n); return p; }
static inline const FarPtr *frames_at(u16 off) { return (const FarPtr *)mp(DGROUP, off); }

/* page 0 -> page 1 (slot 5), "if (DS:AD1C == -2)" in the original */
static void copy_page01(void)
{
    if (DSS(DS_driver_id) == -2) drv_copy_page(desc_planes(g_page(0)), desc_planes(g_page(1)));
}

static void tc_rect_set(FarPtr d)
{
    DSW(DS_tc_rect) = 0;
    DSW((u16)(DS_tc_rect + 2)) = 0;
    DSW((u16)(DS_tc_rect + 4)) = (u16)(desc_w(d) - 1);
    DSW((u16)(DS_tc_rect + 6)) = (u16)(desc_h(d) + TC_Y - 1);
}

/* 1a21:b16e tc_wheel_strip (near): the hub picture (DS:9634) at wheel x on page B at y 0x80; returns a 40-px strip of the
 * car around the wheel over the background (arena pool 1). */
static FarPtr tc_wheel_strip(s16 wx)
{
    FarPtr hub = ds_far(DS_tc_hub);
    FarPtr q = arena_bitmap_alloc((s16)(desc_w(hub) + 8), (s16)desc_h(hub), has_mask(hub) ? 1 : 0, 1);
    bitmap_make_shifted(hub, (s16)(wx & (s16)~x_align()), q);
    s16 qw = (s16)desc_w(q), qh = (s16)desc_h(q);
    FarPtr t = arena_bitmap_alloc(qw, qh, 0, 1);
    s16 sx = (s16)((TC_X + wx) & x_align());
    screen_save_rect(sx, 0x80, qw, qh, t);
    Rect r = { qw, qh, 0, 0, 0, 0 };
    drv_blit_masked(q, t, &r, 1);
    Rect r2 = { qw, qh, 0, 0, sx, 0x80 };
    drv_blit(t, g_back(), &r2, 1);
    arena_pop_low(2);
    FarPtr car = ds_far(DS_tc_car);
    Rect r3 = { 0x28, (s16)desc_h(car), (s16)(x_align() & wx), 0, 0, (s16)(0x9B - desc_h(car) - TC_Y) };
    FarPtr s = arena_bitmap_alloc(0x28, (s16)(0xA2 - TC_Y), 1, 1);
    screen_save_rect((s16)(r3.sx + TC_X), TC_Y, 0x28, (s16)(0xA2 - TC_Y), s);
    far_memset(desc_mask(s), 0xFF, (u16)(desc_size(s) / DSW(DS_g_nplanes)));
    drv_blit_masked(car, s, &r3, 1);
    drv_blit(car, s, &r3, 2);
    return s;
}

/* the 5th frame slot (DS:969C) is the descriptor copy shown while the hand holds the wheel */
static void tc_hold_off_copy(FarPtr dst, FarPtr src) { memcpy(far_mp(dst), far_mp(src), DESC_BYTES); }

/* 1a21:b358 tc_wheel_swap (near): at wheel x: the old wheel goes down (a hand holds it), rolls away to the left, the new
 * one rolls in from the left and goes up into place. DS:9630 = the car strip at the wheel. */
static void tc_wheel_swap(s16 x, u16 old_pics, u16 new_pics)
{
    FarPtr hand = pic_get(0x47F, 1);
    ds_far_wr(DS_tc_hand, hand);
    FarPtr w0 = pic_get_padded(DSS(old_pics), 1);
    ds_far_wr(DS_tc_tmp, w0);
    DSW(DS_tc_shift) = (u16)(x & (s16)~x_align());
    FarPtr f0 = arena_bitmap_alloc((s16)desc_w(w0), (s16)desc_h(w0), 1, 1);
    slot_set(0, f0);
    bitmap_make_shifted(w0, DSS(DS_tc_shift), f0);
    FarPtr cp = arena_bitmap_alloc((s16)desc_w(f0), (s16)desc_h(f0), 1, 1);
    slot_set(4, cp);
    bitmap_copy_into(f0, cp, 3);
    DSW(DS_tc_hy) = (u16)(desc_h(f0) - desc_h(hand));
    bitmap_composite_behind(hand, f0, DSS(DS_tc_shift), DSS(DS_tc_hy));
    copy_page01();
    FarPtr strip = ds_far(DS_tc_strip);
    tc_rect_set(strip);
    u16 anim = sprite_scene_setup((s16)(x & x_align()), 0x9A, frames_at(DS_tc_frame_copy), 0, 1,
                                  ds_far(DS_tc_bmp_a), strip, DS_tc_rect);
    DSW(DS_tc_anim) = anim;
    anim_phase_set(anim, 0, 8, 0, 1);             /* down 8 */
    while (drv_anim_step(DSW(DS_tc_anim)) != 0) {
        if (DSS((u16)(DSW(DS_tc_anim) + 0xE)) == 0x85) tc_hold_off_copy(slot(4), slot(0));
        wait_vretraces(3);
    }
    copy_page01();
    for (s16 si = 1; si < 4; si++) {              /* the old wheel's rolling frames */
        s16 sh = (s16)((2 * si + DSS(DS_tc_shift)) & (s16)~x_align());
        DSW(DS_tc_x2) = (u16)sh;
        FarPtr f;
        if (sh == 0) {
            f = pic_get_padded(DSS((u16)(old_pics + 2 * si)), 1);
        } else {
            FarPtr p = pic_get(DSS((u16)(old_pics + 2 * si)), 2);
            ds_far_wr(DS_tc_tmp, p);
            f = arena_bitmap_alloc((s16)(desc_w(p) + 8), (s16)desc_h(p), 1, 1);
            bitmap_make_shifted(p, sh, f);
            arena_pop(1);
        }
        slot_set((s16)(4 - si), f);
        bitmap_composite_behind(hand, f, sh, DSS(DS_tc_hy));
    }
    anim = sprite_scene_setup(x, 0xA1, frames_at(DS_tc_frames), 0, 4, ds_far(DS_tc_bmp_b), ds_far(DS_tc_save), 0);
    DSW(DS_tc_anim) = anim;
    anim_phase_set(anim, 0, (s16)((s16)(desc_w(slot(0)) + x) >> 1), -2, 0);   /* rolls away */
    sound_vec(0, 1);
    DSW(DS_tc_sfx) = 8;
    while (drv_anim_step(DSW(DS_tc_anim)) != 0) wait_vretraces(2);
    DSW(DS_tc_sfx) = 0;
    sound_vec(0, 0);
    arena_pop_low(6);
    copy_page01();
    slot_set(0, pic_get_padded(DSS(new_pics), 1));
    DSW(DS_tc_shift) = (u16)((x & 6) >> 1);
    for (s16 si = 1; si < 4; si++) {              /* the new wheel's frames */
        s16 sh = (s16)((2 * si) & (s16)~x_align());
        DSW(DS_tc_x2) = (u16)sh;
        FarPtr f;
        if (sh == 0) {
            f = pic_get_padded(DSS((u16)(new_pics + 2 * si)), 1);
        } else {
            FarPtr p = pic_get(DSS((u16)(new_pics + 2 * si)), 2);
            ds_far_wr(DS_tc_tmp, p);
            f = arena_bitmap_alloc((s16)(desc_w(p) + 8), (s16)desc_h(p), 1, 1);
            bitmap_make_shifted(p, sh, f);
            arena_pop(1);
        }
        slot_set(si, f);
    }
    for (s16 di = 0; di < 4; di++) {
        if (di == DSS(DS_tc_shift)) {
            FarPtr f = slot(di);
            cp = arena_bitmap_alloc((s16)desc_w(f), (s16)desc_h(f), 1, 1);
            slot_set(4, cp);
            bitmap_copy_into(f, cp, 3);
        }
        bitmap_composite_behind(hand, slot(di), (s16)((2 * di) & (s16)~x_align()), DSS(DS_tc_hy));
    }
    anim = sprite_scene_setup((s16)-(s16)desc_w(slot(0)), 0xA1, frames_at(DS_tc_frames), 0, 4,
                              ds_far(DS_tc_bmp_b), ds_far(DS_tc_save), 0);
    DSW(DS_tc_anim) = anim;
    anim_phase_set(anim, 0, (s16)(((s16)(desc_w(slot(0)) + x) >> 1) + 1), 2, 0);   /* rolls in */
    sound_vec(0, 1);
    DSW(DS_tc_sfx) = 8;
    while (drv_anim_step(DSW(DS_tc_anim)) != 0) wait_vretraces(2);
    DSW(DS_tc_sfx) = 0;
    sound_vec(0, 0);
    copy_page01();
    strip = ds_far(DS_tc_strip);
    tc_rect_set(strip);
    anim = sprite_scene_setup(x, 0xA1, frames_at((u16)(DS_tc_frames + 4 * DSS(DS_tc_shift))), 0, 1,
                              ds_far(DS_tc_bmp_a), strip, DS_tc_rect);
    DSW(DS_tc_anim) = anim;
    anim_phase_set(anim, 0, 8, 0, -1);            /* up 8 */
    while (drv_anim_step(DSW(DS_tc_anim)) != 0) {
        if (DSS((u16)(DSW(DS_tc_anim) + 0xE)) == 0x85) tc_hold_off_copy(slot((s16)DSS(DS_tc_shift)), slot(4));
        wait_vretraces(3);
    }
    sound_vec(14);
    arena_pop_low(6);
}

/* The jack cranking of tire_change_anim: while the car moves, the jack pictures DS:95FC[si] cycle into the car's
 * background DS:96A4; `dir` is added to the car's y on the steps where the jack changes direction (sic). up: the
 * crank sound (vector 7) is switched off after the loop only, otherwise after each step. */
static void tc_crank(u16 anim, s16 si, s16 di, s16 dir, const Rect *r3c, bool up)
{
    DSW(DS_tc_anim) = anim;
    for (;;) {
        sound_vec(7, 1);
        if (drv_anim_step(DSW(DS_tc_anim)) == 0) break;
        wait_vretraces(3);
        if (!up) sound_vec(7, 0);
        if (di == 0) {
            if (si != 0) si--;
            else { si = 1; di = 1; }
        } else if (si == 2) { si = 1; di = 0; }
        else si++;
        if (di == 0) DSW((u16)(DSW(DS_tc_anim) + 0xE)) = (u16)(DSS((u16)(DSW(DS_tc_anim) + 0xE)) + dir);
        FarPtr p = ds_far((u16)(DS_tc_jack + 4 * si));
        drv_blit_masked(p, ds_far(DS_tc_carbg), r3c, 1);
        drv_blit(p, ds_far(DS_tc_carbg), r3c, 2);
    }
    sound_vec(7, 0);
}

/* 1a21:b9bc tire_change_anim (garage.md, Change tires): jack under the car, the car goes up, each wheel is swapped
 * (tc_wheel_swap), the car goes down, the jack goes away. old_pics / new_pics: DS offsets of the 4 wheel picture ids
 * (DS_TYRE_PICS + 8 * grade). */
void tire_change_anim(u16 old_pics, u16 new_pics)
{
    u16 save_mirror = DSW(DS_g_mirror);
    arena_reset_stacks();
    cursor_ctl(-4);
    DSW(DS_g_mirror) = 0;
    DSW(DS_tc_x) = DSW(DS_anim_engine_ref);
    u16 cur = DSW(DS_cur_car);
    s16 model = DSS((u16)(cur + CR_MODEL));
    s16 fl = (s16)car_flags(cur);
    for (s16 k = 0; k < 3; k++) ds_far_wr((u16)(DS_tc_jack + 4 * k), pic_get(DSS((u16)(DS_JACK_PICS + 2 * k)), 1));
    DSW(DS_tc_ybase) = 0;                         /* PORT: Tandy would use 0x98 - jack height */
    FarPtr jack0 = ds_far(DS_tc_jack), jack1 = ds_far((u16)(DS_tc_jack + 4)), jack2 = ds_far((u16)(DS_tc_jack + 8));
    FarPtr jbg = arena_bitmap_alloc((s16)desc_w(jack1), (s16)(0xBA - TC_Y), 0, 1);
    ds_far_wr(DS_tc_jackbg, jbg);
    screen_save_rect((s16)(x_align() & 0x90), TC_Y, (s16)desc_w(jack1), (s16)(0xBA - TC_Y), jbg);
    car_compose((FarPtr *)mp(DGROUP, DS_tc_car), model, fl, 0, 0, 0, 1);
    DSW(DS_tc_wrear) = DSB((u16)(DS_MODEL_LAYOUT + 12 * model));
    DSW(DS_tc_wfront) = DSB((u16)(DS_MODEL_LAYOUT + 1 + 12 * model));
    FarPtr car = ds_far(DS_tc_car);
    s16 wy = (s16)(desc_h(car) - 0x1A);           /* [bp-?]: y of the wheels in the car picture */
    {
        s16 r[4] = { (s16)desc_w(car), 0x1A, 0, wy };
        ds_far_wr(DS_tc_bottom, bitmap_extract(car, r, 1));      /* the bottom of the car */
    }
    show_picture(0x3FA, -1);                      /* the garage backdrop piece (not mirrored) */
    {
        Rect q = { (s16)desc_w(jack1), (s16)desc_h(jack1), 0x90, (s16)(0xBA - desc_h(jack1)), 0,
                   (s16)(0xBA - desc_h(jack1) - TC_Y) };
        drv_blit(g_back(), jbg, &q, 1);
    }
    FarPtr sh = pic_get(0x472, 2);                /* ground shadow */
    ds_far_wr(DS_tc_shadow, sh);
    s16 left, right;
    bitmap_opaque_extent(car, &left, &right);
    FarPtr shsave = arena_bitmap_alloc((s16)desc_w(car), (s16)desc_h(sh), 0, 2);
    ds_far_wr(DS_tc_shsave, shsave);
    screen_save_rect(TC_X, (s16)(0x9F - desc_h(sh)), (s16)desc_w(car), (s16)desc_h(sh), shsave);
    FarPtr sq = arena_bitmap_alloc((s16)(desc_w(sh) + 8), (s16)desc_h(sh), 1, 2);
    ds_far_wr(DS_tc_shq, sq);
    bitmap_make_shifted(sh, (s16)(left & (s16)~x_align()), sq);
    Rect r = { (s16)desc_w(sq), (s16)desc_h(sq), 0, 0, (s16)(left & x_align()), 0 };
    drv_blit_masked(sq, shsave, &r, 1);
    arena_pop(1);
    sq = arena_bitmap_alloc((s16)(desc_w(sh) + 8), (s16)desc_h(sh), 1, 2);
    ds_far_wr(DS_tc_shq, sq);
    bitmap_make_shifted(sh, (s16)((right + 1) & (s16)~x_align()), sq);
    r.w = (s16)desc_w(sq);
    r.dx = (s16)((s16)(u16)~x_align() - r.w + right + 1);
    drv_blit_masked(sq, shsave, &r, 1);
    arena_pop(1);
    r.w = (s16)desc_w(shsave);
    r.dx = TC_X;
    r.dy = (s16)(0x9F - r.h);
    drv_blit(shsave, g_back(), &r, 1);
    arena_pop(2);
    DSW(DS_tc_ybase) = 0;
    FarPtr carbg = arena_bitmap_alloc((s16)desc_w(car), (s16)(0x9F - TC_Y), 1, 1);
    ds_far_wr(DS_tc_carbg, carbg);
    screen_save_rect(TC_X, TC_Y, (s16)desc_w(car), (s16)(0x9F - TC_Y), carbg);
    far_memset(desc_mask(carbg), 0xFF, (u16)(desc_size(carbg) / DSW(DS_g_nplanes)));
    Rect r3c = { (s16)desc_w(jack1), (s16)desc_h(jack1), 0, 0, (s16)(0x90 - TC_X), (s16)(0x9B - desc_h(jack1) - TC_Y) };
    drv_blit_masked(jack0, carbg, &r3c, 1);
    drv_blit(jack0, carbg, &r3c, 2);
    ds_far_wr(DS_tc_hub, pic_get(0x47C, 2));      /* the hub */
    DSW(DS_tc_ybase) = 0;
    FarPtr front_strip = tc_wheel_strip(DSS(DS_tc_wfront));
    FarPtr rear_strip = tc_wheel_strip(DSS(DS_tc_wrear));
    arena_pop(1);
    car = ds_far(DS_tc_car);
    FarPtr save = arena_bitmap_alloc((s16)(desc_w(car) + TC_X), 0x1A, 0, 1);
    ds_far_wr(DS_tc_save, save);
    screen_save_rect(0, 0x88, (s16)(desc_w(car) + TC_X), 0x1A, save);
    {
        Rect q = { (s16)desc_w(car), (s16)(desc_h(save) - 8), 0, (s16)(desc_h(car) - 0x13), TC_X, 0 };
        drv_blit_masked(car, save, &q, 1);
        Rect q2;
        q2.w = (s16)desc_w(jack1);
        q2.h = (s16)desc_h(save);
        q2.sx = 0;
        q2.sy = (s16)(7 - (s16)(q2.h - desc_h(jack1)));
        q2.dx = 0x90;
        if (q2.sy < 0) { q2.dy = (s16)-q2.sy; q2.sy = 0; }
        else q2.dy = 0;
        drv_blit_masked(jack2, save, &q2, 1);
    }
    if (DSS(DS_driver_id) == -2) {                /* the backgrounds cached in VRAM pages 2 / 3 */
        FarPtr d = far_make(DGROUP, DS_tc_desc);
        ds_far_wr(DS_tc_save_car, d);
        bitmap_init(d, desc_planes(g_page(2)), FAR_NULL, (s16)desc_w(car), 200, -2);
        bitmap_init(ds_far(DS_tc_bmp_a), planes_plus(g_page(2), desc_stride(car)), FAR_NULL, (s16)desc_w(front_strip), 200, -2);
        bitmap_init(ds_far(DS_tc_bmp_b), desc_planes(g_page(3)), FAR_NULL, 0x140, (s16)desc_h(save), -2);
        bitmap_init(ds_far(DS_tc_bmp_c), desc_planes(g_page(3)), FAR_NULL, (s16)desc_w(jbg), 200, -2);
        Rect q = { (s16)desc_w(carbg), (s16)(desc_h(car) + 4), 0, (s16)(0x9B - desc_h(car)), 0, (s16)(0x9B - desc_h(car)) };
        drv_blit(carbg, d, &q, 1);
        q.w = (s16)desc_w(front_strip); q.h = 0x21; q.sx = 0; q.sy = 0x81; q.dx = 0; q.dy = 0x81;
        drv_blit(rear_strip, ds_far(DS_tc_bmp_a), &q, 1);
        q.w = (s16)desc_w(save); q.h = (s16)desc_h(save); q.sy = 0; q.dy = 0;
        drv_blit(save, ds_far(DS_tc_bmp_b), &q, 1);
        q.w = (s16)desc_w(jbg); q.h = (s16)(desc_h(jack0) + 0x20); q.sy = (s16)(0x9A - desc_h(jack0)); q.dy = q.sy;
        drv_blit(jbg, ds_far(DS_tc_bmp_c), &q, 1);
    }
    /* PORT: the non-VGA branch (copies of the descriptors DS:79EC.. made from the strips) is not ported */
    /* the jack comes up */
    DSW(DS_tc_ybase) = 0;
    copy_page01();
    u16 anim = sprite_scene_setup(0x90, 0xB9, frames_at((u16)(DS_tc_jack + 4)), 0, 1, ds_far(DS_tc_bmp_c), jbg, 0);
    DSW(DS_tc_anim) = anim;
    anim_phase_set(anim, 0, 0x20, 0, -1);
    if (DSW(DS_snd_device) == 0) music_stop();
    sound_vec(6, 1);
    DSW(DS_tc_sfx) = 0x10;
    while (drv_anim_step(DSW(DS_tc_anim)) != 0) wait_vretraces(3);
    DSW(DS_tc_sfx) = 0;
    sound_vec(6, 0);
    sound_vec(22, 1);                             /* mute during the lift */
    DSW(DS_tc_ybase) = 0;
    /* the old wheels on the car picture */
    FarPtr ow = pic_get(DSS(old_pics), 2);
    ds_far_wr(DS_tc_tmp, ow);
    bitmap_composite_behind(ow, car, DSS(DS_tc_wrear), wy);
    bitmap_composite_behind(ow, car, DSS(DS_tc_wfront), wy);
    arena_pop(1);
    copy_page01();
    /* the car goes up 7 */
    tc_rect_set(carbg);
    anim = sprite_scene_setup(TC_X, 0x9E, frames_at(DS_tc_car), 0, 1, ds_far(DS_tc_save_car), carbg, DS_tc_rect);
    anim_phase_set(anim, 0, 7, 0, -1);
    tc_crank(anim, 0, 0, 1, &r3c, true);
    DSW(DS_tc_ybase) = 0;
    /* rear wheel */
    ds_far_wr(DS_tc_strip, rear_strip);
    tc_wheel_swap((s16)(DSS(DS_tc_wrear) + TC_X), old_pics, new_pics);
    ds_far_wr(DS_tc_strip, front_strip);
    {
        Rect q = { (s16)desc_w(front_strip), 0x21, 0, 0x81, 0, 0x81 };
        drv_blit(front_strip, ds_far(DS_tc_bmp_a), &q, 1);
        FarPtr s28 = ds_far(DS_tc_save);
        Rect q2 = { 0x28, (s16)desc_h(s28), (s16)(DSS(DS_tc_wrear) + TC_X), 0, (s16)(DSS(DS_tc_wrear) + TC_X), 0 };
        q2.sy = (s16)(0xA2 - q2.h);
        drv_blit(g_front(), s28, &q2, 1);
        drv_blit(g_front(), ds_far(DS_tc_bmp_b), &q2, 1);
    }
    /* front wheel */
    tc_wheel_swap((s16)(DSS(DS_tc_wfront) + TC_X), old_pics, new_pics);
    DSW(DS_tc_ybase) = 0;
    /* the new wheels on the car picture */
    FarPtr nw = pic_get(DSS(new_pics), 1);
    slot_set(0, nw);
    car = ds_far(DS_tc_car);
    {
        Rect q = { (s16)desc_w(car), (s16)desc_h(nw), 0, 0, 0, (s16)(desc_h(car) - desc_h(nw)) };
        drv_blit(ds_far(DS_tc_bottom), car, &q, 3);
        bitmap_composite_behind(nw, car, DSS(DS_tc_wrear), wy);
        bitmap_composite_behind(nw, car, DSS(DS_tc_wfront), wy);
    }
    copy_page01();
    /* the car goes down 9 */
    drv_blit_masked(jack2, carbg, &r3c, 1);
    drv_blit(jack2, carbg, &r3c, 2);
    tc_rect_set(carbg);
    anim = sprite_scene_setup(TC_X, 0x9A, frames_at(DS_tc_car), 0, 1, ds_far(DS_tc_save_car), carbg, DS_tc_rect);
    anim_phase_set(anim, 0, 9, 0, 1);
    tc_crank(anim, 2, 1, -1, &r3c, false);
    sound_vec(22, 0);                             /* unmute */
    DSW(DS_tc_ybase) = 0;
    copy_page01();
    /* the jack goes down */
    anim = sprite_scene_setup(0x90, 0x9A, frames_at((u16)(DS_tc_jack + 4)), 0, 1, ds_far(DS_tc_bmp_c), jbg, 0);
    DSW(DS_tc_anim) = anim;
    anim_phase_set(anim, 0, 0x20, 0, 1);
    sound_vec(6, 1);
    DSW(DS_tc_sfx) = 0x10;
    while (drv_anim_step(DSW(DS_tc_anim)) != 0) wait_vretraces(3);
    DSW(DS_tc_sfx) = 0;
    sound_vec(6, 0);
    if (DSW(DS_snd_device) == 0 && DSW(DS_music_on) != 0) music_resume();
    arena_reset_stacks();
    drv_copy_page(desc_planes(g_front()), desc_planes(g_back()));
    DSW(DS_anim_engine_ref) = DSW(DS_tc_x);
    DSW(DS_g_mirror) = save_mirror;
    DSW(DS_tc_ybase) = 0;
    cursor_ctl(-2);
}
