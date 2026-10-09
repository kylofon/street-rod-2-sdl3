#pragma once
/* Public API of the garage "shop" subsystem (port/spec/garage.md): the car / part model, the newspaper with the
 * classified ads, selling parts and cars. Other game modules include this header.
 *
 * Implemented in game/garage*.c (this slice, S5.2 W2a):
 *   garage_data.c         spare_add 426c, car_new 42aa, part_release 46f0, car_free 4716, car_flags 4804, spares_collect 70d2, part_install 715a, part_uninstall 7196,
 *                         spare_unlink 71be, car_runnable 7a44
 *   garage_classifieds.c  classifieds_open 0a68 ... classifieds 11a2, newspaper_front 3e30
 *   garage_shop.c         sell_spare_parts 3f5a, how_about 478e, your_cars 4852
 * Other slices: the garage screen and its screens (game/garage_screens.h), the car drawing (game/garage_draw.h),
 * the UI toolkit (game/ui.h), pools / alloc / calendar / broke_check / cheapest_ad_price / msg_box (game/flow.h).
 *
 * Conventions (PORTING.md): car and part records are near pointers of the original = u16 DS offsets (0 = none);
 * a "slot" is the DS offset of a car's part pointer (car + CR_ENGINE ...). The record layouts and the part /
 * model table accessors are in game/records.h. Player state: DS_cur_car, DS_car_list (the other cars),
 * DS_spare_parts, DS_car_count, DS_part_count, DS_money (symbols.h). */
#include "types.h"
#include "mem.h"
#include "symbols.h"
#include "game/records.h"

/* ============================================================ car and part model */

void spare_add(s16 type, s16 value);          /* 0001:426c: a new spare part of catalogue `type` (wear by category) */
u16  car_new(s16 model, s16 value, u16 reuse);/* 0001:42aa: stock car (reuse != 0: refill that record and its parts) */
s16  part_release(u16 p);                     /* 0001:46f0: frees the node, returns its value (0 for none) */
s16  car_free(u16 car);                       /* 0001:4716: frees car + all parts, returns max(0, remaining value) */
u16  car_flags(u16 car);                      /* 0001:4804: picture flags 4 roof, 2 rear, 1 front, 0x20 no muffler, 8 supercharger */
/* 0001:70d2: spares of category `cat` after n0 reserved entries: nodes[n0..] (C array of DS offsets), wear[n0..]
 * (C array: wear / 100 or 0x80), DS_list_entries[n0..] (catalogue name pointers); *count = total entries. */
void spares_collect(s16 cat, u16 *nodes, s8 *wear, s16 n0, s16 *count);
void part_install(u16 part, u16 slot);        /* 0001:715a: slot = DS offset of the car's part pointer; cur_car value updated */
void part_uninstall(u16 slot);                /* 0001:7196 */
u16  spare_unlink(u16 part);                  /* 0001:71be: takes the part off the spare list, returns it (callers: part_install(spare_unlink(p), slot)) */
s16  car_runnable(u16 car, s16 full);         /* 0001:7a44 */

/* ============================================================ newspaper, shops, selling */

void newspaper_front(void);                   /* 0001:3e30: front page; menu 3 (1 Used Cars, 2 Auto Parts) runs classifieds() itself */
void classifieds(s16 section, s16 reroll);    /* 0001:11a2: 1 Used Cars, 2 Auto Parts; reroll != 0: a new week of ads */
void sell_spare_parts(void);                  /* 0001:3f5a: garage code 5 */
s16  your_cars(void);                         /* 0001:4852: garage code 6; returns the next garage mode (0 / 3) */
s16  how_about(u16 *offer);                   /* 0001:478e: "How about $%d ?" -20 OK / -21 No */

/* the classified ad pages (garage_classifieds.c, also called through classifieds()) */
void classifieds_open(u16 list, s16 first_row);   /* 0001:0a68: list = DS offset of the line-count array (DS_ad_lines) */
s16  classifieds_run(void);                   /* 0001:0ac8: -3 garage, else the 1-based row of the clicked ad */

/* ============================================================ accessors for the other garage slices */

/* the part a car carries in `slot_off` (CR_TRANS, CR_ENGINE, CR_MANIFOLD, CR_CARB + 2k, CR_TYRES, CR_EXHAUST, CR_MUFFLER, CR_DIFF) */
static inline u16 car_part(u16 car, u16 slot_off)   { return DSW((u16)(car + slot_off)); }
static inline s16 part_type(u16 p)                  { return DSS((u16)(p + PT_TYPE)); }
static inline s16 part_category(u16 p)              { return parts_category(part_type(p)); }
static inline s16 part_grade(u16 p)                 { return parts_grade(part_type(p)); }
/* tyre grade 0..2 of a car (catalogue grade of its tyres), and DS_TYRE_PICS + 8 * grade (the 4 wheel pictures) */
static inline s16 car_tyre_grade(u16 car)           { return part_grade(car_part(car, CR_TYRES)); }
static inline u16 tyre_pics_ofs(s16 grade)          { return (u16)(DS_TYRE_PICS + 8 * grade); }
