/* Sound front end: the 25-slot dispatch table DS:BB8C (sound.md 1.2), the sound parts of the platform ISRs
 * (5.2, 5.3), music control (0001:1728 / 1784 / 2164) and the device initialisation.
 *
 * The slot table holds far code pointers exactly as the original; every routine is registered with codeptr_register
 * (snd_register_codeptrs) and called through the table, so a slot means the same on both devices. */
#include "sound/snd_int.h"

#include "platform/platform.h"

/* ---------------------------------------------------------------- dispatch */

#include <stdarg.h>

static void slot_call(int k, int arg)
{
    FarPtr p = ds_far((u16)(DS_snd_vec + 4 * k));
    CodeFn f = codeptr_lookup(p);
    if (f) ((void (*)(int))f)(arg);
}

void snd_slot_call_va(int k, int arg, ...) { slot_call(k, arg); }

void snd_register_codeptrs(void)
{
    spk_registry();
    adlib_registry();
}

/* ---------------------------------------------------------------- music control */

/* the jukebox button -40 in screen 0 (DS:3AD8[0]): label pointer at +2. PORT: the search is bounded. */
static u16 find_jukebox_button(void)
{
    u16 p = DSW(DS_SCREENS);
    for (int i = 0; i < 64; i++, p = (u16)(p + 0x12))
        if (DSS(p + 6) == -40) return p;
    return 0;
}

void music_stop(void)                                                    /* 0001:1728 */
{
    u16 rec = find_jukebox_button();
    if (DSW(DS_snd_device) == 1) {
        u16 s = DSW(DS_music_cur);
        if (DSW(s + SS_PLAYING) == 0) return;                /* nothing to stop: no label change, no click_clear */
        DSW(s + SS_PLAYING) = 0;
        sound_vec(23);
        DSB(DSW(DS_music_cur) + SS_REGATE) = 1;
        if (rec) DSW(rec + 2) = DS_lbl_tunes;                /* "Catch some tunes" */
    } else if (DSW(DS_snd_device) == 0) {
        if (rec) DSW(rec + 2) = DS_lbl_tunes;
        if (DSB(DS_sound_gate) != 0) mus_stop();
    } else return;
    click_clear();
}

void music_resume(void)                                                  /* 0001:1784 */
{
    u16 rec = find_jukebox_button();
    if (DSW(DS_snd_device) == 1) {
        if (DSW(DS_music_on) == 0) return;
        u16 s = DSW(DS_music_cur);
        if (DSW(s + SS_PLAYING) != 0) return;
        if (DSW(s + SS_BASE) == DSW(s + SS_SEC_START)) return;   /* never started / just rewound */
        DSW(s + SS_PLAYING) = 1;
        DSB(s + SS_REGATE) = 1;
        if (rec) DSW(rec + 2) = DS_lbl_squelch;              /* "Squelch it !" */
    } else if (DSW(DS_snd_device) == 0) {
        if (rec) DSW(rec + 2) = DS_lbl_squelch;
        if (DSB(DS_sound_gate) != 0) mus_resume();
    } else return;
    click_clear();
}

void music_resume_after_race(void)                                       /* 0001:2164 */
{
    if (DSW(DS_MUS_RESTORE) != 0) {
        DSW(DS_music_on) = 1;
        music_resume();
    }
}

/* ---------------------------------------------------------------- hooks (modules.h) */

void snd_state_init(void) { spk_song_defaults(); }                       /* platform_install 5.1 */
void snd_adlib_timbres_load(void) { adlib_garage_timbres_load(); }
void snd_mus_songs_load(void) { mus_songs_load(); }

void snd_init(s16 device)
{
    if (device == 1) speaker_sound_init();
    else adlib_sound_init();
}

void snd_exit(void) { sound_vec(21); }

/* 0001:2246, sound.md 5.2 */
void snd_timer_tick(void)
{
    if (DSB(DS_sound_on) == 0 && DSB(DS_sound_gate) != 0) sound_vec(23);   /* sound switched off: silence */
    if (DSW(DS_snd_device) == 1 && DSW(DSW(DS_music_cur) + SS_PLAYING) != 0) speaker_music_tick();
    if (DSW(DS_snd_device) == 0 && DSB(DS_sound_on) != 0 && DSB(DS_sound_gate) == 0) sound_vec(24);
}

/* 0001:2622, sound.md 5.3 */
void snd_race_tick(u8 phase)
{
    if (DSB(DS_sound_on) == 0) {
        if (DSB(DS_sound_gate)) sound_vec(23);
        return;
    }
    if (DSB(DS_sound_gate) != DSB(DS_sound_on)) sound_vec(24);
    if (DSW(DS_snd_device) == 1) speaker_race_tick(phase);
    else if (DSW(DS_snd_device) == 0) adlib_race_tick();
}

/* 0001:20a0 race_isr_enter, sound part (the vector swap is in timer.c) */
void snd_race_enter(void)
{
    DSW(DS_RACE_FLAG_A2B2) = 1;                   /* the rest of race_isr_enter: no music toggle while driving */
    DSW(DS_ACT_WORD) = 1;
    DSW(0x1F78) = 0;
    if (DSW(DS_snd_device) == 0) {
        if (DSW(DS_ANIM_FLAG) == 0) {
            DSW(DS_MUS_RESTORE) = DSW(DS_music_on);
            if (DSW(DS_music_on) != 0) { music_stop(); DSW(DS_music_on) = 0; }   /* music off while driving */
        }
        adlib_engine_on(1);
        adlib_engine_set(0, 0x50);
    }
}

/* 0001:2176 race_isr_leave, sound part */
void snd_race_leave(void)
{
    DSW(DS_RACE_FLAG_A2B2) = 0;
    if (DSW(DS_snd_device) == 1) {
        sound_vec(23);
    } else if (DSW(DS_snd_device) == 0) {
        if (DSW(DS_ANIM_FLAG) == 0 && DSW(DS_in_challenge_race) == 0) music_resume_after_race();
        adlib_garage_timbres_load();
        adlib_engine_on(0);
    }
    DSW(DS_ACT_WORD) = 0;
    DSW(DS_siren_on) = 0;
    DSW(DS_squeal) = 0;
    DSW(DS_on_shoulder) = 0;
}

/* tune-up screen (sound.md 5.4) */
void snd_ignition_tone(u16 car)
{
    s8 p = DSC((u16)(car + 0x23));
    if (DSW(DS_snd_device) == 1) {
        spk_engine_tone((u16)(u8)p);
    } else {
        u16 vol = (u16)((p == 0 ? -8 : 0) + 0x52);
        adlib_engine_set((u16)((u8)((p + 16) * 2) << 8), vol);
        adlib_engine2_set((u16)((u8)(p * 3 + 0x20) << 8), vol);
    }
}
