/* game_flow: the summer calendar and the game clock — port/spec/game_flow.md, garage.md
 * (0001:63e0-66ae, 1a21:0fe8).
 *
 * The date is T = game_clock DS:1F6A + (rt_ticks DS:1F6E - rt_base DS:1F72) (u32): activities add 0x222 per game hour,
 * real time adds 18.2 per second (one game hour = 30 s). Day 0 is a Monday, June 16, 1969 (SR1: Sunday, 1963); the summer
 * is over after day 90. */
#include "game/flow.h"

#include <stdio.h>

#include "game/records.h"
#include "game/ui.h"
#include "platform/platform.h"
#include "platform/video.h"

static u32 clock_now(void)
{
    return DSL(DS_game_clock) + DSL(DS_rt_ticks) - DSL(DS_rt_base);
}

/* 0001:63e0 day_index: T / 0x1998 (signed long divide), at most 0x5B */
s16 day_index(void)
{
    s16 d = (s16)((s32)clock_now() / (s32)FLOW_GAME_DAY);
    return d > 0x5B ? 0x5B : d;
}

/* 0001:640e classified_week: day_index() / 7 (signed), at most 11 (news headline photo, ad re-roll) */
s16 classified_week(void)
{
    s16 w = (s16)(day_index() / 7);
    return w > 11 ? 11 : w;
}

/* 0001:6426 clock_hm */
void clock_hm(s16 *hour, s16 *min)
{
    s32 t = (s32)clock_now();
    if (t > 0x91908) t = 0x91908;                     /* 91 days */
    u16 r = (u16)(t % (s32)FLOW_GAME_DAY);
    u16 h = (u16)(r / FLOW_GAME_HOUR);
    *hour = (s16)h;
    u16 part = (u16)(r - (u16)(h * FLOW_GAME_HOUR));
    *min = (s16)((s32)((u32)part * 0x3C) / (s32)FLOW_GAME_HOUR);
}

/* 0001:64a2 summer_over_check */
void summer_over_check(void)
{
    if (day_index() > 0x5A) DSB(DS_game_over) |= FLOW_END_SUMMER;
}

/* 0001:64b2 month_of_day: 0 June .. 3 September; *dom = day of the month */
s16 month_of_day(s16 d, s16 *dom)
{
    if (d < 0x0F) { *dom = (s16)(d + 0x10); return 0; }
    if (d < 0x2E) { *dom = (s16)(d - 0x0E); return 1; }
    if (d < 0x4D) { *dom = (s16)(d - 0x2D); return 2; }
    if (d < 0x5B) { *dom = (s16)(d - 0x4C); return 3; }
    *dom = 0x0F;
    return 3;
}

/* 0001:650a calendar_show */
void calendar_show(s16 final)
{
    summer_over_check();
    if (final == 0 && DSW(DS_game_over) != 0) return;
    s16 d = day_index();
    s16 dom;
    s16 m = month_of_day(d, &dom);
    s16 dd = (s16)(d + 0x16);                         /* weekday offset 0x16 (SR1 0x15) */
    s16 col = (s16)(dd % 7);
    s16 row = (s16)(dd / 7 - DSS((u16)(DS_CAL_ROW0 + 2 * m)));
    s16 x = (s16)(col * 0x12 + DSS(DS_CAL_X0));
    s16 y = (s16)(row * 7 + DSS((u16)(DS_CAL_Y0 + 2 * m)));
    DSW(DS_g_mirror) = 1;
    cursor_ctl(-3);
    FarPtr buf = pic_get(m < 2 ? 0x478 : 0x47A, 2);   /* LIB2 #112 June/July, #114 August/September */
    pic_draw_masked(buf, 0x479, (u8)x, (u8)y, 0);     /* LIB2 #113 the X */
    u16 w = desc_w(buf), hh = desc_h(buf);
    FarPtr bg = arena_bitmap_alloc((s16)w, (s16)hh, 0, 2);
    screen_save_rect(0x50, 4, (s16)w, (s16)hh, bg);
    screen_put_bitmap(buf, 0x50, 4);
    cursor_ctl(-1);
    page_copy_rect(g_back(), g_front(), 0x50, 4, (s16)(w + 0x4F), (s16)(hh + 3));
    arena_pop(1);
    if (final == 0) {
        status_print(-1, ds_str(DS_cal_empty), 0);    /* "" */
        wait_ticks(500);
        screen_put_bitmap_mirror(bg, 0x50, 4);
    }
    arena_pop(1);
}

/* 0001:66ae date_print: "June 16, 1969" at (0x61, y) on the back page in the scheme's colours (+0x41) */
void date_print(s16 y)
{
    s16 dom;
    s16 m = month_of_day(day_index(), &dom);
    char buf[0x20];
    snprintf(buf, sizeof buf, ds_str(DS_date_fmt), ds_str(DSW((u16)(DS_month_names + 2 * m))), dom);
    u8 save[5];
    for (int i = 0; i < 5; i++) save[i] = DSB((u16)(DS_g_text_fg + i));
    u16 sch = (u16)(DSW(DS_g_scheme) + 0x41);
    for (int i = 0; i < 5; i++) DSB((u16)(DS_g_text_fg + i)) = DSB((u16)(sch + i));
    drv_draw_text(g_back(), 0x61, y, buf);
    for (int i = 0; i < 5; i++) DSB((u16)(DS_g_text_fg + i)) = save[i];
}

/* 1a21:0fe8 summer_over_screen */
void summer_over_screen(void)
{
    msg_box(0x60);                                    /* "Looks like ya took too much time, jr!" */
    drv_pal_black();
    cursor_ctl(-4);
    DSW(DS_g_mirror) = 1;
    screen_fill_rect(0, 0, 0x140, 200, 0);
    calendar_show(1);
    pal_load(DS_pal_menu);
    drv_pal_normal();
    wait_ticks(300);
}
