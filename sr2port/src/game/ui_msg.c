/* The SR2 UI toolkit: message boxes and the generic error box (garage.md "Message boxes", game_flow.md
 * msg_box): 1a21:2018 - 2888, 1a21:1e50, 0001:3db4, 0001:4d90. */
#include "game/ui.h"

#include <string.h>

#include "host.h"
#include "modules.h"
#include "platform/platform.h"
#include "platform/textmode.h"
#include "platform/video.h"

/* Box record: DS:1E46 + 0x12 * type. */
#define BOX_PIC    0x00
#define BOX_LINES  0x02
#define BOX_X      0x04
#define BOX_Y      0x06
#define BOX_W      0x08
#define BOX_H      0x0A
#define BOX_TDX    0x0C
#define BOX_TDY    0x0E
#define BOX_BYTES  0x12

#define MSG_BOX_TYPE(id)  DSW((u16)(DS_msg_table + 6 * (id)))
#define MSG_LINE(id, i)   DSW((u16)(DS_msg_table + 6 * (id) + 2 + 2 * (i)))

/* While racing (DS:B7AE) the box is drawn straight to the visible page: DS:AD26 = 1, the front page pointer
 * becomes the race front page and the back page the race back page (EGA double buffering) or the front page. */
static void race_pages_on(FarPtr *saved_front, u16 *saved_present)
{
    *saved_front = ds_far(DS_g_front);
    *saved_present = DSW(DS_g_mirror);
    DSW(DS_g_mirror) = 1;
    FarPtr back;
    if (DSS(DS_driver_id) == -2) {
        ds_far_wr(DS_g_front, ds_far(DS_page1_ptr));
        back = ds_far(DS_page2_ptr);
    } else {
        back = ds_far(DS_page1_ptr);
    }
    ds_far_wr(DS_g_back, back);
}

/* The original restores BOTH page pointers from the saved front pointer (sic). */
static void race_pages_off(FarPtr saved_front, u16 saved_present)
{
    ds_far_wr(DS_g_back, saved_front);
    ds_far_wr(DS_g_front, saved_front);
    DSW(DS_g_mirror) = saved_present;
}

/* 1a21:2018 dialog_msg: the line into DS:AB04, then screen 27 (a box with the text and an OK button). */
void dialog_msg(const char *s)
{
    strcpy(ds_str(DS_dialog_text), s);
    ui_dialog(27);
}

/* 1a21:204c fatal_message: the error box of the start-up code and "Quit demo" (hooked as modules.fatal_message). */
void fatal_message(const char *s)
{
    u16 saved_mirror = DSW(DS_g_mirror);
    u16 demo = DSW(DS_demo);
    DSW(DS_demo) = 0;
    strcpy(ds_str(DS_fatal_text), s);
    if (DSC(DS_ui_level) < 1) {
        /* text mode: puts(DS:1E2C), puts(message), puts("Hit Return when ready ..."), wait for a key
         * (PORT: the DOS console calls are the text-mode model's). */
        dos_print_line(ds_str(DS_fatal_head), 0);
        dos_print_line(ds_str(DS_fatal_text), 0);
        dos_print_line(ds_str(DS_fatal_hit_ret), 0);
        while (!text_kbhit()) host_pump();
        (void)text_getch();
        DSW(DS_demo) = demo;
        return;
    }
    if (DSW(DS_fatal_cleared) != 0) {
        screen_fill_rect(0, 0, 0x140, 200, 0);
        DSW(DS_fatal_cleared) = (u16)(DSW(DS_fatal_cleared) + 1);
    }
    u8 visible = DSB(DS_g_pal_visible);
    if (visible == 0) {
        DSB(DS_g_pal_visible) = 1;
        drv_pal_normal();
    }
    if (DSB(DS_fatal_shift) != 0) {                      /* the box's y coordinates move up by 0x4B */
        u16 e = ui_screen(33);
        for (;;) {
            UIEL_S(e, UIEL_Y0) = (s16)(UIEL_S(e, UIEL_Y0) - 0x4B);
            UIEL_S(e, UIEL_Y1) = (s16)(UIEL_S(e, UIEL_Y1) - 0x4B);
            if (DSB(e) & UIF_LAST) break;
            e = (u16)(e + UIEL_BYTES);
        }
    }
    if (DSW(DS_fatal_cleared) != 0) ega_set_palette(DS_fatal_pal);
    DSW(DS_g_mirror) = 1;
    input_reset();
    FarPtr back = g_back();
    ds_far_wr(DS_g_back, g_front());                     /* draw on the visible page */
    ui_dialog(33);                                       /* box, "OK", the text, "Hit Return when ready ..." */
    ds_far_wr(DS_g_back, back);
    if (DSB(DS_fatal_shift) != 0) {
        u16 e = ui_screen(33);
        for (;;) {
            UIEL_S(e, UIEL_Y0) = (s16)(UIEL_S(e, UIEL_Y0) + 0x4B);
            UIEL_S(e, UIEL_Y1) = (s16)(UIEL_S(e, UIEL_Y1) + 0x4B);
            if (DSB(e) & UIF_LAST) break;
            e = (u16)(e + UIEL_BYTES);
        }
    }
    DSW(DS_g_mirror) = saved_mirror;
    DSB(DS_g_pal_visible) = visible;
    if (visible == 0) drv_pal_black();
    DSW(DS_demo) = demo;
}

/* 1a21:2530 msg_draw: message `id` (DS:11EA table) with its box picture; the background is saved in DS:1E7C
 * (msg_close puts it back). id < 0: the box is removed at once after drawing (not restorable). */
void msg_draw(s16 id)
{
    FarPtr sv_front = { 0, 0 };
    u16 sv_present = 0;
    if (DSB(DS_racing) != 0) race_pages_on(&sv_front, &sv_present);
    cursor_ctl(-3);
    if (!far_is_null(ds_far(DS_msg_saved_bg))) {          /* a box is still open: restore under it */
        u16 box = DSW(DS_msg_box_rec);
        screen_put_bitmap(ds_far(DS_msg_saved_bg), DSS((u16)(box + BOX_X)), DSS((u16)(box + BOX_Y)));
        arena_pop(1);
        ds_far_wr(DS_msg_saved_bg, far_make(0, 0));
    }
    bool once = id < 0;
    if (once) id = (s16)-id;
    u16 box = (u16)(DS_MSG_BOXES + BOX_BYTES * MSG_BOX_TYPE(id));
    DSW(DS_msg_box_rec) = box;
    s16 h = DSS((u16)(box + BOX_H)), w = DSS((u16)(box + BOX_W));
    s16 y = DSS((u16)(box + BOX_Y)), x = DSS((u16)(box + BOX_X));
    ds_far_wr(DS_msg_saved_bg, screen_save_rect(x, y, w, h, far_make(0, 0)));
    pic_draw_masked(g_back(), DSS((u16)(box + BOX_PIC)), (u8)x, (u8)y, 0);
    memcpy(mp(DGROUP, DS_g_text_fg), mp(DGROUP, (u16)(DSW(DS_SCHEME) + 0x46)), 5);
    box = DSW(DS_msg_box_rec);
    s16 tx = (s16)(x + DSS((u16)(box + BOX_TDX)));
    s16 ty = (s16)(y + DSS((u16)(box + BOX_TDY)));
    for (s16 i = 0; i < DSS((u16)(box + BOX_LINES)); i++) {
        u16 line = MSG_LINE(id, i);
        drv_draw_text(g_back(), (s16)(tx + ((w - font_string_width(ds_str(line))) >> 1)), ty, ds_str(line));
        ty = (s16)(ty + 9);
        box = DSW(DS_msg_box_rec);
    }
    cursor_ctl(-1);
    if (DSW(DS_g_mirror) != 0) {
        box = DSW(DS_msg_box_rec);
        page_copy_rect(g_back(), g_front(), DSS((u16)(box + BOX_X)), DSS((u16)(box + BOX_Y)),
                       (s16)(DSS((u16)(box + BOX_X)) + w - 1), (s16)(DSS((u16)(box + BOX_Y)) + h - 1));
    }
    if (once) {
        arena_pop(1);
        ds_far_wr(DS_msg_saved_bg, far_make(0, 0));
    }
    DSW(DS_click_returns) = 1;                            /* ui_wait: any click ends a wait */
    if (DSB(DS_racing) != 0) race_pages_off(sv_front, sv_present);
}

/* 1a21:274e msg_draw_at: msg_draw with the box's x / y replaced for the call (a value < 0 keeps the table's). */
void msg_draw_at(s16 id, s16 x, s16 y)
{
    s16 a = (s16)(id < 0 ? -id : id);
    u16 box = (u16)(DS_MSG_BOXES + BOX_BYTES * MSG_BOX_TYPE(a));
    s16 ox = DSS((u16)(box + BOX_X)), oy = DSS((u16)(box + BOX_Y));
    if (x >= 0) DSS((u16)(box + BOX_X)) = x;
    if (y >= 0) DSS((u16)(box + BOX_Y)) = y;
    msg_draw(id);
    DSS((u16)(box + BOX_X)) = ox;
    DSS((u16)(box + BOX_Y)) = oy;
    DSW(DS_click_returns) = 0;
}

/* 1a21:27d2 msg_close: put the saved background back. */
void msg_close(void)
{
    FarPtr sv_front = { 0, 0 };
    u16 sv_present = 0;
    if (DSB(DS_racing) != 0) race_pages_on(&sv_front, &sv_present);
    if (!far_is_null(ds_far(DS_msg_saved_bg))) {
        u16 box = DSW(DS_msg_box_rec);
        screen_put_bitmap_mirror(ds_far(DS_msg_saved_bg), DSS((u16)(box + BOX_X)), DSS((u16)(box + BOX_Y)));
        arena_pop(1);
        ds_far_wr(DS_msg_saved_bg, far_make(0, 0));
    }
    DSW(DS_click_returns) = 0;
    if (DSB(DS_racing) != 0) race_pages_off(sv_front, sv_present);
}

/* 1a21:2888 present_back_page: the back page (rows 0 .. 0xBD) to the front page at once. */
void present_back_page(void)
{
    if (DSS(DS_driver_id) == -2) {
        ega_show_page(1);
        drv_pal_normal();
    }
    page_copy_rect(g_back(), g_front(), 0, 0, 0x13F, 0xBD);
    if (DSS(DS_driver_id) == -2) {
        ega_show_page(0);
        return;
    }
    drv_pal_normal();
}

/* 0001:3db4 msg_box: box, a "click anywhere" screen (6) and a wait of DS:1F4A ticks; the wait deadline is kept. */
void msg_box(s16 id)
{
    u32 saved = DSL(DS_wait_until);
    msg_draw(id);
    ui_push(6);
    wait_ticks(DSS(DS_msg_ticks));
    msg_close();
    ui_pop(1);
    DSL(DS_wait_until) = saved;
}

/* 0001:4d90 message_at: the box at (x, y), the same wait, close. */
void message_at(s16 id, s16 x, s16 y)
{
    u32 saved = DSL(DS_wait_until);
    msg_draw_at(id, x, y);
    wait_ticks(DSS(DS_msg_ticks));
    msg_close();
    DSL(DS_wait_until) = saved;
}

/* 1a21:1e50 pay_or_msg: the price (sign-extended word) against the bankroll. */
s16 pay_or_msg(u16 price, s16 msg)
{
    if ((s32)(s16)price > DSSL(DS_money)) {
        msg_box(msg);
        return 0;
    }
    money_add((s16)-(s16)price);
    return 1;
}

/* Registers the hook the platform / video code uses for its message boxes. */
void ui_register_hooks(void)
{
    modules.fatal_message = fatal_message;
}
