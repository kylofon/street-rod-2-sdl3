#pragma once
/* DGROUP offsets used by the UI toolkit slice (game/ui*.c) that symbols.h / platform/ds_extra.h do not
 * name. All SR2 addresses (port/spec/garage.md "UI toolkit" / "Globals", game_flow.md, disassembly). */

/* ---- screen stack and the current screen (garage.md; B872/B874.. aliases DS_ui_stack / DS_menu_stack) */
#define DS_ui_hot_x0        0xB87E   /* i16: cached hot-spot rectangle x0 (hotspot_find); x1 = DS:B880 */
#define DS_ui_hot_x1        0xB880
#define DS_ui_hot_y0        0xB882
#define DS_ui_hot_y1        0xB884
#define DS_ui_list_fill     0xB886   /* u8: first element of the screen has flag 0x20 (list area filled, no frame) */
#define DS_ui_texts         0xB88C   /* near ptr: text pointer array of the list box in use */
#define DS_ui_edit_mode     0xB892   /* u8: an edit field (0x200) was met while drawing */
#define DS_ui_hit           0xB896   /* near ptr: element found by hotspot_find, 0 = none */

/* ---- bounding box of the last draw_screen / ui_bounds (min x, min y, max x, max y) */
#define DS_ui_min_x         0x8824
#define DS_ui_min_y         0x8826
#define DS_ui_max_x         0x8828
#define DS_ui_max_y         0x882A

/* ---- line editor (edit_begin .. edit_end) */
#define DS_edit_caret_on    0x882C   /* u8 */
#define DS_edit_saved_col   0x882E   /* u8: DS:AD2B (line colour) before the session */
#define DS_edit_len         0x8830   /* i16 */
#define DS_edit_max         0x8832   /* i16 */
#define DS_edit_ch          0x8834   /* char + NUL (DS:8835): one-character string for the width of a deleted glyph */
#define DS_edit_buf         0x8836   /* near ptr to the text (PORT: C pointer kept in ui_edit.c, see edit_begin) */
#define DS_edit_x           0x8838
#define DS_edit_y           0x883A
#define DS_edit_cx          0x883C   /* caret x */
#define DS_edit_cy          0x883E   /* caret y */
#define DS_edit_saved_wait  0x8840   /* DS:1F3E before the session */
#define DS_edit_numeric     0x2182   /* u8: numeric-only field */

/* ---- strings and buffers */
#define DS_wear_tmpl        0x2168   /* "(xx% worn)" template: +1 = itoa target, +3 = ' ' */
#define DS_wear_digits      0x2169
#define DS_wear_space       0x216B
#define DS_wear_new         0x2174   /* "(new)" */
#define DS_wear_gap         0x217C   /* "   " between a list text and its wear text */
#define DS_glyph_up         0x20E2   /* "[" , "\", "^", "]": scroll arrows of the UI font (buttons -7 .. -10) */
#define DS_glyph_pgup       0x20E4
#define DS_glyph_pgdn       0x20E6
#define DS_glyph_down       0x20E8
#define DS_empty_str        0x1FFA   /* "" (status line cleared by ui_wait) */
#define DS_dialog_text      0xAB04   /* text of screen 27 (dialog_msg) */
#define DS_fatal_text       0xAA12   /* text of screen 33 (fatal_message) */
#define DS_fatal_head       0x1E2C   /* text-mode line printed before the message */
#define DS_fatal_hit_ret    0x231A   /* "Hit Return when ready ..." */
#define DS_fatal_pal        0x1E2E   /* 16 palette values for the box when the screen was cleared */
#define DS_fatal_cleared    0x7904   /* u16: nonzero when the screen has been cleared by fatal_message */
#define DS_fatal_shift      0xAD30   /* u8: shift the box's y coordinates (-0x4B) */
#define DS_music_off        0xA2B2   /* u16: no music at all (blocks the 'M' toggle) */

/* ---- demo mode (demo_step) */
#define DS_demo_prev_menu   0x2166   /* i16: menu of the previous pick */
#define DS_demo_script_m22  0x214E   /* i16[]: {6,5,3,1,0x3EB x4,1,3,0} */
#define DS_demo_pos_m22     0x2164
#define DS_demo_script_m12  0x2134   /* i16[]: {1,2,1,2,-2,2,1,2,1,-1} */
#define DS_demo_pos_m12     0x2148
#define DS_demo_cnt_m7      0x214A
#define DS_demo_cnt_m34     0x214C
#define DS_demo_gift_a      0x470A   /* i16: value of the free spare tyres (part type 0x34) */
#define DS_demo_gift_b      0x471E   /* ... exhaust manifold (0x36) */
#define DS_demo_gift_c      0x4778   /* ... differential (0x3F) */
