#pragma once
/* Public API of the garage screen and its sub-screens (S5.2 slice W2b; port/spec/garage.md sections 0001:6796-6912,
 * 6d5e-7196, 71f6-7a44 (without car_runnable), 7aec, 80a8, 1a21:3d10 and 2925:3174).
 *
 * Implemented in game/garage_main.c   garage_hotspots_nocar 6796, garage_hotspots_car 67b8, garage_name_clear 68c0,
 *                                     garage_screen 6912
 *                game/garage_paint.c  set_paint_palette 672c, paint_job 6d5e, customize 6edc
 *                game/garage_tires.c  change_tires 71f6
 *                game/garage_info.c   car_info_sr2 72f2
 *                game/garage_trans.c  change_transmission 75b8
 *                game/garage_bay.c    ignition_tune 7aec, engine_bay 80a8, bay_hotspots 1a21:3d10
 *                game/garage_undercar.c under_car_menu 2925:3174
 *
 * Conventions (sr2port/PORTING.md): car and part records are near pointers of the original = u16 DS offsets
 * (0 = none). The "model + shops" header game/garage.h and the UI header game/ui.h are other slices'. */
#include "types.h"
#include "mem.h"

/* The garage screen: mode 0 only the hot spots, 1 full redraw (new car / trip), 2 redraw after a trip with the car
 * "parked", 3 after "Switch it". Returns the UI code of the clicked hot spot or FLOW_GAME_OVER (0x29A). */
s16  garage_screen(s16 mode);
void garage_hotspots_nocar(void);                  /* 0001:6796 */
void garage_hotspots_car(s16 model, s16 x, s16 y); /* 0001:67b8 */
void garage_name_clear(void);                      /* 0001:68c0 */
void set_paint_palette(s16 colour);                /* 0001:672c: EGA registers 6/7 from PAINT[colour] */

void paint_job(void);                              /* 0001:6d5e: code 7 */
void customize(s16 what);                          /* 0001:6edc: codes 11/12/13 -> 2 rear bumper, 1 front bumper, 4 roof */
void change_tires(void);                           /* 0001:71f6: code 8 */
void car_info_sr2(void);                           /* 0001:72f2: code 16 "Car info" (hand drawn) */
#define car_info car_info_sr2                      /* SR1 name (garage.h still declares car_info(void)) */
void change_transmission(void);                    /* 0001:75b8: code 9 */
void ignition_tune(u16 car);                       /* 0001:7aec: the timing-light screen, called by engine_bay "Tune" */
void engine_bay(u16 car);                          /* 0001:80a8: code 10 (cur_car); race views an opponent's car (view only) */
s16  under_car_menu(void);                         /* 2925:3174: code 14 "Under the Car"; returns 2 (Done), 0 (no car) */

/* 1a21:3d10. PORT: ctx_tbl / side_tbl are DS offsets of the two word tables (the original passes near pointers):
 * engine_bay passes 0x4E8E and 0x4E9C. */
void bay_hotspots(u16 ctx_tbl, u16 side_tbl);
