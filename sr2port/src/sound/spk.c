/* PC-speaker driver 2c57:0002-09ca (sound.md 3, 6): the table-driven song sequencer (song_select, the sound
 * part of timer_isr), the loop-sound / tune slot routines, the engine note of the drive ISR and the busy-loop
 * effects. SR1 code (srport sound.c / sfx.c) with the SR2 deltas: near song pointers, total-play loop counts,
 * the hold flag, `60000 - 1.375u` engine note, gate through slots 23 / 24.
 *
 * The effects block the caller, as in the original. Their time runs on the host's emulated timeline
 * (host_busy_wait_ns): timer ticks that fall inside an effect run at their place, so the music sequencer or
 * the drive ISR change the speaker in the middle of an effect as they did on the PC. */
#include "sound/snd_int.h"

#include "platform/platform.h"

/* effect timing (SR1 sound.md 7, DOSBox reference; delay() is the same code in SR2) */
#define SND_DELAY_SCALE        3432u    /* value the port stores in DS_delay_scale */
#define SND_DELAY_UNIT_NS      4470u    /* one delay(1) unit at SND_DELAY_SCALE: 4.47 us */
#define SND_TONE_OVERHEAD_NS  20000u    /* one fx_tone call besides its delay */
#define SND_NOISE_OVERHEAD_NS 25000u    /* one fx_noise step besides its delay */

static inline void pit2_div(u16 d) { host_pit2_divisor(d); }

/* ---------------------------------------------------------------- gate (slots 23 / 24) */

void spk_vec_off(int unused) { (void)unused; host_port61((u8)(host_port61_get() & 0xFC)); DSB(DS_sound_gate) = 0; }   /* 2c57:05ac */
void spk_vec_on(int unused)  { (void)unused; host_port61((u8)(host_port61_get() | 3)); DSB(DS_sound_gate) = 1; }     /* 2c57:05ba (no 25 kHz) */

/* ---------------------------------------------------------------- delay and tone primitives */

/* 2c57:01ae. PORT: no measurement; returns SND_DELAY_SCALE, the constant the port's delay() timing is calibrated for */
static int snd_calibrate_delay(int n)
{
    (void)n;
    DSW(DS_delay_scale) = 0x100;
    return (int)SND_DELAY_SCALE;
}

/* 2c57:1406 delay: mul by the scale; inner LOOP count = bits 8-23 (0 = 65536), repeated bits 24-31 + 1 times */
static void delay(u16 n)
{
    u32 p = (u32)n * DSW(DS_delay_scale);
    u32 inner = (u16)(p >> 8);
    u32 outer = (u8)(p >> 24);
    uint64_t loops = (uint64_t)(outer + 1) * (inner ? inner : 0x10000u);
    host_busy_wait_ns(loops * SND_DELAY_UNIT_NS * 256 / SND_DELAY_SCALE);
}

void spk_set_hz(s32 hz)                                                  /* 2c57:0234 */
{
    if (hz == 0) div_error();
    pit2_div((u16)(0x1234DC / hz));
}

static void fx_tone(s32 hz, u16 d)                                       /* 2c57:025c */
{
    spk_set_hz(hz);
    host_busy_wait_ns(SND_TONE_OVERHEAD_NS);
    delay(d);
}

static void fx_burst(s16 lo, s16 hi, u16 d, s16 n)                       /* 2c57:0278 */
{
    s16 range = (s16)(hi - lo);
    if (n <= 0) return;
    do fx_tone((s16)(rnd(range) + lo), d);
    while (--n);
}

static void fx_sweep_down(s16 from, s16 to, s16 step, u16 d)             /* 2c57:02b8 */
{
    for (s16 f = from; f >= to; f = (s16)(f - step)) fx_tone(f, d);
}

static void fx_warble(s16 c, s16 dev, s16 step, u16 d, s16 n)            /* 2c57:02e6 */
{
    if (n <= 0) return;
    s16 span = (s16)(dev << 1);
    do {
        if (span > 0) {
            s16 f = (s16)(c - dev), i = 0;
            do { fx_tone(f, d); f = (s16)(f + step); i = (s16)(i + step); } while (span > i);
        }
        if (span > 0) {
            s16 f = (s16)(c + dev), i = 0;
            do { fx_tone(f, d); f = (s16)(f - step); i = (s16)(i + step); } while (span > i);
        }
    } while (--n);
}

static void fx_noise(s16 range, s16 ormask, s16 n)                       /* 2c57:0506 */
{
    spk_vec_off(0);
    if (n <= 0) return;
    do {
        host_port61((u8)(host_port61_get() ^ 2));
        host_busy_wait_ns(SND_NOISE_OVERHEAD_NS);
        delay((u16)(rnd(range) | ormask));
    } while (--n);
}

static void fx_noise_down(s16 a, s16 b, s16 sub, s16 step, s16 n)        /* 2c57:0544 */
{
    if (b >= a) return;
    s16 x = a;
    do {
        fx_noise(a, (s16)(a - sub), n);
        a = (s16)(a - step);
        x = (s16)(x - step);
    } while (b < x);
}

static void fx_noise_up(s16 a, s16 b, s16 sub, s16 step, s16 n)          /* 2c57:0578 */
{
    if (a >= b) return;
    s16 x = a;
    do {
        fx_noise(a, (s16)(a - sub), n);
        a = (s16)(a + step);
        x = (s16)(x + step);
    } while (x < b);
}

/* ---------------------------------------------------------------- one-shot effects (slots 8 9 12-17) */

#define FX_GUARD() do { if (DSB(DS_sound_on) == 0) return; } while (0)

static void spk_fx_click(int u)    { (void)u; FX_GUARD(); spk_vec_on(0); fx_warble(3000, 10, 10, 60, 9); spk_vec_off(0); }   /* 07a6 */
static void spk_fx_chirp_hi(int u) { (void)u; FX_GUARD(); spk_vec_on(0); fx_warble((s16)(rnd(100) + 0x1838), 40, 10, 1, 10); spk_vec_off(0); }   /* 07d2 */
static void spk_fx_chirp_lo(int u) { (void)u; FX_GUARD(); spk_vec_on(0); fx_warble(0x1450, 40, 10, 1, 10); spk_vec_off(0); }   /* 080a */
static void spk_fx_clank(int u)    { (void)u; FX_GUARD(); spk_vec_on(0); fx_noise(200, 10, 60); spk_vec_off(0); }   /* 0836, 085c */

static void spk_fx_crash(int u)                                          /* 0882 */
{
    (void)u;
    FX_GUARD();
    for (int i = 3; i; i--) {
        spk_vec_on(0);
        fx_burst(3000, 8000, 60, 10);
        fx_noise_down(200, (s16)(rnd(30) + 18), 15, 5, 2);
        fx_noise((s16)(rnd(300) + 40), 2, 30);
        fx_noise_up(16, (s16)(rnd(100) + 50), 15, 5, 4);
    }
    spk_vec_off(0);
}

static void spk_fx_hit(int u)                                            /* 0930 */
{
    (void)u;
    FX_GUARD();
    spk_vec_on(0);
    fx_burst(2000, 8000, 100, 10);
    fx_noise(120, 2, 30);
    spk_vec_on(0);
    fx_burst(4000, 9000, 100, 10);
    fx_noise(220, 2, 20);
    spk_vec_off(0);
}

static void spk_fx_thud(int u) { (void)u; FX_GUARD(); spk_vec_on(0); fx_sweep_down(130, 90, 4, 1000); spk_vec_off(0); }   /* 09a0 */

static void spk_siren_set(int on) { DSW(DS_siren_on) = on != 0; }        /* 0796 */
static void spk_slot_nop(int u) { (void)u; }                             /* 0634 */

/* ---------------------------------------------------------------- the sequencer */

/* 0001:2002 tune_start. `void far tune_start(int word_off, int delay)`; always called through song_select as (2, 0) */
void tune_start(int word_off, int delay)
{
    if (DSW(DS_music_on) != 0) {
        u16 s = DSW(DS_music_cur);
        DSW(s + SS_PLAYING) = 0;
        DSW(s + SS_REP) = 1;
        DSB(s + SS_REGATE) = 1;
        u16 p = (u16)(DSW(s + SS_BASE) + 2 * word_off);
        DSW(s + SS_CUR) = p;
        DSW(s + SS_SEC_START) = p;
        DSW(s + SS_DUR) = (u16)(delay + 1);
        DSW(s + SS_VIB_PTR) = DSW(s + SS_VIB_TBL);
        DSW(DS_vib_rec_ptr) = (u16)(DS_vib_rec + 10 * DSW(DSW(s + SS_VIB_PTR)));
        DSW(DS_vib_enabled) = 1;
        DSW(s + SS_PLAYING) = 1;
    } else {
        DSW(DSW(DS_music_cur) + SS_PLAYING) = 0;
    }
}

/* 0001:208a tune_rewind: stop + gate off (it does not rewind) */
void tune_rewind(void)
{
    u16 s = DSW(DS_music_cur);
    DSW(s + SS_PLAYING) = 0;
    sound_vec(23);
    DSB(s + SS_REGATE) = 1;
}

/* 2c57:0002 song_select (rewritten): near DS pointers, loop / vibrato tables as arguments */
static void song_select(u16 song, u16 loop_tbl, u16 vib_tbl)
{
    if (song == DS_BG_SONG_BASE) {                           /* back to the background tune */
        if ((s16)DSW(DS_music_enabled_saved) != -1) {
            DSW(DS_music_on) = DSW(DS_music_enabled_saved);
            DSW(DS_music_enabled_saved) = 0xFFFF;
        }
        if (DSW(DS_music_on) == 0) sound_vec(23);
        else DSW(DS_bg_song + SS_PLAYING) = 1;
        DSW(DS_music_cur) = DS_bg_song;
        DSW(DS_vib_rec_ptr) = (u16)(DS_vib_rec + 10 * DSW(DSW(DS_bg_song + SS_VIB_PTR)));
        DSW(DS_music_mode) = 1;
    } else {                                                 /* loop sound / jingle */
        if ((s16)DSW(DS_music_enabled_saved) == -1) {        /* plays even when music is off */
            DSW(DS_music_enabled_saved) = DSW(DS_music_on);
            DSW(DS_music_on) = 1;
        }
        DSW(DS_music_mode) = 2;
        DSW(DS_fx_song + SS_BASE) = song;
        DSW(DS_fx_song + SS_SEC_START) = song;
        DSW(DS_fx_song + SS_CUR) = song;
        DSW(DS_fx_song + SS_LOOP_TBL) = loop_tbl;
        DSW(DS_fx_song + SS_LOOP_PTR) = loop_tbl;
        DSW(DS_MUSIC_HOLD) = DSW(loop_tbl) == 0;
        DSW(DS_fx_song + SS_VIB_TBL) = vib_tbl;
        DSW(DS_fx_song + SS_VIB_PTR) = vib_tbl;
        DSW(DS_music_cur) = DS_fx_song;
        tune_start(2, 0);                                    /* first event on the next tick */
    }
    DSW(DS_vib_enabled) = 1;
}

/* 2c57:00b8 vib_set / 0140.. are dead (no callers) and not ported */

/* loop-sound slots: song, vibrato table */
static void loop_slot(int on, u16 song, u16 vib)
{
    if (on) song_select(song, 0x542E, vib);
    else    song_select(DS_BG_SONG_BASE, DS_BG_LOOP_TBL, DS_BG_VIB_TBL);
}
static void spk_loop_putt(int on)   { loop_slot(on, 0x5544, 0x5474); }  /* 0608, slot 0 */
static void spk_loop_hum100(int on) { loop_slot(on, 0x5524, 0x547C); }  /* 0636, slot 3 */
static void spk_loop_hum150(int on) { loop_slot(on, 0x5534, 0x548C); }  /* 0662, slot 2 */
static void spk_loop_tick(int on)   { loop_slot(on, 0x5490, 0x5480); }  /* 068e, slot 4 */
static void spk_loop_blips(int on)  { loop_slot(on, 0x54BC, 0x5484); }  /* 06ba, slot 7 */
static void spk_loop_pulse(int on)  { loop_slot(on, 0x5500, 0x548C); }  /* 06e6, slot 6 */
static void spk_loop_spray(int on)  { loop_slot(on, 0x5570, 0x548C); }  /* 0712, slot 5 */
static void spk_loop_wrench(int on) { loop_slot(on, 0x54DC, 0x5488); }  /* 073e, slot 10 */
static void spk_bg_tune(int u)      { (void)u; song_select(DS_BG_SONG_BASE, DS_BG_LOOP_TBL, DS_BG_VIB_TBL); }   /* 076a, slot 18 */
static void spk_jingle_tune(int u)  { (void)u; song_select(0x6E48, 0x542A, 0x5468); }   /* 0780, slot 19 */
static void sound_stop(int u)       { (void)u; DSW(DS_MUSIC_HOLD) = 0; tune_rewind(); } /* 01a2, slots 20 / 21 */

/* 2c57:00f6 music_mute (speaker slot 22; SR1 0f38:7001) */
void music_mute(s16 on)
{
    if (on) {
        if (DSW(DS_music_on) == 0) { DSW(DS_music_muted) = 0; return; }
        music_stop();
        DSW(DS_music_on) = 0;
        DSW(DS_music_muted) = 1;
    } else {
        if (DSW(DS_music_muted) == 0) return;
        DSW(DS_music_on) = 1;
        music_resume();
        DSW(DS_music_muted) = 0;
    }
}
static void spk_music_mute(int on) { music_mute((s16)on); }

/* 0001:2246 sound part, sound.md 6.2: one sequencer tick */
void speaker_music_tick(void)
{
    u16 s = DSW(DS_music_cur);
    if (DSW(DS_MUSIC_HOLD) == 0) DSW(s + SS_DUR) = (u16)(DSW(s + SS_DUR) - 1);
    if (DSW(s + SS_DUR) != 0) {                              /* inside a note: vibrato */
        if (DSW(DS_vib_enabled) && DSW(s + SS_NOTE) != 0) {
            DSW(DS_vib_countdown) = (u16)(DSW(DS_vib_countdown) - 1);
            if (DSW(DS_vib_countdown) == 0) {
                u16 rec = DSW(DS_vib_rec_ptr);
                s16 period = DSS(rec + 6);
                if (period == 0) return;                     /* PORT: a divide error in the original; no record has period 0 */
                s16 r = (s16)(DSS(rec) % period);
                DSW(rec) = (u16)r;
                s16 w = DSS(DS_vib_wave + 2 * (DSS(rec + 2) * 8 + r));
                u16 div = (u16)((u16)(w * (s16)DSW(DS_vib_amount)) + DSW(s + SS_NOTE));
                DSW(DS_VIB_LAST_DIV) = div;
                DSW(rec) = (u16)(DSW(rec) + 1);
                pit2_div(div);
                DSW(DS_vib_countdown) = DSW(rec + 8);
            }
        }
        return;
    }
    for (;;) {                                               /* fetch events until a note / rest is set */
        DSW(s + SS_DUR) = DSW(DSW(s + SS_CUR));
        DSW(s + SS_CUR) = (u16)(DSW(s + SS_CUR) + 2);
        if (DSW(s + SS_DUR) == 0) {                          /* marker */
            u16 m = DSW(DSW(s + SS_CUR));
            DSW(s + SS_CUR) = (u16)(DSW(s + SS_CUR) + 2);
            if (m == 0) {                                    /* end of song */
                sound_vec(23);
                DSW(s + SS_PLAYING) = 0;
                DSB(s + SS_REGATE) = 1;
                DSW(s + SS_CUR) = DSW(s + SS_BASE);
                DSW(s + SS_SEC_START) = DSW(s + SS_BASE);
                return;
            }
            DSW(s + SS_VIB_PTR) = (u16)(DSW(s + SS_VIB_PTR) + 2);
            DSW(s + SS_REP) = (u16)(DSW(s + SS_REP) + 1);
            if (DSS(DSW(s + SS_LOOP_PTR)) >= DSS(s + SS_REP)) {
                DSW(s + SS_CUR) = DSW(s + SS_SEC_START);     /* play the section again */
            } else {
                DSW(s + SS_LOOP_PTR) = (u16)(DSW(s + SS_LOOP_PTR) + 2);
                if (DSS(DSW(s + SS_LOOP_PTR)) < 0) {
                    DSW(s + SS_CUR) = (u16)(DSW(s + SS_BASE) + 4);
                    DSW(s + SS_LOOP_PTR) = DSW(s + SS_LOOP_TBL);
                    DSW(s + SS_VIB_PTR) = DSW(s + SS_VIB_TBL);
                }
                if (DSS(DSW(s + SS_LOOP_PTR)) == 0) DSW(DS_MUSIC_HOLD) = 1;   /* (dead with the shipped tables) */
                DSW(s + SS_SEC_START) = DSW(s + SS_CUR);
                DSW(s + SS_REP) = 1;
            }
            DSW(DS_vib_rec_ptr) = (u16)(DS_vib_rec + 10 * DSW(DSW(s + SS_VIB_PTR)));
            continue;
        }
        u16 note = DSW(DSW(s + SS_CUR));
        DSW(s + SS_CUR) = (u16)(DSW(s + SS_CUR) + 2);
        DSW(s + SS_NOTE) = note;
        if (note == 0) {                                     /* rest: gate off, divisor unchanged */
            DSB(s + SS_REGATE) = 1;
            sound_vec(23);
            return;
        }
        if (DSW(DS_vib_enabled)) {
            u16 rec = DSW(DS_vib_rec_ptr);
            if (note == 6000) {                              /* engine override (never in the data) */
                DSW(s + SS_NOTE) = note = DSW(0xA9C4);
                DSW(DS_vib_amount) = DSW(DS_engine_vib_amount);
            } else {
                DSW(DS_vib_countdown) = DSW(rec + 8);
                DSW(DS_vib_amount) = (u16)((u16)(note / 500) * DSW(rec + 4));
            }
        }
        pit2_div(note);
        if (DSB(DS_sound_on) && (DSB(DS_sound_gate) == 0 || DSB(s + SS_REGATE))) {
            sound_vec(24);
            if (DSB(s + SS_REGATE)) DSB(s + SS_REGATE) = 0;
        }
        return;
    }
}

/* platform_install 5.1: initial state of the background tune */
void spk_song_defaults(void)
{
    DSW(DS_bg_song + SS_PLAYING) = 0;
    DSW(DS_bg_song + SS_DUR) = 1;
    DSW(DS_bg_song + SS_REP) = 1;
    DSB(DS_bg_song + SS_REGATE) = 1;
    DSW(DS_bg_song + SS_BASE) = DS_BG_SONG_BASE;
    DSW(DS_bg_song + SS_SEC_START) = DS_BG_SONG_BASE + 4;
    DSW(DS_bg_song + SS_CUR) = DS_BG_SONG_BASE + 4;
    DSW(DS_bg_song + SS_LOOP_TBL) = DS_BG_LOOP_TBL;
    DSW(DS_bg_song + SS_LOOP_PTR) = DS_BG_LOOP_TBL;
    DSW(DS_bg_song + SS_VIB_TBL) = DS_BG_VIB_TBL;
    DSW(DS_bg_song + SS_VIB_PTR) = DS_BG_VIB_TBL;
    DSW(DS_vib_rec_ptr) = (u16)(DS_vib_rec + 10 * DSW(DS_BG_VIB_TBL));
    DSW(DS_fx_song + SS_PLAYING) = 0;
}

/* ---------------------------------------------------------------- drive ISR (sound.md 5.3, speaker) */

void speaker_race_tick(u8 phase)
{
    if (phase & 1) {                                         /* engine phase */
        if (DSW(DS_ANIM_FLAG) == 0) {
            s16 rpm = DSS(DS_RPM);
            u16 bonus = DSW(DS_RPM) == 0x1644 ? 6000 : 0;     /* the limiter */
            u16 u = (u16)(((s32)rpm * rpm) >> 10);
            u16 div = (u16)(60000 - (u16)(u + (u >> 3) + (u >> 2)) - bonus);
            u16 old = DSW(DS_SPK_PULSE);
            if (DSS(DSW(DS_CUR_CAR_PTR) + 0x1E) == 0 && (DSW(DS_SPK_PULSE) = (u16)(old + 1), (s16)old < 1)) {
                host_pit2_divisor((u16)((div & 0xFF) | 0xF000));   /* pulse: a very low tone */
            } else {
                host_pit2_divisor(div);
                DSW(DS_SPK_PULSE) = 0;
            }
        } else {                                             /* animation (garage) player */
            s16 dd = (s16)(DSW(DS_ANIM_X) - DSW(DS_ANIM_REF));
            u16 d = (u16)(dd < 0 ? -dd : dd);
            u16 t = (u16)((u32)(u16)(d >> 1) * d);
            if (t > 20000) t = 20000;
            u16 div = (u16)(60000 - t);
            u16 old = DSW(DS_SPK_PULSE);
            if (DSS(DS_ANIM_MUFFLER) == 0 && (DSW(DS_SPK_PULSE) = (u16)(old + 1), (s16)old < 2)) {
                host_pit2_divisor((u16)((div & 0xFF) | 0xF000));
            } else {
                host_pit2_divisor(div);
                DSW(DS_SPK_PULSE) = 0;
            }
        }
    } else {                                                 /* siren / squeal phase */
        DSB(DS_SQUEAL_PH) ^= 1;
        if (DSW(DS_siren_on)) {
            u16 n = DSW(DS_SIREN_CNT);
            DSW(DS_SIREN_CNT) = (u16)(n + 1);
            pit2_div((s16)n < 10 ? 0x054B : 0x0712);
            if (DSS(DS_SIREN_CNT) > 20) DSW(DS_SIREN_CNT) = 0;
        } else if ((DSW(DS_squeal) || DSW(DS_on_shoulder)) && DSB(DS_SQUEAL_PH)) {
            pit2_div(0x0095);                                /* 8 kHz blip for one tick */
        }
    }
}

/* tune-up screen: speaker note, divisor high byte alternates (sound.md 5.4) */
void spk_engine_tone(u16 car_ign)
{
    static u8 tog;                                           /* (the original's toggle lives in its frame / BSS) */
    s8 p = (s8)car_ign;
    u8 lo = (u8)((-16 - p) * 4);
    tog ^= 1;
    if (tog && p != 0) lo = 0xF0;
    pit2_div((u16)(lo << 8));
}

/* ---------------------------------------------------------------- init */

void speaker_sound_init(void)                                        /* 2c57:05c8 */
{
    for (int i = 0; i < 100; i++) DSB(DS_snd_vec + i) = DSB(DS_SPK_SLOTS + i);
    DSW(DS_snd_device) = 1;
    DSW(DS_delay_scale) = (u16)snd_calibrate_delay(10);
    /* out 43h, B6h: channel 2 lo / hi, mode 3 (implied by the host model) */
    spk_set_hz(25000);
    DSW(DS_snd_state) = 1;
}

void spk_registry(void)
{
    static const struct { u32 fn; void (*f)(int); } t[] = {
        { FN_spk_loop_putt, spk_loop_putt }, { FN_spk_slot_nop, spk_slot_nop }, { FN_spk_loop_hum150, spk_loop_hum150 },
        { FN_spk_loop_hum100, spk_loop_hum100 }, { FN_spk_loop_tick, spk_loop_tick }, { FN_spk_loop_spray, spk_loop_spray },
        { FN_spk_loop_pulse, spk_loop_pulse }, { FN_spk_loop_blips, spk_loop_blips }, { FN_fx_clank, spk_fx_clank },
        { FN_fx_clank2, spk_fx_clank }, { FN_spk_loop_wrench, spk_loop_wrench }, { FN_siren_set, spk_siren_set },
        { FN_fx_click, spk_fx_click }, { FN_fx_chirp_hi, spk_fx_chirp_hi }, { FN_fx_chirp_lo, spk_fx_chirp_lo },
        { FN_fx_crash, spk_fx_crash }, { FN_fx_hit, spk_fx_hit }, { FN_fx_thud, spk_fx_thud },
        { FN_spk_bg_tune, spk_bg_tune }, { FN_spk_jingle_tune, spk_jingle_tune }, { FN_sound_stop, sound_stop },
        { FN_music_mute, spk_music_mute }, { FN_spk_vec_off, spk_vec_off }, { FN_spk_vec_on, spk_vec_on },
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) codeptr_register(t[i].fn, (CodeFn)t[i].f);
}
