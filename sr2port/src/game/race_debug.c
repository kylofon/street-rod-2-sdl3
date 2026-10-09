/* DEVELOPER AID (not part of the original, off by default), as SR1's race_debug_main.
 *
 * SR_DEBUG_RACE=drag|mulholland|aqueduct|grudge|cruise|bob skips the title and the menus: the set-up of title_and_setup
 * 0001:3cc4 without the title and the protection screen, new_game, a stock car of model SR_DEBUG_MODEL (default 3) with
 * a full tank as the current car, then
 *   drag / mulholland / aqueduct   a race (road mode 1 / 0x10 / 8) against person SR_DEBUG_OPP (default 0; 21 = the
 *                                  King) for kicks, race_results, then the drive home;
 *   grudge                         Grudge Night (event17_run: the time run and the rounds);
 *   cruise                         the cruise to Burger's Drive-In (drive_to_event, location 2);
 *   bob                            the cruise, then Burger's Drive-In (bob_drive_in).
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

bool race_debug_main(void)
{
    const char *mode = getenv("SR_DEBUG_RACE");
    if (!mode || !*mode) return false;
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
    cursor_ctl(-2);
    new_game();
    strcpy(ds_str(DS_player_name), "Tester");
    u16 car = car_new(env_int("SR_DEBUG_MODEL", 3), 0, 0);
    DSW(DS_cur_car) = car;
    CARW(car, CR_GAS) = 0xAA;
    car_setup(car, PLAYER);
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
