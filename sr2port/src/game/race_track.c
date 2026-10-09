/* Track builder, race_render segment 0dcb (port/spec/race_render.md 1d04-2866, 33ac; SR1 race_track.c):
 * the town and the race road at new game / load (track_build_all 0dcb:2866), the course part with its seven
 * feature functions (track_build_course 0dcb:24d8), the bank arrays BL / BR, the drag-strip copies L2 / R2 / C2 and
 * the Aqueduct flags (aq_build 0dcb:33ac).
 *
 * The arrays live in far segment 3726 (TRKW / TRKS, race_int.h), written in place without bounds checks as the
 * original (the course loops are bounded at 0x3DE, the town loop reads L[-1] / R[-2] at i = 0 / 1). Build state
 * DS:96A8 yaw, DS:96AA cursor, DS:96AC yaw step. 16-bit arithmetic (idiv / imul low word). */
#include "game/race_int.h"
#include "game/race.h"

#include "platform/platform.h"

#define L_   TRK_L
#define R_   TRK_R
#define YAW_ TRK_YAW
#define C_   TRK_C
#define HGT_ TRK_HGT

#define POS  DSS(DS_build_seg)
#define YAWB DSS(DS_build_yaw)

/* ------------------------------------------------------------------------------ feature helpers */

/* YAW[pos .. pos+n-1] = yaw, stopping at the end of the array; pos not advanced */
static void prefill(s16 n)
{
    s16 pos = POS;
    if (!(pos < (s16)(pos + n))) return;
    for (s16 j = pos; j < (s16)(pos + n); j++) {
        if (j > 0x3DD) break;
        TRKS(YAW_, j) = YAWB;
    }
}

/* heading ramp of the curve features: step = n + DS:96AC (7 normal, 6 slow); every ramped segment gets C |= 8 */
static void ramp(s16 n, s16 delta)
{
    s16 step = (s16)(n + DSS(DS_build_yaw_step));
    s16 b = cdiv16(YAWB, step);
    s16 target = (s16)(b + delta);
    if (delta > 0 ? (b < target) : (target < b)) {
        s16 si = (s16)(step * b);
        do {
            if (POS > 0x3DD) break;
            TRKB(C_, POS) |= 8;
            TRKS(YAW_, POS) = si;
            if (delta > 0) { si = (s16)(si + step); b++; } else { si = (s16)(si - step); b--; }
            POS++;
        } while (delta > 0 ? (b < target) : (target < b));
    }
    YAWB = (s16)(step * b);
}

static void keep_side(s16 i)
{
    TRKW(L_, i) &= 0x9811;
    TRKW(R_, i) &= 0x9811;
}

/* YAW[s .. pos-1] = yaw (no bound) */
static void yaw_fill_from(s16 s)
{
    if (!(s < POS)) return;
    for (s16 j = s; j != POS; j++) TRKS(YAW_, j) = YAWB;
}

/* ----------------------------------------------------------------------------------- features */

/* 0dcb:1d04 feat_curve_left (codes 1, 4, 6) */
static void feat_curve_left(s16 n)
{
    prefill(0x32);
    TRKW(C_, POS) |= 0x0200;
    POS += 5;
    keep_side(POS);
    TRKW(C_, POS) |= 0x0200;
    POS += 6;
    ramp(n, +0x28);
}

/* 0dcb:1dfa feat_curve_right (codes 2, 3, 5) */
static void feat_curve_right(s16 n)
{
    prefill(0x32);
    keep_side(POS);
    TRKW(C_, POS) |= 0x0400;
    POS += 3;
    TRKW(C_, POS) |= 0x0400;
    POS += 3;
    ramp(n, -0x2D);
}

/* 0dcb:1eea feat_s_bend_left (code 10) */
static void feat_s_bend_left(s16 n)
{
    prefill(0x5A);
    POS++;
    keep_side(POS);
    TRKW(R_, POS) |= 0x2000;
    POS += 6;
    TRKW(R_, POS) |= 0x2000;
    ramp(n, +0x28);
    ramp(n, -0x2B);
}

/* 0dcb:2034 feat_s_bend_right (code 11) */
static void feat_s_bend_right(s16 n)
{
    prefill(0x5A);
    POS++;
    keep_side(POS);
    TRKW(R_, POS) |= 0x4000;
    POS += 6;
    TRKW(R_, POS) |= 0x4000;
    ramp(n, -0x23);
    ramp(n, +0x30);
}

/* 0dcb:217e feat_roadworks_right (code 7): only where the banks differ (Mulholland stretches) */
static void feat_roadworks_right(void)
{
    s16 s = POS;
    if (TRKS(TRK_BR, POS) == TRKS(TRK_BL, POS)) return;
    POS += 3;
    TRKW(R_, POS) |= 0x0800;
    POS += 4;
    TRKW(C_, POS) &= 0x0FF7;
    POS++;
    TRKW(L_, POS) &= 0xEFEE;
    TRKW(R_, POS) |= 0x1000;
    POS++;
    for (s16 k = 0; k < 5; k++) {
        if (POS > 0x3DD) break;
        TRKW(C_, POS) |= 0x40;
        TRKW(C_, POS) &= 0x0FF5;
        TRKW(R_, POS) &= 0xEFEE;
        TRKW(L_, POS) &= 0xEFEE;
        POS++;
    }
    TRKW(C_, POS) &= 0x0FF5;
    POS += 4;
    yaw_fill_from(s);
}

/* 0dcb:2282 feat_narrow_bridge (code 9): one lane for 20 segments, banks -500 (bridge over the drop) */
static void feat_narrow_bridge(void)
{
    s16 s = POS;
    TRKW(C_, POS) |= 0x0800;
    POS += 4;
    keep_side(POS);
    TRKW(C_, POS) |= 0x0800;
    for (s16 k = 0; k < 6; k++) {
        if (POS > 0x3DD) break;
        TRKW(L_, POS) &= 0x8000;
        TRKW(R_, POS) &= 0x8000;
        POS++;
    }
    for (s16 k = 0; k < 0x14; k++) {
        if (POS > 0x3DD) break;
        TRKW(C_, POS) |= 0x80;
        TRKW(C_, POS) &= 0x0FF3;
        TRKW(R_, POS) &= 0x6FEE;
        TRKW(L_, POS) &= 0x6FEE;
        TRKS(TRK_BR, POS) = -500;
        TRKS(TRK_BL, POS) = -500;
        POS++;
    }
    TRKW(C_, POS) &= 0x0FFB;
    TRKW(R_, POS) &= 0x6FEE;
    TRKW(L_, POS) &= 0x6FEE;
    for (s16 k = 0; k < 3; k++) {
        if (POS > 0x3DD) break;
        TRKW(L_, POS) &= 0x8000;
        TRKW(R_, POS) &= 0x8000;
        POS++;
    }
    yaw_fill_from(s);
}

/* 0dcb:23d4 feat_roadworks_left (code 8) */
static void feat_roadworks_left(void)
{
    s16 s = POS;
    if (TRKS(TRK_BR, POS) == TRKS(TRK_BL, POS)) return;
    POS += 4;
    TRKW(L_, POS) |= 0x0800;
    POS += 4;
    TRKW(C_, POS) &= 0x0FF7;
    POS++;
    TRKW(R_, POS) &= 0xEFEE;
    TRKW(L_, POS) |= 0x1000;
    POS++;
    for (s16 k = 0; k < 5; k++) {
        if (POS > 0x3DD) break;
        TRKW(C_, POS) |= 0x20;
        TRKW(C_, POS) &= 0x0FF5;
        TRKW(R_, POS) &= 0xEFEE;
        TRKW(L_, POS) &= 0xEFEE;
        POS++;
    }
    TRKW(C_, POS) &= 0x0FF5;
    POS += 3;
    yaw_fill_from(s);
}

/* ------------------------------------------------------------------------------------ banks */

/* the bank arrays BL / BR (inline in 24d8 and 2866): hills / drops beside the Mulholland stretches */
static void build_banks(void)
{
    for (s16 i = 200; i < 0x3DE; i = (s16)(i + 10)) {
        for (s16 j = 0; j < 10; j++) {
            s16 s = (s16)(i + j);
            if (i < 0x17C) { TRKS(TRK_BR, s) = 0; TRKS(TRK_BL, s) = 0; continue; }
            s16 bl = TRKS(TRK_BL, s - 1), br = TRKS(TRK_BR, s - 1);
            bool up = (i <= 0x18E) || (i > 599 && i < 0x26B) || (i > 699 && i < 0x2CF);
            bool down = !up && ((i >= 400 && i <= 0x1A2) || (i >= 500 && i <= 0x206));
            if (up)        { TRKS(TRK_BL, s) = (s16)(bl + 200); TRKS(TRK_BR, s) = (s16)(br - 200); }
            else if (down) { TRKS(TRK_BL, s) = (s16)(bl - 200); TRKS(TRK_BR, s) = (s16)(br + 200); }
            else           { TRKS(TRK_BL, s) = bl; TRKS(TRK_BR, s) = br; }
        }
    }
}

/* ------------------------------------------------------------------------------------ course */

/* 0dcb:24d8 track_build_course: arg 100 = a random layout into DS:7DFE, else rebuild DS:7DFE */
void track_build_course(s16 arg)
{
    aq_build();
    build_banks();
    POS = 0x186;
    YAWB = 0;
    DSS(DS_build_yaw_step) = DSW(DS_slow_machine) ? 5 : 6;
    for (s16 i = 0x17C; i < 0x3DE; i++) {
        TRKW(YAW_, i) = 0;
        TRKW(HGT_, i) = 0;
        TRKW(C_, i) &= 0xF117;
        TRKW(L_, i) &= 0xF7FF;
        TRKW(R_, i) &= 0x97FF;
        if ((TRKW(R_, i) & 0x1000) && !(TRKB(R_, i + 1) & 0x10)) TRKW(R_, i) &= 0xEFFF;
        if ((TRKW(L_, i) & 0x1000) && !(TRKB(L_, i + 1) & 0x10)) TRKW(L_, i) &= 0xEFFF;
        if (cmod16((s16)(i + 1), (s16)((DSS(DS_slow_machine) + 1) * 4)) == 0) TRKB(C_, i) |= 8;
    }
    if (arg == 100) DSS(DS_road_param) = rnd(10);
    for (s16 k = 0; k <= 0xC && POS <= 0x3DD; k++) {
        u16 f = (u16)(DSS((u16)(DS_course_layouts + 2 * (k + DSS(DS_road_param) * 0xD))) - 1);
        if (f >= 0xB) continue;
        switch (f + 1) {                                   /* jump table 0dcb:2716 */
        case 1: case 4: case 6: feat_curve_left(1); break;
        case 2: case 3: case 5: feat_curve_right(1); break;
        case 7:  feat_roadworks_right(); break;
        case 8:  feat_roadworks_left(); break;
        case 9:  feat_narrow_bridge(); break;
        case 10: feat_s_bend_left(1); break;
        case 11: feat_s_bend_right(1); break;
        }
    }
    for (s16 i = 0x17C; i < 0x3DE; i = (s16)(i + 10)) {
        for (s16 j = 0; j < 10; j++) {
            s16 s = (s16)(i + j);
            if (!(TRKB(C_, s) & 0x80) && (i < 0x398 || i > 0x3C0))
                TRKS(HGT_, s) = (s16)(TRKS(HGT_, s - 1) + DSS((u16)(DS_course_height_deltas + 2 * ((i - 0x17C) / 10))));
            else
                TRKS(HGT_, s) = TRKS(HGT_, s - 1);
        }
    }
    for (; POS < 0x3DE; POS++) TRKS(YAW_, POS) = YAWB;
    TRKB(C_, 0x3BF) |= 0x10;                                /* Mulholland finish line (finish seg 0x3C0) */
    for (s16 k = 0; k < 0x74; k++) {                        /* drag strip */
        TRKW(TRK_L2, k) = TRKW(L_, 0xC0 + k);
        TRKW(TRK_R2, k) = TRKW(R_, 0xC0 + k);
        TRKW(TRK_C2, k) = (u16)(TRKW(C_, 0xC0 + k) | 8);
    }
    TRKB(TRK_C2, 0x57) |= 0x10;                             /* drag-strip finish line (finish seg 0x58) */
}

/* ------------------------------------------------------------------------------------ whole track */

static bool divides(s16 v, s16 d) { return cmod16(v, d) == 0; }

/* 0dcb:2866 track_build_all: full = 1 (new game / start-up): town + new race-road seed; 0 (load): race road from
 * the saved seed DS:A9F8 */
void track_build_all(s16 full)
{
    s16 lvl = DSS(DS_slow_machine);
    if (DSS(DS_driver_id) == -2) {                          /* VGA: pages A000 / A200 / A400 */
        /* PORT: the original stores the relocated segment 3723 (page 0 at A000:0000 through a descriptor there) */
        ds_far_wr(DS_page0_ptr, far_make(SEG(0x3723), 0x0000));
        ds_far_wr(DS_page1_ptr, far_make(DGROUP, DS_page1_desc));
        ds_far_wr(DS_page2_ptr, far_make(DGROUP, DS_page2_desc));
    } else {                                                /* other drivers (parked) */
        ds_far_wr(DS_page0_ptr, ds_far(DS_g_front));
        ds_far_wr(DS_page1_ptr, ds_far(DS_g_back));
        ds_far_wr(DS_page2_ptr, far_make(SEG(0x371C), 0x0024));
    }
    build_banks();
    s16 i;
    if (full) {
        /* ---- town: segments 0 .. 0xB3 */
        for (i = 0; i < 0xB4; i++) {
            TRKW(HGT_, i) = 0; TRKW(YAW_, i) = 0; TRKW(C_, i) = 0; TRKW(R_, i) = 0; TRKW(L_, i) = 0;
            TRKW(C_, i) = 1;
            if (divides((s16)(i + 1), (s16)((lvl + 1) * 4))) TRKW(C_, i) = 9;
            if (divides((s16)(i + 2), 0x10)) {              /* intersection */
                TRKB(C_, i + 2) |= 4; TRKB(C_, i + 1) |= 4; TRKB(C_, i) |= 4;
                TRKB(R_, i - 1) |= 0x40;
                TRKB(L_, i + 3) |= 0x40;
                TRKB(C_, i + 1) |= 1; TRKB(C_, i + 2) |= 1; TRKB(C_, i + 3) |= 1;
                TRKW(HGT_, i + 1) = 0; TRKW(YAW_, i + 1) = 0;
                TRKW(HGT_, i + 2) = 0; TRKW(YAW_, i + 2) = 0;
                TRKW(HGT_, i + 3) = 0; TRKW(YAW_, i + 3) = 0;
                i += 3;
                continue;
            }
            if (divides((s16)(i + 4), 0x10) || divides((s16)(i + 8), 0x10) || divides((s16)(i + 12), 0x10)) {
                if (rnd(2) == 0) {
                    TRKB(L_, i - 1) |= 4; TRKB(L_, i) |= 4;
                    if (rnd(3) == 0) { TRKB(L_, i - 1) |= 8; TRKB(L_, i) |= 8; }
                    TRKB(L_, i) |= 0x10;
                } else {
                    TRKB(L_, i - 2) |= 4; TRKB(L_, i) |= 4; TRKB(L_, i - 1) |= 4;
                    if (rnd(3) == 0) { TRKB(L_, i - 2) |= 8; TRKB(L_, i) |= 8; TRKB(L_, i - 1) |= 8; }
                    TRKB(L_, i - 1) |= 0x10;
                }
            } else
                TRKB(L_, i) |= 0x80;
            if (divides((s16)(i + 5), 0x10) || divides((s16)(i + 9), 0x10) || divides((s16)(i + 12), 0x10)) {
                if (rnd(3) == 0) {
                    TRKB(R_, i - 2) |= 4; TRKB(R_, i) |= 4; TRKB(R_, i - 1) |= 4;
                    if (rnd(4) == 0) { TRKB(R_, i - 2) |= 8; TRKB(R_, i) |= 8; TRKB(R_, i - 1) |= 8; }
                    TRKB(R_, i - 1) |= 0x10;
                } else {
                    TRKB(R_, i - 1) |= 4; TRKB(R_, i) |= 4;
                    if (rnd(2) == 0) { TRKB(R_, i - 1) |= 8; TRKB(R_, i) |= 8; }
                    TRKB(R_, i) |= 0x10;
                }
            } else
                TRKB(R_, i) |= 0x80;
            if (rnd((s16)(lvl * 0x28 + 6)) == 0 && !(TRKB(L_, i) & 0x10)) TRKW(L_, i) |= 0x8000;
            if (rnd((s16)(lvl * 0x28 + 7)) == 0 && !(TRKB(R_, i) & 0x10)) TRKW(R_, i) |= 0x8000;
            if (rnd((s16)(lvl * 0x28 + 3)) == 0 && !(TRKB(L_, i) & 0x10)) TRKW(L_, i) |= 0x100;
            if (rnd((s16)(lvl * 0x28 + 4)) == 0 && !(TRKB(R_, i) & 0x10)) TRKW(R_, i) |= 0x100;
        }
        TRKW(C_, 10 + rnd(0x14)) |= 0x4000;                 /* town billboards */
        TRKW(C_, 35 + rnd(0x0F)) |= 0x8000;
        TRKW(C_, 65 + rnd(0x14)) |= 0x4000;
        TRKW(C_, 90 + rnd(0x1E)) |= 0x8000;
        TRKW(C_, 115 + rnd(0x14)) |= 0x4000;
        TRKW(C_, 140 + rnd(0x14)) |= 0x8000;
        TRKB(R_, 0x2C) |= 1;                                /* location signs */
        TRKB(L_, 0x6C) |= 1;
        TRKB(L_, 0x9C) |= 1;
        DSL(DS_road_seed) = DSL(DS_rnd_seed);
    }
    s16 cb = 0, ca = 0;
    DSL(DS_rnd_seed) = DSL(DS_road_seed);
    /* ---- race road: segments 0xB4 .. 0x3DD */
    for (i = 0xB4; i < 0x3DE; i++) {
        TRKW(HGT_, i) = 0; TRKW(YAW_, i) = 0; TRKW(C_, i) = 0; TRKW(L_, i) = 0; TRKW(R_, i) = 0;
        if (divides((s16)(i + 1), (s16)((DSS(DS_slow_machine) + 1) * 4))) TRKW(C_, i) = 8;
        if (divides(i, (s16)((lvl + 1) * 4))) { TRKB(L_, i) |= 1; TRKB(R_, i) |= 1; }
        if (divides((s16)(i + 2), (s16)((lvl + 1) * 8))) TRKB(C_, i) |= 2;
        if (rnd((s16)(lvl * 200 + 7)) == 0) TRKW(L_, i) |= 0x8000;
        if (TRKS(TRK_BR, i) < 0) { TRKW(R_, i) |= 0x8000; TRKB(R_, i) &= 0xFE; }
        if (i > 400 && rnd(0x40) == 0 && ca++ > 0x46) {
            TRKB(C_, i) |= 4;
            TRKS(TRK_LANE, i) = (s8)(rnd(0x10) + 1);
            ca = 0;
        }
        if (i > 400 && rnd(0x50) == 0 && !(TRKB(C_, i) & 4)) {
            TRKW(C_, i) |= 0x1000;
            TRKS(TRK_LANE, i) = (s8)(rnd(0x10) + 1);
        }
        if (lvl == 0) {
            if (rnd(0x23) == 0) { TRKB(L_, i) |= 0x40; TRKB(L_, i - 1) |= 0x40; TRKB(L_, i - 2) |= 0x40; }
            if (rnd(0x19) == 0) { TRKB(R_, i) |= 0x40; TRKB(R_, i - 2) |= 0x40; }
        } else {
            if (rnd((s16)(lvl * 100 + 0x23)) == 0) TRKB(L_, i) |= 0x40;
            if (rnd((s16)(lvl * 100 + 0x19)) == 0) TRKB(R_, i) |= 0x40;
        }
        if (rnd((s16)(lvl * 0x4B + 4)) == 0 && !(TRKB(L_, i) & 0x40)) {
            TRKB(L_, i) |= 0x80;
            if (rnd(2) == 0) TRKB(L_, i) |= 2;
        }
        if (rnd((s16)(lvl * 0x4B + 6)) == 0 && !(TRKB(R_, i) & 0x40)) {
            TRKB(R_, i) |= 0x80;
            if (rnd(2) == 0) TRKB(R_, i) |= 2;
        }
        if (rnd((s16)(lvl * 100 + 0x15)) == 0) TRKB(L_, i) |= 4;
        if (rnd((s16)(lvl * 100 + 0x19)) == 0) TRKB(R_, i) |= 4;
        if (rnd((s16)(lvl * 100 + 0x17)) == 0 && !(TRKB(L_, i) & 4)) TRKB(L_, i) |= 8;
        if (rnd((s16)(lvl * 100 + 0x1E)) == 0 && !(TRKB(R_, i) & 4)) TRKB(R_, i) |= 8;
        if (rnd((s16)(lvl * 0x32 + 2)) == 0) {
            TRKW(L_, i) |= 0x100;
            if (rnd(2) == 0) TRKW(L_, i) |= 0x200;
        }
        if (rnd((s16)(lvl * 0x32 + 2)) == 0) {
            TRKW(R_, i) |= 0x100;
            if (rnd(2) == 0) TRKW(R_, i) |= 0x200;
        }
        if (i > 0xF0 && cb > 0x46) {                        /* bank markers where a bank rises */
            if (rnd((s16)(lvl * 100 + 0x50)) == 0 && TRKS(TRK_BL, i + 1) > 0) {
                TRKB(L_, i) |= 0x10; TRKW(L_, i - 1) |= 0x1000; cb = 0;
            }
            if (rnd((s16)(lvl * 100 + 0x50)) == 0 && TRKS(TRK_BR, i + 1) > 0) {
                TRKB(R_, i) |= 0x10; TRKW(R_, i - 1) |= 0x1000; cb = 0;
            }
        }
        ca++;
        cb++;
    }
    TRKW(L_, 0x17B) &= 0x1811; TRKW(R_, 0x17B) &= 0x1811; TRKB(C_, 0x17B) |= 0x10;   /* drag-race finish line */
    s16 r = rnd(0x8C), s = (s16)(r + 0xDC);
    if (TRKB(R_, s) & 2) s = (s16)(r + 0xDB);
    TRKW(L_, s) &= 0x9811; TRKW(R_, s) &= 0x9811; TRKW(C_, s) |= 0x8000;               /* the overhead sign */
    for (i = 0xDC; (s16)(i + 0x5A) < 0x17C; ) {             /* one hill pair */
        for (s16 k = 0; k < 0x11; k++) TRKS(HGT_, i++) = DSS((u16)(DS_hill_profile_17 + 2 * k));
        i = (s16)(i + 0x14);
        for (s16 k = 0; k < 0x11; k++) TRKS(HGT_, i++) = (s16)-DSS((u16)(DS_hill_profile_17 + 2 * k));
        i = (s16)(i + 0x1E);
    }
    TRKW(C_, 0x181) |= 0x100;                               /* speed-limit sign */
    if (!full) rnd(-1);                                     /* reseed from the clock */
}

/* 0dcb:33ac aq_build: Aqueduct segment flags AQ (deterministic) and the reversed copy AQM */
void aq_build(void)
{
    s16 i;
    for (i = 6; i < 0x172; i = (s16)(i + 0x18)) {
        TRKW(TRK_AQ, i) = 2; TRKW(TRK_AQ, i - 1) = 2; TRKW(TRK_AQ, i - 2) = 2; TRKW(TRK_AQ, i - 3) = 2;
    }
    for (i = 1; i < 0x172; i = (s16)(i + 6)) TRKW(TRK_AQ, i) |= 1;
    static const s16 b10[] = { 4, 76, 220, 316, 364 };
    for (unsigned k = 0; k < 5; k++) TRKW(TRK_AQ, b10[k]) |= 0x10;
    TRKW(TRK_AQ, 124) |= 0x80; TRKW(TRK_AQ, 268) |= 0x80;
    for (i = 0x12; i < 0x172; i = (s16)(i + 0x18)) TRKW(TRK_AQ, i - 1) |= 4;
    TRKW(TRK_AQ, 137) &= ~4u; TRKW(TRK_AQ, 281) &= ~4u; TRKW(TRK_AQ, 137) |= 0x100; TRKW(TRK_AQ, 281) |= 0x100;
    for (i = 0x12; i < 0x172; i = (s16)(i + 0x18)) TRKW(TRK_AQ, i - 2) |= 8;
    TRKW(TRK_AQ, 136) &= ~8u; TRKW(TRK_AQ, 280) &= ~8u; TRKW(TRK_AQ, 136) |= 0x200; TRKW(TRK_AQ, 280) |= 0x200;
    static const s16 b20[] = { 5, 34, 59, 100, 179, 214, 322 };
    static const s16 b40[] = { 23, 82, 130, 155, 238, 262, 358 };
    for (unsigned k = 0; k < 7; k++) TRKW(TRK_AQ, b20[k]) |= 0x20;
    for (unsigned k = 0; k < 7; k++) TRKW(TRK_AQ, b40[k]) |= 0x40;
    TRKW(TRK_AQ, 341) |= 0x400;
    for (i = 0; i < 0x172; i++) TRKW(TRK_AQM, i) = TRKW(TRK_AQ, 0x171 - i);
}

/* 0dcb:1470 track_lane_x */
s16 track_lane_x(s16 seg)
{
    return (s16)(TRKS(TRK_LANE, seg) * 0x70 + 0x5DC);
}
