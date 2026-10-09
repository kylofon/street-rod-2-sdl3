#pragma once
/* DGROUP offsets used by the game_flow slice (game/flow_*.c) that symbols.h / platform/ds_extra.h do not name.
 * All are SR2 addresses taken from port/spec/game_flow.md and checked against the disassembly. */

/* ---- save games (?:HOTROD<n>.SAV): the three ":HOTROD\0" ".SAV\0" templates of save_slots_scan / load_game_screen /
 * save_game_screen (the ".SAV" part is 8 bytes after the template), and their message strings */
#define DS_hotrod_scan        0x4CAE
#define DS_hotrod_load        0x4CBB
#define DS_hotrod_save        0x4CF8
#define DS_msg_name_ptr       0x4CAA      /* near ptr to "The game must have a name !" */
#define DS_msg_no_saves       0x4CC8      /* "No saved games found !" */
#define DS_msg_cannot_read    0x4CE0      /* "Cannot read that game !" */
#define DS_msg_disk_error     0x4D06      /* "Disk error during Save" */

/* ---- hall of fame and data files */
#define DS_hall_path          0x4BCA      /* "?:hall_dat" */
#define DS_hall_rows_y        0x48EA      /* u16[10]: text row y of the hall of fame lines (0x3B + 11 i) */
#define DS_hall_last_y        0x48FC      /* u16: y of the tenth row (= DS_hall_rows_y[9]); the copy rectangle ends 8 below it */
#define DS_hall_hr            0x48FE      /* "hr" */
#define DS_hall_scheme_a      0x4902      /* 5 bytes: EGA colour scheme patch (scheme + 0x28) */
#define DS_hall_scheme_b      0x4908      /* 5 bytes: EGA colour scheme patch (scheme + 0x23) */
#define DS_data_path          0x8850      /* "X:SR2_data" built by hot_data_load; also the probe of data_disk_check */
#define DS_data_name_ptr      0x14D8      /* near ptr to "SR2_data" (advanced by hot_data_load, then == DS_end_name_ptr) */
#define DS_end_name_ptr       0x14DA      /* near ptr to "SR2_end" */
#define DS_data_dst           0x14DC      /* u16[9] destination offsets of the SR2_data blocks (8th set at run time) */
#define DS_data_size          0x14F0      /* u16[9] sizes */
#define DS_data_dst7          0x14EA      /* DS_data_dst + 2*7 */
#define DS_text_pool          0x8860      /* SR2_data text pool */
#define DS_text_pool_end      0xBC4A      /* u16: offset of the last (person 21) name in the pool */
#define DS_ad_text_tbl        0xBBF4      /* u16[22] ad text offsets */
#define DS_person_name_tbl    0xBC20      /* u16[22] person name offsets (BBF4 + 44) */
#define DS_road_bitmaps       0xAAA4      /* 6 x 14 bytes: three near ptrs each at +0, +2, +6 */
#define DS_msg_cant_open      0x4B94      /* "Can't open data file" */
#define DS_msg_data_read      0x4BA9      /* "Data disk read fail" */
#define DS_shp_missing        0x4B64      /* u16: 1 = SR2_shp not on the floppy */
#define DS_msg_end_open       0x81F8      /* "Can't open end data file" */
#define DS_msg_end_read       0x8211      /* "End Data disk read fail" */
#define DS_end_frames         0x9714      /* far ptr: SR2_end frame table (84 x 6) */
#define DS_end_script         0x9710      /* far ptr: SR2_end script (183 x 4) */

/* ---- copy protection */
#define DS_prot_fmt           0x4848      /* "What`is`the`%d  `letter`in`line`%d`on`page`%d?" */
#define DS_prot_text          0x2334      /* sprintf target; the ordinal suffix chars are at 0x2341 / 0x2342 */
#define DS_prot_edit          0x2362      /* edit buffer */
#define DS_prot_palette       0x48D8      /* EGA palette of the question screen */
#define DS_empty_string       0x4877      /* "" */

/* ---- calendar */
#define DS_cal_empty          0x4D5A      /* "" */
#define DS_date_fmt           0x4D76      /* "%s %d, 1969" */
#define DS_month_names        0x4D82      /* near ptr[4] */

/* ---- scratch buffers that the original keeps in its stack frame (DOS reads and writes go through mem[]).
 * PORT: they live in the DGROUP stack area of the original (DS:BC50..C650: SS = DS, SP starts at DS:C650), which the
 * port's C code never uses. */
#define FLOW_STK_HDR          0xC000      /* 0x38 save header (save_slots_scan) */
#define FLOW_STK_CAR          0xC040      /* 0x2A car record (load_car_chain) */
#define FLOW_STK_PART         0xC070      /* 0x08 part record (load_part_chain) */
#define FLOW_STK_ELAPSED      0xC080      /* 0x04 rt_ticks - rt_base (load / save) */
#define FLOW_STK_NAME         0xC090      /* 0x10 hall of fame name edit (hall_of_fame_screen) */
#define FLOW_STK_FNAME        0xC0B0      /* 0x14 "X:HOTROD<n>.SAV" */

/* ---- UI elements (0x12 bytes, see game/ui.h): +4 style (scheme entry index), +8 x0, +0A y0, +0C x1, +0E y1 */
#define FLOW_ELEM_SIZE        0x12
