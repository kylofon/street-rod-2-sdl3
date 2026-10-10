/* AdLib sound effects and engine drone: the slot routines of DS:73C2 (2c57:0b2c-13d8, 21b1-21f0), sound.md 3
 * and 5.3. Effects borrow the music voices 2..5 of the same chip and load their timbre from MSR.SND. */
#include "sound/snd_int.h"

#include <stdlib.h>

/* MSC rand() (34b0:23a8): used by two chirps, not the game's rnd */
static int msc_rand(void)
{
    static u32 seed = 1;
    seed = seed * 214013u + 2531011u;
    return (int)((seed >> 16) & 0x7FFF);
}

#define GUARD() do { if (DSB(DS_sound_gate) == 0) return; } while (0)

/* "key" routines: the timbre is already loaded on voice V */
static void key_fx(int on, int v, int vol, int note)
{
    GUARD();
    if (on) {
        adlib_set_volume(v, (u16)vol, 1);
        adlib_voice_off(v);
        adlib_note_on(v, note);
    } else {
        adlib_voice_off(v);
    }
}

static void adlib_fx_putt(int on)     { key_fx(on, 5, 0x38, 0x0E); }   /* 0c88, slot 0 */
static void adlib_fx_hum(int on)      { key_fx(on, 4, 0x68, 0x4F); }   /* 0d2c, slot 3 */
static void adlib_fx_tick(int on)     { key_fx(on, 4, 0x6A, 0x54); }   /* 0d7c, slot 4 */
static void adlib_fx_spray(int on)    { key_fx(on, 5, 0x6A, 0x5A); }   /* 0dcc, slot 2 */
static void adlib_fx_bolt(int on)     { key_fx(on, 4, 0x40, 0x55); }   /* 0edc, slot 5 */
static void adlib_fx_airwrench(int on){ key_fx(on, 5, 0x4A, 0x1C); }   /* 0f2c, slot 6 */
static void adlib_fx_slide(int on)    { key_fx(on, 4, 0x5A, 0x52); }   /* 0f7c, slot 10 */

void adlib_fx_tyre(int on)                                               /* 0cd8, slot 1 */
{
    DSW(DS_TYRE_NOISE) = (u16)on;
    key_fx(on, 5, 0x60, 0x78);
}

static void adlib_fx_lugnut(int on)                                      /* 0e1c, slot 7 */
{
    GUARD();
    if (on) {
        adlib_play_timbre(3, 0x7F, "turn1");
        adlib_voice_off(3);
        adlib_note_on(3, 0x0E);
    } else adlib_voice_off(3);
}

static void adlib_fx_clank(int u)                                        /* 0e6c, slot 8 */
{
    (void)u;
    GUARD();
    adlib_play_timbre(4, 0x7F, "hitcar");
    adlib_voice_off(4);
    adlib_note_on(4, 0x28);
}

static void adlib_fx_clank2(int u)                                       /* 0ea4, slot 9 */
{
    (void)u;
    GUARD();
    adlib_play_timbre(3, 0x70, "hitcar");
    adlib_voice_off(3);
    adlib_note_on(3, 0x2E);
}

static void adlib_fx_siren(int on)                                       /* 0fcc, slot 11 */
{
    GUARD();
    if (on) {
        adlib_play_timbre(2, 0x7F, "police");
        adlib_set_bend_range(5);
        adlib_pitch_bend(2, 0x2000);
        adlib_voice_off(2);
        adlib_note_on(2, 0x50);
        DSW(DS_siren_on) = 1;
    } else {
        adlib_voice_off(2);
        adlib_pitch_bend(2, 0x2000);
        DSW(DS_siren_on) = 0;
    }
}

void adlib_siren_bend(int x)                                             /* 1050 */
{
    if (DSB(DS_sound_gate)) adlib_pitch_bend(2, (u16)(0x1FFF * x + 0x2000));
}

static void adlib_fx_click(int u)                                        /* 10fa, slot 12 */
{
    (void)u;
    GUARD();
    adlib_play_timbre(3, 0x7F, "piano1");
    adlib_voice_off(3);
    adlib_note_on(3, 0x5C);
    adlib_voice_off(3);
}

static void adlib_fx_chirp_hi(int u)                                     /* 113e, slot 13 */
{
    (void)u;
    GUARD();
    adlib_play_timbre(3, 0x75, "turn");
    adlib_voice_off(3);
    adlib_note_on(3, msc_rand() % 8 + 0x61);
    adlib_voice_off(3);
}

static void adlib_fx_chirp_lo(int u)                                     /* 118e, slot 14 */
{
    (void)u;
    GUARD();
    adlib_play_timbre(3, 0x75, "turn");
    adlib_voice_off(3);
    adlib_note_on(3, 0x3D);
}

static void adlib_fx_crash(int u)                                        /* 11c6, slot 15 */
{
    (void)u;
    GUARD();
    adlib_play_timbre(4, 0x7F, "hitcar");
    adlib_voice_off(4);
    adlib_note_on(4, 0x14);
    adlib_play_timbre(5, 0x7F, "crash");
    adlib_voice_off(5);
    adlib_note_on(5, 0x19);
    adlib_voice_off(4);
    adlib_voice_off(5);
}

static void adlib_fx_hit(int u)                                          /* 1246, slot 16 */
{
    (void)u;
    GUARD();
    adlib_play_timbre(4, 0x7F, "hitcar");
    adlib_voice_off(4);
    adlib_note_on(4, 0x14);
    adlib_play_timbre(3, 0x7F, "turn");
    adlib_voice_off(3);
    adlib_note_on(3, msc_rand() % 8 + 0x3D);
    adlib_voice_off(4);
    adlib_voice_off(3);
    adlib_engine_on(1);
}

static void adlib_fx_thud(int u)                                         /* 12e0, slot 17 */
{
    (void)u;
    GUARD();
    adlib_play_timbre(3, 0x75, "turn1");
    adlib_voice_off(3);
    adlib_note_on(3, 0x10);
}

/* ---------------------------------------------------------------- engine drones (0b2c-0c5c) */

void adlib_engine_on(int on)
{
    GUARD();
    if (on) {
        adlib_play_timbre(3, 0x40, "motor");
        adlib_set_bend_range(6);
        adlib_pitch_bend(3, 0);                   /* lowest pitch: 6 semitones down */
        adlib_voice_off(3);
        adlib_note_on(3, 0x18);
    } else {
        adlib_set_volume(3, 0, 1);
        adlib_voice_off(3);
    }
}

void adlib_engine_set(u16 bend, u16 vol)
{
    if (DSB(DS_sound_gate)) { adlib_pitch_bend(3, bend); adlib_set_volume(3, vol, 1); }
}

void adlib_engine2_on(int on)                                            /* tune screen only */
{
    if (!DSB(DS_sound_gate)) return;
    if (on) {
        adlib_play_timbre(4, 0x40, "motor");
        adlib_set_bend_range(6);
        adlib_pitch_bend(4, 0);
        adlib_voice_off(4);
        adlib_note_on(4, 0x18);
    } else {
        adlib_voice_off(4);
        adlib_pitch_bend(4, 0x2000);
        adlib_play_timbre(4, 0x6A, "paint");      /* the garage timbre again */
    }
}

void adlib_engine2_set(u16 bend, u16 vol)
{
    if (DSB(DS_sound_gate)) { adlib_pitch_bend(4, bend); adlib_set_volume(4, vol, 1); }
}

void snd_adlib_engine(int on) { adlib_engine_on(on); adlib_engine2_on(on); }    /* tune-up screen */

void adlib_garage_timbres_load(void)                                     /* 1394 */
{
    if (DSB(DS_sound_gate)) {
        adlib_play_timbre(4, 0x6A, "paint");
        adlib_play_timbre(5, 0x70, "part");
    }
}

/* ---------------------------------------------------------------- music slots */

static void adlib_music_list_a(int u) { (void)u; if (DSB(DS_sound_gate)) mus_list_play(DS_SONG_LIST_A); }   /* 1318, slot 18 */
static void adlib_music_list_b(int u) { (void)u; if (DSB(DS_sound_gate)) mus_list_play(DS_SONG_LIST_B); }   /* 1356, slot 19 */
static void adlib_slot_nop(int u) { (void)u; }                           /* 13c4, slot 22 */
static void adlib_music_stop_slot(int u) { (void)u; mus_stop(); }        /* 13c6, slot 20 */
static void adlib_music_shutdown(int u) { (void)u; mus_stop(); mus_timer_remove(); }   /* 13cc, slot 21 */

/* 2c57:21b1 slot 23 */
void adlib_vec_off(int u)
{
    (void)u;
    if (DSW(DS_snd_state) == 2) {
        mus_stop();
        DSB(DS_sound_gate) = 0;
        for (int i = 0; i < 11; i++) adlib_voice_off(i);
    }
}

/* 2c57:21f0 slot 24 */
void adlib_vec_on(int u)
{
    (void)u;
    if (DSW(DS_snd_state) == 2) {
        if (DSW(DS_music_on) != 0 && (DSW(DS_ACT_WORD) & 0x1A) == 0) mus_resume();
        DSB(DS_sound_gate) = 1;
        switch (DSW(DS_ACT_WORD)) {               /* activity word set by the garage / race code */
        case 1:  adlib_engine_on(1); if (DSW(DS_siren_on) != 0) sound_vec(11, 0); break;   /* driving: drone back, siren forced off */
        case 2:  adlib_engine_on(1); adlib_engine2_on(1); break;                          /* tune screen */
        case 4:  sound_vec(4, 1); break;
        case 8:  sound_vec(0, 1); break;
        case 0x10: sound_vec(6, 1); break;
        }
    }
}

/* ---------------------------------------------------------------- init (13d8) */

void adlib_sound_init(void)
{
    for (int i = 0; i < 100; i++) DSB(DS_snd_vec + i) = DSB(DS_ADLIB_SLOTS + i);
    DSB(DS_sound_gate) = 1;
    adlib_music_init();                           /* chip reset, INT 8 = the music ISR */
    snd_bank_and_intro_load();
    DSW(DS_snd_device) = 0;
    DSW(DS_snd_state) = 1;
}

/* ---------------------------------------------------------------- drive ISR (sound.md 5.3, AdLib) */

void adlib_race_tick(void)
{
    u16 bend, vol;
    if (DSW(DS_ANIM_FLAG) == 0) {
        s16 rpm = DSS(DS_RPM);
        bend = rpm == 0x1644 ? 0x3FFF : (u16)(rpm * 3 - 0x1194);     /* idle 1500 rpm -> 0 */
        int a = (rpm - 0x5DC) / 128;
        vol = DSS(DSW(DS_CUR_CAR_PTR) + 0x1E) != 0 ? (u16)(a + 0x50) : 0x7F00;   /* clamped to 7Fh by set_volume */
    } else {
        s16 dd = (s16)(DSW(DS_ANIM_X) - DSW(DS_ANIM_REF));
        u16 d = (u16)(dd < 0 ? -dd : dd);
        u16 t = (u16)((u32)(u16)(d >> 1) * d);
        if (t > 20000) t = 20000;
        bend = (u16)(t >> 1);
        vol = DSS(DS_ANIM_MUFFLER) == 0 ? 0x7F00 : 0x50;
    }
    adlib_engine_set(bend, vol);
    if (DSW(DS_siren_on)) {
        u16 n = DSW(DS_SIREN_CNT);
        DSW(DS_SIREN_CNT) = (u16)(n + 1);
        if ((s16)n > 0x28) DSW(DS_SIREN_CNT) = 0;
        if (DSW(DS_SIREN_CNT) == 0) adlib_siren_bend(0);
        else if (DSW(DS_SIREN_CNT) == 0x14) adlib_siren_bend(1);
    }
    DSB(DS_SQUEAL_PH) = DSB(DS_SQUEAL_PH) > 14 ? 0 : (u8)(DSB(DS_SQUEAL_PH) + 1);   /* 0..15 phase counter */
    if (DSW(DS_squeal) == 0 && DSW(DS_on_shoulder) == 0) {
        if (DSW(DS_TYRE_NOISE)) adlib_fx_tyre(0);
    } else if (DSB(DS_SQUEAL_PH) == 0) adlib_fx_tyre(1);
    else if (DSB(DS_SQUEAL_PH) == 8) adlib_fx_tyre(0);
}

void adlib_registry(void)
{
    static const struct { u32 fn; void (*f)(int); } t[] = {
        { FN_adlib_fx_putt, adlib_fx_putt }, { FN_adlib_fx_tyre, adlib_fx_tyre }, { FN_adlib_fx_spray, adlib_fx_spray },
        { FN_adlib_fx_hum, adlib_fx_hum }, { FN_adlib_fx_tick, adlib_fx_tick }, { FN_adlib_fx_bolt, adlib_fx_bolt },
        { FN_adlib_fx_airwrench, adlib_fx_airwrench }, { FN_adlib_fx_lugnut, adlib_fx_lugnut }, { FN_adlib_fx_clank, adlib_fx_clank },
        { FN_adlib_fx_clank2, adlib_fx_clank2 }, { FN_adlib_fx_slide, adlib_fx_slide }, { FN_adlib_fx_siren, adlib_fx_siren },
        { FN_adlib_fx_click, adlib_fx_click }, { FN_adlib_fx_chirp_hi, adlib_fx_chirp_hi }, { FN_adlib_fx_chirp_lo, adlib_fx_chirp_lo },
        { FN_adlib_fx_crash, adlib_fx_crash }, { FN_adlib_fx_hit, adlib_fx_hit }, { FN_adlib_fx_thud, adlib_fx_thud },
        { FN_adlib_music_list_a, adlib_music_list_a }, { FN_adlib_music_list_b, adlib_music_list_b },
        { FN_adlib_music_stop_slot, adlib_music_stop_slot }, { FN_adlib_music_shutdown, adlib_music_shutdown },
        { FN_adlib_slot_nop, adlib_slot_nop }, { FN_adlib_vec_off, adlib_vec_off }, { FN_adlib_vec_on, adlib_vec_on },
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) codeptr_register(t[i].fn, (CodeFn)t[i].f);
}
