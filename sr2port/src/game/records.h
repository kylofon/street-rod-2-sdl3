#pragma once
/* Shared SR2 record layouts (port/spec/garage.md "Records" / "Tables"). All records live in mem[]; "pointers"
 * between records are near pointers of the original = u16 DS offsets (0 = null). Used by game_flow, garage, race. */
#include "types.h"
#include "mem.h"
#include "symbols.h"

/* ---- car record, 0x2A bytes, pool DS_car_pool (16 records), free list DS_car_freelist */
#define CR_VALUE      0x00    /* s16 */
#define CR_MODEL      0x02    /* s16 index into DS_MODELS (0..21), -1 = free */
#define CR_COLOUR     0x04    /* s8 paint 0..5 */
#define CR_CLASS      0x05    /* u8 copy of MODEL class */
#define CR_TRANS      0x06    /* part ptr */
#define CR_ENGINE     0x08    /* part ptr */
#define CR_MANIFOLD   0x0A    /* part ptr */
#define CR_CARB       0x0C    /* part ptr [3]: +0C +0E +10 */
#define CR_TYRES      0x12    /* part ptr */
#define CR_TRANS_BOLT 0x14    /* s8[2]: 0 out, 1 loose .. 3 tight */
#define CR_BOLT       0x16    /* s8[6]: +16 engine, +17/+18 manifold, +19/+1A/+1B carburettor 0/1/2 */
#define CR_EXHAUST    0x1C    /* exhaust manifold part ptr */
#define CR_MUFFLER    0x1E    /* part ptr */
#define CR_DIFF       0x20    /* differential part ptr */
#define CR_ENGINE_LINK 0x22   /* s8: 0 no engine, -1 unplugged, 1 connected */
#define CR_IGNITION   0x23    /* s8 -5..+3 */
#define CR_GAS        0x24    /* s16 tenths of a gallon */
#define CR_FLAGS      0x26    /* u16: high byte 0x20 roof chopped, 0x40 rear bumper stripped, 0x80 front bumper stripped */
#define CR_NEXT       0x28    /* car ptr */
#define CAR_BYTES     0x2A
#define CAR_POOL_N    16

/* ---- part node, 8 bytes, pool DS_part_pool (190 nodes; the garage is full at 181 = 0xB5), free list DS_part_freelist */
#define PT_VALUE      0x00    /* s16 */
#define PT_WEAR       0x02    /* s16 0..10000, -128 = does not wear */
#define PT_TYPE       0x04    /* s16 index into DS_PARTS (0..63), -1 = free */
#define PT_NEXT       0x06    /* part ptr */
#define PART_BYTES    0x08
#define PART_POOL_N   190
#define PART_LIMIT    0xB5

/* ---- part catalogue DS_PARTS (0x4502), 64 x 10 bytes */
#define PARTS_REC     10
static inline s16 parts_price(s16 t)    { return DSS((u16)(DS_PARTS + PARTS_REC * t)); }
static inline s8  parts_grade(s16 t)    { return DSC((u16)(DS_PARTS + PARTS_REC * t + 2)); }
static inline u8  parts_make(s16 t)     { return DSB((u16)(DS_PARTS + PARTS_REC * t + 3)); }
static inline s8  parts_category(s16 t) { return DSC((u16)(DS_PARTS + PARTS_REC * t + 4)); }
static inline u16 parts_text(s16 t)     { return DSW((u16)(DS_PARTS + PARTS_REC * t + 6)); }   /* near ptr to NUL-terminated name */
static inline s16 parts_lines(s16 t)    { return DSS((u16)(DS_PARTS + PARTS_REC * t + 8)); }
/* categories: 0 engine, 1 transmission, 2 carburettor, 3 manifold, 4 tires, 5 exhaust manifold, 6 muffler, 7 differential */

/* ---- car models DS_MODELS (0xA80C), 22 x 20 bytes (0..20 cars, 21 = the King's car) */
#define MODEL_REC     20
#define MODEL_COUNT   22
static inline s16 model_price(s16 m)    { return DSS((u16)(DS_MODELS + MODEL_REC * m)); }
static inline s16 model_trade_in(s16 m) { return DSS((u16)(DS_MODELS + MODEL_REC * m + 2)); }
static inline u8  model_class(s16 m)    { return DSB((u16)(DS_MODELS + MODEL_REC * m + 4)); }
static inline u8  model_next(s16 m)     { return DSB((u16)(DS_MODELS + MODEL_REC * m + 5)); }
static inline u8  model_b6(s16 m)       { return DSB((u16)(DS_MODELS + MODEL_REC * m + 6)); }  /* runtime rnd(5) preview colour */
static inline u16 model_specA(s16 m)    { return DSW((u16)(DS_MODELS + MODEL_REC * m + 8)); }  /* low nibble: make mask 1 GM / 2 Ford / 4 Chrysler */
static inline u16 model_specB(s16 m)    { return DSW((u16)(DS_MODELS + MODEL_REC * m + 10)); }
static inline u16 model_ad_ptr(s16 m)   { return DSW((u16)(DS_MODELS + MODEL_REC * m + 12)); }
static inline s16 model_ad_lines(s16 m) { return DSS((u16)(DS_MODELS + MODEL_REC * m + 14)); }
static inline s16 model_pic(s16 m)      { return DSS((u16)(DS_MODELS + MODEL_REC * m + 16)); } /* LIB2 id */

/* ---- people DS_people (0xAB7E), 22 x 0x12 bytes (the opponents; #21 the King) */
#define PERSON_REC    0x12

/* ---- model overlays DS_MODEL_OVERLAYS (0xBAAC) 10 bytes: u8 race, u8 b1; u16 roof, scoop, rear bumper, front bumper (LIB2 ids, 0 = none) */

/* ---- game clock: units per game hour (546 rt ticks), per 12-hour day */
#define FLOW_GAME_HOUR   0x222u
#define FLOW_GAME_DAY    0x1998u
#define FLOW_GAME_OVER   0x29A    /* garage_screen / game_loop: the game is over (DS_game_over != 0) */
/* DS_game_over (0x1BC6) flags: 1 summer over, 2 the King beaten (race code), 4 debts?, 0x10 broke */
/* DS_location (0x1BC8): 1 garage, 2 street/Burger's, 3 gas station, 4 Phyllis event, 5 newspaper, 6 Bob's drive-in */
