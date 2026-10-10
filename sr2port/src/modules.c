/* Wiring of the cross-subsystem hooks (modules.h). Each stand-in below is replaced by the ported
 * function once its subsystem lands: e.g.  modules.snd_timer_tick = snd_timer_tick;  */
#include "modules.h"
#include "game/ui.h"
#include "game/events.h"
#include "game/race.h"
#include "sound/sound.h"

#include <stdio.h>

Modules modules;

void modules_init(void)
{
    race_register_hooks();                        /* race_phys_step (race_isr from 0001:28fa) */

    snd_register_codeptrs();                      /* the 50 slot routines of DS:530A / DS:73C2 (S5.4) */
    modules.snd_state_init = snd_state_init;
    modules.adlib_timbres_load = snd_adlib_timbres_load;
    modules.snd_timer_tick = snd_timer_tick;
    modules.snd_race_tick = snd_race_tick;
    modules.snd_race_enter = snd_race_enter;
    modules.snd_race_leave = snd_race_leave;
    modules.snd_exit = snd_exit;
    modules.snd_init = snd_init;
    modules.mus_songs_load = snd_mus_songs_load;

    modules.event15_overlay = pump_strip_draw;     /* 2925:1cae */

    ui_register_hooks();                          /* fatal_message 1a21:204c */
}
