/* Race: drive entry points and the frame loop (port/spec/race.md, checked against the decompile / disassembly).
 *
 *   0001:8c68 drive_to_event   0001:8d0a opp_palette_set   0001:8d9e drive_run   0001:8ef4 race_logic
 *   1a21:77f4 race_run         1a21:9b0e race_view_reset (= arena_reset)
 *
 * Timing: road_frame ends with pace_ticks(8) (deadline metronome on DS:1D02); the player physics runs in the race ISR
 * (platform race_isr -> modules.race_phys_step) on every 12th tick (DS:1F76 / DS:1F77 dividers). */
#include "game/race_int.h"
#include "game/race.h"

#include "game/events.h"
#include "game/flow.h"
#include "game/garage_screens.h"
#include "game/ui.h"
#include "host.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "platform/platform.h"
#include "platform/video.h"

#define MODE DSS(DS_road_mode)

/* 0001:8c68 drive_to_event: the cruise scene to / from an event (location DS:1BC8) */
void drive_to_event(void)
{
    s16 save = MODE;
    if (DSS(DS_location) != 0) {
        MODE = 0;
        u16 car = cur_car();
        if (CARS(car, CR_GAS) < 3) CARW(car, CR_GAS) = 0;
        else CARW(car, CR_GAS) -= 2;                            /* 0.2 gal */
        drv_pal_black();
        cursor_ctl(-4);
        DSS(DS_dash_type) = OVL_DASH(CARS(cur_car(), CR_MODEL));
        DSW(DS_has_clock) = grade_of(CARW(cur_car(), CR_TRANS)) == 0 ? 1 : 0;
        DSB(DS_racing) = 0;
        phys_reset(PLAYER);
        road_state_init();
        DSW(DS_race_state) = 0;
        drive_run();
    }
    MODE = save;
}

/* 0001:8d0a opp_palette_set: EGA palette registers 0Ch / 03h = the opponent's paint pair DS:4D1E (3-byte stride);
 * -1 restores the saved pair */
void opp_palette_set(s16 idx)
{
    if (DSS(DS_driver_id) != -2 && DSS(DS_driver_id) != -6) return;
    u8 a, b;
    if (idx == -1) {
        a = DSB(DS_pal_saved_a);
        b = DSB(DS_pal_saved_b);
        DSB(DS_pal_saved_b) = 0xFF;
        DSB(DS_pal_saved_a) = 0xFF;
    } else {
        if (DSB(DS_pal_saved_a) == 0xFF) { DSB(DS_pal_saved_a) = DSB(DS_pal_live_0c); DSB(DS_pal_saved_b) = DSB(DS_pal_live_03); }
        a = DSB((u16)(DS_paint_pairs + idx * 3));
        b = DSB((u16)(DS_paint_pairs + idx * 3 + 1));
    }
    if (DSB(DS_g_pal_visible) != 0) {
        ega_set_palreg(0xC, (s8)a);
        ega_set_palreg(3, (s8)b);
    }
    DSB(DS_pal_live_0c) = a;
    DSB(DS_pal_live_03) = b;
}

/* 0001:8d9e drive_run */
s16 drive_run(void)
{
    if (DSB(DS_racing)) drv_pal_black();
    s16 oc = DSS(DS_opp_paint);
    DSW(DS_start_lights) = 0;
    DSW(DS_light_latch) = 0;
    pal_load(DS_pal_game);
    set_paint_palette(CARC(cur_car(), CR_COLOUR));
    if (DSB(DS_racing)) opp_palette_set(oc);
    DSW(DS_start_panel_id) = (MODE & 6) ? 0x815 : MODE == 8 ? 0x813 : 0x816;
    static const u16 zero[] = { DS_aq_def_floor_h, DS_aq_saved_floor_h, DS_aq_def_xl, DS_aq_saved_xl, DS_aq_def_xr,
                                DS_aq_saved_xr, DS_aq_def_top_h, DS_aq_saved_top_h, DS_aq_def_top_xl, DS_aq_wall_l_shift,
                                DS_aq_def_top_xr, DS_aq_wall_r_shift, DS_view_bounce_y, DS_cliff_fall };
    for (unsigned i = 0; i < sizeof zero / sizeof zero[0]; i++) DSW(zero[i]) = 0;
    wait_ticks(0xF);
    DSB(DS_click_held) = 0;
    DSB(DS_joy_buttons) = 0;
    DSW(DS_mouse_left) = 0;
    DSW(DS_click_seen) = 0;
    wait_ticks(0x19);
    input_poll_any();                                           /* 0001:8c2c drive_abort_poll */
    DSW(DS_aq_bounce_flag) = 0;
    DSW(DS_fall_dir) = 0;
    DSW(DS_fallen) = 0;
    s16 r = 0;                                                  /* PORT: uninitialised in the skip path (spec §6) */
    if ((DSW(DS_trip_skip) == 0 && DSW(DS_click_seen) == 0) || DSB(DS_racing) || DSS(DS_location) == 4) {
        if (DSW(DS_lib3_dir_read) == 0) { lib_read_dir(3); DSW(DS_lib3_dir_read) = 1; }
        r = race_run();
    } else {
        arena_reset();                                          /* race_view_reset */
        DSW(DS_arena2_used) = 0;
    }
    DSB(DS_racing) = 0;
    sound_vec(SND_ENGINE, 0);
    sound_vec(SND_SIREN, 0);
    if (DSW(DS_race_saved_cursor_x) || DSW(DS_race_saved_cursor_y)) {
        DSW(DS_cursor_x) = DSW(DS_race_saved_cursor_x);
        DSW(DS_cursor_y) = DSW(DS_race_saved_cursor_y);
        DSW(DS_race_saved_cursor_x) = 0;
        DSW(DS_race_saved_cursor_y) = 0;
    }
    return r;
}

static u16 results(void) { return (u16)(PHW(OPP, PH_RESULT) | PHW(PLAYER, PH_RESULT)); }
static bool after(u32 deadline) { return !tick_after(deadline, ticks_now()); }

static void start_seq(u16 text)
{
    DSW(DS_ctl_1F44) = 0;
    race_isr_enter();
    input_reset();
    DSW(DS_race_state) = 1;
    status_print(-1, ds_str(text), 0);
}

static u16 jump_start(void)
{
    race_isr_leave();
    DSB((u16)(PLAYER + PH_RESULT)) |= 0x81;
    return 1;
}

/* 0001:8ef4 race_logic: per-frame state machine (cruise auto-drive, start lights, bracket starts) */
u16 race_logic(void)
{
    const u16 p = PLAYER;
    s16 st = DSS(DS_race_state);
    if (MODE == 2) {                                            /* Grudge Night time run */
        if (st == 0) {
            start_seq(0x4FD0);
            DSL(DS_wait_until) = ticks_now() + 0x6E;
            return results();
        }
        if (st < 1) return (u16)(st - 1);
        if (st <= 2) {
            if (PHS(p, PH_SPEED) > 0) return jump_start();
            if (after(DSL(DS_wait_until))) {
                if (st == 1) {
                    DSW(DS_start_lights) = 0x10;
                    DSL(DS_wait_until) = ticks_now() + 0x50;
                    DSW(DS_race_state) = 2;
                } else {
                    DSW(DS_start_lights) = 0x20;
                    DSW(DS_race_state) = 3;
                    status_print(-1, ds_str(0x4FDC), 0);
                }
            }
            PHW(OPP, PH_STEPS) = 0;
            PHW(p, PH_STEPS) = 0;
            DSW(DS_opp_green) = 1;
            return results();
        }
        if (st == 3) {
            race_step();
            if (PHW(p, PH_RESULT) != 0) return 1;
            if (PHW(OPP, PH_RESULT) != 0) return 1;
            if (PHS(p, PH_SPEED) > 0) { status_print(-1, ds_str(0x4FE1), 0); DSW(DS_race_state) = 4; }
            return 0;
        }
        if (st != 4) return (u16)(st - 4);
        if (PHW(p, PH_RESULT) != 0 || PHW(OPP, PH_RESULT) != 0) return 1;
        race_step();
        return 0;
    }
    if (MODE == 4) {                                            /* Grudge Night round: bracket start */
        if (st == 0) {
            start_seq(0x4FE2);
            DSW(DS_bracket_p_state) = 0;
            DSW(DS_bracket_o_state) = 0;
            u32 base = ticks_now() + 0x6E;
            DSL(DS_bracket_o_time) = base;
            DSL(DS_bracket_p_time) = base;
            s16 d = sub16(DSS(DS_tourn_opp_dialin), DSS(DS_tourn_dialin));
            DSS(DS_cruise_gain) = d;
            if (d > 0) DSL(DS_bracket_p_time) = base + (u32)((s32)d * 7);      /* 7 ticks per tenth */
            else       DSL(DS_bracket_o_time) = base - (u32)((s32)d * 7);
            return results();
        }
        if (st == 1) { if (PHS(p, PH_SPEED) > 0) return jump_start(); }
        else if (st != 2) {
            if (st == 3) {
                if (PHW(p, PH_RESULT) != 0) return 1;
                if (PHW(OPP, PH_RESULT) != 0) {
                    if (sub16(DSS(DS_seg), DSS(DS_opp_seg)) < 8) { DSW(DS_race_state) = 4; return 0; }
                    return 1;
                }
                race_step();
                return 0;
            }
            if (st != 4) return (u16)(st - 4);
            race_step();
            return sub16(DSS(DS_opp_seg), DSS(DS_seg)) < 7 ? 0 : 1;
        }
        race_step();
        if (DSS(DS_bracket_p_state) < 2 && after(DSL(DS_bracket_p_time))) {
            if (DSS(DS_bracket_p_state) == 0) {
                DSL(DS_bracket_p_time) = ticks_now() + 0x50;
                DSW(DS_bracket_p_state) = 1;
                DSW(DS_start_lights) |= 0x10;
            } else {
                DSW(DS_bracket_p_state) = 2;
                DSW(DS_race_state) = 2;
                status_print(-1, ds_str(0x4FEE), 0);
                DSW(DS_start_lights) |= 0x20;
                PHW(OPP, PH_STEPS) = 0;
                PHW(p, PH_STEPS) = 0;
            }
        }
        if (DSS(DS_bracket_o_state) < 2 && after(DSL(DS_bracket_o_time))) {
            if (DSS(DS_bracket_o_state) == 0) {
                DSL(DS_bracket_o_time) = ticks_now() + 0x50;
                DSW(DS_bracket_o_state) = 1;
                DSW(DS_start_lights) |= 2;
            } else {
                DSW(DS_bracket_o_state) = 2;
                DSW(DS_opp_green) = 1;
                DSW(DS_ai_steps_done) = PHW(p, PH_STEPS);
                DSW(DS_start_lights) |= 4;
            }
        }
        if (DSS(DS_bracket_p_state) == 2 && PHS(p, PH_SPEED) > 0) {
            status_print(-1, ds_str(0x4FF3), 0);
            DSW(DS_bracket_p_state) = 3;
        }
        if (DSS(DS_bracket_p_state) == 3 && DSS(DS_bracket_o_state) == 2) DSW(DS_race_state) = 3;
        return results();
    }
    if (!DSB(DS_racing)) {                                      /* ---- cruising */
        if (st == 0) {
            start_seq(0x5006);
            if (PHW(p, PH_AUTO_MODE) == 1) PHW(p, PH_AUTO_MODE) = 2;
            DSW(DS_cruise_gain) = DSW(DS_cruise_accel);
            PHW(p, PH_GEAR) = 1;
            PHW(p, PH_GEAR_DIRTY) = 1;
            return 0;
        }
        if (st != 1) return (u16)(st - 1);
        input_poll_any();
        s16 seg = DSS(DS_seg);
        if ((TRKB(TRK_L, seg + 1) & 1) || (TRKB(TRK_R, seg + 1) & 1) || DSW(DS_click_seen) != 0) {
            DSB(0x3B3E) = 0;
            DSB(DS_click_held) = 0;
            race_isr_leave();
            return 1;                                           /* arrived (or aborted) */
        }
        PHW(p, PH_RPM) += DSW(DS_cruise_gain);
        if (PHS(p, PH_RPM) > 0x125C) PHW(p, PH_RPM) = 0x125C;
        if (PHW(p, PH_GEAR) != 0)
            PHS(p, PH_SPEED) = cdiv16(PHS(p, PH_RPM), DSS((u16)(DS_cruise_ratio + PHS(p, PH_GEAR) * 2)));
        if (PHS(p, PH_RPM) <= 0x1194) return 0;
        if (PHS(p, PH_NGEARS) <= PHS(p, PH_GEAR)) { DSW(DS_cruise_gain) = 0; return 0; }
        PHW(p, PH_GEAR)++;
        u16 g2 = (u16)(PHS(p, PH_GEAR) * 2);
        DSW(DS_cruise_gain) = DSW((u16)(DS_cruise_accel + g2));
        PHW(p, PH_RPM) = (u16)(DSS((u16)(DS_cruise_ratio + g2)) * PHS(p, PH_SPEED));
        PHW(p, PH_GEAR_DIRTY) = 1;
        return 0;
    }
    /* ---- racing: street drag, Aqueduct, Mulholland */
    if (st == 0) {
        start_seq(0x4FF4);
        s32 r = rnd(0x67);
        DSL(DS_wait_until) = (u32)((s32)ticks_now() + r + 0x6E);
        return results();
    }
    if (st == 1) {
        if (PHS(p, PH_SPEED) > 0) return jump_start();
        if (after(DSL(DS_wait_until))) {
            DSW(DS_race_state) = 2;
            status_print(-1, ds_str(0x5000), 0);
            DSW(DS_start_lights) = 1;
        }
        return results();
    }
    if (st == 2) {
        race_step();
        if (PHW(p, PH_RESULT) != 0) return 1;
        if (PHW(OPP, PH_RESULT) != 0) return 1;
        if (PHS(p, PH_SPEED) > 0) { status_print(-1, ds_str(0x5005), 0); DSW(DS_race_state) = 3; }
        return 0;
    }
    if (st != 3) return (u16)(st - 3);
    if (PHW(p, PH_RESULT) != 0 || PHW(OPP, PH_RESULT) != 0) return 1;
    race_step();
    return 0;
}

/* 1a21:77f4 race_run: cockpit set-up, split screen, the frame loop, restore (VGA path) */
s16 race_run(void)
{
    const u16 p = PLAYER;
    s16 shown_rpm = 0, shown_speed = 0, res;
    s16 dash_w = 0;
    arena_clear();
    do {
        drv_pal_black();
        gfx_screen_mode(3);
        arena_reset();
        DSW(DS_cga_dbuf) = 0;                                   /* PORT: CGA double buffer (Hercules/CGA) not ported */
        DSW(DS_dash_redraw) = DSS(DS_driver_id) == -2 ? 1 : 0;
        DSS(DS_dash_rows_y) = DSS(DS_driver_id) == -2 ? 0 : 100;
        FarPtr pic = pic_get(0x7DD, 2);                         /* top frame strip */
        Rect r = { (s16)desc_w(pic), (s16)desc_h(pic), 0, 0, 0, 0 };
        drv_blit(pic, ds_far(DS_page1_ptr), &r, 1);
        if (DSS(DS_driver_id) == -2) drv_blit(pic, ds_far(DS_page2_ptr), &r, 1);
        arena_pop(1);
        FarPtr dash = pic_get(DSS((u16)(0x78C8 + 2 * DSS(DS_dash_type))), 2);
        if (DSW(DS_has_clock)) {
            u16 e = (u16)(0x78F2 + 6 * DSS(DS_dash_type));
            FarPtr face = pic_get(DSS(e), 2);
            Rect rf = { (s16)desc_w(face), (s16)desc_h(face), 0, 0, DSS((u16)(e + 2)), DSS((u16)(e + 4)) };
            drv_blit(face, dash, &rf, 1);
            arena_pop(1);
        }
        dash_w = (s16)desc_w(dash);
        r = (Rect){ dash_w, (s16)desc_h(dash), 0, 0, 0, (s16)(DSS(DS_driver_id) == -2 ? 0 : 100) };
        drv_blit(dash, ds_far(DS_page0_ptr), &r, 1);
        arena_pop(1);
        arena_reset();
        DSW(DS_arena2_used) = 0;
        shifter_draw(PHS(p, PH_AUTO_MODE), PHS(p, PH_NGEARS));
        shifter_knob(ds_far(DS_page0_ptr), p);
        DSS(DS_wheel_shown) = 100;
        wheel_load();
        wheel_draw(0, DSS(DS_wheel_shown), ds_far(DS_page0_ptr));
        DSS(DS_wheel_shown) = 0;
        if (MODE == 8) race_load_graphics_rot();
        else road_load_graphics();
        shp_load_set();
        shown_rpm = PHS(p, PH_RPM);
        shown_speed = PHS(p, PH_SPEED);
        s16 rpm0 = shown_rpm, sp0 = shown_speed;
        speedo_init(PHS(p, PH_SPEED));
        shown_speed = needle_update(PHS(p, PH_SPEED), sp0, 0);
        if (DSW(DS_has_clock) == 0) {
            tach_init(rpm0);
            shown_rpm = needle_update(PHS(p, PH_RPM), rpm0, 1);
        } else
            clock_draw();
    } while (DSS(DS_dash_redraw) > 1);
    DSW(DS_dash_redraw) = 0;
    if (DSS(DS_driver_id) == -2) {
        ega_set_split(0x63);
        DSW(DS_split_row) = 100;
    } else
        DSW(DS_split_row) = 0;
    if (MODE & 6) DSW(DS_start_lights) = MODE == 2 ? 0x88 : 0x92;
    road_pages_init();
    road_frame();
    drv_pal_normal();
    if (MODE == 4) {
        if (DSS(DS_driver_id) == -2) {
            Rect r = { 0x140, 0x5F, 0, 0, 0, 0 };
            drv_blit(ds_far(DS_page1_ptr), ds_far(DS_page2_ptr), &r, 1);
        }
        cursor_ctl(-2);
        msg_box(0x70);                                          /* "Drivers stage your vehicles!" */
        cursor_ctl(-4);
        DSW(DS_start_lights) = 9;
    }
    if (DSW(DS_race_saved_cursor_x) == 0 && DSW(DS_race_saved_cursor_y) == 0) {
        DSW(DS_race_saved_cursor_x) = DSW(DS_cursor_x);
        DSW(DS_race_saved_cursor_y) = DSW(DS_cursor_y);
    }
    DSB(DS_kbd_dir_y) = 0;
    DSB(DS_kbd_dir_x) = 0;
    DSW(DS_cursor_y) = 0;
    DSW(DS_cursor_x) = 0;
    if (getenv("SR_TRACE")) fprintf(stderr, "trace: %ldms race_run mode %d racing %d\n", (long)clock(), MODE, DSB(DS_racing));   /* PORT: developer aid */
    for (;;) {
        road_step();
        if (DSW(DS_demo) != 0 && (u8)key_read() == 'Q') { race_isr_leave(); platform_exit(); }
        res = (s16)race_logic();
        s16 w = PHS(p, PH_WHEEL);
        if (res != 0) break;
        if (PHW(p, PH_GEAR_DIRTY) != 0) shifter_knob(ds_far(DS_page0_ptr), p);
        if (abs16((s16)(PHS(p, PH_SPEED) - shown_speed)) > 1) shown_speed = needle_update(PHS(p, PH_SPEED), shown_speed, 0);
        if (DSW(DS_has_clock) == 0 && abs16((s16)(PHS(p, PH_RPM) - shown_rpm)) > 1)
            shown_rpm = needle_update(PHS(p, PH_RPM), shown_rpm, 1);
        if (DSS(DS_wheel_shown) != w) {
            wheel_draw(w, DSS(DS_wheel_shown), ds_far(DS_page0_ptr));
            DSS(DS_wheel_shown) = w;
        }
    }
    if (getenv("SR_TRACE")) fprintf(stderr, "trace: %ldms race_run end %d seg %d opp %d\n", (long)clock(), res, DSS(DS_seg), DSS(DS_opp_seg));   /* PORT: developer aid */
    arena_reset();
    DSW(DS_arena2_used) = 0;
    FarPtr shown = DSS(DS_back_page) == 1 ? ds_far(DS_page2_ptr) : ds_far(DS_page1_ptr);
    FarPtr back = DSS(DS_back_page) == 1 ? ds_far(DS_page1_ptr) : ds_far(DS_page2_ptr);
    if (DSS(DS_driver_id) == -2) {
        Rect r = { dash_w, 100, 0, 0, 0, (s16)(DSW(DS_g_vga) == 0 ? 101 : 100) };
        drv_blit(ds_far(DS_page0_ptr), shown, &r, 1);
        r = (Rect){ dash_w, 1, 0, 0, 0, 100 };
        drv_blit(ds_far(DS_page0_ptr), shown, &r, 1);
        ega_set_split(-1);
        DSW(DS_split_row) = 0;
        drv_copy_page(desc_planes(shown), desc_planes(ds_far(DS_page0_ptr)));
        drv_copy_page(desc_planes(shown), desc_planes(back));
    }
    gfx_screen_mode(1);
    return res;
}
