/* Garage: the newspaper and the classified ads (garage.md 0001:0a68 - 11a2, 3e30). The weekly rolling list of
 * car ads (ids 0..20 new, 21..41 used copies) and the 64-part catalogue shop. State lives in the save block
 * DS:9B40 (game_flow saves it). cheapest_ad_price 0001:0e94 and hotspots_reset 0001:0e74 are game_flow's / the UI
 * slice's. */
#include "game/garage.h"

#include <stdlib.h>
#include <stdio.h>
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

/* The 5 colour bytes of the colour scheme at +off into DS:AD28 (text fg, text bg, fill, line, frame). */
static void scheme_colours(u16 off)
{
    u16 sch = DSW(DS_g_scheme);
    for (u16 i = 0; i < 5; i++) DSB((u16)(DS_g_text_fg + i)) = DSB((u16)(sch + off + i));
}

/* sprintf(strchr(text, '$') + 1, fmt, v) on a DGROUP text. */
static void patch_price(u16 text, u16 fmt, s16 v)
{
    char *p = strchr(ds_str(text), '$');
    if (p) sprintf(p + 1, ds_str(fmt), (int)v);
}

/* ============================================================================== 0001:0a68 */

/* Prepares the page: list = DS offset of the line-count array (list[0] = rows, list[i] = text lines of row i). */
void classifieds_open(u16 list, s16 first_row)
{
    arena_reset();
    DSW(DS_page_list) = list;
    scheme_colours(0x41);
    DSW(DS_PAGE_RECT) = 0x40;
    DSW((u16)(DS_PAGE_RECT + 2)) = 0xBE;
    DSW((u16)(DS_PAGE_RECT + 0xA)) = 0;
    DSW((u16)(DS_PAGE_RECT + 8)) = 0;
    DSW((u16)(DS_PAGE_RECT + 6)) = 0;
    DSW((u16)(DS_PAGE_RECT + 4)) = 0;
    DSW(DS_page_first_row) = (u16)first_row;
    DSB(DS_page_redraw) = 1;
    ds_far_wr(DS_page_paper, far_make(0, 0));
    ui_push(4);
}

/* ============================================================================== 0001:0ac8 */

/* Draws the page when needed and waits. Returns -3 (Go to the garage) or the 1-based row of the clicked ad. */
s16 classifieds_run(void)
{
    u16 list = DSW(DS_page_list);
    s16 total = DSS(list);                          /* N rows */
    for (;;) {
        if (DSB(DS_page_redraw) != 0) {
            cursor_ctl(-4);
            if (far_is_null(ds_far(DS_page_paper))) ds_far_wr(DS_page_paper, pic_get(0x3F5, 3));   /* paper, LIB2 #13 */
            Rect *rc = (Rect *)mp(DGROUP, DS_PAGE_RECT);
            rc->dx = 0;
            rc->sx = 0;
            drv_blit(ds_far(DS_page_paper), g_back(), rc, 1);                 /* left margin strip */
            rc->sx = 0x40;
            rc->dx = 0x100;
            drv_blit(ds_far(DS_page_paper), g_back(), rc, 1);                 /* right margin strip */
            scheme_colours(0x41);
            FarPtr bk = g_back();
            u16 save_h = rd16(bk.seg, (u16)(bk.off + DESC_H));
            wr16(bk.seg, (u16)(bk.off + DESC_H), 0xBE);
            fill_rect(g_back(), 0x40, 0, 0xC0, 0xBE);
            s16 y = 0;
            s16 row = (s16)DSW(DS_page_first_row);
            u16 el = DSW(DS_menu_elems);                                       /* element array of screen 4 */
            for (;;) {
                s16 nl = DSS((u16)(list + 2 * row));
                u16 text = DSW((u16)(DS_list_entries + 2 * row));
                hline(g_back(), 0x44, (s16)(y + 1), 0xB8);                    /* separator above the ad */
                y = (s16)(y + 3);
                if (y > 0xBD) break;                                           /* the next ad does not fit */
                el = (u16)(el + UIEL_BYTES);
                DSW((u16)(el + UIEL_Y0)) = (u16)(y - 2);
                DSW((u16)(el + UIEL_FLAGS)) = 1;
                DSW((u16)(el + UIEL_CODE)) = (text == DSW(DS_AD_HDR_CARS) || text == DSW(DS_AD_HDR_PARTS)) ? 0 : (u16)row;
                bool patch = DSS((u16)(el + UIEL_CODE)) > 0 && row < (s16)DSW(DS_ad_part_row);   /* car ads: '$' gets the price */
                s16 price = DSS((u16)(DS_ad_price + 2 * row));
                for (s16 i = 0; i < nl; i++) {
                    s16 x = 0x44;
                    if (patch) patch_price(text, DS_FMT_AD_PRICE, price);
                    for (;;) {                                                 /* one line = segments joined by '+' */
                        drv_draw_text(g_back(), x, y, ds_str(text));
                        text = (u16)(text + strlen(ds_str(text)) + 1);
                        if (DSB(text) == 0) text++;                            /* skip an empty string */
                        if (DSB(text) != '+') break;
                        if (patch) patch_price(text, DS_FMT_AD_PRICE2, price);
                        text++;
                        x = DSS(DS_g_text_end_x);                             /* continues after the text just drawn */
                    }
                    y = (s16)(y + 9);
                    if (y > 0xBD) break;                                       /* clipped */
                }
                s16 y1 = (s16)(y + 1);
                if (y1 > 0xBD) y1 = 0xBD;
                DSW((u16)(el + UIEL_Y1)) = (u16)y1;
                s16 cur = row;
                row++;
                if (cur >= total) row = 1;                                     /* the list wraps around */
                if (y >= 0xBD) break;
            }
            DSW(DS_page_last_row) = (u16)(row == 1 ? total : row - 1);
            s16 si = (s16)(y + 1);
            s16 y1 = si > 0xBD ? 0xBD : si;
            DSW((u16)(el + UIEL_Y1)) = (u16)y1;                                /* closing separator */
            if (y1 < 0xBD) hline(g_back(), 0x44, si, 0xB8);
            el = (u16)(el + UIEL_BYTES);
            while (DSS((u16)(el + UIEL_CODE)) >= 0) {                          /* unused rows, up to "Next page" */
                DSW((u16)(el + UIEL_FLAGS)) = 0;
                el = (u16)(el + UIEL_BYTES);
            }
            DSB(DS_page_redraw) = 0;
            if (getenv("SR_TRACE"))                                            /* PORT: developer aid */
                for (u16 e = DSW(DS_menu_elems) + UIEL_BYTES; DSS((u16)(e + UIEL_CODE)) >= 0; e = (u16)(e + UIEL_BYTES))
                    if (DSW(e) & 1)
                        fprintf(stderr, "  ad row %d y %d-%d price %d\n", DSS((u16)(e + UIEL_CODE)), DSS((u16)(e + UIEL_Y0)),
                                DSS((u16)(e + UIEL_Y1)), DSS((u16)(DS_ad_price + 2 * DSS((u16)(e + UIEL_CODE)))));
            wr16(bk.seg, (u16)(bk.off + DESC_H), save_h);
            cursor_ctl(-1);
            if (DSW(DS_g_mirror) != 0) page_copy_rect(g_back(), g_front(), 0, 0, 0x140, 0xBE);
            click_clear();
        }
        s16 r;
        do r = ui_wait(3000); while (r == 0);
        if (r == -3) return -3;
        s16 first;
        if (r == -2) {                                                         /* Next page: restarts at the LAST row shown */
            first = (s16)DSW(DS_page_last_row);
        } else if (r == -1) {                                                  /* Previous page: walk back a page's worth */
            first = (s16)(DSW(DS_page_first_row) - 1);
            if (first < 1) first = total;
            for (s16 room = 0xBD;;) {
                room = (s16)(room - (9 * DSS((u16)(list + 2 * first)) + 3));
                if (room <= 0) break;
                first--;
                if (first < 1) first = total;
            }
        } else {                                                               /* an ad was clicked: redrawn next time */
            DSB(DS_page_redraw) = 1;
            return r;
        }
        DSW(DS_page_first_row) = (u16)first;
        DSB(DS_page_redraw) = 1;
    }
}

/* ============================================================================== 0001:0efe .. 112e */

/* Row `row` of the lists = car ad `id` (0..20 new at the dealer price, 21..41 the same model used at the trade-in price). */
static void classifieds_add_car_row(s16 id, s16 row, u8 *item)
{
    *item = (u8)id;
    s16 m = id;
    bool used = false;
    if ((s8)id >= 0x15) {
        m = (s16)(m - 0x15);
        used = true;
    }
    DSW((u16)(DS_list_entries + 2 * row)) = model_ad_ptr(m);
    DSS((u16)(DS_ad_lines + 2 * row)) = (s8)model_ad_lines(m);
    DSW((u16)(DS_ad_price + 2 * row)) = used ? (u16)model_trade_in(m) : (u16)model_price(m);
}

/* 0001:0f7e: this week's ads (the week changed): 15..25 ids from the pool of ids with a zero counter. Returns the count. */
static s16 classifieds_roll_ads(s16 row, u8 *items)
{
    s16 total = (s16)(rnd(11) + 15);
    s16 want = total;
    s16 pool = DSS(DS_ad_pool);
    if (!(total < pool)) {                          /* pool too small: take every available id, age the others */
        want = (s16)(total - pool);
        DSW(DS_ad_pool) = 0;
        for (s16 id = 0; id < 42; id++) {
            u16 cool = (u16)(DS_ad_cool + id);
            if (DSB(cool) == 0) {
                DSB(cool)++;
                DSB((u16)(DS_AD_WEEK + row)) = (u8)id;
                classifieds_add_car_row(id, row, items);
                row++;
                items++;
            } else if (--DSB(cool) == 0) {
                DSW(DS_ad_pool)++;
            }
        }
    }
    while (want > 0) {                              /* random ids from the pool */
        s16 k = rnd((s16)DSW(DS_ad_pool));
        s16 i = 0;
        if (k >= 0) {                               /* i - 1 = the k-th id with a zero counter */
            s16 zeros = 0;
            do {
                if (DSB((u16)(DS_ad_cool + i)) == 0) zeros++;
                i++;
            } while (zeros <= k);
        }
        s16 id = (s16)(i - 1);
        DSB((u16)(DS_ad_cool + id))++;
        DSB((u16)(DS_AD_WEEK + row)) = (u8)id;
        classifieds_add_car_row(id, row, items);
        DSW(DS_ad_pool)--;
        row++;
        items++;
        want--;
    }
    return total;
}

/* 0001:1048: the same week, rows from the saved ids (the 0x80 sold flag stays in DS:9B42). */
static s16 classifieds_rebuild_ads(s16 row, u8 *items)
{
    s16 k = 0;
    while (DSB((u16)(DS_ad_ids + k)) != 0xFF) {
        classifieds_add_car_row(DSB((u16)(DS_ad_ids + k)) & 0x7F, row, items);
        row++;
        items++;
        k++;
    }
    return k;
}

/* Rows row.. = catalogue parts first..last (text pointer, lines, item = catalogue index). */
static void classifieds_add_parts(s16 row, u8 *items, s16 first, s16 last)
{
    for (s16 t = first; t <= last; t++) {
        DSW((u16)(DS_list_entries + 2 * row)) = parts_text(t);
        DSS((u16)(DS_ad_lines + 2 * row)) = (s8)parts_lines(t);
        *items++ = (u8)t;
        row++;
    }
}

/* 0001:107c: the 9 engines */
static s16 classifieds_add_engines(s16 row, u8 *items)
{
    classifieds_add_parts(row, items, 0, 8);
    return 9;
}

/* 0001:10b6: 12 transmissions (4 grades x 3 makes, automatic first) */
static s16 classifieds_add_transmissions(s16 row, u8 *items)
{
    classifieds_add_parts(row, items, 9, 20);
    return 12;
}

/* 0001:10f2: 26 manifolds */
static s16 classifieds_add_manifolds(s16 row, u8 *items)
{
    classifieds_add_parts(row, items, 24, 49);
    return 26;
}

/* 0001:112e: the 3 carburettors, then tires, exhaust manifolds, mufflers and differentials (indices 50..63) */
static s16 classifieds_add_misc_parts(s16 row, u8 *items)
{
    classifieds_add_parts(row, items, 21, 23);
    classifieds_add_parts((s16)(row + 3), items + 3, 50, 63);
    return 17;
}

/* ============================================================================== 0001:11a2 */

/* The "See it" strip of a car ad (cars only): the model drawn static over the newspaper, click anywhere to leave. */
static void see_it_preview(s16 m)
{
    cursor_ctl(-4);
    arena_reset();
    FarPtr bg = arena_bitmap_alloc(0x140, 0x52, 0, 0);
    screen_save_rect(0, 0x40, 0x140, 0x52, bg);
    scheme_colours(0x37);
    s16 colour = (s8)model_b6(m);                   /* the fixed preview colour rolled when SR2_DATA was loaded */
    set_paint_palette(colour);
    bool ega = DSS(DS_driver_id) == -2 || DSS(DS_driver_id) == -6;
    s16 bgcol;
    if (ega) {
        bgcol = (s8)DSB((u16)(DS_PAINT + 3 * colour + 2));
        DSS((u16)(DS_LOC_COLOUR_EGA + 10)) = bgcol;                           /* DS:1C04 */
    } else {
        bgcol = DSS((u16)(DS_LOC_COLOUR + 10));                               /* DS:1BF6 */
    }
    screen_fill_rect(0, 0x41, 0x140, 0x46, (u8)bgcol);
    screen_fill_rect(0, 0x87, 0x140, 10, DSB(DS_g_text_bg));
    if (!ega) {
        u8 lc = DSB(DS_g_line_col);
        DSB(DS_g_line_col) = 3;
        vline(g_back(), 0, 0x40, 0x52);
        vline(g_back(), 0x13F, 0x40, 0x52);
        vline(g_front(), 0, 0x40, 0x52);
        vline(g_front(), 0x13F, 0x40, 0x52);
        screen_hline(0, 0x40, 0x140);
        screen_hline(0, 0x91, 0x140);
        DSB(DS_g_line_col) = lc;
    }
    u16 specA = model_specA(m);
    u16 flags = (((specA >> 8) & 0x3C) == 0x20) ? 8 : 0;                       /* supercharger visible */
    car_draw(7, m, flags, 0, 0x86, (u16)(DS_TYRE_PICS + ((specA & 0x30) >> 1)));
    u16 text = model_ad_ptr(m);
    s16 w = font_string_width(ds_str(text));
    screen_text((s16)((s16)(0x140 - w) / 2), 0x88, ds_str(text));
    cursor_ctl(-2);
    DSW(DS_wait_any_key) = 1;
    ui_menu(6);                                                                /* screen 6: click anywhere */
    DSW(DS_wait_any_key) = 0;
    screen_put_bitmap_mirror(bg, 0, 0x40);
    arena_reset();
}

/* 0001:11a2: section 1 Used Cars, 2 Auto Parts; reroll != 0 starts a new week of ads. */
void classifieds(s16 section, s16 reroll)
{
    u8 items[96];                                   /* car ids, then catalogue indices (parts: items[row - 3]) */
    DSW(DS_location) = 5;
    DSW(DS_AD_ROW1_LINES) = 3;
    DSW((u16)(DS_list_entries + 2)) = DSW(DS_AD_HDR_CARS);
    s16 n;
    if (reroll == 0) {
        n = classifieds_rebuild_ads(2, items);
    } else {
        memset(mp(DGROUP, DS_ad_ids), 0xFF, 26);
        n = classifieds_roll_ads(2, items);
    }
    DSW((u16)(DS_ad_lines + 2 * (n + 2))) = 3;
    DSW((u16)(DS_list_entries + 2 * (n + 2))) = DSW(DS_AD_HDR_PARTS);
    DSW(DS_ad_part_row) = (u16)(n + 3);
    n = (s16)(n + classifieds_add_engines((s16)DSW(DS_ad_part_row), items + n));
    n = (s16)(n + classifieds_add_transmissions((s16)(n + 3), items + n));
    n = (s16)(n + classifieds_add_manifolds((s16)(n + 3), items + n));
    n = (s16)(n + classifieds_add_misc_parts((s16)(n + 3), items + n));
    DSS(DS_ad_lines) = (s16)(n + 2);
    classifieds_open(DS_ad_lines, section == 1 ? 1 : (s16)(DSW(DS_ad_part_row) - 1));   /* page starts at the section header */

    for (;;) {
        s16 r = classifieds_run();
        if (r == -3) break;
        u16 see = DSW((u16)(DS_SCREENS + 2 * 5));                              /* screen 5: the buy dialog */
        while (DSS((u16)(see + UIEL_CODE)) != -25) see = (u16)(see + UIEL_BYTES);   /* "See it" */
        bool is_car = r < (s16)DSW(DS_ad_part_row);
        s16 id = 0, m = 0, t = 0;
        bool used = false;
        u16 text;
        s16 price;
        if (is_car) {
            if (DSB((u16)(DS_ad_ids + r - 2)) & 0x80) {
                msg_box(119);                                                  /* Sold! */
                continue;
            }
            id = items[r - 2];
            used = id >= 0x15;
            m = used ? (s16)(id - 0x15) : id;
            text = model_ad_ptr(m);
            price = used ? model_trade_in(m) : model_price(m);
            DSW((u16)(see + UIEL_FLAGS)) = 0x11;
        } else {
            t = items[r - 3];
            text = parts_text(t);
            price = parts_price(t);
            DSW((u16)(see + UIEL_FLAGS)) = 0;                                  /* "See it" greyed out */
        }
        while (!(DSB((u16)(see + UIEL_FLAGS + 1)) & 1)) see = (u16)(see + UIEL_BYTES);   /* the centred title line */
        DSW((u16)(see + UIEL_TEXT)) = text;
        see = (u16)(see + UIEL_BYTES);
        patch_price(DSW((u16)(see + UIEL_TEXT)), DS_FMT_BUY, price);           /* "You want to buy it for $" */
        s16 b;
        bool again = false;
        while ((b = ui_menu(5)) != -20) {                                      /* -25 See it, -20 Yeah, -21 Forget it */
            if (b != -25) {
                again = true;
                break;
            }
            see_it_preview(m);
        }
        if (again) continue;
        if (DSSL(DS_money) < (s32)price) {
            patch_price(shop_msg_line(1), DS_FMT_SHORT, (s16)(price - (s16)DSW(DS_money)));   /* "No go, hot shot. You're $.. short." */
            msg_box(-1);                                                       /* a negative id: the box stays on screen */
            continue;
        }
        money_add((s16)-price);
        if (is_car) {
            u16 c = car_new(m, price, 0);
            DSB((u16)(c + CR_COLOUR)) = model_b6(m);                           /* the colour "See it" showed */
            if (used) {                                                        /* second-hand: 60-90 % worn */
                DSW((u16)(DSW((u16)(c + CR_TRANS)) + PT_WEAR)) = (u16)(rnd(3000) + 6000);
                DSW((u16)(DSW((u16)(c + CR_ENGINE)) + PT_WEAR)) = (u16)(rnd(3000) + 6000);
                id = (s16)(m + 0x15);
            }
            DSB((u16)(DS_ad_ids + r - 2)) |= 0x80;                             /* sold; the same id stays out of the next re-rolls */
            DSB((u16)(DS_ad_cool + id)) += 3;
            msg_box(-2);                                                       /* It's yours! */
            clock_add(0x444);
            break;
        }
        spare_add(t, price);
        clock_add(0x222);
        if (DSS(DS_part_count) > 0xB4) break;
    }
    hotspots_reset();
    if (DSW(DS_cur_car) != 0) set_paint_palette((s8)DSB((u16)(DSW(DS_cur_car) + CR_COLOUR)));
}

/* ============================================================================== 0001:3e30 */

void newspaper_front(void)
{
    u16 save = DSW(DS_g_mirror);
    DSW(DS_g_mirror) = 0;
    show_picture(0x3EF, -1);                                                   /* front page, LIB2 #7 */
    s16 week = classified_week();
    show_picture_at(DSS((u16)(DS_NEWS_PICS + 2 * week)), 0, 0x88, -1);         /* headline photo of the week */
    show_picture_at(0x4A4, 0, 0xA7, -1);                                       /* bottom strip */
    cursor_ctl(-3);
    date_print(0x79);
    cursor_ctl(-1);
    DSW(DS_g_mirror) = save;
    if (save) page_copy_rect(g_back(), g_front(), 0, 0, 0x13F, 0xBD);
    drv_pal_normal();                                                          /* driver slot 8 */
    cursor_ctl(1);
    s16 r = ui_menu(3);                                                        /* 1 Used Cars, 2 Auto Parts, else leave */
    if (r == 1 || r == 2) {
        classifieds(r, week != DSS(DS_AD_WEEK) ? 1 : 0);
        DSW(DS_AD_WEEK) = (u16)week;
    }
}
