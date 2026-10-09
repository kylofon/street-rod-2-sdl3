/* The engine bay screens (S5.2 / W2b): ignition_tune 0001:7aec (timing light), engine_bay 0001:80a8 and bay_hotspots
 * 1a21:3d10. From port/spec/garage.md "engine_bay" / "ignition_tune" / "bay_hotspots", checked against the disassembly
 * (the spec pseudocode matches it except where noted). The bay drawing helpers (bay_*, trans_*) are in game/garage_draw.h.
 * SR1 base: srport garage_bay.c. */
#include "game/ds_screens.h"

/* ============================================================================== 1a21:3d10 */

/* bay_hotspots: rebuild the hot spots of screen 14 for the bolts that exist and the connector. ctx_tbl / side_tbl are
 * DS offsets of word tables indexed by e = 0..5 (bolt e+1): engine_bay passes 0x4E8E / 0x4E9C. */
void bay_hotspots(u16 ctx_tbl, u16 side_tbl)
{
    u16 p = ui_screen(14);                          /* [bp-2]: next element */
    u16 wires = 0;                                  /* [bp-4]: the Wires element (code -13) */
    s16 save_ctx = DSS(DS_bay_ctx), save_side = DSS(DS_bay_side);
    s16 L = 0, cnt = 0;                             /* layer and bolt within the layer */
    s16 x = 0, y = 0;

    for (s16 e = 0; e < 6; e++) {
        if (DSS((u16)(DS_bay_slot_pic + 2 * L)) != 0) {
            DSS(DS_bay_ctx) = DSS((u16)(ctx_tbl + 2 * e));
            DSS(DS_bay_side) = DSS((u16)(side_tbl + 2 * e));
            bay_bolt_pos(&x, &y);
            x = (s16)(x + 0x18);
            while (DSS((u16)(p + UIEL_CODE)) < 0) {             /* skip the buttons, remember Wires */
                if (DSS((u16)(p + UIEL_CODE)) == -13) wires = p;
                p = (u16)(p + UIEL_BYTES);
            }
            DSS((u16)(p + UIEL_CODE)) = (s16)(e + 1);
            DSS((u16)(p + UIEL_X0)) = (s16)(x - 5);
            DSS((u16)(p + UIEL_X1)) = (s16)(x + 5);
            DSS((u16)(p + UIEL_Y1)) = (s16)(y + 2);
            DSS((u16)(p + UIEL_Y0)) = (s16)(y + 2 - 15);
            DSW((u16)(p + UIEL_FLAGS)) = 1;
            DSB((u16)(p + UIEL_KEYS)) = (u8)(e + 2);            /* scancode of the digit key */
            p = (u16)(p + UIEL_BYTES);
        }
        cnt++;
        if (DSS((u16)(DS_BOLTS_PER_LAYER + 2 * L)) == cnt) { L++; cnt = 0; }
    }

    for (;;) {                                      /* the rest: bolt elements that are not used are disabled */
        u16 q = p;
        s16 code = DSS((u16)(q + UIEL_CODE));
        if (code == -13) wires = q;
        else if (code > 0) DSB(q) &= 0xFE;
        p = (u16)(p + UIEL_BYTES);
        if (DSB(q) & UIF_LAST) break;
    }

    if (wires != 0) {
        if (DSS(DS_bay_slot_pic) != 0) {            /* an engine: the connector's rectangle */
            s16 idx = bay_engine_idx();
            s16 a, c;
            if (DSS(DS_bay_connector_pic) == 0x432) { a = 0x21; c = 0x16; } else { a = 0x24; c = 0x14; }
            s16 cx = DSS((u16)(DS_CONNECTOR_POS + 4 * idx)), cy = DSS((u16)(DS_CONNECTOR_POS + 4 * idx + 2));
            DSS((u16)(wires + UIEL_X0)) = (s16)(cx - a + 0x0E);
            DSS((u16)(wires + UIEL_X1)) = (s16)(cx - a + 0x0E + 0x14);
            DSS((u16)(wires + UIEL_Y0)) = (s16)(cy - c - 7);
            DSS((u16)(wires + UIEL_Y1)) = (s16)(cy - c - 7 + 0x0F);
            DSB(wires) |= UIF_ACTIVE;
        } else {
            DSB(wires) &= 0xFE;
        }
    }
    ui_sort_screen(14);
    DSS(DS_bay_ctx) = save_ctx;
    DSS(DS_bay_side) = save_side;
}

/* ============================================================================== 0001:7aec */

/* ignition_tune: screen 30 (-7 Retard, -10 Advance, -1 Done); the timing-light animation. car->ignition ranges
 * -5..+3 (0 is best). The engine tone (AdLib 2c57:0b2c/0bd2/0ba6/0c5c, PC-speaker PIT writes) belongs to the sound
 * module (S5.4): snd_adlib_engine / snd_ignition_tone are stand-ins. */
void ignition_tune(u16 car)
{
    FarPtr back = g_back(), front = g_front();
    Rect r1, r2, r3;
    s16 changed = 1;

    DSW(DS_g_mirror) = 0;
    screen_fill_rect(0x1C, 4, 0xC0, 0xB5, 0);
    screen_fill_rect(0xF2, 0x1F, 0x27, 9, 5);
    screen_fill_rect(0xF2, 0x96, 0x27, 9, 5);
    show_picture_at(0x420, 0x40, 0x21, -1);         /* LIB2 #56 engine front */
    {
        u8 keep = DSB(DS_g_line_col);               /* the line under the strobe, in ui colour 1 */
        DSB(DS_g_line_col) = DSB((u16)(DS_ui_colours + 2));
        hline(back, 0x96, 0x71, 3);
        DSB(DS_g_line_col) = keep;
    }

    /* A: the background under the strobe light */
    FarPtr A = arena_bitmap_alloc(0x10, 0x1E, 0, 2);
    r1 = (Rect){ 0x10, 0x1E, 0x90, 0x5E, 0, 0 };
    drv_blit(back, A, &r1, 1);
    r1.dx = 0x90; r1.dy = 0x5E;
    /* B: the fan, frames of 0x20 pixels (background + the picture 0x424) */
    FarPtr P1 = pic_get(0x424, 2);
    r1.sx = 0; r1.sy = 0;
    FarPtr B = arena_bitmap_alloc(0x60, 0x2D, 0, 2);
    r2 = (Rect){ 0x20, 0x2D, 0x48, 0x63, 0, 0 };
    drv_blit(back, B, &r2, 1);
    r2.w = 0x40; r2.sx = 0; r2.sy = 0; r2.dx = 0x20;
    drv_blit(P1, B, &r2, 1);
    r2.dx = 0x48; r2.dy = 0x63;
    /* C: the belt, 16-pixel frames (background + the picture 0x434) */
    FarPtr P2 = pic_get(0x434, 2);
    FarPtr C = arena_bitmap_alloc(0x30, 0x21, 0, 2);
    r3 = (Rect){ 0x10, 0x21, 0x60, 0x3C, 0, 0 };
    drv_blit(back, C, &r3, 1);
    r2.w = 0x20; r3.w = 0x20; r3.sx = 0; r3.sy = 0; r3.dx = 0x10;
    drv_blit(P2, C, &r3, 1);
    r3.w = 0x10; r3.dx = 0x60; r3.dy = 0x3C;

    DSW(DS_g_mirror) = 1;
    page_copy_rect(back, front, 0x1C, 0, 0x127, 0xBC);
    ui_push(30);
    music_stop();
    DSW(DS_music_mode) = 2;
    if (DSW(DS_snd_device) == 0) { snd_adlib_engine(1); }          /* AdLib: 2c57:0b2c(1), 2c57:0bd2(1) */
    DSW(DS_GS_ENGINE_SOUND) = 2;                                   /* engine-sound mode on */

    s16 b;
    do {
        if (DSB(DS_sound_on) != 0 && DSB(DS_sound_gate) != DSB(DS_sound_on)) sound_vec(24);   /* sound resume vector */
        s8 ign = DSC((u16)(car + CR_IGNITION));
        snd_ignition_tone(car);                                    /* speaker: PIT channel 2; AdLib: notes from ign */
        cursor_ctl(-3);
        if (changed) {                                             /* the timing-light flash pattern */
            s16 y0 = (s16)((ign + 0x25) * 3);
            drv_blit(A, back, &r1, 1);
            u16 sseg = DSW(DS_g_draw_seg), soff = DSW(DS_g_draw_off);
            FarPtr pl = desc_planes(back);
            DSW(DS_g_draw_seg) = pl.seg;                           /* ega_line draws into DS:9DC2/9BC0 */
            DSW(DS_g_draw_off) = pl.off;
            for (s16 k = 0; k < 5; k++) {
                s8 a = DSC((u16)(DS_TIMING_FLASH + (ign * 5 + k) * 2));
                s8 e = DSC((u16)(DS_TIMING_FLASH + (ign * 5 + k) * 2 + 1));
                if (a >= 0) drv_line((s16)(a + 0x90), (s16)(y0 + k), (s16)(e + 0x90), (s16)(y0 + k), (s16)DSW(DS_ui_colours));
            }
            DSW(DS_g_draw_seg) = sseg;
            DSW(DS_g_draw_off) = soff;
        }
        drv_blit(B, back, &r2, 1);
        drv_blit(C, back, &r3, 1);
        r2.sx = (s16)(r2.sx + 0x20);
        if (r2.sx > 0x40) r2.sx = 0;
        r3.sx = (s16)(r3.sx - 0x10);
        if (r3.sx < 0) r3.sx = 0x20;
        cursor_ctl(-1);
        page_copy_rect(back, front, 0x48, 0x63, 0x67, 0x8F);
        page_copy_rect(back, front, 0x60, 0x3C, 0x6F, 0x5C);
        if (changed) {
            page_copy_rect(back, front, 0x90, 0x5E, 0x9F, 0x7B);
            changed = 0;
        }
        b = ui_wait(4);
        if (b == -10) {                                            /* Advance */
            DSC((u16)(car + CR_IGNITION)) = (s8)(DSC((u16)(car + CR_IGNITION)) + 1);
            if (DSC((u16)(car + CR_IGNITION)) > 3) DSC((u16)(car + CR_IGNITION)) = 3;
            changed = 1;
        } else if (b == -7) {                                      /* Retard */
            DSC((u16)(car + CR_IGNITION)) = (s8)(DSC((u16)(car + CR_IGNITION)) - 1);
            if (DSC((u16)(car + CR_IGNITION)) < -5) DSC((u16)(car + CR_IGNITION)) = -5;
            changed = 1;
        }
    } while (b != -1);

    if (DSW(DS_snd_device) == 0) { snd_adlib_engine(0); }
    DSW(DS_GS_ENGINE_SOUND) = 0;
    if (DSW(DS_snd_device) == 1) sound_vec(23);                    /* speaker: PIT back to normal */
    DSW(DS_music_mode) = 1;
    if (DSW(DS_music_on) != 0) music_resume();
    gs_clock_add(0x444);                                           /* a fixed 2 hours */
    ui_pop(1);
    arena_pop(5);
}

/* ============================================================================== 0001:80a8 */

/* engine_bay: screen 14 (bolts 1..6, -13 Wires, -3 Tune, -1 Done, -2 Parts). Any car can be shown; only cur_car is
 * interactive. Bolt b = 1..6 is car->bolt[b-1] (CR_BOLT + b - 1): b1 engine, b2/b3 manifold, b4/b5/b6 carburettors.
 * The state variables below are the original's stack frame; g192 is [bp-0x192], which the original uses both for
 * the manifold grade and as the layer-sum accumulator of the bolt dispatch (a stale value is read by the "fit" code
 * after a rejected part). */
void engine_bay(u16 car)
{
    u16 node[192];
    s8 wear[192];
    s16 n;
    s16 st[8];                                      /* [1..6]: 0 out, 1 tightening, 2 loosening */
    s16 layers, max_layers;
    s16 g192 = 0;

    for (s16 b = 1; b <= 6; b++) {
        s8 v = DSC((u16)(car + CR_BOLT + b - 1));
        st[b] = v == 0 ? 0 : (v == 1 ? 1 : 2);
        DSS((u16)(DS_bay_bolt_pic + 2 * (b - 1))) = v;              /* bolt tightness 0..3 (not a picture id) */
    }
    s8 link = DSC((u16)(car + CR_ENGINE_LINK));
    DSS(DS_bay_connector_pic) = link == 0 ? 0 : (link < 0 ? 0x432 : 0x433);
    if (DSW((u16)(car + CR_ENGINE)) != 0)                           /* manifold-0 picture: V-6 0x429, V-8 0x42A */
        DSS(DS_bay_manifold0_pic) = (s16)(0x42A - (gs_part_grade(DSW((u16)(car + CR_ENGINE))) == 0));
    for (s16 s = 0; s < 5; s++) {
        u16 p = DSW((u16)(car + CR_ENGINE + 2 * s));
        DSS((u16)(DS_bay_slot_pic + 2 * s)) = p == 0 ? 0 : DSS((u16)(DS_BAY_PICS + 2 * (gs_part_grade(p) + 9 * s)));
    }
    DSS(DS_BAY_LAYOUT) = DSS(DS_bay_slot_pic) == 0x425 ? 4 : 5;     /* BAY_LAYOUT[0][0..1]: V-6 / V-8 layout code */
    DSS(DS_GS_BAY_LAYOUT_V6) = DSS(DS_BAY_LAYOUT);
    layers = 0;
    for (s16 s = 0; s < 5; s++) layers = (s16)(layers + (DSS((u16)(DS_bay_slot_pic + 2 * s)) != 0));
    {
        u16 man = DSW((u16)(car + CR_MANIFOLD));
        if (man != 0) {
            g192 = gs_part_grade(man);
            max_layers = (s16)(DSS((u16)(DS_CARBS_PER_MANIFOLD + 2 * g192)) + 2);
            for (s16 k = 3; k < 6; k++)                             /* layout code of carburettor k-3 */
                DSS((u16)(DS_BOLT_CTX + 2 * (k + 1))) = DSS((u16)(DS_BAY_LAYOUT + 2 * (g192 * 3 + k - 3)));
        } else {
            max_layers = 5;
        }
    }

    cursor_ctl(-4);
    bay_draw_car(car);                              /* rebuild the bay bitmap */
    if (DSW(DS_cur_car) != car) return;             /* somebody else's engine (race): view only, cursor stays hidden */
    bay_draw_parts(1, 0);
    cursor_ctl(-2);
    bay_hotspots(DS_GS_BOLT_CTX_T1, DS_GS_BOLT_SIDE_T2);
    ui_push(14);

    for (;;) {
        s16 b;
        s16 slot, cascade;
        bool check = false;                         /* run the "part drops out" test */
        do b = ui_wait(3000); while (b == 0);

        if (b == -13) {                             /* Wires: toggle the engine connector */
            s8 lk = (s8)-DSC((u16)(car + CR_ENGINE_LINK));
            DSC((u16)(car + CR_ENGINE_LINK)) = lk;
            if (lk == 0) continue;
            if (lk > 0) {
                DSS(DS_bay_connector_pic) = 0x433;
                bay_connector_draw();
                gs_clock_add(0x6D);
                bay_hotspots(DS_GS_BOLT_CTX_T1, DS_GS_BOLT_SIDE_T2);
                continue;
            }
            DSS(DS_bay_connector_pic) = 0x432;
            bay_connector_draw();
            gs_clock_add(0x6D);
            DSS(DS_bay_ctx) = 0;
            bay_hotspots(DS_GS_BOLT_CTX_T1, DS_GS_BOLT_SIDE_T2);
            b = 1;                                  /* the engine test below looks at bolt 1 */
            check = true;
        } else if (b == -3) {                       /* Tune */
            if (car_runnable(car, 0) == 0) {
                msg_box(26);                        /* Can't tune that mess, klutz! */
            } else {
                ignition_tune(car);
                cursor_ctl(-4);
                bay_draw_parts(1, 0);
                cursor_ctl(-2);
            }
            continue;
        } else if (b == -2) {                       /* Parts: fit the next layer */
            s16 cat = DSS((u16)(DS_LAYER_CAT + 2 * layers));
            spares_collect(cat, node, wear, 0, &n);
            for (;;) {
                s16 sel = gs_list_box(7, 0, wear, n);
                if (sel == -4) {
                    sel = -1;                       /* Forget it */
                } else if (sel == -3) {
                    sel = list_selected();
                } else if (sel == 0x3EA) {          /* no spare engine / manifold / carburettor */
                    if (cat == 0) msg_box(27);
                    if (cat == 3) msg_box(28);
                    if (cat == 2) msg_box(29);
                    sel = -1;
                }
                if (sel < 0) break;
                if (max_layers <= layers) { msg_box(22); break; }   /* Where will you put it, speedy? */

                s16 t = DSS((u16)(node[sel] + PT_TYPE));
                s16 model = gs_car_model(car);
                s16 mk = gs_part_fits(t, model);
                s16 fits = 0;
                s16 first_bolt = 0;
                slot = layers;
                if (layers == 0) {                  /* engine */
                    fits = mk != 0;
                    DSS(DS_bay_ctx) = 0;
                    first_bolt = 0;
                    DSC((u16)(car + CR_ENGINE_LINK)) = -1;
                    DSS(DS_bay_connector_pic) = 0x432;
                    gs_clock_add(fits ? 0x666 : 0x888);
                } else if (layers == 1) {           /* manifold */
                    if (mk != 0) {
                        g192 = parts_grade(t);
                        fits = DSB((u16)(DS_MANIFOLD_FITS + gs_part_grade(DSW((u16)(car + CR_ENGINE))) * 9 + g192)) != 0;
                    }
                    DSS(DS_bay_ctx) = 1;
                    first_bolt = 1;
                    max_layers = (s16)(DSS((u16)(DS_CARBS_PER_MANIFOLD + 2 * parts_grade(t))) + 2);
                    for (s16 k = 4; k <= 6; k++)
                        DSS((u16)(DS_BOLT_CTX + 2 * k)) = DSS((u16)(DS_BAY_LAYOUT + 2 * (g192 * 3 + k - 4)));
                    gs_clock_add(fits ? 0x333 : 0x444);
                } else if (layers <= 4) {           /* carburettor */
                    if (mk != 0) {
                        g192 = gs_part_grade(DSW((u16)(car + CR_MANIFOLD)));
                        fits = DSB((u16)(DS_CARB_FITS + g192 * 3 + parts_grade(t))) != 0;
                    }
                    gs_clock_add(fits ? 0x199 : 0x222);
                    first_bolt = 3;
                    slot = 2;
                    s16 ctx = DSS((u16)(DS_BAY_LAYOUT + 2 * (g192 * 3)));
                    DSS(DS_bay_ctx) = ctx;
                    switch (ctx) {
                    case 7:
                        if (DSW((u16)(car + CR_CARB)) != 0) {
                            if (DSW((u16)(car + CR_CARB + 2)) == 0) { DSS(DS_bay_ctx) = 8; first_bolt++; slot = 3; }
                            else { DSS(DS_bay_ctx) = 9; first_bolt = (s16)(first_bolt + 2); slot = 4; }
                        }
                        break;
                    case 10: case 12: case 15:
                        if (DSW((u16)(car + CR_CARB)) != 0) { DSS(DS_bay_ctx) = (s16)(ctx + 1); first_bolt++; slot++; }
                        break;
                    default:                        /* 4, 5, 6, 14: a single carburettor */
                        break;
                    }
                } else {
                    break;                          /* (unreachable) all five layers are full */
                }

                s16 pic = DSS((u16)(DS_BAY_PICS + 2 * (parts_grade(t) + 9 * slot)));
                DSS((u16)(DS_bay_slot_pic + 2 * slot)) = pic;
                if (!fits) {
                    sound_vec(10, 1);
                    bay_part_in_anim(DSS((u16)(DS_bay_slot_pic + 2 * slot)), 1);
                    sound_vec(10, 0);
                    sound_vec(14);
                    msg_box(24);                    /* That won't fit! */
                    sound_vec(10, 1);
                    bay_part_out_anim();
                    sound_vec(10, 0);
                    DSS((u16)(DS_bay_slot_pic + 2 * slot)) = 0;
                    if (slot == 0) { DSC((u16)(car + CR_ENGINE_LINK)) = 0; DSS(DS_bay_connector_pic) = 0; }
                    continue;                       /* the part stays a spare; the same list is shown again */
                }
                part_install(spare_unlink(node[sel]), (u16)(car + CR_ENGINE + 2 * slot));
                cursor_ctl(-4);
                sound_vec(10, 1);
                bay_part_in_anim(DSS((u16)(DS_bay_slot_pic + 2 * slot)), 0);
                sound_vec(10, 0);
                sound_vec(14);
                if (slot == 0) {
                    if (pic == 0x425) { DSS(DS_bay_manifold0_pic) = 0x429; DSS(DS_BAY_LAYOUT) = 4; }
                    else { DSS(DS_bay_manifold0_pic) = 0x42A; DSS(DS_BAY_LAYOUT) = 5; }
                    DSS(DS_bay_connector_pic) = 0x432;
                    DSS(DS_GS_BAY_LAYOUT_V6) = DSS(DS_BAY_LAYOUT);
                    bay_connector_draw();
                }
                for (s16 k = (s16)(DSS((u16)(DS_BOLTS_PER_LAYER + 2 * layers)) - 1); k >= 0; k--) {   /* fresh loose bolts */
                    s16 i = (s16)(first_bolt + k);
                    DSS(DS_bay_side) = k;
                    sound_vec(5, 1);
                    DSS((u16)(DS_bay_bolt_pic + 2 * i)) = 1;
                    bay_bolt_draw(1);
                    sound_vec(5, 0);
                    sound_vec(13);
                    st[i + 1] = 1;
                    DSC((u16)(car + CR_BOLT + i)) = 1;
                }
                bay_hotspots(DS_GS_BOLT_CTX_T1, DS_GS_BOLT_SIDE_T2);
                cursor_ctl(-2);
                layers++;
                DSC((u16)(car + CR_IGNITION)) = (s8)(rnd(9) - 5);
                break;                              /* back to the bay (SR1 stayed in the list) */
            }
            continue;
        } else if (b == -1) {                       /* Done */
            ui_pop(1);
            DSW(DS_bay_bitmap) = 0;
            DSW((u16)(DS_bay_bitmap + 2)) = 0;
            arena_reset_stacks();
            return;
        } else {                                    /* a bolt */
            g192 = 0;
            s16 L = 0;                              /* which layer does bolt b hold? */
            /* PORT: the original does not range-check b; screen 14 only has live codes 1..6 and the buttons. */
            if (b < 1 || b > 6) continue;
            while (L < 5) {
                g192 = (s16)(g192 + DSS((u16)(DS_BOLTS_PER_LAYER + 2 * L)));
                if (g192 >= b) break;
                L++;
            }
            if (DSS((u16)(DS_bay_slot_pic + 2 * L)) == 0) continue;
            gs_clock_add(0x36);
            u16 bolt = (u16)(car + CR_BOLT + b - 1);
            if (st[b] == 2) {                       /* loosen */
                DSS(DS_bay_ctx) = DSS((u16)(DS_BOLT_CTX + 2 * b));
                DSS(DS_bay_side) = DSS((u16)(DS_BOLT_SIDE + 2 * b));
                DSC(bolt) = (s8)(DSC(bolt) - 1);
                if (DSC(bolt) <= 1) {               /* out: drops with its anim */
                    DSC(bolt) = 0;
                    st[b] = 0;
                    DSS((u16)(DS_bay_bolt_pic + 2 * (b - 1))) = 0;
                    sound_vec(5, 1);
                    bay_bolt_remove_anim();
                    sound_vec(5, 0);
                    sound_vec(13);
                } else {
                    DSS((u16)(DS_bay_bolt_pic + 2 * (b - 1))) = DSC(bolt);
                    bay_bolt_draw(DSC(bolt));
                    sound_vec(17);
                }
                check = true;
            } else if (st[b] == 1) {                /* tighten */
                if (DSC(bolt) == 3) {
                    st[b] = 2;
                } else {
                    DSC(bolt) = (s8)(DSC(bolt) + 1);
                    DSS((u16)(DS_bay_bolt_pic + 2 * (b - 1))) = DSC(bolt);
                    DSS(DS_bay_ctx) = DSS((u16)(DS_BOLT_CTX + 2 * b));
                    DSS(DS_bay_side) = DSS((u16)(DS_BOLT_SIDE + 2 * b));
                    bay_bolt_draw(DSC(bolt));
                    sound_vec(17);
                }
            } else {                                /* insert a bolt */
                DSS(DS_bay_ctx) = DSS((u16)(DS_BOLT_CTX + 2 * b));
                DSS(DS_bay_side) = DSS((u16)(DS_BOLT_SIDE + 2 * b));
                sound_vec(5, 1);
                DSS((u16)(DS_bay_bolt_pic + 2 * (b - 1))) = 1;
                bay_bolt_draw(1);
                sound_vec(5, 0);
                sound_vec(13);
                st[b] = 1;
                DSC(bolt) = 1;
            }
        }
        if (!check) continue;

        /* The part drops out when its last bolt is out; a removed part can let the one below go too (cascade). */
        do {
            slot = -1;
            cascade = 0;
            s16 ctx = DSS(DS_bay_ctx);
            if (ctx == 0) {                         /* engine (bolt 1 or the connector): last layer, unplugged, bolt out */
                if (layers == 1 && DSC((u16)(car + CR_ENGINE_LINK)) == -1 && st[1] == 0) {
                    slot = 0;
                    DSC((u16)(car + CR_ENGINE_LINK)) = 0;
                    DSS(DS_bay_connector_pic) = 0;
                    gs_clock_add(0x666);
                }
            } else if (ctx == 1) {                  /* manifold: both bolts out and no carburettor left */
                if (layers == 2 && st[2] == 0 && st[3] == 0) {
                    slot = 1;
                    cascade = 1;
                    gs_clock_add(0x333);
                }
            } else if (st[b] == 0) {                /* a carburettor bolt is out: that carburettor drops */
                slot = (s16)(b - 2);
                if (layers == 3) cascade = 2;
                gs_clock_add(0x199);
            }
            if (slot >= 0) {
                layers--;
                part_uninstall((u16)(car + CR_ENGINE + 2 * slot));
                cursor_ctl(-4);
                DSS((u16)(DS_bay_slot_pic + 2 * slot)) = 0;
                if (cascade != 0) DSS(DS_bay_ctx) = (s16)(cascade - 1);
                bay_draw_car(car);
                bay_draw_parts(1, 1);
                bay_hotspots(DS_GS_BOLT_CTX_T1, DS_GS_BOLT_SIDE_T2);
                DSC((u16)(car + CR_IGNITION)) = (s8)(rnd(9) - 5);
                cursor_ctl(-2);
            }
        } while (cascade != 0);
    }
}
