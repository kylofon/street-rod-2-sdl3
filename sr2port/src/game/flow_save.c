/* game_flow: saved games ?:HOTROD<n>.SAV (n = 0..14) and the "Save, Restart, or Quit." box —
 * port/spec/game_flow.md, FORMATS.md "Save games" (0001:55b0-63e0). Delta port of SR1 flow_save.c.
 *
 * File layout (little-endian, no header magic): the 0x38 bytes DS:A9D8..AA0F, the current car block, the other cars'
 * blocks, the spare part chain, DS:479C (12), the people table DS:AB7E (0x18C), game_clock DS:1F6A (4), rt_ticks - rt_base
 * (4), DS:7DFE (2), the garage block DS:9B40 (0x80). A car block is the 0x2A-byte record followed by the part chains of
 * its pointer fields +06, +08 +0A +0C +0E +10, +12, +1C, +1E, +20 (in that order) that are non-zero; a part chain is 8-byte
 * records, one more following while +06 != 0. Pointer values are written as they are in mem[] (their only meaning in
 * the file is zero / non-zero), so files are byte-identical to the original's for the same state, and a loaded game lands
 * at the same DGROUP addresses (pools reset, records allocated in file order). */
#include "game/flow.h"
#include "game/race.h"

#include "game/garage.h"
#include "game/garage_screens.h"
#include "game/records.h"
#include "game/ui.h"
#include "platform/platform.h"
#include "platform/video.h"

/* fd of a failed open / creat. PORT: the original tests an uninitialised stack word (">= 0": close and, for load,
 * new_game); the port treats it as "not open". */
#define FD_NONE 0xFFFF

/* The UI slice's list screens for the slot lists (game/ui.h; list texts = the u16 label pointers DS:A61C, n rows). */
static s16 slot_list_box(s16 screen, s16 sel, s16 n)
{
    return list_box_run(screen, sel, (const u16 *)mp(DGROUP, DS_list_entries), NULL, n);          /* 1a21:4ebc */
}

static s16 slot_list_edit_run(s16 screen, s16 sel, s16 n)
{
    return slot_list_edit(screen, sel, (const u16 *)mp(DGROUP, DS_list_entries), n);           /* 1a21:565c */
}

static bool wr_ok(u16 fd, u16 off, u16 n)
{
    u16 put = 0;
    return dos_write(fd, ds_ptr(off), n, &put) == 0 && put == n;
}

static bool rd_ok(u16 fd, u16 off, u16 n)
{
    u16 got = 0;
    return dos_read(fd, ds_ptr(off), n, &got) == 0 && got == n;
}

/* 0001:55b0 save_part_chain */
static s16 save_part_chain(u16 fd, u16 p)
{
    for (; p; p = DSW((u16)(p + PT_NEXT)))
        if (!wr_ok(fd, p, PART_BYTES)) return 1;
    return 0;
}

/* 0001:55fa load_part_chain: head = DS offset of the chain's head pointer */
static s16 load_part_chain(u16 fd, u16 head)
{
    u16 prev = 0;
    if (DSW(head) == 0) return 0;
    DSW(head) = 0;
    for (;;) {
        if (!rd_ok(fd, FLOW_STK_PART, PART_BYTES)) return 1;
        u16 p = part_alloc();
        for (u16 i = 0; i < PART_BYTES; i++) DSB((u16)(p + i)) = DSB((u16)(FLOW_STK_PART + i));
        if (prev) DSW((u16)(prev + PT_NEXT)) = p;
        else DSW(head) = p;
        prev = p;
        if (DSW((u16)(p + PT_NEXT)) == 0) return 0;
        DSW((u16)(p + PT_NEXT)) = 0;
    }
}

/* the ten part-pointer fields of a car record, in file order */
static const u8 car_chain_fields[10] = { CR_TRANS, 0x08, 0x0A, 0x0C, 0x0E, 0x10, CR_TYRES, CR_EXHAUST, CR_MUFFLER, CR_DIFF };

/* 0001:56a4 save_car_chain */
static s16 save_car_chain(u16 fd, u16 c, s16 one)
{
    for (; c; c = DSW((u16)(c + CR_NEXT))) {
        if (!wr_ok(fd, c, CAR_BYTES)) return 1;
        for (u16 k = 0; k < 10; k++)
            if (save_part_chain(fd, DSW((u16)(c + car_chain_fields[k])))) return 1;
        if (one) return 0;
    }
    return 0;
}

/* 0001:5776 load_car_chain: head = DS offset of the list head pointer */
static s16 load_car_chain(u16 fd, u16 head, s16 one)
{
    u16 prev = 0;
    const u16 rec = FLOW_STK_CAR;
    if (DSW(head) == 0) return 0;
    DSW(head) = 0;
    for (;;) {
        if (!rd_ok(fd, rec, CAR_BYTES)) return 1;
        for (u16 k = 0; k < 10; k++)
            if (load_part_chain(fd, (u16)(rec + car_chain_fields[k]))) return 1;
        u16 c = car_alloc();
        for (u16 i = 0; i < 0x2A; i++) DSB((u16)(c + i)) = DSB((u16)(rec + i));     /* 0x15 words */
        if (prev) DSW((u16)(prev + CR_NEXT)) = c;
        else DSW(head) = c;
        prev = c;
        if (DSW((u16)(c + CR_NEXT)) == 0 || one) return 0;
        DSW((u16)(c + CR_NEXT)) = 0;
    }
}

/* 0001:58b2 save_slots_scan: DS:90B8[n] = n if HOTROD<n>.SAV (n = 0..14) has a readable header, else 0xFF; the list
 * DS:A61C gets the labels (DS:8FC8 + 16 n, the first 15 characters of the header's name) of the present files, or of all
 * 15 slots with for_save (an empty label "" for the missing ones). *count = the number of rows. */
static void save_slots_scan(s16 for_save, s16 *count)
{
    u16 name = FLOW_STK_FNAME;
    s16 k = 0;
    for (s16 n = 0; n < 15; n++) {
        u16 fd = 0;
        fl_save_name(name, DS_hotrod_scan, n);
        if (dos_open(ds_str(name), 0, &fd) == 0) {
            if (rd_ok(fd, FLOW_STK_HDR, 0x38)) {
                DSB((u16)(DS_slot_present + n)) = (u8)n;
                DSW((u16)(DS_list_entries + 2 * k)) = (u16)(DS_slot_labels + 16 * n);
                fl_strncpy((u16)(DS_slot_labels + 16 * n), FLOW_STK_HDR, 15);
                k++;
            } else {
                DSB((u16)(DS_slot_present + n)) = 0xFF;
            }
            dos_close(fd);
        } else {
            DSB((u16)(DS_slot_present + n)) = 0xFF;
        }
        if (for_save && DSB((u16)(DS_slot_present + n)) == 0xFF) {
            DSW((u16)(DS_list_entries + 2 * k)) = (u16)(DS_slot_labels + 16 * k);
            DSB((u16)(DS_slot_labels + 16 * k)) = 0;
            k++;
        }
    }
    *count = k;
}

/* 0001:59e2 load_game_screen: 0 loaded, -1 "Forget it" / no saves, -0x29A read error */
s16 load_game_screen(void)
{
    s16 cnt;
    cursor_ctl(-4);
    cursor_ctl(1);
    save_slots_scan(0, &cnt);
    u16 s = DSW(DS_g_mirror);
    DSW(DS_g_mirror) = 0;
    show_picture(0x3E9, 0);                           /* LIB2 #1 Highway Patrol book */
    if (cnt > 0) {
        show_picture_at(0x3EC, 0xF0, 100, -1);        /* LIB2 #4 "load" */
        show_picture_at(0x3EE, 0xF0, 0x8C, -1);       /* LIB2 #6 "forget it" */
    }
    DSW(DS_g_mirror) = s;
    if (s) page_copy_rect(g_back(), g_front(), 0, 0, 0x140, 0xBE);
    cursor_ctl(-2);
    if (cnt == 0) {
        dialog_msg(ds_str(DS_msg_no_saves));          /* "No saved games found !" */
        return -1;
    }
    s16 r = slot_list_box(0x1A, 0, cnt);
    if (r == -4) return -1;                           /* Forget it */
    if (r == -3) r = DSS(DS_ui_sel);                  /* Load: the selected row */
    s16 slot = 0;                                     /* the (r+1)-th present file (r >= 0: do-while as the original) */
    if (r >= 0) {
        s16 k = 0;
        do {
            if ((s8)DSB((u16)(DS_slot_present + slot)) >= 0) k++;
            slot++;
        } while (k <= r);
    }
    slot--;
    cursor_ctl(-4);
    u16 name = fl_save_name(FLOW_STK_FNAME, DS_hotrod_load, (s16)(s8)DSB((u16)(DS_slot_present + slot)));
    u16 fd = FD_NONE;
    if (dos_open(ds_str(name), 0, &fd) != 0) fd = FD_NONE;
    else if (rd_ok(fd, DS_save_name, 0x38)) {
        pools_init(0);                                /* the header's pointers now mean "present" */
        if (!load_car_chain(fd, DS_cur_car, 1) && !load_car_chain(fd, DS_car_list, 0) &&
            !load_part_chain(fd, DS_spare_parts) &&
            rd_ok(fd, DS_counters, 12) && rd_ok(fd, DS_people, 0x18C) &&
            rd_ok(fd, DS_game_clock, 4) && rd_ok(fd, FLOW_STK_ELAPSED, 4) &&
            rd_ok(fd, DS_road_param, 2) && rd_ok(fd, DS_garage_block, 0x80)) {
            dos_close(fd);
            if (DSW(DS_cur_car)) set_paint_palette((s16)(s8)DSB((u16)(DSW(DS_cur_car) + CR_COLOUR)));
            track_build_all(0);                        /* regenerate the road from the saved seed DS:A9F8 */
            track_build_course(DSS(DS_road_param));
            DSL(DS_rt_base) = DSL(DS_rt_ticks) - DSL(FLOW_STK_ELAPSED);
            cursor_ctl(-2);
            return 0;
        }
    }
    cursor_ctl(-2);
    dialog_msg(ds_str(DS_msg_cannot_read));           /* "Cannot read that game !" */
    cursor_ctl(-3);
    if ((s16)fd > -1) {
        dos_close(fd);
        new_game();
    }
    cursor_ctl(-1);
    return -0x29A;
}

/* Highlight row r of the save list on both pages (a typed name equal to a slot's label selects that slot): the frame of the
 * row element DS:B888 + 0x12 r in the colours of the first row's scheme entry, line colour = the background. */
static void save_highlight_row(s16 r)
{
    cursor_ctl(-3);
    u16 first = DSW(DS_ui_row0);                      /* the first list row element */
    u16 row = (u16)(first + FLOW_ELEM_SIZE * r);
    u8 save[5];
    for (int i = 0; i < 5; i++) save[i] = DSB((u16)(DS_g_text_fg + i));
    u16 sch = (u16)(DSW(DS_g_scheme) + 5 * DSW((u16)(first + 4)));
    for (int i = 0; i < 5; i++) DSB((u16)(DS_g_text_fg + i)) = DSB((u16)(sch + i));
    u8 line = DSB(DS_g_line_col);
    DSB(DS_g_line_col) = DSB(DS_g_text_bg);
    s16 w = (s16)(DSW((u16)(first + 0x0C)) - DSW((u16)(first + 8)));
    draw_box1(g_back(), DSS((u16)(row + 8)), DSS((u16)(row + 0x0A)), w, 8);
    draw_box1(g_front(), DSS((u16)(row + 8)), DSS((u16)(row + 0x0A)), w, 8);
    for (int i = 0; i < 5; i++) DSB((u16)(DS_g_text_fg + i)) = save[i];
    DSB(DS_g_line_col) = line;
    cursor_ctl(-1);
}

/* 0001:5d82 save_game_screen */
void save_game_screen(void)
{
    s16 cnt;
    s16 sel = -1, r;
    cursor_ctl(-4);
    cursor_ctl(1);
    save_slots_scan(1, &cnt);                         /* all 15 rows, row r = slot r */
    DSW(DS_g_mirror) = 0;
    show_picture(0x3E9, 0);                           /* book */
    show_picture_at(0x3EB, 0x48, 0xA9, -1);           /* LIB2 #3 name field */
    show_picture_at(0x3ED, 0xF0, 100, -1);            /* LIB2 #5 "save" */
    show_picture_at(0x3EE, 0xF0, 0x8C, -1);           /* "forget it" */
    page_copy_rect(g_back(), g_front(), 0, 0, 0x140, 0xBE);
    DSW(DS_g_mirror) = 1;
    fl_strcpy(DS_save_name_edit, DS_save_name);       /* the field starts with the current save name */
    const u16 name = FLOW_STK_FNAME;
    for (;;) {
        s16 first = 0;                                /* the first free slot is the default selection */
        while (first < cnt && (s8)DSB((u16)(DS_slot_present + first)) >= 0) first++;
        if (first >= cnt) first = (s16)(cnt - 1);
        if (sel < 0) sel = first;
        cursor_ctl(-2);
        r = slot_list_edit_run(0x1C, sel, cnt);       /* rows, name field, Save -0x17, Forget it -0x18 */
        if (r == -0x18) return;
        if (r == -0x17) r = DSS(DS_ui_sel);
        sel = r;
        s16 trimmed = 0;
        if (str_is_blank(DS_save_name_edit)) {
            if (str_is_blank(DSW((u16)(DS_list_entries + 2 * sel)))) {
                dialog_msg(ds_str(DSW(DS_msg_name_ptr)));       /* "The game must have a name !" */
                cursor_ctl(-4);
                continue;
            }                                         /* else keep the existing label */
        } else {
            trimmed = trim_trailing_spaces(DS_save_name_edit);
            /* a typed name equal to a slot's label selects that slot */
            s16 k;
            for (k = 0; k < 15; k++)
                if (fl_strcmp(DSW((u16)(DS_list_entries + 2 * k)), DS_save_name_edit) == 0) break;
            if (k < 15) {
                save_highlight_row(sel);
                sel = k;
            }
        }
        /* PORT: the original fills name[0] (the drive) only before the creat below, so the remove() of an overwritten game
         * uses a stale drive byte (without effect: the drive prefix is ignored and the creat truncates). */
        fl_save_name(name, DS_hotrod_save, sel);
        s16 ans = (s8)DSB((u16)(DS_slot_present + sel)) >= 0 ? ui_dialog(0x1D) : 0;   /* "The old game will be replaced !" */
        cursor_ctl(-4);
        if (ans == -0x15) {                           /* Forget it: put the blanks back */
            for (s16 k = 0; k < trimmed; k++) {
                u16 len = fl_strlen(DS_save_name_edit);
                DSB((u16)(DS_save_name_edit + len + 1)) = 0;
                DSB((u16)(DS_save_name_edit + len)) = ' ';
            }
            continue;
        }
        if (ans == -0x14) dos_remove(ds_str(name));   /* Go ahead */
        fl_strcpy(DS_save_name, str_is_blank(DS_save_name_edit) ? DSW((u16)(DS_list_entries + 2 * sel)) : DS_save_name_edit);
        DSL(FLOW_STK_ELAPSED) = DSL(DS_rt_ticks) - DSL(DS_rt_base);
        DSB(name) = (u8)data_disk_check();
        u16 fd = FD_NONE;
        if (dos_creat(ds_str(name), 0, &fd) != 0) fd = FD_NONE;
        else if (wr_ok(fd, DS_save_name, 0x38) &&
                 !save_car_chain(fd, DSW(DS_cur_car), 1) && !save_car_chain(fd, DSW(DS_car_list), 0) &&
                 !save_part_chain(fd, DSW(DS_spare_parts)) &&
                 wr_ok(fd, DS_counters, 12) && wr_ok(fd, DS_people, 0x18C) &&
                 wr_ok(fd, DS_game_clock, 4) && wr_ok(fd, FLOW_STK_ELAPSED, 4) &&
                 wr_ok(fd, DS_road_param, 2) && wr_ok(fd, DS_garage_block, 0x80)) {
            dos_close(fd);
            cursor_ctl(-2);
            return;
        }
        cursor_ctl(-2);
        dialog_msg(ds_str(DS_msg_disk_error));        /* "Disk error during Save" */
        cursor_ctl(-4);
        if ((s16)fd > -1) {
            dos_close(fd);
            dos_remove(ds_str(name));
        }
        save_slots_scan(1, &cnt);
    }
}

/* 0001:630e quit_menu: "Save, Restart, or Quit." Save Game -0x17, Old Game -2, New Game -1, Quit -0x14 (after a confirmation
 * menu 0x23: Quit -0x14 / back -0x15), Forget it -0x16. The time spent in the menus does not count for the clock. */
s16 quit_menu(void)
{
    s16 x, y, x2, y2;
    u32 t0 = DSL(DS_rt_ticks);
    FarPtr bg = ui_save_bg(1, &x, &y, 2);
    s16 r;
    for (;;) {
        r = ui_menu(1);
        if (r != -0x14) break;
        FarPtr bg2 = ui_save_bg(0x23, &x2, &y2, 2);
        r = ui_menu(0x23);
        screen_put_bitmap_mirror(bg2, x2, y2);
        arena_pop(1);
        if (r != -0x15) break;
    }
    screen_put_bitmap_mirror(bg, x, y);
    arena_pop(1);
    DSL(DS_rt_base) += DSL(DS_rt_ticks) - t0;
    return r;
}
