/* game_flow: copy protection, the end of a game (Load / New / Quit menu, "King Street Rodders" wall of fame, the
 * ending after beating the King) and the Grudge Night message patching — port/spec/game_flow.md
 * (0001:3378-36ac, 0001:a51c, 0001:b2ea / b356, 2925:2714 / 27da). Delta port of SR1 flow_end.c. */
#include "game/flow.h"

#include <stdio.h>
#include <string.h>

#include "game/records.h"
#include "game/ui.h"
#include "modules.h"
#include "platform/platform.h"
#include "platform/video.h"

#define HALL_REC 0x17                   /* name[16] + hours[7] */

static inline bool is_ega_family(void)
{
    return DSS(DS_driver_id) == -2 || DSS(DS_driver_id) == -6;
}

/* 0001:3378 copy_protection: a manual-lookup question whose answer is never checked; always returns 1.
 * Quirk of the original (kept): the `jne` after ui_edit (0001:345C) targets the middle of the next instruction, so any
 * result other than -0x14 (Esc / cancel) leaves the stack unbalanced and crashes at the `retf`; the port returns 1 for
 * every result. The "answered" mark is a word store at e + 3 (the letter byte and the next entry's n byte). */
s16 copy_protection(void)
{
    cursor_ctl(-2);
    drv_blinds(desc_planes(g_front()), desc_planes(g_back()));          /* black screen */
    if (is_ega_family()) ega_set_palette(DS_prot_palette);
    else drv_pal_normal();
    DSB(DS_g_pal_visible) = 1;
    DSW(DS_g_mirror) = 1;
    s16 i = rnd(0x18);
    u16 e = (u16)(DS_protection_table + 4 * i);     /* 24 entries {n (word number), page, line, letter} */
    char text[0x60];
    snprintf(text, sizeof text, ds_str(DS_prot_fmt), (s16)(s8)DSB(e), (s16)(s8)DSB((u16)(e + 2)), (s16)(s8)DSB((u16)(e + 1)));
    for (u16 k = 0; k <= strlen(text); k++) DSB((u16)(DS_prot_text + k)) = (u8)text[k];
    if (DSB(e) == 1) { DSB(DS_prot_text + 0x0D) = 's'; DSB(DS_prot_text + 0x0E) = 't'; }       /* ordinal suffix */
    else if (DSB(e) == 2) { DSB(DS_prot_text + 0x0D) = 'n'; DSB(DS_prot_text + 0x0E) = 'd'; }
    else { DSB(DS_prot_text + 0x0D) = 't'; DSB(DS_prot_text + 0x0E) = 'h'; }
    DSB(DS_prot_edit) = 0;
    s16 r = ui_edit(0x1F, 1, ds_str(DS_prot_edit));
    if (r == -0x14) {
        DSW((u16)(e + 3)) = 1;
        DSW(DS_prot_edit) = 1;
    }
    status_print(-1, ds_str(DS_empty_string), 0);
    return 1;
}

/* 0001:348a game_over_menu: "Load Game" -2, "New Game" -1, "Quit" 4 */
s16 game_over_menu(void)
{
    pal_load(DS_pal_menu);
    status_init(0xBE);
    DSW(DS_g_mirror) = 1;
    u8 fg = DSB(DS_g_text_fg), bg = DSB(DS_g_text_bg), line = DSB(DS_g_line_col), fill = DSB(DS_g_fill_col);
    if (is_ega_family()) {
        DSB(DS_g_text_fg) = 0x0F; DSB(DS_g_text_bg) = 1; DSB(DS_g_line_col) = 0x0D; DSB(DS_g_fill_col) = 3;
    } else {
        DSB(DS_g_text_fg) = 3; DSB(DS_g_text_bg) = 0; DSB(DS_g_line_col) = 2; DSB(DS_g_fill_col) = 1;
    }
    screen_fill_rect(0, 0, 0x140, 0xBE, DSB(DS_g_text_bg));
    draw_box1(g_back(), 100, 0x32, 0x78, 0x5A);
    draw_box1(g_front(), 100, 0x32, 0x78, 0x5A);
    draw_box1(g_back(), 0x66, 0x34, 0x74, 0x56);
    draw_box1(g_front(), 0x66, 0x34, 0x74, 0x56);
    DSB(DS_g_text_fg) = fg; DSB(DS_g_text_bg) = bg; DSB(DS_g_line_col) = line; DSB(DS_g_fill_col) = fill;
    cursor_ctl(-2);
    drv_pal_normal();
    input_reset();
    s16 r = ui_menu(0x20);
    cursor_ctl(-4);
    return r;
}

/* ============================================================================ hall of fame */

/* 0001:35e8 hall_is_slower */
static s16 hall_is_slower(s16 i)
{
    return fl_atoi((u16)(DS_hall + 0x10 + i * HALL_REC)) > DSS(DS_hall_new_score);
}

/* 0001:360c hall_insert: hall_count already includes the new row (or is 10) */
static void hall_insert(s16 i)
{
    s16 n = (s16)(DSS(DS_hall_count) - i - 1);
    if (i < 0) return;
    u16 row = (u16)(DS_hall + i * HALL_REC);
    if (n > 0) fl_memmove((u16)(row + HALL_REC), row, (u16)(n * HALL_REC));
    fl_strcpy(row, DS_player_name);                   /* the driver (licence) name DS:A9E8 */
    fl_itoa(DSS(DS_hall_new_score), (u16)(row + 0x10));
}

/* 0001:3682 hall_score: hours since June 16, 00:00 */
static s16 hall_score(void)
{
    s16 h, m;
    clock_hm(&h, &m);
    return (s16)(day_index() * 0x18 + h + 0x0C);
}

/* 0001:36ac hall_of_fame_screen */
void hall_of_fame_screen(s16 won)
{
    u16 clip = DSW(DS_pic_rows);
    s16 row = -1;
    drv_pal_black();
    DSW(DS_pic_rows) = 200;
    DSW(DS_g_mirror) = 1;
    show_picture(0x4B9, -1);                          /* LIB2 #209 "King Street Rodders" wall */
    DSW(DS_pic_rows) = clip;
    pal_shadow_swap();
    u16 sch = DSW(DS_g_scheme);
    u8 s28[5], s23[5];
    for (int i = 0; i < 5; i++) { s28[i] = DSB((u16)(sch + 0x28 + i)); s23[i] = DSB((u16)(sch + 0x23 + i)); }
    if (is_ega_family())
        for (int i = 0; i < 5; i++) {
            DSB((u16)(sch + 0x28 + i)) = DSB((u16)(DS_hall_scheme_a + i));
            DSB((u16)(sch + 0x23 + i)) = DSB((u16)(DS_hall_scheme_b + i));
        }
    drv_pal_normal();
    if (won) {
        DSW(DS_hall_new_score) = (u16)hall_score();
        for (s16 i = 0; i < DSS(DS_hall_count); i++)
            if (hall_is_slower(i)) { row = i; break; }
        if (DSS(DS_hall_count) < 10) {
            if (row < 0) row = DSS(DS_hall_count);
            DSW(DS_hall_count)++;
        }
        hall_insert(row);                             /* full and slower than all ten: no entry */
    }
    if (is_ega_family()) { DSB(DS_g_text_fg) = 4; DSB(DS_g_text_bg) = 0; }
    else { DSB(DS_g_text_fg) = 3; DSB(DS_g_text_bg) = 2; }
    u16 rec = DS_hall;
    for (s16 i = 0; i < DSS(DS_hall_count); i++, rec = (u16)(rec + HALL_REC)) {
        s16 y = DSS((u16)(DS_hall_rows_y + 2 * i));
        drv_draw_text(g_back(), 0x2E, y, ds_str(rec));
        s16 x = 0xA1;
        /* right-align the hours: 16-bit v *= 10 until >= 1000. PORT: v = 0 loops forever in the original (never happens:
         * a score is at least 12); the port stops there. */
        s16 v = fl_atoi((u16)(rec + 0x10));
        if (v < 1000)
            do { x += 6; v = (s16)(u16)(v * 10); } while (v < 1000 && v != 0);
        drv_draw_text(g_back(), x, y, ds_str((u16)(rec + 0x10)));
        drv_draw_text(g_back(), 0xBF, y, ds_str(DS_hall_hr));       /* "hr" */
    }
    page_copy_rect(g_back(), g_front(), 0x2E, DSS(DS_hall_rows_y), 0xD3, (s16)(DSS(DS_hall_last_y) + 8));
    if (row < 0) {
        wait_click_or_key(700);
    } else {
        u16 r = (u16)(DS_hall + row * HALL_REC);
        fl_strcpy(FLOW_STK_NAME, r);
        edit_run(0x2E, DSS((u16)(DS_hall_rows_y + 2 * row)), ds_str(FLOW_STK_NAME), 0x0F);   /* the name may be edited */
        fl_strcpy(r, FLOW_STK_NAME);
        hall_save();
    }
    drv_pal_black();
    for (int i = 0; i < 5; i++) { DSB((u16)(sch + 0x28 + i)) = s28[i]; DSB((u16)(sch + 0x23 + i)) = s23[i]; }
    pal_shadow_swap();
}

/* ================================================================================ the ending */

/* 0001:a51c ending_trigger (front of SR1 ending_sequence a544; called by bob_drive_in after the King's race, which then
 * shows msg 0x44 and sets DS:1BC6 |= 2). SR1's girlfriend / lips sequence does not exist in SR2. */
void ending_trigger(void)
{
    u16 h = DSW(DS_pic_rows);
    cursor_ctl(-4);
    status_init(0xBE);
    pal_load(DS_pal_identity2);
    DSW(DS_pic_rows) = 200;
    show_picture_at(0x43B, 8, 0x14, 8);               /* LIB2 #51 "Well, you've done it..." */
    DSW(DS_pic_rows) = h;
    drv_pal_normal();
    wait_ticks(0x2EE);                                /* 750 ticks, input skips */
    /* sound_vec(19)() — the ending music (DS:BBD8, S5.4) */
    ending_anim();
    arena_clear();
    /* sound_vec(18)() — the title music (DS:BBD4, S5.4) */
}

/* 2925:2714 end_data_load: the two SR2_end tables go behind the shape-set data in arena 2 */
void end_data_load(void)
{
    u16 h;
    u16 base = DSW(DS_arena2), seg = DSW((u16)(DS_arena2 + 2));
    DSW(DS_end_frames) = (u16)(base + DSW(DS_arena2_used));
    DSW((u16)(DS_end_frames + 2)) = seg;
    DSW(DS_arena2_used) += 0x1F8;
    DSW(DS_end_script) = (u16)(DSW(DS_arena2_used) + base);
    DSW((u16)(DS_end_script + 2)) = seg;
    DSW(DS_arena2_used) += 0x2DC;
    if (dos_open(ds_str(DSW(DS_end_name_ptr)), 0, &h) != 0) {      /* "SR2_end", no drive prefix */
        modules.fatal_message(ds_str(DS_msg_end_open));           /* "Can't open end data file" */
        platform_exit();
    }
    u16 got;
    if (dos_read(h, ds_far(DS_end_frames), 0x1F8, &got) != 0 || got != 0x1F8) {
        modules.fatal_message(ds_str(DS_msg_end_read));           /* "End Data disk read fail" */
        platform_exit();
    }
    if (dos_read(h, ds_far(DS_end_script), 0x2DC, &got) != 0 || got != 0x2DC) {
        modules.fatal_message(ds_str(DS_msg_end_read));
        platform_exit();
    }
    dos_close(h);
}

typedef struct { s16 minx, miny, maxx, maxy; } Extent;   /* dirty rectangle, inclusive; empty = {1000, 1000, -1, -1} */

static const Extent EXTENT_EMPTY = { 1000, 1000, -1, -1 };

/* 2925:27da ending_anim: the SR2_end player. F = frames {u8 w, u8 h, u16 sx, u8 sy, u8 0} (84 x 6), S = script steps
 * {i8 frame (-1 ends), u8 dx, u8 dy, i8 flag} (4 bytes): sprites cut from LIB2 #181 are drawn over the backdrop #211 on the
 * back page; a step with a flag shows the dirty rectangle, a negative flag switches to the final picture (#195). */
void ending_anim(void)
{
    arena_reset();                                    /* 1a21:9b0e */
    DSW(DS_arena2_used) = 0;
    FarPtr sheet = pic_get(0x49D, 1);                 /* LIB2 #181 sprite sheet */
    FarPtr bg = pic_get(0x4BB, -1);                   /* LIB2 #211 backdrop (arena 2) */
    end_data_load();
    bool restore = false;
    screen_put_bitmap(bg, 0, 0);
    drv_blinds(desc_planes(g_front()), desc_planes(g_back()));
    Extent cur = EXTENT_EMPTY, prev = EXTENT_EMPTY;
    FarPtr F = ds_far(DS_end_frames), S = ds_far(DS_end_script);
    pace_ticks(0);
    for (;; S = far_add(S, 4)) {
        s16 f = (s8)rd8(S.seg, S.off);
        if (f < 0) break;
        Rect r;
        if (restore) {                                /* erase the sprites of the last step */
            r.dx = r.sx = cur.minx;
            r.dy = r.sy = cur.miny;
            r.w = (s16)(cur.maxx - cur.minx + 1);
            r.h = (s16)(cur.maxy - cur.miny + 1);
            drv_blit(bg, g_back(), &r, 1);
            prev = cur;
            cur = EXTENT_EMPTY;
            restore = false;
        }
        FarPtr fr = far_add(F, (u16)(6 * f));
        s16 cut[4];
        cut[0] = rd8(fr.seg, fr.off);                                    /* w */
        cut[1] = rd8(fr.seg, (u16)(fr.off + 1));                         /* h */
        cut[2] = (s16)rd16(fr.seg, (u16)(fr.off + 2));                   /* sx */
        cut[3] = rd8(fr.seg, (u16)(fr.off + 4));                         /* sy */
        s16 dx = rd8(S.seg, (u16)(S.off + 1)), dy = rd8(S.seg, (u16)(S.off + 2));
        FarPtr spr = bitmap_extract(sheet, cut, 2);
        r.w = cut[0]; r.h = cut[1]; r.sx = 0; r.sy = 0; r.dx = dx; r.dy = dy;
        bitmap_blit_masked_shift(spr, g_back(), &r, 0);      /* colour 0 transparent */
        arena_pop(1);
        s16 x0 = (s16)(dx & ~7), x1 = (s16)(((dx + cut[0] + 7) & ~7) - 1);
        s16 y1 = (s16)(dy + cut[1] - 1);
        if (x0 < prev.minx) prev.minx = x0;
        if (x0 < cur.minx) cur.minx = x0;
        if (dy < prev.miny) prev.miny = dy;
        if (dy < cur.miny) cur.miny = dy;
        if (x1 > prev.maxx) prev.maxx = x1;
        if (x1 > cur.maxx) cur.maxx = x1;
        if (y1 > prev.maxy) prev.maxy = y1;
        if (y1 > cur.maxy) cur.maxy = y1;
        s16 flag = (s8)rd8(S.seg, (u16)(S.off + 3));
        if (flag == 0) continue;
        pace_ticks(10);
        wait_vretraces(1);
        page_copy_rect(g_back(), g_front(), prev.minx, prev.miny, prev.maxx, prev.maxy);
        restore = true;
        if (flag > 0) continue;
        arena_pop_low(1);                             /* final picture: the sheet is replaced by LIB2 #195 */
        sheet = pic_get(0x4AB, 2);
        r.w = 0x88; r.h = 0x2E; r.sx = 0x60; r.sy = 0x68; r.dx = 0x88; r.dy = 0x92;
        drv_blit(sheet, bg, &r, 1);                   /* into the backdrop, so that the next restore shows it */
        cur.minx = 0x88; cur.miny = 0x92;
        cur.maxx = (s16)(r.w + 0x87);
        cur.maxy = (s16)(r.h + 0x91);
        restore = true;
    }
    wait_vretraces(0x50);
    DSW(DS_arena2_used) = 0;
}

/* ====================================================================== Grudge Night messages */

/* 0001:b2ea tournament_time_text: patches the time (tenths of a second) as "%2d.%d" into the blank "  . " of the first
 * line of message msg (the text of message m is a near pointer at DS:11EA + 6 m + 2) */
void tournament_time_text(s16 t, s16 msg)
{
    u16 line = DSW((u16)(DS_msg_table + 6 * msg + 2));
    u16 p = fl_strchr(line, '.');
    char buf[8];
    snprintf(buf, sizeof buf, "%2d.%d", t / 10, t % 10);        /* format string DS:51DD */
    for (u16 k = 0; buf[k]; k++) DSB((u16)(p - 2 + k)) = (u8)buf[k];
}

/* 0001:b356 tournament_result_message (mode 2 = qualifier): the message after a Grudge Night run, from the time
 * DS:B8C2 (tenths of a second), the dial-in DS:ADA8, the round DS:AD6C, the alive flag DS:9B3E and the handicap DS:ADDA */
void tournament_result_message(s16 mode)
{
    s16 id;
    cursor_ctl(-2);
    if (DSS(DS_tourn_alive) == 0) {                   /* the player lost this race */
        if (DSS(DS_tourn_dialin) > DSS(DS_tourn_time)) {        /* the opponent broke out: you advance */
            msg_box(0x79);
            DSW(DS_tourn_alive) = 1;
            return;
        }
        s32 t = ((s32)(100 - DSS(DS_pct_covered)) * DSS(DS_tourn_time)) / 100;
        DSS(DS_tourn_time) = (s16)t;
        if (t > 0x3E7) DSS(DS_tourn_time) = 0x3E7;
        tournament_time_text(DSS(DS_tourn_time), 0x69);
        id = DSS(DS_tourn_round) >= 2 ? 0x69 : 0x6A;
    } else if (mode == 2) {                           /* qualifier */
        if (DSS(DS_tourn_time) <= 0x320) {
            tournament_time_text(DSS(DS_tourn_time), 0x66);
            id = 0x66;
        } else {
            DSW(DS_tourn_alive) = 0;                  /* breakout time too weak */
            id = 0x71;
        }
    } else if (DSS(DS_tourn_dialin) > DSS(DS_tourn_time)) {     /* ran quicker than the dial-in */
        DSW(DS_tourn_alive) = 0;
        id = DSS(DS_tourn_round) >= 2 ? 0x6E : 0x6F;
    } else {
        u16 d = fl_strchr(DSW((u16)(DS_msg_table + 6 * 0x67 + 2)), 'd');   /* "You won round   with ..." */
        DSB((u16)(d + 2)) = (u8)('1' + DSS(DS_tourn_round));
        tournament_time_text(DSS(DS_tourn_time), 0x67);
        id = DSS(DS_tourn_round) > 2 ? 0x68 : 0x67;
    }
    msg_box(id);
}
