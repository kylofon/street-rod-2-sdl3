/* Placeholder game_main (S4.1): LIB1 picture #0 through the C picture unpacker and the planar VGA
 * model, identity palette as SR2's start-up screens use (port/spec/video.md "Palettes"). */
#include "game/game.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "host.h"
#include "mem.h"
#include "platform/ega.h"
#include "platform/lib.h"
#include "platform/vga.h"

int game_run_ms;

#define DS_PAL_LIVE 0x1DA6      /* 17 bytes: 16 palette registers + overscan, identity at start-up */

static void set_palette(const u8 *tab)
{
    for (int i = 0; i < 17; i++) ega_set_palette_reg(i, (u8)(tab[i] | (i >= 8 && i < 16 ? 0x10 : 0)));
}

/* Planar picture (4 planes of h rows of w/8 bytes) to the screen page at video offset 0. */
static void show_planar(const u8 *pix, int w, int h)
{
    int bpr = w / 8, plane = bpr * h;
    for (int p = 0; p < 4; p++) {
        vga_out(0x3C4, 2);
        vga_out(0x3C5, (u8)(1 << p));
        for (int y = 0; y < h && y < 200; y++)
            for (int x = 0; x < bpr && x < EGA_BYTES_PER_LINE; x++)
                vga_wr((u16)(y * EGA_BYTES_PER_LINE + x), pix[p * plane + y * bpr + x]);
    }
    vga_out(0x3C5, 0x0F);
    ega_touch();
}

int game_main(void)
{
    vga_mode_0d();
    set_palette(mp(DGROUP, DS_PAL_LIVE));

    Lib lib;
    if (!lib_open(&lib, "LIB1")) host_fatal("cannot read LIB1");
    static u8 pix[0x10000];
    size_t n = lib_pic_decode(&lib, 0, pix, sizeof pix);
    if (!n) host_fatal("LIB1 picture 0 cannot be decoded");
    fprintf(stderr, "LIB1: %d pictures; #0 is %ux%u, %u bytes\n", lib.count, lib.entries[0].w, lib.entries[0].h, (unsigned)n);
    show_planar(pix, lib.entries[0].w, lib.entries[0].h);
    lib_close(&lib);

    Uint64 end = game_run_ms > 0 ? SDL_GetTicks() + (Uint64)game_run_ms : 0;
    for (;;) {
        host_pump();
        if (end && SDL_GetTicks() >= end) break;
    }
    return 0;
}
