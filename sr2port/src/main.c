/* Street Rod 2 SDL3 port - entry point.
 *
 * usage: sr2port [--game-dir DIR] [--scale N] [--fullscreen] [--check] [--run-ms N] [switches of SR.EXE]
 *   --game-dir    folder with the original game files (default: "Game" in the working directory)
 *   --scale       initial window scale: 320x240 times N (default 3)
 *   --fullscreen  start in full screen (Alt+Enter switches)
 *   --check       load and verify the original executable, print a summary and exit (no window)
 *   --run-ms N    developer aid: exit after N ms (headless snapshot runs)
 *   the original's command-line switches are passed on as they are: 1, nomouse, demo, nouemem, e c v h t (force EGA / CGA / VGA / Hercules / Tandy: skips the video menu)
 */
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/game.h"
#include "host.h"
#include "mem.h"
#include "platform/ega.h"

static int usage(const char *prog)
{
    fprintf(stderr, "usage: %s [--game-dir DIR] [--scale N] [--fullscreen] [--check] [--run-ms N] "
                    "[1] [nomouse] [demo] [nouemem] [e|c|v|h|t]\n", prog);
    return 2;
}

/* The program file in `dir`, whatever the case of the name. */
static void find_exe(const char *dir, char *path, size_t n)
{
    int count = 0;
    char **entries = SDL_GlobDirectory(dir, NULL, 0, &count);
    snprintf(path, n, "%s/%s", dir, SR_EXE_NAME);
    for (int i = 0; entries && i < count; i++)
        if (SDL_strcasecmp(entries[i], SR_EXE_NAME) == 0) { snprintf(path, n, "%s/%s", dir, entries[i]); break; }
    SDL_free(entries);
}

int main(int argc, char **argv)
{
    const char *dir = "Game";
    char *game_argv[64];
    int game_argc = 0;
    game_argv[game_argc++] = argv[0];
    int scale = 3;
    bool check = false, fullscreen = false;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "--game-dir") && v) { dir = v; i++; }
        else if (!strcmp(a, "--scale") && v) { scale = atoi(v); i++; }
        else if (!strcmp(a, "--run-ms") && v) { game_run_ms = atoi(v); i++; }
        else if (!strcmp(a, "--fullscreen")) fullscreen = true;
        else if (!strcmp(a, "--check")) check = true;
        else if (a[0] == '-' && a[1] == '-') return usage(argv[0]);
        else if (game_argc < 63) game_argv[game_argc++] = argv[i];   /* a switch of SR.EXE */
    }

    char exe_path[1024], err[256];
    find_exe(dir, exe_path, sizeof exe_path);
    if (!mem_load_exe(exe_path, err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Street Rod 2", err, NULL);
        return 1;
    }
    if (check) {
        printf("%s ok: image %u bytes at %04X:0000, DGROUP %04X\n", SR_EXE_NAME, mem_image_size, LOAD_SEG, DGROUP);
        return 0;
    }

    if (!host_init(dir, scale, fullscreen)) return 1;
    ega_init();                     /* 16-colour planar display: frame source, planes */
    game_argv[game_argc] = NULL;
    return game_main(game_argc, game_argv);     /* the original's switches (1, nomouse, demo, nouemem, e c v h t) are read there */
}
