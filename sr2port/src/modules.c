/* Wiring of the cross-subsystem hooks (modules.h). Each stand-in below is replaced by the ported
 * function once its subsystem lands: e.g.  modules.snd_timer_tick = snd_timer_tick;  */
#include "modules.h"

#include <stdio.h>

Modules modules;

/* ---- stand-ins (PORT: temporary, until the owning subsystem is ported) */

static void nop(void) {}
static void nop_u8(u8 v) { (void)v; }
static void nop_s16(s16 v) { (void)v; }
static void nop_page(FarPtr p, s16 i) { (void)p; (void)i; }

/* 1a21:204c draws a message box and waits for a key. PORT: until game_flow (S5.2) is ported the stand-in
 * only reports the text on stderr (every caller leaves through platform_exit right after). */
static void fatal_message_stub(const char *s)
{
    fprintf(stderr, "message: %s\n", s);
}

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

    modules.event15_overlay = nop_page;           /* garage / race */

    modules.fatal_message = fatal_message_stub;   /* S5.2 */
}
