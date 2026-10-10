#pragma once
/* Private header of the race subsystem (game/race_*.c): driving, physics, opponents, Bob's Drive-In, results
 * (port/spec/race.md) and the road renderer (port/spec/race_render.md). Other modules use game/race.h.
 *
 * All state stays in mem[] at its SR2 DGROUP address (DS_ names from symbols.h where they exist, the rest below);
 * records are near pointers = u16 DS offsets. Track arrays are far word arrays in file segment 3726. */
#include "types.h"
#include "mem.h"
#include "symbols.h"
#include "game/records.h"
#include "platform/ds_extra.h"
#include "platform/video.h"

/* ============================================================================ phys block (0xC0 bytes) */
/* player DS:A100, opponent DS:A1E4 (race.md §1; layout = SR1 §3.1, +BE = steps) */
#define PH_SPEED        0x00
#define PH_STEER_IN     0x02
#define PH_WHEEL        0x04
#define PH_BLOWOUT_SEG  0x0A
#define PH_TIRE_SLIP    0x0C
#define PH_RPM          0x0E
#define PH_F10          0x10
#define PH_F12          0x12    /* opponent: band_hi */
#define PH_RPM_UP       0x14    /* opponent: band_up */
#define PH_RPM_DOWN     0x16
#define PH_GEAR         0x18
#define PH_GEAR_DIRTY   0x1A
#define PH_NGEARS       0x1C
#define PH_REDLINE      0x1E
#define PH_AUTOMATIC    0x20
#define PH_AUTO_SHIFT   0x22
#define PH_AUTO_MODE    0x24
#define PH_F26          0x26
#define PH_F28          0x28
#define PH_F2A          0x2A
#define PH_RATIO        0x34    /* s16[5] */
#define PH_GRIP         0x3E
#define PH_CUR_RATIO    0x40
#define PH_BAND         0x42
#define PH_CURVE        0x44    /* [5 gears][3 bands] {rpm_lo, rpm_hi, up, down}, gear stride 0x18 */
#define PH_RESULT       0xBC
#define PH_STEPS        0xBE
#define PH_BAND_HI      PH_F12
#define PH_BAND_UP      PH_RPM_UP
#define CRV(gear, band) ((u16)(PH_CURVE + (gear) * 0x18 + (band) * 8))
#define CRV_LO   0
#define CRV_HI   2
#define CRV_UP   4
#define CRV_DOWN 6

#define PHS(p, off) DSS((u16)((p) + (off)))
#define PHW(p, off) DSW((u16)((p) + (off)))
#define PLAYER DS_player_phys
#define OPP    DS_opp_phys

/* car / part records: records.h CR_ / PT_ */
#define CARS(c, off) DSS((u16)((c) + (off)))
#define CARW(c, off) DSW((u16)((c) + (off)))
#define CARC(c, off) DSC((u16)((c) + (off)))
#define CARB(c, off) DSB((u16)((c) + (off)))
#define PARTS(p, off) DSS((u16)((p) + (off)))
static inline u16 cur_car(void) { return DSW(DS_cur_car); }
/* G(p) = (int8)DS:4504[10 * p->type] */
static inline s16 grade_of(u16 part) { return DSC((u16)(DS_PARTS + PARTS_REC * DSS((u16)(part + PT_TYPE)) + 2)); }

/* person record DS:AB7E + 18 i (i = 0..21, 21 = the King) */
#define PERSON(i)    ((u16)(DS_people + PERSON_REC * (i)))
#define PR_A        0x00   /* i8 skill */
#define PR_B        0x01   /* i8 steadiness */
#define PR_FACE     0x04
#define PR_NAME     0x06
#define PR_MODEL    0x08
#define PR_PAINT    0x0A   /* i8 */
#define PR_STATUS   0x0B   /* i8 -1 gone, 0 unknown, 1 met */
#define PR_RACES    0x0C   /* i8 */
#define PR_CUSTOM   0x0D   /* u8 1 front bumper, 2 rear bumper, 4 roof, 0x20 scoop */
#define PR_REROLL   0x0E
#define PR_WINR     0x0F   /* i8 win rating 1..8 */
#define PR_HOLER    0x10   /* i8 holeshot rating 1..8 */

/* car model DS:A80C + 20 m, overlay DS:BAAC + 10 m (records.h) */
#define MODEL_PTR(m)      ((u16)(DS_MODELS + MODEL_REC * (m)))
static inline u8  model_cls(s16 m)    { return DSB((u16)(DS_MODELS + MODEL_REC * m + 4)); }
#define OVL(m)            ((u16)(DS_MODEL_OVERLAYS + 10 * (m)))
#define OVL_B0(m)         DSC(OVL(m))
#define OVL_DASH(m)       DSC((u16)(OVL(m) + 1))
#define OVL_ROOF(m)       DSS((u16)(OVL(m) + 2))
#define OVL_SCOOP(m)      DSS((u16)(OVL(m) + 4))
#define OVL_REAR(m)       DSS((u16)(OVL(m) + 6))
#define OVL_FRONT(m)      DSS((u16)(OVL(m) + 8))

/* ============================================================================ DGROUP without a symbols.h name */
#define DS_ctl_1F44          0x1F44   /* i16 cleared by race_ctrl_reset / phys_reset */
#define DS_ctl_1F46          0x1F46
#define DS_ctl_1F48          0x1F48
#define DS_limiter_count     0x2006   /* i16 rev limiter counter */
#define DS_fire_prev         0x2009   /* u8 */
#define DS_light_latch       0xAD0A   /* i16 start light / starter frame latch (autopilot shift gate) */
#define DS_trip_skip         0x4B64   /* i16 set by hot_data_load: trips are not driven */
#define DS_lib3_dir_read     0x4FCE   /* i16 LIB3 directory loaded once */
#define DS_speedo_shown      0x9E92   /* i16 mph shown on the speedo */
#define DS_dash_rows_y       0xB8C4   /* i16 y offset of the dashboard rows (0 VGA, 100 other) */
#define DS_dash_redraw       0x7904   /* i16 race_run outer loop */
#define DS_split_row         0xAD30   /* i16 */
#define DS_cga_dbuf          0x78C6   /* i16 (CGA double buffer, 0 on VGA) */
#define DS_starter_seg       0x9E6A   /* i16 segment of the starter figure (a746) */
#define DS_status_money      0x4844   /* near char*: label of the money status line */
#define DS_hotspot_list      0x3B00   /* near ptr: current hot-spot list (18-byte records) */
#define DS_opp_pic_id        0xAD6E   /* opponent car picture */
#define DS_opp_paint         0xAD70
#define DS_opp_tyre_set      0xAD72
#define DS_opp_face          0xAD74
#define DS_pal_saved_a       0x4FCC   /* u8 saved palette registers 0Ch / 03h */
#define DS_pal_saved_b       0x4FCD
#define DS_pal_live_0c       0x1DB2   /* live palette table DS:1DA6 entries 0Ch / 03h */
#define DS_pal_live_03       0x1DA9
#define DS_paint_pairs       0x4D1E   /* 6 x {reg 0Ch, reg 03h, u8} */
#define DS_aq_wall_l_shift   0xAD6A   /* Aqueduct outer wall shifts (race init) */
#define DS_aq_wall_r_shift   0xB166
#define DS_aq_def_xl         0xA2B8
#define DS_aq_def_xr         0xA7A6
#define DS_aq_def_top_h      0xA5FA
#define DS_aq_def_top_xl     0x9A5A
#define DS_aq_def_top_xr     0x9B14
#define DS_aq_saved_xl       0xB8BE
#define DS_aq_saved_xr       0xBB8A
#define DS_aq_saved_top_h    0xB8E6
#define DS_cliff_fall        0xBBF2   /* i16 crash after a Mulholland fall */
#define DS_fallen            0x7F2C   /* i16 nothing is drawn after the cliff fall */
#define DS_bump_allowed      0xA7D6   /* i16 the opponent may bump the player off line */
#define DS_cruise_accel      0x501A   /* i16[6] cruise rpm gain per gear */
#define DS_cruise_ratio      0x5024   /* i16[6] cruise rpm per mph per gear */
#define DS_dash_type         DS_dash_overlay_idx   /* AB28 */
#define DS_race_kind         DS_road_mode          /* ADD6 */
#define DS_player_won        DS_tourn_alive        /* 9B3E */
#define DS_race_time10       DS_tourn_time         /* B8C2 */
#define DS_holeshot          DS_start_flag         /* AA10 */
#define DS_player_rating_w   DS_skill_a            /* AA00 */
#define DS_player_rating_h   DS_skill_b            /* AA01 */
#define DS_races_total       DS_counters           /* 479C */

/* ============================================================================ track arrays */
#define TRK_SEG   0x3726           /* file segment (SEGW adds the load segment) */
#define TRK_L     0x27B6
#define TRK_R     0x2F72
#define TRK_YAW   0x372E
#define TRK_C     0x3EEA
#define TRK_HGT   0x46A6
#define TRK_AQ    0x4E62
#define TRK_BL    0x5146
#define TRK_BR    0x5902
#define TRK_L2    0x60BE
#define TRK_R2    0x61A6
#define TRK_AQM   0x628E
#define TRK_C2    0x6572
#define TRK_LANE  0x665A
#define TRK_N     0x3DE
#define TRKW(arr, i) SEGW(TRK_SEG, (u16)((arr) + 2 * (i)))
#define TRKS(arr, i) SEGS(TRK_SEG, (u16)((arr) + 2 * (i)))
#define TRKB(arr, i) SEGB(TRK_SEG, (u16)((arr) + 2 * (i)))     /* low byte (byte tests of the original) */
static inline FarPtr trk_ptr(u16 arr, s16 i) { return far_make(SEG(TRK_SEG), (u16)(arr + 2 * i)); }

/* ============================================================================ helpers */
static inline s16 abs16(s16 v) { return (s16)(v < 0 ? -v : v); }
static inline s16 q_shr(s16 x, int n) { return x < 0 ? (s16)-(s16)((u16)-x >> n) : (s16)(x >> n); }  /* sgn(x)*(|x|>>n) */
static inline s16 add16(s16 a, s16 b) { return (s16)(a + b); }
static inline s16 sub16(s16 a, s16 b) { return (s16)(a - b); }
static inline s16 shl16(s16 v, int n) { return (s16)(u16)((u16)v << n); }
static inline s16 mul16(s16 a, s16 b) { return (s16)(a * b); }                       /* imul, low word */
static inline s16 cdiv16(s16 a, s16 b) { return b ? (s16)((s32)a / b) : 0; }          /* idiv (truncating) */
static inline s16 cmod16(s16 a, s16 b) { return b ? (s16)((s32)a % b) : 0; }

/* sound vector table DS:BB8C + 4k (S5.4) */
#include "game/snd.h"
#define SND_ENGINE  1
#define SND_PUMP    4
#define SND_BREAK   8
#define SND_SIREN  11
#define SND_CRASH  15
#define SND_SCREECH 16

/* ============================================================================ functions (race_*.c) */
/* race_gfx.c */
s16  sin256(s16 deg);                                        /* 0dcb:02b4 */
s16  line_x_at_y(s16 x1, s16 y1, s16 x2, s16 y2, s16 y);     /* 0dcb:0327 */
s16  line_y_at_x(s16 x1, s16 y1, s16 x2, s16 y2, s16 x);     /* 0dcb:0350 */
void rotate_about_centre(s16 x, s16 y, s16 *ox, s16 *oy);    /* 0dcb:0379 */
void road_fillmode_begin(void);                              /* 0dcb:0410 */
void road_fillmode_end(void);                                /* 0dcb:0425 */
void clip_line(s16 x1, s16 y1, s16 x2, s16 y2, s16 colour);  /* 0dcb:7de4 */
s16  clip_polygon(const s16 *in, s16 n, s16 *out, s16 *nout);/* 0dcb:7f30: points {x,y}, out >= 16 points */
void poly_fill_pts(const s16 *pts, s16 n, s16 fill, s16 edge);   /* [A0F6] with a C point array */
void shape_draw(s16 x, s16 y, s16 lod, FarPtr set, s16 side);/* 0dcb:4154 */
void shp_b_draw(s16 x, s16 y, s16 group, FarPtr set);        /* 0dcb:8288 */
void shp_load_set(void);                                     /* 1a21:75ea */
FarPtr drv_shrink(FarPtr pic, u8 mask);                      /* slot 15 [A0EA] */
FarPtr drv_rotate(FarPtr pic, s16 angle, s16 mode, s8 *ax, s8 *ay, s16 pivot);   /* slot 16 [A0EE] */
void drv_plot_points(FarPtr page, s16 xoff, u16 pts, s16 n, s16 colour);         /* slot 19 [A0FA] */
s16  bitmap_visible_width(FarPtr p);                         /* 1a21:a6e8 */
s16  popcount8(u8 m);                                        /* 0dcb:b7da */
void race_gfx_register(void);                                /* driver slot targets */

/* race_track.c */
void track_build_all(s16 full);                              /* 0dcb:2866 */
void track_build_course(s16 arg);                            /* 0dcb:24d8 */
void aq_build(void);                                         /* 0dcb:33ac */
s16  track_lane_x(s16 seg);                                  /* 0dcb:1470 */

/* race_road.c */
void road_set_seg_ptrs(s16 seg);                             /* 0dcb:056a */
s16  road_proj_x(s16 z, s16 x, s16 dx);                      /* 0dcb:062c */
s16  road_proj_y(s16 z, s16 y, s16 dy);                      /* 0dcb:0684 */
s16  road_proj_x_mirror(s16 z, s16 x, s16 dx);               /* 0dcb:06ba */
s16  road_proj_y_mirror(s16 z, s16 y, s16 dy);               /* 0dcb:070e */
void road_state_init(void);                                  /* 0dcb:0736 */
void road_frame(void);                                       /* 0dcb:11fc */
void road_advance_segment(void);                             /* 0dcb:12ca */
s16  road_edge_collision(void);                              /* 0dcb:148a */
void road_step(void);                                        /* 0dcb:1774 */
void road_pages_init(void);                                  /* 0dcb:3516 */
void road_page_flip(void);                                   /* 0dcb:354e */
void blit_clip_y(Rect *r);                                   /* 0dcb:35c4 */
void blit_clip_xy(Rect *r);                                  /* 0dcb:3602 */
void road_vline(s16 colour, s16 x, s16 y1, s16 y2);          /* 0dcb:3664 */
void clip_set_ymin0(void);                                   /* 0dcb:36ce */
void clip_set_mirror(void);                                  /* 0dcb:36d6 */
void clip_set_view(void);                                    /* 0dcb:36f0 */
void mirror_bg_prepare(s16 with_sky);                        /* 0dcb:370a */
void road_load_graphics(void);                               /* 0dcb:37ce */
void shape_draw_dot(s16 x, s16 y);                           /* 0dcb:512e */
void scenery_draw_shape(s16 x, s16 y, s16 depth, s16 side, s16 type, s16 noclamp);   /* 0dcb:5158 */
void road_poly_fill(const s16 *pts12, s16 n, s16 colour);    /* 0dcb:58f4 */
void road_fall_off_edge(s16 dir);                            /* 0dcb:7da2 */
FarPtr back_page(void);                                      /* DS:7E02 == 1 ? page 1 : page 2 */
void mirror_draw_backdrop(void);                             /* 0dcb:57a6 */
void mirror_finish(void);                                    /* 0dcb:5a2c */

/* race_opp.c */
void opponent_draw(void);                                    /* 0dcb:af98 */
void start_props_load(void);                                 /* 0dcb:a5aa */
void road_draw_starter_slide(void);                          /* 0dcb:a746 */
void starter_draw_mirror(s16 x, s16 y, s16 row);             /* 0dcb:a85c */
void road_draw_start_tree(s16 x, s16 ybase, s16 row);        /* 0dcb:aab0 */
FarPtr pic_alloc_shrunk(s16 w, s16 h, s16 masked, u8 ymask, u8 xmask);   /* 0dcb:b7fa */

/* race_aq.c (Aqueduct, mode 8) */
s16  aq_lane_target(s16 x, s16 dx, s16 seg, s16 ox, s16 oseg);   /* 0001:d2a8 */
void aq_project_front(void);                                 /* 0dcb:18ec */
void aq_project_mirror(void);                                /* 0dcb:1b02 */
s16  aq_wall_collision(void);                                /* 0dcb:1cd2 */
void aq_draw_front(void);                                    /* 0dcb:a094 */
void aq_draw_mirror(void);                                   /* 0dcb:9ce8 */
void aq_wall_bounce(s16 side);                               /* 0dcb:a1c0 */
s16  aq_collide_probe(s16 flags);                            /* 0dcb:a2d8 */
void race_load_graphics_rot(void);                           /* 0dcb:8840 */
void aq_mirror_stars(void);                                  /* 0dcb:85a0 */
s16  opp_bank_variant(void);                                 /* 0dcb:ae9c */
FarPtr opp_sprite_aq(s16 s, s16 v);                          /* 0dcb:ad40 */
void starter_draw_mirror_aq(s16 row);                        /* 0dcb:a92a */

/* race_phys.c */
void race_ctrl_reset(void);                                  /* 0001:2610 */
void race_phys_step(void);                                   /* race_isr 0001:2622 from 28fa */
void phys_reset(u16 p);                                      /* 0001:bc88 */
void model_car_setup(s16 model, u16 phys);                   /* 0001:c5d8 */
s16  drivetrain_rating(s16 model);                           /* 0001:c866 */
void opponent_setup(void);                                   /* 0001:ca28 */
void opp_gear(s16 v, u16 p);                                 /* 0001:cb00 */
s16  speed_to_step(s16 mph);                                 /* 0001:ce9c */
void race_step(void);                                        /* 0001:d908 */
void finish_stats(void);                                     /* 0001:b1b2 */

/* race_drive.c */
s16  drive_run(void);                                        /* 0001:8d9e */
u16  race_logic(void);                                       /* 0001:8ef4 */
s16  race_run(void);                                         /* 1a21:77f4 */

/* race_dash.c */
void speedo_init(s16 mph);                                   /* 1a21:02ac */
void tach_init(s16 rpm);                                     /* 1a21:0484 */
s16  needle_update(s16 nv, s16 old, s16 tach);               /* 1a21:055e */
void clock_draw(void);                                       /* 1a21:070e */
void wheel_load(void);                                       /* 1a21:1174 */
void wheel_knob_crash(s16 pos);                              /* 1a21:11f6 */
void wheel_draw(s16 nw, s16 old, FarPtr dst);                /* 1a21:1288 */
void shifter_draw(s16 automatic, s16 ngears);                /* 1a21:13fe */
void shifter_knob(FarPtr dst, u16 phys);                     /* 1a21:1614 */
void gear_label(s16 i);                                      /* gear letter on the status line */

/* race_results.c */
void race_msg(void);                                         /* 0001:b256 */
s16  tires_replace(void);                                    /* 0001:ac68 */
void patch_dollar_amount(u16 text, s16 amount);              /* 0001:b538 */
void text_draw_clipped(s16 x, s16 y, u16 s, s16 maxw);       /* 0001:ad36 */

/* race_bob.c */
s16  king_status(void);                                      /* 0001:975e */
s16  jail(void);                                             /* 0001:a9fc */
