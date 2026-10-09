/* PORT: S5.3 stubs for the race-module functions that game_flow (game_loop, event15, event17) calls.
 * S5.3 replaces this whole file with the real race module (port/spec/race.md); nothing here is original code.
 *
 * Stubs (each marked "PORT: S5.3 stub"), all declared in game/events.h under their port/symbols.csv names:
 *   0001:8c68 drive_to_event    only the fuel use of a trip (-0.2 gal); the cruise scene is not drawn
 *   0001:8d0a opp_palette_set   no-op
 *   0001:a626 bob_drive_in      no-op: Burger's Drive-In (street race, Mulholland, Aqueduct, the King) is not played
 *   0001:9a02 opponent_load     points DS:A80A / DS:A7AA at the person / model records only
 *   0001:be82 race_start        no-op: no race is run
 *   0001:b584 race_results      the race is "lost": DS:9B3E = 0 (Grudge Night: eliminated), returns 0
 *   0001:c6f6 car_max_speed     a plausible top speed from the model number (no physics)
 *   0001:c294 car_setup         no-op
 * Money and the game clock are untouched, so the game loop's own bookkeeping (trip hours, $30 stake) still works. */
#include "game/events.h"

#include "game/records.h"

/* 0001:8c68 drive_to_event */
void drive_to_event(void) /* PORT: S5.3 stub */
{
    if (DSS(DS_location) != 0) {
        u16 car = DSW(DS_cur_car);
        if (car != 0) {
            s16 fuel = DSS((u16)(car + CR_GAS));
            DSS((u16)(car + CR_GAS)) = (s16)(fuel < 3 ? 0 : fuel - 2);   /* the trip burns 0.2 gal */
        }
    }
}

/* 0001:8d0a opp_palette_set */
void opp_palette_set(s16 idx) /* PORT: S5.3 stub */
{
    (void)idx;
}

/* 0001:a626 bob_drive_in */
void bob_drive_in(void) /* PORT: S5.3 stub */
{
}

/* 0001:9a02 opponent_load: person i of DS_people -> DS:A80A, its model record -> DS:A7AA */
void opponent_load(s16 i) /* PORT: S5.3 stub */
{
    u16 rec = (u16)(DS_people + PERSON_REC * i);
    DSW(DS_opp_rec) = rec;
    DSW(DS_opp_model_ptr) = (u16)(DS_MODELS + MODEL_REC * DSS((u16)(rec + 8)));
}

/* 0001:be82 race_start */
void race_start(void) /* PORT: S5.3 stub */
{
}

/* 0001:b584 race_results: 0 lost, 1 won, 2 left the event */
u16 race_results(void) /* PORT: S5.3 stub */
{
    DSS(DS_tourn_alive) = 0;                      /* Grudge Night: the player did not qualify / lost the round */
    return 0;
}

/* 0001:c6f6 car_max_speed */
s16 car_max_speed(s16 model) /* PORT: S5.3 stub */
{
    if (model < 0) {
        u16 car = DSW(DS_cur_car);
        model = car ? DSS((u16)(car + CR_MODEL)) : 0;
    }
    return (s16)(60 + 3 * model);
}

/* 0001:c294 car_setup */
void car_setup(u16 car, u16 phys) /* PORT: S5.3 stub */
{
    (void)car;
    (void)phys;
}

/* 0dcb:2866 road_seed_init / 0dcb:24d8 road_set_param (race_render) */
void road_seed_init(s16 fresh) { (void)fresh; } /* PORT: S5.3 stub */
void road_set_param(s16 v) { (void)v; }         /* PORT: S5.3 stub */
