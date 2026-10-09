#pragma once
/* Public API of the game_flow state / clock / save / end / data part of Street Rod 2 (port/spec/game_flow.md,
 * garage.md for the functions the garage spec owns). Rewritten from the SR1 header for SR2 (S5.2, slice W1c).
 *
 * Implemented in game/flow*.c:
 *   flow_state.c    part/car pools 0001:3f28-4250, garage_full_check 3e08, str_is_blank 5494, trim_trailing_spaces 54ba,
 *                   opponents / new_game / new_game_prompt 51f8-550e, broke_check 68e2, cheapest_ad_price 0e94
 *   flow_clock.c    day_index 63e0 ... date_print 66ae, classified_week 640e, summer_over_screen 1a21:0fe8
 *   flow_save.c     save_part_chain 55b0 ... quit_menu 630e (?:HOTROD<n>.SAV)
 *   flow_end.c      copy_protection 3378, game_over_menu 348a, hall of fame 35e8-36ac, ending_trigger a51c,
 *                   end_data_load / ending_anim 2925:2714 / 27da, tournament_time_text b2ea / result message b356
 *   flow_data.c     rnd 1a21:6180, title_sequence 1a21:0e82, title_and_setup 3cc4, hot_data_load 1a21:6e6a,
 *                   data_disk_check 7052, hall_scramble / unscramble / load / save 70fc-71cc
 *   flow_util.c     MS C runtime string helpers on DGROUP strings
 * Not here (platform, S5.1): pools_init 0001:3934 (platform/memory.c), input_reset, wait_click_or_key.
 * Not here (wave 2): game_loop 4dd6, game_main, event15_run, event17_run and the tournament screens. */
#include "types.h"
#include "mem.h"
#include "symbols.h"
#include "game/ds_flow.h"

/* ---- clock constants (game_clock units) */
#define FLOW_GAME_HOUR   0x222u   /* game_clock units per game hour (546 rt ticks, 30 s) */
#define FLOW_GAME_DAY    0x1998u  /* 12 game hours */
#define FLOW_GAME_OVER   0x29A    /* garage_screen: the game is over (DS_game_over != 0) */

/* DS:1BC6 (DS_game_over) flags */
#define FLOW_END_SUMMER  0x01
#define FLOW_END_KING    0x02
#define FLOW_END_DEBTS   0x04
#define FLOW_END_BROKE   0x10

#define FLOW_PEOPLE_N    22       /* DS_people: 22 x 0x12 (21 opponents + the King) */
#define FLOW_PERSON_SIZE 0x12

/* The inline `game_clock += 0x222` of game_loop (u32 add with carry). Callers adding other amounts
 * (0x111, 0x444 ...) use flow_clock_add. */
static inline void flow_clock_add(u32 units) { DSL(DS_game_clock) += units; }
static inline void clock_hour(void) { flow_clock_add(FLOW_GAME_HOUR); }

/* ---- objects (the pools are reset by pools_init 0001:3934, platform.h); no bounds checks */
u16  part_alloc(void);                 /* 0001:3f28: pop DS:AA0C, part_count++ (returns a DS offset) */
void part_free(u16 p);                 /* 0001:3f3e: push, +4 = -1, part_count-- */
u16  car_alloc(void);                  /* 0001:423a: pop DS:AA06, car_count++ (link at +0x28) */
void car_release(u16 c);               /* 0001:4250: push, +2 = -1, car_count-- */

/* ---- messages and checks */
s16  garage_full_check(void);          /* 0001:3e08: >= 16 cars (msg 0x22) / >= 0xB5 parts (0x23) -> 1 */
s16  broke_check(void);                /* 0001:68e2: nothing owned and cheapest_ad_price() > money */
s32  cheapest_ad_price(void);          /* 0001:0e94: cheapest unsold car ad (DS:9B42 list), 0 for an empty list */
s16  str_is_blank(u16 s);              /* 0001:5494: every character is ' ' or '_' (s = DS offset) */
s16  trim_trailing_spaces(u16 s);      /* 0001:54ba (spec game_flow.md: str_rtrim): returns the blanks removed */

/* ---- new game */
void opponent_init(s16 i, s16 *models);/* 0001:51f8: models[0] = count, models[1..] (used ones -> -1) */
void opponents_init(void);             /* 0001:534c */
void new_game(void);                   /* 0001:5400: pools, money, opponents, clocks, counters, ad block */
s16  new_game_prompt(void);            /* 0001:550e: 0 new game, -2 "Old Game", other codes as returned */

/* ---- calendar and clock (T = game_clock + rt_ticks - rt_base) */
s16  day_index(void);                  /* 0001:63e0: 0..91 */
s16  classified_week(void);            /* 0001:640e: min(day_index() / 7, 11) */
void clock_hm(s16 *hour, s16 *min);    /* 0001:6426: hour 0..11, minute 0..59 */
void summer_over_check(void);          /* 0001:64a2: day > 90 -> DS:1BC6 |= 1 */
s16  month_of_day(s16 day, s16 *dom);  /* 0001:64b2: 0 June .. 3 September */
void calendar_show(s16 final);         /* 0001:650a */
void date_print(s16 y);                /* 0001:66ae: "%s %d, 1969" at x 0x61 */
void summer_over_screen(void);         /* 1a21:0fe8 */

/* ---- game over, hall of fame, ending */
s16  game_over_menu(void);             /* 0001:348a: -2 load, -1 new, 4 quit */
void hall_of_fame_screen(s16 won);     /* 0001:36ac */
s16  copy_protection(void);            /* 0001:3378: asks, never checks, returns 1 */
void ending_trigger(void);             /* 0001:a51c: after the King's race (the caller shows msg 0x44, DS:1BC6 |= 2) */
void end_data_load(void);              /* 2925:2714: SR2_end into the SR2_SHP arena */
void ending_anim(void);                /* 2925:27da: the SR2_end player */
void tournament_time_text(s16 t, s16 msg);       /* 0001:b2ea: patches "%2d.%d" into the template of message msg */
void tournament_result_message(s16 mode);        /* 0001:b356 (mode 2 = qualifier) */

/* ---- save / load */
s16  load_game_screen(void);           /* 0001:59e2: 0 loaded, -1 cancelled, -0x29A read error */
void save_game_screen(void);           /* 0001:5d82 */
s16  quit_menu(void);                  /* 0001:630e: Save -0x17, Old -2, New -1, Quit -0x14, Forget it -0x16 */

/* ---- start-up and data files */
s16  rnd(s16 n);                       /* 1a21:6180 (platform.h): 32-bit LCG */
void title_sequence(void);             /* 1a21:0e82 */
s16  title_and_setup(void);            /* 0001:3cc4: returns -1 (the code that makes game_loop run new_game_prompt) */
void hot_data_load(void);              /* 1a21:6e6a (platform.h) */
char data_disk_check(void);            /* 1a21:7052: drive letter of the data disk */
void hall_load(void);                  /* 1a21:711c (platform.h) */
void hall_save(void);                  /* 1a21:71cc */
void hall_scramble(void);              /* 1a21:70fc */
void hall_unscramble(void);            /* 1a21:710c */

/* ---- functions of other slices that this slice calls */
void msg_box(s16 id);                  /* 0001:3db4 (UI slice) */
void wait_ticks(s16 n);                /* 0001:3d80 (UI slice): returns after n ticks or on input */

/* MS C 5.1 runtime helpers on DGROUP strings (flow_util.c); all arguments are DS offsets */
void fl_strcpy(u16 dst, u16 src);                  /* strcpy */
void fl_strncpy(u16 dst, u16 src, u16 n);          /* strncpy (no NUL when src is longer) */
void fl_strcat(u16 dst, u16 src);                  /* strcat */
s16  fl_strcmp(u16 a, u16 b);                      /* strcmp: <0 / 0 / >0 */
u16  fl_strlen(u16 s);                             /* strlen */
u16  fl_strchr(u16 s, u8 c);                       /* strchr: DS offset of the match or 0 */
s16  fl_atoi(u16 s);                               /* 34b0:1680 atoi (16-bit wrap) */
void fl_itoa(s16 v, u16 dst);                      /* 34b0:16dc itoa(v, dst, 10) */
void fl_memmove(u16 dst, u16 src, u16 n);          /* memmove */
u16  fl_save_name(u16 buf, u16 tmpl, s16 n);       /* {drive, tmpl ":HOTROD\0"} + itoa(n) + ".SAV" (tmpl + 8) */
