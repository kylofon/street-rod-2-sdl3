/* Garage: selling (garage.md 0001:3f5a sell_spare_parts, 478e how_about, 4852 your_cars; message_at 4d90 is the UI slice's, ui_msg.c). The
 * classified ads and the newspaper are in garage_classifieds.c. The SR1 code used the x87 emulator for the
 * haggling; the SR2 exe does it in integer arithmetic (verified in the disassembly), reproduced here with the
 * MSC long helpers = plain s32 operators. */
#include "game/garage.h"

#include <stdio.h>
#include <string.h>

#include "game/ds_shop.h"
#include "game/flow.h"
#include "game/garage_draw.h"
#include "game/garage_screens.h"
#include "game/ui.h"
#include "platform/platform.h"
#include "platform/video.h"

static inline void clock_add(u32 v) { DSL(DS_game_clock) += v; }

/* The element of screen `scr` (DS_SCREENS[scr]) whose code is `code`. */
static u16 screen_element(s16 scr, s16 code)
{
    u16 e = DSW((u16)(DS_SCREENS + 2 * scr));
    while (DSS((u16)(e + 6)) != code) e = (u16)(e + 0x12);
    return e;
}

/* ============================================================================== 0001:3f5a */

void sell_spare_parts(void)
{
    u16 node[200];                                  /* the spare nodes in list order (190-node pool) */
    s8 wear[200];
    s16 rounds = 0;
    s16 sel = 0;
    for (;;) {
        s16 n = 0;
        for (u16 p = DSW(DS_spare_parts); p != 0; p = DSW((u16)(p + PT_NEXT))) {
            node[n] = p;
            s16 t = DSS((u16)(p + PT_TYPE));
            DSW((u16)(DS_list_entries + 2 * n)) = parts_text(t);
            s16 w = DSS((u16)(p + PT_WEAR));
            wear[n] = w == -0x80 ? (s8)0x80 : (s8)(w / 100);
            n++;
        }
        u16 e = screen_element(7, -3);              /* the "Change" button is "Sell it" here */
        DSW((u16)(e + 2)) = DS_TXT_SELL_IT;
        s16 r = list_box(7, sel < n ? sel : (s16)(n - 1), (const u16 *)mp(DGROUP, DS_list_entries), wear, n);
        sel = r;
        DSW((u16)(e + 2)) = DS_TXT_CHANGE;
        if (r == -4) return;                        /* Forget it */
        if (r == -3) sel = DSS(DS_ui_sel);          /* Sell it */
        else if (r == 0x3EA) {
            msg_box(rounds == 0 ? 5 : 4);           /* no spare parts, dummy / All parts gone */
            return;
        }
        u16 p = node[sel];
        s32 v = DSS((u16)(p + PT_VALUE));
        s16 t = DSS((u16)(p + PT_TYPE));
        s32 q = (s16)((s16)(10000 - DSS((u16)(p + PT_WEAR))) / 100);      /* remaining life in % (101: does not wear) */
        s32 price = 0;
        switch (parts_category(t)) {
        case 0: case 1:                                                    /* engine, transmission */
            price = v * q * 9 / 1000;
            break;
        case 2: case 3:                                                    /* carburettor, manifold */
            price = v * (s32)(rnd(20) + 70) / 100;
            break;
        case 4:
            price = 0;
            break;
        case 5: case 6: case 7:                                            /* exhaust manifold, muffler, differential */
            price = v * 7 / 10;
            break;
        default:
            break;
        }
        price = price / 5 * 5;
        s16 id;
        if (price != 0) {
            id = 6;
            sprintf(ds_str(shop_msg_line(6)), ds_str(DS_FMT_TAKE), (s16)price);     /* "Will you take $%-d ?" */
        } else {
            id = 7;                                                        /* I'll take it off your hands for nothing */
        }
        msg_draw_at(id, -1, -1);
        s16 b = ui_dialog(8);
        msg_close();
        if (b != -21) {                                                    /* -21 = No thanks */
            if (sel == 0) DSW(DS_spare_parts) = DSW((u16)(p + PT_NEXT));
            else DSW((u16)(node[sel - 1] + PT_NEXT)) = DSW((u16)(p + PT_NEXT));
            part_free(p);
            clock_add(0x444);
            money_add((s16)price);
        }
        rounds++;
    }
}

/* ============================================================================== 0001:478e */

s16 how_about(u16 *offer)
{
    u16 r = (u16)(*offer % 5);
    if (r != 0) {
        *offer = (u16)(*offer - r);
        if (*offer == 0) *offer = 5;
    }
    char *p = strchr(ds_str(shop_msg_line(14)), '$');
    sprintf(p + 1, ds_str(DS_FMT_HOWABOUT), (s16)*offer);                  /* "How about $1234 ?" */
    msg_draw_at(14, -1, -1);
    s16 b = ui_menu(11);                            /* -20 OK, -21 No */
    msg_close();
    return b;
}

/* ============================================================================== 0001:4852 */

/* ASK of the spec: edit_number until a non-zero asking price; false = "Never mind" (-22). */
static bool ask_price(u16 *ask)
{
    do {
        if (edit_number(ask, ds_str(DS_SELL_EDIT)) == -22) return false;
    } while (*ask == 0);
    return true;
}

s16 your_cars(void)
{
    u16 node[20];                                   /* node[1] = cur_car, then the other cars; node[k] precedes node[k+1] in the list */
    FarPtr saved = { 0, 0 };
    s16 rounds = 0;
    s16 sel = 0;
    s16 sx = 0, sy = 0;
    u16 ask = 0, offer = 0, base = 0;
    for (;;) {
        s16 n = 0;
        u16 cur = DSW(DS_cur_car);
        if (cur != 0) {
            node[1] = cur;
            DSW(DS_list_entries) = model_ad_ptr(DSS((u16)(cur + CR_MODEL)));
            n = 1;
        }
        for (u16 c = DSW(DS_car_list); c != 0; c = DSW((u16)(c + CR_NEXT))) {
            node[n + 1] = c;
            DSW((u16)(DS_list_entries + 2 * n)) = model_ad_ptr(DSS((u16)(c + CR_MODEL)));
            n++;
        }
        s16 r = list_box(9, sel < (s16)(n - 1) ? sel : (s16)(n - 1), (const u16 *)mp(DGROUP, DS_list_entries), NULL, n);
        sel = r;
        if (r == -3) return 0;                      /* Forget it */
        if (r == 0x3EA) {
            msg_box(rounds == 0 ? 9 : 8);           /* no car, dummy / All cars gone */
            return 0;
        }
        sel = DSS(DS_ui_sel);
        if (r == -1) {                              /* Switch it */
            DSW(DS_switch_new) = node[sel + 1];
            DSW(DS_switch_old) = DSW(DS_cur_car);
            u16 nw = DSW(DS_switch_new);
            if (nw == DSW(DS_cur_car)) {
                DSW(DS_switch_new) = 0;
                return 3;
            }
            if ((DSW(DS_cur_car) != 0) == sel) DSW(DS_car_list) = DSW((u16)(nw + CR_NEXT));
            else DSW((u16)(node[sel] + CR_NEXT)) = DSW((u16)(nw + CR_NEXT));
            if (DSW(DS_cur_car) != 0) {
                DSW((u16)(DSW(DS_cur_car) + CR_NEXT)) = DSW(DS_car_list);
                DSW(DS_car_list) = DSW(DS_cur_car);
            }
            DSW(DS_cur_car) = nw;
            return 3;
        }
        /* Sell it (-2, or any other code) */
        u16 car = node[sel + 1];
        s16 value = DSS(car);
        base = (u16)((s32)value * (rnd(20) + 75) / 100);
        offer = base;
        strcpy(ds_str(DS_SELL_NAME), ds_str(model_ad_ptr(DSS((u16)(car + CR_MODEL)))));
        DSB(DS_SELL_EDIT) = 0;
        saved = ui_save_bg(10, &sx, &sy, 1);
        ui_push(10);
        do {
            if (!ask_price(&ask)) break;
            if ((u16)((s32)value * 115 / 100) < ask) { msg_box(10); break; }       /* You must be kidding! */
            if ((u16)value < ask) {
                msg_box(11);                                                       /* No way! */
                if (!ask_price(&ask)) break;
                if ((u16)value < ask) { msg_box(12); break; }                      /* Get lost! */
            }
            if (offer < ask) {
                offer = (u16)((s32)base * (rnd(5) + 85) / 100);
                if (offer < ask && how_about(&offer) == -20) ask = offer;
            }
            if (offer < ask && !ask_price(&ask)) break;
            if (offer < ask) {
                offer = (u16)((s32)base * (rnd(5) + 91) / 100);
                if (offer < ask && how_about(&offer) == -20) ask = offer;
            }
            if (offer < ask && !ask_price(&ask)) break;
            if ((u16)((u16)(ask * 9) / 10) > offer) { msg_box(13); break; }        /* No thanks! */
            msg_box((u16)(ask << 1) < (u16)value ? 15 : 16);                        /* You must be crazy... / I'll take it! */
            clock_add(0x888);
            money_add((s16)ask);
            if (sel == 0 && DSW(DS_cur_car) != 0) {                                  /* the car in the garage */
                screen_put_bitmap_mirror(saved, sx, sy);
                arena_pop_low(1);
                saved.off = saved.seg = 0;
                cursor_ctl(-4);
                u16 c = DSW(DS_cur_car);
                car_draw(6, DSS((u16)(c + CR_MODEL)), car_flags(c), 0, 0x9E, tyre_pics_ofs(car_tyre_grade(c)));
                ui_pop(1);
                garage_name_clear();
                ui_push(0);
                cursor_ctl(-2);
                car_free(DSW(DS_cur_car));
                DSW(DS_cur_car) = 0;
            } else {
                if ((DSW(DS_cur_car) != 0) == sel) DSW(DS_car_list) = DSW((u16)(car + CR_NEXT));
                else DSW((u16)(node[sel] + CR_NEXT)) = DSW((u16)(car + CR_NEXT));
                car_free(car);
            }
        } while (0);
        ui_pop(1);
        if (!far_is_null(saved)) {
            screen_put_bitmap_mirror(saved, sx, sy);
            arena_pop_low(1);
            saved.off = saved.seg = 0;
        }
        rounds++;
    }
}
