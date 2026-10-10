#pragma once
/* Public API of the race subsystem (port/spec/race.md, race_render.md) beyond the game_flow entry points that
 * game/events.h declares (drive_to_event, opp_palette_set, bob_drive_in, opponent_load, race_start, race_results,
 * car_max_speed, car_setup). Implemented in game/race_*.c. */
#include "types.h"
#include "mem.h"
#include "game/events.h"

/* Registers the race hooks in `modules` (modules.h): modules.race_phys_step. */
void race_register_hooks(void);

/* 0dcb:2866 track_build_all: page pointers, banks, town (full = 1: new game / start-up, new race-road seed into
 * DS:A9F8), the race road from the seed (full = 0: after loading a saved game) */
void track_build_all(s16 full);
/* 0dcb:24d8 track_build_course: the course part; 100 = a random layout into DS:7DFE, else rebuild DS:7DFE */
void track_build_course(s16 arg);

/* DEVELOPER AID (game/race_debug.c): SR_DEBUG_RACE=drag|mulholland|aqueduct|grudge|cruise|bob runs that drive straight
 * after the platform start-up and exits; returns false (doing nothing) when the variable is not set. */
bool race_debug_main(void);
bool end_debug_main(void);   /* SR_DEBUG_END=king|summer|broke|garage (race_debug.c) */
