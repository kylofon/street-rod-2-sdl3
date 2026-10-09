#pragma once
/* DGROUP offsets and private glue declarations of the garage "shop" slice (S5.2 W2a): car / part model,
 * newspaper, classifieds, sell parts, your cars. Included by garage_data.c, garage_shop.c and
 * garage_classifieds.c only. Offsets are SR2 (spec garage.md "Globals"). */
#include "types.h"
#include "mem.h"
#include "symbols.h"

/* ---- classified ads (the save block DS:9B40, 0x80 bytes) */
#define DS_AD_WEEK        0x9B40   /* u16 week of the stored ad list; 0xFFFF after new_game */
/* DS_ad_ids 9B42 u8[26], DS_ad_cool 9B5C u8[42], DS_ad_pool 9B86, DS_ad_part_row 9B88, DS_ad_price 9B8A u16[27],
 * DS_ad_lines 9F2C i16[] (symbols.h); the row text pointers A61C = DS_list_entries (u16[], 0-based) */
#define DS_AD_ROW1_LINES  0x9F2E   /* DS_ad_lines + 2: lines of row 1 (the "USED CARS" header, 3) */

/* ---- classified page state (symbols.h: DS_page_first_row 1EC8, DS_page_last_row 1ECA, DS_page_list 1ECC,
 * DS_page_paper 1ECE far, DS_page_redraw 1ED2) */
#define DS_PAGE_RECT      0x1ED4   /* Rect {w 0x40, h 0xBE, sx, sy, dx, dy}: paper strip source / destination */
#define DS_AD_HDR_CARS    0x1EE8   /* u16 near pointer to the 3-line "---------- USED CARS ----------" text */
#define DS_AD_HDR_PARTS   0x1EEE   /* u16 near pointer to the 3-line "------------ PARTS ------------" text */
#define DS_FMT_AD_PRICE   0x1EF4   /* "%d" (price patch of a first text line) */
#define DS_FMT_AD_PRICE2  0x1EF7   /* "%d" (price patch of a '+' continuation) */
#define DS_FMT_BUY        0x1EFA   /* "%-3d ?" ("You want to buy it for $" ...) */
#define DS_FMT_SHORT      0x1F01   /* "%d short." */

/* ---- sell parts / your cars */
#define DS_FMT_TAKE       0x4C72   /* "Will you take $%-d ?" */
#define DS_FMT_HOWABOUT   0x4C87   /* "%-d ?" */
#define DS_TXT_SELL_IT    0x226C   /* "Sell it" */
#define DS_TXT_CHANGE     0x2274   /* "Change" */
#define DS_SELL_NAME      0x2238   /* text buffer of element 3 of screen 10 (the car's name) */
#define DS_SELL_EDIT      0x2266   /* edit buffer of screen 10 ("I'll take $" field) */

/* msg_table (DS_msg_table 11EA): entry n = 6 bytes {box, line1, line2}; the line pointers are patched text
 * ("$12345", "$1234") by sprintf. */
static inline u16 shop_msg_line(s16 id) { return DSW((u16)(DS_msg_table + 6 * id + 2)); }
