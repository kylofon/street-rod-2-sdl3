#pragma once
/* DGROUP offsets the platform / video code uses that symbols.h (generated from port/symbols.csv) does not
 * name, or names differently from the SR1 code the port started from. All SR2 addresses; sources are the
 * "Globals" tables of port/spec/platform.md and video.md and the disassembly. Add to symbols.csv and drop
 * from here when the symbol file learns them. */

/* ---- aliases for SR1 names */
#define DS_bios_ticks          DS_rt_ticks          /* 1F6E: 18.2 Hz BIOS tick counter (u32) */
#define DS_g_draw_off          DS_draw_off          /* 9BC0 */
#define DS_g_draw_seg          DS_draw_seg          /* 9DC2 */
#define DS_g_scheme            DS_SCHEME            /* 9DC6: pointer to the colour scheme table in use */
#define DS_g_saved_front       0x1C4C               /* pages remembered by gfx_screen_mode(0): front (back = DS_g_saved_back) */

/* ---- platform: start-up, timer, keyboard, mouse */
#define DS_gfx_mode_set       0x0048               /* "video mode was set" */
#define DS_timer_installed    0x0044
#define DS_screen_flag        0x004C               /* gfx_screen_mode(0) */
#define DS_timer_on           0x1F4E               /* ISRs installed */
#define DS_auto_drive         0x1F82               /* self-driving flag (no switch in SR2) */
#define DS_chain_count        0x1F76               /* u8 sub-tick divider of the BIOS chain */
#define DS_race_step_count    0x1F77               /* u8 12-tick physics countdown */
#define DS_isr_installing     0x1F7A
#define DS_isr_cursor_busy    0x2000
#define DS_isr_race_busy      0x2008
#define DS_anim_player        0x1BCA
#define DS_cursor_moved       0x881C               /* u8: pointer moved / redraw */
#define DS_old_int8           0xA2AC               /* far ptr: the BIOS INT 8 handler */
#define DS_quit_demo          0x1F84               /* "Quit demo" */
#define DS_joy_y              0x3B22
#define DS_joy_centre_x       0x3B26
#define DS_joy_centre_y       0x3B28
#define DS_joy_thr_x_lo       0x3B2A
#define DS_joy_thr_x_hi       0x3B2C
#define DS_joy_thr_y_lo       0x3B2E
#define DS_joy_thr_y_hi       0x3B30
#define DS_step_x             0x3B54               /* i16[3] {-1, 0, 1} at 3B52 indexed -1..1 from the middle */
#define DS_step_y             0x3B5A
#define DS_hot_x1             0xB880               /* cached hot-spot rectangle: x1 (-1 = invalid) */
#define DS_hot_code           0xB898               /* hovered code (7D00h = none) */
#define DS_menu_depth         0xB87C               /* UI menu depth (cleared by gfx_init) */
#define DS_menu_aux           0x8820
#define DS_menu_stack         0xB872
#define DS_menu_elems         0xB894               /* current element array (UIEL, 0x12 bytes each) */
#define DS_bios_mode_saved    0x7A2A               /* video detection results (INT 10h / equipment) */
#define DS_bios_ega_info      0x7A2B
#define DS_bios_equip_saved   0x7A2C
#define DS_bios_ega_mem       0x7A2D
#define DS_bios_ega_switch    0x7A2E

/* ---- text menus and switches (strings in DGROUP) */
#define DS_VIDEO_MENU_LINES   0x01A0               /* "" + the pointer table 01A2..01AC */
#define DS_VIDEO_MENU_L2      0x0147
#define DS_VIDEO_MENU_L3      0x016F
#define DS_SND_MENU_TITLE     0x01C3
#define DS_SND_MENU_BLANK1    0x01E1
#define DS_SND_MENU_BLANK2    0x01E2
#define DS_SND_MENU_BLANK3    0x01E3
#define DS_SND_MENU_REC       0x01E4
#define DS_SND_MENU_LINES     0x01F8               /* pointers {01AE "1 - AdLib", 01B8 "2 - Normal"} */
#define DS_SW_ONE             0x0212
#define DS_SW_NOMOUSE         0x0214
#define DS_SW_DEMO            0x021C
#define DS_SW_NOUEMEM         0x0221
#define DS_SW_E               0x0229
#define DS_SW_C               0x022B
#define DS_SW_H               0x022D
#define DS_SW_V               0x022F
#define DS_SW_T               0x0231
#define DS_BIG_PICS           0x491A               /* {3, 1, 4, 0} */
#define DS_RESIDENT_PICS      0x4922
#define DS_libs_preloaded     0x986C
#define DS_music_enabled      0x5304

/* ---- libraries, memory */
#define DS_MSG_NOT_FOUND      0x4B6E               /* pointer to "File not found: ????????" */
#define DS_VRAM_POOL_BASE     0x490E               /* far ptr A000:0000 of the VRAM picture cache */
#define DS_song_name0         0x7304               /* "?:msr2..." song file names (drive letter patched) */
#define DS_song_name1         0x7310
#define DS_song_name2         0x731C
#define DS_song_name3         0x7328

/* ---- video */
#define DS_g_aux_a            0xAD18
#define DS_g_text_bg          0xAD29
#define DS_g_line_col         0xAD2B
#define DS_g_text_y           0xAD34
#define DS_g_gc3              0x7B9A               /* word whose high byte is the GC3 function select (EGA span / line) */
#define DS_scheme_cga         0x1D56
#define DS_composite_keep_mask 0x79E2
#define DS_pic_last           0x9858               /* far Desc*: picture of the last pic_blit_at */
#define DS_pic_x              0x1E40               /* show_picture_at origin, rows (clip / clear height) */
#define DS_pic_y              0x1E42
#define DS_pic_rows           0x1E44
#define DS_cursor_hot_tab     0x200C               /* i16[3][2]: pointer hot spots per shape */
#define DS_tiles3             0x50E4               /* 3 tile pairs {x, w} of event 15 */
#define DS_view_desc          0xAD58               /* far Desc*; AD5C / AD5E / AD60 = view x, y, h */
#define DS_view_x             0xAD5C
#define DS_view_y             0xAD5E
#define DS_view_h             0xAD60
#define DS_status_x0          0xAD36               /* x where the message area begins (58h) */
#define DS_status_y           0xAD38               /* text row of the status line */
#define DS_status_width       0xAD3A               /* default centring width (A0h) */
#define DS_status_used        0xAD3C               /* first char of the last message (0 = area blank) */
#define DS_status_money_x     0xAD3E               /* x of the bankroll after the label */

/* pointer (cursor) state block A2BA..A2E0 (SR1 7AEC..7B12) */
#define DS_ptr_save_rect      0xA2BE               /* Rect {w, h, sx, sy, dx, dy} of the save-under (pixels) */
#define DS_ptr_w              0xA2BE
#define DS_ptr_h              0xA2C0
#define DS_ptr_x              0xA2C6
#define DS_ptr_y              0xA2C8
#define DS_ptr_bank           0xA2CA               /* far: planes of the pre-shifted pointer bank */
#define DS_ptr_bank_mask      0xA2CE               /* far: its mask */
#define DS_ptr_shape_off      0xA2D2               /* u16: shape * DS:A2D4 */
#define DS_ptr_shape_size     0xA2D4               /* u16: bytes of one shape (stride * 16) */
#define DS_ptr_shape          0xA2D6               /* i16 */
#define DS_ptr_mode           0xA2D8               /* i8: 0 off, 1 draw + mirror, 2 draw, -1 / -2 erase */
#define DS_ptr_last_x0        0xA2DA               /* i16 last drawn rect x0, y0, x1, y1 (x1 < 0: none) */
#define DS_ptr_last_y0        0xA2DC
#define DS_ptr_last_x1        0xA2DE
#define DS_ptr_last_y1        0xA2E0

/* ---- sprite animation (anim_rect.c) */
#define DS_anim_engine_ref    0xB866               /* x before the step (read by the engine-sound code) */

/* ---- misc */
#define DS_race_front         0xAD0C               /* far Desc*: race-mode front page */
#define DS_race_back          0xADA2               /* far Desc*: race-mode back page (VGA) */
#define DS_rnd_seed           0x4A84               /* u32 LCG seed */
#define DS_wait_until         0x1F3A               /* u32 deadline of the UI wait loop (0 = none) */
#define DS_dancer_sprite      0xAAFC               /* far Desc*: the dancer sprite (picture 0x47D) */
