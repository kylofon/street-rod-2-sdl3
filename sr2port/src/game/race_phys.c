/* Race: car set-up (integer), the player physics step of the race ISR, the opponent AI, police, track progress,
 * the race start (port/spec/race.md §3, §4; checked against the decompile / disassembly).
 *
 *   0001:2610 race_ctrl_reset   race_isr 0001:2622 from 28fa (race_phys_step)   0001:b1b2 finish_stats
 *   0001:bc88 phys_reset        0001:be82 race_start      0001:c02c car_perf        0001:c294 car_setup
 *   0001:c5d8 model_car_setup   0001:c6f6 car_max_speed   0001:c866 drivetrain_rating
 *   0001:ca28 opponent_setup    0001:cb00 opp_gear        0001:cb98 opp_ai          0001:ce9c speed_to_step
 *   0001:cee0 player_advance    0001:cf9a opp_dodge_street 0001:d4cc opp_advance_steer
 *   0001:d85c police_spawn      0001:d908 race_step
 *
 * All arithmetic is the original's 16 / 32-bit integer arithmetic (C truncation; `q_shr` = the cdq/xor/sar idiom). */
#include "game/race_int.h"
#include "game/race.h"

#include <string.h>

#include "game/events.h"
#include "platform/platform.h"
#include "platform/video.h"

#define MODE DSS(DS_road_mode)

/* ============================================================================ car set-up */

/* The fields of a car record car_perf / car_setup read. The original reads them through a car_t*, either a car of
 * the pool or the temporary stock car of c5d8 / c6f6 on the stack. */
typedef struct {
    s16 model;
    u16 flags;                  /* +26 */
    s8  timing;                 /* +23 */
    s16 eng_wear;               /* engine +02 */
    s16 e, tr, ty, man, carb0, exh, muf, dif;   /* grades; muf = -1 without a muffler */
} CarView;

static s16 grade_t(s16 type) { return parts_grade(type); }

static void car_view_of(u16 car, CarView *v)
{
    v->model = CARS(car, CR_MODEL);
    v->flags = CARW(car, CR_FLAGS);
    v->timing = CARC(car, CR_IGNITION);
    v->eng_wear = PARTS(CARW(car, CR_ENGINE), PT_WEAR);
    v->e = grade_of(CARW(car, CR_ENGINE));
    v->tr = grade_of(CARW(car, CR_TRANS));
    v->ty = grade_of(CARW(car, CR_TYRES));
    v->man = grade_of(CARW(car, CR_MANIFOLD));
    v->carb0 = grade_of(CARW(car, CR_CARB));
    v->exh = grade_of(CARW(car, CR_EXHAUST));
    v->muf = CARW(car, CR_MUFFLER) ? grade_of(CARW(car, CR_MUFFLER)) : -1;
    v->dif = grade_of(CARW(car, CR_DIFF));
}

/* the temporary stock car of 0001:c5d8 / c6f6: engine, transmission, manifold, carburettors, tyres from spec A, all
 * with the wear DS:4790 (-128), timing 0. PORT (spec §6 open question): the original leaves the flags word +26 and the
 * exhaust / muffler / differential pointers +1C/+1E/+20 uninitialised (stack garbage); the port uses flags 0 and the
 * spec-B stock parts of car_new 0001:42aa. */
static void car_view_stock(s16 m, CarView *v)
{
    u16 a = model_specA(m), b = model_specB(m);
    s16 make = (s16)((a & 0xE) >> 1);
    s16 tg = (s16)((a & 0xC0) >> 6);
    s16 mg = (s16)((a >> 10) & 0xF);
    v->model = m;
    v->flags = 0;
    v->timing = 0;
    v->eng_wear = DSS(0x4790);
    v->e = grade_t((s16)(((a >> 8) & 3) * 3 + make));
    v->tr = grade_t((s16)((tg == 3) + (tg + 3) * 3 + make));    /* the SR1 "+1 for racing" quirk kept */
    v->ty = grade_t((s16)(((a & 0x30) >> 4) + 0x32));
    v->man = grade_t((s16)(make + (mg + 8) * 3));
    v->carb0 = grade_t((s16)((a >> 14) + 0x15));
    v->exh = grade_t((s16)((b & 0x1F) * 3 + make + 0x35));
    v->muf = grade_t((s16)(((b & 0x3E0) >> 5) + 0x3B));
    v->dif = grade_t((s16)((b >> 10) + 0x3D));
}

/* 0001:c02c car_perf: integer performance "percent" */
static s32 car_perf(const CarView *c)
{
    s16 m = c->model;
    s16 cls = model_cls(m);
    s16 carb = DSS((u16)(0x4E6E + 2 * c->man)) > 0 ? c->carb0 : 0;
    s16 roof  = ((c->flags & 0x2000) || OVL_ROOF(m) == 0) ? 1 : 0;
    s16 front = ((c->flags & 0x8000) || OVL_FRONT(m) == 0) ? 1 : 0;
    s16 rear  = ((c->flags & 0x4000) || OVL_REAR(m) == 0) ? 1 : 0;
    s16 t = c->timing;
    s16 s = (s16)(((s16)(DSS((u16)(0x5294 + 2 * (carb * 9 + c->man))) + DSS((u16)(0x52D0 + 2 * c->tr)))) * 0xF
                + (s16)((cdiv16((s16)(abs16(t) * 4), -3) + 2) * 0x2C + DSS((u16)(0x52EC + 2 * cls))) * 10
                + (s16)(((roof * 2 + front) * 4 + rear) * 4 + DSS((u16)(0x52CA + 2 * c->e))) * 0x19);
    s32 S = s;
    if (c->dif == 0)      S += (S * 5) / 100;
    else if (c->dif == 2) S += (S * 5) / -100;
    S /= 100;
    S *= (s32)(s16)(10000 - c->eng_wear);
    S /= 10000;
    S *= (u16)c->tr < 1 ? 8 : 10;
    return S * 3 / 100 + 0x46;
}

/* 0001:c294 car_setup core */
static void car_setup_view(const CarView *c, u16 p)
{
    s32 f = car_perf(c);
    s32 g = f * 13 / 10;
    if (g > 0x82) g = 0x82;
    u16 last = 0;
    for (s16 gear = 0; gear <= 4; gear++) {
        s32 r = (s32)DSS((u16)(0x5242 + 2 * (c->tr * 5 + gear))) * g;
        if (r == 0) r = 0x14;
        PHW(p, PH_RATIO + gear * 2) = (u16)(570000L / r);
        u16 b = (u16)(p + CRV(gear, 0));
        last = b;
        DSW(b + 0) = 0;
        DSW(b + 2) = DSW(b + 8) = DSW((u16)(0x5288 + c->e * 4));
        DSW(b + 0x0A) = DSW(b + 0x10) = DSW((u16)(0x5288 + c->e * 4 + 2));
        DSW(b + 0x12) = 0x1644;
        s16 v = (s16)((s32)DSS((u16)(0x526A + 2 * (c->e * 5 + gear))) * f / 100);
        s16 up = cdiv16(v, (s16)(gear == 0 ? 1 : 5));
        if (gear == 1) up = (s16)(up * (c->ty == 2 ? 3 : 2));
        if (c->muf < 0)       up = (s16)(up + cdiv16((s16)(up * 2 * 2 + up), 100));
        else if (c->muf == 1) up = (s16)(up + cdiv16((s16)(up * 2 + up), 100));
        if (c->exh == 1)      up = (s16)(up + cdiv16((s16)(up * 5), 100));
        if (c->dif == 0)      up = (s16)(up + cdiv16((s16)(up * 5), -100));
        else if (c->dif == 2) up = (s16)(up + cdiv16((s16)(up * 5), 100));
        DSW(b + 4) = (u16)up;
        DSW(b + 0x0C) = (u16)v;
        DSW(b + 0x14) = (u16)cdiv16(v, (s16)(gear == 0 ? 1 : 10));
        for (int k = 0; k < 3; k++) DSW((u16)(b + 6 + k * 8)) = (u16)(gear ? cdiv16(v, (s16)(5 - gear)) : v);
    }
    u16 a = c->tr == 0;
    PHW(p, PH_AUTO_MODE) = a;
    PHW(p, PH_AUTOMATIC) = a;
    if (a) {
        PHW(p, PH_F28) = 0xFFFF;
        PHW(p, PH_F26) = 0xFFFF;
        PHW(p, PH_AUTO_SHIFT) = DSW((u16)(last + 0x0A));
    }
    PHW(p, PH_REDLINE) = DSW((u16)(0x52D8 + 2 * c->e));
    PHW(p, PH_GRIP) = (u16)(cdiv16((s16)((0x82 - model_cls(c->model)) * 5), 0x32) + DSS((u16)(0x52E6 + 2 * c->ty)));
    PHW(p, PH_NGEARS) = DSW((u16)(0x52DE + 2 * c->tr));
    PHW(p, PH_GEAR_DIRTY) = 1;
}

/* 0001:c294 car_setup */
void car_setup(u16 car, u16 phys)
{
    CarView v;
    car_view_of(car, &v);
    car_setup_view(&v, phys);
}

/* 0001:c5d8 model_car_setup */
void model_car_setup(s16 model, u16 phys)
{
    CarView v;
    car_view_stock(model, &v);
    car_setup_view(&v, phys);
}

/* 0001:c6f6 car_max_speed (-1 = the current car) */
s16 car_max_speed(s16 model)
{
    CarView v;
    if (model < 0) car_view_of(cur_car(), &v);
    else car_view_stock(model, &v);
    s32 g = car_perf(&v) * 13 / 10;
    if (g > 0x82) g = 0x82;
    return (s16)g;
}

/* 0001:c866 drivetrain_rating */
s16 drivetrain_rating(s16 m)
{
    if (m < 0) return (s16)(grade_of(CARW(cur_car(), CR_ENGINE)) + grade_of(CARW(cur_car(), CR_TRANS)) + 1);
    u16 a = model_specA(m);
    return (s16)(((a >> 8) & 3) + ((a & 0xC0) >> 6) + 1);
}

/* ============================================================================ reset / set-up */

/* 0001:2610 race_ctrl_reset */
void race_ctrl_reset(void)
{
    DSW(DS_ctl_1F44) = 0;
    PHW(PLAYER, PH_WHEEL) = 0;
    PHW(PLAYER, PH_STEER_IN) = 0;
    PHW(PLAYER, PH_RPM_DOWN) = 0;
    PHW(PLAYER, PH_RPM_UP) = 0;
}

/* 0001:bc88 phys_reset */
void phys_reset(u16 p)
{
    DSB(DS_click_edge) = 0;
    PHW(p, PH_WHEEL) = 0; PHW(p, PH_STEER_IN) = 0; PHW(p, PH_BAND) = 0;
    PHW(p, PH_GEAR) = 0; PHW(p, PH_SPEED) = 0; PHW(p, PH_STEPS) = 0; PHW(p, PH_RESULT) = 0;
    if (PHW(p, PH_AUTOMATIC) != 0) {
        u16 r = PHW(p, PH_REDLINE);
        PHW(p, CRV(2, 2) + CRV_LO) = r;
        PHW(p, CRV(2, 1) + CRV_HI) = r;
        PHW(p, PH_AUTO_MODE) = 1;
    }
    PHW(p, PH_F26) = 0xFFFF;
    PHW(p, PH_F28) = 0xFFFF;
    PHW(p, PH_RPM) = 0x5DC;
    PHW(p, PH_CUR_RATIO) = PHW(p, PH_F2A);                      /* quirk kept: +2A, not ratio[0] */
    PHW(p, PH_RPM_UP) = PHW(p, CRV(0, 0) + CRV_UP);
    PHW(p, PH_RPM_DOWN) = PHW(p, CRV(0, 0) + CRV_DOWN);
    PHW(p, PH_F12) = PHW(p, CRV(0, 0) + CRV_HI);
    PHW(p, PH_F10) = PHW(p, CRV(0, 0) + CRV_LO);
    DSW(DS_ctl_1F44) = 0;
    DSW(DS_ctl_1F48) = 0;
    DSW(DS_ctl_1F46) = 0;
    DSW(DS_gear_label_cache) = 0xFFFF;
    switch (MODE) {
    case 0: case 1: DSS(DS_start_seg) = 200; DSS(DS_track_end) = 0x17C; break;
    case 2: case 4: DSS(DS_start_seg) = 8;   DSS(DS_track_end) = 0x58;  break;
    case 8:         DSS(DS_start_seg) = 8;   DSS(DS_track_end) = 0x156; break;
    case 0x10:      DSS(DS_start_seg) = 0x172; DSS(DS_track_end) = 0x3C0; break;
    default: break;
    }
    u16 ty = CARW(cur_car(), CR_TYRES);                         /* always the player's tyres */
    s16 wear = PARTS(ty, PT_WEAR);
    PHW(p, PH_TIRE_SLIP) = DSW((u16)(0x521E + 2 * (grade_of(ty) * 5 + wear / 2000)));
    if (PARTS(ty, PT_WEAR) < 8000 || (s16)(rnd(2000) + PARTS(ty, PT_WEAR)) < 0x251D) PHW(p, PH_BLOWOUT_SEG) = 0;
    else {
        s16 h = sub16(DSS(DS_track_end), DSS(DS_start_seg));
        PHW(p, PH_BLOWOUT_SEG) = (u16)(rnd((s16)(h >> 2)) + (h >> 1) + DSS(DS_start_seg));
    }
    DSB(DS_tires_blown) = 0;
    DSW(DS_ai_steps_done) = 0;
    s16 vmax = car_max_speed(DSS(DS_opp_model));
    DSS(DS_opp_vmax) = vmax;
    DSS(DS_opp_v70) = (s16)((s32)vmax * 7000 / 10000);
    DSS(DS_opp_v80) = (s16)((s32)vmax * 8000 / 10000);
    DSS(DS_opp_slack) = DSS(DS_cpu_speed) < 4 ? 10 : 16;
    DSW(DS_ai_yield_toggle) = 0;
    DSW(DS_stats_done) = 0;
    DSW(DS_holeshot_decided) = 0;
}

/* 0001:ca28 opponent_setup */
void opponent_setup(void)
{
    model_car_setup(DSS(DS_opp_model), OPP);
    DSS(DS_opp_skill) = DSC(DSW(DS_opp_rec));
    phys_reset(OPP);
    DSW(DS_opp_green) = 0;
    PHW(OPP, PH_GEAR) = 0;
    PHW(OPP, PH_CUR_RATIO) = PHW(OPP, PH_RATIO);
    PHW(OPP, PH_RPM) = 0x5DC;
    PHS(OPP, PH_SPEED) = PHS(OPP, PH_RATIO) ? (s16)(0x5DC / (s32)PHS(OPP, PH_RATIO)) : 0;
    s16 band = 1;
    u16 crv = (u16)(OPP + CRV(0, 1));
    if (DSS(crv + CRV_HI) < 0x5DD) { band = 2; crv = (u16)(crv + 8); }
    else if (DSS(crv + CRV_LO) > 0x5DC) { band = 0; crv = (u16)(crv - 8); }
    DSS(DS_opp_band_tmp) = band;
    PHS(OPP, PH_BAND) = band;
    PHW(OPP, PH_BAND_HI) = DSW(crv + CRV_HI);
    PHW(OPP, PH_BAND_UP) = DSW(crv + CRV_UP);
    DSS(DS_opp_shift_rpm) = PHW(OPP, PH_AUTO_MODE) == 0 ? (s16)((DSS(DS_opp_skill) - 10) * 200 + PHS(OPP, PH_REDLINE)) : 0x14B4;
    DSS(DS_opp_seg) = DSS(DS_start_seg);
    DSS(DS_opp_lat) = (s16)(((s16)(DSS(DS_ai_edge_right) + DSS(DS_ai_edge_left)) >> 1) - ((MODE & 6) ? 200 : 0) - 200);
    DSS(DS_opp_lat_target) = DSS(DS_opp_lat);
    DSW(DS_opp_seg_frac64) = 0;
    DSW(DS_opp_alongside) = 0;
    DSW(DS_opp_contact) = 0;
}

/* 0001:be82 race_start */
void race_start(void)
{
    cursor_ctl(-4);
    cursor_ctl(1);
    input_reset();
    DSB(DS_racing) = 1;
    phys_reset(PLAYER);
    road_state_init();
    opponent_setup();
    if (MODE == 0x10 && DSW(DS_vs_king) == 0) {
        DSW(DS_police_state) = rnd(4) == 0 ? 1 : 0;
        if (DSW(DS_police_state)) DSS(DS_police_seg) = (s16)(rnd(0xF0) + 0x1E0);
    } else DSW(DS_police_state) = 0;
    DSW(DS_race_state) = 0;
    drive_run();
    race_isr_leave();
    DSL(DS_wait_until) = 0;
    input_reset();
    status_label(ds_str(DSW(DS_status_money)));
    if (DSS(DS_seg) <= DSS(DS_start_seg)) return;               /* never left the line */
    u16 car = cur_car();
    if (rnd(3) > 0) {                                           /* ignition timing drifts */
        s8 t = CARC(car, CR_IGNITION);
        s8 n;
        if (t >= 1) { if (rnd(3) <= t) goto wear; n = (s8)rnd(3); }
        else if (t < 0) { if (t <= -rnd(5)) goto wear; n = (s8)-(s8)rnd(5); }
        else n = (s8)((s8)rnd(6) - 3);
        CARC(car, CR_IGNITION) = n;
    }
wear:;
    u16 ty = CARW(car, CR_TYRES);
    s16 base = grade_of(ty) == 0 ? ((MODE & 7) == 0 ? 700 : 300) : ((MODE & 7) == 0 ? 400 : 200);
    PARTS(ty, PT_WEAR) = (s16)(PARTS(ty, PT_WEAR) + rnd(300) + base);
    if (PARTS(ty, PT_WEAR) > 10000) PARTS(ty, PT_WEAR) = 10000;
}

/* ============================================================================ the physics step */

/* race_isr 0001:2622 from 28fa: the platform's race_isr has counted the ticks, chained the BIOS and the sound, checked
 * DS:1F7A / DS:B7AE / DS:1BCA and the DS:1F77 countdown and the busy flag; this is one player step. */
void race_phys_step(void)
{
    const u16 p = PLAYER;
    bool shift = 0, brake = 0, steer = 0, gas = 0;
    bool robot = DSW(DS_demo) != 0 || DSW(DS_auto_drive) != 0;
    if (DSW(DS_kbd_dir_lock) == 0 && DSW(DS_demo) == 0) {
        s16 kx = DSC(DS_kbd_dir_x);
        if (kx) { steer = 1; DSW(DS_cursor_x) = kx > 0 ? 1 : 0xFFFF; }
        s16 ky = DSC(DS_kbd_dir_y);
        if (ky) { if (ky > 0) brake = 1; else gas = 1; }
        if (DSB(DS_joy_present)) {
            joy_read();
            s16 jx = DSS(DS_joy_x);
            if (jx < DSS(DS_joy_thr_x_lo)) { steer = 1; DSW(DS_cursor_x)--; }
            else if (jx > DSS(DS_joy_thr_x_hi)) { steer = 1; DSW(DS_cursor_x)++; }
            s16 jy = DSS(DS_joy_y);
            if (jy < DSS(DS_joy_thr_y_lo)) { gas = 1; DSW(DS_cursor_y)--; }
            else if (jy > DSS(DS_joy_thr_y_hi)) { brake = 1; DSW(DS_cursor_y)++; }
        }
        if (DSB(DS_mouse_present)) {
            mouse_poll();
            if (DSS(DS_mouse_dy) > 0) { DSW(DS_cursor_y)++; brake = 1; }
            s16 mx = DSS(DS_mouse_dx);
            if (mx) { steer = 1; DSW(DS_cursor_x) += mx > 0 ? 1 : 0xFFFF; }
            if (DSW(DS_mouse_right) != 0) gas = 1;
        }
    }
    shift = DSB(DS_click_held) != 0 && DSB(DS_fire_prev) == 0;
    DSB(DS_fire_prev) = DSB(DS_click_held);
    if (DSB(DS_click_edge) != 0) { shift = 1; DSB(DS_click_edge) = 0; }
    /* PORT: A / Z (kbd_take_shift_dir) is a shift event with a forced direction (port addition of the SR1 port) */
    s8 shift_dir = kbd_take_shift_dir();
    if (shift_dir) shift = 1;
    if (robot) {                                                /* the computer drives */
        if ((s16)(PHS(p, PH_RPM_UP) * 4 + PHS(p, PH_RPM)) < 0x1644) {
            if ((MODE & 6) || DSS(DS_opp_seg) > DSS(DS_seg) || DSS(DS_seg) > (s16)(DSS(DS_track_end) - 0x3C)) gas = 1;
        } else if (PHS(p, PH_GEAR) < PHS(p, PH_NGEARS) && DSW(DS_light_latch) != 0) {
            gas = 1; shift = 1;
        }
        if (PHS(p, PH_RPM) < 0x7D0 && PHS(p, PH_GEAR) > 1 && !gas) shift = 1;
        if (PHW(p, PH_AUTOMATIC) != 0 && PHS(p, PH_GEAR) > 0) shift = 0;
    }
    if (DSB(DS_racing) != 0) {
        DSL(DS_distance) += (u32)(s32)PHS(p, PH_SPEED);
        PHW(p, PH_STEPS)++;
    }
    if (DSS(DS_track_end) < DSS(DS_seg)) {                      /* past the finish: stop the car */
        brake = 1; steer = 0; gas = 0;
        if (PHS(p, PH_SPEED) > 0xF) PHW(p, PH_SPEED) -= 0xF;
    }
    if (DSS(DS_ai_brake) > 0) {
        brake = 1; steer = 0; gas = 0;
        DSW(DS_ai_brake)--;
    }
    s16 w = 0;
    if (!robot) {
        u16 car = cur_car();
        if (PHS(p, PH_RPM) > 0x1388) {
            PARTS(CARW(car, CR_ENGINE), PT_WEAR)++;
            PARTS(CARW(car, CR_TRANS), PT_WEAR)++;
        }
        if ((DSB(DS_bios_ticks) & 0xF) == 0) PARTS(CARW(cur_car(), CR_ENGINE), PT_WEAR)++;
        w = PARTS(CARW(cur_car(), CR_ENGINE), PT_WEAR);
        if (w >= 0x26AC) { DSB(p + PH_RESULT) |= 5; race_ctrl_reset(); }
    }
    if (gas) {
        PHW(p, PH_RPM) += PHW(p, PH_RPM_UP);
        if (PHW(p, PH_AUTOMATIC) != 0 && PHS(p, PH_RPM) > 0x163A) PHW(p, PH_RPM) = 0x163A;
        if (!robot && PHS(p, PH_RPM) > 0x1644) {
            PHW(p, PH_RPM) = 0x1644;
            s16 lim = (s16)(w / -1000 + 0xF);
            DSW(DS_limiter_count)++;
            if (lim < DSS(DS_limiter_count)) { DSB(p + PH_RESULT) |= 5; race_ctrl_reset(); }
        }
    } else if (PHS(p, PH_RPM) > 0x5DC || (PHS(p, PH_GEAR) > 0 && PHS(p, PH_RPM) > PHS(p, PH_RPM_DOWN))) {
        PHW(p, PH_RPM) -= PHW(p, PH_RPM_DOWN);
    }
    if (PHS(p, PH_RPM) < 0x1644) DSW(DS_limiter_count) = 0;

    bool stall = PHS(p, PH_SPEED) < 5 && !gas && PHS(p, PH_GEAR) > 0;
    bool doshift = shift || stall;
    if (!doshift && PHW(p, PH_AUTOMATIC) != 0 && PHS(p, PH_AUTO_MODE) > 1) {
        if (gas && PHS(p, PH_RPM) > PHS(p, PH_AUTO_SHIFT) && PHS(p, PH_GEAR) < PHS(p, PH_NGEARS)) doshift = 1;
        else if (!gas && PHS(p, PH_GEAR) > 1 && PHW(p, PH_BAND) == 0) doshift = 1;
    }
    if (doshift) {
        u16 trn = CARW(cur_car(), CR_TRANS);
        PARTS(trn, PT_WEAR) = (s16)(PARTS(trn, PT_WEAR) + (PHS(p, PH_RPM) > 0x1388 ? 0xF : 3));
        if (DSW(DS_demo) == 0) {
            s16 tw = PARTS(CARW(cur_car(), CR_TRANS), PT_WEAR);
            bool blow = tw >= 0x26AC;
            if (!blow && PHW(p, PH_AUTOMATIC) == 0 && gas && PHS(p, PH_RPM) > 0x1388) {
                s16 r = rnd(0xCCD);
                if ((s16)(tw / 100 + 100) > r) blow = 1;
            }
            if (blow) { DSB(p + PH_RESULT) |= 0x11; race_ctrl_reset(); }
        }
        if (shift_dir > 0) {                                        /* PORT: A */
            if (PHS(p, PH_GEAR) < PHS(p, PH_NGEARS)) PHW(p, PH_GEAR)++;
        } else if (shift_dir < 0) {                                 /* PORT: Z */
            if (PHS(p, PH_GEAR) > 0) PHW(p, PH_GEAR)--;
        } else if ((brake || (PHW(p, PH_BAND) == 0 && !gas)) && PHS(p, PH_GEAR) > 0) {
            PHW(p, PH_GEAR)--;
        } else if ((gas || (PHS(p, PH_BAND) > 0 && !brake)) && PHS(p, PH_GEAR) < PHS(p, PH_NGEARS)) {
            PHW(p, PH_GEAR)++;
        }
        if (PHW(p, PH_AUTOMATIC) != 0) {
            if (shift_dir) PHW(p, PH_AUTO_MODE) = shift_dir > 0 ? 2 : 1;   /* PORT: A = D, Z = N */
            else if (shift) PHW(p, PH_AUTO_MODE) = PHW(p, PH_AUTO_MODE) == 1 ? 2 : 1;
            if (stall || PHW(p, PH_GEAR) == 0) PHW(p, PH_AUTO_MODE) = 1;
            if (PHW(p, PH_AUTO_MODE) == 1) PHW(p, PH_GEAR) = 0;
        } else if (stall) {
            PHW(p, PH_GEAR) = 0;
        }
        PHW(p, PH_GEAR_DIRTY) = 1;
        PHW(p, PH_CUR_RATIO) = PHW(p, (u16)(PH_RATIO + PHS(p, PH_GEAR) * 2));
        PHW(p, PH_RPM) = (u16)(PHS(p, PH_SPEED) * PHS(p, PH_CUR_RATIO));
        if (PHW(p, PH_RPM) == 0) PHW(p, PH_RPM) = 0x5E1;
    }
    s16 k = -1;
    u16 crv;
    do {
        k++;
        crv = (u16)(p + PH_CURVE + PHS(p, PH_GEAR) * 0x18 + k * 8);
    } while (k < 2 && DSS(crv + CRV_HI) < PHS(p, PH_RPM));
    if (k != PHS(p, PH_BAND) || PHW(p, PH_GEAR_DIRTY) != 0) {
        PHW(p, PH_BAND) = (u16)k;
        PHW(p, PH_RPM_UP) = DSW(crv + CRV_UP);
        PHW(p, PH_RPM_DOWN) = DSW(crv + CRV_DOWN);
    }
    if (brake || (DSW(DS_on_shoulder) != 0 && PHS(p, PH_SPEED) > 8)) {
        s16 v = (s16)(PHS(p, PH_SPEED) - ((PHS(p, PH_SPEED) >> 4) + 2));
        if (v < 0) v = 0;
        PHS(p, PH_SPEED) = v;
        PHW(p, PH_RPM) = (u16)(v * PHS(p, PH_CUR_RATIO));
        if (PHS(p, PH_RPM) < 0x5DC) PHW(p, PH_RPM) = 0x5DC;
    } else if (PHW(p, PH_GEAR) == 0 && PHW(p, PH_SPEED) != 0) {
        PHW(p, PH_SPEED)--;
        PHW(p, PH_RPM) = (u16)(PHS(p, PH_SPEED) * 0x1E + 0x5DC);
    } else if (PHW(p, PH_CUR_RATIO) != 0) {
        PHS(p, PH_SPEED) = cdiv16(PHS(p, PH_RPM), PHS(p, PH_CUR_RATIO));
    }
    s16 ss = DSS(DS_cursor_x);
    if (steer && abs16((s16)(PHS(p, PH_STEER_IN) + ss)) > abs16(PHS(p, PH_STEER_IN))) {
        s16 si = abs16(PHS(p, PH_STEER_IN));
        s16 di = abs16(PHS(p, PH_WHEEL));
        if ((s16)(abs16(ss) + di) == abs16((s16)(PHS(p, PH_WHEEL) + ss)) && si < di)
            PHW(p, PH_STEER_IN) = PHW(p, PH_WHEEL);
        PHW(p, PH_STEER_IN) += (u16)(ss * 2);
        if (abs16(PHS(p, PH_STEER_IN)) > 0x18) PHS(p, PH_STEER_IN) = PHS(p, PH_STEER_IN) > 0 ? 0x18 : -0x18;
        DSW(DS_cursor_x) = 0;
        PHW(p, PH_WHEEL) = PHW(p, PH_STEER_IN);
    } else {
        PHW(p, PH_STEER_IN) = 0;
    }
    if (abs16(PHS(p, PH_WHEEL)) > 0) {
        s16 wh = PHS(p, PH_WHEEL);
        PHS(p, PH_WHEEL) = (s16)(wh + (wh > 0 ? -((wh >> 2) + 1) : ((s16)(-wh) >> 2) + 1));
    }
    if (DSB(DS_racing) != 0 && road_edge_collision()) {
        DSB(p + PH_RESULT) |= 0x21;
        race_ctrl_reset();
    }
}

/* ============================================================================ opponent */

/* 0001:cb00 opp_gear: lowest gear whose band-1 top is above the rpm for speed v */
void opp_gear(s16 v, u16 p)
{
    PHW(p, PH_GEAR) = 1;
    u16 ratio = (u16)(p + 0x36), b1 = (u16)(p + 0x64), b0 = (u16)(p + 0x5C), b2 = (u16)(p + 0x6C);
    u16 si;
    for (;;) {
        PHW(p, PH_CUR_RATIO) = DSW(ratio);
        PHW(p, PH_RPM) = (u16)(PHS(p, PH_CUR_RATIO) * v);
        si = b1;
        if (DSS(si + CRV_HI) > PHS(p, PH_RPM)) {
            u16 di = PHS(p, PH_RPM) >= DSS(si) ? 1 : 0;
            PHW(p, PH_BAND) = di;
            if (di == 0) si = b0;
            DSW(DS_opp_band_tmp) = di;
            break;
        }
        if (PHW(p, PH_GEAR) == PHW(p, PH_NGEARS)) {
            si = b2;
            PHW(p, PH_BAND) = 2;
            DSW(DS_opp_band_tmp) = 2;
            break;
        }
        ratio = (u16)(ratio + 2); b1 = (u16)(b1 + 0x18); b0 = (u16)(b0 + 0x18); b2 = (u16)(b2 + 0x18);
        PHW(p, PH_GEAR)++;
    }
    PHW(p, PH_BAND_UP) = DSW(si + CRV_UP);                          /* band_hi NOT updated (quirk) */
    PHS(p, PH_SPEED) = cdiv16(PHS(p, PH_RPM), PHS(p, PH_CUR_RATIO));
}

static s16 smax(s16 a, s16 b) { return a > b ? a : b; }

/* 0001:cb98 opp_ai: the opponent's engine, once per player physics step since the last frame */
static void opp_ai(void)
{
    if (MODE == 2 && DSS(DS_opp_seg) >= 0x50) {                 /* time-run reference car stops at seg 0x50 */
        if (DSW(DS_timerun_ref_steps) == 0) DSW(DS_timerun_ref_steps) = PHW(PLAYER, PH_STEPS);
        PHW(OPP, PH_SPEED) = 0;
        return;
    }
    s16 v0 = PHS(OPP, PH_SPEED), lim = 0;
    s16 n = (s16)(PHS(PLAYER, PH_STEPS) - DSS(DS_ai_steps_done));
    DSW(DS_ai_steps_done) = PHW(PLAYER, PH_STEPS);
    if ((MODE & 0x10) && (TRKB(TRK_C, DSS(DS_opp_seg)) & 0x60) && PHS(OPP, PH_SPEED) > 0x3B) {
        opp_gear(0x3B, OPP);                                    /* Mulholland slow section */
        goto tail;
    }
    for (s16 i = 0; i < n; i++) {
        s16 os = DSS(DS_opp_seg);
        if (!(MODE & 0xE) && TRKS(TRK_YAW, os) != TRKS(TRK_YAW, os + 1)) {
            if (DSW(DS_ai_curve_braked) == 0) {
                DSW(DS_ai_curve_braked) = 1;
                lim = (s16)(DSS(DS_opp_vmax) - (DSS(DS_opp_seg) > DSS(DS_seg) ? 0x1E : 3));
                if (lim < PHS(OPP, PH_SPEED)) opp_gear(lim, OPP);
                continue;
            }
        } else DSW(DS_ai_curve_braked) = 0;
        if (!(MODE & 6)) {
            s16 d = (s16)(DSS(DS_opp_seg) - DSS(DS_seg));
            if (d > 0xC) {
                lim = (PHS(PLAYER, PH_SPEED) >= 0x61 || DSW(DS_vs_king) != 0) ? DSS(DS_opp_v80) : DSS(DS_opp_v70);
                if (lim < PHS(OPP, PH_SPEED)) { opp_gear(lim, OPP); continue; }
            } else if (d > -5 && d < 5) {
                lim = (s16)(PHS(PLAYER, PH_SPEED) + DSS(DS_opp_slack));
                if (DSS(DS_seg) >= 0x1E && lim < DSS(DS_opp_v70)) lim = DSS(DS_opp_v70);
                if (lim < PHS(OPP, PH_SPEED)) { opp_gear(lim, OPP); continue; }
            }
            if (DSS(DS_police_state) >= 2 && PHS(PLAYER, PH_SPEED) > 0x32) {
                lim = (s16)(PHS(PLAYER, PH_SPEED) + 0x20);
                if (lim < PHS(OPP, PH_SPEED)) { opp_gear(lim, OPP); continue; }
            }
        }
        PHW(OPP, PH_RPM) += PHW(OPP, PH_BAND_UP);
        if (PHS(OPP, PH_RPM) > DSS(DS_opp_shift_rpm)) PHW(OPP, PH_RPM) = DSW(DS_opp_shift_rpm);
        if (PHS(OPP, PH_RPM) > PHS(OPP, PH_BAND_HI)) {
            if (PHS(OPP, PH_BAND) < 2) PHW(OPP, PH_BAND)++;
            u16 c = (u16)(OPP + PH_CURVE + PHS(OPP, PH_GEAR) * 0x18 + PHS(OPP, PH_BAND) * 8);
            PHW(OPP, PH_BAND_HI) = DSW(c + CRV_HI);
            PHW(OPP, PH_BAND_UP) = DSW(c + CRV_UP);
        }
        if (PHS(OPP, PH_GEAR) < PHS(OPP, PH_NGEARS) &&
            ((PHW(OPP, PH_AUTO_MODE) != 0 && PHS(OPP, PH_RPM) >= PHS(OPP, PH_AUTO_SHIFT)) ||
             PHS(OPP, PH_RPM) >= DSS(DS_opp_shift_rpm))) {
            PHW(OPP, PH_GEAR)++;
            if (PHW(OPP, PH_AUTO_MODE) == 0) {
                if (PHS(OPP, PH_GEAR) >= PHS(OPP, PH_NGEARS))
                    DSW(DS_opp_shift_rpm) = (DSS(DS_cpu_speed) >= 4 || DSS(DS_driver_id) == -6) ? 0x1518 : 0x14B4;
                else
                    DSS(DS_opp_shift_rpm) = (s16)((DSS(DS_opp_skill) - 10) * 200 + PHS(OPP, PH_REDLINE));
            }
            PHW(OPP, PH_CUR_RATIO) = PHW(OPP, (u16)(PH_RATIO + PHS(OPP, PH_GEAR) * 2));
            PHW(OPP, PH_RPM) = (u16)(PHS(OPP, PH_SPEED) * PHS(OPP, PH_CUR_RATIO));
            DSW(DS_opp_band_tmp) = 1;
            u16 c = (u16)(OPP + PH_CURVE + PHS(OPP, PH_GEAR) * 0x18 + 8);
            if (DSS(c + CRV_HI) <= PHS(OPP, PH_RPM)) { DSW(DS_opp_band_tmp) = 2; c = (u16)(c + 8); }
            else if (DSS(c + CRV_LO) > PHS(OPP, PH_RPM)) { DSW(DS_opp_band_tmp) = 0; c = (u16)(c - 8); }
            PHW(OPP, PH_BAND) = DSW(DS_opp_band_tmp);
            PHW(OPP, PH_BAND_HI) = DSW(c + CRV_HI);
            PHW(OPP, PH_BAND_UP) = DSW(c + CRV_UP);
        }
        PHS(OPP, PH_SPEED) = cdiv16(PHS(OPP, PH_RPM), PHS(OPP, PH_CUR_RATIO));
    }
tail:
    (void)lim;
    if (MODE == 4) return;
    if (DSS(DS_opp_seg) >= DSS(DS_track_end)) {
        s16 dec = (s16)(DSS(DS_seg) + 5) >= DSS(DS_opp_seg) ? 10 : 5;
        PHS(OPP, PH_SPEED) = dec < v0 ? (s16)(v0 - dec) : 0;
    }
}

/* 0001:ce9c speed_to_step: 64ths of a segment per frame */
s16 speed_to_step(s16 v)
{
    if (v <= 0) return 0;
    if (DSS(DS_cpu_speed) < 4) return v <= 0x47 ? (s16)(v + 1) : (s16)((v - 4) * 2);
    if (v > 0x82) v = 0x82;
    return DSB((u16)(0x8016 + v));
}

/* 0001:cee0 player_advance */
static void player_advance(void)
{
    DSS(DS_step_per_frame) = speed_to_step(PHS(PLAYER, PH_SPEED));
    if (MODE == 8) {
        if (DSW(DS_aq_obstacle_latch) == 0) DSW(DS_aq_obstacle_latch) = 1;
        else if (DSS(DS_substep) + DSS(DS_step_per_frame) > 0x38) {
            u16 fl = TRKW(0x4E66, DSS(DS_seg));                 /* AQ[seg + 2] (race.md: [871E]:4E66) */
            if (fl & 0x90) {
                s16 old = DSS(DS_substep);
                DSS(DS_substep) = 0x38;
                if (aq_collide_probe((s16)fl)) {
                    DSS(DS_step_per_frame) = (s16)(0x38 - DSS(DS_substep));
                    DSW(DS_aq_obstacle_latch) = 0;
                } else DSS(DS_substep) = old;
            }
        }
    }
    DSS(DS_substep) = add16(DSS(DS_substep), DSS(DS_step_per_frame));
    if (DSS(DS_substep) > 0x3F) road_advance_segment();
    if (DSS(DS_track_end) <= DSS(DS_seg) && DSW(DS_stats_done) != 0 && PHS(PLAYER, PH_SPEED) < 1) {
        if (DSW(DS_player_ahead) == 0) DSB(PLAYER + PH_RESULT) |= 1;
        else DSB(OPP + PH_RESULT) |= 1;
        race_ctrl_reset();
    }
}

static s16 mid4(void) { return (s16)((s16)(DSS(DS_ai_edge_right) + DSS(DS_ai_edge_left)) >> 1); }

/* 0001:cf9a opp_dodge_street: lateral step around blocked spans (street races, kinds 1 and 0x10) */
static s16 opp_dodge_street(s16 x, s16 dx, s16 s)
{
    bool found = false;
    s16 L = 0, R = 0;
    for (s16 k = 6; k >= 0; k--) {
        s16 j = (s16)(s + k);
        if (TRKB(TRK_C, j) & 0x80) {                            /* special section ahead */
            s16 d = abs16((s16)((s - DSS(DS_seg)) * 8 + DSS(DS_opp_seg_frac64) + DSS(DS_substep)));
            if (d < 0x40 && PHS(OPP, PH_SPEED) > 0x32 && PHS(PLAYER, PH_SPEED) < PHS(OPP, PH_SPEED))
                opp_gear(smax(PHS(PLAYER, PH_SPEED), 0x32), OPP);
            return cdiv16(sub16(mid4(), x), 2);
        }
        if ((TRKW(TRK_C, j) & 0x1004) && PHS(OPP, PH_SPEED) > 0x32) {     /* a car in a lane */
            s16 xo = track_lane_x(j);
            if (!found) { L = xo; R = (s16)(xo + 0x70); }
            else if (L > xo && (s16)(L - xo) < 0xE1) L = xo;
            else if (R < xo && (s16)(xo - R + 0x70) < 0xE1) R = (s16)(xo + 0x70);
            found = true;
            continue;
        }
        if (TRKW(TRK_L, j) & 0x1010) {                          /* obstacle on the left */
            if (!found || (s16)(R + 0xE1) < 0x79E) { L = 0x5DC; R = 0x79E; }
            else if ((s16)(L - 0xE1) < 0x79E) L = 0x5DC;
            found = true;
            continue;
        }
        if (TRKW(TRK_R, j) & 0x1010) {                          /* obstacle on the right */
            if (!found || L > 0xB22) { L = 0xB22; R = 0xCE4; }
            else if (R <= 0xA40) R = 0xCE4;
            found = true;
            continue;
        }
        if ((s16)(j - DSS(DS_seg)) != 1) continue;
        if (k == 0 && found) break;
        if (k < 2 && abs16(sub16(DSS(DS_opp_lat), DSS(DS_cam_x))) < 0x118) {   /* right behind the player */
            if (found && PHS(OPP, PH_SPEED) > 0x28) opp_gear(smax((s16)(PHS(OPP, PH_SPEED) - 0x1E), 10), OPP);
            return 0;
        }
        s16 lo = (s16)(DSS(DS_cam_x) - 0x136), hi = (s16)(DSS(DS_cam_x) + 0x15E);
        if (!found) { L = lo; R = hi; }
        else if (lo < L && R < hi) { L = lo; R = hi; }
        else {
            if ((s16)(L - 0xE1) < hi && R > hi) L = lo;
            if (R < lo && (s16)(R + 0xE1) > lo) R = hi;
        }
        found = true;
    }
    if (!found) return q_shr(dx, 3);
    s16 step = 0;
    s16 a = (s16)(x - 0x82), c = (s16)(x + 0xAA);
    if (L <= c && a <= R) {
        s16 toL = (s16)(c - L + 100), toR = (s16)(R - a + 100);
        if ((s16)(L - 300) < DSS(DS_ai_edge_left)) step = toR;
        else if ((s16)(R + 300) > DSS(DS_ai_edge_right)) step = (s16)-toL;
        else if (toR < toL) step = (s16)(x + toR) < DSS(DS_ai_edge_right) ? toR : (s16)-toL;
        else step = (s16)(x - toL) <= DSS(DS_ai_edge_left) ? toR : (s16)-toL;
    }
    return q_shr(step, 3);
}

/* 0001:d4cc opp_advance_steer: opponent track position and lateral target */
static void opp_advance_steer(void)
{
    if (sub16(DSS(DS_opp_seg), DSS(DS_seg)) == -1 && abs16(sub16(DSS(DS_opp_lat), DSS(DS_cam_x))) < 0xB4)
        DSS(DS_opp_seg_step) = 0;
    else DSS(DS_opp_seg_step) = speed_to_step(PHS(OPP, PH_SPEED));
    if (DSW(DS_aq_bounce_flag) == 0) {
        DSS(DS_opp_seg_frac64) = add16(DSS(DS_opp_seg_frac64), DSS(DS_opp_seg_step));
        if (DSS(DS_opp_seg_frac64) > 0x3F && DSS(DS_opp_seg_step) > 0) {
            DSS(DS_opp_seg) = add16(DSS(DS_opp_seg), (s16)(DSS(DS_opp_seg_frac64) >> 6));
            DSW(DS_opp_seg_frac64) &= 0x3F;
        }
    }
    if (DSS(DS_track_end) <= DSS(DS_opp_seg) && DSW(DS_stats_done) != 0 && PHS(PLAYER, PH_SPEED) < 1) {
        if (DSW(DS_player_ahead) == 0) DSB(PLAYER + PH_RESULT) |= 1;
        else DSB(OPP + PH_RESULT) |= 1;
        race_ctrl_reset();
        return;
    }
    bool fl;
    s16 c;
    if (MODE == 8) { c = 0; fl = false; }
    else {
        fl = ((TRKB(TRK_C, DSS(DS_opp_seg) + 4) | TRKB(TRK_C, DSS(DS_opp_seg))) & 0x80) != 0;
        c = sub16(TRKS(TRK_YAW, DSS(DS_seg) + 4), TRKS(TRK_YAW, DSS(DS_seg) + 5));
    }
    s16 d = sub16(DSS(DS_opp_seg), DSS(DS_seg));
    s16 lat = DSS(DS_cam_x);
    s16 mid = mid4();
#define TARGET DSS(DS_opp_lat_target)
#define SIDE DSW(DS_ai_side)
#define RECENTRE DSW(DS_ai_recentre)
    if (MODE & 4) goto move;
    if (d > 0xC) {
        TARGET = mid;
        if (c) SIDE = c < 0;
        RECENTRE = 1;
        goto move;
    }
    if (d > 6) {
        DSW(DS_bump_allowed) = 1;
        if (c) goto curve_side;
        if (fl) TARGET = mid;
        else TARGET = SIDE ? (s16)(mid - 200) : (s16)(mid + 300);
        goto move;
    }
    if (d > (DSS(DS_cpu_speed) < 4 ? 4 : 2)) {
        if (c) goto curve_side;
        if (fl) { TARGET = mid; goto move; }
        if (!RECENTRE) goto move;
        TARGET = SIDE ? (s16)(mid + 300) : (s16)(mid - 200);
        RECENTRE = 0;
        SIDE = SIDE == 0;
        goto move;
    }
    if (d > -4) {
        if (DSS(DS_opp_seg_step) > DSS(DS_step_per_frame)) {
            if (DSS(DS_opp_sprite) >= 9) {                      /* side view: overtaking */
                s16 dl = sub16(DSS(DS_opp_lat), lat);
                if ((dl > -0xB4 && dl < 0xB4) || fl) {          /* no room: drop back */
                    DSS(DS_opp_seg) = (s16)(DSS(DS_seg) - 2);
                    opp_gear(smax(PHS(PLAYER, PH_SPEED), 0x32), OPP);
                }
                if (sub16(lat, DSS(DS_edge_left)) < (s16)-sub16(lat, DSS(DS_edge_right))) { SIDE = 0; TARGET = (s16)(lat + 0x212); }
                else { SIDE = 1; TARGET = (s16)(lat - 0x14A); }
                goto move;
            }
            if (DSS(DS_opp_sprite) != 0 && DSS(DS_opp_sprite) != 2) goto move;
            TARGET = lat <= DSS(DS_opp_lat) ? (s16)(lat + 0x212) : (s16)(lat - 0x14A);
            goto move;
        }
        if (DSS(DS_opp_sprite) != 0) goto move;
        TARGET = (s16)(lat - 0x14A);
        goto move;
    }
    if (d > -6) {
        DSW(DS_bump_allowed) = 1;
        if (c < 0) { TARGET = (s16)(mid - 100); SIDE = 1; goto move; }
        if (c > 0) { TARGET = (s16)(mid + 400); SIDE = 0; goto move; }
        TARGET = fl ? (s16)(mid + 300) : (SIDE ? mid : (s16)(mid + 500));
        goto move;
    }
    TARGET = (s16)(mid + 200);
    RECENTRE = 1;
    goto move;
curve_side:
    if (c < 0) { TARGET = (s16)(mid - 200); SIDE = 1; }
    else       { TARGET = (s16)(mid + 300); SIDE = 0; }
move:;
    s16 step;
    if (MODE & 0x11) step = opp_dodge_street(DSS(DS_opp_lat), sub16(TARGET, DSS(DS_opp_lat)), DSS(DS_opp_seg));
    else if (MODE == 8) step = aq_lane_target(DSS(DS_opp_lat), sub16(TARGET, DSS(DS_opp_lat)), DSS(DS_opp_seg), DSS(DS_cam_x), DSS(DS_seg));
    else step = (s16)(sub16(TARGET, DSS(DS_opp_lat)) >> 3);
    DSS(DS_opp_lat) = add16(DSS(DS_opp_lat), step);
    DSS(DS_lat_last_opp_step) = DSS(DS_cam_x);
#undef TARGET
#undef SIDE
#undef RECENTRE
}

/* 0001:d85c police_spawn: the opponent turns into the police car */
static void police_spawn(void)
{
    s16 v = PHS(OPP, PH_SPEED);
    DSW(DS_opp_rec) = PERSON(10);
    model_car_setup(0x15, OPP);
    DSS(DS_opp_skill) = DSC(DSW(DS_opp_rec));
    opp_gear(v, OPP);
    opp_palette_set(5);
    memmove(mp(DGROUP, DS_opp_sprites), mp(DGROUP, DS_police_sprites), 0x30);
    s16 vmax = car_max_speed(0x15);
    DSS(DS_opp_vmax) = vmax;
    DSS(DS_opp_v70) = (s16)((s32)vmax * 6000 / 10000);
    DSS(DS_opp_v80) = (s16)((s32)vmax * 8000 / 10000);
}

/* 0001:b1b2 finish_stats: once per race */
void finish_stats(void)
{
    if (DSW(DS_stats_done) != 0) return;
    s32 steps = PHS(PLAYER, PH_STEPS);
    if (steps == 0) steps = 1;                                  /* PORT: _aFldiv by 0 would trap; the race always has steps */
    DSS(DS_avg_speed) = (s16)((s32)DSL(DS_distance) / steps);
    DSS(DS_race_time10) = (s16)(PHS(PLAYER, PH_STEPS) * 10 / 6);
    DSW(DS_player_ahead) = (s16)(sub16(DSS(DS_opp_seg), DSS(DS_seg)) * 8 + q_shr(sub16(DSS(DS_opp_seg_frac64), DSS(DS_substep)), 3)) <= 0 ? 1 : 0;
    if (DSW(DS_player_ahead)) DSS(DS_pct_covered) = 100;
    else {
        s32 num = (s32)(s16)(sub16(DSS(DS_seg), DSS(DS_start_seg)) * 100);
        s32 den = (s16)sub16(DSS(DS_opp_seg), DSS(DS_start_seg));
        s16 pct = den ? (s16)(num / den) : 0;
        if (pct > 99) pct = 99;
        DSS(DS_pct_covered) = pct;
    }
    DSW(DS_stats_done) = 1;
}

/* ============================================================================ per frame */

/* 0001:d908 race_step */
void race_step(void)
{
    bool start_lane = DSW(DS_start_active) && (DSW(DS_start_panel_id) == 0x813 || DSW(DS_start_panel_id) == 0x816);
    if (PHW(PLAYER, PH_AUTO_MODE) == 0) {
        if (DSW(DS_gear_label_cache) != PHW(PLAYER, PH_GEAR)) {
            DSW(DS_gear_label_cache) = PHW(PLAYER, PH_GEAR);
            gear_label(PHS(PLAYER, PH_GEAR));
        }
    } else if (DSW(DS_gear_label_cache) != PHW(PLAYER, PH_AUTO_MODE)) {
        DSW(DS_gear_label_cache) = PHW(PLAYER, PH_AUTO_MODE);
        gear_label((s16)(PHS(PLAYER, PH_AUTO_MODE) + 4));
    }
    if (PHW(PLAYER, PH_BLOWOUT_SEG) != 0 && PHS(PLAYER, PH_BLOWOUT_SEG) < DSS(DS_seg)) DSB(DS_tires_blown) = 1;
    if (DSW(DS_squeal) == 0) { if (abs16(PHS(PLAYER, PH_WHEEL)) > 0xB) DSW(DS_squeal) = 1; }
    else if (abs16(PHS(PLAYER, PH_WHEEL)) < 10) DSW(DS_squeal) = 0;
    if (MODE != 4 || DSS(DS_opp_green) > 0) opp_ai();
    switch (DSW(DS_police_state)) {
    case 1:
        if (DSS(DS_police_seg) <= DSS(DS_seg)) {
            if (DSS(DS_seg) < (s16)(DSS(DS_opp_seg) + 7)) DSW(DS_police_state) = 0;
            else { DSW(DS_police_state) = 2; police_spawn(); }
        }
        break;
    case 2:
        if (DSS(DS_seg) <= (s16)(DSS(DS_opp_seg) + 0xF)) { DSW(DS_police_state) = 3; sound_vec(SND_SIREN, 1); }
        break;
    case 3:
        if (DSS(DS_seg) < (s16)(DSS(DS_opp_seg) - 2)) {       /* the cop passed you: pulled over */
            DSB(PLAYER + PH_RESULT) |= 0x41;
            race_ctrl_reset();
            return;
        }
        break;
    }
    s16 p = (s16)(DSS(DS_seg) * 8 + q_shr(DSS(DS_substep), 3));
    s16 o = (s16)(DSS(DS_opp_seg) * 8 + q_shr(DSS(DS_opp_seg_frac64), 3));
    if (DSW(DS_holeshot_decided) == 0 && (p >= 8 || o >= 8)) {  /* quirk (spec §6): decided at the first call */
        DSW(DS_holeshot_decided) = 1;
        DSW(DS_holeshot) = o < p ? 1 : 0;
    }
    player_advance();
    opp_advance_steer();
    if (MODE & 6) {
        if (DSS(DS_cam_x) < (s16)((0xCE4 + 0x5DC) / 2 + 0x96)) goto lane;    /* crossed the centre line */
    }
    if (start_lane) {
        s16 a = MODE == 8 ? 0xCF8 : 0xCE4, b = MODE == 8 ? 0x5C8 : 0x5DC;
        if ((s16)((a + b) / 2 + 0xA0) > DSS(DS_cam_x)) goto lane;          /* bumped the starter */
    }
    if (DSS(DS_track_end) <= DSS(DS_seg) || DSS(DS_track_end) <= DSS(DS_opp_seg)) finish_stats();
    if (PHW(PLAYER, PH_RESULT) != 0 || PHW(OPP, PH_RESULT) != 0) {
        if (DSS(DS_police_state) >= 2) {
            if (PHW(PLAYER, PH_RESULT) != 0) DSB(PLAYER + PH_RESULT) |= 0x41;
            sound_vec(SND_SIREN, 0);
        }
        race_ctrl_reset();
        return;
    }
    if (DSW(DS_demo) != 0 || DSW(DS_opp_alongside) == 0 || DSS(DS_seg) >= DSS(DS_track_end)) return;
    if (DSW(DS_opp_contact) != 0 && DSS(DS_police_state) <= 1) {
        DSB(PLAYER + PH_RESULT) |= 3;
        race_ctrl_reset();
        return;
    }
    if (DSW(DS_opp_sprite) == 0 && DSS(DS_step_per_frame) > 1) {
        DSW(DS_alongside_count)++;
        if (PHS(PLAYER, PH_STEER_IN) < 0) {                     /* steering left into him */
            s16 seg = DSS(DS_seg);
            if (TRKS(TRK_YAW, seg) < TRKS(TRK_YAW, seg + 1) && mid4() < DSS(DS_cam_x)) return;
            sound_vec(SND_SCREECH);
            if (DSW(DS_ai_yield_toggle) == 0) {
                DSW(DS_opp_seg)--;
                opp_gear(smax((s16)(PHS(OPP, PH_SPEED) - 10), 10), OPP);
                DSW(DS_ai_yield_toggle) = 1;
            } else DSW(DS_ai_yield_toggle) = 0;
            return;
        }
        if (DSW(DS_bump_allowed) == 0 || start_lane || (MODE & 6) || DSS(DS_alongside_count) <= 3 || DSS(DS_police_state) >= 2) return;
        if (rnd((s16)(((15 - DSC((u16)(DSW(DS_opp_rec) + PR_B))) >> 2) + 2)) == 0) {   /* bumped off line */
            sound_vec(SND_SCREECH);
            sound_vec(SND_SCREECH);
            DSS(DS_cam_x) = add16(DSS(DS_cam_x), 300);
            if (PHS(PLAYER, PH_SPEED) > 0x14) DSW(DS_ai_brake) = 2;
            DSW(DS_bump_allowed) = 0;
            DSS(DS_heading) = add16(DSS(DS_heading), rnd(2) == 0 ? -0xF : 0xF);
            sound_vec(SND_SCREECH);
        }
        DSW(DS_alongside_count) = 0;
        return;
    }
    if (DSW(DS_opp_sprite) != 0) DSW(DS_alongside_count) = 0;
    return;
lane:
    DSW(PLAYER + PH_RESULT) |= 0x101;
    race_ctrl_reset();
}

/* ============================================================================ hooks */

#include "modules.h"

void race_register_hooks(void)
{
    modules.race_phys_step = race_phys_step;      /* race_isr 0001:2622 from 28fa */
}
