#pragma once
/* The SR2 UI toolkit (segment 1a21 / 0001 of SR.EXE): screens of 18-byte elements, the input / wait loop,
 * message boxes, list boxes, text entry and the demo player. Used by game_flow, garage and race.
 * Specs: port/spec/garage.md "UI toolkit" + the functions 1a21:2018 - 60d2, port/spec/game_flow.md
 * (ui_wait 0001:1d02, msg_box, demo_step, ...). SR2 has NO animation-script interpreter (SR1 ui_anim.c): the
 * sounds are sound_vec calls (game/snd.h).
 *
 * Files: ui_menu.c (screens, hot spots, ui_menu / ui_dialog), ui_msg.c (message boxes, fatal_message, msg_box),
 * ui_list.c (list boxes), ui_edit.c (line editor), ui_wait.c (input_reset, ui_wait, wait loops, demo player).
 * DS offsets that symbols.h does not name: game/ds_ui.h.
 *
 * Conventions (sr2port/PORTING.md): all state stays in mem[] at the original DGROUP addresses. Near pointers
 * into DGROUP (element pointers, text pointers, the list text array of DS:A61C) are u16 DS offsets; pointers
 * to buffers the original keeps on its stack (edit buffers, the wear bytes of a list) are C pointers.
 *
 * ---- Screens (UIEL, garage.md "UI toolkit")
 * DS:3AD8 = 36 near pointers; a SCREEN NUMBER is the index 0..35 into it (0 garage, 1 quit, 2 licence,
 * 3 newspaper, 4 classified page, 6 "click anywhere", 7 spare-part list, 9 your cars, 27 OK dialog,
 * 33 fatal message ...). A screen is an array of 18-byte elements (UIEL) ending with the one whose flags
 * have UIF_LAST. The elements are sorted by x0 (hotspot_find relies on it; ui_sort_screen re-sorts).
 *   +0 u16 flags   +2 u16 text (near ptr, 0 = none)   +4 s16 colour scheme (DS:9DC6 + 5*scheme: fg, bg,
 *   fill, line, frame; scheme <= 0 keeps the current colours)   +6 s16 code (what ui_wait returns when the
 *   element is clicked / its hot key is pressed: negative = buttons, positive = rows / hot spots)
 *   +8 x0  +A y0  +C x1  +E y1   +10 u8 hot key 1, +11 u8 hot key 2 (scancodes, 0xFF = none).
 * ui_push(s) draws the screen and makes its UIF_ACTIVE elements the hot spots; ui_pop(1) removes it and
 * re-activates the one below. State: DS:B874[] stack, DS:B87C depth (no limit check), DS:B894 current
 * element array (0 = none), DS:B898 code under the pointer, DS:B896 its element, DS:B87E..B884 the cached
 * rectangle in which that answer stays valid (hotspot_find). Found while drawing: DS:B888 first list row
 * element, DS:B88A row count, DS:B890 first title / edit field element, DS:B886 = first element's 0x20.
 *
 * ---- List boxes (garage.md 4e0c - 565c)
 * Rows are elements with UIF_ROW (their codes are 1..rows, or -1..-rows for slot_list_edit); the optional
 * title field (UIF_TITLE / UIF_EDIT) mirrors the selected text. Indices are 0-BASED. `texts` is the array of
 * near text pointers (the callers' DS:A61C = DS_list_entries, as `(const u16 *)mp(DGROUP, DS_list_entries)`,
 * or any C array of DS offsets); `wear` the optional wear bytes (0x80 = no wear text) as a C array. The
 * selected index is left in DS:B88E (list_selected()) and is both the input and the output of the call.
 * Buttons -7 / -8 / -9 / -10 scroll one row / one page up / one page down / one row down. */
#include "types.h"
#include "mem.h"
#include "symbols.h"
#include "game/ds_ui.h"

/* ---- UIEL */
#define UIEL_FLAGS    0x00
#define UIEL_TEXT     0x02
#define UIEL_SCHEME   0x04
#define UIEL_CODE     0x06
#define UIEL_X0       0x08
#define UIEL_Y0       0x0A
#define UIEL_X1       0x0C
#define UIEL_Y1       0x0E
#define UIEL_KEYS     0x10
#define UIEL_BYTES    0x12
#define UIF_ACTIVE    0x0001      /* hot spot */
#define UIF_LAST      0x0002      /* last element of the screen */
#define UIF_PANEL     0x0008      /* panel with a 3-pixel frame (and a caption); 0x04 is ignored */
#define UIF_BUTTON    0x0010
#define UIF_FILL      0x0020      /* on the first element: list area filled before the rows, no frame */
#define UIF_FRAME     0x0040
#define UIF_TEXT      0x0080      /* static text */
#define UIF_CTEXT     0x0100      /* centred text */
#define UIF_EDIT      0x0200      /* edit field */
#define UIF_TITLE     0x0400      /* title / selection bar */
#define UIF_ROW       0x0800      /* list row (not drawn) */

static inline u16 ui_screen(s16 n)            { return DSW((u16)(DS_SCREENS + 2 * n)); }   /* first element of screen n */
#define UIEL_W(e, f)  DSW((u16)((e) + (f)))
#define UIEL_S(e, f)  DSS((u16)((e) + (f)))

/* ================================================================ screens (ui_menu.c) */
void ui_push(s16 screen);                     /* 1a21:4b96 */
void ui_pop(s16 all);                         /* 1a21:4cac: all != 0 re-pushes the screen below */
void ui_bounds(s16 screen, s16 *x, s16 *y, s16 *w, s16 *h);   /* 1a21:4284: rectangle of a screen, 3 px margin */
FarPtr ui_save_bg(s16 screen, s16 *x, s16 *y, s16 kind);      /* 1a21:434a: kind 1 low / 2 high arena; *x unaligned */
s16  hotspot_find(s16 x, s16 y);              /* 1a21:43dc: code of the active element under (x, y), 0 = none */
s16  draw_element(u16 el);                    /* 1a21:44a8: 1 if it drew something */
void draw_screen(u16 els);                    /* 1a21:473e: all elements to the last, accumulating DS:8824.. */
void ui_redraw_element(u16 el);               /* 1a21:4798 */
void ui_sort_screen(s16 screen);              /* 1a21:4d7c: sort by (x0, y0), keep the LAST flag at the end */
s16  ui_menu(s16 screen);                     /* 1a21:4cf4: ui_push, ui_wait(16000) until non-zero, ui_pop(1) */
#define ui_run ui_menu                        /* garage.md's name */
s16  ui_dialog(s16 screen);                   /* 1a21:4d26: save background, ui_menu, restore */
void hotspots_reset(void);                    /* 0001:0e74: arena_clear, DS:1ECC/1ECE/1ED0 = 0, ui_pop(1) */
/* PORT: SR1's ui_state_reset (0f38:4bff) does not exist in SR2: gfx_init clears DS:B87C itself. */

/* ================================================================ message boxes (ui_msg.c) */
/* Message id n: DS:11EA + 6n = {box type, line 1, line 2} (near pointers); box record DS:1E46 + 0x12*type:
 * {pic, lines, x, y, w, h, text dx, text dy, extra}. */
void dialog_msg(const char *s);               /* 1a21:2018: text into DS:AB04, ui_dialog(27) (OK button) */
void fatal_message(const char *s);            /* 1a21:204c: modules.fatal_message; register with ui_register_hooks() */
void msg_draw(s16 id);                        /* 1a21:2530: id < 0: removed at once */
void msg_draw_at(s16 id, s16 x, s16 y);       /* 1a21:274e: at (x, y) (< 0 keeps the table's), DS:1F4C = 0 */
void msg_close(void);                         /* 1a21:27d2 (SR1 msg_erase) */
#define msg_erase msg_close
void present_back_page(void);                 /* 1a21:2888: back page -> front at once (garage backdrop) */
void msg_box(s16 id);                         /* 0001:3db4: box, ui_push(6), wait DS:1F4A ticks, close */
void message_at(s16 id, s16 x, s16 y);        /* 0001:4d90: msg_draw_at, wait DS:1F4A ticks (deadline saved), msg_close */
s16  pay_or_msg(u16 price, s16 msg);          /* 1a21:1e50: affordable -> money_add(-price), 1; else msg_box, 0 */
void ui_register_hooks(void);                 /* modules.fatal_message = fatal_message */

/* ================================================================ list boxes (ui_list.c) */
u16  wear_text(s16 pct);                      /* 1a21:4e0c: DS offset of "(new)" or "(NN% worn)" (built at DS:2168) */
/* 1a21:4e56: text (DS offset) at (x, y); show_wear: wear_text(pct) right-aligned to `right`. */
void list_row(s16 x, s16 y, u16 text, s16 right, s16 show_wear, s16 pct);
/* 1a21:4ebc: returns 0x3EA for an empty list, else the code (button) that ended it; row clicks only select. */
s16  list_box_run(s16 screen, s16 sel, const u16 *texts, const s8 *wear, s16 n);
s16  list_box(s16 screen, s16 sel, const u16 *texts, const s8 *wear, s16 n);   /* 1a21:55ea: saves the background */
s16  slot_list_edit(s16 screen, s16 sel, const u16 *texts, s16 n);   /* 1a21:565c: save-game picker (codes -1..-n) */
static inline s16 list_selected(void)        { return DSS(DS_ui_sel); }         /* DS:B88E */

/* ================================================================ text entry (ui_edit.c) */
void edit_begin(s16 x, s16 y, char *buf, s16 maxlen);   /* 1a21:5c52 */
void edit_key(void);                          /* 1a21:5d5e: the key in DS:1F40 */
void edit_blink(void);                        /* 1a21:5f16 */
void edit_end(void);                          /* 1a21:5f90 */
s16  edit_run(s16 x, s16 y, char *buf, s16 maxlen);     /* 1a21:5fb8: 0x3EB / button code / -22 / -24 */
s16  edit_number(u16 *v, char *buf);          /* 1a21:6056: field DS:B890; 0 Enter, -22 never mind, -24 offer */
s16  ui_edit_number(s16 screen, u16 *v);      /* 1a21:60d2 */
s16  ui_edit(s16 screen, s16 maxlen, char *buf);   /* 1a21:610c: text field of a screen; <= 0 */

/* ================================================================ input, waits, demo (ui_wait.c) */
/* input_reset 0001:17ea and wait_click_or_key 1a21:0dca are declared in platform/platform.h. */
s16  ui_wait(s16 ticks_max);                  /* 0001:1d02: 0 timeout, 1000 DS:1F3A passed, 0x3EB any key / click, else a code */
void wait_ticks(s16 n);                       /* 0001:3d80: ui_wait(30000) until n ticks pass or a key / click */
void kbd_inject_scancode(u8 sc);              /* 0001:3245: dead copy of the INT 9 body (no callers); sc = scan code */
void demo_step(void);                         /* 1a21:4806: pick the next demo target (ui_wait / ui_push call it) */
