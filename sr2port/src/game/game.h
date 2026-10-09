#pragma once
/* The game entry: main.c calls game_main(argc, argv) after the host and the display model are up.
 * game_flow (S5.2) owns it; until then game/flow_stub.c stops after the title. */
extern int game_run_ms;           /* developer aid: --run-ms N, 0 = run until the window is closed */
int game_main(int argc, char **argv);
