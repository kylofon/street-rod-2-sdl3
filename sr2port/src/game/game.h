#pragma once
/* The game proper. Phase 4 placeholder: game_main shows LIB1 picture #0 (S4.1); the real
 * game_loop (SR2 main, 0001:xxxx) arrives with S5.x. */
#include "types.h"

int game_main(void);
/* --run-ms N (developer aid): leave game_main after N ms; 0 = run until the window is closed. */
extern int game_run_ms;
