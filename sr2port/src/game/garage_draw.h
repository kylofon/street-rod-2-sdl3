#pragma once
/* Public API of the garage drawing code: the engine-bay and transmission pictures, the car pictures and their
 * animations (car_draw, car_compose, the car-hop, the tyre change), the sprite-scene frame scripts and the
 * "Under the Car" pictures. Specs: port/spec/garage.md (functions 1a21:2e92 - 1a21:3c14, 1a21:8268 - 1a21:a25e,
 * 1a21:b16e - 1a21:b9bc, 2925:2b26 - 2925:2ff8) and port/spec/video.md (1a21:9d38 sprite_scene_setup).
 *
 *   garage_bay_draw.c  trans_* and bay_* drawers, under_car_*
 *   garage_draw.c      car_strip_setup, car_compose, car_draw, walker_anim, carhop_*, town_bg_save
 *   garage_anim.c      sprite_scene_setup, anim_ring_reverse / anim_phase_set / anim_phases / anim_script
 *   garage_tire.c      tire_change_anim (+ tc_wheel_strip / tc_wheel_swap)
 *
 * Conventions (PORTING.md): car and part records are DS offsets (u16); tyre_pics = DS offset of 4 wheel picture ids
 * (DS_TYRE_PICS + 8 * grade), 0 = none. Descriptors are FarPtr. */
#include "types.h"
#include "mem.h"
#include "symbols.h"

/* ============================================================ transmission screen (garage.md, change_transmission) */

void trans_bolt_draw(s16 tight, s16 bolt);            /* 1a21:2e92: bolt 0/1 at tightness 0..3 (1 slides in) */
void trans_bolt_remove_anim(s16 bolt);                /* 1a21:2fc4 */
void trans_slide_anim(s16 id, s16 reject);            /* 1a21:30f2: picture `id` slides in; reject: stops 16 px short */
void trans_remove_anim(void);                         /* 1a21:322e (clank, panel picture 0x41F); the original's args are unused */
void trans_screen_draw(s16 pic, u16 bolts);           /* 1a21:328c: pic 0 = nothing; bolts = DS offset of 2 tightness bytes */

/* ============================================================== engine bay (engine_bay, ignition_tune) */

s16  bay_engine_idx(void);                            /* 1a21:332e: 0..2 for the engine pictures 0x425..0x427 (else value - 0x427) */
s16  bay_carb_pos(s16 *x, s16 *y);                    /* 1a21:334a: carburettor seat of context DS:AD9C; returns 0/1/2 (picture 0x42D..0x42F) */
void bay_part_pos(s16 *x, s16 *y);                    /* 1a21:33aa: seat of the part of context DS:AD9C (bay coordinates) */
void bay_bolt_pos(s16 *x, s16 *y);                    /* 1a21:33fe: position of bolt side DS:ADA0 of context DS:AD9C */
void bay_bolt_draw(s16 tight);                        /* 1a21:34a2: SR1 took a picture id; SR2 takes the tightness 1..3 */
void bay_bolt_remove_anim(void);                      /* 1a21:369a */
void bay_part_in_anim(s16 pic, s16 reject);           /* 1a21:381e */
void bay_part_out_anim(void);                         /* 1a21:39a4: SR1 took the picture id; no arguments now */
void bay_connector_draw(void);                        /* 1a21:3a5a */
void bay_draw_car(u16 car);                           /* 1a21:3b34 */
void bay_draw_parts(s16 own, s16 clank);              /* 1a21:3c14: SR1 (ctx_tbl, side_tbl, own); now (own, clank) */

/* ============================================================== Under the Car (garage code 14) */

void under_car_slide_in(void);                        /* 2925:2b26: the jack lifts the car (near function of segment 2925) */
void under_car_draw(void);                            /* 2925:2e60: backdrop 0x4AF (kept in DS:9718) + fitted exhaust / muffler / differential */
void under_car_part_pic(s16 cat, s16 type);           /* 2925:2ff8: redraw part category 5 / 6 / 7; type < 0 erases it */

/* ================================================================ car pictures (1a21:7f5c - 1a21:98e2) */

/* 1a21:8546: frames[0] = the car body with its customisations and the driver (picture DS:AD74), frames 1..n-1 each
 * 2 px further right; wheels tyre_pics (0 = none), smoke: the ground shadow 0x472. Also sets the gas-cap point
 * DS:AD62/AD64 to the layout point. SR1 had a sticker argument (gone). */
void car_compose(FarPtr *frames, s16 model, s16 flags, s16 driver, u16 tyre_pics, s16 smoke, s16 n);
/* 1a21:89fa: draw / animate a car: mode 1 drive in from the left (race), 2 leave to the right, 3 in place, 4 drive in,
 * 6 drive out, 7 static (See it), 8 static redraw. y = ground line (0x9E in the garage). Sets DS:AD5C/AD5E/AD60
 * (car x / y / h) and the gas-cap point DS:AD62/AD64. SR1 also took sticker and smoke arguments (gone). */
void car_draw(s16 mode, s16 model, s16 flags, s16 driver, s16 y, u16 tyre_pics);
/* 1a21:831c: background of a driving car: rows top-h .. top of `strip`; *out_bgsrc = a copy, *out_bg = the shared strip
 * descriptor (VGA); *out_ov = 0 or 0x790A (the smoke overlay rect). SR1 had two overlay outputs. */
void car_strip_setup(FarPtr strip, FarPtr *out_bgsrc, s16 smoke_pic, FarPtr *out_bg, s16 top, s16 h, u16 *out_ov);
/* 1a21:9216: rows y .. y+h-1 of `pic` (source sx, sy, width w) into the car strip DS:7906 (top row base - 0x45) at x dx; at
 * Bob's the background columns DS:5070 are refreshed from the front page. SR1 had a ninth, unused argument. */
void walker_anim(FarPtr pic, s16 sx, s16 sy, s16 w, s16 h, s16 dx, s16 y, s16 base);
/* 1a21:945a: Bob's car-hop walks in (mode 0) / away (mode 1); opp != 0: the opponent's car (DS:AD58, AD5C/AD5E) in front. */
void carhop_anim(s16 mode, s16 opp);
/* 1a21:98e2: snapshot of the town background behind the car into DS:95EE (0x28 x 0x34 from page B at (0x68, 0x52)). */
void town_bg_save(void);
/* 1a21:8268: pump slice idx of the sheet DS:B89A over the car view into dst (car_draw mode 4 at the gas station). */
void pump_overlay(s16 x, s16 y0, s16 w, s16 h, FarPtr dst, s16 idx);

/* 1a21:b9bc: jack under the car, the car goes up, each wheel is swapped, the car goes down, the jack goes away.
 * old_pics / new_pics: DS offsets of the 4 wheel picture ids (DS_TYRE_PICS + 8 * grade). */
void tire_change_anim(u16 old_pics, u16 new_pics);

/* ============================================================== sprite scenes (video.md, garage.md) */

/* 1a21:9d38: sprite animation of n frames for the driver slot 9 (drv_anim_step): `frames` = C array of descriptors,
 * tails = DS offset of 8-byte Tail records or 0, `a` / `b` = the two background descriptors the Anim keeps at +2 / +6
 * (car_draw passes the shared strip, then its copy), ov = DS offset of the overlay rect or 0. (x, y) = bottom-left;
 * the Anim is placed at (x, y + 1 - frames[0].h). Returns the Anim, DS:A544. SR1 had two overlay arguments. */
u16  sprite_scene_setup(s16 x, s16 y, const FarPtr *frames, u16 tails, s16 n, FarPtr a, FarPtr b, u16 ov);
void anim_phase_set(u16 anim, s16 i, s16 count, s16 dx, s16 dy);   /* 1a21:a0d4 (script pointer at Anim + 0x10) */
void anim_ring_reverse(u16 anim);                                  /* 1a21:a016 */
void anim_phases(u16 anim, s16 mode, s16 d1, s16 d2, s16 d3, s16 dist, s16 p1, s16 p2);   /* 1a21:a11e */
void anim_script(u16 anim, s16 mode, s16 dist);                    /* 1a21:a25e */

/* ===================================================================== used from other slices */

/* garage_data.c / garage.h: */
u16  car_flags(u16 car);                                           /* 0001:4804 */
s16  car_runnable(u16 car, s16 full);                              /* 0001:7a44 */
/* ui slice (1a21:2888): copies the back page to the front page. */
void present_back_page(void);
