/* The SR2 UI toolkit: list boxes (garage.md 1a21:4e0c - 565c): the spare-part / car lists (screens 7, 9, 16,
 * 26 ...) and the save-game picker. Rows are screen elements with UIF_ROW; indices are 0-based. */
#include "game/ui.h"

#include <stdio.h>
#include <string.h>

#include "platform/platform.h"
#include "platform/video.h"

static u16 ds_offset_of(const void *p)
{
    const u8 *b = mp(DGROUP, 0);
    return (const u8 *)p >= b && (const u8 *)p < b + 0x10000 ? (u16)((const u8 *)p - b) : 0;
}

/* The 5-byte colour record of an element's scheme <-> DS:AD28.. */
static void cols_of(u16 el, u8 *c) { memcpy(c, mp(DGROUP, (u16)(DSW(DS_SCHEME) + 5 * UIEL_S(el, UIEL_SCHEME))), 5); }
static void cols_set(const u8 *c)  { memcpy(mp(DGROUP, DS_g_text_fg), c, 5); }

/* 1a21:4e0c wear_text: "(new)" for 0, else "(NN% worn)" built in place at DS:2168. */
u16 wear_text(s16 pct)
{
    if (pct == 0) return DS_wear_new;
    DSB(DS_wear_space) = ' ';
    char tmp[16];
    int n = snprintf(tmp, sizeof tmp, "%d", pct);
    memcpy(ds_str(DS_wear_digits), tmp, (size_t)n + 1);           /* itoa(pct, DS:2169, 10) */
    ds_str(DS_wear_digits)[n] = '%';                              /* the NUL becomes '%' (a 2/3-digit value eats the blank) */
    return DS_wear_tmpl;
}

/* 1a21:4e56 list_row: the text at (x, y); with show_wear the wear text right-aligned to `right` (6 px per char). */
void list_row(s16 x, s16 y, u16 text, s16 right, s16 show_wear, s16 pct)
{
    drv_draw_text(g_back(), x, y, ds_str(text));
    if (show_wear) {
        u16 w = wear_text(pct);
        drv_draw_text(g_back(), (s16)(right - 6 * (s16)strlen(ds_str(w))), y, ds_str(w));
    }
}

static s16 has_wear(const s8 *wear, s16 i) { return wear != NULL && (u8)wear[i] != 0x80; }

/* The title field of a list screen shows the selected text (+ "   " + the wear text). */
static void field_text(u16 field, u16 text, const s8 *wear, s16 i)
{
    char *buf = ds_str(DSW((u16)(field + UIEL_TEXT)));
    strcpy(buf, ds_str(text));
    if (has_wear(wear, i)) {
        size_t n = strlen(buf);
        strcpy(buf + n, ds_str(DS_wear_gap));
        n += strlen(buf + n);
        strcpy(buf + n, ds_str(wear_text(wear[i])));
    }
    ui_redraw_element(field);
}

/* 1a21:4ebc list_box_run */
s16 list_box_run(s16 screen, s16 sel, const u16 *texts, const s8 *wear, s16 n)
{
    s16 top = 0;                                       /* [bp-36h]: first visible entry */
    s16 prev = -1;                                     /* [bp-4]: hovered row, redrawn highlighted */
    if (n == 0) return 0x3EA;
    ui_push(screen);
    s16 rows = DSS(DS_ui_rows);
    u16 field = DSW(DS_ui_field);
    DSW(DS_ui_texts) = ds_offset_of(texts);
    DSS(DS_ui_sel) = sel;
    if (rows < n) {
        top = sel;
        if (sel + rows > n) top = (s16)(n - rows);
    }
    if (field != 0) field_text(field, texts[sel], wear, sel);

    u16 row0 = DSW(DS_ui_row0);
    s16 x0 = UIEL_S(row0, UIEL_X0), y0 = UIEL_S(row0, UIEL_Y0);
    s16 w = (s16)(UIEL_S(row0, UIEL_X1) - x0);
    s16 rowh = (s16)(UIEL_S(row0, UIEL_Y1) - y0);
    s16 h = (s16)(rowh * rows);
    u8 saved[5], scheme[5];
    memcpy(saved, mp(DGROUP, DS_g_text_fg), 5);
    cols_of(row0, scheme);
    u8 hover_fg = scheme[3];                           /* [bp-0Fh]: the highlighted row's text colour */
    cols_set(saved);                                   /* (the colours of the caller until the first redraw) */

    for (;;) {
        /* ---- redraw (LAB_503f) */
        cursor_ctl(-3);
        if (DSB(DS_ui_list_fill)) fill_rect(g_back(), x0, y0, w, h);
        cols_set(scheme);
        s16 count = (s16)(n - top);
        if (count > rows) count = rows;
        u16 e = DSW(DS_ui_row0);
        for (s16 i = 0; i < count; i++) {
            list_row(UIEL_S(e, UIEL_X0), UIEL_S(e, UIEL_Y0), texts[top + i], UIEL_S(e, UIEL_X1),
                     has_wear(wear, (s16)(top + i)), wear ? wear[top + i] : 0);
            e = (u16)(e + UIEL_BYTES);
        }
        if (!DSB(DS_ui_list_fill)) {
            u16 f = (u16)(DSW(DS_ui_row0) + UIEL_BYTES * sel);        /* (sic: not sel - top) */
            cols_set(scheme);
            draw_box1(g_back(), UIEL_S(f, UIEL_X0), UIEL_S(f, UIEL_Y0), w, 8);
        }
        cursor_ctl(-1);
        if (DSW(DS_g_mirror) != 0)
            page_copy_rect(g_back(), g_front(), (s16)(x0 - 1), (s16)(y0 - 1), (s16)(w + x0 + 1), (s16)(h + y0 + 1));

        /* ---- wait for input */
        for (;;) {
            cols_set(scheme);
            s16 r = ui_wait(1);
            if (r == 0) {                              /* hover highlight */
                s16 hov = DSS(DS_ui_hover);
                bool valid = hov >= 1 && n >= top + hov;
                if ((valid || prev >= 0) && hov - prev != 1) {
                    cursor_ctl(-3);
                    s16 bx0 = 1000, by0 = 1000, bx1 = -1, by1 = -1, idx = -1;
                    if (valid) {
                        idx = (s16)(hov - 1);
                        u16 el = (u16)(DSW(DS_ui_row0) + UIEL_BYTES * idx);
                        bx0 = UIEL_S(el, UIEL_X0);
                        by0 = UIEL_S(el, UIEL_Y0);
                        bx1 = UIEL_S(el, UIEL_X1);
                        u8 fg = DSB(DS_g_text_fg);
                        DSB(DS_g_text_fg) = hover_fg;
                        list_row(bx0, by0, texts[idx + top], bx1, has_wear(wear, (s16)(idx + top)),
                                 wear ? wear[idx + top] : 0);
                        DSB(DS_g_text_fg) = fg;
                        by1 = UIEL_S(el, UIEL_Y1);
                    }
                    if (prev >= 0) {
                        u16 el = (u16)(DSW(DS_ui_row0) + UIEL_BYTES * prev);
                        list_row(UIEL_S(el, UIEL_X0), UIEL_S(el, UIEL_Y0), texts[prev + top], UIEL_S(el, UIEL_X1),
                                 has_wear(wear, (s16)(prev + top)), wear ? wear[prev + top] : 0);
                        if (UIEL_S(el, UIEL_X0) < bx0) bx0 = UIEL_S(el, UIEL_X0);
                        if (UIEL_S(el, UIEL_Y0) < by0) by0 = UIEL_S(el, UIEL_Y0);
                        if (UIEL_S(el, UIEL_X1) > bx1) bx1 = UIEL_S(el, UIEL_X1);
                        if (UIEL_S(el, UIEL_Y1) > by1) by1 = UIEL_S(el, UIEL_Y1);
                    }
                    cursor_ctl(-1);
                    if (DSW(DS_g_mirror) != 0) page_copy_rect(g_back(), g_front(), bx0, by0, bx1, by1);
                    prev = idx;
                }
                continue;
            }
            if (r > 0) {                               /* a click on row r only selects it */
                if (r + top <= n) {
                    s16 nsel = (s16)(r + top - 1);
                    DSS(DS_ui_sel) = nsel;
                    if (field != 0) field_text(field, texts[nsel], wear, nsel);
                    if (!DSB(DS_ui_list_fill)) {
                        cursor_ctl(-3);
                        DSB(DS_g_text_bg) = scheme[1];                 /* erase the old frame in the background colour */
                        DSB(DS_g_line_col) = scheme[1];
                        u16 o = (u16)(DSW(DS_ui_row0) + UIEL_BYTES * sel);
                        s16 ox = UIEL_S(o, UIEL_X0), oy = UIEL_S(o, UIEL_Y0);
                        cols_set(scheme);
                        DSB(DS_g_line_col) = scheme[1];
                        draw_box1(g_back(), ox, oy, w, 8);
                        DSB(DS_g_line_col) = hover_fg;
                        u16 nw = (u16)(DSW(DS_ui_row0) + UIEL_BYTES * nsel);
                        draw_box1(g_back(), UIEL_S(nw, UIEL_X0), UIEL_S(nw, UIEL_Y0), w, 8);
                        cursor_ctl(-1);
                        if (DSW(DS_g_mirror) != 0) {
                            s16 nx = UIEL_S(nw, UIEL_X0), ny = UIEL_S(nw, UIEL_Y0);
                            s16 cx0 = (s16)(ox - 1), cy0 = (s16)(oy - 1), cx1 = (s16)(ox + w), cy1 = (s16)(oy + 8);
                            if (nx - 1 < cx0) cx0 = (s16)(nx - 1);
                            if (cx1 < nx + w + 2) cx1 = (s16)(nx + w + 2);
                            if (ny - 1 < cy0) cy0 = (s16)(ny - 1);
                            if (cy1 < ny + rowh + 2) cy1 = (s16)(ny + rowh + 2);
                            page_copy_rect(g_back(), g_front(), cx0, cy0, cx1, cy1);
                        }
                    }
                    sel = nsel;
                }
                continue;
            }
            /* a button */
            if (r == -10) {
                if (top + 1 < n) top = (s16)(top + 1);
            } else if (r == -9) {
                top = (s16)(top + rows);
                if (n <= top) top = (s16)(n - 1);
            } else if (r == -8) {
                top = (s16)(top - rows);
                if (top < 0) top = 0;
            } else if (r == -7) {
                if (top != 0) top = (s16)(top - 1);
            } else {
                ui_pop(1);
                return r;
            }
            break;                                     /* scrolled: redraw */
        }
    }
}

/* 1a21:55ea list_box */
s16 list_box(s16 screen, s16 sel, const u16 *texts, const s8 *wear, s16 n)
{
    if (n == 0) return 0x3EA;
    s16 x, y;
    FarPtr bg = ui_save_bg(screen, &x, &y, 2);
    s16 r = list_box_run(screen, sel, texts, wear, n);
    screen_put_bitmap_mirror(bg, x, y);
    arena_pop(1);
    return r;
}

/* 1a21:565c slot_list_edit: the save-game picker. Rows are -1 .. -n; a text field below is edited (edit_begin)
 * and takes the text of the picked slot. Buttons (< -n) end it. */
s16 slot_list_edit(s16 screen, s16 sel, const u16 *texts, s16 n)
{
    s16 prev = -1;
    ui_push(screen);
    s16 rows = DSS(DS_ui_rows);
    DSW(DS_ui_texts) = ds_offset_of(texts);
    DSS(DS_ui_sel) = sel;
    u16 row0 = DSW(DS_ui_row0);
    u16 field = DSW(DS_ui_field);
    s16 x0 = UIEL_S(row0, UIEL_X0), y0 = UIEL_S(row0, UIEL_Y0);
    s16 w = (s16)(UIEL_S(row0, UIEL_X1) - x0);
    s16 rowh = (s16)(UIEL_S(row0, UIEL_Y1) - y0);
    s16 yend = (s16)(y0 + rows * rowh);
    u8 scheme[5], fcols[5];
    cols_of(row0, scheme);
    cols_of(field, fcols);
    u8 hover_fg = scheme[3];
    cols_set(scheme);
    cursor_ctl(-3);
    s16 count = rows < n ? rows : n;
    u16 e = DSW(DS_ui_row0);
    for (s16 i = 0; i < count; i++) {
        list_row(UIEL_S(e, UIEL_X0), UIEL_S(e, UIEL_Y0), texts[i], UIEL_S(e, UIEL_X1), 0, 0);
        e = (u16)(e + UIEL_BYTES);
    }
    {
        u16 f = (u16)(DSW(DS_ui_row0) + UIEL_BYTES * sel);
        cols_set(scheme);
        draw_box1(g_back(), UIEL_S(f, UIEL_X0), UIEL_S(f, UIEL_Y0), w, 8);
    }
    cursor_ctl(-1);
    page_copy_rect(g_back(), g_front(), (s16)(x0 - 1), (s16)(y0 - 1), (s16)(x0 + w + 1), (s16)(yend + 1));
    cols_set(fcols);
    edit_begin(UIEL_S(field, UIEL_X0), UIEL_S(field, UIEL_Y0), ds_str(UIEL_W(field, UIEL_TEXT)),
               (s16)((UIEL_S(field, UIEL_X1) - UIEL_S(field, UIEL_X0)) / 6));
    cols_set(scheme);
    s16 blink = 0;

    for (;;) {
        s16 r;
        while ((r = ui_wait(1)) == 0) {
            s16 hov = DSS(DS_ui_hover);
            bool valid = hov < 0 && hov >= -n;
            s16 idx = (s16)(-1 - hov);
            if ((valid || prev >= 0) && idx != prev) {
                s16 bx0 = 1000, by0 = 1000, bx1 = -1, by1 = -1;
                cursor_ctl(-3);
                if (valid) {
                    u16 el = (u16)(DSW(DS_ui_row0) + UIEL_BYTES * idx);
                    bx0 = UIEL_S(el, UIEL_X0);
                    by0 = UIEL_S(el, UIEL_Y0);
                    bx1 = UIEL_S(el, UIEL_X1);
                    u8 fg = DSB(DS_g_text_fg);
                    DSB(DS_g_text_fg) = hover_fg;
                    list_row(bx0, by0, texts[idx], bx1, 0, 0);
                    DSB(DS_g_text_fg) = fg;
                    by1 = UIEL_S(el, UIEL_Y1);
                } else {
                    idx = -1;
                }
                if (prev >= 0) {
                    u16 el = (u16)(DSW(DS_ui_row0) + UIEL_BYTES * prev);
                    list_row(UIEL_S(el, UIEL_X0), UIEL_S(el, UIEL_Y0), texts[prev], UIEL_S(el, UIEL_X1), 0, 0);
                    if (UIEL_S(el, UIEL_X0) < bx0) bx0 = UIEL_S(el, UIEL_X0);
                    if (UIEL_S(el, UIEL_Y0) < by0) by0 = UIEL_S(el, UIEL_Y0);
                    if (UIEL_S(el, UIEL_X1) > bx1) bx1 = UIEL_S(el, UIEL_X1);
                    if (UIEL_S(el, UIEL_Y1) > by1) by1 = UIEL_S(el, UIEL_Y1);
                }
                cursor_ctl(-1);
                page_copy_rect(g_back(), g_front(), bx0, by0, bx1, by1);
                prev = idx;
            }
            blink++;
            if (blink == 0x23) {
                cols_set(fcols);
                edit_blink();
                blink = 0;
                cols_set(scheme);
            }
        }
        if (r >= 0) {                                  /* a key (0x3EB) */
            cols_set(scheme);
            if ((s16)DSW(DS_last_key) > 0) {
                cols_set(fcols);
                edit_key();
                DSS(DS_ui_hover) = (s16)(-1 - prev);
                cols_set(scheme);
            }
            continue;
        }
        if (r < -n) {                                  /* a button: "Save Game" / "Forget it" */
            cols_set(fcols);
            edit_end();
            ui_pop(1);
            return r;
        }
        s16 nsel = (s16)(-1 - r);
        DSS(DS_ui_sel) = nsel;
        cursor_ctl(-3);
        DSB(DS_g_text_bg) = scheme[1];
        DSB(DS_g_line_col) = scheme[1];
        u16 o = (u16)(DSW(DS_ui_row0) + UIEL_BYTES * sel);
        s16 ox = UIEL_S(o, UIEL_X0), oy = UIEL_S(o, UIEL_Y0);
        cols_set(scheme);
        DSB(DS_g_line_col) = scheme[1];
        draw_box1(g_back(), ox, oy, w, 8);
        DSB(DS_g_line_col) = hover_fg;
        u16 nw = (u16)(DSW(DS_ui_row0) + UIEL_BYTES * nsel);
        s16 nx = UIEL_S(nw, UIEL_X0), ny = UIEL_S(nw, UIEL_Y0);
        draw_box1(g_back(), nx, ny, w, 8);
        cursor_ctl(-1);
        {
            s16 cx0 = (s16)(ox - 1), cy0 = (s16)(oy - 1), cx1 = (s16)(ox + w), cy1 = (s16)(oy + 8);
            if (nx - 1 < cx0) cx0 = (s16)(nx - 1);
            if (cx1 < nx + w + 2) cx1 = (s16)(nx + w + 2);
            if (ny - 1 < cy0) cy0 = (s16)(ny - 1);
            if (cy1 < ny + rowh + 2) cy1 = (s16)(ny + rowh + 2);
            page_copy_rect(g_back(), g_front(), cx0, cy0, cx1, cy1);
        }
        cols_set(fcols);
        for (s16 k = 0; k < w; k++) fill_rect(g_back(), (s16)(k + UIEL_S(field, UIEL_X0)), UIEL_S(field, UIEL_Y0), 6, 8);
        strcpy(ds_str(UIEL_W(field, UIEL_TEXT)), ds_str(texts[nsel]));
        DSS(DS_ui_sel) = nsel;
        edit_begin(UIEL_S(field, UIEL_X0), UIEL_S(field, UIEL_Y0), ds_str(UIEL_W(field, UIEL_TEXT)),
                   (s16)((UIEL_S(field, UIEL_X1) - UIEL_S(field, UIEL_X0)) / 6));
        page_copy_rect(g_back(), g_front(), (s16)(x0 - 1), (s16)(y0 - 1), (s16)(x0 + w + 1), (s16)(yend + 1));
        cols_set(scheme);
        sel = nsel;
    }
}
