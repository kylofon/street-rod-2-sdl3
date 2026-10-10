#pragma once
/* Sound (S5.4): sound.md. The 25-slot dispatch table DS:BB8C, the PC-speaker driver (table-driven
 * song sequencer, busy-loop effects), the AdLib driver (OPL2 register writes into Nuked-OPL3 through
 * the host), the MSR.SND bank, the .MUS interpreter and the music timer ISR 2f1b:00cb.
 *
 * Files: slots.c (table, init, platform hooks, music control), spk.c (speaker), adlib.c (OPL2 driver),
 * mus.c (bank, .MUS, mus_tick, timer ISR), adfx.c (AdLib effects, engine drone).
 *
 * Who calls what
 *   game code           sound_vec(k, arg)  = (*DS:BB8C[k])(arg); music_stop / music_resume / music_mute;
 *                       snd_adlib_engine / snd_ignition_tone (tune-up screen), music_resume_after_race
 *   platform (timer.c)  snd_int8_set instead of host_set_tick_handler (the AdLib music ISR owns INT 8)
 *   modules.c           the hooks of modules.h (snd_*), snd_register_codeptrs() */
#include "types.h"
#include "mem.h"
#include "symbols.h"

/* slot k of the table with an int argument (extra arguments are ignored; `sound_vec(k)` passes 0) */
void snd_slot_call_va(int k, int arg, ...);
/* PORT: sound_vec(k) / sound_vec(k, arg): the argument defaults to 0 */
#define sound_vec(...) snd_slot_call_va(__VA_ARGS__, 0, 0)

void music_stop(void);                 /* 0001:1728 */
void music_resume(void);               /* 0001:1784 */
void music_mute(s16 on);               /* 2c57:00f6 (speaker slot 22) */
void music_resume_after_race(void);    /* 0001:2164 */
void tune_start(int word_off, int delay);   /* 0001:2002 */
void tune_rewind(void);                /* 0001:208a */

/* tune-up screen (garage ignition_tune, sound.md 5.4) */
void snd_adlib_engine(int on);         /* 2c57:0b2c(on) + 2c57:0bd2(on) */
void snd_ignition_tone(u16 car);       /* speaker: PIT channel 2 high byte; AdLib: both drones */

/* int 8 (the game's timer / drive ISR): the AdLib music ISR is the vector and chains to it */
void snd_int8_set(void (*isr)(void));

/* hooks of modules.h */
void snd_register_codeptrs(void);      /* codeptr_register of every slot routine */
void snd_state_init(void);             /* platform_install: song states, DS:ADD2, DS:9DB0 */
void snd_adlib_timbres_load(void);     /* 2c57:1394 */
void snd_timer_tick(void);             /* timer_isr sound part */
void snd_race_tick(u8 phase);          /* race_isr sound part */
void snd_race_enter(void);
void snd_race_leave(void);
void snd_exit(void);                   /* slot 21 */
void snd_init(s16 device);             /* 2c57:05c8 (1) / 2c57:13d8 (0) */
void snd_mus_songs_load(void);         /* 2c57:0ade */
