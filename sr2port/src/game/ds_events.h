#pragma once
/* DGROUP offsets used by the event code (gas station 2925:1ab8-2104, Grudge Night 2925:000e-1970, game_loop
 * 0001:4dd6) that symbols.h / platform/ds_extra.h do not name. Addresses from port/spec/game_flow.md and the
 * disassembly (work/SR2_unp.exe). Everything that symbols.h already has is used under its symbols.h name. */
#include "mem.h"
#include "symbols.h"

/* ---- strings / pointers */
#define DS_money_label       0x4844   /* u16 near ptr: "`Bankroll:`$" (status_label argument) */
#define DS_gas_status_str    0x5128   /* "" (gas station status line) */

/* ---- gas station (event15) */
#define DS_gas_menu          0x3B04   /* u16 near ptr: element array (0x12 bytes each, +0 flags: 1 on, 2 last) of the pushed menu */
#define DS_gas_pal_flag      0x1DB2   /* u8: set to 4 by event15_run */
#define DS_pump_sound        0x1F7C   /* u16: 4 while a pump runs (sound code reads it) */
#define DS_PUMP_X0           0x50F0   /* i16[4]: x of the 4 pump strips (index 1, 2 = the two pumps; DS_PUMP_X = &[1]) */
#define DS_led_x             0x5110   /* i16[6] LED digit x (pump 1; pump 2 adds 0x9B) */
#define DS_led_y             0x511C   /* i16[6] LED digit y */
#define DS_led_on            0xAD44   /* u16 colour of a lit LED */
#define DS_led_off           0xAD46   /* u16 colour of an unlit LED */

/* ---- Grudge Night (event17) */
#define DS_tourn_self        0x9100   /* the unsorted player entry {-1, 0, B8C2} (T[9]) */
#define DS_tourn_pic_border  0x9108   /* far Desc*: border picture (LIB2 #207 / LIB3 #38) */
#define DS_tourn_pic_photo   0x910C   /* far Desc*: saved photo area */
#define DS_tourn_pic_face    0x9110   /* far Desc*: saved face area */
#define DS_tourn_pic_panel   0x9114   /* far Desc*: saved framed panel */
#define DS_tourn_clip        0x4FBC   /* rect {0x18, 0x1E, 0x11F, 0x4F} for the panel wipe */
#define DS_tourn_ref_const   0xA9C3   /* u8: race-code constant that scales the opponents' dial-ins */

/* Grudge Night table T[i] = {s16 id (-1 = player), s16 state, s16 time}, 10 entries at DS_tourn_table. */
#define TOURN_REC            6
#define TOURN_N              10
#define TOURN_ID(i)          DSS((u16)(DS_tourn_table + TOURN_REC * (i)))
#define TOURN_STATE(i)       DSS((u16)(DS_tourn_table + TOURN_REC * (i) + 2))
#define TOURN_TIME(i)        DSS((u16)(DS_tourn_table + TOURN_REC * (i) + 4))
