/* DEVELOPER AID (not part of the original, off by default), as SR1's race_debug_main.
 *
 * SR_DEBUG_RACE=drag|mulholland|aqueduct|grudge|cruise|bob|jail skips the title and the menus: the set-up of title_and_setup
 * 0001:3cc4 without the title and the protection screen, new_game, a stock car of model SR_DEBUG_MODEL (default 3) with
 * a full tank as the current car, then
 *   drag / mulholland / aqueduct   a race (road mode 1 / 0x10 / 8) against person SR_DEBUG_OPP (default 0; 21 = the
 *                                  King) for kicks, race_results, then the drive home;
 *   grudge                         Grudge Night (event17_run: the time run and the rounds);
 *   cruise                         the cruise to Burger's Drive-In (drive_to_event, location 2);
 *   bob                            the cruise, then Burger's Drive-In (bob_drive_in);
 *   jail                           the jail screen (jail).
 * The program exits afterwards. Combine with SDL_VIDEO_DRIVER=dummy SR_SNAPSHOT_DIR=... SR_KEYS=... (host.h): the
 * autopilot of the demo drives when the "demo" switch is given, e.g. SR_KEYS="1:03" for the sound menu. */
#include "game/race_int.h"
#include "game/race.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/flow.h"
#include "game/garage.h"
#include "game/garage_screens.h"
#include "platform/platform.h"
#include "platform/video.h"

static s16 env_int(const char *name, s16 def)
{
    const char *s = getenv(name);
    return s && *s ? (s16)atoi(s) : def;
}

/* the shared set-up: title_and_setup without the title and the protection screen, new_game, a licence name and (with
 * car) a stock car of model SR_DEBUG_MODEL as the current car */
static void debug_prepare(bool with_car)
{
    gfx_screen_mode(0);
    drv_pal_black();
    gfx_screen_mode(1);
    track_build_all(1);
    DSB(DS_save_name) = 0;
    DSB(DS_player_name) = 0;
    DSW(DS_demo) = DSW(DS_sw_demo);
    if (!DSW(DS_title_resident)) {
        lib_read_dir(2);
        hot_data_load();
        hall_load();
        if (DSS(DS_driver_id) == -2) pic_park_list(DS_RESIDENT_PICS);
    }
    pic_load_list(DS_RESIDENT_PICS, 1);
    if (getenv("SR_DEBUG_PROT")) { copy_protection(); cursor_ctl(-4); }   /* the protection screen first; as title_and_setup */
    cursor_ctl(-2);
    new_game();
    strcpy(ds_str(DS_player_name), "Tester");
    if (!with_car) return;
    u16 car = car_new(env_int("SR_DEBUG_MODEL", 3), 0, 0);
    DSW(DS_cur_car) = car;
    CARW(car, CR_GAS) = (u16)env_int("SR_DEBUG_GAS", 0xAA);       /* SR_DEBUG_GAS: a low tank for the gas station */
    car_setup(car, PLAYER);
}

/* DEVELOPER AID SR_DEBUG_END=king|summer|broke|garage (called at the start of game_loop, which then skips the title, the
 * protection screen and the new-game prompt and runs the garage loop from the garage):
 *   king     the ending after beating the King: ending_trigger (the picture, ending_anim), then DS:1BC6 |= 2 as bob_drive_in
 *            does; the loop goes on with the hall of fame (a new row, name edit) and the game-over menu;
 *   summer   the clock set past day 0x5A: summer_over_screen, hall of fame, game-over menu;
 *   broke    no car, no parts, money 0: "You're outta dough", hall of fame, game-over menu;
 *   garage   a new game with a stock car at the garage (for save / load runs with SR_KEYS).
 * SR_DEBUG_HOURS=n adds n game hours to the clock first. */
bool end_debug_main(void)
{
    const char *mode = getenv("SR_DEBUG_END");
    if (!mode || !*mode) return false;
    bool broke = !strcmp(mode, "broke");
    debug_prepare(!broke);
    if (broke) {
        DSSL(DS_money) = 0;
        DSB(DS_ad_ids) = 3;                           /* one car ad (broke_check: an empty list is never "broke") */
        DSB((u16)(DS_ad_ids + 1)) = 0xFF;
    }
    else if (!strcmp(mode, "summer")) DSL(DS_game_clock) = 0x1998u * 0x5Bu;
    else if (!strcmp(mode, "king")) {
        ending_trigger();                             /* bob_drive_in 0001:a88e */
        DSW(DS_game_over) |= 2;
    }
    DSL(DS_game_clock) += 0x222u * (u32)env_int("SR_DEBUG_HOURS", 0);
    return true;
}

bool race_debug_main(void)
{
    const char *mode = getenv("SR_DEBUG_RACE");
    if (!mode || !*mode) return false;
    debug_prepare(true);
    if (!strcmp(mode, "drag") || !strcmp(mode, "mulholland") || !strcmp(mode, "aqueduct")) {
        DSS(DS_location) = 2;
        DSS(DS_road_mode) = !strcmp(mode, "drag") ? 1 : !strcmp(mode, "mulholland") ? 0x10 : 8;
        opponent_load(env_int("SR_DEBUG_OPP", 0));
        set_paint_palette(DSS(DS_opp_paint));
        DSW(DS_bet_kind) = 0;
        DSW(DS_bet_amount) = 0;
        DSW(DS_bet_button) = 1;
        DSW(DS_in_challenge_race) = 1;
        race_start();
        u16 r = race_results();
        fprintf(stderr, "race_debug: race_results -> %u, won %u, result %04X/%04X\n", (unsigned)r,
                (unsigned)DSW(DS_player_won), (unsigned)PHW(PLAYER, PH_RESULT), (unsigned)PHW(OPP, PH_RESULT));
        DSW(DS_in_challenge_race) = 0;
        if (DSW(DS_cur_car) && (r == 1 || r == 2 || r == 3)) { DSS(DS_location) = 1; drive_to_event(); }
    } else if (!strcmp(mode, "jail")) {               /* the jail screen (a ticket / race 4) */
        DSS(DS_location) = 6;
        jail();
    } else if (!strcmp(mode, "grudge")) {
        DSS(DS_location) = 4;
        drive_to_event();
        DSS(DS_location) = 2;
        event17_run();
    } else {
        DSS(DS_location) = 2;
        drive_to_event();
        if (!strcmp(mode, "bob")) bob_drive_in();
    }
    fprintf(stderr, "race_debug: done\n");
    platform_exit();
}
