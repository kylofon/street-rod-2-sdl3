/* PORT: stand-ins for the game_flow module (S5.2), just enough for the start-up chain of S5.1 to run to the
 * title: input_reset, wait_click_or_key, title_sequence, rnd, and the data loaders. Every function here is
 * game_flow's (port/spec/game_flow.md); S5.2 replaces this file with the real module. */
#include "game/game.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <time.h>

#include "host.h"
#include "modules.h"
#include "platform/platform.h"
#include "platform/video.h"

int game_run_ms;

/* 1a21:6180 rnd: a 32-bit LCG (SR1: Wichmann-Hill). n < 1: seed = time(NULL), returns -1. */
s16 rnd(s16 n)
{
    if (n < 1) {
        DSL(DS_rnd_seed) = (u32)time(NULL);
        return -1;
    }
    u32 seed = DSL(DS_rnd_seed) * 0x015A4E35u + 1;
    DSL(DS_rnd_seed) = seed;
    return (s16)(((seed >> 16) & 0x7FFF) % (u32)(u16)n);
}

/* 0001:17ea input_reset: wait until 10 ticks pass without a key, then clear the pending input state. */
void input_reset(void)
{
    u32 until = ticks_now() + 10;
    while (tick_after(until, ticks_now())) {
        s16 n = 0;
        while ((u8)key_read() != 0) n++;
        if (n) until = ticks_now() + 10;
        host_pump();
    }
    DSL(DS_wait_until) = 0;
    if (DSW(DS_demo) == 0 && DSB(DS_mouse_present)) {
        mouse_poll();
        DSW(DS_mouse_acc_y) = 0;
        DSW(DS_mouse_acc_x) = 0;
    }
    DSB(DS_click_code) = 0;
    for (u16 i = 0; i < 16; i++) DSB((u16)(DS_kbd_held_flags_a + i)) = 0;
    DSB(DS_kbd_dir_aux) = 0;
    DSB(DS_kbd_last_dir + 1) = 0;
    DSB(DS_kbd_last_dir) = 0;
    DSB(DS_kbd_dir_y) = 0;
    DSB(DS_kbd_dir_x) = 0;
    DSB(DS_click_held) = 0;
}

/* 1a21:0dca wait_click_or_key: returns after t ticks, on a pending click, the mouse or joystick button,
 * Enter or Esc. (The 'M' music toggle is S5.4.) */
void wait_click_or_key(s16 t)
{
    u32 until = ticks_now() + (u32)(s32)t;
    while (tick_after(until, ticks_now())) {
        host_pump();
        if (DSB(DS_click_code) != 0) {
            DSB(DS_click_code) = 0;
            return;
        }
        if (DSB(DS_mouse_present)) {
            mouse_buttons();
            if (DSW(DS_mouse_left) != 0) return;
        }
        if (DSB(DS_joy_present)) {
            joy_read();
            if (DSB(DS_joy_buttons) != 0) {
                DSB(DS_joy_buttons) = 0;
                DSB(DS_click_code) = 0;
                return;
            }
        }
        u8 k = (u8)key_read();
        if (k == '\r' || k == 0x1B) return;
    }
}

/* 0001:3d80 wait_ticks (not the metronome pace_ticks): returns after n ticks or on any key / click. PORT:
 * the original runs ui_wait(30000) with the any-key flag; this is the same without the hot-spot machinery. */
static void wait_ticks_or_input(s16 n)
{
    u32 until = ticks_now() + (u32)(s32)n;
    while (tick_after(until, ticks_now())) {
        host_pump();
        if (key_read() != 0 || DSB(DS_click_code) != 0) {
            DSB(DS_click_code) = 0;
            return;
        }
    }
}

/* 1a21:0e82 title_sequence (VGA path): "California Dreams presents", the title, the credits (two waits). */
void title_sequence(void)
{
    u16 h = DSW(DS_pic_rows);
    if (DSS(DS_driver_id) == -2 || DSS(DS_driver_id) == -6) drv_pal_black();
    DSW(DS_pic_rows) = 200;
    show_picture_at(3, 0x28, 0x28, 0);                /* LIB1 #2 "California Dreams presents" */
    pic_free(3);
    drv_pal_normal();
    DSW(DS_g_mirror) = 0;
    show_picture(1, -1);                              /* LIB1 #0 title, drawn off screen */
    pic_free(1);
    wait_click_or_key(400);
    drv_pal_normal();                                 /* title visible */
    drv_blinds(desc_planes(g_front()), desc_planes(g_back()));
    show_picture(4, -1);                              /* LIB1 #3 credits, off screen */
    pic_free(4);
    wait_ticks_or_input(0x14);
    wait_click_or_key(400);
    drv_blinds(desc_planes(g_front()), desc_planes(g_back()));   /* credits visible */
    wait_ticks_or_input(0x14);
    wait_click_or_key(600);
    DSW(DS_pic_rows) = h;
    screen_fill_rect(0, 0, 0x140, 200, 0);
    drv_blinds(desc_planes(g_front()), desc_planes(g_back()));   /* black */
}

/* 1a21:6e6a hot_data_load / 1a21:711c hall_load: SR2_DATA and the hall of fame (S5.2). */
void hot_data_load(void)
{
}

void hall_load(void)
{
}

/* 0001:3cc4 title_and_setup, up to its first call into the rest of game_flow. */
static void title_and_setup(void)
{
    gfx_screen_mode(0);
    /* sound_vec(18)(): the title music (S5.4) */
    title_sequence();
    drv_pal_black();
    gfx_screen_mode(1);
    /* S5.2: copy_protection() (0001:3378, unless "demo"), road_seed_init, the resident pictures, then the
     * new-game / load screen of game_loop 0001:4dd6. */
    drv_pal_normal();                                 /* PORT: so that the pointer below can be seen */
    cursor_ctl(-2);                                   /* the pointer, as title_and_setup ends */
}

/* PORT (S5.1 test aid): the pointer part of ui_wait 0001:1d02 - mouse deltas, the keyboard / joystick motion the
 * timer ISR left in DS:3B32/3B34, and the redraw - so that the pointer, the mouse, the keyboard ISR model and
 * the INT 8 handler can be tried at the end of the start-up chain. S5.2 replaces it with the real ui_wait. */
static void idle_pointer(void)
{
    host_pump();
    if (DSW(DS_demo) == 0 && DSB(DS_mouse_present)) {
        mouse_poll();
        u8 moved = (DSW(DS_mouse_dx) != 0 || DSW(DS_mouse_dy) != 0) ? 1 : 0;
        DSB(DS_cursor_moved) |= moved;
        if (DSB(DS_cursor_moved) != 0) {
            DSW(DS_cursor_x) = (u16)(DSW(DS_cursor_x) + DSW(DS_mouse_dx));
            DSW(DS_cursor_y) = (u16)(DSW(DS_cursor_y) + DSW(DS_mouse_dy));
        }
    }
    if (DSC(DS_cursor_visible) > 0 && DSB(DS_cursor_moved) != 0) {
        s16 x = DSS(DS_cursor_x), y = DSS(DS_cursor_y);
        if (x < 0 || x > 0x13A) { x = x < 0 ? 0 : 0x13A; DSS(DS_cursor_x) = x; }
        if (y < 0 || y > 0xC2) { y = y < 0 ? 0 : 0xC2; DSS(DS_cursor_y) = y; }
        DSB(DS_cursor_moved) = 0;
        cursor_redraw(x, y);
    }
    (void)key_read();
}

/* 0001:07b0 main after platform_main_init: wait_click_or_key(1), game_loop, platform_exit. */
int game_main(int argc, char **argv)
{
    platform_main_init(argc, argv);
    wait_click_or_key(1);
    title_and_setup();
    fprintf(stderr, "S5.1: start-up chain reached the first game_flow call (copy protection / new game, S5.2)\n");
    Uint64 end = game_run_ms > 0 ? SDL_GetTicks() + (Uint64)game_run_ms : 0;
    for (;;) {
        idle_pointer();
        if (end && SDL_GetTicks() >= end) break;
    }
    platform_exit();
}
