#pragma once
/* Private header of the garage-screen slice (W2b): DS offsets that symbols.h / ds_extra.h do not name, small
 * helpers, and the includes the garage_main/paint/tires/trans/bay/info/undercar .c files share. */
#include "types.h"
#include "mem.h"
#include "symbols.h"
#include "platform/ds_extra.h"
#include "platform/platform.h"
#include "platform/video.h"
#include "game/records.h"
#include "game/snd.h"
#include "game/flow.h"
#include "game/ui.h"
#include "game/garage.h"
#include "game/garage_draw.h"
#include "game/garage_screens.h"

/* ---- DS offsets (all checked against work/SR2_unp.exe) */
#define DS_GS_EMPTY        0x4DAB   /* "" (clears the status line) */
#define DS_GS_DIGITS       0x4DAC   /* "1234567890" (strcspn set of the message-20 patch) */
#define DS_GS_MSG20_LINE2  0x1266   /* word: near pointer to line 2 of message 20 (msg table DS:11EA + 6*20 + 4) */
#define DS_GS_MONEY_FMT    0x4844   /* word: near pointer, the label passed to status_label */
#define DS_GS_CUST_TEXT    0x4E02   /* word[4]: "Chop roof", "Rebuild roof", "Strip bumper", "Restore bumper" */
#define DS_GS_CARINFO_TITLE 0x4E68  /* word: near pointer to "Car parts" */
#define DS_GS_CARINFO_FUEL  0x4E6A  /* word: near pointer to the "Fuel ..." line buffer */
#define DS_GS_CARINFO_SPEED 0x4E6C  /* word: near pointer to the "Max speed ..." line buffer */
#define DS_GS_FMT_MPH      0x4E3E   /* " %3d mph" */
#define DS_GS_FMT_GAL      0x4E47   /* " %2d gallons" */
#define DS_GS_REMOVE_MUFFLER 0x8238 /* "Remove current muffler" */
#define DS_GS_PAL_GARAGE   0x1DDA   /* 17-byte palette block pal_load()ed by garage_screen */
#define DS_GS_PAL_SHADOW   0x1DAC   /* bytes: registers 6 / 7 of the paint colour (set_paint_palette) */
#define DS_GS_PAL_SHADOW2  0x1DE0   /* the second copy */
#define DS_GS_ENGINE_SOUND 0x1F7C   /* engine-sound mode (ignition_tune) */
#define DS_GS_BAY_LAYOUT_V6 0x4EB6  /* BAY_LAYOUT[0][1] (BAY_LAYOUT[0][0] is DS_BAY_LAYOUT) */
#define DS_GS_BOLT_CTX_T1  0x4E8E   /* engine_bay's ctx table  (DS_BOLT_CTX + 2): bolt k+1 */
#define DS_GS_BOLT_SIDE_T2 0x4E9C   /* engine_bay's side table (DS_BOLT_SIDE + 2) */

/* ---- small helpers */
static inline u16 gs_cur_car(void) { return DSW(DS_cur_car); }
static inline void gs_clock_add(u16 v) { DSL(DS_game_clock) += v; }
static inline s16 gs_car_model(u16 car) { return DSS((u16)(car + CR_MODEL)); }
/* grade (catalogue byte +2) of the part type behind a part node */
static inline s16 gs_part_grade(u16 part) { return parts_grade(DSS((u16)(part + PT_TYPE))); }
static inline s16 gs_tyre_grade(u16 car) { return gs_part_grade(DSW((u16)(car + CR_TYRES))); }
/* DS offset of the 4 wheel picture ids of a tyre grade (TYRE_PICS = DS:4D30, 8 bytes per grade) */
static inline u16 gs_tyre_pics(s16 grade) { return (u16)(DS_TYRE_PICS + 8 * grade); }
/* make mask of a catalogue part and the car model's make bits: nonzero = fits */
static inline s16 gs_part_fits(s16 part_type, s16 model) { return (s16)(parts_make(part_type) & (u8)model_specA(model) & 0x0F); }

/* list_box over the text array DS:A61C (0-based near text pointers) filled by spares_collect */
static inline s16 gs_list_box(s16 screen, s16 sel, const s8 *wear, s16 n)
{
    return list_box(screen, sel, (const u16 *)mp(DGROUP, DS_list_entries), wear, n);
}

/* ---- functions of other modules the garage screens call (identical prototypes may be repeated) */
s16  car_max_speed(s16 model);                    /* 0001:c6f6 (race): -1 = the current car */

/* ---- sound stand-ins (S5.4): the original calls 2c57:0b2c / 0bd2 (AdLib engine tone) and writes the PIT (speaker) */
#ifndef snd_adlib_engine
#define snd_adlib_engine(on) ((void)(on))
#endif
#ifndef snd_ignition_tone
#define snd_ignition_tone(car) ((void)(car))
#endif
