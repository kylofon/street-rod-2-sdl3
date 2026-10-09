#pragma once
/* The game loop's event trips (port/spec/game_flow.md): the gas station (event15, game_loop code 15), Grudge Night
 * (event17, code 17) and the race-module entry points game_flow calls (game/race_*.c).
 *
 * Implemented in game/flow_loop.c (game_loop, game_main), event15.c (2925:1ab8-2104), event17.c (2925:000e-1970,
 * the bracket logic) and event17_screens.c (2925:0522-16c8, the framed screens), game/race_*.c (race module). */
#include "types.h"
#include "mem.h"

/* ---- game loop 0001:4dd6 (flow_loop.c) */
void game_loop(void);                          /* main's game part; returns when the player quits */

/* ---- gas station (event15.c) */
void event15_run(void);                        /* 2925:2104: game_loop code 15 */
/* 2925:1cae: pump strip k (0..3) of the clean background DS:B89A onto `page`; also modules.event15_overlay
 * (ega_anim_step draws it over the driving car at the gas station) */
void pump_strip_draw(FarPtr page, s16 k);
void pump_dial_anim(void);                     /* 2925:1c10 (far) */
void counters_draw(s16 pump);                  /* 2925:1e08 (far): the LED gallon / money counters of pump 1 or 2 */
void fuel_sound(s16 pump, s16 op);             /* 2925:20be (far): 1 start, 2 count one step, 3 stop */

/* ---- Grudge Night (event17.c / event17_screens.c) */
void event17_run(void);                        /* 2925:1970: game_loop code 17 */
void tournament_pick_field(void);              /* 2925:000e */
void tournament_seed(void);                    /* 2925:00fa */
void tournament_next_round(void);              /* 2925:01fa */
void tournament_finish(void);                  /* 2925:1786 */
void event17_leave(s16 last_result);           /* 2925:193e */
void tournament_intro_screen(void);            /* 2925:1642 */
void tournament_round_screen(void);            /* 2925:16c8 */
void tournament_round_header(void);            /* 2925:12f2 */
void tournament_entrants_list(void);           /* 2925:13d0 */
void tournament_standings(const s16 *ids);     /* 2925:14b6: 4 person ids (-1 = player), best first */
void tournament_screen_build(s16 with_photo);  /* 2925:0a0c */
void panel_frame_draw(FarPtr dst, FarPtr border);                         /* 2925:0522 */
void bitmap_remap_colors(FarPtr b, s16 from1, u16 to1, s16 from2, u16 to2);  /* 2925:0812 (EGA planes) */
s16  clip_rect_split(const s16 *clip, const s16 *r, s16 *out1, s16 *out2);   /* 2925:0c4c: r / outN are s16[10] */
void panel_wipe(u16 mode, FarPtr bmp, s16 x, s16 y, const s16 *clip);       /* 2925:0d02 */

/* ================================================================================ race module entry points
 * (port/spec/race.md; implemented in game/race_*.c, more in game/race.h) */
void drive_to_event(void);                     /* 0001:8c68: the cruise scene to / from an event (location DS:1BC8) */
void opp_palette_set(s16 idx);                 /* 0001:8d0a: opponent car colour in palette registers 6/7 (-1 = restore) */
void bob_drive_in(void);                       /* 0001:a626: Burger's Drive-In (street race, Mulholland, Aqueduct, the King) */
void opponent_load(s16 i);                     /* 0001:9a02: load person i into DS:A80A / A7AA, ... */
void race_start(void);                         /* 0001:be82: the race itself (Grudge Night rounds, DS:1FFC = 1) */
u16  race_results(void);                       /* 0001:b584: result handling; 0 lost, 1 won, 2 = disqualified / left the event */
s16  car_max_speed(s16 model);                 /* 0001:c6f6 (-1 = the player's current car) */
void car_setup(u16 car, u16 phys);             /* 0001:c294: car record -> physics block (game_loop passes DS:A100) */
