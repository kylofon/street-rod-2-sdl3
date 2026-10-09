/* Platform: the keyboard - INT 9 handler 0001:3014 with the translation table DS:3B6E, the single-byte
 * key slot, the hot-key remap and text mode - platform.md 0001:1bea-1c5c, 0001:2fd6-3194.
 *
 * The host delivers the XT set-1 byte stream (make / break codes, E0/E1 prefixes) in event order; each
 * byte is one INT 9 with that value at port 60h. The translation tables live in mem[] and are patched
 * by the game exactly as in the original. */
#include "platform/platform.h"

#include "host.h"

/* PORT: A / Z shift up / down during a race (not in the original, where Space, the joystick button and
 * the left mouse button are one "shift" control and the physics picks the direction from the pedals).
 * The key press is remembered until the next physics step takes it (race physics step, S5.3). */
static s8 shift_dir;

s8 kbd_take_shift_dir(void)
{
    s8 d = shift_dir;
    shift_dir = 0;
    return d;
}

/* 0001:3014 kbd_isr: SR1 kbd_isr without the demo gating (SR2 has neither the check nor the Esc rule). */
void kbd_isr(u8 sc)
{
    /* in al,61h; out 61h, al|80h; out 61h, al: the XT keyboard acknowledge (bit 7). The host speaker
     * model stores bits 2-7 only, so this has no audible effect. */
    u8 p = host_port61_get();
    host_port61((u8)(p | 0x80));
    host_port61(p);

    if (sc == 0xFF || sc >= 0xE0) return;                 /* E0 / E1 prefixes and overruns ignored */
    if (DSB(DS_racing) != 0 && (sc == 0x1E || sc == 0x2C || sc == 0x9E || sc == 0xAC)) {
        if (sc == 0x1E) shift_dir = 1;                    /* PORT: A = shift up (see kbd_take_shift_dir) */
        else if (sc == 0x2C) shift_dir = -1;              /* PORT: Z = shift down */
        return;                                           /* consumed: no 'A' / 'Z' left in the key slot */
    }

    bool make = !(sc & 0x80);
    u8 i = sc & 0x7F;
    if (i > 0x55) { i = 0x56; make = true; }              /* table[56h] = 0 */
    s8 k = (s8)DSB((u16)(DS_kbd_xlat + i));
    if (k == 0) return;
    if (k < 0 && k >= -30) {                              /* direction key: -2, -4, ... -16 */
        u16 dxy = (u16)(DS_kbd_xlat + k);                 /* (dx, dy) at DS:3B5E..3B6D */
        u16 held = (u16)(DS_kbd_held_flags + k);           /* held flags DS:3BD2..3BEF */
        if (make) {
            if (DSB(held) != 0) return;
            DSB(held) = 1;
                        DSB(DS_kbd_dir_x) = (u8)(DSB(DS_kbd_dir_x) + DSB(dxy));
            DSB(DS_kbd_dir_y) = (u8)(DSB(DS_kbd_dir_y) + DSB((u16)(dxy + 1)));
            DSW(DS_kbd_last_dir) = DSW(dxy);
            DSW(DS_kbd_dir_lock) = 0;
        } else {
            if (DSB(held) == 0) return;
                        DSB(held) = 0;
            DSB(DS_kbd_dir_x) = (u8)(DSB(DS_kbd_dir_x) - DSB(dxy));
            DSB(DS_kbd_dir_y) = (u8)(DSB(DS_kbd_dir_y) - DSB((u16)(dxy + 1)));
            DSW(DS_kbd_dir_lock) = 0;
        }
        return;
    }
    u8 key = (u8)k;
    if (make) {
        if (key == 0x1F) {                                /* primary click: Space, Ins, keypad 0, 55h */
            if (DSB(DS_click_held) == 0) {
                                DSB(DS_click_code) = 0x1F;
                DSW(DS_click_x) = DSW(DS_cursor_x);
                DSW(DS_click_y) = DSW(DS_cursor_y);
                DSW(DS_kbd_dir_lock) = 0;
            }
            DSB(DS_click_held) |= 1;
            DSB(DS_click_edge) = 1;
            DSW(DS_click_seen) = 1;
        } else if (key == 0x1E) {                         /* secondary click (no key in the default table) */
            if (DSB(DS_click_held) == 0) {
                                DSB(DS_click2_code) = 0x1E;
                DSW(DS_click2_x) = DSW(DS_cursor_x);
                DSW(DS_click2_y) = DSW(DS_cursor_y);
                DSW(DS_kbd_dir_lock) = 0;
            }
        } else if (key == 0x1D) {
            DSB(DS_sound_on) ^= 1;                        /* Ctrl: sound on / off */
        } else {
            DSB(DS_key) = key;                            /* the single key slot (overwrites) */
        }
    } else if (key == 0x1F) {
                DSB(DS_click_held) &= 0xFE;
        DSW(DS_kbd_dir_lock) = 0;
    }
}

/* 0001:3168 key_read: cli; al = DS:3BD7; DS:3BD7 = 0; sti (AH = 0) */
s16 key_read(void)
{
    u8 k = DSB(DS_key);
    DSB(DS_key) = 0;
    return k;
}

/* Keys released while the window has no focus never send their break code; the host reports the loss
 * of focus and the port sends the break codes of every key (breaks of keys that are up are no-ops in
 * kbd_isr; Ctrl acts on its make code only).
 * PORT: host-side, no counterpart in the original. */
static void kbd_focus_lost(void)
{
    for (u8 sc = 0x01; sc <= 0x55; sc++) kbd_isr((u8)(sc | 0x80));
}

/* 0001:3175 kbd_install: INT 9 = 0001:3014, the old vector in DS:3BDC/3BDE */
void kbd_install(void)
{
    ds_far_wr(DS_old_int9, far_make(0xF000, 0xE987));    /* PORT: the BIOS INT 9 entry, never called */
    host_set_kbd_handler(kbd_isr);
    host_set_focus_lost_handler(kbd_focus_lost);
}

/* 0001:3194 kbd_restore */
void kbd_restore(void)
{
    host_set_kbd_handler(NULL);
    host_set_focus_lost_handler(NULL);
}

/* 0001:1c5c kbd_hotspot_refresh (new): rebuilds the hot-key remap of the current menu. The saved pairs
 * {scan, old key} are at DS:1FBE (0-terminated); every active hot spot of the menu (UIEL records of 0x12
 * bytes, DS:B894, up to the one with flags & 2) puts its two key bytes (+10h, +11h; FFh = none) onto the
 * translation table as the negative hot-key code (code - 32h) that ui_wait maps back. The last pair is
 * scan 32h ('M') -> A6h (the juke-box hot key). */
void kbd_hotspot_refresh(void)
{
    u16 p = DS_kbd_saved_pairs;
    s16 n = 1;
    kbd_remap_restore(0);
    if (DSW(DS_menu_depth) != 0) {
        DSW(DS_menu_aux) = DSW((u16)(DS_menu_stack + 2 * DSW(DS_menu_depth)));
        u16 el = DSW(DS_menu_elems);
        for (;;) {
            u16 flags = DSW((u16)(el + 0));
            if (flags & 1) {
                for (int j = 0; j < 2; j++) {
                    u8 k = DSB((u16)(el + 0x10 + j));
                    if (k == 0xFF) break;
                    DSB(p) = k;
                    DSB((u16)(p + 1)) = DSB((u16)(DS_kbd_xlat + k));
                    DSB((u16)(DS_kbd_xlat + k)) = (u8)((u8)DSW((u16)(el + 6)) - 0x32);
                    p = (u16)(p + 2);
                    n++;
                }
            }
            if (flags & 2) break;
            el = (u16)(el + 0x12);
        }
    }
    (void)n;
    DSB(p) = 0x32;
    DSB((u16)(p + 1)) = DSB((u16)(DS_kbd_xlat + 0x32));
    DSB((u16)(DS_kbd_xlat + 0x32)) = 0xA6;
    DSB((u16)(p + 2)) = 0;
}

/* 0001:1bea kbd_remap_restore: clears the held flags DS:3BE0..3BEF and DS:3B36..3B39 and restores the
 * translation table from the saved pairs at DS:1FBE ({scan, old}, 0-terminated): all of them, or only
 * those with old > 1Fh when keep_low != 0; DS:1FBE = 0 (remap inactive) when keep_low == 0. Unlike
 * SR1 there is no "remap active" flag and no return value. */
void kbd_remap_restore(s16 keep_low)
{
    for (u16 i = 0; i < 16; i++) DSB((u16)(DS_kbd_held_flags_a + i)) = 0;
    DSB(DS_kbd_dir_aux) = 0;
    DSB(DS_kbd_last_dir + 1) = 0;
    DSB(DS_kbd_last_dir) = 0;
    DSB(DS_kbd_dir_y) = 0;
    DSB(DS_kbd_dir_x) = 0;
    u16 si = DS_kbd_saved_pairs;
    if (keep_low != 0) {
        for (; DSB(si) != 0; si += 2)
            if (DSB((u16)(si + 1)) > 0x1F) DSB((u16)(DS_kbd_xlat + (s8)DSB(si))) = DSB((u16)(si + 1));
    } else {
        for (; DSB(si) != 0; si += 2) DSB((u16)(DS_kbd_xlat + (s8)DSB(si))) = DSB((u16)(si + 1));
        DSB(DS_kbd_saved_pairs) = 0;
    }
}

/* 0001:2fd6 kbd_text_mode_on (cli ... sti) */
void kbd_text_mode_on(s16 keep_space)
{
    if (keep_space == 0) DSB(DS_kbd_xlat + 0x39) = 0x20;   /* Space (39h) -> ' ' */
    DSB(DS_kbd_xlat + 0x0E) = 0x08;   /* Backspace (0Eh) */
    DSB(DS_kbd_xlat + 0x33) = 0x2C;   /* ',' (33h) */
    DSB(DS_kbd_xlat + 0x34) = 0x2E;   /* '.' (34h) */
    DSB(DS_click_held) &= 0xFE;
}

/* 0001:2ffd kbd_text_mode_off (cli ... sti) */
void kbd_text_mode_off(void)
{
    DSB(DS_kbd_xlat + 0x0E) = 0;
    DSB(DS_kbd_xlat + 0x33) = 0;
    DSB(DS_kbd_xlat + 0x34) = 0;
    DSB(DS_kbd_xlat + 0x39) = 0x1F;               /* Space: the click again */
}
