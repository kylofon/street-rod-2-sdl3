#pragma once
/* Private declarations of the sound module (not for other subsystems: use sound.h). */
#include "sound/sound.h"
#include "host.h"
#include "codeptr.h"

/* DGROUP offsets that symbols.h names differently or lacks */
#define DS_MUSIC_HOLD     0x5302   /* u16: duration counter frozen (sound.md 2) */
#define DS_RPM            0xA10E   /* s16: engine rpm of the player's car (PLAYER phys + 0Eh) */
#define DS_ANIM_FLAG      0x1BCA   /* u16: animation player running (garage car drive) */
#define DS_ANIM_MUFFLER   0xB8D4   /* i16: 1 = the car has a muffler */
#define DS_ANIM_X         0xAD5C
#define DS_ANIM_REF       0xB866
#define DS_CUR_CAR_PTR    0xAA02
#define DS_ACT_WORD       0x1F7C   /* activity word: 1 driving, 2 tune screen, 4 / 8 / 10h animations */
#define DS_SPK_PULSE      0x881E   /* speaker engine low-tone pulse phase */
#define DS_MUS_RESTORE    0xB8E0   /* music was on before the race */
#define DS_RACE_FLAG_A2B2 0xA2B2
#define DS_VIB_LAST_DIV   0x8822
#define DS_TYRE_NOISE     0x72EE
#define DS_SIREN_CNT      0x2004
#define DS_SQUEAL_PH      0x200A
#define DS_BG_LOOP_TBL    0x540C
#define DS_BG_VIB_TBL     0x5432
#define DS_BG_SONG_BASE   0x5594
#define DS_SPK_SLOTS      0x530A
#define DS_ADLIB_SLOTS    0x73C2
#define DS_SONG_LIST_A    0x73A0
#define DS_SONG_LIST_B    0x73B0

/* SongState18 (speaker sequencer) at DS_bg_song / DS_fx_song, near pointers into DGROUP */
#define SS_NOTE      0x00   /* u16 PIT divisor of the sounding note, 0 = rest */
#define SS_DUR       0x02   /* u16 ticks left */
#define SS_REP       0x04   /* s16 plays of the current section so far */
#define SS_PLAYING   0x06   /* u16 */
#define SS_REGATE    0x08   /* u8 */
#define SS_BASE      0x0A
#define SS_SEC_START 0x0C
#define SS_CUR       0x0E
#define SS_LOOP_TBL  0x10
#define SS_LOOP_PTR  0x12
#define SS_VIB_TBL   0x14
#define SS_VIB_PTR   0x16

/* ---- spk.c: speaker driver (slots, sequencer, effects) */
void speaker_sound_init(void);
void speaker_music_tick(void);         /* timer_isr: one sequencer tick of the active song state */
void speaker_race_tick(u8 phase);
void spk_vec_off(int unused);          /* slot 23 */
void spk_vec_on(int unused);           /* slot 24 */
void spk_song_defaults(void);
void spk_set_hz(s32 hz);
void spk_registry(void);
void spk_engine_tone(u16 car_ign);

/* ---- adlib.c: OPL2 driver (sound.md 3, 2c57:16c6..24ad) */
void adlib_write(u8 reg, u8 val);
void adlib_reset(void);
void adlib_voice_off(int v);
void adlib_note_on(int v, int note);
void adlib_pitch_bend(int v, u16 bend);
void adlib_set_volume(int v, u16 vol, int store);
void adlib_voice_timbre(int v, const u8 *buf56);
void adlib_set_bend_range(u16 range);
void adlib_set_perc_mode(u8 mode);
void adlib_play_timbre(int v, int vol, const char *name);

/* ---- mus.c: bank, .MUS files, interpreter, timer ISR */
void snd_bank_and_intro_load(void);
void mus_songs_load(void);
void mus_play(int slot);
void mus_list_play(u16 list);
void mus_stop(void);
void mus_resume(void);
void adlib_music_init(void);
void mus_timer_remove(void);
int  snd_timbre_lookup(const char *name, int *index, u8 *out56);
extern bool mus_isr_installed;

/* ---- adfx.c: AdLib effects, engine drone, AdLib slots */
void adlib_sound_init(void);
void adlib_engine_on(int on);
void adlib_engine_set(u16 bend, u16 vol);
void adlib_engine2_on(int on);
void adlib_engine2_set(u16 bend, u16 vol);
void adlib_fx_tyre(int on);
void adlib_siren_bend(int x);
void adlib_garage_timbres_load(void);
void adlib_vec_off(int unused);        /* slot 23 */
void adlib_vec_on(int unused);         /* slot 24 */
void adlib_registry(void);
void adlib_race_tick(void);
