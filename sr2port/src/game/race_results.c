/* Race: results, fines, damage (port/spec/race.md 0001:ac68-b584, checked against the decompile / disassembly).
 *
 *   0001:ac68 tires_replace   0001:ad36 text_draw_clipped  0001:adc0 ticket_show   0001:af0a crash_show
 *   0001:b256 race_msg        0001:b46e engine_blown       0001:b4da trans_blown   0001:b538 patch_dollar_amount
 *   0001:b584 race_results */
#include "game/race_int.h"
#include "game/race.h"

#include <stdio.h>
#include <string.h>

#include "game/events.h"
#include "game/flow.h"
#include "game/garage.h"
#include "game/ui.h"
#include "platform/platform.h"
#include "platform/video.h"

#define MODE DSS(DS_road_mode)

/* 0001:2164 music_resume_if_on (sound) */
static void music_resume_if_on(void) {}                         /* PORT: S5.4 stub (sound) */

/* 0001:ac68 tires_replace: blown tyres off; a spare set or plain tyres for $50. Returns the unpaid part. */
s16 tires_replace(void)
{
    s16 unpaid = 0;
    u16 car = cur_car();
    if (car == 0) return 0;
    CARW(car, CR_VALUE) = (u16)(CARS(car, CR_VALUE) - part_release(CARW(car, CR_TYRES)));
    CARW(car, CR_TYRES) = 0;
    u16 nodes[0xC0];
    s8 wear[0xC0];                                  /* PORT: the original passes wear = NULL and writes DS:0000+n */
    memset(nodes, 0, sizeof nodes);
    s16 n = 0;
    spares_collect(4, nodes, wear, 0, &n);
    if (n < 1) {
        DSSL(DS_money) -= 0x32;
        if (DSSL(DS_money) < 0) { unpaid = (s16)-DSS(DS_money); DSSL(DS_money) = 0; }
        spare_add(0x32, DSS(0x46F6));
        u16 p = DSW(DS_spare_parts);
        DSW(DS_spare_parts) = DSW((u16)(p + PT_NEXT));
        part_install(p, (u16)(cur_car() + CR_TYRES));
    } else {
        /* PORT: the original takes nodes[1] (the second spare set); with one spare set that entry is stack garbage, the
         * port takes nodes[0] then */
        u16 pick = n > 1 ? nodes[1] : nodes[0];
        part_install(spare_unlink(pick), (u16)(cur_car() + CR_TYRES));
        unpaid = 0;
    }
    return unpaid;
}

/* 0001:ad36 text_draw_clipped: cut the string from the end until it fits, draw, restore */
void text_draw_clipped(s16 x, s16 y, u16 s, s16 maxw)
{
    char *p = ds_str(s);
    s16 len = (s16)strlen(p);
    char *q = p + len - 1;
    char removed = 0;
    while (font_string_width(p) > maxw) {
        if (removed) *q = removed;
        q--;
        removed = *q;
        *q = 0;
    }
    drv_draw_text(g_back(), x, y, p);
    if (removed) *q = removed;
}

/* 0001:adc0 ticket_show: the speeding ticket with the player's name and car */
static void ticket_show(void)
{
    arena_reset();
    FarPtr t = pic_get(0x7D9, 1);
    FarPtr f = pic_get(0x803, 1);
    Rect r = { (s16)desc_w(f), (s16)desc_h(f), 0, 0, 0, 0x45 };
    drv_blit_masked(f, g_back(), &r, 1);
    r = (Rect){ (s16)desc_w(t), (s16)desc_h(t), 0, 0, (s16)desc_w(f), 0x14 };
    drv_blit(t, g_back(), &r, 1);
    s16 maxw = (s16)(desc_w(t) - 8);
    DSW(DS_g_mirror) = 0;
    memcpy(mp(DGROUP, DS_g_text_fg), mp(DGROUP, (u16)(DSW(DS_SCHEME) + 0x41)), 5);
    text_draw_clipped(0x5E, 0x48, DS_player_name, maxw);
    text_draw_clipped(0x5E, 99, DSW((u16)(MODEL_PTR(CARS(cur_car(), CR_MODEL)) + 0xC)), maxw);
    drv_copy_page(desc_planes(g_back()), desc_planes(g_front()));
    DSW(DS_g_mirror) = 1;
    cursor_ctl(-2);
    input_reset();
}

/* 0001:af0a crash_show: crash picture of the dash type, wheel knob, glass cracks above 90 mph */
static void crash_show(void)
{
    DSB(DS_fall_dir) = 0;
    DSW(DS_fallen) = 0;
    s16 split = (DSS(DS_driver_id) == -2 && DSW(DS_g_vga) == 0) ? 1 : 0;
    sound_vec(SND_CRASH);
    arena_reset();
    FarPtr a = pic_get(0x810, 1);
    FarPtr b = pic_get(0x812, 1);
    FarPtr c = far_make(0, 0);
    if (DSS(DS_dash_type) == 0) c = pic_get(0x800, 1);
    else if (DSS(DS_dash_type) == 1) c = pic_get(0x811, 1);
    Rect r = { (s16)desc_w(a), 0x5A, 0, 0, 0, 0xB };
    drv_blit_masked(a, g_back(), &r, 1);
    r = (Rect){ (s16)desc_w(a), (s16)(desc_h(a) - 0x59), 0, 0x59, 0, (s16)(0x64 + split) };
    drv_blit_masked(a, g_back(), &r, 1);
    if (DSS(DS_dash_type) != 2) {
        r = (Rect){ 0x100, (s16)desc_h(c), 0, 0, 0x40, (s16)(0x5A + split) };
        drv_blit_masked(c, g_back(), &r, 1);
    }
    r = (Rect){ (s16)desc_w(b), (s16)desc_h(b), 0, 0, 0, (s16)(0x77 + split) };
    drv_blit(b, g_back(), &r, 1);
    wheel_knob_crash(DSS(DS_wheel_shown));
    drv_copy_page(desc_planes(g_back()), desc_planes(g_front()));
    if (PHS(PLAYER, PH_SPEED) > 0x5A) {                       /* shattered glass */
        for (s16 i = 0; i < 3; i++) {
            u16 e = (u16)(0x5136 + 6 * i);
            FarPtr p = pic_get(DSS(e), 1);
            Rect g = { (s16)desc_w(p), (s16)desc_h(p), 0, 0, DSS((u16)(e + 2)), DSS((u16)(e + 4)) };
            wait_vretraces((s16)(0x14 - i * 8));
            drv_blit_masked(p, g_back(), &g, 1);
            drv_copy_page(desc_planes(g_back()), desc_planes(g_front()));
        }
    }
    cursor_ctl(-2);
    drv_pal_normal();
    input_reset();
    wait_ticks(100);
    input_reset();
    arena_reset();
}

/* 0001:b256 race_msg: time and speed into DS:A570 / DS:A5C6, screen 0x17 */
void race_msg(void)
{
    s16 t = DSS(DS_race_time10);
    if (DSW(DS_player_won) == 0) {
        snprintf(ds_str(0xA570), 0x56, ds_str(0x518F), t / 10, t % 10, (int)DSS(DS_pct_covered));
        snprintf(ds_str(0xA5C6), 0x40, ds_str(0x51B3), (int)DSS(DS_avg_speed));
    } else {
        snprintf(ds_str(0xA570), 0x56, ds_str(0x514A), t / 10, t % 10, (int)DSS(DS_avg_speed));
        memcpy(mp(DGROUP, 0xA5C6), mp(DGROUP, 0x517D), 0x12);    /* "Congratulations !" */
    }
    ui_push(0x17);
    ui_pop(1);
}

/* 0001:b46e engine_blown: engine, manifold and carburettors gone */
static void engine_blown(s16 msg)
{
    u16 car = cur_car();
    if (car != 0) {
        for (u16 s = CR_ENGINE; s < CR_ENGINE + 10; s = (u16)(s + 2)) {
            CARW(car, CR_VALUE) = (u16)(CARS(car, CR_VALUE) - part_release(CARW(car, s)));
            CARW(car, s) = 0;
        }
        memset(mp(DGROUP, (u16)(car + 0x16)), 0, 6);
        CARB(car, CR_ENGINE_LINK) = 0;
    }
    input_reset();
    sound_vec(SND_BREAK, 0x1000);
    message_at((s16)-msg, -1, 0x49);
}

/* 0001:b4da trans_blown */
static void trans_blown(s16 msg)
{
    u16 car = cur_car();
    if (car != 0) {
        CARW(car, CR_VALUE) = (u16)(CARS(car, CR_VALUE) - part_release(CARW(car, CR_TRANS)));
        CARW(car, CR_TRANS) = 0;
        CARB(car, 0x14) = 0;
        CARB(car, 0x15) = 0;
    }
    input_reset();
    sound_vec(SND_BREAK, 0x1000);
    message_at((s16)-msg, -1, 0x49);
}

/* 0001:b538 patch_dollar_amount: "$123" -> "$<amount>", the sprintf NUL becomes a blank */
void patch_dollar_amount(u16 text, s16 amount)
{
    char *p = strchr(ds_str(text), '$');
    if (!p) return;
    p[2] = ' ';
    p[3] = ' ';
    char buf[8];
    snprintf(buf, sizeof buf, ds_str(0x51E4), amount);          /* "%d" */
    size_t n = strlen(buf);
    memcpy(p + 1, buf, n);
    p[1 + n] = ' ';
}

/* 0001:b584 race_results: 1 drive home (won pink slips / repaired), 2 drive home, 3 stay at Bob's, 4 jail, or the
 * repair menu's code - 2 */
u16 race_results(void)
{
    s16 bet = DSS(DS_bet_kind), amount = DSS(DS_bet_amount);
    u16 res0 = PHW(PLAYER, PH_RESULT);
    s16 eng = (s16)(res0 & 4), trn = (s16)(res0 & 0x10);
    bool breakdown = res0 == 5 || res0 == 0x11 || res0 == 0x15;
    u16 car = cur_car();
    static const u16 slots[3] = { CR_TRANS, CR_ENGINE, CR_TYRES };
    for (int i = 0; i < 3; i++) {
        u16 pw = (u16)(CARW(car, slots[i]) + PT_WEAR);
        if (DSS(pw) > 0x26AC) DSW(pw) = 0x26AC;
    }
    DSW(DS_player_won) = 0;
    DSW(DS_stats_done) = 0;
    DSL(DS_distance) = 0;
    DSB(DS_click_edge) = 0;
    DSB(DS_click_held) = 0;
    if (PHW(PLAYER, PH_AUTOMATIC) != 0) PHW(PLAYER, PH_AUTO_MODE) = 1;
    if (DSB(DS_tires_blown) != 0) DSB(PLAYER + PH_RESULT) |= 8;
    s16 used = MODE == 8 ? 10 : MODE == 0x10 ? 15 : 5;
    if (used < CARS(car, CR_GAS)) CARW(car, CR_GAS) -= (u16)used; else CARW(car, CR_GAS) = 0;
    drv_pal_normal();
    u16 res = PHW(PLAYER, PH_RESULT);
    if (PHW(OPP, PH_RESULT) & 1) {                              /* ---- won */
        music_resume_if_on();
        DSW(DS_player_won) = 1;
        if (!(MODE & 6)) {
            race_msg();
            cursor_ctl(-2);
            msg_box(DSW(DS_vs_king) ? (MODE == 8 ? 0x50 : 0x4F) : DSS((u16)(0x512A + 2 * bet)));
        } else tournament_result_message(MODE);
        money_add(amount);
        if (bet == 5) {                                         /* pink slips: his car */
            u16 orec = DSW(DS_opp_rec);
            u16 n = car_new(DSS(DS_opp_model), DSS(DSW(DS_opp_model_ptr)), 0);
            PARTS(CARW(n, CR_TYRES), PT_WEAR) = rnd(500);
            u8 cu = DSB((u16)(orec + PR_CUSTOM));
            u16 f = CARW(n, CR_FLAGS);
            f = (u16)((f & 0xDFFF) | ((cu & 4) ? 0x2000 : 0));
            f = (u16)((f & 0xBFFF) | ((cu & 2) ? 0x4000 : 0));
            f = (u16)((f & 0x7FFF) | ((cu & 1) ? 0x8000 : 0));
            CARW(n, CR_FLAGS) = f;
            CARB(n, CR_COLOUR) = DSB((u16)(orec + PR_PAINT));
            u16 cc = cur_car();
            if (CARB(cc, CR_CLASS) < CARB(n, CR_CLASS)) {        /* better class: drive it */
                CARW(cc, CR_NEXT) = CARW(DSW(DS_car_list), CR_NEXT);
                DSW(DS_car_list) = cc;
                DSW(DS_cur_car) = n;
                car_setup(n, PLAYER);
            }
            if (DSW(DS_demo) == 0) {
                bool gone = true;
                if (DSB((u16)(DSW(DS_opp_rec) + PR_REROLL)) == 0 && DSW(DS_vs_king) == 0) {
                    s16 list[22];
                    s16 cnt = 0;
                    u8 mc = DSB((u16)(DSW(DS_opp_model_ptr) + 4));
                    for (s16 m = 0; m < 0x15; m++) if (model_cls(m) < mc) list[++cnt] = m;
                    if (cnt > 0) {
                        list[0] = cnt;
                        opponent_init(DSS(DS_opp_index), list);
                        DSB((u16)(DSW(DS_opp_rec) + PR_REROLL)) = 1;
                        gone = false;
                    }
                }
                if (gone) DSC((u16)(DSW(DS_opp_rec) + PR_STATUS)) = -1;
            }
        }
        if ((PHW(PLAYER, PH_RESULT) & 8) && tires_replace() != 0) return 2;
        return bet == 5 ? 1 : 3;
    }
    if (res & 0x40) {                                           /* ---- police */
        sound_vec(SND_SIREN, 0);
        music_resume_if_on();
        ticket_show();
        s16 fine = (res & 8) ? tires_replace() : 0;
        fine = (s16)(fine + (PHS(PLAYER, PH_SPEED) > 0x50 ? 0x4B : PHS(PLAYER, PH_SPEED) > 0x32 ? 0x32 : 0x14));
        ui_push(6);
        ui_wait(0);
        char buf[0x30];
        snprintf(buf, sizeof buf, ds_str(0x51E7), (int)fine);
        status_print(-1, buf, (s16)(0x140 - DSS(DS_status_x0)));
        wait_ticks(0xFA);
        if ((s32)fine > DSSL(DS_money)) {                       /* sentenced for debts */
            msg_box(0x52);
            ui_pop(1);
            money_add((s16)-DSS(DS_money));
            status_print(-1, ds_str(0x520D), (s16)(0x140 - DSS(DS_status_x0)));
            return 4;
        }
        money_add((s16)-fine);
        u32 save = DSL(DS_wait_until);
        wait_ticks(DSS(DS_msg_ticks));
        DSL(DS_wait_until) = save;
        status_print(-1, ds_str(0x520F), (s16)(0x140 - DSS(DS_status_x0)));
        ui_pop(1);
        return 3;
    }
    if (res == 0x81) {                                          /* jumped the light */
        music_resume_if_on();
        cursor_ctl(-2);
        msg_box((MODE & 6) ? -0x51 : -0x48);
        return 3;
    }
    if (res == 0x101) {                                         /* crossed the centre line / bumped the starter */
        music_resume_if_on();
        cursor_ctl(-2);
        msg_box(!(MODE & 6) ? 0x78 : DSS(DS_tourn_round) < 2 ? 0x6D : 0x6C);
        return 1;
    }
    if (bet == 5) { car_free(cur_car()); DSW(DS_cur_car) = 0; }
    money_add((s16)-amount);
    if (res == 1) {                                             /* lost, car intact */
        music_resume_if_on();
        if (MODE == 4) tournament_result_message(4);
        else {
            race_msg();
            cursor_ctl(-2);
            s16 id = bet == 5 ? 0x53 : 0x54;
            if (DSW(DS_vs_king)) id = MODE == 1 ? 0x55 : 0x56;
            msg_box(id);
        }
        return bet == 5 ? 2 : 3;
    }
    if (breakdown) {
        music_resume_if_on();
        cursor_ctl(-2);
        if (eng) engine_blown(bet == 5 ? 0x5A : 0x58);
        else if (trn) trans_blown(bet == 5 ? 0x59 : 0x57);
        return 2;
    }
    crash_show();                                               /* ---- crash */
    music_resume_if_on();
    if (bet == 5) {
        if (res & 8) { msg_box(0x5C); tires_replace(); return 2; }
        msg_box(0x5B);
        return 2;
    }
    if (PHS(PLAYER, PH_SPEED) > 0x5A || DSW(DS_cliff_fall) != 0) {   /* totaled */
        s16 got = (s16)(car_free(cur_car()) / 10 + 0x13);
        DSW(DS_cur_car) = 0;
        money_add(got);
        patch_dollar_amount(DSW(0x141C), got);
        input_reset();
        msg_box(0x5D);
        return 2;
    }
    s16 slow;
    if (PHS(PLAYER, PH_SPEED) > 0x3C) slow = 0;
    else if (PHS(PLAYER, PH_SPEED) > 0x1E) slow = 1;
    else {
        if (res & 8) { msg_box(0x5F); tires_replace(); return 2; }
        msg_box(0x5E);
        return 1;
    }
    s16 cost = (s16)(rnd(slow ? 0x14 : 0x28) + 100);
    patch_dollar_amount(slow ? 0x22B4 : 0x227C, cost);
    s16 tires = (s16)(res & 8);
    if (tires) memcpy(mp(DGROUP, 0x9AEA), mp(DGROUP, 0x5210), 0xD);   /* "Plus tires !" */
    else DSB(0x9AEA) = 0;
    s16 menu = slow ? 0x19 : 0x18;
    DSW((u16)(DSW((u16)(DS_SCREENS + 2 * menu)) + 0x12)) = (s32)cost <= DSSL(DS_money) ? 0x11 : 0;   /* "Fix it" */
    s16 b = ui_menu(menu);
    if (b == 1) {
        money_add((s16)-cost);
        if (tires) tires_replace();
        return 1;
    }
    if (b == 2) {                                               /* "Junk it" */
        money_add((s16)((s16)(CARS(cur_car(), CR_VALUE) << 2) / 10));
        DSW(DS_cur_car) = 0;
        return 2;
    }
    return (u16)(b - 2);                                        /* quirk: the menu code minus 2 */
}
