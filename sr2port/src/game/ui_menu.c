/* The SR2 UI toolkit: screens made of 18-byte elements (garage.md "UI toolkit"): drawing, activation
 * (ui_push / ui_pop), hot spots, ui_menu / ui_dialog. 1a21:4284 - 4d7c, 1a21:4cf4 / 4d26, 0001:0e74. */
#include "game/ui.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <string.h>

#include "platform/platform.h"
#include "platform/video.h"

/* 1a21:4284 ui_bounds: bounding box of ALL elements of a screen (disabled ones included), 3 px margin. */
void ui_bounds(s16 screen, s16 *x, s16 *y, s16 *w, s16 *h)
{
    u16 e = ui_screen(screen);
    DSS(DS_ui_min_y) = DSS(DS_ui_min_x) = 1000;
    DSS(DS_ui_max_y) = DSS(DS_ui_max_x) = 1;
    for (;;) {
        if (UIEL_S(e, UIEL_X0) < DSS(DS_ui_min_x)) DSS(DS_ui_min_x) = UIEL_S(e, UIEL_X0);
        if (UIEL_S(e, UIEL_X1) > DSS(DS_ui_max_x)) DSS(DS_ui_max_x) = UIEL_S(e, UIEL_X1);
        if (UIEL_S(e, UIEL_Y0) < DSS(DS_ui_min_y)) DSS(DS_ui_min_y) = UIEL_S(e, UIEL_Y0);
        if (UIEL_S(e, UIEL_Y1) > DSS(DS_ui_max_y)) DSS(DS_ui_max_y) = UIEL_S(e, UIEL_Y1);
        u16 last = DSB(e);
        e = (u16)(e + UIEL_BYTES);
        if (last & UIF_LAST) break;
    }
    *x = (s16)(DSS(DS_ui_min_x) - 3);
    if (*x < 0) *x = 0;
    *y = (s16)(DSS(DS_ui_min_y) - 3);
    if (*y < 0) *y = 0;
    *w = (s16)(DSS(DS_ui_max_x) + 3);
    if (*w > 0x140) *w = 0x140;
    *h = (s16)(DSS(DS_ui_max_y) + 3);
    if (*h > 200) *h = 200;
    *w = (s16)(*w - *x);
    *h = (s16)(*h - *y);
}

/* 1a21:434a ui_save_bg: the screen's rectangle, byte-aligned on the left and right, into a new arena
 * bitmap (kind 1 = low arena, freed with arena_pop_low; 2 = high arena, arena_pop). *x stays unaligned. */
FarPtr ui_save_bg(s16 screen, s16 *x, s16 *y, s16 kind)
{
    s16 w, h;
    ui_bounds(screen, x, y, &w, &h);
    s16 top = *y;
    s16 right = (s16)(((s16)~DSS(DS_g_x_align) + *x + w) & DSS(DS_g_x_align));
    if (right > 0x140) right = 0x140;
    s16 x0 = (s16)(DSS(DS_g_x_align) & *x);
    s16 bottom = (s16)(top + h);
    if (bottom > 200) bottom = 200;
    FarPtr bmp = arena_bitmap_alloc((s16)(right - x0), (s16)(bottom - top), 0, kind);
    return screen_save_rect(x0, top, (s16)(right - x0), (s16)(bottom - top), bmp);
}

/* 1a21:43dc hotspot_find: the active element under (x, y) (elements are sorted by x0), and the rectangle
 * around (x, y) in which the answer stays the same (DS:B87E / B880 / B882 / B884 = x0, x1, y0, y1). Sets
 * DS:B896 = the element, DS:B898 = its code (0 = none); returns the code. */
s16 hotspot_find(s16 x, s16 y)
{
    u16 e = DSW(DS_ui_cur);
    if (e == 0) {
        DSS(DS_ui_hot_y0) = DSS(DS_ui_hot_x0) = -1;
        DSS(DS_ui_hot_y1) = DSS(DS_ui_hot_x1) = 1000;
        return DSS(DS_ui_hover) = 0;                      /* (DS:B896 is left alone) */
    }
    DSS(DS_ui_hot_y0) = DSS(DS_ui_hot_x0) = 0;
    DSS(DS_ui_hot_x1) = 0x13F;
    DSS(DS_ui_hot_y1) = 199;
    for (;;) {
        u16 fl = DSW(e);
        if (fl & UIF_ACTIVE) {
            if (UIEL_S(e, UIEL_X0) > x) {                     /* the list is ordered by x0 */
                DSS(DS_ui_hot_x1) = UIEL_S(e, UIEL_X0);
                break;
            }
            if (UIEL_S(e, UIEL_X1) > x) {
                if (UIEL_S(e, UIEL_Y0) > y) {
                    if (UIEL_S(e, UIEL_Y0) < DSS(DS_ui_hot_y1)) DSS(DS_ui_hot_y1) = UIEL_S(e, UIEL_Y0);
                } else if (UIEL_S(e, UIEL_Y1) > y) {          /* hit */
                    DSS(DS_ui_hot_x0) = UIEL_S(e, UIEL_X0);
                    DSS(DS_ui_hot_x1) = UIEL_S(e, UIEL_X1);
                    DSS(DS_ui_hot_y0) = UIEL_S(e, UIEL_Y0);
                    DSS(DS_ui_hot_y1) = UIEL_S(e, UIEL_Y1);
                    DSW(DS_ui_hit) = e;
                    return DSS(DS_ui_hover) = UIEL_S(e, UIEL_CODE);
                } else if (UIEL_S(e, UIEL_Y1) > DSS(DS_ui_hot_y0)) {
                    DSS(DS_ui_hot_y0) = UIEL_S(e, UIEL_Y1);
                }
            } else if (UIEL_S(e, UIEL_X1) > DSS(DS_ui_hot_x0)) {
                DSS(DS_ui_hot_x0) = UIEL_S(e, UIEL_X1);
            }
        }
        e = (u16)(e + UIEL_BYTES);
        if (fl & UIF_LAST) break;
    }
    DSW(DS_ui_hit) = 0;
    return DSS(DS_ui_hover) = 0;
}

/* 1a21:44a8 draw_element: one element on the back page; 1 if it counts for the bounding box. */
s16 draw_element(u16 el)
{
    FarPtr page = g_back();
    s16 x0 = UIEL_S(el, UIEL_X0), y0 = UIEL_S(el, UIEL_Y0), x1 = UIEL_S(el, UIEL_X1), y1 = UIEL_S(el, UIEL_Y1);
    u16 text = UIEL_W(el, UIEL_TEXT);

    if (UIEL_S(el, UIEL_SCHEME) > 0)                    /* DS:AD28.. = scheme record (5 bytes) */
        memmove(mp(DGROUP, DS_g_text_fg), mp(DGROUP, (u16)(DSW(DS_SCHEME) + 5 * UIEL_S(el, UIEL_SCHEME))), 5);

    switch (DSW(el) & 0xFFD8) {                         /* bits 0x01, 0x02, 0x04, 0x20 do not matter */
    case UIF_PANEL:
    case UIF_FRAME: {
        s16 w = (s16)(x1 - x0), h = (s16)(y1 - y0);
        fill_rect(page, x0, y0, w, h);
        draw_frame3(page, x0, y0, w, h);
        if (text) {
            drv_draw_text(page, (s16)(x0 + ((w - font_string_width(ds_str(text))) >> 1)), y0, ds_str(text));
            hline(page, x0, (s16)(y0 + 9), w);
        }
        return 1;
    }
    case UIF_BUTTON: {
        s16 x = (s16)(x0 + 1), y = (s16)(y0 + 1);
        s16 w = (s16)(x1 - x0 - 2), h = (s16)(y1 - y0 - 2);
        fill_rect(page, x, y, w, h);
        draw_frame3(page, x, y, w, h);
        u16 s;
        switch (UIEL_S(el, UIEL_CODE)) {                /* arrow glyphs of the UI font */
        case -10: s = DS_glyph_down; break;
        case -9:  s = DS_glyph_pgdn; break;
        case -8:  s = DS_glyph_pgup; break;
        case -7:  s = DS_glyph_up; break;
        default:  s = text; break;
        }
        drv_draw_text(page, (s16)(x + ((w - font_string_width(ds_str(s))) >> 1)), (s16)(y + ((h - 8) >> 1)), ds_str(s));
        return 1;
    }
    case UIF_TEXT:
        drv_draw_text(page, x0, y0, ds_str(text));
        return 1;
    case UIF_CTEXT:
        drv_draw_text(page, (s16)(x0 + ((x1 - font_string_width(ds_str(text)) - x0) >> 1)),
                      (s16)(y0 + ((y1 - y0 - 8) >> 1)), ds_str(text));
        return 1;
    case UIF_EDIT:
        DSB(DS_ui_edit_mode) = 1;
        /* an edit field is a title field that is not drawn: */
        __attribute__((fallthrough));
    case UIF_TITLE:
        if (DSB(DS_ui_edit_mode) == 0) {
            s16 w = (s16)(x1 - x0);
            fill_rect(page, x0, y0, w, 8);
            drv_draw_text(page, (s16)(x0 + ((w - font_string_width(ds_str(text))) >> 1)), y0, ds_str(text));
        }
        if (DSW(DS_ui_field) == 0) DSW(DS_ui_field) = el;
        return DSB(DS_ui_edit_mode) == 0;
    case UIF_ROW:
        if (DSS(DS_ui_rows)++ == 0) DSW(DS_ui_row0) = el;
        return 0;
    default:
        return 0;
    }
}

/* 1a21:473e draw_screen: every element to the one with UIF_LAST; the drawn ones widen DS:8824.. (min x, min y,
 * max x, max y), which the caller initialises. */
void draw_screen(u16 els)
{
    u16 e = els;
    u16 last;
    do {
        if (draw_element(e)) {
            if (UIEL_S(e, UIEL_X0) < DSS(DS_ui_min_x)) DSS(DS_ui_min_x) = UIEL_S(e, UIEL_X0);
            if (UIEL_S(e, UIEL_X1) > DSS(DS_ui_max_x)) DSS(DS_ui_max_x) = UIEL_S(e, UIEL_X1);
            if (UIEL_S(e, UIEL_Y0) < DSS(DS_ui_min_y)) DSS(DS_ui_min_y) = UIEL_S(e, UIEL_Y0);
            if (UIEL_S(e, UIEL_Y1) > DSS(DS_ui_max_y)) DSS(DS_ui_max_y) = UIEL_S(e, UIEL_Y1);
        }
        last = DSB(e);
        e = (u16)(e + UIEL_BYTES);
    } while (!(last & UIF_LAST));
}

/* 1a21:4798 ui_redraw_element */
void ui_redraw_element(u16 el)
{
    if (DSW(DS_g_mirror) != 0) cursor_ctl(-3);
    draw_element(el);
    if (DSW(DS_g_mirror) != 0) {
        cursor_ctl(-1);
        page_copy_rect(g_back(), g_front(), (s16)(UIEL_S(el, UIEL_X0) - 3), (s16)(UIEL_S(el, UIEL_Y0) - 3),
                       (s16)(UIEL_S(el, UIEL_X1) + 6), (s16)(UIEL_S(el, UIEL_Y1) + 6));
    }
}

/* 1a21:4b96 ui_push: draw screen `screen` (back page, mirrored to the front page when DS:AD26) and make it the
 * current menu. */
void ui_push(s16 screen)
{
    if (getenv("SR_TRACE")) fprintf(stderr, "trace: %ldms ui_push screen %d\n", (long)clock(), screen);   /* PORT: developer aid */
    u16 saved_present = DSW(DS_g_mirror);
    cursor_ctl(-3);
    DSS((u16)(DS_ui_stack + 2 * DSS(DS_ui_depth))) = screen;
    DSS(DS_ui_depth)++;                                  /* no limit check in SR2 */
    DSS(DS_ui_hot_y1) = DSS(DS_ui_hot_x1) = DSS(DS_ui_hot_y0) = DSS(DS_ui_hot_x0) = -1;
    u16 els = ui_screen(screen);
    DSW(DS_ui_cur) = els;
    DSW(DS_ui_row0) = 0;
    DSW(DS_ui_field) = 0;
    DSB(DS_ui_edit_mode) = 0;
    DSS(DS_ui_rows) = 0;
    DSB(DS_ui_list_fill) = (u8)(DSB(els) & UIF_FILL);
    u8 colours[5];
    memcpy(colours, mp(DGROUP, DS_g_text_fg), 5);
    DSS(DS_ui_min_y) = DSS(DS_ui_min_x) = 1000;
    DSS(DS_ui_max_y) = DSS(DS_ui_max_x) = -1;
    DSW(DS_g_mirror) = 0;
    draw_screen(els);
    memcpy(mp(DGROUP, DS_g_text_fg), colours, 5);
    cursor_ctl(-1);
    DSW(DS_g_mirror) = saved_present;
    if (saved_present && DSS(DS_ui_max_x) >= 0)
        page_copy_rect(g_back(), g_front(), (s16)(DSS(DS_ui_min_x) - 3), (s16)(DSS(DS_ui_min_y) - 3),
                       (s16)(DSS(DS_ui_max_x) + 6), (s16)(DSS(DS_ui_max_y) + 6));
    kbd_hotspot_refresh();
    click_clear();
    if (getenv("SR_TRACE") && getenv("SR_TRACE")[0] == '2') {      /* PORT: developer aid: the live element rectangles */
        for (u16 e = els;; e = (u16)(e + UIEL_BYTES)) {
            fprintf(stderr, "  el code %d (%d,%d)-(%d,%d)\n", UIEL_S(e, UIEL_CODE), UIEL_S(e, UIEL_X0), UIEL_S(e, UIEL_Y0),
                    UIEL_S(e, UIEL_X1), UIEL_S(e, UIEL_Y1));
            if (DSW(e) & 2) break;
        }
    }
    if (DSW(DS_demo) != 0 && screen != 4) demo_step();
}

/* 1a21:4cac ui_pop: drop the top screen; all != 0 draws the one below again. */
void ui_pop(s16 all)
{
    kbd_remap_restore(0);
    DSW(DS_ui_cur) = 0;
    DSS(DS_ui_depth)--;
    if (DSS(DS_ui_depth) != 0 && all != 0) {
        DSS(DS_ui_depth)--;
        ui_push(DSS((u16)(DS_ui_stack + 2 * DSS(DS_ui_depth))));
        return;
    }
    DSS(DS_ui_hot_y1) = DSS(DS_ui_hot_x1) = DSS(DS_ui_hot_y0) = DSS(DS_ui_hot_x0) = -1;
}

/* 1a21:4d7c ui_sort_screen: selection sort of the elements by (x0, y0); the LAST flag stays on the final record. */
void ui_sort_screen(s16 screen)
{
    u16 a = ui_screen(screen);
    while (!(DSB(a) & UIF_LAST)) {
        u16 c = a;
        for (;;) {
            c = (u16)(c + UIEL_BYTES);
            s16 ax = UIEL_S(a, UIEL_X0), cx = UIEL_S(c, UIEL_X0);
            if (ax > cx || (ax == cx && UIEL_S(a, UIEL_Y0) > UIEL_S(c, UIEL_Y0))) {
                u8 tmp[UIEL_BYTES];
                memcpy(tmp, mp(DGROUP, a), UIEL_BYTES);
                memcpy(mp(DGROUP, a), mp(DGROUP, c), UIEL_BYTES);
                memcpy(mp(DGROUP, c), tmp, UIEL_BYTES);
            }
            if (DSB(c) & UIF_LAST) break;
            if (DSB(a) & UIF_LAST) {                    /* the swap carried the LAST flag to the anchor */
                DSB(a) &= (u8)~UIF_LAST;
                DSB(c) |= UIF_LAST;
                break;
            }
        }
        a = (u16)(a + UIEL_BYTES);
    }
    kbd_hotspot_refresh();
}

/* 1a21:4cf4 ui_menu */
s16 ui_menu(s16 screen)
{
    ui_push(screen);
    s16 r;
    do r = ui_wait(0x3E80); while (r == 0);
    ui_pop(1);
    if (getenv("SR_TRACE")) fprintf(stderr, "trace: %ldms ui_menu(%d) -> %d\n", (long)clock(), screen, r);
    return r;
}

/* 1a21:4d26 ui_dialog */
s16 ui_dialog(s16 screen)
{
    s16 x, y;
    FarPtr bg = ui_save_bg(screen, &x, &y, 2);
    s16 r = ui_menu(screen);
    screen_put_bitmap_mirror(bg, x, y);
    arena_pop(1);
    return r;
}

/* 0001:0e74 hotspots_reset (classifieds close, platform range): arena_clear, DS:1ECC / 1ECE / 1ED0 = 0, ui_pop(1). */
void hotspots_reset(void)
{
    arena_clear();
    DSW(DS_page_list) = 0;
    DSW(DS_page_paper) = 0;
    DSW((u16)(DS_page_paper + 2)) = 0;
    ui_pop(1);
}
