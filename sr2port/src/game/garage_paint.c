/* Paint job and customising (S5.2 / W2b): set_paint_palette 0001:672c, paint_job 6d5e, customize 6edc.
 * From port/spec/garage.md, checked against the disassembly. SR1 base: srport garage_main.c. */
#include "game/ds_screens.h"

/* 0001:672c set_paint_palette: registers 6 / 7 of the EGA palette = PAINT[colour] (DS:4D1E, 3 bytes per colour:
 * reg6, reg7, "See it" strip background), plus the shadow copies of the palette table. Only on EGA/VGA drivers. */
void set_paint_palette(s16 colour)
{
    if (DSS(DS_driver_id) == -2 || DSS(DS_driver_id) == -6) {
        u16 p = (u16)(DS_PAINT + 3 * colour);
        if (DSB(DS_g_pal_visible) != 0) {
            ega_set_palreg(6, DSB(p));
            ega_set_palreg(7, DSB((u16)(p + 1)));
        }
        DSB(DS_GS_PAL_SHADOW) = DSB(p);
        DSB((u16)(DS_GS_PAL_SHADOW + 1)) = DSB((u16)(p + 1));
        DSB(DS_GS_PAL_SHADOW2) = DSB(p);
        DSB((u16)(DS_GS_PAL_SHADOW2 + 1)) = DSB((u16)(p + 1));
    }
}

/* 0001:6d5e paint_job: screen 13 (1 Previous colour, 2 Next colour, 3 Go ahead, -23 Forget it). Palette entry 12
 * holds the sample colour while the dialog is up. */
void paint_job(void)
{
    if (gs_cur_car() == 0) { msg_box(9); return; }                 /* no car, dummy */
    if (DSS(DS_driver_id) != -2 && DSS(DS_driver_id) != -6) { msg_box(97); return; }   /* need a better graphics card */

    s16 x, y;
    FarPtr bg = ui_save_bg(13, &x, &y, 2);
    ui_push(13);
    s16 i = DSC((u16)(gs_cur_car() + CR_COLOUR));
    s16 b;
    for (;;) {
        ega_set_palreg(12, DSB((u16)(DS_PAINT + 3 * i)));
        do b = ui_wait(3000); while (b == 0);
        if (b == 1) { if (--i < 0) i = 5; continue; }
        if (b == 2) { if (++i > 5) i = 0; continue; }
        break;
    }
    if (b == 3 && pay_or_msg(20, 25)) {                            /* Paint ain't free! */
        sound_vec(2, 1);                                           /* start the spray effect */
        DSW(DS_g_mirror) = 1;
        ui_pop(1);
        screen_put_bitmap_mirror(bg, x, y);
        arena_pop(1);
        DSB((u16)(gs_cur_car() + CR_COLOUR)) = (u8)i;
        set_paint_palette(DSC((u16)(gs_cur_car() + CR_COLOUR)));
        gs_clock_add(0x888);
        sound_vec(2, 0);
        return;
    }
    ui_pop(1);
    ega_set_palreg(12, DSB((u16)(DS_PAINT + 3 * DSC((u16)(gs_cur_car() + CR_COLOUR)))));   /* restore the sample colour */
    screen_put_bitmap_mirror(bg, x, y);
    arena_pop(1);
}

/* 0001:6edc customize: 4 roof, 2 rear bumper, 1 front bumper. The hot spots are disabled without a car. */
void customize(s16 what)
{
    s16 cost = 0, label = 0;
    u16 hours = 0;
    u16 car = gs_cur_car();
    u16 ov = (u16)(DS_MODEL_OVERLAYS + 10 * gs_car_model(car));   /* {race, b1, roof, scoop, rear, front} */
    u16 dlg = ui_screen(15);                                       /* the dialog's first element carries the title */

    if (what & 4) {
        if (DSS((u16)(ov + 2)) == 0) { msg_box(31); return; }      /* can't chop this roof */
        label = (DSB((u16)(car + CR_FLAGS + 1)) & 0x20) ? 1 : 0;
        DSW((u16)(dlg + UIEL_TEXT)) = DSW((u16)(DS_GS_CUST_TEXT + 2 * label));
        cost = 70; hours = 0x2220;
    }
    if (what & 2) {
        if (DSS((u16)(ov + 6)) == 0) { msg_box(32); return; }
        label = (DSB((u16)(car + CR_FLAGS + 1)) & 0x40) ? 3 : 2;
        DSW((u16)(dlg + UIEL_TEXT)) = DSW((u16)(DS_GS_CUST_TEXT + 2 * label));
        cost = 15; hours = 0x888;
    }
    if (what & 1) {
        if (DSS((u16)(ov + 8)) == 0) { msg_box(32); return; }
        label = (DSB((u16)(car + CR_FLAGS + 1)) & 0x80) ? 3 : 2;
        DSW((u16)(dlg + UIEL_TEXT)) = DSW((u16)(DS_GS_CUST_TEXT + 2 * label));
        cost = 15; hours = 0x888;
    }

    s16 x, y;
    FarPtr bg = ui_save_bg(15, &x, &y, 2);
    ui_push(15);                                                   /* -20 OK, -22 Forget it */
    s16 b;
    do b = ui_wait(3000); while (b == 0);
    ui_pop(1);
    screen_put_bitmap_mirror(bg, x, y);
    arena_pop(1);
    if (b == -20 && pay_or_msg((u16)cost, 33)) {                   /* Customization ain't free! */
        car = gs_cur_car();
        if (what & 4) DSB((u16)(car + CR_FLAGS + 1)) ^= 0x20;
        if (what & 2) DSB((u16)(car + CR_FLAGS + 1)) ^= 0x40;
        if (what & 1) DSB((u16)(car + CR_FLAGS + 1)) ^= 0x80;
        cursor_ctl(-4);
        car_draw(8, gs_car_model(car), (s16)car_flags(car), 0, 0x9E, gs_tyre_pics(gs_tyre_grade(car)));
        sound_vec(9);                                              /* the clank */
        cursor_ctl(-2);
        gs_clock_add(hours);
    }
}
