/* Wiring of the cross-subsystem hooks (modules.h). Each stand-in below is replaced by the ported
 * function once its subsystem lands: e.g.  modules.snd_timer_tick = snd_timer_tick;  */
#include "modules.h"
#include "game/ui.h"
#include "game/events.h"

#include <stdio.h>

Modules modules;

/* ---- stand-ins (PORT: temporary, until the owning subsystem is ported) */

static void nop(void) {}
static void nop_u8(u8 v) { (void)v; }
static void nop_s16(s16 v) { (void)v; }

void modules_init(void)
{
    modules.race_phys_step = nop;                 /* S5.3 */

    modules.snd_state_init = nop;                 /* S5.4 */
    modules.adlib_timbres_load = nop;
    modules.snd_timer_tick = nop;
    modules.snd_race_tick = nop_u8;
    modules.snd_race_enter = nop;
    modules.snd_race_leave = nop;
    modules.snd_exit = nop;
    modules.snd_init = nop_s16;
    modules.mus_songs_load = nop;

    modules.event15_overlay = pump_strip_draw;     /* 2925:1cae */

    ui_register_hooks();                          /* fatal_message 1a21:204c */
}
