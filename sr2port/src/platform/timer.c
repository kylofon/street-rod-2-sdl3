/* Platform: timer install / restore / exit and the two INT 8 handlers - platform.md 0001:189e-19e8,
 * 0001:2222-2622, platform_exit 0001:18ce (game_flow-owned, kept here with the install/restore pair).
 * SR2 has no tick-timer queue.
 *
 * The host calls the current tick handler (host_set_tick_handler, the port's INT 8 vector) once per
 * PIT tick at the rate platform_install programs (divisor 4000h = 72.8 Hz), from host_pump(). The
 * handlers run synchronously between two game-code instructions that pump the host, which is where a
 * real interrupt would have been taken as far as any of the game's state is concerned. */
#include "platform/platform.h"

#include <stdlib.h>

#include "host.h"
#include "modules.h"
#include "platform/video.h"
#include "sound/sound.h"

/* ------------------------------------------------------------------------------------------------ */
/* The BIOS INT 8 handler the game chains to every 4th tick (DS:A2AC).                              */

/* PORT: the BIOS handler is modelled as its tick count 0040:006C (u32, in mem[]); the floppy motor
 * count and INT 1Ch are of no use to the port. */
static void bios_int8(void)
{
    wr32(0x0040, 0x006C, rd32(0x0040, 0x006C) + 1);
}

/* ------------------------------------------------------------------------------------------------ */
/* The ISRs                                                                                         */

/* 0001:2246 timer_isr: per tick the u32 counter DS:1F66, the BIOS chain every 4th tick, the sound slots
 * (S5.4 hook), the demo cursor mover or the keyboard / joystick cursor motion. */
void timer_isr(void)
{
    DSL(DS_ticks)++;
    u8 old = DSB(DS_chain_count);
    DSB(DS_chain_count) = (u8)(old - 1);
    if (old != 0) {
        /* out 20h,20h (EOI); sti */
    } else {
        bios_int8();                              /* pushf; lcall [DS:A2AC] (the BIOS sends the EOI) */
        DSB(DS_chain_count) = 3;
        DSL(DS_bios_ticks)++;
    }

    modules.snd_timer_tick();                     /* sound.md 5.2: slot[23] / speaker song tick / slot[24] */

    if (DSW(DS_demo) != 0) {                      /* demo: the cursor goes to an explicit target */
        if (DSS(DS_demo_target_x) < 0) return;
        if (DSS(DS_cursor_x) != DSS(DS_demo_target_x) || DSS(DS_cursor_y) != DSS(DS_demo_target_y)) {
            DSS(DS_cursor_x) = (s16)(DSS(DS_cursor_x) + sign((s16)(DSS(DS_demo_target_x) - DSS(DS_cursor_x))));
            DSS(DS_cursor_y) = (s16)(DSS(DS_cursor_y) + sign((s16)(DSS(DS_demo_target_y) - DSS(DS_cursor_y))));
            DSB(DS_cursor_moved) = 1;
        } else {                                  /* arrived: inject the primary and secondary click */
            DSB(DS_click_code) = 1;
            DSB(DS_click2_code) = 1;
            DSS(DS_click_x) = DSS(DS_demo_target_x);
            DSS(DS_click_y) = DSS(DS_demo_target_y);
            DSS(DS_demo_target_x) = -1;
        }
        return;
    }

    if (DSC(DS_cursor_visible) > 0) {             /* pointer visible: keyboard / joystick motion */
        /* cli */
        if (DSB(DS_isr_cursor_busy) != 0) return;
        DSB(DS_isr_cursor_busy) = 1;
        /* sti */
        s16 dx = 0, dy = 0;
        if (DSW(DS_kbd_dir_lock) == 0) {          /* keyboard ISR not in the middle of an update */
            s16 kx = DSC(DS_kbd_dir_x), ky = DSC(DS_kbd_dir_y);
            if (kx) dx = kx > 0 ? 1 : -1;
            if (ky) dy = ky > 0 ? 1 : -1;
        }
        if (DSB(DS_joy_present)) {                /* the joystick overrides per axis */
            joy_read();
            s16 jx = DSS(DS_joy_x), jy = DSS(DS_joy_y);
            if (jx < DSS(DS_joy_thr_x_lo)) dx = -1;
            else if (DSS(DS_joy_thr_x_hi) < jx) dx = 1;
            if (jy < DSS(DS_joy_thr_y_lo)) dy = -1;
            else if (DSS(DS_joy_thr_y_hi) < jy) dy = 1;
        }
        if (dx | dy) {                            /* +-1 px through the tables {-1,0,1} at DS:3B52 / DS:3B58 (index 0 = the middle word) */
            DSS(DS_cursor_x) = (s16)(DSS(DS_cursor_x) + DSS((u16)(DS_step_x + dx * 2)));
            DSS(DS_cursor_y) = (s16)(DSS(DS_cursor_y) + DSS((u16)(DS_step_y + dy * 2)));
            DSB(DS_cursor_moved) = 1;
        }
        DSB(DS_isr_cursor_busy) = 0;
    }
}

/* 0001:2622 race_isr: tick counter, sound (S5.4 hook; it uses the chain counter before the decrement),
 * the BIOS chain, and on every 4th tick the physics gate (DS:1F7A == 0, racing, no animation player,
 * the 12-tick countdown DS:1F77) and one car step (race.md, S5.3 hook). */
void race_isr(void)
{
    DSL(DS_ticks)++;
    if (DSB(DS_isr_installing) == 0) modules.snd_race_tick(DSB(DS_chain_count) & 1);
    u8 old = DSB(DS_chain_count);
    DSB(DS_chain_count) = (u8)(old - 1);
    if (old != 0) return;                         /* out 20h,20h; sti */
    DSL(DS_bios_ticks)++;
    bios_int8();
    DSB(DS_chain_count) = 3;
    if (DSB(DS_isr_installing) != 0 || DSB(DS_racing) == 0 || DSW(DS_anim_player) != 0) return;
    DSB(DS_race_step_count)--;
    if ((s8)DSB(DS_race_step_count) > 0) return;
    DSB(DS_race_step_count) = 3;
    if (DSB(DS_isr_race_busy) != 0) return;
    DSB(DS_isr_race_busy) = 1;
    modules.race_phys_step();
    DSB(DS_isr_race_busy) = 0;
}

/* 0001:2222 sign */
s16 sign(s16 v)
{
    if (v == 0) return 0;
    return v > 0 ? 1 : -1;
}

/* race_isr_enter / race_isr_leave (INT 8 swapped between the two handlers, mouse y shift 6 / 1) belong to
 * the race module of SR2 (S5.3); kept here with the ISRs as in the SR1 port. */
void race_isr_enter(void)
{
    snd_int8_set(race_isr);                       /* _dos_setvect(8, 0001:2622); with AdLib music: the chained ISR */
    if (DSW(DS_demo) == 0 && DSB(DS_mouse_present)) {
        mouse_poll();
        DSW(DS_mouse_acc_y) = 0;
        DSW(DS_mouse_acc_x) = 0;
        DSW(DS_mouse_shift_x) = 1;
        DSW(DS_mouse_shift_y) = 6;
    }
    DSB(DS_click_code) = 0;
    DSB(DS_click_held) = 0;
    DSB(DS_kbd_last_dir + 1) = 0;
    DSB(DS_kbd_last_dir) = 0;
    DSB(DS_race_step_count) = 0;
    modules.snd_race_enter();
}

void race_isr_leave(void)
{
    snd_int8_set(timer_isr);                      /* _dos_setvect(8, 0001:2246) */
    modules.snd_race_leave();
    if (DSW(DS_demo) == 0 && DSB(DS_mouse_present)) {
        mouse_poll();
        DSW(DS_mouse_acc_y) = 0;
        DSW(DS_mouse_acc_x) = 0;
        DSW(DS_mouse_shift_x) = 1;
        DSW(DS_mouse_shift_y) = 1;
    }
    DSB(DS_click_code) = 0;
    DSB(DS_click_held) = 0;
    DSB(DS_kbd_last_dir + 1) = 0;
    DSB(DS_kbd_last_dir) = 0;
}

/* ------------------------------------------------------------------------------------------------ */
/* Install / restore / exit                                                                         */

/* 0001:19e8 platform_install */
void platform_install(void)
{
    s16 sx = 0, sy = 0;

    /* Joystick detection (ports 201h): trigger the one-shots, then poll until both A axes have timed
     * out with all buttons up ((v & F3h) == F0h), at most 65535 reads.
     * PORT: the gameport is the host's first gamepad (host_joy_read). Connected with no button held,
     * the reads would see the axes running (F3h) and then F0h; absent, the port floats at FFh and the
     * counter wraps. A held button keeps (v & F3h) != F0h, as on the original. */
    host_pump();                                  /* let SDL report gamepads connected at start-up */
    s16 ax = 0, ay = 0;
    u8 buttons = 0;
    bool pad = host_joy_read(&ax, &ay, &buttons);
    u8 first = pad ? 0xF3 : 0xFF;
    u16 n;                                        /* si */
    u8 v;                                         /* di */
    if (pad && (buttons & 3) == 0) { n = 2; v = 0xF0; }
    else { n = 0; v = pad ? (u8)(0xF3 & ~(buttons << 4)) : 0xFF; }
    if (DSW(DS_demo) == 0 && n != 0 && v != first) {
        DSB(DS_joy_present) = 1;                  /* calibrate: the centre starts at 0 */
        joy_read(); sx = DSS(DS_joy_x); sy = DSS(DS_joy_y);
        joy_read(); sx = (s16)(sx + DSS(DS_joy_x)); sy = (s16)(sy + DSS(DS_joy_y));
        joy_read(); sx = (s16)(sx + DSS(DS_joy_x)); sy = (s16)(sy + DSS(DS_joy_y));
        DSS(DS_joy_centre_x) = idiv32_16(sx, 3, NULL);
        DSS(DS_joy_centre_y) = idiv32_16(sy, 3, NULL);
        s16 hx = (s16)(DSS(DS_joy_centre_x) >> 1);                   /* sar */
        DSS(DS_joy_thr_x_lo) = (s16)-hx;
        DSS(DS_joy_thr_x_hi) = hx;
        s16 hy = (s16)(DSS(DS_joy_centre_y) >> 1);
        DSS(DS_joy_thr_y_lo) = (s16)-hy;
        DSS(DS_joy_thr_y_hi) = hy;
    }

    if (DSW(DS_mouse_enabled) != 0 && DSW(DS_demo) == 0 && mouse_reset() == -1) {
        DSB(DS_mouse_present) = 1;
        DSW(DS_mouse_acc_y) = 0;
        DSW(DS_mouse_acc_x) = 0;
        DSW(DS_mouse_shift_x) = 1;
        DSW(DS_mouse_shift_y) = 1;
    } else {
        DSB(DS_mouse_present) = 0;
    }

    modules.snd_state_init();                     /* sound.md 5.1: song states, DS:ADD2, DS:9DB0 */

    /* PORT: _harderr(0001:191a crit_err_handler) is not installed: the port has no INT 24h (file errors
     * come back as return codes). */

    /* cli; PIT channel 0 mode 3, divisor 4000h; the old INT 8 vector in DS:A2AC; INT 8 = timer_isr
     * (the original hooks it three times to learn the ISR far pointers for the music ISR, S5.4) */
    host_set_pit_divisor(0x4000);
    ds_far_wr(DS_old_int8, far_make(0xF000, 0xFEA5));   /* PORT: the BIOS INT 8 entry, never called */
    DSB(DS_isr_installing) = 0;
    snd_int8_set(timer_isr);
    /* sti */
    if (DSW(DS_snd_device) == 0) modules.adlib_timbres_load();     /* 2c57:1394 (S5.4) */
    kbd_install();
    DSW(DS_timer_on) = 1;
    /* out 43h, B6h: channel 2 mode 3 (implied by the host speaker model) */
    input_reset();
    DSW(DS_timer_installed) = 1;
}

/* 0001:189e timer_restore */
void timer_restore(void)
{
    snd_int8_set(NULL);                           /* PIT 0 back to 18.2 Hz, old INT 8 */
    host_set_pit_divisor(0);
    kbd_restore();
    DSW(DS_timer_on) = 0;
    DSW(DS_timer_installed) = 0;
}

/* 0001:18ce platform_exit (game_flow): "Quit demo" box in demo mode, arena_free_all, timer_restore,
 * the sound exit (slot 21) and the video restore. */
_Noreturn void platform_exit(void)
{
    if (DSW(DS_demo) != 0) modules.fatal_message(ds_str(DS_quit_demo));     /* DS:1F84 "Quit demo" */
    arena_free_all();
    if (DSW(DS_timer_installed) != 0) timer_restore();
    if (DSW(DS_snd_state) != 0) modules.snd_exit();                         /* sound_vec(21) */
    if (DSW(DS_gfx_mode_set) != 0) {
        drv_pal_normal();                         /* lcall [DS:A0CE] */
        /* PORT: 0001:c978 video_restore (equipment byte, BIOS text mode) has no host equivalent: the
         * window closes below. */
    }
    host_port61(0);                               /* PORT: speaker off (the sound exit does this in the original) */
    /* PORT: exit(5) of the original; the port exits with 0 (the DOS code carries no meaning). */
    host_shutdown();
    exit(0);
}
