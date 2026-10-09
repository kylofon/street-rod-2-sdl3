/* Grudge Night (event 17, game_loop code 17): the drag-race tournament. port/spec/game_flow.md 2925:000e-1970,
 * Ghidra decompile port/decomp/sr2_ds.c 41247-42573. The races themselves (race_start / race_results,
 * opponent_load, opp_palette_set) are the race module's (S5.3, see race_stub.c); the framed screens are in
 * event17_screens.c.
 *
 * Table T (DS_tourn_table, 10 x 6 bytes): {s16 id (person, -1 = the player), s16 state, s16 time (dial-in)}.
 * States: 0 still in, -1 out early, 2 lost in round 2 / 3 (races for 3rd place), later +1 steps mark the final places. */
#include "game/events.h"
#include "game/ds_events.h"

#include "game/flow.h"
#include "game/records.h"
#include "game/ui.h"
#include "platform/platform.h"
#include "platform/video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static s16 person_model(s16 id) { return DSS((u16)(DS_people + PERSON_REC * id + 8)); }

/* 2925:000e tournament_pick_field: the 9 unbeaten opponents (the King, #21, excluded) whose car's top speed is
 * closest to the player's */
void tournament_pick_field(void)
{
    s16 spd[21];
    s16 mine = car_max_speed(-1);
    s16 n = 0, worst = 0, worst_d = 0;   /* PORT: `worst` is uninitialised in the original until the first entry with d > 0 */
    for (s16 id = 0; id < 21; id++) {
        if (DSB((u16)(DS_people + PERSON_REC * id + 0x0B)) == 0xFF) continue;     /* already beaten */
        spd[id] = car_max_speed(person_model(id));
        s16 d = (s16)abs(spd[id] - mine);
        if (n < 9) {
            TOURN_ID(n) = id;
            n++;
            if (worst_d < d) { worst = (s16)(n - 1); worst_d = d; }
        } else if (d < worst_d) {                                                 /* replace the farthest one */
            TOURN_ID(worst) = id;
            worst_d = 0;
            for (s16 j = 0; j < n; j++) {
                s16 dj = (s16)abs(spd[TOURN_ID(j)] - mine);
                if (worst_d < dj) { worst_d = dj; worst = j; }
            }
        }
    }
}

/* 2925:00fa tournament_seed: dial-in time of each opponent, the player's qualifier time, sort ascending */
void tournament_seed(void)
{
    u8 ref = DSB(DS_tourn_ref_const);
    TOURN_ID(9) = -1;
    TOURN_STATE(9) = 0;
    TOURN_TIME(9) = DSS(DS_tourn_time);
    DSS(DS_tourn_dialin) = DSS(DS_tourn_time);
    for (s16 i = 0; i < 9; i++) {
        u8 hi = DSB((u16)(DS_MODELS + MODEL_REC * person_model(TOURN_ID(i)) + 0x13));   /* high byte of the flag word */
        s32 prod = (s32)(s16)((hi * 10) / 6) * (s32)DSS(DS_timerun_ref_steps);
        TOURN_TIME(i) = (s16)(ref ? prod / (s32)ref : 0);       /* PORT: no division by zero (the original would trap) */
        TOURN_STATE(i) = 0;
    }
    for (s16 i = 0; i < TOURN_N; i++) {
        s16 best = 32000, bj = i;
        for (s16 j = (s16)(i + 1); j < TOURN_N; j++) {
            if (TOURN_TIME(j) < best) { best = TOURN_TIME(j); bj = j; }
        }
        if (best < TOURN_TIME(i)) {
            s16 id = TOURN_ID(i), st = TOURN_STATE(i), t = TOURN_TIME(i);
            TOURN_ID(i) = TOURN_ID(bj); TOURN_STATE(i) = TOURN_STATE(bj); TOURN_TIME(i) = TOURN_TIME(bj);
            TOURN_ID(bj) = id; TOURN_STATE(bj) = st; TOURN_TIME(bj) = t;
        }
    }
}

/* first entry after `from` (or from itself when `inclusive`) whose state is `st`; TOURN_N when there is none.
 * PORT: the original's loops run on past the table when nothing is found. */
static s16 find_state(s16 from, s16 st)
{
    for (s16 i = from; i < TOURN_N; i++)
        if (TOURN_STATE(i) == st) return i;
    return TOURN_N;
}

/* 2925:01fa tournament_next_round: prepare round DS_tourn_round (0..3): pairings, the opponent of this round */
void tournament_next_round(void)
{
    s16 round = DSS(DS_tourn_round);
    if (round == 0) {
        s16 P = 0;
        while (TOURN_ID(P) >= 0 && P < TOURN_N - 1) P++;
        DSS(DS_tourn_player) = P;
        DSS(DS_tourn_opp) = (P & 1) == 0 ? (s16)(P + 1) : (s16)(P - 1);
        opponent_load(TOURN_ID(DSS(DS_tourn_opp)));
        s16 k = 9;
        if (P == k || k - P == 1) k = (s16)(k - 2);
        if (rnd(100) < 0x3C) TOURN_STATE(k) = -1;
        else TOURN_STATE((s16)(k - 1)) = -1;
    } else {
        s16 P = DSS(DS_tourn_player), o = DSS(DS_tourn_opp);
        if (DSS(DS_tourn_alive) == 0) TOURN_STATE(P) = 2;
        else TOURN_STATE(o) = round == 3 ? 2 : -1;
        if (round == 3) {                                     /* the final and the race for 3rd place */
            s16 a = find_state(0, 0);
            s16 b = find_state((s16)(a + 1), 0);
            if (a < TOURN_N && b < TOURN_N) {
                if (TOURN_ID(a) < 0 || TOURN_ID(b) < 0) {
                    o = TOURN_ID(a) < 0 ? b : a;
                    DSS(DS_tourn_opp) = o;
                    opponent_load(TOURN_ID(o));
                } else {
                    if (rnd(100) < 0x3C) a = b;
                    TOURN_STATE(a)++;
                }
            }
            a = find_state(0, 2);
            b = find_state((s16)(a + 1), 2);
            if (a < TOURN_N && b < TOURN_N) {
                if (TOURN_ID(a) < 0 || TOURN_ID(b) < 0) {
                    o = TOURN_ID(a) < 0 ? b : a;
                    DSS(DS_tourn_opp) = o;
                    opponent_load(TOURN_ID(o));
                } else {
                    if (rnd(100) < 0x3C) a = b;
                    TOURN_STATE(a)++;
                }
            }
        } else {
            for (s16 i = 0; i < TOURN_N; i++) {
                if (TOURN_STATE(i) != 0) continue;
                s16 j = find_state((s16)(i + 1), 0);
                if (j >= TOURN_N) break;                      /* PORT: odd number of undecided entries */
                s16 sel = i;
                if (TOURN_ID(j) < 0 || (sel = j, TOURN_ID(i) < 0)) {
                    DSS(DS_tourn_opp) = sel;                  /* the player's pair: he races the other one */
                    opponent_load(TOURN_ID(sel));
                } else {
                    s16 loser = rnd(100) < 0x3C ? j : i;
                    TOURN_STATE(loser) = round == 2 ? 2 : -1;
                }
                i = j;
            }
        }
    }
    DSS(DS_tourn_opp_dialin) = TOURN_TIME(DSS(DS_tourn_opp));
}

/* strchr / sprintf into the message text in DGROUP (the original patches the message template in place) */
static u16 ds_strchr(u16 s, char c)
{
    while (DSB(s) != (u8)c && DSB(s) != 0) s++;
    return s;
}

/* 2925:1786 tournament_finish: final places, the prize message, the standings screen */
void tournament_finish(void)
{
    s16 ids[4] = { 0, 0, 0, 0 };
    s16 P = DSS(DS_tourn_player), o = DSS(DS_tourn_opp);
    cursor_ctl(-4);
    if (DSS(DS_tourn_round) < 4) {                            /* ended early: the player is last of the placed ones */
        TOURN_STATE(P) = 3;
        s16 k = 0;
        for (s16 i = 0; i < TOURN_N; i++)
            if (TOURN_STATE(i) == 0) TOURN_STATE(i) = k++;
    } else {
        s16 w = DSS(DS_tourn_alive) != 0 ? o : P;
        TOURN_STATE(w)++;
    }
    for (s16 i = 0; i < TOURN_N; i++)
        if (TOURN_STATE(i) >= 0 && TOURN_STATE(i) < 4) ids[TOURN_STATE(i)] = TOURN_ID(i);   /* PORT: bounded to the 4 places */
    s16 place = TOURN_STATE(P);
    if (place < 3) {
        /* message 0x6B, line 2: "Your number ? finish earns``you``$   ." gets the place digit and the prize */
        u16 line = DSW((u16)(DS_msg_table + 6 * 0x6B + 4));
        u16 p = (u16)(ds_strchr(line, 'e') + 3);
        char buf[16];
        snprintf(buf, sizeof buf, "%1d", place + 1);
        memcpy(mp(DGROUP, p), buf, strlen(buf) + 1);
        DSB((u16)(p + strlen(buf))) = ' ';                    /* the NUL of sprintf is blanked again */
        u16 q = (u16)(ds_strchr(p, '$') + 1);
        snprintf(buf, sizeof buf, "%d", DSW((u16)(DS_tourn_prizes + 2 * place)));
        memcpy(mp(DGROUP, q), buf, strlen(buf) + 1);
        DSB((u16)(q + strlen(buf))) = '.';
        msg_box(0x6B);
        money_add((s16)DSW((u16)(DS_tourn_prizes + 2 * place)));
    }
    tournament_screen_build(0);
    screen_put_bitmap(ds_far(DS_tourn_pic_panel), 0x30, 0x18);
    screen_put_bitmap(ds_far(DS_tourn_pic_border), 0x18, 0x1E);
    page_copy_rect(g_back(), g_front(), 0x18, 0x18, 0x11F, 0xBB);
    tournament_standings(ids);
    cursor_ctl(-2);
    wait_ticks(2000);
}

/* 2925:193e event17_leave: back to the garage (location 1), or no scene when the last result was 2 */
void event17_leave(s16 last_result)
{
    drv_pal_black();
    opp_palette_set(-1);
    DSS(DS_location) = (s16)(last_result != 2);
    drive_to_event();
    DSW(DS_road_mode) = 0;
}

static void bump_down(u16 p) { if (DSC(p) > 1) DSC(p)--; }
static void bump_up(u16 p)   { if (DSC(p) < 8) DSC(p)++; }

/* 2925:1970 event17_run (far): Grudge Night. game_loop has taken the $30 stake and set location 4 -> 2. */
void event17_run(void)
{
    s16 r;
    DSW(DS_road_mode) = 2;                                    /* race mode word: 2 qualifier, 4 round */
    DSS(DS_bet_amount) = 0;
    DSS(DS_bet_kind) = 0;
    DSS(DS_tourn_round) = -1;
    tournament_pick_field();
    opponent_load(0x15);                                      /* the King's record is the reference car of the qualifier */
    DSS(DS_vs_king) = 0;
    DSS(DS_timerun_ref_steps) = 0;
    tournament_intro_screen();
    DSS(DS_in_challenge_race) = 1;
    race_start();
    r = (s16)race_results();                                  /* the qualifier: sets the dial-in (DS:B8C2) and DS:9B3E */
    DSS(DS_in_challenge_race) = 0;
    if (DSS(DS_tourn_alive) != 0) {
        tournament_seed();
        DSS(DS_tourn_round) = 0;
        do {
            tournament_next_round();
            tournament_round_screen();
            DSW(DS_road_mode) = 4;
            DSS(DS_in_challenge_race) = 1;
            race_start();
            DSS(DS_in_challenge_race) = 0;
            r = (s16)race_results();
            if (DSS(DS_timerun_ref_steps) == 0) DSS(DS_timerun_ref_steps) = DSB(DS_tourn_ref_const);
            u16 opp = DSW(DS_opp_rec);
            if (DSW(DS_start_flag) == 0) {                    /* who was quicker off the line */
                bump_down(DS_skill_b);
                bump_up((u16)(opp + 0x10));
            } else {
                bump_up(DS_skill_b);
                bump_down((u16)(opp + 0x10));
            }
            if (DSS(DS_tourn_alive) == 0) {
                bump_down(DS_skill_a);
                bump_up((u16)(opp + 0xF));
                if (DSS(DS_tourn_round) < 2) goto leave;      /* lost in round 0 or 1: no standings */
                if (r == 2) break;
            } else {
                bump_up(DS_skill_a);
                bump_down((u16)(opp + 0xF));
            }
            DSS(DS_tourn_round)++;
        } while (DSS(DS_tourn_round) < 4);
        tournament_finish();
        if (TOURN_STATE(DSS(DS_tourn_player)) == 0) DSS(DS_grudge_wins)++;   /* won Grudge Night */
    }
leave:
    event17_leave(r);
}
