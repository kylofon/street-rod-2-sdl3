#pragma once
/* Calls from the platform / video layer into subsystems that are ported separately (race, game_flow,
 * sound S5.4, the UI toolkit). The original calls them directly; the port goes through these function
 * pointers so each subsystem can be built and tested alone. modules_init() (modules.c) sets every hook
 * to its port implementation, or to a documented stand-in while that subsystem is missing. The
 * coordinator wires new implementations in modules.c only. */
#include "mem.h"

typedef struct {
    /* ---- race (port/spec/race.md section 3) */
    void (*race_phys_step)(void);        /* race_isr 0001:2622 step 6: one car-control / physics step */

    /* ---- sound (port/spec/sound.md, S5.4) */
    void (*snd_state_init)(void);        /* platform_install step 3: song states, DS:ADD2, DS:9DB0 */
    void (*adlib_timbres_load)(void);    /* platform_install step 6 (DS:0050 == 0): 2c57:1394 */
    void (*snd_timer_tick)(void);        /* timer_isr step 3: slot[23] / speaker song tick / slot[24] */
    void (*snd_race_tick)(u8 phase);     /* race_isr step 3 (phase = DS:1F76 & 1) */
    void (*snd_race_enter)(void);        /* race_isr_enter */
    void (*snd_race_leave)(void);        /* race_isr_leave */
    void (*snd_exit)(void);              /* platform_exit: sound_vec(21) */
    void (*snd_init)(s16 device);        /* main: speaker_sound_init 2c57:05c8 (1) / adlib_sound_init 2c57:13d8 (0) */
    void (*mus_songs_load)(void);        /* main, DS:0050 == 0: 2c57:0ade */

    /* ---- garage / race (port/spec/garage.md, race.md) */
    void (*event15_overlay)(FarPtr page, s16 i);   /* 2925:1cae: event 15 overlay drawn by ega_anim_step */

    /* ---- game_flow / UI (port/spec/game_flow.md, garage.md) */
    void (*fatal_message)(const char *s);/* 1a21:204c: message box "... Hit Return when ready" (errors,
                                            "Quit demo") */
} Modules;

extern Modules modules;

void modules_init(void);
