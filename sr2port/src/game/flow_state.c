/* game_flow: player objects, checks and the new-game set-up — port/spec/game_flow.md, garage.md
 * (0001:3e08-4250, 0001:0e94, 0001:51f8-550e, 0001:68e2). Delta port of SR1 flow_state.c. */
#include "game/flow.h"
#include "game/race.h"

#include "game/records.h"
#include "game/ui.h"
#include "platform/platform.h"
#include "platform/video.h"

/* ============================================================================== object pools */

/* 0001:3f28 part_alloc */
u16 part_alloc(void)
{
    u16 p = DSW(DS_part_freelist);
    DSW(DS_part_freelist) = DSW((u16)(p + PT_NEXT));
    DSW((u16)(p + PT_NEXT)) = 0;
    DSW(DS_part_count)++;
    return p;
}

/* 0001:3f3e part_free */
void part_free(u16 p)
{
    DSW((u16)(p + PT_NEXT)) = DSW(DS_part_freelist);
    DSW((u16)(p + PT_TYPE)) = 0xFFFF;
    DSW(DS_part_freelist) = p;
    DSW(DS_part_count)--;
}

/* 0001:423a car_alloc (SR1 0000:3fb1; the link is at +0x28) */
u16 car_alloc(void)
{
    u16 c = DSW(DS_car_freelist);
    DSW(DS_car_freelist) = DSW((u16)(c + CR_NEXT));
    DSW((u16)(c + CR_NEXT)) = 0;
    DSW(DS_car_count)++;
    return c;
}

/* 0001:4250 car_release (SR1 0000:3fd4) */
void car_release(u16 c)
{
    DSW((u16)(c + CR_NEXT)) = DSW(DS_car_freelist);
    DSW((u16)(c + CR_MODEL)) = 0xFFFF;
    DSW(DS_car_freelist) = c;
    DSW(DS_car_count)--;
}

/* ============================================================================ checks */

/* 0001:3e08 garage_full_check */
s16 garage_full_check(void)
{
    if (DSS(DS_car_count) >= CAR_POOL_N) { msg_box(0x22); return 1; }    /* "Sell some cars first!" */
    if (DSS(DS_part_count) >= PART_LIMIT) { msg_box(0x23); return 1; }   /* "Sell some parts first!" */
    return 0;
}

/* 0001:0e94 cheapest_ad_price: the cheapest car ad of the list DS:9B42 (0xFF-terminated, bit 7 = sold). Ids 0..20 are the
 * models' prices, ids 21.. the second price field (trade-in) of model id - 21. The minimum starts at 10000; an empty
 * list gives 0. */
s32 cheapest_ad_price(void)
{
    s16 min = 10000;
    u16 n = 0;
    for (;; n++) {
        u8 b = DSB((u16)(DS_ad_ids + n));
        if (b == 0xFF) break;
        if (b & 0x80) continue;
        s16 price = b < 0x15 ? model_price(b) : model_trade_in((s16)(b - 21));
        if (price < min) min = price;
    }
    return n == 0 ? 0 : (s32)min;
}

/* 0001:68e2 broke_check: nothing owned and the cheapest ad is dearer than the money (signed 32-bit) */
s16 broke_check(void)
{
    return DSW(DS_cur_car) == 0 && DSW(DS_car_list) == 0 && DSW(DS_spare_parts) == 0 &&
           cheapest_ad_price() > DSSL(DS_money);
}

/* 0001:5494 str_is_blank: only ' ' and '_' */
s16 str_is_blank(u16 s)
{
    for (; DSB(s); s++)
        if (DSB(s) != ' ' && DSB(s) != '_') return 0;
    return 1;
}

/* 0001:54ba trim_trailing_spaces (game_flow.md: str_rtrim): removes the trailing blanks of the string at DS:s and returns how
 * many. As in the original the scan reads s[len-1] first (for an empty string that is the byte before the buffer) and,
 * for an all-blank string, stops at the byte before the buffer. */
s16 trim_trailing_spaces(u16 s)
{
    s16 n = 0;
    if (s == 0) return 0;
    u16 p = (u16)(s + fl_strlen(s) - 1);
    if (DSB(p) == 0) return 0;
    for (;;) {
        if (DSB(p) != ' ') return n;
        DSB(p) = 0;
        n++;
        p--;
        if (DSB(p) == 0) return n;
    }
}

/* ================================================================================== new game */

/* 0001:51f8 opponent_init: models[0] = count, models[1..count] = model numbers (-1 = taken). The order of the rnd calls matters. */
void opponent_init(s16 i, s16 *models)
{
    s16 col;
    for (;;) {                                        /* colour 5 at half weight */
        col = rnd(6);
        if (col != 5 || rnd(2) == 1) break;
    }
    u16 r = (u16)(DS_people + i * FLOW_PERSON_SIZE);
    DSB((u16)(r + 0x0A)) = (u8)col;
    DSB((u16)(r + 0x0B)) = 0;
    DSB((u16)(r + 0x0C)) = 0;
    DSB((u16)(r + 0x0D)) = 0;
    DSB((u16)(r + 0x0E)) = 0;
    s16 k;
    do { k = rnd(models[0]); } while (models[k + 1] == -1);
    s16 m = models[k + 1];
    DSW((u16)(r + 8)) = (u16)m;
    models[k + 1] = -1;
    s16 lvl = (s16)((model_class(m) >> 1) + 1);
    u16 ov = (u16)(DS_MODEL_OVERLAYS + 10 * m);       /* +2 roof, +6 rear bumper, +8 front bumper (LIB2 ids) */
    if (DSS((u16)(ov + 6)) > 0 && DSS((u16)(ov + 8)) > 0)
        DSB((u16)(r + 0x0D)) |= rnd(0x19) < lvl ? 3 : 0;
    if (DSS((u16)(ov + 2)) > 0)
        DSB((u16)(r + 0x0D)) |= rnd(0x1E) < lvl ? 4 : 0;
    DSB((u16)(r + 0x0D)) |= rnd(0x14) < lvl ? 0x20 : 0;
    /* the two skill bytes from the person's race parameters b and a (arithmetic shifts, byte arithmetic) */
    DSB((u16)(r + 0x0F)) = (u8)(7 - (DSC((u16)(r + 1)) >> 1));
    DSB((u16)(r + 0x10)) = (u8)(6 - (DSC(r) >> 2));
}

/* 0001:534c opponents_init: the 21 car models by class (< 3 slow, < 6 mid, else fast), 7 opponents each */
void opponents_init(void)
{
    /* The three lists are 11-word arrays on the original's stack (fast, slow, mid in memory order). A class with more
     * than 10 models would run into its neighbour; the data has at most 10 per class.
     * PORT: the arrays are sized for all 22 models so nothing is overwritten either way. */
    s16 slow[24] = {0}, mid[24] = {0}, fast[24] = {0};
    for (s16 m = 0; m < 21; m++) {
        u8 c = model_class(m);
        if (c >= 6) fast[++fast[0]] = m;
        else if (c < 3) slow[++slow[0]] = m;
        else mid[++mid[0]] = m;
    }
    s16 i = 0;
    for (s16 n = 7; n; n--) opponent_init(i++, slow);
    for (s16 n = 7; n; n--) opponent_init(i++, mid);
    for (s16 n = 7; n; n--) opponent_init(i++, fast);
    DSB(0xAD03) = 0;                                  /* the King's +0B (person 21 + 0x0B) */
}

/* 0001:5400 new_game */
void new_game(void)
{
    pools_init(1);
    DSSL(DS_money) = (s32)DSW(DS_demo) * 40000 + 0x4B0;       /* $1200; demo $41200 (SR1 $750 / $10000) */
    DSB(DS_skill_b) = 1;
    DSB(DS_skill_a) = 1;
    track_build_all(1);
    track_build_course(100);
    opponents_init();
    DSL(DS_game_clock) = 0;
    DSL(DS_rt_base) = DSL(DS_rt_ticks);
    for (u16 k = 0; k < 6; k++) DSW((u16)(DS_counters + 2 * k)) = 0;    /* 479C..47A6 */
    DSW(DS_garage_block) = 0xFFFF;                    /* classified ads: none rolled yet */
    DSB(DS_ad_ids) = 0xFF;
    DSW(DS_ad_pool) = 0x2A;
    for (u16 k = 0; k < 0x2A; k++) DSB((u16)(DS_ad_cool + k)) = 0;
}

/* 0001:550e new_game_prompt: 0 new game, -2 "Old Game", other codes as returned */
s16 new_game_prompt(void)
{
    DSW(DS_g_mirror) = 0;
    show_picture_at(0x3EA, 0x10, 6, 0);               /* LIB2 #2 driver licence */
    cursor_ctl(1);
    DSW(DS_g_mirror) = 1;
    pal_shadow_patch();                               /* 1a21:19aa */
    page_copy_rect(g_back(), g_front(), 0, 0, 0x13F, 0xBD);
    drv_pal_normal();
    pal_load(DS_pal_menu);                            /* 1a21:198a (the original's pal_reg12_13(DS:1DDA)) */
    s16 r = ui_edit(2, 0x0F, ds_str(DS_player_name)); /* name field, "Old Game" -2, "OK" -3, Enter 0 */
    trim_trailing_spaces(DS_player_name);
    if (r == -3 || r == 0) {
        new_game();
        return 0;
    }
    if (r == -2) return -2;
    return r;
}
