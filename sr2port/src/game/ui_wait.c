/* The SR2 UI toolkit: the input / wait loop (game_flow.md 0001:1d02 ui_wait, 0001:3d80 wait_ticks, 1a21:0dca
 * wait_click_or_key, 0001:17ea input_reset, 0001:3245) and the demo player (1a21:4806 demo_step). */
#include "game/ui.h"
#include <stdlib.h>
#include <stdio.h>

#include <string.h>

#include "game/garage.h"
#include "game/snd.h"
#include "host.h"
#include "platform/platform.h"
#include "platform/video.h"

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

/* The juke-box key ('M'): music on / off. */
static void music_toggle(void)
{
    if (DSW(DS_music_on) != 0) {
        music_stop();
        DSW(DS_music_on) = 0;
    } else {
        DSW(DS_music_on) = 1;
        music_resume();
    }
}

static bool in_hot_rect(s16 x, s16 y)
{
    return DSS(DS_ui_hot_x0) <= x && DSS(DS_ui_hot_x1) > x && DSS(DS_ui_hot_y0) <= y && DSS(DS_ui_hot_y1) > y;
}

/* 0001:1d02 ui_wait: waits max_ticks; returns 0 on timeout, 1000 when the deadline DS:1F3A passes, 0x3EB for any
 * key / click while DS:1F3E (key in DS:1F40) or a click with DS:1F4C, otherwise the code of the hot spot clicked or
 * the hot key pressed (the keyboard remap holds code - 0x32). */
s16 ui_wait(s16 max_ticks)
{
    s16 k;
    s16 cx = 0, cy = 0;
    bool timed = (s32)DSL(DS_wait_until) > (s32)ticks_now();
    u32 until = ticks_now() + (u32)(s32)max_ticks;
    do {
        host_pump();
        if (DSW(DS_demo) != 0 && DSS(DS_demo_target_x) == -2) demo_step();
        k = (s8)key_read();
        if (DSW(DS_demo) == 0) {
            if (k > 0) {
                if (DSW(DS_wait_any_key) != 0) {
                    DSW(DS_last_key) = (u16)k;
                    return 0x3EB;
                }
                if (k == 0x7F) platform_exit();
            } else if (k < 0) {
                k = (s16)(k + 0x32);
                if (k != -0x28) return k;                      /* the code of a hot key */
                if (DSW(DS_wait_any_key) != 0) {
                    DSW(DS_last_key) = 0x4D;
                    return 0x4D;
                }
                if (DSW(DS_music_mode) == 1 && DSW(DS_music_off) == 0) music_toggle();
            }
        } else {                                                /* demo: 'Q' quits, only the juke-box key works */
            if (k == 0x51) platform_exit();
            k = (s16)(k + 0x32);
            if (k == -0x28 && DSW(DS_music_mode) == 1 && DSW(DS_music_off) == 0) music_toggle();
        }

        if (DSW(DS_demo) == 0 && DSB(DS_mouse_present)) {
            mouse_poll();
            if (DSW(DS_mouse_dx) != 0 || DSW(DS_mouse_dy) != 0) DSB(DS_cursor_moved) |= 1;
            if (DSB(DS_cursor_moved) != 0) {
                DSW(DS_cursor_x) = (u16)(DSW(DS_cursor_x) + DSW(DS_mouse_dx));
                DSW(DS_cursor_y) = (u16)(DSW(DS_cursor_y) + DSW(DS_mouse_dy));
            }
        }

        if (DSB(DS_click_code) != 0 || DSB(DS_click2_code) != 0) {   /* a click */
            if (DSW(DS_demo) != 0 && DSS(DS_demo_target_x) == -1) DSS(DS_demo_target_x) = -2;
            cx = DSS(DS_click_x);
            cy = DSS(DS_click_y);
            DSB(DS_click_code) = 0;
            DSB(DS_click2_code) = 0;
            if (DSW(DS_click_returns) != 0) {                   /* a message box is up: any click ends the wait */
                DSW(DS_last_key) = 0;
                return 0x3EB;
            }
            if (DSS(DS_ui_depth) != 0) {                        /* a menu is active */
                s16 id;
                if (in_hot_rect(cx, cy)) id = DSS(DS_ui_hover);
                else id = hotspot_find(cx, cy);
                if (getenv("SR_TRACE")) fprintf(stderr, "trace: click %d,%d -> %d\n", cx, cy, id);   /* PORT: developer aid */
                if (id != 0) return id;
            }
            if (DSW(DS_wait_any_key) != 0) {
                DSW(DS_last_key) = 0;
                return 0x3EB;
            }
        } else if (timed && (s32)DSL(DS_wait_until) < (s32)ticks_now()) {
            DSL(DS_wait_until) = 0;
            return 1000;
        }

        if (DSC(DS_cursor_visible) > 0 && DSB(DS_cursor_moved) != 0) {   /* the pointer moved: redraw it */
            s16 x = DSS(DS_cursor_x), y;
            if (x < 0 || x > 0x13A) { x = x < 0 ? 0 : 0x13A; DSS(DS_cursor_x) = x; }
            y = DSS(DS_cursor_y);
            if (y < 0 || y > 0xC2) { y = y < 0 ? 0 : 0xC2; DSS(DS_cursor_y) = y; }
            DSB(DS_cursor_moved) = 0;
            cursor_redraw(x, y);
            if (DSS(DS_ui_depth) != 0 && !in_hot_rect(x, y)) {      /* the status line shows the hot spot's text */
                s16 prev = DSS(DS_ui_hover);
                s16 id = hotspot_find(x, y);
                if (id != 0) {
                    if (id != prev) status_print(-1, ds_str(DSW((u16)(DSW(DS_ui_hit) + UIEL_TEXT))), 0);
                } else if (prev != 0) {
                    status_print(-1, ds_str(DS_empty_str), 0);
                }
            }
        }
    } while ((s32)until > (s32)ticks_now());
    return 0;
}

/* 0001:3d80 wait_ticks: n ticks, or a key / click. */
void wait_ticks(s16 n)
{
    DSL(DS_wait_until) = ticks_now() + (u32)(s32)n;
    DSW(DS_wait_any_key) = 1;
    ui_wait(30000);
    DSL(DS_wait_until) = 0;
    DSW(DS_wait_any_key) = 0;
}

/* 1a21:0dca wait_click_or_key: returns after t ticks, on a pending click, the mouse or joystick button, Enter or
 * Esc; 'M' toggles the music meanwhile. */
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
        if (k == 'M') music_toggle();
        else if (k == '\r' || k == 0x1B) return;
    }
}

/* 0001:3245 kbd_inject_scancode: a dead copy of the INT 9 handler body (kbd_isr) for the scan code in AL, without
 * the A/Z race keys, the port-61 acknowledge, DS:3BD4 and DS:3BF6; nothing calls it. */
void kbd_inject_scancode(u8 sc)
{
    if (sc >= 0xE0) return;
    bool make = !(sc & 0x80);
    u8 i = sc & 0x7F;
    if (i > 0x55) { i = 0x56; make = true; }
    s8 c = (s8)DSB((u16)(DS_kbd_xlat + i));
    if (c == 0) return;
    if (c < 0 && c >= -30) {                                    /* direction key */
        u16 dxy = (u16)(DS_kbd_xlat + c);
        u16 held = (u16)(DS_kbd_held_flags + c);
        if (make) {
            if (DSB(held) != 0) return;
            DSB(held) = 1;
            DSW(DS_kbd_dir_lock) = 0xFFFF;
            DSB(DS_kbd_dir_x) = (u8)(DSB(DS_kbd_dir_x) + DSB(dxy));
            DSB(DS_kbd_dir_y) = (u8)(DSB(DS_kbd_dir_y) + DSB((u16)(dxy + 1)));
            DSW(DS_kbd_last_dir) = DSW(dxy);
            DSW(DS_kbd_dir_lock) = 0;
        } else {
            if (DSB(held) == 0) return;
            DSW(DS_kbd_dir_lock) = 0xFFFF;
            DSB(held) = 0;
            DSB(DS_kbd_dir_x) = (u8)(DSB(DS_kbd_dir_x) - DSB(dxy));
            DSB(DS_kbd_dir_y) = (u8)(DSB(DS_kbd_dir_y) - DSB((u16)(dxy + 1)));
            DSW(DS_kbd_dir_lock) = 0;
        }
        return;
    }
    if (make) {
        if (c == 0x1F) {
            if (DSB(DS_click_held) == 0) {
                DSW(DS_kbd_dir_lock) = 0xFFFF;
                DSB(DS_click_code) = 0x1F;
                DSW(DS_click_x) = DSW(DS_cursor_x);
                DSW(DS_click_y) = DSW(DS_cursor_y);
                DSW(DS_kbd_dir_lock) = 0;
            }
            DSB(DS_click_held) |= 1;
        } else if (c == 0x1E) {
            if (DSB(DS_click_held) == 0) {
                DSW(DS_kbd_dir_lock) = 0xFFFF;
                DSB(DS_click2_code) = 0x1E;
                DSW(DS_click2_x) = DSW(DS_cursor_x);
                DSW(DS_click2_y) = DSW(DS_cursor_y);
                DSW(DS_kbd_dir_lock) = 0;
            }
        } else if (c == 0x1D) {
            DSB(DS_sound_on) ^= 1;
        } else {
            DSB(DS_key) = (u8)c;
        }
    } else if (c == 0x1F) {
        DSW(DS_kbd_dir_lock) = 0xFFFF;
        DSB(DS_click_held) &= 0xFE;
        DSW(DS_kbd_dir_lock) = 0;
    }
}

/* ---- demo player */

/* demo_step: no menu on screen, or the script says "wait": aim the pointer at the middle of the screen (the timer ISR
 * glides it there and injects the click). */
static void demo_aim_idle(void)
{
    DSS(DS_demo_target_x) = 0xA0;
    DSS(DS_demo_target_y) = 100;
    if (DSS(DS_cursor_x) == 0xA0) DSS(DS_demo_target_x) = 0xD2;
}

/* The element with the given code, from the first one. PORT: the original scans on without a stop; a missing code
 * yields 0 here (the caller then re-picks) instead of running through DGROUP. */
static u16 demo_find_code(u16 el, s16 code)
{
    for (;;) {
        if (UIEL_S(el, UIEL_CODE) == code) return el;
        if (DSW(el) & UIF_LAST) return 0;
        el = (u16)(el + UIEL_BYTES);
    }
}

/* 1a21:4806 demo_step: the demo (DS:9730) plays by itself with the mouse pointer. Picks the next element to click
 * from the menu on top of the stack, stores its centre as the target (DS:20EA / B196). */
void demo_step(void)
{
    s16 menu = DSS((u16)(DS_menu_stack + 2 * DSS(DS_ui_depth)));
    u16 el0 = DSW(DS_ui_cur);
    s16 target = 0;
    u16 e = 0;

    if (el0 == 0) {                                             /* no menu: a message box or an animation */
        wait_click_or_key(0x8C);
        demo_aim_idle();
        return;
    }
    if (menu == 0 && DSS(DS_car_count) > 2) {
        target = 6;                                             /* garage: "Cars" */
    } else if (menu == 0 && DSS(DS_part_count) > 0x14) {
        target = 5;                                             /* "Sell parts" */
    } else {
        switch (menu) {
        case 0:                                                 /* the garage */
            DSS(DS_demo_pos_m22) = 0;
            DSS(DS_bob_races_started) = 0;
            if (DSW(DS_cur_car) == 0) {
                target = 1;                                     /* newspaper */
            } else if (DSS((u16)(DSW(DS_cur_car) + CR_GAS)) <= 0x28) {
                target = 15;                                    /* low on fuel: the gas station */
            } else {                                            /* shuffle bag of garage codes */
                s16 k = rnd(DSS(DS_demo_bag_left));
                s16 i = k;
                while (DSS((u16)(DS_demo_used + 2 * i)) > 0) i++;
                target = DSS((u16)(DS_demo_garage_codes + 2 * i));
                DSS((u16)(DS_demo_used + 2 * i)) = 1;
                if (--DSS(DS_demo_bag_left) == 0) {
                    DSS(DS_demo_bag_left) = 17;
                    memset(mp(DGROUP, DS_demo_used), 0, 34);
                }
            }
            break;
        case 1: target = -0x16; break;                          /* quit menu: Forget it */
        case 2: target = -3; break;                             /* licence: OK */
        case 3:                                                 /* newspaper menu */
            if (DSW(DS_spare_parts) == 0) {                     /* gift: tyres, exhaust manifold, differential */
                spare_add(0x34, DSS(DS_demo_gift_a));
                spare_add(0x36, DSS(DS_demo_gift_b));
                spare_add(0x3F, DSS(DS_demo_gift_c));
            }
            if (DSW(DS_cur_car) == 0) target = 1;
            break;
        case 6: wait_click_or_key(0x8C); break;
        case 7: case 9: case 16:
            if (DSS(DS_demo_prev_menu) == 0x0C || DSS(DS_demo_prev_menu) == 0x22) {
                target = -3;
            } else if (++DSS(DS_demo_cnt_m7) >= 5) {
                target = menu == 9 ? -1 : -3;
                DSS(DS_demo_cnt_m7) = 0;
            }
            break;
        case 12:                                                /* script {1,2,1,2,-2,2,1,2,1,-1} */
            target = DSS((u16)(DS_demo_script_m12 + 2 * DSS(DS_demo_pos_m12)));
            e = demo_find_code(el0, target);
            if (e == 0 || !(DSW(e) & UIF_ACTIVE)) {
                DSS(DS_demo_target_x) = -2;
                return;
            }
            if (target == -1) DSS(DS_demo_pos_m12) = 0;
            else DSS(DS_demo_pos_m12)++;
            break;
        case 14: {                                              /* under the car */
            if (DSS(DS_demo_prev_menu) == 0x1E) {
                target = -1;
            } else {
                s16 r = rnd(4);
                target = r < 0 ? 0 : r <= 1 ? -3 : r == 2 ? -2 : r == 3 ? -1 : 0;
            }
            break;
        }
        case 20: target = DSS(DS_bob_races_started) == 0x0F ? 1 : 2; break;
        case 22:                                                /* script {6,5,3,1,0x3EB x4,1,3,0} */
            target = DSS((u16)(DS_demo_script_m22 + 2 * DSS(DS_demo_pos_m22)));
            DSS(DS_demo_pos_m22)++;
            if (target == 0) {
                DSS(DS_demo_pos_m22) = 0;
            } else if (target == 0x3EB) {
                demo_aim_idle();
                return;
            }
            break;
        case 27: case 33: DSS(DS_demo_target_x) = 2; break;     /* (overwritten below) */
        case 32: target = -1; break;
        case 34:
            if (++DSS(DS_demo_cnt_m34) >= 5) {
                target = -1;
                DSS(DS_demo_cnt_m34) = 0;
            }
            break;
        default: break;
        }
    }
    if (target == 0) {                                          /* a random active element */
        s16 n = 0;
        u16 last;
        e = el0;
        do {
            if (DSW(e) & UIF_ACTIVE) n++;
            last = DSW(e);
            e = (u16)(e + UIEL_BYTES);
        } while (!(last & UIF_LAST));
        s16 k = rnd(n), i = 0;
        e = el0;
        for (;;) {
            while (!(DSW(e) & UIF_ACTIVE) && !(DSW(e) & UIF_LAST)) e = (u16)(e + UIEL_BYTES);   /* PORT: bounded */
            if (i++ >= k || (DSW(e) & UIF_LAST)) break;
            e = (u16)(e + UIEL_BYTES);
        }
    } else {
        e = demo_find_code(el0, target);                        /* (no flag test) */
        if (e == 0) {                                           /* PORT: see demo_find_code */
            DSS(DS_demo_target_x) = -2;
            return;
        }
    }
    DSS(DS_demo_target_x) = (s16)((UIEL_S(e, UIEL_X0) + UIEL_S(e, UIEL_X1)) >> 1);
    DSS(DS_demo_target_y) = (s16)((UIEL_S(e, UIEL_Y0) + UIEL_S(e, UIEL_Y1)) >> 1);
    if (DSS(DS_cursor_x) == DSS(DS_demo_target_x))              /* already in that column: aim at the right half */
        DSS(DS_demo_target_x) = (s16)(DSS(DS_demo_target_x) + ((UIEL_S(e, UIEL_X1) - DSS(DS_demo_target_x)) >> 1));
    DSS(DS_demo_prev_menu) = menu;
}
