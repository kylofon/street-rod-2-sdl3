/* "Car info" (S5.2 / W2b): car_info_sr2 0001:72f2, the hand-drawn parts panel. From port/spec/garage.md, checked
 * against the disassembly (the any-key flag is DS:1F3E, the spec says 1F3D). */
#include "game/ds_screens.h"

#include <stdio.h>
#include <string.h>

/* 0001:72f2 car_info_sr2 */
void car_info_sr2(void)
{
    u16 car = gs_cur_car();
    u16 parts[10];
    FarPtr back = g_back();
    s16 saved;

    if (car == 0) { msg_box(9); return; }           /* no car, dummy (present flag untouched) */
    saved = (s16)DSW(DS_g_mirror);
    DSW(DS_g_mirror) = 0;
    cursor_ctl(-3);
    FarPtr bg = screen_save_rect(0x1D, 0x16, 0x106, 0x92, (FarPtr){ 0, 0 });    /* panel background (arena) */
    /* colours = the second scheme entry (fg word, bg word, frame byte) */
    memcpy(mp(DGROUP, DS_g_text_fg), mp(DGROUP, (u16)(DSW(DS_SCHEME) + 5)), 5);
    fill_rect(back, 0x20, 0x19, 0x100, 0x8C);
    draw_frame3(back, 0x20, 0x19, 0x100, 0x8C);
    {
        const char *title = ds_str(DSW(DS_GS_CARINFO_TITLE));
        drv_draw_text(back, (s16)(((0x100 - font_string_width(title)) >> 1) + 0x20), 0x19, title);
    }
    hline(back, 0x20, 0x22, 0x100);

    for (s16 i = 0; i < 5; i++) parts[i] = DSW((u16)(car + CR_ENGINE + 2 * i));   /* engine, manifold, carb 0..2 */
    parts[5] = DSW((u16)(car + CR_TRANS));
    parts[6] = DSW((u16)(car + CR_TYRES));
    parts[7] = DSW((u16)(car + CR_EXHAUST));
    parts[8] = DSW((u16)(car + CR_MUFFLER));
    parts[9] = DSW((u16)(car + CR_DIFF));

    s16 y = 0x24;
    for (s16 i = 0; i < 10; i++) {
        u16 p = parts[i];
        if (p == 0) continue;
        s16 w = DSS((u16)(p + PT_WEAR));
        list_row((s16)(DSS((u16)(DS_CARINFO_INDENT + 2 * i)) + 0x20), y, parts_text(DSS((u16)(p + PT_TYPE))),
                 0x110, w != -0x80, (s16)(w / 100));
        y = (s16)(y + 10);
    }

    s16 v = car_runnable(car, 1) != 0 ? car_max_speed(-1) : 0;
    sprintf(ds_str((u16)(DSW(DS_GS_CARINFO_SPEED) + 9)), ds_str(DS_GS_FMT_MPH), (int)v);                 /* " %3d mph" */
    sprintf(ds_str((u16)(DSW(DS_GS_CARINFO_FUEL) + 5)), ds_str(DS_GS_FMT_GAL), (int)(DSS((u16)(car + CR_GAS)) / 10));   /* " %2d gallons" */
    drv_draw_text(back, 0x20, (s16)(y + 10), ds_str(DSW(DS_GS_CARINFO_FUEL)));
    drv_draw_text(back, 0x20, (s16)(y + 20), ds_str(DSW(DS_GS_CARINFO_SPEED)));
    cursor_ctl(-1);
    page_copy_rect(back, g_front(), 0x1D, 0x16, 0x123, 0xA8);

    DSW(DS_g_mirror) = (u16)saved;
    DSW(DS_wait_any_key) = 1;
    ui_menu(6);                                     /* any click or key closes it */
    DSW(DS_wait_any_key) = 0;
    screen_put_bitmap_mirror(bg, 0x1D, 0x16);
    arena_pop(1);
}
