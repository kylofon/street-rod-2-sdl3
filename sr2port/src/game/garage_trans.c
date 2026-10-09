/* The transmission screen (S5.2 / W2b): change_transmission 0001:75b8. From port/spec/garage.md, checked against the
 * disassembly. Drawing helpers (trans_*) are in game/garage_draw.h. SR1 base: srport garage_bay.c. */
#include "game/ds_screens.h"

/* 0001:75b8 change_transmission. Screen 12: -2 Parts, -1 Done, bolt hot spots 1 and 2 (elements 2 and 3, enabled
 * while a transmission is fitted). */
void change_transmission(void)
{
    u16 node[192];
    s8 wear[192];
    s16 n;
    s16 st[3];                                      /* [1], [2]: 0 out, 1 tightening, 2 loosening */
    u16 car = gs_cur_car();

    if (car == 0) { msg_box(9); return; }           /* no car, dummy */
    for (s16 i = 0; i < 2; i++) {
        s8 v = DSC((u16)(car + CR_TRANS_BOLT + i));
        st[i + 1] = v == 0 ? 0 : (v == 1 ? 1 : 2);
    }
    u16 hs1 = (u16)(ui_screen(12) + 2 * UIEL_BYTES);                /* [bp-2]: element 2 (bolt 1) */
    u16 hs2 = (u16)(hs1 + UIEL_BYTES);                              /* [bp-4]: element 3 (bolt 2) */
    u16 tr = DSW((u16)(car + CR_TRANS));
    if (tr != 0) {
        DSW(DS_trans_pic) = DSW((u16)(DS_TRANS_PICS + 2 * gs_part_grade(tr)));
        DSB(hs1) |= UIF_ACTIVE;
        DSB(hs2) |= UIF_ACTIVE;
    } else {
        DSW(DS_trans_pic) = 0;
        DSB(hs1) &= 0xFE;
        DSB(hs2) &= 0xFE;
    }
    DSW(DS_g_mirror) = DSS(DS_trans_pic) == 0 ? 1 : 0;
    show_picture_at(0x41F, 0x20, 10, -1);           /* the panel (LIB2 #1055) */
    DSW(DS_g_mirror) = 1;
    trans_screen_draw(DSS(DS_trans_pic), (u16)(car + CR_TRANS_BOLT));
    ui_push(12);

    for (;;) {
        s16 b;
        do b = ui_wait(3000); while (b == 0);
        if (b == -1) break;                         /* Done */

        if (b == -2) {                              /* Parts */
            spares_collect(1, node, wear, 0, &n);   /* category 1 = transmissions */
            for (;;) {
                s16 r = gs_list_box(7, 0, wear, n);
                s16 sel = r;                        /* PORT: as the original, any other code is used as the index */
                if (r == -4) {
                    sel = -1;
                } else if (r == -3) {
                    if (DSW((u16)(car + CR_TRANS)) != 0) { msg_box(22); sel = -1; }   /* Where will you put it, speedy? */
                    else sel = list_selected();
                } else if (r == 0x3EA) {
                    msg_box(23);                    /* no spare transmissions */
                    sel = -1;
                }
                if (sel < 0) break;

                s16 old = DSS(DS_trans_pic);
                s16 t = DSS((u16)(node[sel] + PT_TYPE));
                DSW(DS_trans_pic) = DSW((u16)(DS_TRANS_PICS + 2 * parts_grade(t)));
                if (gs_part_fits(t, gs_car_model(car)) != 0) {
                    part_install(spare_unlink(node[sel]), (u16)(car + CR_TRANS));
                    cursor_ctl(-4);
                    sound_vec(10, 1);
                    trans_slide_anim(DSS(DS_trans_pic), 0);
                    sound_vec(10, 0);
                    sound_vec(14);
                    gs_clock_add(0x666);
                    for (s16 i = 1; i >= 0; i--) {  /* two loose bolts, second one first */
                        st[i + 1] = 1;
                        sound_vec(5, 1);
                        DSB((u16)(car + CR_TRANS_BOLT + i)) = 1;
                        trans_bolt_draw(1, i);
                        sound_vec(5, 0);
                        sound_vec(13);
                    }
                    DSB(hs1) |= UIF_ACTIVE;
                    DSB(hs2) |= UIF_ACTIVE;
                    click_clear();
                    kbd_hotspot_refresh();          /* 0001:1c6c */
                    cursor_ctl(-2);
                    break;
                }
                trans_slide_anim(DSS(DS_trans_pic), 1);
                msg_box(24);                        /* That won't fit! */
                trans_remove_anim();
                gs_clock_add(0x888);
                DSW(DS_trans_pic) = (u16)old;
            }
            continue;
        }

        if (DSW(DS_trans_pic) == 0) continue;       /* no transmission: the bolts do nothing */
        /* PORT: the original does not range-check the code; only 1 and 2 exist on screen 12. */
        if (b != 1 && b != 2) continue;
        s16 i = (s16)(b - 1);
        u16 bolt = (u16)(car + CR_TRANS_BOLT + i);
        if (st[b] == 2) {                           /* loosen */
            DSC(bolt) = (s8)(DSC(bolt) - 1);
            if (DSC(bolt) <= 1) {
                DSC(bolt) = 0;
                sound_vec(5, 1);
                trans_bolt_remove_anim(i);
                st[b] = 0;
                sound_vec(5, 0);
                sound_vec(13);
                click_clear();
            } else {
                trans_bolt_draw(DSC(bolt), i);
                sound_vec(17);
            }
            if (st[1] == 0 && st[2] == 0) {         /* both out: the transmission drops out */
                trans_remove_anim();
                gs_clock_add(0x666);
                DSW(DS_trans_pic) = 0;
                DSC((u16)(car + CR_TRANS_BOLT + 1)) = 0;
                DSC((u16)(car + CR_TRANS_BOLT)) = 0;
                part_uninstall((u16)(car + CR_TRANS));
                DSB(hs1) &= 0xFE;
                DSB(hs2) &= 0xFE;
                click_clear();
            }
        } else if (st[b] == 1) {                    /* tighten */
            if (DSC(bolt) == 3) {
                st[b] = 2;
            } else {
                DSC(bolt) = (s8)(DSC(bolt) + 1);
                trans_bolt_draw(DSC(bolt), i);
                sound_vec(17);
            }
        } else {                                    /* put a bolt in */
            cursor_ctl(-4);
            sound_vec(5, 1);
            DSC(bolt) = 1;
            trans_bolt_draw(1, i);
            sound_vec(5, 0);
            sound_vec(13);
            cursor_ctl(-2);
            st[b] = 1;
        }
    }
    ui_pop(1);
}
