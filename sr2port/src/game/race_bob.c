/* Burger's Drive-In: opponents, the challenge, the car-hop, jail and the King (port/spec/race.md 0001:9432-a9fc, checked
 * against the decompile / disassembly).
 *
 *   0001:9432 waitress_toggle   0001:9506 str_drop_last_word  0001:953c opponent_show   0001:975e king_status
 *   0001:9802 king_chance       0001:9848 opp_sort            0001:999a pick_opponent   0001:9a02 opponent_load
 *   0001:9b26 opponent_select   0001:9b78 opp_known_list      0001:9be0 opponent_search 0001:9c8e challenge_accept
 *   0001:9e3c challenge         0001:a270 bob_hotspots        0001:a3ba opponent_car_view
 *   0001:a4ea bob_sign_draw     0001:a588 bob_screen_draw     0001:a626 bob_drive_in    0001:a9fc jail */
#include "game/race_int.h"
#include "game/race.h"

#include <stdio.h>
#include <string.h>

#include "game/events.h"
#include "game/flow.h"
#include "game/garage.h"
#include "game/garage_draw.h"
#include "game/garage_screens.h"
#include "game/ui.h"
#include "platform/platform.h"
#include "platform/video.h"

#define MODE DSS(DS_road_mode)

/* PORT: the temporary car of opponent_car_view lives in the stack area of the original (DS:BC50..C650, never used by
 * the port's C code), as game_flow's scratch records (ds_flow.h) */
#define RACE_STK_CAR    0xC200u      /* 0x2A car record */
#define RACE_STK_PARTS  0xC230u      /* 10 x 8-byte part records */

static void set_deadline(u16 n) { DSL(DS_wait_until) = ticks_now() + n; }
static s16 wait_event(void) { s16 b; do b = ui_wait(3000); while (b == 0); return b; }

/* 0001:9432 waitress_toggle: the car-hop walks in (mode 0, pct_in %) and may leave at once (pct_out %), or leaves
 * (mode 1). Returns 1 if she moved. */
static s16 waitress_toggle(s16 mode, s16 arg, s16 pct_in, s16 pct_out)
{
    s16 drawn = 0;
    if (mode == 0) {
        if (DSW(DS_carhop_state) == 0 && DSS(DS_carhop_gate) >= 3 && rnd(100) < pct_in) {
            cursor_ctl(-4);
            carhop_anim(0, arg);
            drawn = 1;
            DSW(DS_carhop_state) = 1;
            if (rnd(100) < pct_out) {
                wait_vretraces(100);
                carhop_anim(1, arg);
                DSW(DS_carhop_state) = 0;
                DSW(DS_carhop_gate) = 0;
            }
        }
    } else if (mode == 1 && DSW(DS_carhop_state) == 1 && rnd(100) < pct_in) {
        cursor_ctl(-4);
        carhop_anim(1, arg);
        drawn = 1;
        DSW(DS_carhop_state) = 0;
        DSW(DS_carhop_gate) = 0;
    }
    if (drawn) cursor_ctl(-2);
    return drawn;
}

/* 0001:9506 str_drop_last_word */
static void str_drop_last_word(char *s)
{
    s16 i = (s16)(strlen(s) - 1);
    while (i >= 0) {
        if (s[i] == ' ') break;
        s[i] = 0;
        i = (s16)(strlen(s) - 1);
    }
    if (i >= 0) s[i] = 0;
}

/* 0001:953c opponent_show: status line "driver  car", the opponent's car (car_draw mode 1 drive in, 2 drive off,
 * 4 first arrival) and the car-hop */
static void opponent_show(s16 mode)
{
    s16 no_time = DSS(DS_car_pass);                                /* DS:7916 */
    u16 rec = DSW(DS_opp_rec);
    char s[80];
    snprintf(s, sizeof s, "%s%s%s", ds_str(DSW((u16)(rec + PR_NAME))), ds_str(0x5030),
             ds_str(DSW((u16)(DSW(DS_opp_model_ptr) + 0xC))));
    s16 jail_scr = DSS(DS_location) == 6;
    if (mode == 4) DSW(DS_carhop_state) = 0xFFFF;
    DSW(DS_opp_present) = mode == 2;
    if (mode == 1 && !waitress_toggle(0, DSS(DS_opp_present), (s16)(DSS(DS_demo) * 8 + jail_scr * 10 + 0xD), 10))
        waitress_toggle(1, DSS(DS_opp_present), 0x14, 0);
    size_t n = strlen(s);
    if (n > 0 && s[n - 1] == '.') s[n - 1] = 0;
    while (font_string_width(s) > DSS(DS_status_width) - 0xF) str_drop_last_word(s);
    status_print(-1, s, 0);
    car_draw(mode, DSS(DS_opp_model), DSC((u16)(rec + PR_CUSTOM)), 1, 0xA8, (u16)(DS_TYRE_PICS + 8 * DSS(DS_opp_tyre_set)));
    s16 m = no_time ? 2 : mode;
    if (m == 2) { DSW(DS_carhop_gate)++; status_print(-1, ds_str(0x5033), 0); }
    if (m == 4) DSW(DS_carhop_state) = 0;
    DSW(DS_opp_present) = m != 2;
    if (!jail_scr || !DSW(DS_opp_present)) {
        if (!waitress_toggle(0, DSS(DS_opp_present), (s16)(DSS(DS_demo) * 8 + jail_scr * 10 + 0xD), DSW(DS_opp_present) ? 0 : 8))
            waitress_toggle(1, DSS(DS_opp_present), 0x28, 0);
    }
}

/* 0001:975e king_status: 0 = the King accepts, 1..6 = why not */
s16 king_status(void)
{
    bool any = false;
    for (s16 i = 0; i < 0x15; i++) if (DSC((u16)(PERSON(i) + PR_STATUS)) != -1) any = true;
    if (!any) return 0;
    s16 M = DSS(DS_mulholland_wins), Q = DSS(DS_aqueduct_wins);
    s16 t = (s16)(M + Q);
    if (t < 3) return 1;
    if (t >= 7 && M < 6 && Q < 6) return 3;
    if (t < 7 && M < 6 && Q < 6) return 2;
    if (M < 6) return 5;
    if (Q < 6) return 6;
    if (DSW(DS_grudge_wins) == 0) return 4;
    return (DSS(DS_races_total) >> 1) <= t ? 0 : 3;
}

/* 0001:9802 king_chance: % that the King drives in */
static s16 king_chance(void)
{
    if (DSS(DS_opp_model) == 0x15) return 0;
    s16 s = king_status();
    if (s == 0) return DSB(DS_king_ready) == 1 ? 10 : 0x32;
    if (s == 1) return 5;
    if (s == 2) return 8;
    return 12;
}

/* 0001:9848 opp_sort: candidate list DS:912A ordered by top-speed difference */
static void opp_sort(s16 all)
{
    s16 spd[0x15], used[0x15], cls[0x15];
    for (s16 i = 0; i < 0x15; i++) {
        s16 m = DSS((u16)(PERSON(i) + PR_MODEL));
        s16 vp = car_max_speed(-1);
        s16 vo = car_max_speed(m);
        spd[i] = abs16((s16)(vo - vp));
        cls[i] = abs16((s16)(model_cls(m) - model_cls(CARS(cur_car(), CR_MODEL))));
        used[i] = DSC((u16)(PERSON(i) + PR_STATUS)) == -1;
    }
    DSW(DS_cand_count) = 0;
    for (;;) {
        if ((all ? 0x15 : 9) <= DSS(DS_cand_count)) break;
        s16 found = 0, best = 32000, bi = 0;
        for (s16 i = 0; i < 0x15; i++)
            if (!used[i] && spd[i] < best) { bi = i; best = spd[i]; found = 1; }
        if (!found) break;
        DSW((u16)(DS_cand + 2 * DSS(DS_cand_count))) = (u16)bi;
        DSW(DS_cand_count)++;
        used[bi] = 1;
    }
    if (!all) {                             /* sic: tests the class difference of PERSON k-1, not of cand[k-1] */
        u16 src = DS_cand;
        for (s16 k = DSS(DS_cand_count); k > 6; k--) {
            if (cls[k - 1] > 1) {
                DSW((u16)(DS_cand + 2 * (k - 1))) = DSW(src);
                src += 2;
            }
        }
    }
}

/* 0001:999a pick_opponent: the King with pct_king %, else a random candidate, no immediate repeat */
static s16 pick_opponent(s16 pct_king, s16 all)
{
    if (rnd(100) < pct_king) return 0x15;
    opp_sort(all);
    if (DSW(DS_cand_count) == 0) return 0x15;
    s16 o = DSS((u16)(DS_cand + 2 * rnd(DSS(DS_cand_count))));
    if (o == DSS(DS_last_pick)) o = DSS((u16)(DS_cand + 2 * rnd(DSS(DS_cand_count))));
    DSW(DS_last_pick) = (u16)o;
    return o;
}

/* 0001:9a02 opponent_load: person i becomes the opponent */
void opponent_load(s16 i)
{
    DSW(DS_opp_index) = (u16)i;
    u16 rec = PERSON(i);
    DSW(DS_opp_rec) = rec;
    DSS(DS_opp_model) = DSS((u16)(rec + PR_MODEL));
    DSW(DS_opp_model_ptr) = MODEL_PTR(DSS(DS_opp_model));
    if (i == 0x15) DSW(DS_vs_king) = 1;
    else {
        DSW(DS_vs_king) = 0;
        DSC((u16)(rec + PR_STATUS)) = 1;
        s16 m = DSS(DS_opp_model);
        s16 ch = (s16)((model_cls(m) >> 1) + 1);
        if (OVL_REAR(m) > 0 && OVL_FRONT(m) > 0 && DSC((u16)(rec + PR_RACES)) > 8)
            DSB((u16)(rec + PR_CUSTOM)) |= rnd(25) < ch ? 3 : 0;
        if (OVL_ROOF(m) > 0 && DSC((u16)(rec + PR_RACES)) > 8)
            DSB((u16)(rec + PR_CUSTOM)) |= rnd(30) < ch ? 4 : 0;
        DSB((u16)(rec + PR_CUSTOM)) |= rnd(50) < ch ? 0x20 : 0;
    }
    u16 mp_ = DSW(DS_opp_model_ptr);
    DSW(DS_opp_pic_id) = DSW((u16)(mp_ + 0x10));
    DSW(DS_opp_tyre_set) = (u16)((DSW((u16)(mp_ + 8)) & 0x30) >> 4);
    DSS(DS_opp_paint) = DSC((u16)(rec + PR_PAINT));
    DSW(DS_opp_face) = DSW((u16)(rec + PR_FACE));
}

/* 0001:9b26 opponent_select */
static void opponent_select(s16 i, s16 mode, s16 flag)
{
    opponent_load(i);
    set_paint_palette(DSS(DS_opp_paint));
    if (flag == 1) DSW(DS_car_pass) = 1;
    if (flag == 2 && DSS(DS_opp_index) == 0x15) DSW(DS_car_pass) = king_status() == 1;
    opponent_show(mode);
}

/* 0001:9b78 opp_known_list: names[] (DS offsets), idx[] of the people met (status 1), the King when ready */
static void opp_known_list(u16 names, s16 *idx, s16 *count)
{
    DSC((u16)(PERSON(DSS(0x4CAC)) + PR_STATUS)) = 0;
    DSB(DS_king_ready) = king_status() == 0 ? 1 : 0;
    s16 n = 0;
    for (s16 i = 0; i <= 0x15; i++) {
        if (DSC((u16)(PERSON(i) + PR_STATUS)) == 1) {
            idx[n] = i;
            DSW((u16)(names + 2 * n)) = DSW((u16)(PERSON(i) + PR_NAME));
            n++;
        }
    }
    *count = n;
}

/* 0001:9be0 opponent_search: "Looking for" list; 1 = an opponent was selected */
static s16 opponent_search(s16 mode)
{
    s16 idx[24] = { 0 }, n = 0;
    status_print(-1, ds_str(0x5036), 0);
    opp_known_list(DS_list_entries, idx, &n);
    s16 b = list_box(0x10, 0, (const u16 *)mp(DGROUP, DS_list_entries), NULL, n);
    if (b == -4) return 0;
    if (b == -3) {
        if (DSW(DS_opp_present)) { status_print(-1, ds_str(0x5037), 0); opponent_show(2); }
        opponent_select(idx[list_selected()], mode, 0);
        return 1;
    }
    if (b == 0x3EA) { msg_box(0x29); return 0; }
    return 0;                                      /* PORT: the original returns b - 0x3EA (a leftover AX); unused */
}

/* 0001:9c8e challenge_accept: share of the opponent's advantages in percent */
static s16 challenge_accept(s16 bet)
{
    s16 dr = (s16)(drivetrain_rating(DSS(DS_opp_model)) - drivetrain_rating(CARS(cur_car(), CR_MODEL)));
    s16 vp = car_max_speed(-1);
    s16 dv = (s16)(car_max_speed(DSS(DS_opp_model)) - vp);
    u16 rec = DSW(DS_opp_rec);
    s16 dw = (s16)(DSC((u16)(rec + PR_WINR)) - DSC(DS_player_rating_w));
    s16 dh = (s16)(DSC((u16)(rec + PR_HOLER)) - DSC(DS_player_rating_h));
    s16 c;
    if (!dr && !dv && !dw && !dh) c = 50;
    else {
        s16 wa = bet < 3 ? 1 : 2, wb = bet < 3 ? 2 : 1;   /* (decompile names: local_a, local_8) */
        s16 pr = dr > 0 ? dr : 0, pv = dv > 0 ? dv : 0, pw = dw > 0 ? dw : 0, ph = dh > 0 ? dh : 0;
        s16 den = (s16)((abs16(dr) + abs16(dw)) * wa + (abs16(dv) + abs16(dh)) * wb);
        s32 num = (s32)(s16)((pw + pr) * wa + (ph + pv) * wb) * 100;
        c = den ? (s16)(num / den) : 0;
    }
    if (bet < 2) c = (s16)(c * 2);
    else if (bet > 3) { if (c > 50) c = (s16)(c + (c >> 2)); else if (c < 50) c = (s16)(c - (c >> 2)); }
    if (DSW(DS_demo)) c = (s16)(c + (c >> 2) + 10);
    return rnd(100) <= c;
}

static void race_clock(void)
{
    flow_clock_add(MODE == 1 ? 0x444 : MODE == 0x10 ? 0x777 : 0x555);
}

/* 0001:9e3c challenge: race kind, bet and the King's rules; runs the race. 2 = raced, 0 = no race */
static s16 challenge(s16 king)
{
    arena_reset_stacks();
    u16 car = cur_car();
    if (king) {
        s16 s = king_status();
        if (s) { if (s != 1) msg_box(DSS((u16)(0x5054 + 2 * s))); return 0; }
        switch (DSS(DS_king_stage)) {
        case 0:
            if (!(CARS(car, CR_GAS) > 6)) { msg_box(0x41); return 0; }
            MODE = 1; msg_box(0x3E); DSW(DS_bet_kind) = 0; DSW(DS_bet_amount) = 0; break;
        case 1:
            if (!(CARS(car, CR_GAS) > 0x10)) { msg_box(0x41); return 0; }
            MODE = 0x10; msg_box(0x3F); DSW(DS_bet_kind) = 0; DSW(DS_bet_amount) = 0; break;
        case 2:
            if (CARS(car, CR_GAS) < 0xC) { msg_box(0x41); return 0; }
            MODE = 8; msg_box(0x40); DSW(DS_bet_kind) = 5; DSW(DS_bet_amount) = 0; break;
        default: break;
        }
        DSW(DS_in_challenge_race) = 1;
        race_start();
        race_clock();
        return 2;
    }
    if (CARS(car, CR_GAS) < 7) { msg_box(0x41); return 0; }
    s16 sx = 0, sy = 0;
    FarPtr save = ui_save_bg(0x13, &sx, &sy, 1);
    ui_push(0x13);
    set_deadline(800);
    s16 b = wait_event();
    s16 menu = MODE == 1 ? 0x11 : 0x12;            /* PORT: uninitialised in the original for other codes */
    s16 msg = 0;
    if (b == -0x17) goto cancel;
    if (b == 1)      { menu = 0x11; MODE = 1; }
    else if (b == 2) { menu = 0x12; MODE = 0x10; }
    else if (b == 3) { menu = 0x12; MODE = 8; }
    else if (b == 1000) { msg = 0x43; goto msg_cancel; }
    if ((MODE == 0x10 && !(CARS(car, CR_GAS) > 0x10)) || (MODE == 8 && !(CARS(car, CR_GAS) > 0xB))) { msg = 0x41; goto msg_cancel; }
    ui_pop(1);
    DSW(DS_g_mirror) = 0;
    screen_put_bitmap_mirror(save, sx, sy);
    ui_push(menu);
    page_copy_rect(g_back(), g_front(), sx, sy, (s16)(desc_w(save) + sx - 1), (s16)(desc_h(save) + sy - 1));
    DSW(DS_g_mirror) = 1;
    s16 last = -1, a = 0, k = 0;                   /* PORT: a / k uninitialised until a bet button (as SR1 port) */
    for (s16 tries = 0; tries < 2; tries++) {
        set_deadline(800);
        b = wait_event();
        DSW(DS_bet_button) = (u16)b;
        switch (b) {
        case 1: a = 0;    k = 0; break;
        case 2: a = 10;   k = 1; break;
        case 3: a = 0x32; k = 2; break;
        case 7: a = 0x19; k = 3; break;
        case 8: a = 0x96; k = 4; break;
        case 9: a = 0;    k = 5; break;
        case -0x17: case 1000: msg = 0x43; goto msg_cancel;
        default: break;
        }
        if ((s32)a > DSSL(DS_money)) { msg_box(0x42); continue; }
        if (challenge_accept(k)) {
            DSW(DS_bet_kind) = (u16)k;
            DSW(DS_bet_amount) = (u16)a;
            ui_pop(1);
            screen_put_bitmap_mirror(save, sx, sy);
            msg_box(DSS((u16)(0x504C + 2 * ((MODE != 1) * 3 + rnd(3)))));
            DSW(DS_in_challenge_race) = 1;
            race_start();
            DSW(DS_bob_races_started)++;
            race_clock();
            return 2;
        }
        s16 r;
        do r = rnd(5); while (r == last);
        last = r;
        msg_box(DSS((u16)(0x5038 + 2 * ((MODE != 1) * 5 + r))));
    }
    goto cancel;
msg_cancel:
    msg_box(msg);
cancel:
    ui_pop(1);
    screen_put_bitmap_mirror(save, sx, sy);
    arena_reset_stacks();
    return 0;
}

/* 0001:a270 bob_hotspots: "See it" / "Click to challenge" rectangles on the opponent's car (records with ids 1..4 of
 * the current hot-spot list, SR2_DATA block 4 at DS:9872) */
static void bob_hotspots(s16 present)
{
    u16 r1 = 0, r2 = 0, r3 = 0, r4 = 0;
    for (u16 p = DSW(DS_hotspot_list);; p = (u16)(p + 0x12)) {
        s16 id = DSS((u16)(p + 6));
        if (id == 1) r1 = p; else if (id == 2) r2 = p; else if (id == 3) r3 = p; else if (id == 4) r4 = p;
        if (DSB(p) & 2) break;
    }
    DSW((u16)(r4 + 0xE)) = 0x77;
    if (!present) {
        DSB(r2) &= 0xFE;
        DSB(r3) &= 0xFE;
        return;
    }
    u16 H = (u16)(DS_MODEL_HOTSPOTS + 20 * DSS(DS_opp_model));
    s16 x = (s16)(DSB((u16)(H + 4)) + DSS(DS_car_x)), y = (s16)(DSB((u16)(H + 5)) + DSS(DS_car_y));
    DSW((u16)(r2 + 8)) = (u16)x;
    DSW((u16)(r2 + 0xC)) = (u16)(x + DSB((u16)(H + 6)) - 1);
    DSW((u16)(r2 + 0xA)) = (u16)y;
    DSW((u16)(r2 + 0xE)) = (u16)(DSS((u16)(r1 + 0xA)) - 1);
    DSB(r2) |= 1;
    x = (s16)(DSB((u16)(H + 0xC)) + DSS(DS_car_x));
    y = (s16)(DSB((u16)(H + 0xD)) + DSS(DS_car_y));
    s16 x1 = (s16)(x + DSB((u16)(H + 0xE)) - 1), y1 = (s16)(y + DSB((u16)(H + 0xF)) - 1);
    if (x <= DSS((u16)(r2 + 0xC))) x = (s16)(DSS((u16)(r2 + 0xC)) + 1);
    DSW((u16)(r3 + 8)) = (u16)x;
    DSW((u16)(r3 + 0xC)) = (u16)x1;
    DSW((u16)(r3 + 0xA)) = (u16)y;
    DSW((u16)(r3 + 0xE)) = (u16)y1;
    DSB(r3) |= 1;
    if (DSS((u16)(r2 + 0xA)) <= DSS((u16)(r4 + 0xE))) DSW((u16)(r4 + 0xE)) = (u16)(DSS((u16)(r2 + 0xA)) - 1);
    ui_sort_screen(0x14);
}

/* 0001:a3ba opponent_car_view: a stock car of his model in the engine bay view */
static void opponent_car_view(void)
{
    ui_push(6);
    arena_reset_stacks();
    u16 c = RACE_STK_CAR;
    memset(mp(DGROUP, c), 0, CAR_BYTES);
    memset(mp(DGROUP, RACE_STK_PARTS), 0, 10 * PART_BYTES);
    DSW((u16)(c + CR_TRANS)) = RACE_STK_PARTS;
    for (u16 i = 0; i < 5; i++) DSW((u16)(c + CR_ENGINE + 2 * i)) = (u16)(RACE_STK_PARTS + 8 * (1 + i));
    DSW((u16)(c + CR_TYRES)) = (u16)(RACE_STK_PARTS + 8 * 6);
    DSW((u16)(c + CR_EXHAUST)) = (u16)(RACE_STK_PARTS + 8 * 7);
    DSW((u16)(c + CR_MUFFLER)) = (u16)(RACE_STK_PARTS + 8 * 8);
    DSW((u16)(c + CR_DIFF)) = (u16)(RACE_STK_PARTS + 8 * 9);
    car_new(DSS(DS_opp_model), 0, c);
    engine_bay(c);
    FarPtr save = screen_save_rect(0x18, 0, 200, 0xBD, far_make(0, 0));
    bay_draw_parts(0, 0);
    cursor_ctl(-2);
    status_print(-1, ds_str(0x5062), 0);
    wait_ticks(800);
    DSW(DS_g_aux_b) = 0;
    DSW(DS_g_aux_a) = 0;
    screen_put_bitmap_mirror(save, 0x18, 0);
    arena_reset_stacks();
    ui_pop(1);
}

/* 0001:a4ea bob_sign_draw */
static void bob_sign_draw(void)
{
    ds_far_wr(DS_dancer_sprite, pic_get(0x47D, 2));
    dancer_blit(g_back());
    arena_pop(1);
}

/* 0001:a588 bob_screen_draw */
static void bob_screen_draw(void)
{
    pal_load(DS_pal_menu);
    DSW(DS_g_mirror) = 0;
    show_picture(0x431, -1);
    bob_sign_draw();
    if (DSS(DS_driver_id) == -2 || DSS(DS_driver_id) == -6) drv_pal_black();
    if (DSS(DS_driver_id) == -2) { ega_show_page(1); drv_pal_normal(); }
    page_copy_rect(g_back(), g_front(), 0, 0, 0x13F, 0xBD);
    if (DSS(DS_driver_id) == -2) ega_show_page(0); else drv_pal_normal();
    DSW(DS_g_mirror) = 1;
    status_label(ds_str(DSW(DS_status_money)));
}

/* 0001:a9fc jail */
s16 jail(void)
{
    s16 mode = 4;
    DSW(DS_g_mirror) = 0;
    pal_load(DS_pal_menu);
    cursor_ctl(-3);
    arena_reset();
    ds_far_wr(0xADAA, pic_get(0x481, 1));
    ds_far_wr(0x9750, pic_get(0x483, 1));
    show_picture(0x431, -1);
    FarPtr bars = pic_get(0x482, 2);
    Rect r = { (s16)desc_w(bars), 5, 0, 0, 0, 0 };
    drv_blit(bars, g_back(), &r, 1);
    r.h = (s16)(desc_h(bars) - 5); r.sy = 5; r.dy = (s16)(0xBE - r.h);
    drv_blit(bars, g_back(), &r, 1);
    for (u16 e = 0x506A; e < 0x50CC; e = (u16)(e + 14)) {   /* 7 entries; the original's si = e + 6 runs while si < 0x50D2 after the step */
        Rect d = { DSS((u16)(e + 6)), DSS((u16)(e + 8)), DSS((u16)(e + 2)), DSS((u16)(e + 4)), DSS((u16)(e + 10)), DSS((u16)(e + 12)) };
        drv_blit(ds_far(DSW(e)), g_back(), &d, 1);
    }
    status_print(-1, ds_str(0x5067), 0);
    present_back_page();
    DSW(DS_g_mirror) = 1;
    status_label(ds_str(DSW(DS_status_money)));
    DSW(DS_opp_present) = 0;
    DSW(DS_carhop_state) = 0xFFFF;
    cursor_ctl(1);
    cursor_ctl(-1);
    DSS(DS_location) = 6;
    for (;;) {
        ui_push(0x15);
        set_deadline(mode == 4 ? 0x8C : 0x118);
        s16 pct = mode == 4 ? 0 : king_chance();
        s16 b = wait_event();
        if (b == 1) break;
        if (b == 1000) {
            if (rnd(100) < 6) { cursor_ctl(-4); status_print(-1, ds_str(0x5068), 0); cursor_ctl(-2); }
            opponent_select(pick_opponent(pct, 1), mode, 1);
            mode = 1;
        }
        ui_pop(0);
    }
    ui_pop(1);
    DSS(DS_location) = 1;
    return 0;
}

/* 0001:a626 bob_drive_in: the main loop of Burger's Drive-In */
void bob_drive_in(void)
{
    s16 mode = 4, seen = 0;
    bool quick = false;
    status_print(-1, ds_str(0x5063), 0);
    DSW(DS_cursor_x) = 0x9E;
    DSW(DS_cursor_y) = 0x80;
    bob_screen_draw();
    DSW(DS_ptr_last_x1) = 0xFFFF;
    cursor_ctl(1);
    cursor_ctl(-2);
    DSW(DS_carhop_state) = 0xFFFF;
    DSW(DS_bob_first_visit) = 1;
    DSS(DS_location) = 2;
    DSB(DS_click_held) = 0;
    DSB(DS_joy_buttons) = 0;
    DSW(DS_opp_present) = 0;
    DSW(DS_mouse_left) = 0;
    for (;;) {
        bob_hotspots(DSS(DS_opp_present));
        ui_push(0x14);
        set_deadline(quick ? 5 : mode == 4 ? 0x8C : 0x118);
        quick = false;
        s16 pct = mode == 4 ? 0 : king_chance();
        summer_over_check();
        if (DSW(DS_game_over)) return;
        s16 b = wait_event();
        ui_pop(1);
        switch (b) {
        case 1:                                             /* back home */
            status_print(-1, ds_str(0x5064), 0);
            DSS(DS_location) = 1;
            drive_to_event();
            return;
        case 2:                                             /* challenge */
            if (!DSW(DS_opp_present)) { DSL(DS_wait_until) = ticks_now(); goto new_opponent; }
            DSW(DS_status_used) = 1;
            for (;;) {
                status_print(-1, ds_str(0x5066), 0);
                if (!challenge(DSS(DS_vs_king))) goto leaves;
                s16 r = (s16)race_results();
                DSW(DS_in_challenge_race) = 0;
                drv_pal_black();
                opp_palette_set(-1);
                if (MODE != 1 && DSW(DS_bet_button) != 1) DSW(DS_races_total)++;
                if (DSW(DS_vs_king)) {
                    if (!DSW(DS_player_won)) {
                        DSW(DS_king_stage) = 0;
                        DSB(DS_king_ready) = 0;
                        DSW(DS_mulholland_wins) -= 3;
                        DSW(DS_aqueduct_wins) -= 2;
                    } else if (MODE == 1) DSW(DS_king_stage) = 1;
                    else if (MODE == 0x10) DSW(DS_king_stage) = 2;
                    else { ending_trigger(); DSW(DS_game_over) |= 2; return; }
                }
                if (r == 4) { jail(); cursor_ctl(-4); DSW(DS_game_over) |= 4; return; }
                u16 rec = DSW(DS_opp_rec);
                if (DSW(DS_player_won)) {
                    if (MODE == 1) DSW(DS_drag_wins)++;
                    else if (MODE == 0x10) DSW(DS_mulholland_wins)++;
                    else if (MODE == 8) DSW(DS_aqueduct_wins)++;
                    if (DSC(DS_player_rating_w) < 8) DSC(DS_player_rating_w)++;
                    if (DSC((u16)(rec + PR_WINR)) > 1) DSC((u16)(rec + PR_WINR))--;
                } else {
                    if (DSC(DS_player_rating_w) > 1) DSC(DS_player_rating_w)--;
                    if (DSC((u16)(rec + PR_WINR)) < 8) DSC((u16)(rec + PR_WINR))++;
                }
                DSC((u16)(rec + PR_RACES))++;
                if (DSW(DS_holeshot)) {
                    if (DSC(DS_player_rating_h) < 8) DSC(DS_player_rating_h)++;
                    if (DSC((u16)(rec + PR_HOLER)) > 1) DSC((u16)(rec + PR_HOLER))--;
                } else {
                    if (DSC(DS_player_rating_h) > 1) DSC(DS_player_rating_h)--;
                    if (DSC((u16)(rec + PR_HOLER)) < 8) DSC((u16)(rec + PR_HOLER))++;
                }
                if (r == 2 || r == 1) { DSS(DS_location) = r != 2 ? 1 : 0; drive_to_event(); return; }
                bob_screen_draw();
                mode = 4;
                DSW(DS_carhop_state) = 0xFFFF;
                DSW(DS_opp_present) = 0;
                seen = 0;
                if (DSW(DS_king_stage) == 0) goto next;
                opponent_select(0x15, 4, 0);               /* the King stays for the next stage */
            }
        case 3:                                             /* "See it" */
            if (!DSW(DS_opp_present)) break;
            if (seen > 0 || DSW(DS_vs_king)) { msg_box(0x43); goto leaves; }
            opponent_car_view();
            seen++;
            flow_clock_add(0x111);
            break;
        case 4:
            if (opponent_search(mode)) { seen = 0; break; }
            quick = true;
            break;
        case 1000:                                          /* timeout: somebody drives in, or off */
            summer_over_check();
            if (DSW(DS_game_over)) return;
            if (!DSW(DS_opp_present)) goto new_opponent;
            status_print(-1, ds_str(0x5065), 0);
            goto leaves;
        default:
            break;
        }
        continue;
    leaves:
        opponent_show(2);
        seen = 0;
        continue;
    new_opponent:
        opponent_select(pick_opponent(pct, 0), mode, 2);
        mode = 1;
    next:;
    }
}
