/* Tyre changing (S5.2 / W2b): change_tires 0001:71f6. From port/spec/garage.md, checked against the disassembly. */
#include "game/ds_screens.h"

/* 0001:71f6 change_tires: screen 7 lists the spare tyres (-3 Change, -4 Forget it). */
void change_tires(void)
{
    u16 node[192];
    s8 wear[192];
    s16 n;                                          /* [bp-4]: the count, which doubles as the selection (sic) */
    u16 car = gs_cur_car();

    if (car == 0) { msg_box(9); return; }           /* no car, dummy */
    spares_collect(4, node, wear, 0, &n);           /* category 4 = tyres */
    s16 r = gs_list_box(7, 0, wear, n);
    if (r == -4) return;                            /* Forget it */
    if (r == -3) n = list_selected();               /* 0-based selection */
    else if (r == 0x3EA) { msg_box(30); return; }   /* no spare tyres */
    /* PORT: any other code leaves the count in the selection variable, as the original does. */
    s16 old_g = gs_tyre_grade(car);
    s16 new_g = gs_part_grade(node[n]);
    part_install(spare_unlink(node[n]), (u16)(car + CR_TYRES));
    tire_change_anim(gs_tyre_pics(old_g), gs_tyre_pics(new_g));    /* TYRE_PICS = DS:4D30, 8 bytes per grade */
    gs_clock_add(0x444);
}
