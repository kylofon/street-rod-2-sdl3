/* The garage screen (S5.2 / W2b): hot spots 0001:6796 / 67b8, garage_name_clear 68c0, garage_screen 6912.
 * From port/spec/garage.md, checked against the disassembly (work/SR2_unp.exe). SR1 base: srport garage_main.c. */
#include "game/ds_screens.h"

#include <stdio.h>
#include <string.h>

/* 0001:6796 garage_hotspots_nocar: no car, so the five car hot spots (codes 9..13) are disabled. */
void garage_hotspots_nocar(void)
{
    u16 e = ui_screen(0);
    for (;;) {
        s16 code = DSS((u16)(e + UIEL_CODE));
        if (code >= 9 && code <= 13) DSB((u16)(e + UIEL_FLAGS)) &= 0xFE;
        if (DSB((u16)(e + UIEL_FLAGS)) & UIF_LAST) break;
        e = (u16)(e + UIEL_BYTES);
    }
}

/* 0001:67b8 garage_hotspots_car: the five car hot spots from MODEL_HOTSPOTS (DS:9872, 20 bytes per model:
 * five {dx, dy, w, h} byte rectangles). code 11 rect 0 rear bumper, 13 rect 1 roof, 9 rect 2 transmission,
 * 10 rect 3 hood, 12 rect 4 front bumper. */
void garage_hotspots_car(s16 model, s16 x, s16 y)
{
    u16 m = (u16)(DS_MODEL_HOTSPOTS + 0x14 * model);
    u16 e = ui_screen(0);
    for (;;) {
        s16 k = -1;
        switch (DSS((u16)(e + UIEL_CODE))) {
        case 11: k = 0; break;
        case 13: k = 1; break;
        case 9:  k = 2; break;
        case 10: k = 3; break;
        case 12: k = 4; break;
        default: break;
        }
        if (k >= 0) {
            u16 r = (u16)(m + 4 * k);
            DSW((u16)(e + UIEL_X0)) = (u16)(DSB(r) + x);
            DSW((u16)(e + UIEL_Y0)) = (u16)(DSB((u16)(r + 1)) + y);
            DSW((u16)(e + UIEL_X1)) = (u16)(DSB((u16)(r + 2)) + DSW((u16)(e + UIEL_X0)));
            DSW((u16)(e + UIEL_Y1)) = (u16)(DSB((u16)(r + 3)) + DSW((u16)(e + UIEL_Y0)));
            DSB((u16)(e + UIEL_FLAGS)) |= UIF_ACTIVE;
        }
        if (DSB((u16)(e + UIEL_FLAGS)) & UIF_LAST) break;
        e = (u16)(e + UIEL_BYTES);
    }
}

/* 0001:68c0 garage_name_clear: the title field shows DS:220A; blank it. */
void garage_name_clear(void)
{
    strcpy(ds_str(DS_garage_title), ds_str(DS_blank_title));
}

/* 0001:6912 garage_screen */
s16 garage_screen(s16 mode)
{
    s16 code = 0;                                  /* [bp-0xa] */
    s16 f;                                         /* [bp-0xc]: car_flags */
    bool redraw = false;

    if (DSC(DS_cursor_visible) > 0) cursor_ctl(-4);
    summer_over_check();
    if (DSW(DS_game_over) != 0) {
        if (DSW(DS_game_over) & 1) cursor_ctl(-2);
        return FLOW_GAME_OVER;
    }
    cursor_ctl(0);
    status_print(-1, ds_str(DS_GS_EMPTY), 0);      /* clears the status line */
    DSS(DS_location) = 1;                          /* trip destination "garage" */

    if (mode == 1 || mode == 2) {
        redraw = true;
    } else if (mode == 3) {                        /* "Switch it" */
        if (DSW(DS_switch_new) != 0) {
            u16 old = DSW(DS_switch_old);
            if (old != 0) {                        /* the old car drives out */
                u16 save = DSW(DS_cur_car);
                f = (s16)car_flags(old);
                DSW(DS_cur_car) = old;             /* car_draw reads cur_car */
                car_draw(6, gs_car_model(old), (s16)f, 0, 0x9E, gs_tyre_pics(gs_tyre_grade(old)));
                DSW(DS_cur_car) = save;
                garage_name_clear();
                ui_push(0);
                ui_pop(1);
            }
            redraw = true;
        } else {
            cursor_ctl(-2);
            msg_box(17);                           /* Here it is! */
            cursor_ctl(-4);
        }
    }

    if (redraw) {
        DSW(DS_g_mirror) = 0;
        if (mode != 3) {                           /* background: LIB2 #18 (0x3FA) + two overlays */
            FarPtr p = pic_get(0x3FA, 2);
            pic_draw_masked(p, 0x3FC, 0x90, 0xA3, 0);
            pic_draw_masked(p, 0x4AE, 0x50, 0xAC, 0);
            screen_put_bitmap_mirror(p, 0, 0);
            arena_pop(1);
            pal_load(DS_GS_PAL_GARAGE);
            ega_set_palreg(12, DSW(DS_cur_car) != 0
                                   ? DSB((u16)(DS_PAINT + 3 * DSC((u16)(DSW(DS_cur_car) + CR_COLOUR)))) : 5);
        }
        DSW(DS_g_mirror) = 1;
        u16 car = DSW(DS_cur_car);
        if (car == 0) {
            if (mode != 3) present_back_page();    /* 1a21:2888: back page -> front page */
            DSB(DS_garage_title) = 0;
        } else {
            DSS(DS_garage_car_pic) = model_pic(gs_car_model(car));
            DSS(DS_garage_car_colour) = DSC((u16)(car + CR_COLOUR));
            DSS(DS_garage_car_tyre) = gs_tyre_grade(car);
            set_paint_palette(DSS(DS_garage_car_colour));
            DSS(DS_garage_car_driver) = 0;
            DSS(DS_garage_car_model) = gs_car_model(car);
            /* only the roof / bumper bits are copied now (no sticker field) */
            DSW(DS_garage_car_flags) = (u16)((DSW((u16)(car + CR_FLAGS)) & 0xE000) | (DSW(DS_garage_car_flags) & 0x1FFF));
            for (s16 i = 0; i < 5; i++)            /* engine, manifold, carb 0..2; NB an empty slot reads DS:0004 */
                DSS((u16)(DS_bay_slot_pic + 2 * i)) = DSS((u16)(DSW((u16)(car + CR_ENGINE + 2 * i)) + PT_TYPE));
            f = (s16)car_flags(car);
            strcpy(ds_str(DS_garage_title), ds_str(model_ad_ptr(DSS(DS_garage_car_model))));
            s16 m;
            if (mode == 2) {
                m = 3;                             /* parked */
            } else {
                if (mode != 3) present_back_page();
                m = 4;                             /* drives in */
            }
            car_draw(m, DSS(DS_garage_car_model), (s16)f, 0, 0x9E, gs_tyre_pics(DSS(DS_garage_car_tyre)));
        }
    }

    /* 69bc: hot spots */
    if (DSW(DS_cur_car) == 0) garage_hotspots_nocar();
    else garage_hotspots_car(DSS(DS_garage_car_model), DSS(DS_car_x), DSS(DS_car_y));
    ui_sort_screen(0);                             /* re-sort the garage hot spots by position */
    status_label(ds_str(DSW(DS_GS_MONEY_FMT)));
    cursor_ctl(-2);

    if (DSS(DS_car_count) > 15) {
        msg_box(18);                               /* no room for all these cars */
    } else if (DSS(DS_part_count) > PART_LIMIT - 1) {
        if (DSS(DS_part_count) == PART_LIMIT) {
            msg_box(19);                           /* too much junk */
        } else {                                   /* message 20, line 2: "Must sell 12 parts ..." */
            char *q = ds_str(DSW(DS_GS_MSG20_LINE2));
            q += strcspn(q, ds_str(DS_GS_DIGITS));
            sprintf(q, "%d", (int)(s16)(DSS(DS_part_count) - (PART_LIMIT - 1)));   /* itoa(parts - 0xB4, q, 10) */
            q[strlen(q)] = ' ';
            msg_box(20);
        }
    }
    if (broke_check() != 0) {
        msg_box(21);                               /* You're outta dough */
        DSW(DS_game_over) |= 0x10;
    } else {
        input_reset();
        code = ui_menu(0);                         /* screen 0 = the garage hot spots */
    }
    summer_over_check();
    if (DSW(DS_game_over) & 0x10) DSW(DS_game_over) = 0x10;
    if (DSW(DS_game_over) != 0) {
        code = FLOW_GAME_OVER;
        if ((DSW(DS_game_over) & 1) == 0) cursor_ctl(-4);
    }
    return code;
}
