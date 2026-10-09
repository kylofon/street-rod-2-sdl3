#pragma once
/* DGROUP offsets used by the garage drawing code (garage_draw.c, garage_bay_draw.c, garage_anim.c, garage_tire.c)
 * that symbols.h / platform/ds_extra.h do not name. All SR2 addresses, taken from the disassembly and the Ghidra
 * decompile (port/decomp/sr2_ds.c); the SR1 address each one replaces is in the comment. */
#include "types.h"
#include "mem.h"
#include "symbols.h"
#include "platform/ds_extra.h"

#define FAR_NULL ((FarPtr){ 0, 0 })

/* ---- car drawing (car_draw 1a21:89fa, car_strip_setup, carhop_anim, walker_anim) */
#define DS_strip_x          0x7912   /* i16 strip x (Tandy / two-page drivers only; SR1 5A20) */
#define DS_strip_w          0x7914   /* i16 strip width (SR1 5A22) */
#define DS_strip_ov         0x790A   /* s16[4] overlay rect of the smoke strip (SR1 5A10 / 5A18); y1 at 7910 */
#define DS_strip_ov_y1      0x7910
#define DS_car_tails        0xAA3C   /* Tail[4] (8 bytes each): the car frames cached in VRAM (SR1 7EC2) */
#define DS_car_dirty        0x95DE   /* s16[4] saved dirty rect of the car animation (SR1 6CBC) */
#define DS_hop_dirty        0x95E6   /* s16[4] saved dirty rect of the car-hop (SR1 6CC4) */
#define DS_town_bg          0x95EE   /* far Desc*: town background snapshot behind the car (SR1 6CCC) */
#define DS_car_running      0xA808   /* i16: the car runs during the animation (engine sound variant) */
#define DS_has_muffler      0xB8D4   /* i16: 1 = the car has a muffler (flags & 0x20 == 0) */
#define DS_anim_view        DS_anim_w /* 1BCC far Desc*: sprite of the last animation step (symbols.h names its first word) */
#define DS_pump_save_a      DS_gas_pump_save_a   /* 9A4A far[2], stride 4 */
#define DS_pump_save_b      DS_gas_pump_save_b   /* 9ACC far[2] */
#define DS_PUMP_FRAME_W     0x50FA   /* i16[2] pump widths {8, 16} (= DS_PUMP_W + 2) */
#define DS_CARHOP_SCRIPT0   0x79C0   /* 4 x {count, dx, dy} step script, mode 0 (walks in) */
#define DS_CARHOP_SCRIPT1   0x79D0   /* the same, mode 1 (walks away) */
#define DS_CARHOP_IDX0      0x7924   /* u8[77] frame indices (1-based), mode 0 */
#define DS_CARHOP_IDX1      0x7972   /* u8[77] frame indices (1-based), mode 1 */
#define DS_anim_script      0x9B16   /* default frame script of the Anim (sprite_scene_setup: Anim + 0x10) */

/* ---- Anim record DS:A544 (platform/anim_rect.c) */
#define DS_scene_bgsrc      0xA546   /* FarPtr (Anim +2) */
#define DS_scene_bg         0xA54A   /* FarPtr (Anim +6) */
#define DS_scene_ovl        0xA54E   /* u16 (+0xA) */
#define DS_scene_x          0xA550   /* i16 (+0xC) */
#define DS_scene_y          0xA552   /* i16 (+0xE) */
#define DS_scene_script     0xA554   /* u16 (+0x10) */
#define DS_scene_node       0xA556   /* u16 (+0x12) */
#define DS_scene_rect       0xA600   /* page node 0's last rect (also the clip rect made by rect_clip_make) */

/* ---- tyre change (tire_change_anim 1a21:b9bc; SR1 equivalents in brackets) */
#define DS_tc_ybase         0x79E4   /* i16 y base (0 except Tandy) [5AA8] */
#define DS_tc_bmp_a         0x79EC   /* far Desc* [5AB0]: VRAM page 2 strip bitmap (rear/front strip background) */
#define DS_tc_bmp_b         0x79F0   /* far Desc* [5AB4]: VRAM page 3 */
#define DS_tc_bmp_c         0x79F4   /* far Desc* [5AB8]: VRAM page 3 (jack background) */
#define DS_tc_jack          0x95FC   /* far[3] jack pictures [6CDA] */
#define DS_tc_wfront        0x95FA   /* front wheel x in the car picture [6CD8] */
#define DS_tc_save_car      0x95F6   /* far Desc* [6CD4]: points at DS_tc_desc */
#define DS_tc_desc          0x9616   /* inline 18-byte Desc [6CF4] */
#define DS_tc_hy      0x9608   /* i16 hand y [6CE6] */
#define DS_tc_rect          0x960A   /* s16[4] overlay rect [6CE8] */
#define DS_tc_tmp           0x9612   /* far scratch picture [6CF0] */
#define DS_tc_save          0x9628   /* far [6D24]: screen strip 0x88.. */
#define DS_tc_jackbg        0x962C   /* far [6D28]: background under the jack */
#define DS_tc_strip         0x9630   /* far [6D2C]: car strip at the wheel being changed */
#define DS_tc_hub           0x9634   /* far [6D30]: hub picture 0x47C */
#define DS_tc_shsave        0x9638   /* far [6D34]: background of the shadow */
#define DS_tc_hand          0x963C   /* far [6D38]: hand picture 0x47F */
#define DS_tc_anim          0x9640   /* u16 [6D3C]: the Anim of the current step */
#define DS_tc_shift         0x9642   /* i16 [6D3E] */
#define DS_tc_x             0x9668   /* i16 x of the car [6DA0] */
#define DS_tc_x2            0x966A   /* i16 [6DA2] */
#define DS_tc_wrear         0x967E   /* rear wheel x in the car picture [6DD4] */
#define DS_tc_bottom        0x9680   /* far [6DD6]: the bottom rows of the car without wheels */
#define DS_tc_shq           0x9684   /* far [6DDA] */
#define DS_tc_shadow        0x9688   /* far [6DDE]: ground shadow picture 0x472 */
#define DS_tc_frames        0x968C   /* far[5] [6DE2]: rolling wheel frames; the 5th (969C) is the hold-off copy [6DF2] */
#define DS_tc_frame_copy    0x969C
#define DS_tc_car           0x96A0   /* far [6DF6]: the car without wheels (frames array of 1) */
#define DS_tc_carbg         0x96A4   /* far [6DFA]: the screen behind the car */
#define DS_tc_sfx           0x1F7C   /* i16: set to 8 / 0x10 around the rolling / jack sounds (loop_idle_putt of SR1) */

/* ---- "Under the Car" */
#define DS_uc_backdrop      0x9718   /* far Desc*: picture 0x4AF, kept for erasing a part */
