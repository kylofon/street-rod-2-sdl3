/* "Under the Car" (S5.2 / W2b): under_car_menu 2925:3174, garage code 14 (exhaust manifold, muffler, differential).
 * From port/spec/garage.md, checked against the disassembly. The drawing helpers are in game/garage_draw.h. */
#include "game/ds_screens.h"

/* 2925:3174 under_car_menu: returns 0 without a car, 2 on Done (game_loop then redraws the garage with mode 2). */
s16 under_car_menu(void)
{
    u16 node[192];
    s8 wear[192];
    s16 n;                                          /* [bp-0xa] */
    s16 n0;                                         /* [bp-2]: reserved leading entries; doubles as the selection (sic) */
    s16 cat = 0;                                    /* si: part category 5 exhaust manifold, 6 muffler, 7 differential */
    s16 none_msg = 0;                               /* [bp-8] */
    u16 slot = 0;                                   /* di: DS offset of the car's part pointer */
    u16 car = gs_cur_car();

    if (car == 0) { msg_box(9); return 0; }         /* no car, dummy */
    under_car_slide_in();
    under_car_draw();

    for (;;) {
        n0 = 0;
        s16 b = ui_dialog(34);                      /* -2 Exhaust manifold, -3 Muffler, -4 Differential, -1 Done */
        if (b == -1) {
            arena_pop(1);
            return 2;
        } else if (b == -4) {
            cat = 7; none_msg = 118; slot = (u16)(car + CR_DIFF);
        } else if (b == -3) {
            cat = 6; none_msg = 117; slot = (u16)(car + CR_MUFFLER);
            if (DSW(slot) != 0) {                   /* a fitted muffler can be taken off */
                DSW(DS_list_entries) = DS_GS_REMOVE_MUFFLER;
                wear[0] = (s8)0x80;
                n0 = 1;
            }
        } else if (b == -2) {
            cat = 5; none_msg = 116; slot = (u16)(car + CR_EXHAUST);
        }
        /* PORT: any other code keeps the previous category, as the original does (it never happens: screen 34 has
         * only those four buttons). */
        spares_collect(cat, node, wear, n0, &n);
        s16 r = gs_list_box(7, 0, wear, n);         /* -3 Change, -4 Forget it */
        if (r == -4) continue;                      /* back to the buttons */
        if (r == 0x3EA) { msg_box(none_msg); continue; }   /* no spare exhaust manifolds / mufflers / differentials */
        s16 sel = n0;                               /* any other code uses the leading-entry count (sic) */
        if (r == -3) sel = list_selected();

        if (cat == 6 && DSW(slot) != 0 && sel == 0) {       /* "Remove current muffler" */
            under_car_part_pic(6, -1);
            part_uninstall(slot);
            continue;
        }
        u16 part = node[sel];
        s16 t = DSS((u16)(part + PT_TYPE));
        if (gs_part_fits(t, gs_car_model(car)) == 0) { msg_box(24); continue; }    /* That won't fit! */
        under_car_part_pic(cat, t);
        part_install(spare_unlink(part), slot);
    }
}
