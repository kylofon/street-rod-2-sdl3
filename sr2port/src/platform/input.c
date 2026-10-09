/* Platform: mouse (INT 33h), joystick (port 201h), click_clear, input_poll_any - platform.md 1a21:769c-7738,
 * 0001:31a6, 0001:1708, 0001:8c2c. (ui_wait, input_reset and the wait loops are game_flow: game/flow_stub.c.) */
#include "platform/platform.h"

#include "host.h"
#include "platform/video.h"

/* ------------------------------------------------------------------------------------------------ */
/* Mouse: INT 33h through int86 (1e16:1b9a), replaced by the host mouse.                            */

/* 1a21:769c mouse_reset: 0 if the INT 33h vector 0000:00CC is null, else INT 33h AX=0 (FFFFh = a
 * mouse). PORT: the host always has a mouse driver (the vector test cannot fail); the "reset" puts the
 * host pointer in relative mode, since the game only reads motion and draws its own pointer. */
/* Developer aid (host.h, SR_KEYS mouse entries): the pointer position DS:3B32/3B34. */
static bool pointer_query(s16 *x, s16 *y)
{
    *x = DSS(DS_cursor_x);
    *y = DSS(DS_cursor_y);
    return true;
}

s16 mouse_reset(void)
{
    host_set_pointer_query(pointer_query);
    host_mouse_set_relative(true);
    s16 mx, my;
    host_mouse_mickeys(&mx, &my);                 /* the reset clears the mickey counters */
    return -1;
}

/* 1a21:76d6 mouse_buttons: INT 33h AX=3 -> DS:05EE = BX & 1, DS:05F0 = BX & 2 */
void mouse_buttons(void)
{
    u8 b = 0;
    host_mouse_read(NULL, NULL, &b);
    DSW(DS_mouse_left) = b & 1;
    DSW(DS_mouse_right) = b & 2;
}

/* 1a21:7708 mouse_motion: INT 33h AX=0Bh, mickeys since the last call (CX, DX) */
void mouse_motion(s16 *dx, s16 *dy)
{
    host_mouse_mickeys(dx, dy);
}

/* 1a21:7738 mouse_poll — platform.md §4.5 */
void mouse_poll(void)
{
    s16 mx, my;
    mouse_motion(&mx, &my);
    DSW(DS_mouse_acc_x) = (u16)(DSW(DS_mouse_acc_x) + mx);
    DSW(DS_mouse_acc_y) = (u16)(DSW(DS_mouse_acc_y) + my);

    s16 acc = DSS(DS_mouse_acc_x);
    bool neg = acc < 0;
    s16 a = (s16)((s16)(neg ? -acc : acc) >> (DSB(DS_mouse_shift_x) & 0x1F));   /* sar ax, cl */
    DSS(DS_mouse_dx) = a;
    if (a) {
        if (neg) DSS(DS_mouse_dx) = (s16)-a;
        DSW(DS_mouse_acc_x) = 0;                  /* the remainder is dropped */
    }
    acc = DSS(DS_mouse_acc_y);
    neg = acc < 0;
    a = (s16)((s16)(neg ? -acc : acc) >> (DSB(DS_mouse_shift_y) & 0x1F));
    DSS(DS_mouse_dy) = a;
    if (a) {
        if (neg) DSS(DS_mouse_dy) = (s16)-a;
        DSW(DS_mouse_acc_y) = 0;
    }

    mouse_buttons();
    if (DSW(DS_mouse_left) != 0) {
        if (DSB(DS_click_held) == 0) {
            DSB(DS_click_code) = 1;
            DSW(DS_click_x) = DSW(DS_cursor_x);
            DSW(DS_click_y) = DSW(DS_cursor_y);
        }
        DSB(DS_click_held) |= 4;
    } else {
        DSB(DS_click_held) &= 0xFB;
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Joystick                                                                                          */

/* 0001:31a6 joy_read. The original times the gameport one-shots (at most 400h
 * reads); the counts, minus the calibration centre DS:3B26/3B28, go to DS:3B20/3B22, the buttons to
 * DS:3B24 (and a click).
 * PORT: the gameport is the host's first gamepad: raw count = 100h + axis * FFh / 32767 (1..1FFh, so
 * a centred stick reads 100h and the thresholds +-centre/2 are half deflection). Without a gamepad
 * the one-shots never fire and the original leaves through its timeout path: nothing is updated. */
void joy_read(void)
{
    s16 ax, ay;
    u8 b;
    if (!host_joy_read(&ax, &ay, &b)) return;     /* jcxz: timeout */
    s16 cx = (s16)(0x100 + (s32)ax * 0xFF / 32767);
    s16 cy = (s16)(0x100 + (s32)ay * 0xFF / 32767);
    DSS(DS_joy_x) = (s16)(cx - DSS(DS_joy_centre_x));
    DSS(DS_joy_y) = (s16)(cy - DSS(DS_joy_centre_y));
    u8 bt = b & 3;                                /* not al; shr al, 4; and al, 3 */
    DSB(DS_joy_buttons) = bt;
    if (bt) {
        if (DSB(DS_click_held) == 0) {
            DSB(DS_click_code) = bt;
            DSW(DS_click_x) = DSW(DS_cursor_x);
            DSW(DS_click_y) = DSW(DS_cursor_y);
        }
        DSB(DS_click_held) |= 2;
    } else {
        DSB(DS_click_held) &= 0xFD;
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Clicks, polling                                                                                   */

/* 0001:1708 click_clear: DS:881C = 1 (pointer moved), DS:B880 = -1 (invalidates the cached hot-spot
 * rectangle), DS:B898 = 32000 (hovered code: none), both pending clicks cleared, the key slot discarded. */
void click_clear(void)
{
    DSB(DS_cursor_moved) = 1;
    DSW(DS_hot_x1) = 0xFFFF;
    DSW(DS_hot_code) = 32000;
    DSB(DS_click_code) = 0;
    DSB(DS_click2_code) = 0;
    key_read();                                   /* discards the key slot */
}

/* 0001:8c2c input_poll_any (new): polls every input device once for a button or Esc/Enter and records it
 * in the click-seen flag DS:3BF6. Used by the race/event wait loops. */
void input_poll_any(void)
{
    if (DSB(DS_mouse_present)) {
        mouse_buttons();
        DSW(DS_click_seen) |= DSW(DS_mouse_left);
    }
    if (DSB(DS_joy_present)) {
        joy_read();
        DSW(DS_click_seen) |= DSB(DS_joy_buttons);
    }
    u8 c = (u8)key_read();
    if (c == 0x1B || c == 0x1F) DSW(DS_click_seen) = 1;
}
