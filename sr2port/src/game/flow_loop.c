/* game_flow: the game loop 0001:4dd6 with the garage dispatch and the event trips, and the game part of main
 * 0001:07b0 (port/spec/game_flow.md "game_loop"). Ground truth: Ghidra decompile port/decomp/sr2_ds.c 4974-5241.
 * Replaces the placeholder game_main of game/flow_stub.c. */
#include "game/game.h"
#include "game/events.h"
#include "game/race.h"
#include "game/ds_events.h"

#include "game/flow.h"
#include "game/garage.h"
#include "game/garage_screens.h"
#include "game/records.h"
#include "game/snd.h"
#include "game/ui.h"
#include "platform/platform.h"
#include "platform/video.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

int game_run_ms;                                  /* developer aid (main.c --run-ms N): quit after N ms */

/* clock_hour() = `game_clock += 0x222` (flow.h): every trip costs two game hours, the newspaper one */

/* 0001:4dd6 game_loop. Returns when the player quits. Original variables: again [bp-4], code [bp-8], redraw [bp-6]. */
void game_loop(void)
{
    s16 again = 1;
    s16 code = title_and_setup();
    s16 prev, redraw, r;

new_or_load:
    do {
        prev = code;
        if (code == -1) {
            again = 1;
            code = new_game_prompt();
        } else {
            code = load_game_screen();
        }
        if (code == -0x29A) { code = -1; again = 1; }
    } while (code != 0 && again);
    status_label(ds_str(DSW(DS_money_label)));                    /* "Bankroll:" and the money */
    redraw = 1;
    if (prev == -1 && DSS(DS_car_count) < 16 && DSS(DS_part_count) < PART_LIMIT) newspaper_front();
    again = 0;

    for (;;) {
        code = garage_screen(redraw);
        if (getenv("SR_TRACE")) fprintf(stderr, "trace: %ldms garage code %d\n", (long)clock(), code);   /* PORT: developer aid */
        if (code == 0) goto new_or_load;
        if (code == FLOW_GAME_OVER) {
            if (DSW(DS_game_over) == FLOW_END_SUMMER) summer_over_screen();
            hall_of_fame_screen((s16)(DSW(DS_game_over) & FLOW_END_KING));
            cursor_ctl(1);
            DSW(DS_game_over) = 0;
            again = 1;
            code = game_over_menu();
            if (code == 4) return;                                /* Quit */
            cursor_ctl(-2);
            DSW(DS_game_over_cache_flag) = 0;
            goto new_or_load;                                     /* -1 new game, -2 the load screen */
        }
        redraw = 2;
        switch (code) {
        case 1:                                                   /* N "Check out the newspaper" */
            if (garage_full_check()) { redraw = 0; continue; }
            if (DSW(DS_cur_car) == 0) redraw = 1;
            newspaper_front();
            if (DSW(DS_cur_car) == 0) redraw = DSW(DS_car_list) != 0 ? 1 : 2;
            clock_hour();
            continue;

        case 2:                                                   /* D "Hit the street": Burger's Drive-In */
        case 15:                                                  /* G "Wanna tank ?": the gas station */
        case 17: {                                                /* mouse only: Grudge Night (Wednesdays) */
            s16 msg;
            if (garage_full_check()) { redraw = 0; continue; }
            if (code == 17 && day_index() % 7 != 2) {             /* "Not until Wednesday!" */
                msg_box(3);
                redraw = 0;
                wait_ticks(10);
                continue;
            }
            u16 car = DSW(DS_cur_car);
            if (car == 0) {
                msg = DSW(DS_car_list) == 0 ? 0x09 : 0x28;        /* "You've got no car" / "Pick a car first!" */
            } else if (car_runnable(car, 1) == 0) {
                msg = 0x27;                                       /* "Your car's not running, speedy!" */
            } else {
                car_setup(car, DS_player_phys);                   /* 0001:c294 */
                s16 fuel = DSS((u16)(car + CR_GAS));              /* 0.1 gal */
                if (code == 2) {
                    if (8 < fuel) {
                        DSS(DS_location) = 2;
                        drive_to_event();
                        clock_hour();
                        bob_drive_in();
                        clock_hour();
                        redraw = 1;
                        continue;
                    }
                    msg = 0x24;                                   /* not enough gas */
                } else if (code == 15) {
                    if (fuel >= 0x96) msg = 0x25;                 /* "You've got a full tank!" */
                    else if (DSSL(DS_money) == 0) msg = 0x26;     /* "Gas ain't free!" */
                    else {
                        DSS(DS_location) = 3;
                        drive_to_event();
                        clock_hour();
                        event15_run();
                        clock_hour();
                        redraw = 1;
                        continue;
                    }
                } else {
                    if (0x17 < fuel) {
                        if (!pay_or_msg(0x1E, 0x72)) { redraw = 0; continue; }   /* the $30 stake, else msg 0x72 */
                        DSS(DS_location) = 4;
                        drive_to_event();
                        clock_hour();
                        DSS(DS_location) = 2;
                        event17_run();
                        clock_hour();
                        redraw = 1;
                        continue;
                    }
                    msg = 0x24;
                }
            }
            msg_box(msg);
            redraw = 0;
            continue;
        }

        case 3:                                                   /* X "Calendar" */
            calendar_show(0);
            redraw = 0;
            continue;

        case 4:                                                   /* Q "Time to quit" */
            redraw = 0;
            r = quit_menu();
            switch (r) {
            case -0x17:                                           /* Save Game */
                if (garage_full_check()) continue;                /* (the original's 0x309 code: stays in the loop) */
                save_game_screen();
                redraw = 2;
                if (DSW(DS_cur_car) == 0 && DSW(DS_car_list) != 0) redraw = 1;
                continue;
            case -0x16:                                           /* Forget it */
            case -4:
                continue;
            case -0x14:                                           /* Quit */
                return;
            default:
                code = r;                                         /* -1 New Game, -2 Old Game */
                goto new_or_load;
            }

        case 5:  sell_spare_parts();     redraw = 0; continue;    /* L "Sell parts" */
        case 6:  redraw = your_cars();   continue;                /* C "Cars" */
        case 7:  paint_job();            redraw = 0; continue;    /* P "New paint job" */
        case 8:  change_tires();         redraw = 0; continue;    /* T "Change tires" */
        case 9:  change_transmission();  continue;                /* A "Change transmission" (redraw 2) */
        case 10: arena_reset_stacks();                            /* 1a21:9b26 */
                 engine_bay(DSW(DS_cur_car));
                 continue;                                        /* H "Pop the hood" */
        case 11: customize(2);           redraw = 0; continue;    /* B "Rear bumper" */
        case 12: customize(1);           redraw = 0; continue;    /* F "Front bumper" */
        case 13: customize(4);           redraw = 0; continue;    /* R "The roof" */
        case 14: redraw = under_car_menu(); continue;             /* S "Under the car" (2925:3174) */
        case 16: car_info_sr2();         redraw = 0; continue;    /* I "Car info" */
        case -40:                                                 /* the juke box: "Catch some tunes" / "Squelch it !" */
            if (DSW(DS_music_on) == 0) { DSW(DS_music_on) = 1; music_resume(); }
            else { music_stop(); DSW(DS_music_on) = 0; }
            redraw = 0;
            wait_ticks(10);
            continue;
        default:
            goto new_or_load;                                     /* again = 0: one new-game (-1) or load call */
        }
    }
}

/* --run-ms: a timer thread asks the host to quit (the host handles SDL_EVENT_QUIT in host_pump) */
static Uint32 SDLCALL run_ms_expired(void *userdata, SDL_TimerID id, Uint32 interval)
{
    (void)userdata; (void)id; (void)interval;
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&ev);
    return 0;
}

/* 0001:07b0 main after the platform initialisation: wait_click_or_key(1), game_loop, platform_exit.
 * (The "demo" switch only sets DS:B8D0 in platform_main_init; the demo player is demo_step, ui_wait.c.) */
int game_main(int argc, char **argv)
{
    platform_main_init(argc, argv);
    if (game_run_ms > 0) SDL_AddTimer((Uint32)game_run_ms, run_ms_expired, NULL);   /* PORT: developer aid */
    race_debug_main();                                /* PORT: developer aid SR_DEBUG_RACE (game/race_debug.c) */
    wait_click_or_key(1);
    game_loop();
    platform_exit();
}
